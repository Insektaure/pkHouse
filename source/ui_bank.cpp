#include "ui.h"
#include "ui_util.h"
#include "i18n.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <iterator>
#include <sys/statvfs.h>

#include <switch.h>

// --- Bank picker (UI 2.0) ----------------------------------------------------------
//
// external/UI_2.0 "7a" to "7d": one screen for the four ways of reaching it -
// after opening a save (7b), All banks (7a), and the two steps of dual bank
// (7c, 7d).

namespace {

// Prefixed: these functions are UI members, where an unprefixed name would
// lose to a UI constant of the same name.
constexpr int BP_LIST_TOP    = 80;
constexpr int BP_LIST_BOTTOM = 658;
constexpr int BP_ROW_H       = 60;
constexpr int BP_ROW_STEP    = 68;
constexpr int BP_GROUP_H     = 36;
constexpr int BP_STATS_Y     = 584;
constexpr int BP_STATS_H     = 70;
constexpr SDL_Color BP_FULL  = {240, 128, 90, 255};

int bankCapacity(GameType g) { return isLGPE(g) ? 1000 : isBDSP(g) ? 1200 : 960; }

std::string dateShort(time_t t, bool withYear) {
    char buf[16];
    const struct tm* tm = localtime(&t);
    if (withYear)
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    else
        std::snprintf(buf, sizeof(buf), "%02d-%02d", tm->tm_mon + 1, tm->tm_mday);
    return buf;
}

std::string clockOf(time_t t) {
    char buf[8];
    const struct tm* tm = localtime(&t);
    std::snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
    return buf;
}

} // anonymous namespace

bool UI::bankSelListOnLeft() const {
    return bankManager_.isAllMode() || bankSelTarget_ == Panel::Game;
}

// Picking a bank for a side, the panel shows the bank under the cursor: the
// point is to see what is in it before opening it. The one exception is the
// save's own picker, where the bank chosen is where the save's Pokemon will go
// and the save beside the list is the useful comparison. In dual bank the
// bank already chosen stays named in the step bar and badged in the list.
Panel UI::bankSelPreviewSource() const {
    if (!isDualBankMode() && save_.isLoaded()) return Panel::Game;
    return Panel::Preview;
}

bool UI::bankSelCanCreate() const { return !bankManager_.isAllMode(); }

int UI::bankSelRowCount() const {
    return static_cast<int>(bankManager_.list().size()) + (bankSelCanCreate() ? 1 : 0);
}

void UI::clearBankPreview() {
    previewBankPath_.clear();
    previewBox_ = 0;
    for (auto it = slotDisplayCache_.begin(); it != slotDisplayCache_.end(); )
        it = (it->first.panel == Panel::Preview) ? slotDisplayCache_.erase(it) : std::next(it);
    bankPreviewSince_ = SDL_GetTicks();
}

// Loads the highlighted bank for the preview once the cursor has rested on it.
bool UI::updateBankPreview() {
    if (screen_ != AppScreen::BankSelector || bankSelPreviewSource() != Panel::Preview)
        return false;
    const auto& banks = bankManager_.list();
    if (bankSelCursor_ < 0 || bankSelCursor_ >= (int)banks.size() || !banks[bankSelCursor_].valid)
        return false;
    const BankInfo& info = banks[bankSelCursor_];
    if (info.fullPath == previewBankPath_) return false;
    if (SDL_GetTicks() - bankPreviewSince_ < BANK_PREVIEW_DELAY_MS) return false;

    // All banks spans games, and the grid's shape (LGPE has five columns)
    // follows the selected game: take the bank's, as opening it would.
    if (bankManager_.isAllMode() && selectedGame_ != info.game) {
        selectedGame_ = info.game;
        invalidateAllSlotDisplays();
    }
    previewBank_.load(info.fullPath);
    previewBank_.setGameType(info.game);
    previewBankPath_ = info.fullPath;
    previewBankName_ = info.name;
    previewBankModified_ = info.modified;
    previewBox_ = 0;
    for (auto it = slotDisplayCache_.begin(); it != slotDisplayCache_.end(); )
        it = (it->first.panel == Panel::Preview) ? slotDisplayCache_.erase(it) : std::next(it);
    return true;
}

