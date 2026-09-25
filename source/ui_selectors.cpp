#include "ui.h"
#include "i18n.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include <switch.h>

void UI::refreshBankCounts() {
    gameBankCounts_.clear();
    for (GameType g : availableGames_) {
        GameType p = pairedGame(g);
        auto it = gameBankCounts_.find(p);
        if (it != gameBankCounts_.end())
            gameBankCounts_[g] = it->second;
        else
            gameBankCounts_[g] = BankManager::countBanks(basePath_, g);
    }
}

// --- Profile selector (UI 2.0) -----------------------------------------------------
//
// Laid out after external/UI_2.0 "Start · Select profile": a card per Switch
// profile with its icon, name and how many games it has saves for.

void UI::loadProfileSaveCounts() {
    const int count = account_.profileCount();
    profileSaveCounts_.assign(count, 0);
    for (int i = 0; i < count; i++)
        for (GameType g : ALL_GAMES)
            if (account_.hasSaveData(i, g)) profileSaveCounts_[i]++;
}

time_t UI::lastBackupTime(int profile, GameType game) const {
    if (profile < 0 || profile >= account_.profileCount()) return 0;

    // backups/<profile>/<game>/<profile>_YYYY-MM-DD_HH-MM-SS/ (see
    // buildBackupDir). The time is read from the folder name, which is what
    // the backup was named after, rather than from a file date.
    const std::string dir = basePath_ + "backups/" + account_.profiles()[profile].pathSafeName
                          + "/" + gamePathNameOf(game) + "/";
    DIR* runs = opendir(dir.c_str());
    if (!runs) return 0;
    time_t newest = 0;
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
        if (when > newest) newest = when;
    }
    closedir(runs);
    return newest;
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
                    std::string initial(1, nick[0]);
                    if (initial[0] >= 'a' && initial[0] <= 'z') initial[0] -= 'a' - 'A';
                    // Keep a multi-byte first letter whole.
                    const unsigned char lead = static_cast<unsigned char>(nick[0]);
                    const size_t len = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2
                                     : (lead & 0xF0) == 0xE0 ? 3 : 4;
                    if (len > 1) initial = nick.substr(0, len);
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
    gameSelPage_ = 0;
    gameSelOnAllBanks_ = false;
    gameSelOnChevron_ = 0;
    showWorking(i18n::get(StrKey::LoadingGameIcons));
    loadGameIcons();
    screen_ = AppScreen::GameSelector;
}

// --- Game Icons ---

void UI::loadGameIcons() {
    freeGameIcons();
    std::string cacheDir = basePath_ + "cache/";
    mkdir(cacheDir.c_str(), 0755);

    bool needSystem = false;
    for (GameType game : availableGames_) {
        // Try loading from cache first
        char hexId[32];
        std::snprintf(hexId, sizeof(hexId), "%016lX", titleIdOf(game));
        std::string cachePath = cacheDir + hexId + ".jpg";

        SDL_Surface* surf = IMG_Load(cachePath.c_str());
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
            SDL_FreeSurface(surf);
            if (tex)
                gameIconCache_[game] = tex;
            continue;
        }
        needSystem = true;
    }

    // Fetch uncached icons from system
    if (needSystem) {
        nsInitialize();
        for (GameType game : availableGames_) {
            if (gameIconCache_.count(game))
                continue; // already loaded from cache

            NsApplicationControlData ctrlData;
            std::memset(&ctrlData, 0, sizeof(ctrlData));
            uint64_t controlSize = 0;
            Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage,
                            titleIdOf(game), &ctrlData, sizeof(ctrlData), &controlSize);
            if (R_FAILED(rc) || controlSize <= sizeof(NacpStruct))
                continue;

            size_t iconSize = controlSize - sizeof(NacpStruct);

            // Save JPEG to cache
            char hexId[32];
            std::snprintf(hexId, sizeof(hexId), "%016lX", titleIdOf(game));
            std::string cachePath = cacheDir + hexId + ".jpg";
            FILE* f = std::fopen(cachePath.c_str(), "wb");
            if (f) {
                std::fwrite(ctrlData.icon, 1, iconSize, f);
                std::fclose(f);
            }

            // Decode and create texture
            SDL_RWops* rw = SDL_RWFromMem(ctrlData.icon, iconSize);
            if (!rw)
                continue;
            SDL_Surface* surf = IMG_Load_RW(rw, 1);
            if (!surf)
                continue;
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
            SDL_FreeSurface(surf);
            if (tex)
                gameIconCache_[game] = tex;
        }
        nsExit();
    }
}

