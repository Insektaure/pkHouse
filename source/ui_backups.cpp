// Backups of one game's save (Y on the game selector): the list of what is on
// the SD card, putting one back onto the console, and deleting old ones.
//
// The writing itself is SaveBackup (save_backup.h), which follows JKSV and is
// transactional whenever the backup fits in one commit. This file is the flow
// around it, in the order that keeps the save safe:
//   1. SaveBackup::prepare reads, hashes and checks it against its meta, and
//      looks at the console - before anything parses it;
//   2. the backup must load as this game's save (SaveFile, as opening one does);
//   3. the person confirms, holding A, with what is known about the backup;
//   4. the save on the console is backed up first, and that backup read back -
//      without it, nothing is written;
//   5. SaveBackup::apply writes, commits, and reads everything back.

#include "ui.h"
#include "ui_util.h"
#include "i18n.h"
#include "led.h"
#include "save_backup.h"
#include "save_file.h"

#include <algorithm>
#include <cstdio>
#include <sys/statvfs.h>

namespace {

constexpr int BM_W = 880, BM_H = 600;
constexpr int BM_HEAD = 104, BM_FOOT = 58;
constexpr int BM_ROW_H = 64, BM_PITCH = 70;

// `seconds` when another backup was made in the same minute: two rows, or a
// row and a note, must never read as the same time (see needsSeconds).
std::string stampOf(time_t t, bool seconds = false) {
    char buf[32];
    const struct tm* tm = localtime(&t);
    if (seconds)
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %02d:%02d:%02d", tm->tm_year + 1900,
                      tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    else
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %02d:%02d",
                      tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);
    return buf;
}

// For the notes on a row ("Replaced by ...", "Same as ..."): the full date,
// as the rows show it, with a single space.
std::string noteStampOf(time_t t, bool seconds = false) {
    const struct tm a = *localtime(&t);
    char buf[32];
    if (seconds)
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", a.tm_year + 1900,
                      a.tm_mon + 1, a.tm_mday, a.tm_hour, a.tm_min, a.tm_sec);
    else
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
                      a.tm_year + 1900, a.tm_mon + 1, a.tm_mday, a.tm_hour, a.tm_min);
    return buf;
}

// Whether another backup in the list was made in the same minute as `t`.
bool needsSeconds(const std::vector<SaveBackup::Entry>& list, time_t t) {
    for (const SaveBackup::Entry& o : list)
        if (o.when != t && o.when / 60 == t / 60) return true;
    return false;
}