void UI::drawBankPickerTopBar() {
    const bool all = bankManager_.isAllMode();
    const bool dual = isDualBankMode() && !all;
    std::vector<std::string> steps;
    int current = 0;
    std::string title;
    if (all) {
        title = i18n::get(StrKey::AllBanks);
    } else if (dual) {
        // The right bank is chosen first, then the left.
        // The bank already chosen is badged in the list (RIGHT / LEFT), so
        // the steps stay short.
        title = i18n::get(StrKey::BsDualTitle);
        steps = {i18n::get(StrKey::StepRight), i18n::get(StrKey::StepLeft)};
        current = bankSelTarget_ == Panel::Bank ? 0 : 1;
    } else {
        title = i18n::get(StrKey::BsTitle);
        steps.push_back(i18n::get(StrKey::StepGame));
        if (selectedProfile_ >= 0) steps.push_back(i18n::get(StrKey::StepBackup));
        steps.push_back(i18n::get(StrKey::StepBank));
        current = static_cast<int>(steps.size()) - 1;
    }
    drawFlowTopBar(title, steps, current);

    // Right: what is open.
    const int cy = 40;
    TTF_Font* f = uiFont(15, true);
    std::string label;
    SDL_Color tint = T().text, bg = T().panelBg;
    if (all) {
        label = i18n::get(StrKey::ChipAllBanks);
        tint = T().accentBank;
        bg = SDL_Color{tint.r, tint.g, tint.b, 40};
    } else if (dual) {
        label = i18n::fmt(StrKey::ChipDual, gameDisplayNameOf(selectedGame_));
        tint = T().accent;
        bg = T().badgeBg;
    } else {
        if (selectedProfile_ >= 0 && selectedProfile_ < account_.profileCount())
            label = account_.profiles()[selectedProfile_].nickname + " \xc2\xb7 ";
        label += gameDisplayNameOf(selectedGame_);
    }
    label = fitText(label, f, 320);
    const int w = 8 + 32 + 10 + textWidth(label, f) + 14;
    const int x = SCREEN_W - 32 - w;
    fillRounded(x, cy - 23, w, 46, 12, bg);
    strokeRounded(x, cy - 23, w, 46, 12, 1, T().panelBorder);
    if (all) {
        fillRounded(x + 8, cy - 16, 32, 32, 8, SDL_Color{tint.r, tint.g, tint.b, 50});
        if (iconBank_) {
            SDL_SetTextureColorMod(iconBank_, tint.r, tint.g, tint.b);
            SDL_Rect dst = {x + 14, cy - 10, 20, 20};
            SDL_RenderCopy(renderer_, iconBank_, nullptr, &dst);
            SDL_SetTextureColorMod(iconBank_, 255, 255, 255);
        }
    } else {
        drawGameIcon(selectedGame_, x + 8, cy - 16, 32, 8, bg);
    }
    drawText(label, x + 8 + 32 + 10, cy - TTF_FontHeight(f) / 2, tint, f);
}