void UI::freeGameIcons() {
    for (auto& [game, tex] : gameIconCache_) {
        if (tex)
            SDL_DestroyTexture(tex);
    }
    gameIconCache_.clear();
}

// --- Game Selector ---

void UI::drawGameSelectorFrame() {
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);

    if (appletMode_) {
        drawTextCentered(i18n::get(StrKey::SelectGameDual), SCREEN_W / 2, 30, T().text, font_);
        drawTextCentered(i18n::get(StrKey::DualBankHint),
                         SCREEN_W / 2, 55, T().textDim, fontSmall_);
    } else {
        drawTextCentered(i18n::get(StrKey::SelectGame), SCREEN_W / 2, 40, T().text, font_);
    }

    // The board, above the icons.
    drawGtsRow(82, 48);

    int numGames = (int)availableGames_.size();
    constexpr int COLS = 6;
    constexpr int ROWS_PER_PAGE = 2;
    constexpr int GAMES_PER_PAGE = COLS * ROWS_PER_PAGE;
    constexpr int CARD_W = 160;
    constexpr int CARD_H = 200;
    constexpr int CARD_GAP = 20;
    constexpr int ICON_SIZE = 128;

    int totalPages = (numGames + GAMES_PER_PAGE - 1) / GAMES_PER_PAGE;
    int pageStart = gameSelPage_ * GAMES_PER_PAGE;
    int pageEnd = std::min(pageStart + GAMES_PER_PAGE, numGames);
    int pageCount = pageEnd - pageStart;

    int rows = (pageCount + COLS - 1) / COLS;
    int totalH = rows * CARD_H + (rows - 1) * CARD_GAP;
    int gridStartY = (SCREEN_H - totalH) / 2;

    for (int i = pageStart; i < pageEnd; i++) {
        int idx = i - pageStart;
        int r = idx / COLS;
        int c = idx % COLS;

        // Center each row: count items in this row
        int rowItems = std::min(COLS, pageCount - r * COLS);
        int rowW = rowItems * CARD_W + (rowItems - 1) * CARD_GAP;
        int rowStartX = (SCREEN_W - rowW) / 2;

        int cardX = rowStartX + c * (CARD_W + CARD_GAP);
        int cardY = gridStartY + r * (CARD_H + CARD_GAP);

        // Card background
        if (i == gameSelCursor_ && !gameSelOnAllBanks_ && !gameSelOnGts_
            && gameSelOnChevron_ == 0) {
            drawRect(cardX, cardY, CARD_W, CARD_H, T().menuHighlight);
            drawRectOutline(cardX, cardY, CARD_W, CARD_H, T().cursor, 3);
        } else {
            drawRect(cardX, cardY, CARD_W, CARD_H, T().panelBg);
        }

        // Icon
        int iconX = cardX + (CARD_W - ICON_SIZE) / 2;
        int iconY = cardY + 10;

        auto it = gameIconCache_.find(availableGames_[i]);
        if (it != gameIconCache_.end() && it->second) {
            SDL_Rect dst = {iconX, iconY, ICON_SIZE, ICON_SIZE};
            SDL_RenderCopy(renderer_, it->second, nullptr, &dst);
        } else {
            // Colored placeholder with game abbreviation
            drawRect(iconX, iconY, ICON_SIZE, ICON_SIZE, T().iconPlaceholder);
            const char* abbr = "";
            switch (availableGames_[i]) {
                case GameType::Sw: abbr = "Sw"; break;
                case GameType::Sh: abbr = "Sh"; break;
                case GameType::BD: abbr = "BD"; break;
                case GameType::SP: abbr = "SP"; break;
                case GameType::LA: abbr = "LA"; break;
                case GameType::S:  abbr = "S";  break;
                case GameType::V:  abbr = "V";  break;
                case GameType::ZA: abbr = "ZA"; break;
                case GameType::GP: abbr = "GP"; break;
                case GameType::GE: abbr = "GE"; break;
                case GameType::FR: case GameType::FR_ES: case GameType::FR_DE: case GameType::FR_IT: case GameType::FR_FR: case GameType::FR_JA: abbr = "FR"; break;
                case GameType::LG: case GameType::LG_ES: case GameType::LG_DE: case GameType::LG_IT: case GameType::LG_FR: case GameType::LG_JA: abbr = "LG"; break;
            }
            drawTextCentered(abbr, iconX + ICON_SIZE / 2, iconY + ICON_SIZE / 2,
                             T().text, font_);
        }

        // Game name below icon
        std::string name = gameDisplayNameOf(availableGames_[i]);
        // Strip "Pokemon " prefix for brevity
        if (name.substr(0, 8) == "Pokemon ")
            name = name.substr(8);
        if (name.length() > 20) name = name.substr(0, 19) + ".";
        drawTextCentered(name, cardX + CARD_W / 2, cardY + ICON_SIZE + 30,
                         T().text, fontSmall_);

        // Bank count under game name
        auto bc = gameBankCounts_.find(availableGames_[i]);
        int bankCount = (bc != gameBankCounts_.end()) ? bc->second : 0;
        std::string bankStr = "(" + std::to_string(bankCount) + ")";
        drawTextCentered(bankStr, cardX + CARD_W / 2, cardY + ICON_SIZE + 50,
                         T().textDim, fontSmall_);
    }

    // "View All Banks" option below the grid
    {
        int lastRow = (pageCount - 1) / COLS;
        int gridBottomY = gridStartY + (lastRow + 1) * (CARD_H + CARD_GAP);
        int allBanksY = gridBottomY + 10;

        std::string label = i18n::get(StrKey::ViewAllBanks);
        const auto& te = getTextEntry(label, font_, T().text);
        int labelW = te.w + 40;  // padding
        int labelH = 36;
        int labelX = (SCREEN_W - labelW) / 2;

        if (gameSelOnAllBanks_) {
            drawRect(labelX, allBanksY, labelW, labelH, T().menuHighlight);
            drawRectOutline(labelX, allBanksY, labelW, labelH, T().cursor, 2);
        } else {
            drawRect(labelX, allBanksY, labelW, labelH, T().panelBg);
        }
        drawTextCentered(label, SCREEN_W / 2, allBanksY + labelH / 2,
                         T().text, font_);
    }

    // Chevron buttons for page navigation
    if (totalPages > 1) {
        constexpr int BTN_W = 40;
        constexpr int BTN_H = 60;
        int btnY = SCREEN_H / 2 - BTN_H / 2;

        // Left button
        int leftX = 10;
        bool canLeft = gameSelPage_ > 0;
        bool leftFocused = gameSelOnChevron_ == -1;
        drawRect(leftX, btnY, BTN_W, BTN_H, leftFocused ? T().menuHighlight : T().panelBg);
        if (leftFocused)
            drawRectOutline(leftX, btnY, BTN_W, BTN_H, T().cursor, 3);
        drawTextCentered("<", leftX + BTN_W / 2, btnY + BTN_H / 2,
                         canLeft ? T().text : T().textDim, font_);

        // Right button
        int rightX = SCREEN_W - BTN_W - 10;
        bool canRight = gameSelPage_ < totalPages - 1;
        bool rightFocused = gameSelOnChevron_ == 1;
        drawRect(rightX, btnY, BTN_W, BTN_H, rightFocused ? T().menuHighlight : T().panelBg);
        if (rightFocused)
            drawRectOutline(rightX, btnY, BTN_W, BTN_H, T().cursor, 3);
        drawTextCentered(">", rightX + BTN_W / 2, btnY + BTN_H / 2,
                         canRight ? T().text : T().textDim, font_);
    }

    // The hint bar, built from parts rather than from one pre-formatted string:
    // the button names are hardware and never translated, so only the verbs go
    // through i18n, and L/R gets a key wide enough to hold it.
    {
        ButtonHint hints[6];
        int n = 0;
        hints[n++] = {"A", StrKey::HintSelect2};
        hints[n++] = {"B", selectedProfile_ >= 0 ? StrKey::HintBack : StrKey::HintQuit};
        if (totalPages > 1)
            hints[n++] = {"L/R", StrKey::HintPage};
        hints[n++] = {"Y", StrKey::HintTheme};
        hints[n++] = {"-", StrKey::HintAbout};
        if (selectedProfile_ >= 0)
            hints[n++] = {"+", StrKey::HintQuit};

        // Whatever goes bottom-right on this screen, so the keys stop short of it.
        std::string right;
        if (selectedProfile_ >= 0) right = account_.profiles()[selectedProfile_].nickname;
        else if (appletMode_)      right = i18n::get(StrKey::DualBankMode);
        const int reserve = right.empty() ? 0
                          : getTextEntry(right, fontSmall_, T().goldLabel).w;
        drawHintBar(hints, n, std::string(), reserve);
    }

    if (selectedProfile_ >= 0) {
        std::string profileLabel = account_.profiles()[selectedProfile_].nickname;
        const auto& e = getTextEntry(profileLabel, fontSmall_, T().goldLabel);
        if (e.tex) drawText(profileLabel, SCREEN_W - e.w - 15, SCREEN_H - 26, T().goldLabel, fontSmall_);
    }
    if (appletMode_) {
        std::string modeLabel = i18n::get(StrKey::DualBankMode);
        const auto& e = getTextEntry(modeLabel, fontSmall_, T().goldLabel);
        if (e.tex) drawText(modeLabel, SCREEN_W - e.w - 15, SCREEN_H - 26, T().goldLabel, fontSmall_);
    }
}