// Trainer names are UCS-2 in every save pkHouse reads.
std::string toUtf8(const std::u16string& s) {
    std::string out;
    for (char16_t c : s) {
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

const char* restoreErrorKey(SaveBackup::Error e) {
    using E = SaveBackup::Error;
    switch (e) {
        case E::Empty:
        case E::NoMainFile:   return StrKey::RsErrIncomplete;
        case E::ReadFailed:   return StrKey::RsErrRead;
        case E::Corrupt:      return StrKey::RsErrCorrupt;
        case E::OtherSave:    return StrKey::RsErrOtherSave;
        case E::OpenSave:
        case E::SaveInfo:     return StrKey::RsErrOpen;
        case E::TooLarge:     return StrKey::RsErrTooLarge;
        case E::WriteFailed:
        case E::CommitFailed: return StrKey::RsErrWrite;
        case E::VerifyFailed: return StrKey::RsErrVerify;
        case E::None:         break;
    }
    return StrKey::RsErrWrite;
}

} // anonymous namespace

std::string UI::backupGameDir(GameType game) const {
    if (selectedProfile_ < 0 || selectedProfile_ >= account_.profileCount()) return std::string();
    return basePath_ + "backups/" + account_.profiles()[selectedProfile_].pathSafeName + "/"
         + gamePathNameOf(game) + "/";
}

SaveBackup::Summary UI::backupSummary(const SaveFile& save, GameType game) const {
    SaveBackup::Summary s;
    if (!save.isLoaded()) return s;
    const TrainerInfo ti = save.getTrainerInfo();
    if (ti.valid) {
        s.trainer = toUtf8(ti.otName);
        s.tid = std::to_string(isFRLG(game) ? (ti.id32 & 0xFFFF) : (ti.id32 % 1000000));
    }
    int n = 0;
    for (int b = 0; b < save.boxCount(); b++)
        for (int slot = 0; slot < save.slotsPerBox(); slot++)
            if (!save.getBoxSlot(b, slot).isEmpty()) n++;
    s.pokemon = n;
    return s;
}

std::string UI::consoleFingerprint(GameType game) {
    const std::string mount = account_.mountSave(selectedProfile_, game);
    if (mount.empty()) return std::string();
    const std::string fp = SaveBackup::fingerprintDir(mount);   // read only: never committed
    account_.unmountSave();
    return fp;
}

// An icon from romfs:/icons/ at exactly `size` pixels, for drawing 1:1.
// The renderer scales with the nearest pixel, which at a third of the size
// keeps one pixel in nine and breaks thin strokes; here every output pixel
// is the average of the source pixels it covers. Cached with uiIcon's.
SDL_Texture* UI::iconAt(const std::string& name, int size) {
    const std::string key = name + "@" + std::to_string(size);
    auto it = uiIcons_.find(key);
    if (it != uiIcons_.end()) return it->second;

    SDL_Texture* tex = nullptr;
    SDL_Surface* loaded = size > 0 ? IMG_Load(("romfs:/icons/" + name + ".png").c_str()) : nullptr;
    SDL_Surface* src = loaded ? SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0) : nullptr;
    if (loaded) SDL_FreeSurface(loaded);
    SDL_Surface* out = src ? SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_RGBA32) : nullptr;
    if (src && out && SDL_LockSurface(src) == 0) {
        if (SDL_LockSurface(out) == 0) {
            const int sw = src->w, sh = src->h;
            for (int y = 0; y < size; y++) {
                const int y0 = y * sh / size, y1 = std::max(y0 + 1, (y + 1) * sh / size);
                for (int x = 0; x < size; x++) {
                    const int x0 = x * sw / size, x1 = std::max(x0 + 1, (x + 1) * sw / size);
                    // Colour weighted by alpha, so transparent pixels do not darken the edge.
                    uint32_t a = 0, r = 0, g = 0, b = 0, n = 0;
                    for (int sy = y0; sy < y1 && sy < sh; sy++) {
                        const uint8_t* row = static_cast<const uint8_t*>(src->pixels) + sy * src->pitch;
                        for (int sx = x0; sx < x1 && sx < sw; sx++) {
                            const uint8_t* p = row + sx * 4;
                            r += p[0] * p[3]; g += p[1] * p[3]; b += p[2] * p[3]; a += p[3];
                            n++;
                        }
                    }
                    uint8_t* d = static_cast<uint8_t*>(out->pixels) + y * out->pitch + x * 4;
                    d[0] = a ? static_cast<uint8_t>(r / a) : 255;
                    d[1] = a ? static_cast<uint8_t>(g / a) : 255;
                    d[2] = a ? static_cast<uint8_t>(b / a) : 255;
                    d[3] = n ? static_cast<uint8_t>(a / n) : 0;
                }
            }
            SDL_UnlockSurface(out);
            tex = SDL_CreateTextureFromSurface(renderer_, out);
        }
        SDL_UnlockSurface(src);
    }
    if (out) SDL_FreeSurface(out);
    if (src) SDL_FreeSurface(src);
    uiIcons_[key] = tex;   // cached even when missing
    return tex;
}

// A mark: the icon tinted, on a faint disc of its colour - drawCheckDisc's
// look, for any icon, drawn at its own size so it stays smooth.
void UI::drawIconDisc(const char* icon, int cx, int cy, int radius, SDL_Color c) {
    fillDisc(cx, cy, radius, SDL_Color{c.r, c.g, c.b, 40});
    const int s = radius * 3 / 2;
    SDL_Texture* tex = iconAt(icon, s);
    if (!tex) return;
    SDL_SetTextureColorMod(tex, c.r, c.g, c.b);
    SDL_Rect dst = {cx - s / 2, cy - s / 2, s, s};
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    SDL_SetTextureColorMod(tex, 255, 255, 255);
}

// --- The list ------------------------------------------------------------------------