void UI::drawBankList(int x, int w) {
    const auto& banks = bankManager_.list();
    const bool all = bankManager_.isAllMode();
    const bool dual = isDualBankMode() && !all;
    int y = BP_LIST_TOP;

    // After opening a save: what happened to its backup.
    if (!isDualBankMode() && lastBackup_ != BackupOutcome::None) {
        const bool saved = lastBackup_ == BackupOutcome::Saved;
        const SDL_Color c = saved ? T().statusOk : lastBackup_ == BackupOutcome::Skipped
                                                    ? T().statusWarn : T().red;
        fillRounded(x, y, w, 52, 10, SDL_Color{c.r, c.g, c.b, 36});
        if (saved) drawCheckDisc(x + 26, y + 26, 13, c, T().bg);
        else { fillDisc(x + 26, y + 26, 13, c); drawTextCentered("!", x + 26, y + 26, T().bg, uiFont(15, true)); }
        TTF_Font* fT = uiFont(14, true);
        const std::string title = saved
            ? i18n::fmt(StrKey::BsBackupSaved, relativeDay(lastBackupWhen_) + " " + clockOf(lastBackupWhen_))
            : i18n::get(lastBackup_ == BackupOutcome::Skipped ? StrKey::BsBackupSkipped : StrKey::BsBackupFailed);
        if (saved) {
            drawText(fitText(title, fT, w - 66), x + 52, y + 8, c, fT);
            // The path under the backups folder: the rest is always the same.
            std::string shortDir = lastBackupDir_;
            const size_t at = shortDir.find("backups/");
            if (at != std::string::npos) shortDir = shortDir.substr(at);
            TTF_Font* fP = uiFont(12);
            drawText(fitText(shortDir, fP, w - 66), x + 52, y + 29, c, fP);
        } else {
            drawText(fitText(title, fT, w - 66), x + 52, y + 26 - TTF_FontHeight(fT) / 2, c, fT);
        }
        y += 52 + 16;
    }

    // What the list is.
    {
        TTF_Font* fTag = uiFont(11, true);
        TTF_Font* fNote = uiFont(13);
        std::string tag, note;
        SDL_Color tagColor = T().accentBank;
        if (all) {
            tag = i18n::get(StrKey::AllBanks);
            note = i18n::fmt(StrKey::AllBanksSub, std::to_string(allBanksTotal_),
                             std::to_string(allBanksFamilies_));
        } else if (dual) {
            const bool right = bankSelTarget_ == Panel::Bank;
            tag = i18n::fmt(StrKey::BsStepN, right ? "1" : "2",
                            i18n::get(right ? StrKey::StepRight : StrKey::StepLeft));
            note = i18n::get(right ? StrKey::BsOpensRight : StrKey::BsOpensLeft);
            tagColor = T().accent;
        } else {
            tag = i18n::get(StrKey::BsForGame);
            const GameType pair = pairedGame(selectedGame_);
            note = pair == selectedGame_ ? i18n::get(StrKey::DsSingleFamily)
                 : i18n::fmt(StrKey::BsSharedBy, gameDisplayNameOf(selectedGame_), gameDisplayNameOf(pair));
        }
        tag = toUpperUtf8(tag);
        const int tw = measureTextTracked(tag, fTag, 2);
        drawTextTracked(tag, x, y + 4, tagColor, fTag, 2);
        int noteRoom = w - tw - 12;
        if (all) {
            // ZL ZR  Jump game, right-aligned.
            TTF_Font* fK = uiFont(12, true);
            TTF_Font* fL = uiFont(13);
            const std::string jl = i18n::get(StrKey::HintJumpGame);
            constexpr int KEY_W = 28;
            const int jw = KEY_W + 4 + KEY_W + 8 + textWidth(jl, fL);
            int jx = x + w - jw;
            for (const char* k : {"ZL", "ZR"}) {
                fillRounded(jx, y, KEY_W, 20, 5, T().keyCap);
                drawTextCentered(k, jx + KEY_W / 2, y + 10, T().keyCapText, fK);
                jx += KEY_W + 4;
            }
            drawText(jl, jx + 4, y + 10 - TTF_FontHeight(fL) / 2, T().textDim, fL);
            noteRoom -= jw + 12;
        }
        drawText(fitText(note, fNote, noteRoom), x + tw + 12, y + 2, T().textDim, fNote);
        y += 36;
    }

    // Rows: a header per game family, the banks, then "New bank".
    struct Row { int kind; int idx; GameType game; };   // 0 header, 1 bank, 2 new
    std::vector<Row> rows;
    for (int i = 0; i < (int)banks.size(); i++) {
        if (i == 0 || banks[i].game != banks[i - 1].game)
            rows.push_back({0, -1, banks[i].game});
        rows.push_back({1, i, banks[i].game});
    }
    if (banks.empty() && !all)
        rows.push_back({0, -1, selectedGame_});
    if (bankSelCanCreate())
        rows.push_back({2, (int)banks.size(), selectedGame_});

    std::vector<int> top(rows.size() + 1, 0);
    for (size_t i = 0; i < rows.size(); i++)
        top[i + 1] = top[i] + (rows[i].kind == 0 ? BP_GROUP_H : BP_ROW_STEP);
    const int total = top.back();
    const int viewH = BP_LIST_BOTTOM - y;

    // Keep the cursor's row in view, and its header with it when it is first.
    int cur = 0;
    for (size_t i = 0; i < rows.size(); i++)
        if (rows[i].kind != 0 && rows[i].idx == bankSelCursor_) { cur = static_cast<int>(i); break; }
    int want = top[cur];
    if (cur > 0 && rows[cur - 1].kind == 0) want = top[cur - 1];
    if (want < bankSelScroll_) bankSelScroll_ = want;
    if (top[cur] + BP_ROW_H + 6 > bankSelScroll_ + viewH) bankSelScroll_ = top[cur] + BP_ROW_H + 6 - viewH;
    bankSelScroll_ = std::max(0, std::min(bankSelScroll_, std::max(0, total - viewH)));

    if (rows.empty()) {
        drawText(i18n::get(StrKey::DsNoBanks), x, y + 10, T().textMuted, uiFont(15));
        return;
    }

    const bool scrolls = total > viewH;
    const int rowW = w - (scrolls ? 16 : 0);
    const SDL_Rect clip = {x - 8, y - 6, w + 16, BP_LIST_BOTTOM - y + 6};
    SDL_RenderSetClipRect(renderer_, &clip);

    TTF_Font* fGroup = uiFont(14, true);
    TTF_Font* fCount = uiFont(12);
    TTF_Font* fName  = uiFont(17, true);
    TTF_Font* fSub   = uiFont(12);
    TTF_Font* fSlots = uiFont(14, true);
    for (size_t r = 0; r < rows.size(); r++) {
        const int ry = y + top[r] - bankSelScroll_;
        const int rh = rows[r].kind == 0 ? BP_GROUP_H : BP_ROW_H;
        if (ry + rh < y - 6) continue;
        if (ry > BP_LIST_BOTTOM) break;

        if (rows[r].kind == 0) {
            int n = 0;
            for (const auto& b : banks) if (b.game == rows[r].game) n++;
            const int cy = ry + BP_GROUP_H / 2;
            fillDisc(x + 8, cy, 4, gameTint(rows[r].game));
            const std::string name = bankGroupNameOf(rows[r].game);
            drawText(name, x + 22, cy - TTF_FontHeight(fGroup) / 2, T().text, fGroup);
            const std::string cnt = i18n::fmt(n == 1 ? StrKey::BsCountOne : StrKey::BsCountMany, std::to_string(n));
            const int cw = textWidth(cnt, fCount);
            drawText(cnt, x + rowW - cw, cy - TTF_FontHeight(fCount) / 2, T().textMuted, fCount);
            const int lx = x + 22 + textWidth(name, fGroup) + 12;
            if (x + rowW - cw - 12 > lx) drawRect(lx, cy, x + rowW - cw - 12 - lx, 1, T().divider);
            continue;
        }

        const bool isCur = rows[r].idx == bankSelCursor_;
        if (isCur) strokeRounded(x - 6, ry - 6, rowW + 12, rh + 12, 16, 3, T().accent);

        if (rows[r].kind == 2) {
            // New bank
            fillRounded(x, ry, rowW, rh, 10, isCur ? T().slotFull : T().bg);
            dashRounded(x, ry, rowW, rh, 10, T().cellEmptyBorder);
            fillRounded(x + 16, ry + 12, 36, 36, 9, T().buttonBg);
            drawTextCentered("+", x + 34, ry + 30, T().text, uiFont(20, true));
            drawText(i18n::get(StrKey::BsNewBank), x + 66, ry + rh / 2 - TTF_FontHeight(fName) / 2 + 1,
                     T().text, uiFont(16, true));
            fillDisc(x + rowW - 30, ry + rh / 2, 12, T().keyCap);
            drawTextCentered("X", x + rowW - 30, ry + rh / 2, T().keyCapText, uiFont(13, true));
            continue;
        }

        const BankInfo& b = banks[rows[r].idx];
        // Already open on the other side: shown, but it cannot be picked there too.
        bool otherSide = false;
        if (isDualBankMode() && b.game == selectedGame_) {
            if (bankSelTarget_ == Panel::Game && b.name == activeBankName_) otherSide = true;
            if (bankSelTarget_ == Panel::Bank && b.name == leftBankName_)   otherSide = true;
        }
        const bool dim = otherSide || !b.valid;

        fillRounded(x, ry, rowW, rh, 10, isCur ? T().slotFull : T().panelBg);
        strokeRounded(x, ry, rowW, rh, 10, 1, isCur ? T().cellBorder : T().panelBorder);
        const SDL_Color teal = T().accentBank;
        fillRounded(x + 13, ry + 12, 36, 36, 9, SDL_Color{teal.r, teal.g, teal.b, static_cast<Uint8>(dim ? 20 : 45)});
        if (iconBank_) {
            SDL_SetTextureColorMod(iconBank_, teal.r, teal.g, teal.b);
            if (dim) SDL_SetTextureAlphaMod(iconBank_, 110);
            SDL_Rect dst = {x + 21, ry + 20, 20, 20};
            SDL_RenderCopy(renderer_, iconBank_, nullptr, &dst);
            SDL_SetTextureColorMod(iconBank_, 255, 255, 255);
            SDL_SetTextureAlphaMod(iconBank_, 255);
        }

        // Right side: fill and bar, or why it cannot be opened.
        const int rightX = x + rowW - 16;
        int nameRoom = rowW - 62 - 16;
        if (!b.valid) {
            const std::string inv = i18n::get(StrKey::InvalidBankFile);
            drawText(inv, rightX - textWidth(inv, fSlots), ry + rh / 2 - TTF_FontHeight(fSlots) / 2, T().red, fSlots);
            nameRoom -= textWidth(inv, fSlots) + 12;
        } else {
            const int cap = bankCapacity(b.game);
            const bool full = b.occupiedSlots >= cap;
            const std::string slots = std::to_string(b.occupiedSlots) + " / " + std::to_string(cap);
            const SDL_Color sc = dim ? T().textMuted : full ? BP_FULL : T().text;
            drawText(slots, rightX - textWidth(slots, fSlots), ry + 12, sc, fSlots);
            constexpr int BAR_W = 96;
            fillRounded(rightX - BAR_W, ry + 39, BAR_W, 5, 2, T().bg);
            const int fillW = b.occupiedSlots > 0 ? std::max(4, BAR_W * std::min(b.occupiedSlots, cap) / cap) : 0;
            if (fillW) fillRounded(rightX - BAR_W, ry + 39, fillW, 5, 2, full ? BP_FULL : teal);
            nameRoom -= BAR_W + 12;

            // Badge beside the numbers: FULL, or the side it is already open on.
            std::string badge;
            SDL_Color bc = teal;
            if (otherSide) badge = i18n::get(bankSelTarget_ == Panel::Game ? StrKey::BsTagRight : StrKey::BsTagLeft);
            else if (full) { badge = i18n::get(StrKey::BsFull); bc = BP_FULL; }
            if (!badge.empty()) {
                TTF_Font* fB = uiFont(11, true);
                const int bw = measureTextTracked(badge, fB, 2) + 18;
                const int bx = rightX - BAR_W - 12 - bw;
                fillRounded(bx, ry + rh / 2 - 11, bw, 22, 6, SDL_Color{bc.r, bc.g, bc.b, 40});
                drawTextTracked(badge, bx + 9, ry + rh / 2 - TTF_FontHeight(fB) / 2, bc, fB, 2);
                nameRoom -= bw + 10;
            }
        }

        drawText(fitText(b.name, fName, nameRoom), x + 62, ry + 9, dim ? T().textMuted : T().text, fName);
        if (b.modified) {
            const std::string sub = i18n::fmt(StrKey::BsEdited, relativeDay(b.modified), dateShort(b.modified, true));
            drawText(fitText(sub, fSub, nameRoom), x + 62, ry + 34, T().textDim, fSub);
        }
    }
    SDL_RenderSetClipRect(renderer_, nullptr);

    if (scrolls) {
        const int trackY = y, trackH = BP_LIST_BOTTOM - 6 - y;
        const int thumbH = std::max(30, trackH * viewH / total);
        const int thumbY = trackY + (trackH - thumbH) * bankSelScroll_ / std::max(1, total - viewH);
        fillRounded(x + w - 5, trackY, 5, trackH, 2, T().buttonBg);
        fillRounded(x + w - 5, thumbY, 5, thumbH, 2, T().textMuted);
    }
}

