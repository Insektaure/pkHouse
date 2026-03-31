#include "ui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
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

// --- Profile Selector ---

void UI::drawProfileSelectorFrame() {
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);

    drawTextCentered("Select Profile", SCREEN_W / 2, 40, T().text, font_);

    const auto& profiles = account_.profiles();
    int count = (int)profiles.size();

    constexpr int CARD_W = 160;
    constexpr int CARD_H = 200;
    constexpr int CARD_GAP = 20;
    constexpr int ICON_SIZE = 128;

    int totalW = count * CARD_W + (count - 1) * CARD_GAP;
    int startX = (SCREEN_W - totalW) / 2;
    int startY = (SCREEN_H - CARD_H) / 2;

    for (int i = 0; i < count; i++) {
        int cardX = startX + i * (CARD_W + CARD_GAP);
        int cardY = startY;

        if (i == profileSelCursor_) {
            drawRect(cardX, cardY, CARD_W, CARD_H, T().menuHighlight);
            drawRectOutline(cardX, cardY, CARD_W, CARD_H, T().cursor, 3);
        } else {
            drawRect(cardX, cardY, CARD_W, CARD_H, T().panelBg);
        }

        int iconX = cardX + (CARD_W - ICON_SIZE) / 2;
        int iconY = cardY + 10;

        if (profiles[i].iconTexture) {
            SDL_Rect dst = {iconX, iconY, ICON_SIZE, ICON_SIZE};
            SDL_RenderCopy(renderer_, profiles[i].iconTexture, nullptr, &dst);
        } else {
            drawRect(iconX, iconY, ICON_SIZE, ICON_SIZE, T().iconPlaceholder);
            if (!profiles[i].nickname.empty()) {
                std::string initial(1, profiles[i].nickname[0]);
                drawTextCentered(initial, iconX + ICON_SIZE / 2, iconY + ICON_SIZE / 2,
                                 T().text, font_);
            }
        }

        std::string name = profiles[i].nickname;
        if (name.length() > 14) name = name.substr(0, 13) + ".";
        drawTextCentered(name, cardX + CARD_W / 2, cardY + ICON_SIZE + 24, T().text, fontSmall_);
    }

    drawStatusBar("A: Select  Y: Theme  -: About  +: Quit");
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
                case SDL_CONTROLLER_BUTTON_X: // Switch Y = theme
                    showThemeSelector_ = true;
                    themeSelCursor_ = themeIndex_;
                    themeSelOriginal_ = themeIndex_;
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
    constexpr GameType allGames[] = {
        GameType::GP, GameType::GE, GameType::Sw, GameType::Sh,
        GameType::BD, GameType::SP, GameType::LA, GameType::S,
        GameType::V, GameType::ZA, GameType::FR, GameType::LG,
        GameType::FR_ES, GameType::LG_ES
    };
    for (GameType g : allGames) {
        if (account_.hasSaveData(index, g))
            availableGames_.push_back(g);
    }

    if (availableGames_.empty()) {
        showMessageAndWait("No Save Data",
            "No supported Pokemon save data found for this profile.");
        return;
    }

    refreshBankCounts();

    gameSelCursor_ = 0;
    gameSelPage_ = 0;
    gameSelOnAllBanks_ = false;
    showWorking("Loading game icons...");
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
        drawTextCentered("Select Game (Dual Bank)", SCREEN_W / 2, 30, T().text, font_);
        drawTextCentered("Use title override mode to transfer between save and bank",
                         SCREEN_W / 2, 55, T().textDim, fontSmall_);
    } else {
        drawTextCentered("Select Game", SCREEN_W / 2, 40, T().text, font_);
    }

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
        if (i == gameSelCursor_ && !gameSelOnAllBanks_) {
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
                case GameType::FR: case GameType::FR_ES: abbr = "FR"; break;
                case GameType::LG: case GameType::LG_ES: abbr = "LG"; break;
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

        std::string label = "View All Banks";
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

    // Chevron arrows for page navigation
    if (totalPages > 1) {
        int chevronY = SCREEN_H / 2;
        // Left chevron (previous page)
        SDL_Color chevronColor = (gameSelPage_ > 0) ? T().text : T().textDim;
        drawTextCentered("<", 30, chevronY, chevronColor, font_);
        // Right chevron (next page)
        chevronColor = (gameSelPage_ < totalPages - 1) ? T().text : T().textDim;
        drawTextCentered(">", SCREEN_W - 30, chevronY, chevronColor, font_);
    }

    if (selectedProfile_ >= 0) {
        drawStatusBar(totalPages > 1 ? "A: Select  B: Back  L/R: Page  Y: Theme  -: About  +: Quit"
                                     : "A: Select  B: Back  Y: Theme  -: About  +: Quit");
        std::string profileLabel = account_.profiles()[selectedProfile_].nickname;
        const auto& e = getTextEntry(profileLabel, fontSmall_, T().goldLabel);
        if (e.tex) drawText(profileLabel, SCREEN_W - e.w - 15, SCREEN_H - 26, T().goldLabel, fontSmall_);
    } else {
        drawStatusBar(totalPages > 1 ? "A: Select  B: Quit  L/R: Page  Y: Theme  -: About"
                                     : "A: Select  B: Quit  Y: Theme  -: About");
    }
    if (appletMode_) {
        std::string modeLabel = "Dual Bank Mode";
        const auto& e = getTextEntry(modeLabel, fontSmall_, T().goldLabel);
        if (e.tex) drawText(modeLabel, SCREEN_W - e.w - 15, SCREEN_H - 26, T().goldLabel, fontSmall_);
    }
}

void UI::handleGameSelectorInput(bool& running) {
    int numGames = (int)availableGames_.size();
    if (numGames == 0) return;

    constexpr int COLS = 6;

    constexpr int GAMES_PER_PAGE = 12;

    auto moveGrid = [&](int dx, int dy) {
        int pageStart = gameSelPage_ * GAMES_PER_PAGE;
        int pageEnd = std::min(pageStart + GAMES_PER_PAGE, numGames);
        int pageCount = pageEnd - pageStart;

        if (gameSelOnAllBanks_) {
            // On "All Banks" row: up goes back to grid, left/right ignored
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

        // Wrap columns within the row
        int rowItems = std::min(COLS, pageCount - row * COLS);
        if (rowItems <= 0) rowItems = COLS; // fallback for row wrap
        if (col < 0) col = rowItems - 1;
        if (col >= rowItems) col = 0;

        // Wrap rows (up from top goes to "All Banks")
        if (row < 0) {
            gameSelOnAllBanks_ = true;
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
                    if (gameSelOnAllBanks_)
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
                    constexpr int GAMES_PER_PAGE = 12;
                    int totalPages = (numGames + GAMES_PER_PAGE - 1) / GAMES_PER_PAGE;
                    if (totalPages > 1 && gameSelPage_ > 0) {
                        gameSelPage_--;
                        gameSelCursor_ = gameSelPage_ * GAMES_PER_PAGE;
                        gameSelOnAllBanks_ = false;
                    }
                    break;
                }
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: { // R = next page
                    constexpr int GAMES_PER_PAGE = 12;
                    int totalPages = (numGames + GAMES_PER_PAGE - 1) / GAMES_PER_PAGE;
                    if (totalPages > 1 && gameSelPage_ < totalPages - 1) {
                        gameSelPage_++;
                        gameSelCursor_ = gameSelPage_ * GAMES_PER_PAGE;
                        gameSelOnAllBanks_ = false;
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
        showMessageAndWait("No Banks", "No banks found for any game.");
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