void UI::drawBackupManager(GameType game, const std::vector<SaveBackup::Entry>& list,
                           int cursor, int scroll, const std::string& onConsole) {
    drawDialogBackdrop();

    const int x = (SCREEN_W - BM_W) / 2, y = (SCREEN_H - BM_H) / 2;
    fillRounded(x, y, BM_W, BM_H, 20, T().panelBg);
    strokeRounded(x, y, BM_W, BM_H, 20, 1, T().panelBorder);
    constexpr int PAD = 28;
    const int inX = x + PAD, inW = BM_W - 2 * PAD;

    // Header: which save, and what a restore does.
    {
        drawGameIcon(game, inX, y + 22, 52, 12, T().panelBg);
        TTF_Font* fTitle = uiFont(22, true);
        TTF_Font* fSub   = uiFont(14);
        const int tx = inX + 52 + 14;
        const std::string who = account_.profiles()[selectedProfile_].nickname;
        const std::string count = i18n::fmt(StrKey::BmCount, std::to_string(list.size()));
        const int cw = textWidth(count, fSub);
        drawText(count, inX + inW - cw, y + 30, T().textDim, fSub);
        drawText(fitText(i18n::get(StrKey::BmTitle), fTitle, inW - 66 - cw - 12), tx, y + 22, T().text, fTitle);
        drawText(fitText(std::string(gameDisplayNameOf(game)) + " \xc2\xb7 " + who, fSub, inW - 66),
                 tx, y + 52, T().textDim, fSub);
        drawRect(x, y + BM_HEAD - 12, BM_W, 1, T().panelBorder);
    }

    const int listY = y + BM_HEAD, listH = BM_H - BM_HEAD - BM_FOOT;
    if (list.empty()) {
        TTF_Font* f = uiFont(16);
        const auto lines = wrapText(i18n::get(StrKey::BmEmpty), f, inW - 80, 3);
        int ly = listY + listH / 2 - static_cast<int>(lines.size()) * 12;
        for (const auto& l : lines) { drawTextCentered(l, x + BM_W / 2, ly, T().textDim, f); ly += 24; }
    } else {
        TTF_Font* fDate  = uiFont(17, true);
        TTF_Font* fSub   = uiFont(13);
        TTF_Font* fBadge = uiFont(12, true);
        const int visible = listH / BM_PITCH;
        for (int i = scroll; i < static_cast<int>(list.size()) && i < scroll + visible; i++) {
            const SaveBackup::Entry& e = list[i];
            const int ry = listY + (i - scroll) * BM_PITCH;
            const bool on = i == cursor;
            fillRounded(inX, ry, inW, BM_ROW_H, 12, on ? T().slotFull : T().bg);
            if (on) strokeRounded(inX, ry, inW, BM_ROW_H, 12, 2, T().accent);

            // Marks on the right, explained by the legend in the footer:
            // checked, made before a restore, what the console holds. The one
            // exception to "checked", a backup without a meta, is a word.
            int bx = inX + inW - 14;
            auto badge = [&](const std::string& text, SDL_Color c) {
                const int bw = measureTextTracked(text, fBadge, 1) + 20;
                bx -= bw;
                fillRounded(bx, ry + BM_ROW_H / 2 - 12, bw, 24, 12, SDL_Color{c.r, c.g, c.b, 40});
                drawTextTracked(text, bx + 10, ry + BM_ROW_H / 2 - TTF_FontHeight(fBadge) / 2, c, fBadge, 1);
                bx -= 8;
            };
            const bool beforeRestore = e.meta.present && e.meta.reason == "restore";
            auto mark = [&](const char* icon, SDL_Color c) {
                constexpr int R = 12;   // 24 px across: the height of the text badges
                drawIconDisc(icon, bx - R, ry + BM_ROW_H / 2, R, c);
                bx -= 2 * R + 6;
            };
            if (e.meta.present) mark("check", T().statusOk);
            else                badge(i18n::get(StrKey::BmOlder), T().textMuted);
            if (beforeRestore)  mark("rollback", T().accentBank);
            if (!onConsole.empty() && e.fingerprint == onConsole)
                mark("gamepad", T().accent);

            // Line 1: when, then how it relates to the others - read from the
            // metas, so it holds across any number of restores:
            //  - a backup made before a restore names the backup that replaced
            //    its save (the badge says what kind it is; gone when that
            //    backup has been deleted);
            //  - a backup that was restored names the last time it was;
            //  - a backup holding the same save as an older one names that one.
            std::string notes;
            auto note = [&](const std::string& n) { notes += (notes.empty() ? "" : " \xc2\xb7 ") + n; };
            if (beforeRestore && !e.meta.restoredFrom.empty()) {
                const auto target = std::find_if(list.begin(), list.end(), [&](const SaveBackup::Entry& o) {
                    return o.name == e.meta.restoredFrom;
                });
                if (target != list.end())
                    note(i18n::fmt(StrKey::BmReplacedBy, noteStampOf(target->when, needsSeconds(list, target->when))));
            }
            {
                time_t last = 0;
                for (const SaveBackup::Entry& o : list)
                    if (o.meta.restoredFrom == e.name && o.when > last) last = o.when;
                if (last) note(i18n::fmt(StrKey::BmRestored, noteStampOf(last, needsSeconds(list, last))));
            }
            // "Same as" goes on line 2, where there is room: line 1 keeps the
            // restore links, which matter more when both do not fit.
            std::string sameAs;
            if (!e.fingerprint.empty()) {
                const SaveBackup::Entry* same = nullptr;   // the oldest with the same save
                for (const SaveBackup::Entry& o : list)
                    if (&o != &e && o.fingerprint == e.fingerprint && o.when < e.when
                        && (!same || o.when < same->when))
                        same = &o;
                if (same)
                    sameAs = i18n::fmt(StrKey::BmSameAs, noteStampOf(same->when, needsSeconds(list, same->when)));
            }
            const int room = bx - (inX + 16) - 8;
            const std::string stamp = fitText(stampOf(e.when, needsSeconds(list, e.when)), fDate, room);
            drawText(stamp, inX + 16, ry + 11, T().text, fDate);
            const int nx = inX + 16 + textWidth(stamp, fDate) + 14;
            if (!notes.empty() && nx < bx - 40)
                drawText(fitText(notes, fSub, bx - 8 - nx), nx, ry + 15, T().accentBank, fSub);

            // Line 2: how old, whose, how much.
            std::string sub = relativeDay(e.when);
            const SaveBackup::Summary& sm = e.meta.summary;
            if (!sm.trainer.empty()) sub += " \xc2\xb7 " + sm.trainer + (sm.tid.empty() ? "" : " \xc2\xb7 " + sm.tid);
            if (sm.pokemon >= 0)     sub += " \xc2\xb7 " + i18n::fmt(StrKey::BmPokemon, std::to_string(sm.pokemon));
            sub += " \xc2\xb7 " + formatSize(static_cast<size_t>(e.bytes));
            if (sm.trainer.empty() && sm.pokemon < 0)
                sub += " \xc2\xb7 " + i18n::fmt(StrKey::BmFiles, std::to_string(e.files));
            // Up to the badges, which are centred on the row and reach into
            // this line. "Same as" after the facts, and the one cut short when
            // both do not fit.
            const std::string facts = fitText(sub, fSub, room);
            drawText(facts, inX + 16, ry + 38, T().textDim, fSub);
            const int sx = inX + 16 + textWidth(facts, fSub) + 14;
            if (!sameAs.empty() && sx < bx - 8 - 40)
                drawText(fitText(sameAs, fSub, bx - 8 - sx), sx, ry + 38, T().accentBank, fSub);
        }
        // Scrollbar when the list is longer than the panel.
        if (static_cast<int>(list.size()) > visible) {
            const int trackX = x + BM_W - 14, trackH = listH - 12;
            const int thumbH = std::max(30, trackH * visible / static_cast<int>(list.size()));
            const int maxScroll = static_cast<int>(list.size()) - visible;
            const int thumbY = listY + (trackH - thumbH) * scroll / std::max(1, maxScroll);
            fillRounded(trackX, listY, 5, trackH, 2, T().buttonBg);
            fillRounded(trackX, thumbY, 5, thumbH, 2, T().textMuted);
        }
    }

    // Keys, inside the panel like the About screen's.
    {
        const int fy = y + BM_H - BM_FOOT;
        drawRect(x, fy, BM_W, 1, T().panelBorder);
        TTF_Font* f = uiFont(15, true);
        const int cy = fy + BM_FOOT / 2;
        int kx = inX;
        auto key = [&](const char* k, const char* label, bool enabled) {
            kx += drawFooterKey(kx, cy, k, false) + 8;
            const std::string l = i18n::get(label);
            drawText(l, kx, cy - TTF_FontHeight(f) / 2, enabled ? T().text : T().textMuted, f);
            kx += textWidth(l, f) + 24;
        };
        key("A", StrKey::BmRestore, !list.empty());
        key("X", StrKey::BmDelete, !list.empty());
        key("B", StrKey::HintClose, true);
        // The legend of the marks, right-aligned. Labels are cut evenly when
        // a language does not fit them beside the keys.
        struct Legend { const char* icon; SDL_Color color; const char* key; };
        const Legend legend[] = {
            {"check",    T().statusOk,   StrKey::BmChecked},
            {"rollback", T().accentBank, StrKey::BmBeforeRestore},
            {"gamepad",  T().accent,     StrKey::BmOnConsole},
        };
        TTF_Font* fl = uiFont(13);
        constexpr int LR = 9, GAP = 16;
        const int n = static_cast<int>(sizeof(legend) / sizeof(legend[0]));
        int want = 0;
        for (const Legend& l : legend) want += 2 * LR + 6 + textWidth(i18n::get(l.key), fl);
        want += GAP * (n - 1);
        const int avail = inX + inW - kx;
        const int labelCap = want <= avail ? 10000
                           : std::max(24, (avail - GAP * (n - 1)) / n - (2 * LR + 6));
        std::vector<std::string> labels;
        int total = GAP * (n - 1);
        for (const Legend& l : legend) {
            labels.push_back(fitText(i18n::get(l.key), fl, labelCap));
            total += 2 * LR + 6 + textWidth(labels.back(), fl);
        }
        int lx = inX + inW - total;
        for (int i = 0; i < n; i++) {
            drawIconDisc(legend[i].icon, lx + LR, cy, LR, legend[i].color);
            lx += 2 * LR + 6;
            drawText(labels[i], lx, cy - TTF_FontHeight(fl) / 2, T().textDim, fl);
            lx += textWidth(labels[i], fl) + GAP;
        }
    }
}