// Three facts about what the box panel shows.
void UI::drawBankStats(Panel src, int x) {
    const int tileW = (PANEL_W - 24) / 3;
    struct Tile { std::string label, value, note; };
    Tile tiles[3];

    const int boxes = src == Panel::Preview ? (previewBankPath_.empty() ? 0 : previewBank_.boxCount())
                                            : panelBoxCount(src);
    int filled = 0, used = 0, total = 0;
    for (int b = 0; b < boxes; b++) {
        const auto& disp = getSlotDisplays(src, b);
        total += static_cast<int>(disp.size());
        int n = 0;
        for (const auto& sd : disp) if (!sd.empty) n++;
        filled += n;
        if (n) used++;
    }
    const bool isSave = src == Panel::Game && !isDualBankMode();
    tiles[0].label = i18n::get(isSave ? StrKey::StInSave : StrKey::StPokemon);
    tiles[1].label = i18n::get(StrKey::StBoxesUsed);
    tiles[2].label = i18n::get(isSave ? StrKey::StBackup : StrKey::StLastEdited);
    if (boxes > 0) {
        tiles[0].value = std::to_string(filled) + " / " + std::to_string(total);
        if (!isSave && total)
            tiles[0].note = i18n::fmt(StrKey::StFull, std::to_string(filled * 100 / total));
        tiles[1].value = std::to_string(used) + " / " + std::to_string(boxes);
    }
    if (isSave) {
        if (lastBackup_ == BackupOutcome::Saved) {
            tiles[2].value = clockOf(lastBackupWhen_);
            tiles[2].note = relativeDay(lastBackupWhen_);
        } else {
            tiles[2].value = "---";
        }
    } else {
        // Known already: no need to ask the SD card on every redraw.
        const time_t modified = src == Panel::Preview
            ? (previewBankPath_.empty() ? 0 : previewBankModified_)
            : src == Panel::Bank ? activeBankModified_ : leftBankModified_;
        // The date alone, with its year since nothing beside it says how
        // long ago it was.
        if (modified)
            tiles[2].value = dateShort(modified, true);
    }

    TTF_Font* fLabel = uiFont(11, true);
    TTF_Font* fValue = uiFont(19, true);
    TTF_Font* fNote  = uiFont(12);
    for (int i = 0; i < 3; i++) {
        const int tx = x + i * (tileW + 12);
        fillRounded(tx, BP_STATS_Y, tileW, BP_STATS_H, 12, T().panelBg);
        strokeRounded(tx, BP_STATS_Y, tileW, BP_STATS_H, 12, 1, T().panelBorder);
        const std::string label = toUpperUtf8(tiles[i].label);
        const std::string v = tiles[i].value.empty() ? "..." : tiles[i].value;
        const std::string& note = tiles[i].note;
        const int right = tx + tileW - 14;

        // The note ("5% full", "3 days ago") always sits at the end of the
        // label line, so a tile keeps one layout whatever its value.
        const int noteW = note.empty() ? 0 : textWidth(note, fNote);
        const int labelMax = tileW - 28 - (noteW ? noteW + 10 : 0);
        drawTextTracked(fitText(label, fLabel, std::max(20, labelMax)), tx + 14, BP_STATS_Y + 14,
                        T().textDim, fLabel, 2);
        if (!note.empty()) {
            const std::string n = fitText(note, fNote, tileW - 28 - 30);
            drawText(n, right - textWidth(n, fNote), BP_STATS_Y + 12, T().textDim, fNote);
        }
        drawText(fitText(v, fValue, tileW - 28), tx + 14, BP_STATS_Y + 33, T().text, fValue);
    }
}

