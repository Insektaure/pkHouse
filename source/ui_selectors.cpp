#include "ui.h"
#include "ui_util.h"
#include "i18n.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include <switch.h>

void UI::refreshBankCounts() {
    gameBankCounts_.clear();
    gameSelInfo_.clear();

    // Banks are per family (paired games share a folder): list each once.
    std::unordered_map<std::string, std::vector<std::string>> byFolder;
    auto banksOf = [&](GameType g) -> const std::vector<std::string>& {
        const std::string folder = bankFolderNameOf(g);
        auto it = byFolder.find(folder);
        if (it == byFolder.end())
            it = byFolder.emplace(folder, BankManager::bankNames(basePath_, g)).first;
        return it->second;
    };

    for (GameType g : availableGames_) {
        GameSelInfo& info = gameSelInfo_[g];
        info.banks = banksOf(g);
        gameBankCounts_[g] = static_cast<int>(info.banks.size());
        info.backups = backupStats(selectedProfile_, g, info.lastBackup);
    }

    allBanksTotal_ = 0;
    allBanksFamilies_ = 0;
    allBanksByFamily_.clear();
    for (GameType g : BankManager::FAMILY_GAMES) {
        const int n = static_cast<int>(banksOf(g).size());
        allBanksTotal_ += n;
        if (n > 0) {
            allBanksFamilies_++;
            allBanksByFamily_.push_back({g, n});
        }
    }

    rebuildGameSelList();
}

// --- Profile selector (UI 2.0) -----------------------------------------------------
//
// Laid out after external/UI_2.0 "Start · Select profile": a card per Switch
// profile with its icon, name and how many games it has saves for.

void UI::loadProfileSaveCounts() {
    const int count = account_.profileCount();
    profileSaveCounts_.assign(count, 0);
    constexpr int GAMES = static_cast<int>(sizeof(ALL_GAMES) / sizeof(ALL_GAMES[0]));
    const int total = count * GAMES;
    uint32_t lastDraw = 0;
    for (int i = 0; i < count; i++) {
        for (int g = 0; g < GAMES; g++) {
            if (account_.hasSaveData(i, ALL_GAMES[g])) profileSaveCounts_[i]++;
            // One check per game per profile: a bar is worth it, redrawn at
            // most every 30 ms so drawing never costs more than the checks.
            const uint32_t now = SDL_GetTicks();
            if (now - lastDraw >= 30) {
                lastDraw = now;
                showWorking(i18n::get(StrKey::LoadingProfiles), false,
                            static_cast<float>(i * GAMES + g + 1) / total);
            }
        }
    }
}

int UI::backupStats(int profile, GameType game, time_t& newest) const {
    newest = 0;
    if (profile < 0 || profile >= account_.profileCount()) return 0;

    // backups/<profile>/<game>/<profile>_YYYY-MM-DD_HH-MM-SS/ (see
    // buildBackupDir). The time is read from the folder name, which is what
    // the backup was named after, rather than from a file date.
    const std::string dir = basePath_ + "backups/" + account_.profiles()[profile].pathSafeName
                          + "/" + gamePathNameOf(game) + "/";
    DIR* runs = opendir(dir.c_str());
    if (!runs) return 0;
    int count = 0;
    while (dirent* r = readdir(runs)) {
        const std::string name = r->d_name;
        if (name.size() < 19) continue;
        struct tm t{};
        if (std::sscanf(name.c_str() + name.size() - 19, "%4d-%2d-%2d_%2d-%2d-%2d",
                        &t.tm_year, &t.tm_mon, &t.tm_mday,
                        &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
            continue;
        t.tm_year -= 1900;
        t.tm_mon  -= 1;
        t.tm_isdst = -1;
        const time_t when = mktime(&t);
        count++;
        if (when > newest) newest = when;
    }
    closedir(runs);
    return count;
}

std::string UI::relativeDay(time_t t) const {
    const time_t now = time(nullptr);
    // Calendar days, not 24-hour spans: last night is "yesterday".
    struct tm a = *localtime(&now);
    struct tm b = *localtime(&t);
    a.tm_hour = b.tm_hour = 12;
    a.tm_min = b.tm_min = a.tm_sec = b.tm_sec = 0;
    const long days = static_cast<long>(std::difftime(mktime(&a), mktime(&b)) / 86400.0 + 0.5);
    if (days <= 0)  return i18n::get(StrKey::RelToday);
    if (days == 1)  return i18n::get(StrKey::RelYesterday);
    if (days < 14)  return i18n::fmt(StrKey::RelDays,   std::to_string(days));
    if (days < 60)  return i18n::fmt(StrKey::RelWeeks,  std::to_string(days / 7));
    if (days < 365) return i18n::fmt(StrKey::RelMonths, std::to_string(days / 30));
    return i18n::fmt(StrKey::RelYears, std::to_string(days / 365));
}

void UI::drawProfileSelectorFrame() {
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);
    drawRect(0, 0, SCREEN_W, ACCENT_RULE_H, T().accent);
    drawLogo(32, 40);

    // Launch mode. This screen only exists when launched over a title - the
    // album applet goes straight to the game list - so it always says so.
    {
        TTF_Font* f = uiFont(13, true);
        const std::string mode = i18n::get(StrKey::ModeTitle);
        const int w = 16 + 8 + 8 + textWidth(mode, f) + 14;
        const int x = SCREEN_W - 32 - w, y = 23, h = 34;
        const SDL_Color ok = T().statusOk;
        fillRounded(x, y, w, h, 10, SDL_Color{ok.r, ok.g, ok.b, 36});
        fillDisc(x + 16, y + h / 2, 4, ok);
        drawText(mode, x + 16 + 8 + 8, y + h / 2 - TTF_FontHeight(f) / 2, ok, f);
    }

    drawTextCentered(i18n::get(StrKey::ProfTitle), SCREEN_W / 2, 160, T().text, uiFont(40, true));
    drawTextCentered(i18n::get(StrKey::ProfSubtitle), SCREEN_W / 2, 206, T().textDim, uiFont(17));

    const auto& profiles = account_.profiles();
    const int count = (int)profiles.size();
    if (count > 0) {
        // Up to eight profiles on a Switch: the cards narrow to fit.
        // Icon, name and save count, with the same margin above and below;
        // centred between the subtitle and the note at the bottom.
        constexpr int GAP = 24, CARD_H = 284, CARD_Y = 279;
        const int cardW = std::min(220, (INFO_W - (count - 1) * GAP) / count);
        const int icon = std::min(160, cardW - 40);
        const int totalW = count * cardW + (count - 1) * GAP;
        const int x0 = (SCREEN_W - totalW) / 2;

        // Placeholder tints for a profile without an icon.
        const SDL_Color tints[3] = {T().accentSave, SDL_Color{240, 128, 90, 255}, T().accentBank};

        TTF_Font* fName   = uiFont(20, true);
        TTF_Font* fSaves  = uiFont(14, true);
        for (int i = 0; i < count; i++) {
            const int x = x0 + i * (cardW + GAP);
            const bool cur = (i == profileSelCursor_);
            const SDL_Color cardBg = cur ? T().slotFull : T().panelBg;
            if (cur)
                strokeRounded(x - 7, CARD_Y - 7, cardW + 14, CARD_H + 14, 24, 4, T().accent);
            fillRounded(x, CARD_Y, cardW, CARD_H, 18, cardBg);
            strokeRounded(x, CARD_Y, cardW, CARD_H, 18, 1, cur ? T().cellBorder : T().panelBorder);

            const int ix = x + (cardW - icon) / 2, iy = CARD_Y + 23;
            if (profiles[i].iconTexture) {
                blitRounded(profiles[i].iconTexture, ix, iy, icon, icon, 22, cardBg);
            } else {
                const SDL_Color t = tints[i % 3];
                const SDL_Color bg = {static_cast<Uint8>((t.r + cardBg.r * 3) / 4),
                                      static_cast<Uint8>((t.g + cardBg.g * 3) / 4),
                                      static_cast<Uint8>((t.b + cardBg.b * 3) / 4), 255};
                fillRounded(ix, iy, icon, icon, 22, bg);
                const std::string& nick = profiles[i].nickname;
                if (!nick.empty()) {
                    // The whole first letter, however many bytes it takes.
                    const unsigned char lead = static_cast<unsigned char>(nick[0]);
                    const size_t len = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2
                                     : (lead & 0xF0) == 0xE0 ? 3 : 4;
                    const std::string initial = toUpperUtf8(nick.substr(0, len));
                    drawTextCentered(initial, ix + icon / 2, iy + icon / 2, t,
                                     uiFont(std::max(24, icon * 2 / 5), true));
                }
            }

            const int cx = x + cardW / 2;
            const int nameY = iy + icon + 30;
            drawTextCentered(fitText(profiles[i].nickname, fName, cardW - 24), cx, nameY,
                             T().text, fName);

            if (i < (int)profileSaveCounts_.size()) {
                const int saves = profileSaveCounts_[i];
                const std::string line = saves == 0 ? i18n::get(StrKey::ProfNoSaves)
                    : i18n::fmt(saves == 1 ? StrKey::ProfSavesOne : StrKey::ProfSavesMany,
                                std::to_string(saves));
                drawTextCentered(fitText(line, fSaves, cardW - 20), cx, nameY + 40,
                                 T().textDim, fSaves);
            }
        }
    }

    drawTextCentered(fitText(i18n::get(StrKey::AppletNote), uiFont(13), INFO_W), SCREEN_W / 2, 637,
                     T().textDim, uiFont(13));

    const ButtonHint hints[] = {
        {"A", StrKey::HintSelect2},
        {"-", StrKey::HintAbout},
        {"+", StrKey::HintQuit},
    };
    drawFooterBar(hints, 3);
}

void UI::handleProfileSelectorInput(bool& running) {
    int count = account_.profileCount();
    if (count == 0) return;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
            return;
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN)
            markDirty();

        if (event.type == SDL_CONTROLLERAXISMOTION) {
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
                int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
                updateStick(lx, ly);
            }
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    profileSelCursor_ = (profileSelCursor_ + count - 1) % count;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    profileSelCursor_ = (profileSelCursor_ + 1) % count;
                    break;
                case SDL_CONTROLLER_BUTTON_B: // Switch A = select
                    selectProfile(profileSelCursor_);
                    break;
                case SDL_CONTROLLER_BUTTON_BACK: // - = about
                    showAbout_ = true;
                    break;
                case SDL_CONTROLLER_BUTTON_START:
                    running = false;
                    break;
            }
        }
    }

    // Joystick repeat navigation
    if (stickDirX_ != 0 || stickDirY_ != 0) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay) {
            if (stickDirX_ < 0)
                profileSelCursor_ = (profileSelCursor_ + count - 1) % count;
            else if (stickDirX_ > 0)
                profileSelCursor_ = (profileSelCursor_ + 1) % count;
            stickMoveTime_ = now;
            stickMoved_ = true;
            markDirty();
        }
    }
}