void UI::showBackupManager(GameType game) {
    if (!renderer_ || selectedProfile_ < 0 || selectedProfile_ >= account_.profileCount()) return;
    markDirty();

    const std::string gameDir = backupGameDir(game);
    // Nothing may hold the save while the manager is open: it mounts it to
    // read it, and a restore opens it directly.
    account_.unmountSave();
    showWorking(i18n::get(StrKey::BmReading));
    std::vector<SaveBackup::Entry> list = SaveBackup::list(gameDir);
    std::string onConsole = consoleFingerprint(game);
    int cursor = 0, scroll = 0;
    const int visible = (BM_H - BM_HEAD - BM_FOOT) / BM_PITCH;
    auto clampView = [&]() {
        const int n = static_cast<int>(list.size());
        cursor = n == 0 ? 0 : std::clamp(cursor, 0, n - 1);
        if (cursor < scroll) scroll = cursor;
        if (cursor >= scroll + visible) scroll = cursor - visible + 1;
        scroll = std::clamp(scroll, 0, std::max(0, n - visible));
    };
    auto reload = [&]() {
        showWorking(i18n::get(StrKey::BmReading));
        list = SaveBackup::list(gameDir);
        onConsole = consoleFingerprint(game);
        clampView();
        refreshBankCounts();   // the detail panel's backup count and date
        frameGen_++;           // the backdrop behind has changed
    };

    // The stick as on the other screens (updateStick, same delays): a move at
    // once, then repeats while held. Up / down only: the list is one column.
    // Cleared on the way in and out, so a stick still tilted on the game
    // selector does not scroll the list, nor the list the selector.
    updateStick(0, 0);

    bool open = true, redraw = true;
    while (open) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { open = false; break; }
            if (event.type == SDL_CONTROLLERAXISMOTION &&
                (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                 event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)) {
                updateStick(SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX),
                            SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY));
                continue;
            }
            if (event.type != SDL_CONTROLLERBUTTONDOWN) continue;
            redraw = true;
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:   cursor--; clampView(); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: cursor++; clampView(); break;
                case SDL_CONTROLLER_BUTTON_A:         // Switch B = close
                    open = false;
                    break;
                case SDL_CONTROLLER_BUTTON_B:         // Switch A = restore
                    if (!list.empty()) {
                        restoreBackup(game, list[cursor]);
                        reload();
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_Y: {       // Switch X = delete
                    if (list.empty()) break;
                    const SaveBackup::Entry e = list[cursor];
                    ConfirmStyle st;
                    st.danger = true;
                    st.hold = true;
                    if (showConfirmDialog(i18n::get(StrKey::BmDeleteTitle),
                                          i18n::fmt(StrKey::BmDeleteBody, stampOf(e.when, needsSeconds(list, e.when))), st)) {
                        if (!SaveBackup::remove(e.dir))
                            showMessageAndWait(i18n::get(StrKey::BmDeleteFailed), e.dir);
                        reload();
                    }
                    break;
                }
            }
            // A restore or a delete ran a dialog with its own event loop: the
            // stick may have been let go meanwhile without this loop seeing it.
            updateStick(SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX),
                        SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY));
        }
        if (open && stickDirY_ != 0) {
            const uint32_t now = SDL_GetTicks();
            const uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
            if (now - stickMoveTime_ >= delay) {
                cursor += stickDirY_;
                clampView();
                stickMoveTime_ = now;
                stickMoved_ = true;
                redraw = true;
            }
        }
        if (redraw && open) {
            drawBackupManager(game, list, cursor, scroll, onConsole);
            SDL_RenderPresent(renderer_);
            redraw = false;
        }
        SDL_Delay(16);
    }
    updateStick(0, 0);
    markDirty();
}