void UI::drawBankSelectorFrame() {
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);
    drawBankPickerTopBar();

    const bool listLeft = bankSelListOnLeft();
    const int panelX = listLeft ? PANEL_X_R : PANEL_X_L;
    const int listX  = listLeft ? PANEL_X_L : PANEL_X_L + PANEL_W + 32;
    const int listW  = listLeft ? PANEL_X_R - 32 - PANEL_X_L : SCREEN_W - 32 - listX;

    drawBankList(listX, listW);

    // The box panel beside it; L/R browse its boxes.
    const Panel src = bankSelPreviewSource();
    std::string tag;
    if (src == Panel::Preview) {
        const auto& banks = bankManager_.list();
        if (bankSelCursor_ >= 0 && bankSelCursor_ < (int)banks.size())
            tag = i18n::get(StrKey::TagBank) + std::string(" \xc2\xb7 ") + banks[bankSelCursor_].name;
        else
            tag = i18n::get(StrKey::TagBank);
    }
    drawBoxPanel(src, true, panelX, previewBox_, tag);
    drawBankStats(src, panelX);

    // Same keys as before; All banks gains ZL/ZR to jump between games.
    if (bankManager_.isAllMode()) {
        const ButtonHint hints[] = {
            {"A", StrKey::HintOpen}, {"ZL ZR", StrKey::HintJumpGame},
            {"B", StrKey::HintBack}, {"-", StrKey::HintAbout},
        };
        drawFooterBar(hints, 4);
    } else {
        const ButtonHint hints[] = {
            {"A", StrKey::HintOpen},   {"X", StrKey::HintNew},
            {"Y", StrKey::HintRename}, {"+", StrKey::HintDelete},
            {"B", StrKey::HintBack},   {"-", StrKey::HintAbout},
        };
        drawFooterBar(hints, 6);
    }

    if (showDeleteConfirm_)
        drawDeleteConfirmPopup();
}

void UI::moveBankCursor(int dir) {
    const int count = bankSelRowCount();
    if (count == 0) return;
    bankSelCursor_ = (bankSelCursor_ + dir + count) % count;
    if (bankSelPreviewSource() == Panel::Preview) clearBankPreview();
}

void UI::jumpBankGroup(int dir) {
    const auto& banks = bankManager_.list();
    if (banks.empty()) return;
    const int n = static_cast<int>(banks.size());
    int i = std::clamp(bankSelCursor_, 0, n - 1);
    const GameType cur = banks[i].game;
    if (dir > 0) {
        while (i < n && banks[i].game == cur) i++;
        if (i >= n) i = 0;
    } else {
        // Back to the start of this group, then to the start of the one
        // before it (wrapping to the last).
        while (i > 0 && banks[i - 1].game == cur) i--;
        int j = i > 0 ? i - 1 : n - 1;
        const GameType prev = banks[j].game;
        while (j > 0 && banks[j - 1].game == prev) j--;
        i = j;
    }
    bankSelCursor_ = i;
    clearBankPreview();
}