void UI::selectProfile(int index) {
    selectedProfile_ = index;

    // Build filtered game list: only games with save data for this profile
    availableGames_.clear();
    for (GameType g : ALL_GAMES) {
        if (account_.hasSaveData(index, g))
            availableGames_.push_back(g);
    }

    if (availableGames_.empty()) {
        showMessageAndWait(i18n::get(StrKey::NoSaveData),
            i18n::get(StrKey::NoSaveDataBody));
        return;
    }

    refreshBankCounts();

    gameSelCursor_ = 0;
    gameSelScroll_ = 0;
    gameSelFilter_ = GF_ALL;
    gameSelOnAllBanks_ = false;
    gameSelOnGts_ = false;
    rebuildGameSelList();
    loadGameIcons();   // shows its own progress
    screen_ = AppScreen::GameSelector;
}

// --- Game Icons ---

void UI::loadGameIcons() {
    freeGameIcons();
    std::string cacheDir = basePath_ + "cache/";
    mkdir(cacheDir.c_str(), 0755);

    // One pass: the cached JPEG when there is one, the system's otherwise
    // (saved to the cache for next time). Progress after each game, since the
    // system fetch is the slow part and the wait is otherwise unexplained.
    const int total = static_cast<int>(availableGames_.size());
    auto report = [&](int done) {
        showWorking(i18n::get(StrKey::LoadingGameIcons), false,
                    total ? static_cast<float>(done) / total : 1.0f);
    };
    report(0);

    bool nsReady = false;
    for (int i = 0; i < total; i++) {
        const GameType game = availableGames_[i];
        char hexId[32];
        std::snprintf(hexId, sizeof(hexId), "%016lX", titleIdOf(game));
        const std::string cachePath = cacheDir + hexId + ".jpg";

        if (SDL_Surface* surf = IMG_Load(cachePath.c_str())) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
            SDL_FreeSurface(surf);
            if (tex)
                gameIconCache_[game] = tex;
            report(i + 1);
            continue;
        }

        if (!nsReady) {
            nsInitialize();
            nsReady = true;
        }
        NsApplicationControlData ctrlData;
        std::memset(&ctrlData, 0, sizeof(ctrlData));
        uint64_t controlSize = 0;
        Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage,
                        titleIdOf(game), &ctrlData, sizeof(ctrlData), &controlSize);
        if (R_SUCCEEDED(rc) && controlSize > sizeof(NacpStruct)) {
            const size_t iconSize = controlSize - sizeof(NacpStruct);

            // Save JPEG to cache
            if (FILE* f = std::fopen(cachePath.c_str(), "wb")) {
                std::fwrite(ctrlData.icon, 1, iconSize, f);
                std::fclose(f);
            }

            // Decode and create texture
            if (SDL_RWops* rw = SDL_RWFromMem(ctrlData.icon, iconSize)) {
                if (SDL_Surface* surf = IMG_Load_RW(rw, 1)) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
                    SDL_FreeSurface(surf);
                    if (tex)
                        gameIconCache_[game] = tex;
                }
            }
        }
        report(i + 1);
    }
    if (nsReady)
        nsExit();
}

void UI::freeGameIcons() {
    for (auto& [game, tex] : gameIconCache_) {
        if (tex)
            SDL_DestroyTexture(tex);
    }
    gameIconCache_.clear();
}

// --- Game Selector (UI 2.0) --------------------------------------------------------
//
// Laid out after external/UI_2.0 "1a · Choose a game": the Online GTS and All
// banks tiles on top, the games as a scrolling grid of cards filtered by
// generation with L/R, and the highlighted game's details on the right.