// --- Restoring one ----------------------------------------------------------------------

bool UI::restoreBackup(GameType game, const SaveBackup::Entry& entry) {
    if (selectedProfile_ < 0 || selectedProfile_ >= account_.profileCount()) return false;
    const UserProfile& profile = account_.profiles()[selectedProfile_];
    const uint64_t titleId = titleIdOf(game);
    const std::string mainFile = saveFileNameOf(game);
    const std::string title = i18n::get(StrKey::RsFailTitle);

    // SaveBackup opens the save itself; nothing may hold it mounted.
    account_.unmountSave();
    showWorking(i18n::get(StrKey::RsChecking));

    // 1. Read, hashed, compared with its meta; the console's save looked at.
    //    First, so that a damaged backup is refused on its MD5 before the
    //    save parser ever reads it.
    const SaveBackup::Prepared prep = SaveBackup::prepare(titleId, profile.uid, entry.dir, mainFile);
    if (prep.error != SaveBackup::Error::None) {
        // Nothing has been written yet, and the message says so, as every
        // failure says whether the save was touched.
        std::string body = i18n::get(restoreErrorKey(prep.error));
        if (!prep.detail.empty()) body += "\n" + prep.detail;
        body += "\n\n" + i18n::get(StrKey::RsUnchangedBody);
        showMessageAndWait(title, body);
        return false;
    }

    // 2. It has to read as this game's save, the way opening the game reads
    //    one - including the encryption round trip for the SCBlock games.
    //    For a backup with a meta this parses files already proven intact;
    //    for an older one it is the only check of its contents.
    std::string trainer;
    {
        SaveFile probe;
        probe.setGameType(game);
        bool ok = probe.load(entry.dir + mainFile);
        if (ok && !isBDSP(game) && !isLGPE(game) && !isFRLG(game))
            ok = probe.verifyRoundTrip() == "OK";
        if (!ok) {
            showMessageAndWait(title, i18n::get(StrKey::RsErrNotASave));
            return false;
        }
        const SaveBackup::Summary sm = backupSummary(probe, game);
        if (!sm.trainer.empty())
            trainer = sm.trainer + (sm.tid.empty() ? "" : " \xc2\xb7 " + sm.tid);
    }

    // 3. Confirm, with everything that is known.
    {
        std::string body = i18n::fmt(StrKey::RsConfirmBody, stampOf(entry.when), relativeDay(entry.when));
        if (!trainer.empty()) body += "\n" + i18n::fmt(StrKey::RsTrainer, trainer);
        body += "\n\n" + i18n::get(StrKey::RsConfirmSafe);
        ConfirmStyle st;
        st.danger = true;
        st.hold = true;
        st.confirmKey = StrKey::RsHold;
        if (prep.legacy)       st.note = i18n::get(StrKey::RsNoteLegacy);
        else if (!prep.atomic) st.note = i18n::get(StrKey::RsNoteChunked);
        if (prep.legacy && !prep.atomic)
            st.note += " " + i18n::get(StrKey::RsNoteChunked);
        if (!showConfirmDialog(i18n::get(StrKey::RsConfirmTitle), body, st))
            return false;
    }

    // 4. The save as it is now, backed up and read back. No backup, no restore.
    std::string safetyDir;
    {
        const std::string mount = account_.mountSave(selectedProfile_, game);
        if (mount.empty()) {
            showMessageAndWait(title, i18n::get(StrKey::RsErrOpen) + "\n\n" + i18n::get(StrKey::RsUnchangedBody));
            return false;
        }
        const size_t saveSize = AccountManager::calculateDirSize(mount);
        struct statvfs vfs;
        if (statvfs("sdmc:/", &vfs) == 0 && static_cast<size_t>(vfs.f_bavail) * vfs.f_bsize < saveSize * 2) {
            account_.unmountSave();
            showMessageAndWait(title, i18n::get(StrKey::RsErrNoSpace));
            return false;
        }
        // What it holds, for the list: read from the console, as it is now.
        SaveBackup::Summary current;
        {
            SaveFile now;
            now.setGameType(game);
            if (now.load(mount + mainFile)) current = backupSummary(now, game);
        }
        safetyDir = buildBackupDir(game);
        size_t written = 0;
        uint32_t lastDraw = 0;
        showWorking(i18n::get(StrKey::RsBackingUp), true, 0.0f);
        ledBlink();
        bool ok = AccountManager::backupSaveDir(mount, safetyDir, [&](size_t n) {
            written += n;
            const uint32_t now = SDL_GetTicks();
            if (now - lastDraw >= 50) {
                lastDraw = now;
                showWorking(i18n::get(StrKey::RsBackingUp), true,
                            saveSize ? std::min(1.0f, static_cast<float>(written) / saveSize) : 1.0f);
            }
        });
        account_.unmountSave();
        // Names the backup about to be restored, so the list can say what
        // this one was made before - across any number of restores.
        ok = ok && SaveBackup::writeMeta(safetyDir, titleId, profile.uid, "restore", current, entry.name);
        ledOff();
        if (!ok) {
            SaveBackup::remove(safetyDir);   // an unverified copy is not a safety net
            showMessageAndWait(title, i18n::get(StrKey::RsErrSafety));
            return false;
        }
    }

    // 5. Write, commit, read back.
    showWorking(i18n::get(StrKey::RsWriting), true, 0.0f);
    ledBlink();
    uint32_t lastDraw = 0;
    const SaveBackup::Outcome out = SaveBackup::apply(prep, titleId, profile.uid, [&](float f) {
        const uint32_t now = SDL_GetTicks();
        if (now - lastDraw >= 50) {
            lastDraw = now;
            showWorking(i18n::get(StrKey::RsWriting), true, f);
        }
    });
    ledOff();

    std::string shortDir = safetyDir;
    const size_t at = shortDir.find("backups/");
    if (at != std::string::npos) shortDir = shortDir.substr(at);

    if (out.error == SaveBackup::Error::None) {
        showMessageAndWait(i18n::get(StrKey::RsDoneTitle),
                           i18n::fmt(StrKey::RsDoneBody, stampOf(entry.when), dialogPath(shortDir)), DialogKind::Success);
        return true;
    }
    std::string body = i18n::get(restoreErrorKey(out.error));
    if (!out.detail.empty()) body += "\n" + out.detail;
    body += "\n\n" + (out.saveChanged ? i18n::fmt(StrKey::RsChangedBody, dialogPath(shortDir))
                                      : i18n::get(StrKey::RsUnchangedBody));
    showMessageAndWait(title, body);
    return false;
}