void UI::handleBankSelectorInput(bool& running) {
    const auto& banks = bankManager_.list();
    const int bankCount = (int)banks.size();
    const bool onNew = bankSelCanCreate() && bankSelCursor_ == bankCount;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
            return;
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN)
            markDirty();

        // Delete confirmation takes priority
        if (showDeleteConfirm_) {
            handleDeleteConfirmEvent(event);
            continue;
        }

        if (event.type == SDL_CONTROLLERAXISMOTION) {
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
                int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
                updateStick(lx, ly);
            }
            // ZL / ZR jump between games in All banks, leaving L/R to page
            // through the preview. Edge-triggered, like the box overview.
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
                event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
                const bool left = event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT;
                const bool pressed = event.caxis.value > TRIGGER_DEADZONE;
                bool& was = left ? zlPressed_ : zrPressed_;
                if (pressed && !was && bankManager_.isAllMode()) {
                    jumpBankGroup(left ? -1 : +1);
                    markDirty();
                }
                was = pressed;
            }
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:   moveBankCursor(-1); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: moveBankCursor(+1); break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: {
                    // Browse the boxes of what the panel shows.
                    const int dir = event.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER ? -1 : +1;
                    const Panel src = bankSelPreviewSource();
                    const int boxes = src == Panel::Preview
                        ? (previewBankPath_.empty() ? 0 : previewBank_.boxCount())
                        : panelBoxCount(src);
                    if (boxes > 0) previewBox_ = (previewBox_ + dir + boxes) % boxes;
                    break;
                }
                case SDL_CONTROLLER_BUTTON_B: // Switch A = open (or create, on "New bank")
                    if (onNew)
                        beginTextInput(TextInputPurpose::CreateBank);
                    else if (bankCount > 0)
                        openSelectedBank();
                    break;
                case SDL_CONTROLLER_BUTTON_A: // Switch B = back
                    if (!activeBankName_.empty()) {
                        // Already have a bank loaded — return to main view
                        screen_ = AppScreen::MainView;
                        if (isDualBankMode() && leftBankName_.empty()) {
                            cursor_ = Cursor{};
                            cursor_.panel = Panel::Bank;
                        }
                    } else {
                        if (!allBanksMode_)
                            account_.unmountSave();
                        allBanksMode_ = false;
                        // Banks may have been created or renamed, and opening
                        // the save made a backup: the cards should say so.
                        refreshBankCounts();
                        screen_ = AppScreen::GameSelector;
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_Y: // Switch X = new
                    if (bankSelCanCreate())
                        beginTextInput(TextInputPurpose::CreateBank);
                    break;
                case SDL_CONTROLLER_BUTTON_X: // Switch Y = rename
                    if (!bankManager_.isAllMode() && bankCount > 0 && !onNew)
                        beginTextInput(TextInputPurpose::RenameBank);
                    break;
                case SDL_CONTROLLER_BUTTON_BACK: // - = about
                    showAbout_ = true;
                    break;
                case SDL_CONTROLLER_BUTTON_START: // + = delete
                    if (bankCount > 0 && !onNew && !bankManager_.isAllMode())
                        showDeleteConfirm_ = true;
                    break;
            }
        }

    }

    // Joystick repeat navigation
    if (stickDirY_ != 0 && bankSelRowCount() > 0 && !showDeleteConfirm_) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay) {
            moveBankCursor(stickDirY_ < 0 ? -1 : +1);
            stickMoveTime_ = now;
            stickMoved_ = true;
            markDirty();
        }
    }
}