namespace {

// Prefixed: these functions are UI members, where an unprefixed name would
// lose to a UI constant of the same name.
constexpr int GS_LEFT      = 38;
constexpr int GS_GRID_R    = 860;             // grid right edge
constexpr int GS_COLS      = 4;
constexpr int GS_CARD_W    = 196;
constexpr int GS_CARD_H    = 164;
constexpr int GS_ROW_H     = 176;             // card + gap
constexpr int GS_TILES_Y   = 86;
constexpr int GS_TILES_H   = 60;
constexpr int GS_FILTER_Y  = 157;
constexpr int GS_FILTER_H  = 42;
constexpr int GS_GRID_Y    = 218;
constexpr int GS_GRID_BOTTOM = 658;           // cards are clipped here
constexpr int GS_FULL_ROWS = 2;               // rows kept fully in view
constexpr int GS_PANEL_X   = 908;
constexpr int GS_PANEL_Y   = 80;
constexpr int GS_PANEL_W   = 340;
constexpr int GS_PANEL_H   = 576;

int baselineTopGs(TTF_Font* f, int baseline) { return baseline - TTF_FontAscent(f); }

// Placeholder for a game whose icon could not be read: a short tag on a tint.
struct GamePlaceholder { const char* tag; SDL_Color tint; };
GamePlaceholder placeholderOf(GameType g) {
    switch (g) {
        case GameType::GP: return {"LGP", {235, 200, 60, 255}};
        case GameType::GE: return {"LGE", {200, 160, 120, 255}};
        case GameType::Sw: return {"SW",  {90, 170, 240, 255}};
        case GameType::Sh: return {"SH",  {235, 95, 95, 255}};
        case GameType::BD: return {"BD",  {100, 150, 240, 255}};
        case GameType::SP: return {"SP",  {240, 140, 190, 255}};
        case GameType::LA: return {"LA",  {215, 190, 120, 255}};
        case GameType::S:  return {"SC",  {235, 90, 80, 255}};
        case GameType::V:  return {"VI",  {170, 120, 230, 255}};
        case GameType::ZA: return {"ZA",  {95, 200, 160, 255}};
        default:
            return isFRLG(g) && gameInfo(g).gamePathName[0] == 'F'
                ? GamePlaceholder{"FR", {240, 130, 70, 255}}
                : GamePlaceholder{"LG", {110, 200, 110, 255}};
    }
}

SDL_Color mix(SDL_Color a, SDL_Color b, int aPart, int bPart) {
    const int t = aPart + bPart;
    return {static_cast<Uint8>((a.r * aPart + b.r * bPart) / t),
            static_cast<Uint8>((a.g * aPart + b.g * bPart) / t),
            static_cast<Uint8>((a.b * aPart + b.b * bPart) / t), 255};
}

std::string dateOf(time_t t) {
    char buf[16];
    const struct tm* tm = localtime(&t);
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    return buf;
}

} // anonymous namespace

int UI::gameGeneration(GameType g) {
    if (isSV(g) || g == GameType::ZA)                      return 9;
    if (isSwSh(g) || isBDSP(g) || g == GameType::LA)       return 8;
    if (isLGPE(g))                                         return 7;
    return 3;
}

bool UI::gameMatchesFilter(GameType g, int filter) const {
    switch (filter) {
        case GF_ALL:    return true;
        case GF_RECENT: {
            // Backed up in the last 30 days.
            auto it = gameSelInfo_.find(g);
            return it != gameSelInfo_.end() && it->second.lastBackup != 0
                && std::difftime(time(nullptr), it->second.lastBackup) <= 30.0 * 86400.0;
        }
        case GF_GEN9:   return gameGeneration(g) == 9;
        case GF_GEN8:   return gameGeneration(g) == 8;
        case GF_GEN7:   return gameGeneration(g) == 7;
        case GF_GEN3:   return gameGeneration(g) == 3;
    }
    return false;
}

std::vector<int> UI::visibleGameFilters() const {
    std::vector<int> out{GF_ALL};
    for (int f = GF_RECENT; f < GF_COUNT; f++) {
        for (GameType g : availableGames_) {
            if (gameMatchesFilter(g, f)) { out.push_back(f); break; }
        }
    }
    return out;
}

void UI::rebuildGameSelList() {
    // A filter that has emptied (a game removed, a backup aged out) falls back
    // to All rather than showing nothing.
    const auto filters = visibleGameFilters();
    if (std::find(filters.begin(), filters.end(), gameSelFilter_) == filters.end())
        gameSelFilter_ = GF_ALL;

    gameSelList_.clear();
    for (int i = 0; i < (int)availableGames_.size(); i++)
        if (gameMatchesFilter(availableGames_[i], gameSelFilter_))
            gameSelList_.push_back(i);

    if (gameSelCursor_ >= (int)gameSelList_.size())
        gameSelCursor_ = std::max(0, (int)gameSelList_.size() - 1);
    const int row = gameSelCursor_ / GS_COLS;
    if (row < gameSelScroll_) gameSelScroll_ = row;
    if (row >= gameSelScroll_ + GS_FULL_ROWS) gameSelScroll_ = row - GS_FULL_ROWS + 1;
}

void UI::stepGameFilter(int dir) {
    const auto filters = visibleGameFilters();
    auto it = std::find(filters.begin(), filters.end(), gameSelFilter_);
    int idx = it == filters.end() ? 0 : static_cast<int>(it - filters.begin());
    idx = (idx + dir + static_cast<int>(filters.size())) % static_cast<int>(filters.size());
    gameSelFilter_ = filters[idx];
    gameSelCursor_ = 0;
    gameSelScroll_ = 0;
    gameSelOnGts_ = gameSelOnAllBanks_ = false;
    rebuildGameSelList();
}

std::vector<std::string> UI::wrapText(const std::string& text, TTF_Font* f, int maxW,
                                      int maxLines) {
    // Breaks at spaces; a word wider than the line on its own - which is what
    // a Japanese or Chinese sentence is, having no spaces - is broken between
    // characters instead of being cut. Each line remembers where it starts in
    // `text`, so when there are more lines than room the last one shown is
    // simply the rest of the text, cut to fit.
    struct Line { std::string s; size_t start; };
    std::vector<Line> out;
    std::string line;
    size_t lineStart = 0;

    size_t i = 0;
    while (i <= text.size()) {
        size_t sp = text.find(' ', i);
        if (sp == std::string::npos) sp = text.size();
        const std::string word = text.substr(i, sp - i);
        const std::string joined = line.empty() ? word : line + " " + word;
        if (textWidth(joined, f) <= maxW) {
            if (line.empty()) lineStart = i;
            line = joined;
        } else if (!line.empty() && textWidth(word, f) <= maxW) {
            out.push_back({line, lineStart});
            line = word;
            lineStart = i;
        } else {
            // Too wide even alone: character by character, carrying on from
            // whatever the current line already holds.
            if (!line.empty()) { out.push_back({line, lineStart}); line.clear(); }
            lineStart = i;
            for (size_t c = 0; c < word.size(); ) {
                const unsigned char lead = static_cast<unsigned char>(word[c]);
                const size_t len = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2
                                 : (lead & 0xF0) == 0xE0 ? 3 : 4;
                const std::string cp = word.substr(c, len);
                if (!line.empty() && textWidth(line + cp, f) > maxW) {
                    out.push_back({line, lineStart});
                    line.clear();
                    lineStart = i + c;
                }
                line += cp;
                c += len;
            }
        }
        if (sp >= text.size()) break;
        i = sp + 1;
    }
    if (!line.empty()) out.push_back({line, lineStart});

    std::vector<std::string> lines;
    for (int n = 0; n < (int)out.size() && n < maxLines; n++) {
        const bool last = (n == maxLines - 1) && (int)out.size() > maxLines;
        lines.push_back(last ? fitText(text.substr(out[n].start), f, maxW) : out[n].s);
    }
    return lines;
}

void UI::drawGameIcon(GameType g, int x, int y, int size, int radius, SDL_Color bg) {
    auto it = gameIconCache_.find(g);
    if (it != gameIconCache_.end() && it->second) {
        blitRounded(it->second, x, y, size, size, radius, bg);
        return;
    }
    const GamePlaceholder ph = placeholderOf(g);
    fillRounded(x, y, size, size, radius, mix(ph.tint, bg, 1, 3));
    strokeRounded(x, y, size, size, radius, 1, mix(ph.tint, bg, 1, 2));
    drawTextCentered(ph.tag, x + size / 2, y + size / 2, ph.tint,
                     uiFont(std::max(12, size * 3 / 10), true));
}

// --- Top bar -------------------------------------------------------------------------

SDL_Color UI::gameTint(GameType g) { return placeholderOf(g).tint; }

void UI::drawCheckDisc(int cx, int cy, int radius, SDL_Color disc, SDL_Color tick) {
    fillDisc(cx, cy, radius, disc);
    if (!iconCheck_) return;
    SDL_SetTextureColorMod(iconCheck_, tick.r, tick.g, tick.b);
    const int s = radius * 3 / 2;
    SDL_Rect dst = {cx - s / 2, cy - s / 2, s, s};
    SDL_RenderCopy(renderer_, iconCheck_, nullptr, &dst);
    SDL_SetTextureColorMod(iconCheck_, 255, 255, 255);
}