void UI::handleGameSelectorInput(bool& running) {
    int numGames = (int)availableGames_.size();
    if (numGames == 0) return;

    constexpr int COLS = 6;

    constexpr int GAMES_PER_PAGE = 12;

    int totalPages = (numGames + GAMES_PER_PAGE - 1) / GAMES_PER_PAGE;

    auto moveGrid = [&](int dx, int dy) {
        int pageStart = gameSelPage_ * GAMES_PER_PAGE;
        int pageEnd = std::min(pageStart + GAMES_PER_PAGE, numGames);
        int pageCount = pageEnd - pageStart;

        // On the GTS band
        if (gameSelOnGts_) {
            if (dy > 0) {
                // Into the grid, keeping the column the cursor came from.
                gameSelOnGts_ = false;
                int col = (gameSelCursor_ - pageStart) % COLS;
                if (col >= pageCount) col = pageCount - 1;
                gameSelCursor_ = pageStart + col;
            } else if (dy < 0) {
                gameSelOnGts_ = false;
                gameSelOnAllBanks_ = true;
            }
            return;
        }

        // On a chevron button
        if (gameSelOnChevron_ != 0) {
            if (dx != 0) {
                if (gameSelOnChevron_ == -1 && dx > 0) {
                    // Right from left chevron → back to grid col 0
                    gameSelOnChevron_ = 0;
                } else if (gameSelOnChevron_ == 1 && dx < 0) {
                    // Left from right chevron → back to grid last col
                    gameSelOnChevron_ = 0;
                    int localIdx = gameSelCursor_ - pageStart;
                    int row = localIdx / COLS;
                    int rowItems = std::min(COLS, pageCount - row * COLS);
                    gameSelCursor_ = pageStart + row * COLS + rowItems - 1;
                }
            }
            if (dy > 0) {
                gameSelOnChevron_ = 0;
                gameSelOnAllBanks_ = true;
            }
            if (dy < 0) {
                gameSelOnChevron_ = 0;
                gameSelOnGts_ = true;
            }
            return;
        }

        if (gameSelOnAllBanks_) {
            // On "All Banks" row: up goes back to the grid, down wraps round to
            // the band at the top, left/right ignored.
            if (dy > 0) {
                gameSelOnAllBanks_ = false;
                gameSelOnGts_ = true;
                return;
            }
            if (dy < 0) {
                gameSelOnAllBanks_ = false;
                // Place cursor on bottom row of current page
                int totalRows = (pageCount + COLS - 1) / COLS;
                int lastRowStart = (totalRows - 1) * COLS;
                int lastRowItems = pageCount - lastRowStart;
                int col = (gameSelCursor_ - pageStart) % COLS;
                if (col >= lastRowItems) col = lastRowItems - 1;
                gameSelCursor_ = pageStart + lastRowStart + col;
            }
            return;
        }

        int localIdx = gameSelCursor_ - pageStart;
        int col = localIdx % COLS;
        int row = localIdx / COLS;
        int totalRows = (pageCount + COLS - 1) / COLS;

        col += dx;
        row += dy;

        // Moving down past the last row goes to "All Banks"
        if (row >= totalRows) {
            gameSelOnAllBanks_ = true;
            return;
        }

        // Navigate to chevrons when going past grid edges (only if page exists)
        if (totalPages > 1) {
            if (col < 0 && gameSelPage_ > 0) {
                gameSelOnChevron_ = -1;
                return;
            }
            int rowItems = std::min(COLS, pageCount - row * COLS);
            if (col >= rowItems && gameSelPage_ < totalPages - 1) {
                gameSelOnChevron_ = 1;
                return;
            }
        }

        // Wrap columns within the row (single-page fallback)
        int rowItems = std::min(COLS, pageCount - row * COLS);
        if (rowItems <= 0) rowItems = COLS;
        if (col < 0) col = rowItems - 1;
        if (col >= rowItems) col = 0;

        // Up from the top row reaches the band rather than wrapping all the
        // way round to "All Banks".
        if (row < 0) {
            gameSelOnGts_ = true;
            return;
        }

        int newLocal = row * COLS + col;
        if (newLocal >= pageCount)
            newLocal = pageCount - 1;
        gameSelCursor_ = pageStart + newLocal;
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
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    moveGrid(-1, 0);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    moveGrid(1, 0);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    moveGrid(0, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    moveGrid(0, 1);
                    break;
                case SDL_CONTROLLER_BUTTON_B: // Switch A = select
                    if (gameSelOnGts_) {
                        enterGts();
                    } else if (gameSelOnChevron_ == -1 && gameSelPage_ > 0) {
                        gameSelPage_--;
                        gameSelCursor_ = gameSelPage_ * GAMES_PER_PAGE;
                        gameSelOnChevron_ = 0;
                    } else if (gameSelOnChevron_ == 1 && gameSelPage_ < totalPages - 1) {
                        gameSelPage_++;
                        gameSelCursor_ = gameSelPage_ * GAMES_PER_PAGE;
                        gameSelOnChevron_ = 0;
                    } else if (gameSelOnAllBanks_)
                        enterAllBanksMode();
                    else
                        selectGame(availableGames_[gameSelCursor_]);
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
                case SDL_CONTROLLER_BUTTON_X: // Switch Y = theme
                    showThemeSelector_ = true;
                    themeSelCursor_ = themeIndex_;
                    themeSelOriginal_ = themeIndex_;
                    break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: { // L = previous page
                    if (totalPages > 1 && gameSelPage_ > 0) {
                        gameSelPage_--;
                        gameSelCursor_ = gameSelPage_ * GAMES_PER_PAGE;
                        gameSelOnAllBanks_ = false;
                        gameSelOnChevron_ = 0;
                        gameSelOnGts_ = false;
                    }
                    break;
                }
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: { // R = next page
                    if (totalPages > 1 && gameSelPage_ < totalPages - 1) {
                        gameSelPage_++;
                        gameSelCursor_ = gameSelPage_ * GAMES_PER_PAGE;
                        gameSelOnAllBanks_ = false;
                        gameSelOnChevron_ = 0;
                        gameSelOnGts_ = false;
                    }
                    break;
                }
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
    bankManager_.initAll(basePath_);

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