void UI::openSelectedBank() {
    const auto& banks = bankManager_.list();
    if (bankSelCursor_ < 0 || bankSelCursor_ >= (int)banks.size())
        return;

    // Stray .bin files that aren't real bank files can't be opened
    if (!banks[bankSelCursor_].valid)
        return;

    const std::string& name = banks[bankSelCursor_].name;

    // Prevent loading same bank on both sides in dual mode
    if (isDualBankMode()) {
        if (bankSelTarget_ == Panel::Game && name == activeBankName_
            && banks[bankSelCursor_].game == selectedGame_) {
            showMessageAndWait(i18n::get(StrKey::AlreadyOpen),
                i18n::get(StrKey::BankAlreadyRight));
            return;
        }
        if (bankSelTarget_ == Panel::Bank && name == leftBankName_
            && banks[bankSelCursor_].game == selectedGame_) {
            showMessageAndWait(i18n::get(StrKey::AlreadyOpen),
                i18n::get(StrKey::BankAlreadyLeft));
            return;
        }
    }

    // In all-banks mode, set game type from the selected bank
    if (allBanksMode_ && bankSelTarget_ == Panel::Bank) {
        selectedGame_ = banks[bankSelCursor_].game;
        invalidateAllSlotDisplays();
        save_.setGameType(selectedGame_);
        bankLeft_.setGameType(selectedGame_);
    }

    showWorking(i18n::get(StrKey::LoadingBank));

    const time_t modified = banks[bankSelCursor_].modified;
    if (isDualBankMode() && bankSelTarget_ == Panel::Game) {
        leftBankPath_ = bankManager_.loadBank(name, bankLeft_);
        bankLeft_.setGameType(selectedGame_);
        leftBankName_ = name;
        leftBankModified_ = modified;
    } else {
        activeBankPath_ = bankManager_.loadBank(name, bank_);
        bank_.setGameType(selectedGame_);
        activeBankName_ = name;
        activeBankModified_ = modified;
    }

    // In dual mode, after loading the right bank, chain to left bank selector
    // (only if there's at least one other bank to choose from)
    if (isDualBankMode() && bankSelTarget_ == Panel::Bank && leftBankName_.empty()) {
        // In all-banks mode, switch to game-specific bank list for second bank
        if (allBanksMode_)
            bankManager_.init(basePath_, selectedGame_);

        if ((int)bankManager_.list().size() > 1) {
            bankSelTarget_ = Panel::Game;
            bankSelCursor_ = 0;
            bankSelScroll_ = 0;
            clearBankPreview();
            return;  // stay on BankSelector screen
        }
    }

    unsavedChanges_ = false;  // everything on screen was just read from disk
    screen_ = AppScreen::MainView;
    invalidateAllSlotDisplays();

    // Reset main view state
    cursor_ = Cursor{};
    cursor_.panel = bankSelTarget_;
    gameBox_ = 0;
    bankBox_ = 0;
    showDetail_ = false;
    showMenu_ = false;
    holding_ = false;
    heldPkm_ = Pokemon{};
    selectedSlots_.clear();
    heldMulti_.clear();
    heldMultiSlots_.clear();
}

// --- Delete Confirmation ---

void UI::drawDeleteConfirmPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    constexpr int POP_W = 500;
    constexpr int POP_H = 180;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().red, 2);

    const auto& banks = bankManager_.list();
    std::string bankName = (bankSelCursor_ >= 0 && bankSelCursor_ < (int)banks.size())
        ? banks[bankSelCursor_].name : "";

    drawTextCentered(i18n::fmt(StrKey::DeleteBankConfirm, bankName),
                     popX + POP_W / 2, popY + 50, T().text, font_);
    drawTextCentered(i18n::get(StrKey::CannotUndo),
                     popX + POP_W / 2, popY + 85, T().red, fontSmall_);
    drawTextCentered(i18n::get(StrKey::AConfirmBCancel),
                     popX + POP_W / 2, popY + POP_H - 25, T().textDim, fontSmall_);
}

void UI::handleDeleteConfirmEvent(const SDL_Event& event) {
    auto tryDelete = [&]() {
        const auto& banks = bankManager_.list();
        if (bankSelCursor_ < 0 || bankSelCursor_ >= (int)banks.size())
            return;
        const std::string& name = banks[bankSelCursor_].name;
        // Cannot delete a bank that is currently loaded
        if (name == activeBankName_ || (isDualBankMode() && name == leftBankName_)) {
            showDeleteConfirm_ = false;
            showMessageAndWait(i18n::get(StrKey::CannotDelete), i18n::get(StrKey::BankCurrentlyLoaded));
            return;
        }
        showWorking(i18n::get(StrKey::DeletingBank));
        bankManager_.deleteBank(name);
        int newCount = (int)bankManager_.list().size();
        if (bankSelCursor_ >= newCount && newCount > 0)
            bankSelCursor_ = newCount - 1;
        clearBankPreview();
        showDeleteConfirm_ = false;
    };

    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_B: // Switch A = confirm
                tryDelete();
                break;
            case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
                showDeleteConfirm_ = false;
                break;
        }
    }
}

// --- Text Input ---