std::vector<std::string> UI::wrapChars(const std::string& text, TTF_Font* f, int maxW,
                                       int maxLines) {
    std::vector<std::string> lines;
    std::string line;
    for (size_t i = 0; i < text.size(); ) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        const size_t len = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : 4;
        const std::string cp = text.substr(i, len);
        if (!line.empty() && textWidth(line + cp, f) > maxW) {
            if ((int)lines.size() == maxLines - 1) {
                lines.push_back(fitText(line + text.substr(i), f, maxW));
                return lines;
            }
            lines.push_back(line);
            line.clear();
        }
        line += cp;
        i += len;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

void UI::drawFlowTopBar(const std::string& title, const std::vector<std::string>& steps,
                        int current) {
    drawRect(0, 0, SCREEN_W, ACCENT_RULE_H, T().accent);
    const int cy = 40;
    int x = drawLogo(32, cy) + 16;
    drawRect(x, cy - 14, 1, 28, T().divider);
    x += 17;
    TTF_Font* fTitle = uiFont(20, true);
    drawText(title, x, baselineTopGs(fTitle, cy + 8), T().text, fTitle);
    drawSteps(steps, current, 634, cy);
}

void UI::drawSteps(const std::vector<std::string>& steps, int current, int centerX, int cy) {
    if (steps.empty()) return;

    TTF_Font* fStep = uiFont(15, true);
    TTF_Font* fNum  = uiFont(13, true);
    constexpr int H = 36, NUM_R = 12, CHEV = 22;
    std::vector<int> widths;
    int total = 0;
    for (const auto& st : steps) {
        widths.push_back(6 + NUM_R * 2 + 8 + textWidth(st, fStep) + 14);
        total += widths.back();
    }
    total += CHEV * (static_cast<int>(steps.size()) - 1);
    int sx = centerX - total / 2;
    for (size_t i = 0; i < steps.size(); i++) {
        const bool cur = (static_cast<int>(i) == current);
        const bool done = (static_cast<int>(i) < current);
        if (cur) {
            fillRounded(sx, cy - H / 2, widths[i], H, H / 2, T().buttonBg);
            strokeRounded(sx, cy - H / 2, widths[i], H, H / 2, 1, T().buttonBorder);
        }
        const int ncx = sx + 6 + NUM_R;
        const SDL_Color ok = T().statusOk;
        if (done) {
            drawCheckDisc(ncx, cy, NUM_R, SDL_Color{ok.r, ok.g, ok.b, 50}, ok);
        } else {
            fillDisc(ncx, cy, NUM_R, cur ? T().accent : T().buttonBg);
            drawTextCentered(std::to_string(i + 1), ncx, cy, cur ? T().keyCapText : T().textDim, fNum);
        }
        drawText(steps[i], ncx + NUM_R + 8, cy - TTF_FontHeight(fStep) / 2,
                 done ? ok : cur ? T().text : T().textMuted, fStep);
        sx += widths[i];
        if (i + 1 < steps.size()) {
            fillArrow(sx + CHEV / 2.0f, cy, 5, ArrowDir::Right, T().textMuted);
            sx += CHEV;
        }
    }
}

void UI::drawGameSelTopBar() {
    // Steps: game, backup, bank. Without a save there is no backup step.
    const bool withSave = selectedProfile_ >= 0 && !appletMode_;
    std::vector<std::string> steps{i18n::get(StrKey::StepGame)};
    if (withSave) steps.push_back(i18n::get(StrKey::StepBackup));
    steps.push_back(i18n::get(StrKey::StepBank));
    drawFlowTopBar(i18n::get(StrKey::GsTitle), steps, 0);
    drawGameSelTopRight();
}

// The selected profile's chip, right-aligned at `rightX`. Returns where the
// next thing to its left should end; `rightX` itself when there is no profile.
int UI::drawProfileChip(int rightX, int cy) {
    if (selectedProfile_ < 0 || selectedProfile_ >= account_.profileCount())
        return rightX;
    const UserProfile& p = account_.profiles()[selectedProfile_];
    TTF_Font* f = uiFont(16, true);
    const std::string name = fitText(p.nickname, f, 160);
    const int w = 8 + 32 + 10 + textWidth(name, f) + 16;
    const int rx = rightX - w;
    fillRounded(rx, cy - 23, w, 46, 23, T().panelBg);
    strokeRounded(rx, cy - 23, w, 46, 23, 1, T().panelBorder);
    if (p.iconTexture)
        blitRounded(p.iconTexture, rx + 8, cy - 16, 32, 32, 16, T().panelBg);
    else {
        fillDisc(rx + 24, cy, 16, T().badgeBg);
        drawTextCentered(p.nickname.substr(0, 1), rx + 24, cy, T().accent, uiFont(15, true));
    }
    drawText(name, rx + 8 + 32 + 10, cy - TTF_FontHeight(f) / 2, T().text, f);
    return rx - 12;
}

void UI::drawGameSelTopRight() {
    const int cy = 40;
    // Right: launch mode, then the profile.
    int rx = drawProfileChip(SCREEN_W - 32, cy);
    {
        TTF_Font* f = uiFont(13, true);
        const bool applet = appletMode_ || selectedProfile_ < 0;
        const std::string mode = i18n::get(applet ? StrKey::ModeApplet : StrKey::ModeTitle);
        const SDL_Color c = applet ? T().statusWarn : T().statusOk;
        const int w = 14 + 8 + 8 + textWidth(mode, f) + 12;
        rx -= w;
        fillRounded(rx, cy - 17, w, 34, 10, SDL_Color{c.r, c.g, c.b, 36});
        fillDisc(rx + 14, cy, 4, c);
        drawText(mode, rx + 14 + 8 + 8, cy - TTF_FontHeight(f) / 2, c, f);
    }
}

// --- GTS and All banks tiles -----------------------------------------------------------

void UI::drawGameSelTiles() {
    constexpr int GTS_W = 531, GAP = 12;
    const SDL_Rect gts = {GS_LEFT, GS_TILES_Y, GTS_W, GS_TILES_H};
    const SDL_Rect all = {GS_LEFT + GTS_W + GAP, GS_TILES_Y, GS_GRID_R - (GS_LEFT + GTS_W + GAP), GS_TILES_H};

    TTF_Font* fTitle = uiFont(17, true);
    TTF_Font* fSub   = uiFont(13);
    auto tile = [&](const SDL_Rect& r, bool focused, SDL_Texture* icon, SDL_Color iconBg,
                    SDL_Color iconTint, const std::string& title, const std::string& sub,
                    bool motif) {
        if (focused)
            strokeRounded(r.x - 6, r.y - 6, r.w + 12, r.h + 12, 18, 3, T().accent);
        fillRounded(r.x, r.y, r.w, r.h, 12, focused ? T().slotFull : T().panelBg);

        // The GTS keeps the old band's motif - stars, a slice of globe and a
        // dotted route between two points - so it stands out from the plain
        // tiles. Kept right of the text, and inside the tile's straight edges
        // so nothing crosses the rounded corners.
        if (motif) {
            const SDL_Rect clip = {r.x + 12, r.y + 1, r.w - 24, r.h - 2};
            drawGtsStars({r.x + 255, r.y + 6, 220, r.h - 12}, 8, 0x5EEDu);
            drawGtsGlobe(clip, r.x + 395, r.y + r.h / 2, 150, 38);
            SDL_RenderSetClipRect(renderer_, &clip);
            drawGtsLink(r.x + 300, r.y + 44, r.x + 478, r.y + 15, 40, 200, 4, 3);
            SDL_RenderSetClipRect(renderer_, nullptr);
        }
        strokeRounded(r.x, r.y, r.w, r.h, 12, 1, T().panelBorder);
        const int icx = r.x + 14 + 18, icy = r.y + r.h / 2;
        fillDisc(icx, icy, 18, iconBg);
        if (icon) {
            SDL_SetTextureColorMod(icon, iconTint.r, iconTint.g, iconTint.b);
            SDL_Rect dst = {icx - 11, icy - 11, 22, 22};
            SDL_RenderCopy(renderer_, icon, nullptr, &dst);
            SDL_SetTextureColorMod(icon, 255, 255, 255);
        }
        const int tx = r.x + 63, tw = r.w - 63 - 30;
        drawText(fitText(title, fTitle, tw), tx, r.y + 9, T().text, fTitle);
        drawText(fitText(sub, fSub, tw), tx, r.y + 33, T().textDim, fSub);
        fillArrow(r.x + r.w - 18.0f, icy, 6, ArrowDir::Right, T().textDim);
    };

    tile(gts, gameSelOnGts_, iconGlobe_, T().accent, T().keyCapText,
         i18n::get(StrKey::GtsTitle), i18n::get(StrKey::GtsSubtitle), true);
    const SDL_Color teal = T().accentBank;
    tile(all, gameSelOnAllBanks_, iconBank_, SDL_Color{teal.r, teal.g, teal.b, 50}, teal,
         i18n::get(StrKey::AllBanks),
         i18n::fmt(StrKey::AllBanksSub, std::to_string(allBanksTotal_),
                   std::to_string(allBanksFamilies_)), false);
}

// --- Filters -------------------------------------------------------------------------

void UI::drawGameSelFilters() {
    const int cy = GS_FILTER_Y + GS_FILTER_H / 2;
    TTF_Font* fKey   = uiFont(12, true);
    TTF_Font* fTab   = uiFont(14, true);
    TTF_Font* fCount = uiFont(11, true);

    auto keyCap = [&](int x, const char* k) {
        fillRounded(x, cy - 10, 22, 20, 5, T().keyCap);
        drawTextCentered(k, x + 11, cy, T().keyCapText, fKey);
    };

    const auto filters = visibleGameFilters();
    auto label = [&](int f) -> std::string {
        switch (f) {
            case GF_ALL:    return i18n::get(StrKey::TabAll);
            case GF_RECENT: return i18n::get(StrKey::TabRecent);
            case GF_GEN9:   return i18n::fmt(StrKey::TabGen, "9");
            case GF_GEN8:   return i18n::fmt(StrKey::TabGen, "8");
            case GF_GEN7:   return i18n::fmt(StrKey::TabGen, "7");
            default:        return i18n::fmt(StrKey::TabGen, "3");
        }
    };
    auto count = [&](int f) {
        int n = 0;
        for (GameType g : availableGames_) if (gameMatchesFilter(g, f)) n++;
        return n;
    };

    int x = GS_LEFT - 6;
    keyCap(x, "L");
    x += 22 + 9;

    std::vector<int> widths;
    int total = 5;
    for (int f : filters) {
        widths.push_back(12 + textWidth(label(f), fTab) + 5 + textWidth(std::to_string(count(f)), fCount) + 12);
        total += widths.back() + 2;
    }
    total += 3;
    fillRounded(x, GS_FILTER_Y, total, GS_FILTER_H, 12, T().panelBg);
    strokeRounded(x, GS_FILTER_Y, total, GS_FILTER_H, 12, 1, T().panelBorder);
    int tx = x + 5;
    for (size_t i = 0; i < filters.size(); i++) {
        const bool cur = filters[i] == gameSelFilter_;
        if (cur) fillRounded(tx, GS_FILTER_Y + 5, widths[i], GS_FILTER_H - 10, 9, T().slotFull);
        const std::string l = label(filters[i]);
        const int base = cy + 5;
        drawText(l, tx + 12, baselineTopGs(fTab, base), cur ? T().text : T().textDim, fTab);
        drawText(std::to_string(count(filters[i])), tx + 12 + textWidth(l, fTab) + 5,
                 baselineTopGs(fCount, base), cur ? T().accent : T().textMuted, fCount);
        tx += widths[i] + 2;
    }
    keyCap(x + total + 9, "R");

    TTF_Font* fInfo = uiFont(13);
    const std::string info = i18n::fmt(StrKey::GsCount, std::to_string(gameSelList_.size()),
                                       std::to_string(availableGames_.size()));
    drawText(info, 884 - textWidth(info, fInfo), cy - TTF_FontHeight(fInfo) / 2, T().textDim, fInfo);
}

// --- Cards ---------------------------------------------------------------------------

void UI::drawGameCard(int listIdx, const SDL_Rect& r, bool isCursor) {
    const GameType g = availableGames_[gameSelList_[listIdx]];
    const GameSelInfo& info = gameSelInfo_[g];

    if (isCursor)
        strokeRounded(r.x - 6, r.y - 6, r.w + 12, r.h + 12, 18, 3, T().accent);
    const SDL_Color bg = isCursor ? T().slotFull : T().panelBg;
    fillRounded(r.x, r.y, r.w, r.h, 12, bg);
    strokeRounded(r.x, r.y, r.w, r.h, 12, 1, T().panelBorder);

    // The icon carries the card; the backup date lives in the detail panel.
    // 90 is as large as it gets with two lines of name under it: 12px above
    // the icon and about as much under the second line.
    constexpr int ICON = 90;
    const int ix = r.x + 15, iy = r.y + 12;
    drawGameIcon(g, ix, iy, ICON, 14, bg);
    // Save found: only meaningful with a profile, where every listed game has one.
    if (selectedProfile_ >= 0) {
        fillDisc(ix + ICON - 3, iy + ICON - 3, 7, bg);
        fillDisc(ix + ICON - 3, iy + ICON - 3, 5, T().statusOk);
    }

    // Generation, and the family's bank count.
    TTF_Font* fGen = uiFont(11, true);
    const std::string gen = i18n::fmt(StrKey::GenLabel, std::to_string(gameGeneration(g)));
    const int genW = measureTextTracked(gen, fGen, 2);
    drawTextTracked(gen, r.x + r.w - 15 - genW, r.y + 12, T().textDim, fGen, 2);
    {
        // Bank icon and count; the word is left to the detail panel.
        TTF_Font* f = uiFont(12, true);
        const std::string fit = std::to_string(info.banks.size());
        const int w = 8 + 12 + 5 + textWidth(fit, f) + 8;
        const int bx = r.x + r.w - 15 - w, by = r.y + 34;
        const SDL_Color teal = T().accentBank;
        fillRounded(bx, by, w, 22, 6, SDL_Color{teal.r, teal.g, teal.b, 40});
        if (iconBank_) {
            SDL_SetTextureColorMod(iconBank_, teal.r, teal.g, teal.b);
            SDL_Rect dst = {bx + 8, by + 5, 12, 12};
            SDL_RenderCopy(renderer_, iconBank_, nullptr, &dst);
            SDL_SetTextureColorMod(iconBank_, 255, 255, 255);
        }
        drawText(fit, bx + 8 + 12 + 5, by + 11 - TTF_FontHeight(f) / 2, teal, f);
    }

    // Name, up to two lines, under the icon.
    TTF_Font* fName = uiFont(16, true);
    const auto lines = wrapText(gameDisplayNameOf(g), fName, r.w - 30);
    for (size_t i = 0; i < lines.size(); i++)
        drawText(lines[i], r.x + 15, iy + ICON + 10 + static_cast<int>(i) * 20, T().text, fName);
}

// --- Detail panel --------------------------------------------------------------------

void UI::drawGameDetailPanel() {
    const int X = GS_PANEL_X, Y = GS_PANEL_Y, W = GS_PANEL_W, H = GS_PANEL_H;
    fillRounded(X, Y, W, H, 16, T().panelBg);
    strokeRounded(X, Y, W, H, 16, 1, T().panelBorder);
    const int inX = X + 19, inW = W - 38;

    TTF_Font* fTitle = uiFont(20, true);
    TTF_Font* fSub   = uiFont(13);
    TTF_Font* fLabel = uiFont(14);
    TTF_Font* fValue = uiFont(14, true);
    TTF_Font* fRel   = uiFont(12);
    TTF_Font* fTag   = uiFont(11, true);

    // The action button along the bottom, and its note.
    auto button = [&](const std::string& label, const std::string& note) {
        const int by = Y + H - 95, bh = 52;
        fillRounded(inX, by, inW, bh, 12, T().accent);
        TTF_Font* f = uiFont(16, true);
        const int lw = textWidth(label, f);
        const int gx = inX + (inW - (24 + 12 + lw)) / 2;
        fillDisc(gx + 12, by + bh / 2, 12, T().keyCapText);
        drawTextCentered("A", gx + 12, by + bh / 2, T().accent, uiFont(13, true));
        drawText(label, gx + 36, by + bh / 2 - TTF_FontHeight(f) / 2, T().keyCapText, f);
        if (!note.empty()) {
            const auto lines = wrapText(note, fRel, inW);
            for (size_t i = 0; i < lines.size(); i++)
                drawTextCentered(lines[i], X + W / 2, by + bh + 16 + static_cast<int>(i) * 15,
                                 T().textMuted, fRel);
        }
    };

    // Header: icon, then the name (up to two lines) and a line under it, the
    // whole text block centred on the icon.
    auto header = [&](const std::function<void(int, int)>& icon, const std::string& title,
                      const std::string& sub) {
        constexpr int ICON = 72, LINE = 24, SUB_GAP = 4;
        const int iconY = Y + 19;
        icon(inX, iconY);
        const auto lines = wrapText(title, fTitle, W - (inX + ICON + 14 - X) - 19);
        const int subH = sub.empty() ? 0 : SUB_GAP + TTF_FontHeight(fSub);
        const int blockH = static_cast<int>(lines.size()) * LINE
                         - (LINE - TTF_FontHeight(fTitle)) + subH;
        int ty = iconY + (ICON - blockH) / 2;
        for (const auto& l : lines) { drawText(l, inX + ICON + 14, ty, T().text, fTitle); ty += LINE; }
        if (!sub.empty())
            drawText(fitText(sub, fSub, W - (inX + ICON + 14 - X) - 19), inX + ICON + 14,
                     ty - (LINE - TTF_FontHeight(fTitle)) + SUB_GAP, T().textDim, fSub);
    };

    if (gameSelOnGts_ || gameSelOnAllBanks_) {
        const bool gts = gameSelOnGts_;
        const SDL_Color teal = T().accentBank;
        // The icon on the same ground as on its tile.
        const SDL_Color iconBg = gts ? T().accent : SDL_Color{teal.r, teal.g, teal.b, 50};
        header([&](int x, int y) {
                   fillRounded(x, y, 72, 72, 16, iconBg);
                   SDL_Texture* ic = gts ? iconGlobe_ : iconBank_;
                   if (!ic) return;
                   const SDL_Color c = gts ? T().keyCapText : teal;
                   SDL_SetTextureColorMod(ic, c.r, c.g, c.b);
                   SDL_Rect dst = {x + 16, y + 16, 40, 40};
                   SDL_RenderCopy(renderer_, ic, nullptr, &dst);
                   SDL_SetTextureColorMod(ic, 255, 255, 255);
               },
               i18n::get(gts ? StrKey::GtsTitle : StrKey::AllBanks),
               gts ? std::string()
                   : i18n::fmt(StrKey::AllBanksSub, std::to_string(allBanksTotal_),
                               std::to_string(allBanksFamilies_)));
        if (gts) {
            // The tile's motif, larger, with the catchphrase set over it.
            const SDL_Rect box = {inX, Y + 103, inW, 200};
            fillRounded(box.x, box.y, box.w, box.h, 12, T().bg);
            const SDL_Rect clip = {box.x + 12, box.y + 1, box.w - 24, box.h - 2};
            drawGtsStars({box.x + 16, box.y + 14, box.w - 32, box.h - 90}, 14, 0x60BEu);
            drawGtsGlobe(clip, box.x + box.w - 70, box.y + 86, 130, 38);
            SDL_RenderSetClipRect(renderer_, &clip);
            drawGtsLink(box.x + 40, box.y + 110, box.x + box.w - 90, box.y + 40, 46, 200, 4, 3);
            SDL_RenderSetClipRect(renderer_, nullptr);
            strokeRounded(box.x, box.y, box.w, box.h, 12, 1, T().panelBorder);

            TTF_Font* fCatch = uiFont(22, true);
            const auto lines = wrapText(i18n::get(StrKey::GtsSubtitle), fCatch, box.w - 40);
            int ty = box.y + box.h - 18 - static_cast<int>(lines.size()) * 27;
            for (const auto& l : lines) { drawText(l, box.x + 20, ty, T().text, fCatch); ty += 27; }

            // What the board offers, as the hub lists it.
            struct Item { const char* title; const char* hint; };
            const Item items[] = {
                {StrKey::GtsBrowseBtn,  StrKey::GtsBrowseHint},
                {StrKey::GtsSearchBtn,  StrKey::GtsSearchHint},
                {StrKey::GtsDepositBtn, StrKey::GtsDepositHint},
            };
            int iy = box.y + box.h + 16;
            for (const auto& it : items) {
                fillDisc(inX + 5, iy + 9, 3, T().accent);
                drawText(fitText(i18n::get(it.title), fValue, inW - 16), inX + 16, iy, T().text, fValue);
                drawText(fitText(i18n::get(it.hint), fRel, inW - 16), inX + 16, iy + 19, T().textDim, fRel);
                iy += 44;
            }
        } else {
            // One chip per family with its bank count.
            int cx = inX, cyy = Y + 130;
            TTF_Font* f = uiFont(14);
            for (const auto& [fam, n] : allBanksByFamily_) {
                const std::string chip = std::string(bankGroupNameOf(fam)) + " \xc2\xb7 " + std::to_string(n);
                const std::string c = fitText(chip, f, inW - 20);
                const int w = textWidth(c, f) + 20;
                if (cx + w > inX + inW) { cx = inX; cyy += 34; }
                if (cyy > Y + H - 140) break;
                fillRounded(cx, cyy, w, 28, 7, T().buttonBg);
                drawText(c, cx + 10, cyy + 14 - TTF_FontHeight(f) / 2, T().text, f);
                cx += w + 6;
            }
        }
        button(i18n::get(StrKey::GtsOpen), std::string());
        return;
    }

    if (gameSelList_.empty()) return;
    const GameType g = availableGames_[gameSelList_[gameSelCursor_]];
    const GameSelInfo& info = gameSelInfo_[g];
    const bool withSave = selectedProfile_ >= 0 && !appletMode_;

    header([&](int x, int y) {
               drawGameIcon(g, x, y, 72, 16, T().panelBg);
           },
           gameDisplayNameOf(g), bankGroupNameOf(g));

    // Facts: save file, last backup, backups on SD, box format.
    int fy = Y + 103;
    const int rows = withSave ? 4 : 2;
    const int boxH = 16 + rows * 26 + (withSave && info.lastBackup ? 16 : 0) + 8;
    fillRounded(inX, fy, inW, boxH, 10, T().bg);
    const int lx = inX + 14, vx = inX + 134;
    int ry = fy + 12;
    auto row = [&](const std::string& label, const std::string& value, SDL_Color vc) {
        drawText(fitText(label, fLabel, vx - lx - 8), lx, ry, T().textDim, fLabel);
        drawText(fitText(value, fValue, inX + inW - 12 - vx), vx, ry, vc, fValue);
        ry += 26;
    };
    if (withSave) {
        const std::string who = selectedProfile_ < account_.profileCount()
            ? account_.profiles()[selectedProfile_].nickname : std::string();
        row(i18n::get(StrKey::DsSaveFile), i18n::fmt(StrKey::DsFound, who), T().statusOk);
        if (info.lastBackup) {
            row(i18n::get(StrKey::DsLastBackup), dateOf(info.lastBackup), T().text);
            drawText(relativeDay(info.lastBackup), vx, ry - 7, T().textDim, fRel);
            ry += 16;
        } else {
            row(i18n::get(StrKey::DsLastBackup), i18n::get(StrKey::NoBackupYet), T().textMuted);
        }
        row(i18n::get(StrKey::DsBackupsSd), std::to_string(info.backups), T().text);
    } else {
        row(i18n::get(StrKey::DsSaveFile), i18n::get(StrKey::DsBankOnly), T().textMuted);
    }
    const GameInfo& gi = gameInfo(g);
    row(i18n::get(StrKey::DsBoxFormat),
        std::to_string(gi.boxCount) + " \xc3\x97 " + std::to_string(gi.slotsPerBox), T().text);

    // Banks of the family, as chips.
    int by = fy + boxH + 20;
    {
        const std::string tag = i18n::fmt(StrKey::DsBanks, std::to_string(info.banks.size()));
        const std::string upper = toUpperUtf8(tag);
        drawTextTracked(upper, inX, by, T().accentBank, fTag, 2);
        const GameType pair = pairedGame(g);
        std::string note = pair == g ? i18n::get(StrKey::DsSingleFamily)
                                     : i18n::fmt(StrKey::DsSharedWith, gameDisplayNameOf(pair));
        note = fitText(note, fRel, inW - measureTextTracked(upper, fTag, 2) - 12);
        drawText(note, inX + inW - textWidth(note, fRel), by - 1, T().textDim, fRel);
    }
    by += 22;
    TTF_Font* fChip = uiFont(14);
    if (info.banks.empty()) {
        drawText(i18n::get(StrKey::DsNoBanks), inX, by + 4, T().textMuted, fChip);
    } else {
        const int limit = Y + H - 95 - 12;   // above the button
        int cx = inX;
        for (size_t i = 0; i < info.banks.size(); i++) {
            const std::string c = fitText(info.banks[i], fChip, inW - 20);
            const int w = textWidth(c, fChip) + 20;
            if (cx + w > inX + inW) { cx = inX; by += 34; }
            // Out of room: say how many are left instead.
            if (by + 28 > limit || (by + 62 > limit && cx + w + 70 > inX + inW && i + 1 < info.banks.size())) {
                const std::string more = i18n::fmt(StrKey::DsMore, std::to_string(info.banks.size() - i));
                drawText(more, cx, by + 14 - TTF_FontHeight(fChip) / 2, T().textDim, fChip);
                break;
            }
            fillRounded(cx, by, w, 28, 7, T().buttonBg);
            drawText(c, cx + 10, by + 14 - TTF_FontHeight(fChip) / 2, T().text, fChip);
            cx += w + 6;
        }
    }

    if (withSave)
        button(i18n::get(StrKey::DsBtnBackup), i18n::get(StrKey::DsBackupNote));
    else
        button(i18n::get(StrKey::DsBtnBank), i18n::get(StrKey::DualBankHint));
}

// --- Frame ---------------------------------------------------------------------------

void UI::drawGameSelectorFrame() {
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);

    drawGameSelTopBar();
    drawGameSelTiles();
    drawGameSelFilters();

    // Grid, clipped so a row can show partly at the bottom.
    const bool onTiles = gameSelOnGts_ || gameSelOnAllBanks_;
    const int count = static_cast<int>(gameSelList_.size());
    const int totalRows = (count + GS_COLS - 1) / GS_COLS;
    {
        const SDL_Rect clip = {GS_LEFT - 8, GS_GRID_Y - 8, GS_GRID_R - GS_LEFT + 16,
                               GS_GRID_BOTTOM - GS_GRID_Y + 8};
        SDL_RenderSetClipRect(renderer_, &clip);
        for (int i = gameSelScroll_ * GS_COLS; i < count; i++) {
            const int row = i / GS_COLS - gameSelScroll_;
            const int y = GS_GRID_Y + row * GS_ROW_H;
            if (y >= GS_GRID_BOTTOM) break;
            const int col = i % GS_COLS;
            const int x = GS_LEFT + col * (GS_GRID_R - GS_LEFT - GS_CARD_W) / (GS_COLS - 1);
            drawGameCard(i, {x, y, GS_CARD_W, GS_CARD_H}, !onTiles && i == gameSelCursor_);
        }
        SDL_RenderSetClipRect(renderer_, nullptr);
    }

    // Scrollbar when there are more rows than fit.
    const int visibleRows = (GS_GRID_BOTTOM - GS_GRID_Y + GS_ROW_H - 1) / GS_ROW_H;
    if (totalRows > GS_FULL_ROWS) {
        const int trackY = GS_GRID_Y, trackH = GS_GRID_BOTTOM - 6 - GS_GRID_Y;
        const int thumbH = std::max(30, trackH * std::min(visibleRows, totalRows) / totalRows);
        const int maxScroll = std::max(1, totalRows - GS_FULL_ROWS);
        const int thumbY = trackY + (trackH - thumbH) * std::min(gameSelScroll_, maxScroll) / maxScroll;
        fillRounded(GS_GRID_R + 18, trackY, 5, trackH, 2, T().buttonBg);
        fillRounded(GS_GRID_R + 18, thumbY, 5, thumbH, 2, T().textMuted);
    }

    drawGameDetailPanel();

    // Same keys as before; L/R now step through the filters instead of pages.
    ButtonHint hints[6];
    int n = 0;
    hints[n++] = {"A", StrKey::HintSelect2};
    if (visibleGameFilters().size() > 1)
        hints[n++] = {"L R", StrKey::HintFilter};
    hints[n++] = {"B", selectedProfile_ >= 0 ? StrKey::HintBack : StrKey::HintQuit};
    hints[n++] = {"-", StrKey::HintAbout};
    if (selectedProfile_ >= 0)
        hints[n++] = {"+", StrKey::HintQuit};
    drawFooterBar(hints, n);
}

// --- Backup screen (UI 2.0) ----------------------------------------------------------
//
// external/UI_2.0 "1b · Backing up": the game list stays behind, dimmed, and
// the detail panel follows the backup step by step. Drawn and presented from
// inside selectGame, which is blocking: nothing can be pressed meanwhile.

void UI::drawBackupProgress(GameType game, const std::string& dir, int step, float fraction) {
    if (!renderer_) return;

    drawGameSelectorFrame();

    // Top bar over again, now on the backup step.
    drawRect(0, 0, SCREEN_W, 76, T().bg);
    drawFlowTopBar(i18n::get(StrKey::BkTitle),
                   {i18n::get(StrKey::StepGame), i18n::get(StrKey::StepBackup),
                    i18n::get(StrKey::StepBank)}, 1);
    drawGameSelTopRight();

    // Everything else waits.
    drawRect(0, 76, GS_PANEL_X - 12, FOOTER_Y - 76, SDL_Color{T().bg.r, T().bg.g, T().bg.b, 160});

    const int X = GS_PANEL_X, Y = GS_PANEL_Y, W = GS_PANEL_W, H = GS_PANEL_H;
    fillRounded(X, Y, W, H, 16, T().panelBg);
    strokeRounded(X, Y, W, H, 16, 1, T().panelBorder);
    const int inX = X + 19, inW = W - 38;

    // Which game
    {
        constexpr int ICON = 56;
        drawGameIcon(game, inX, Y + 19, ICON, 12, T().panelBg);
        TTF_Font* fName = uiFont(20, true);
        TTF_Font* fSub  = uiFont(13);
        const int tx = inX + ICON + 14, tw = inW - ICON - 14;
        drawText(fitText(gameDisplayNameOf(game), fName, tw), tx, Y + 24, T().text, fName);
        drawText(fitText(bankGroupNameOf(game), fSub, tw), tx, Y + 51, T().textDim, fSub);
    }

    // Step title and overall progress: the write is most of it.
    TTF_Font* fTag = uiFont(11, true);
    drawTextTracked(i18n::get(StrKey::BkStep), inX, Y + 92, T().accent, fTag, 2);
    drawText(i18n::get(StrKey::BkHeading), inX, Y + 110, T().text, uiFont(22, true));
    {
        static constexpr float BASE[4] = {0.03f, 0.08f, 0.12f, 0.97f};
        float overall = BASE[std::clamp(step, 0, 3)];
        if (step == 2) overall = 0.12f + 0.85f * std::clamp(fraction, 0.0f, 1.0f);
        const int pct = static_cast<int>(overall * 100.0f + 0.5f);
        TTF_Font* f = uiFont(14, true);
        const std::string p = std::to_string(pct) + "%";
        const int barW = inW - 56;
        fillRounded(inX, Y + 157, barW, 10, 5, T().bg);
        fillRounded(inX, Y + 157, std::max(10, static_cast<int>(barW * overall)), 10, 5, T().accent);
        drawText(p, inX + inW - textWidth(p, f), Y + 162 - TTF_FontHeight(f) / 2, T().text, f);
    }

    // The steps themselves.
    {
        const char* labels[4] = {StrKey::BkOpened, StrKey::BkSpace, StrKey::BkWriting, StrKey::BkLoading};
        const int boxY = Y + 183, rowH = 30;
        fillRounded(inX, boxY, inW, 4 * rowH + 14, 10, T().bg);
        TTF_Font* f = uiFont(14, true);
        for (int i = 0; i < 4; i++) {
            const int cy = boxY + 7 + i * rowH + rowH / 2;
            const int cx = inX + 24;
            const SDL_Color ok = T().statusOk;
            if (i < step) {
                drawCheckDisc(cx, cy, 10, SDL_Color{ok.r, ok.g, ok.b, 50}, ok);
            } else if (i == step) {
                fillDisc(cx, cy, 10, T().accent);
                fillDisc(cx, cy, 3, T().keyCapText);
            } else {
                strokeRounded(cx - 10, cy - 10, 20, 20, 10, 2, T().buttonBorder);
            }
            drawText(fitText(i18n::get(labels[i]), f, inW - 60), cx + 20,
                     cy - TTF_FontHeight(f) / 2, i <= step ? T().text : T().textMuted, f);
        }
    }

    // Where it goes.
    {
        const int dy = Y + 330;
        drawTextTracked(i18n::get(StrKey::BkDestination), inX, dy, T().textDim, fTag, 2);
        TTF_Font* f = uiFont(14, true);
        const auto lines = wrapChars(dir, f, inW, 3);
        for (size_t i = 0; i < lines.size(); i++)
            drawText(lines[i], inX, dy + 20 + static_cast<int>(i) * 19, T().text, f);
    }

    // The LED note, bottom of the panel.
    {
        TTF_Font* f = uiFont(13, true);
        const auto lines = wrapText(i18n::get(StrKey::BkLed), f, inW - 44, 2);
        const int h = 18 + static_cast<int>(lines.size()) * 18;
        const int ny = Y + H - 19 - h;
        const SDL_Color w = T().statusWarn;
        fillRounded(inX, ny, inW, h, 10, SDL_Color{w.r, w.g, w.b, 40});
        fillDisc(inX + 16, ny + h / 2, 4, T().accent);
        for (size_t i = 0; i < lines.size(); i++)
            drawText(lines[i], inX + 30, ny + 9 + static_cast<int>(i) * 18, T().accent, f);
    }

    // Footer: no keys, only what is happening.
    drawFooterBar(nullptr, 0, std::string());
    {
        TTF_Font* f = uiFont(15);
        const int cy = FOOTER_Y + FOOTER_H / 2;
        fillDisc(38, cy, 5, T().accent);
        drawText(i18n::get(StrKey::BkFooter), 52, cy - TTF_FontHeight(f) / 2, T().text, f);
    }

    SDL_RenderPresent(renderer_);
    markDirty();
}