void UI::beginTextInput(TextInputPurpose purpose) {
    textInputPurpose_ = purpose;
    textInputBuffer_.clear();
    textInputCursorPos_ = 0;

    if (purpose == TextInputPurpose::RenameBank) {
        const auto& banks = bankManager_.list();
        if (bankSelCursor_ >= 0 && bankSelCursor_ < (int)banks.size()) {
            renamingBankName_ = banks[bankSelCursor_].name;
            textInputBuffer_ = renamingBankName_;
            textInputCursorPos_ = (int)textInputBuffer_.size();
        }
    } else if (purpose == TextInputPurpose::RenameBoxName) {
        if (renamingBoxBank_) {
            textInputBuffer_ = renamingBoxBank_->getBoxName(renamingBoxIdx_);
            textInputCursorPos_ = (int)textInputBuffer_.size();
        }
    } else if (purpose == TextInputPurpose::SearchSpecies) {
        textInputBuffer_ = searchFilter_.speciesName;
        textInputCursorPos_ = (int)textInputBuffer_.size();
    } else if (purpose == TextInputPurpose::SearchOT) {
        textInputBuffer_ = searchFilter_.otName;
        textInputCursorPos_ = (int)textInputBuffer_.size();
    } else if (purpose == TextInputPurpose::SearchLevelMin) {
        if (searchFilter_.levelMin > 0)
            textInputBuffer_ = std::to_string(searchFilter_.levelMin);
        textInputCursorPos_ = (int)textInputBuffer_.size();
    } else if (purpose == TextInputPurpose::SearchLevelMax) {
        if (searchFilter_.levelMax > 0)
            textInputBuffer_ = std::to_string(searchFilter_.levelMax);
        textInputCursorPos_ = (int)textInputBuffer_.size();
    }

    SwkbdConfig kbd;
    swkbdCreate(&kbd, 0);
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetStringLenMax(&kbd, 32);
    if (purpose == TextInputPurpose::CreateBank)
        swkbdConfigSetHeaderText(&kbd, i18n::get(StrKey::EnterBankName).c_str());
    else if (purpose == TextInputPurpose::RenameBank)
        swkbdConfigSetHeaderText(&kbd, i18n::get(StrKey::RenameBank).c_str());
    else if (purpose == TextInputPurpose::RenameBoxName) {
        swkbdConfigSetHeaderText(&kbd, i18n::get(StrKey::RenameBox).c_str());
        swkbdConfigSetStringLenMax(&kbd, 16);
    } else if (purpose == TextInputPurpose::SearchSpecies)
        swkbdConfigSetHeaderText(&kbd, i18n::get(StrKey::SpeciesNameInput).c_str());
    else if (purpose == TextInputPurpose::SearchOT)
        swkbdConfigSetHeaderText(&kbd, i18n::get(StrKey::OtNameInput).c_str());
    else if (purpose == TextInputPurpose::SearchLevelMin)
        swkbdConfigSetHeaderText(&kbd, i18n::get(StrKey::MinLevel).c_str());
    else if (purpose == TextInputPurpose::SearchLevelMax)
        swkbdConfigSetHeaderText(&kbd, i18n::get(StrKey::MaxLevel).c_str());
    if (purpose == TextInputPurpose::RenameBank && !renamingBankName_.empty())
        swkbdConfigSetInitialText(&kbd, renamingBankName_.c_str());
    else if (!textInputBuffer_.empty())
        swkbdConfigSetInitialText(&kbd, textInputBuffer_.c_str());
    if (purpose == TextInputPurpose::SearchLevelMin || purpose == TextInputPurpose::SearchLevelMax) {
        swkbdConfigSetType(&kbd, SwkbdType_NumPad);
        swkbdConfigSetStringLenMax(&kbd, 3);
    }
    char result[64] = {};
    Result rc = swkbdShow(&kbd, result, sizeof(result));
    swkbdClose(&kbd);
    if (R_SUCCEEDED(rc) && result[0])
        commitTextInput(result);
    else if (R_SUCCEEDED(rc))
        commitTextInput("");
}

void UI::commitTextInput(const std::string& text) {
    if (textInputPurpose_ == TextInputPurpose::CreateBank) {
        {
            Bank temp;
            temp.setGameType(selectedGame_);
            size_t needed = temp.fileSize();
            struct statvfs vfs;
            if (statvfs("sdmc:/", &vfs) == 0) {
                size_t freeSpace = (size_t)vfs.f_bavail * vfs.f_bsize;
                if (freeSpace < needed) {
                    showMessageAndWait(i18n::get(StrKey::NotEnoughSpace),
                        i18n::fmt(StrKey::FreeNeedSpace, formatSize(freeSpace), formatSize(needed)));
                    return;
                }
            }
        }
        // Reject duplicate names with clear feedback instead of failing silently
        if (bankManager_.bankExists(text)) {
            showMessageAndWait(i18n::get(StrKey::BankNameExists),
                i18n::get(StrKey::BankNameExistsBody));
            return;
        }
        showWorking(i18n::get(StrKey::CreatingBank));
        if (bankManager_.createBank(text)) {
            // Select the newly created bank
            const auto& banks = bankManager_.list();
            for (int i = 0; i < (int)banks.size(); i++) {
                if (banks[i].name == text) {
                    bankSelCursor_ = i;
                    break;
                }
            }
            clearBankPreview();
        }
    } else if (textInputPurpose_ == TextInputPurpose::RenameBank) {
        // Reject renaming onto another existing bank with clear feedback.
        // (An unchanged name is a harmless no-op, so don't warn in that case.)
        if (text != renamingBankName_ && bankManager_.bankExists(text)) {
            showMessageAndWait(i18n::get(StrKey::BankNameExists),
                i18n::get(StrKey::BankNameExistsBody));
            return;
        }
        showWorking(i18n::get(StrKey::RenamingBank));
        if (bankManager_.renameBank(renamingBankName_, text)) {
            // Select the renamed bank
            const auto& banks = bankManager_.list();
            for (int i = 0; i < (int)banks.size(); i++) {
                if (banks[i].name == text) {
                    bankSelCursor_ = i;
                    break;
                }
            }
            clearBankPreview();
        }
    } else if (textInputPurpose_ == TextInputPurpose::RenameBoxName) {
        if (renamingBoxBank_ && !text.empty()) {
            renamingBoxBank_->setBoxName(renamingBoxIdx_, text);
            unsavedChanges_ = true;
        }
    } else if (textInputPurpose_ == TextInputPurpose::SearchSpecies) {
        searchFilter_.speciesName = text;
    } else if (textInputPurpose_ == TextInputPurpose::SearchOT) {
        searchFilter_.otName = text;
    } else if (textInputPurpose_ == TextInputPurpose::SearchLevelMin) {
        int val = text.empty() ? 0 : std::atoi(text.c_str());
        searchFilter_.levelMin = (val < 0) ? 0 : (val > 100 ? 100 : val);
    } else if (textInputPurpose_ == TextInputPurpose::SearchLevelMax) {
        int val = text.empty() ? 0 : std::atoi(text.c_str());
        searchFilter_.levelMax = (val < 0) ? 0 : (val > 100 ? 100 : val);
    }
}