void UI::handleGameSelectorInput(bool& running) {
    const int count = static_cast<int>(gameSelList_.size());

    // The two tiles sit above the grid: columns 0-1 lead to the GTS, 2-3 to
    // All banks, and back down into the grid the same way.
    auto moveGrid = [&](int dx, int dy) {
        if (gameSelOnGts_ || gameSelOnAllBanks_) {
            if (dx > 0 && gameSelOnGts_)      { gameSelOnGts_ = false; gameSelOnAllBanks_ = true; }
            else if (dx < 0 && gameSelOnAllBanks_) { gameSelOnAllBanks_ = false; gameSelOnGts_ = true; }
            else if (dy > 0 && count > 0) {
                const int col = gameSelOnGts_ ? 0 : 2;
                gameSelOnGts_ = gameSelOnAllBanks_ = false;
                gameSelScroll_ = 0;
                gameSelCursor_ = std::min(col, count - 1);
            }
            return;
        }
        if (count == 0) {
            if (dy < 0) gameSelOnGts_ = true;
            return;
        }

        int col = gameSelCursor_ % GS_COLS;
        int row = gameSelCursor_ / GS_COLS;
        const int totalRows = (count + GS_COLS - 1) / GS_COLS;
        if (dx != 0) {
            // Wrap within the row.
            const int rowItems = std::min(GS_COLS, count - row * GS_COLS);
            col = (col + dx + rowItems) % rowItems;
        }
        if (dy < 0) {
            if (row == 0) {
                if (col < 2) gameSelOnGts_ = true; else gameSelOnAllBanks_ = true;
                return;
            }
            row--;
        }
        if (dy > 0) {
            if (row + 1 >= totalRows) return;
            row++;
        }
        gameSelCursor_ = std::min(row * GS_COLS + col, count - 1);
        const int r = gameSelCursor_ / GS_COLS;
        if (r < gameSelScroll_) gameSelScroll_ = r;
        if (r >= gameSelScroll_ + GS_FULL_ROWS) gameSelScroll_ = r - GS_FULL_ROWS + 1;
    };

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
            return;
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN)
            markDirty();

        if (event.type == SDL_CONTROLLERAXISMOTION) {
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
                int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
                updateStick(lx, ly);
            }
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  moveGrid(-1, 0); break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: moveGrid(1, 0);  break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:    moveGrid(0, -1); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  moveGrid(0, 1);  break;
                case SDL_CONTROLLER_BUTTON_B: // Switch A = select
                    if (gameSelOnGts_)
                        enterGts();
                    else if (gameSelOnAllBanks_)
                        enterAllBanksMode();
                    else if (count > 0)
                        selectGame(availableGames_[gameSelList_[gameSelCursor_]]);
                    break;
                case SDL_CONTROLLER_BUTTON_A: // Switch B = back
                    if (selectedProfile_ >= 0) {
                        freeGameIcons();
                        account_.unmountSave();
                        screen_ = AppScreen::ProfileSelector;
                    } else {
                        running = false;
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  stepGameFilter(-1); break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: stepGameFilter(+1); break;
                case SDL_CONTROLLER_BUTTON_BACK: // - = about
                    showAbout_ = true;
                    break;
                case SDL_CONTROLLER_BUTTON_START:
                    running = false;
                    break;
            }
        }
    }

    // Joystick repeat navigation
    if (stickDirX_ != 0 || stickDirY_ != 0) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay) {
            if (stickDirX_ != 0) moveGrid(stickDirX_, 0);
            if (stickDirY_ != 0) moveGrid(0, stickDirY_);
            stickMoveTime_ = now;
            stickMoved_ = true;
            markDirty();
        }
    }
}

void UI::enterAllBanksMode() {
    allBanksMode_ = true;
    bankManager_.initAll(basePath_, bankListProgress());

    if (bankManager_.list().empty()) {
        allBanksMode_ = false;
        showMessageAndWait(i18n::get(StrKey::NoBanksTitle), i18n::get(StrKey::NoBanksAnyGame));
        return;
    }

    bankSelCursor_ = 0;
    bankSelScroll_ = 0;
    bankSelTarget_ = Panel::Bank;
    leftBankName_.clear();
    leftBankPath_.clear();
    activeBankName_.clear();
    activeBankPath_.clear();

    screen_ = AppScreen::BankSelector;
}
