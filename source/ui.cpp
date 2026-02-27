#include "ui.h"
#include "ui_util.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <sys/statvfs.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

bool UI::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
        return false;

    if (TTF_Init() < 0) {
        SDL_Quit();
        return false;
    }

    if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & IMG_INIT_PNG) == 0) {
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    window_ = SDL_CreateWindow("pkHouse",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!window_) {
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        SDL_DestroyWindow(window_);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    // Load font
#ifdef __SWITCH__
    PlFontData fontData;
    plInitialize(PlServiceType_System);
    plGetSharedFontByType(&fontData, PlSharedFontType_Standard);
    SDL_RWops* rw = SDL_RWFromMem(fontData.address, fontData.size);
    font_ = TTF_OpenFontRW(rw, 0, 18);
    fontSmall_ = TTF_OpenFontRW(SDL_RWFromMem(fontData.address, fontData.size), 0, 14);
    fontLarge_ = TTF_OpenFontRW(SDL_RWFromMem(fontData.address, fontData.size), 0, 28);
#else
    font_ = TTF_OpenFont("romfs/fonts/default.ttf", 18);
    fontSmall_ = TTF_OpenFont("romfs/fonts/default.ttf", 14);
    fontLarge_ = TTF_OpenFont("romfs/fonts/default.ttf", 28);
#endif

    if (!font_ || !fontSmall_) {
        if (!font_)
            font_ = TTF_OpenFont("romfs:/fonts/default.ttf", 18);
        if (!fontSmall_)
            fontSmall_ = TTF_OpenFont("romfs:/fonts/default.ttf", 14);
    }
    if (!fontLarge_)
        fontLarge_ = TTF_OpenFont("romfs:/fonts/default.ttf", 28);

    // Load status icons
    {
#ifdef __SWITCH__
        const char* iconDir = "romfs:/icons/";
#else
        const char* iconDir = "romfs/icons/";
#endif
        auto loadIcon = [&](const char* name) -> SDL_Texture* {
            std::string path = std::string(iconDir) + name;
            SDL_Surface* s = IMG_Load(path.c_str());
            if (!s) return nullptr;
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer_, s);
            SDL_FreeSurface(s);
            return t;
        };
        iconShiny_      = loadIcon("shiny.png");
        iconAlpha_      = loadIcon("alpha.png");
        iconShinyAlpha_ = loadIcon("shiny_alpha.png");
    }

    // Open game controller
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            pad_ = SDL_GameControllerOpen(i);
            break;
        }
    }

    // Set default theme (persisted selection loaded in run())
    theme_ = &getTheme(0);

    return true;
}

void UI::shutdown() {
    freeGameIcons();
    account_.freeTextures();
    freeSprites();
    if (fontLarge_) TTF_CloseFont(fontLarge_);
    if (fontSmall_) TTF_CloseFont(fontSmall_);
    if (font_) TTF_CloseFont(font_);
    if (pad_) SDL_GameControllerClose(pad_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
#ifdef __SWITCH__
    plExit();
#endif
}

void UI::showSplash() {
    if (!renderer_) return;

#ifdef __SWITCH__
    const char* splashPath = "romfs:/splash.png";
#else
    const char* splashPath = "romfs/splash.png";
#endif

    SDL_Surface* surf = IMG_Load(splashPath);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    int texW, texH;
    SDL_QueryTexture(tex, nullptr, nullptr, &texW, &texH);

    // Scale to fit screen while preserving aspect ratio
    float scale = std::min(static_cast<float>(SCREEN_W) / texW,
                           static_cast<float>(SCREEN_H) / texH);
    int dstW = static_cast<int>(texW * scale);
    int dstH = static_cast<int>(texH * scale);
    SDL_Rect dst = {(SCREEN_W - dstW) / 2, (SCREEN_H - dstH) / 2, dstW, dstH};

    // Hold splash for ~2.5 seconds
    Uint32 holdMs = 2500;
    Uint32 start = SDL_GetTicks();
    while (SDL_GetTicks() - start < holdMs) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                SDL_DestroyTexture(tex);
                return;
            }
        }
        SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }

    // Fade out over ~0.5 seconds
    Uint32 fadeMs = 500;
    start = SDL_GetTicks();
    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                SDL_DestroyTexture(tex);
                return;
            }
        }
        Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed >= fadeMs)
            break;
        int alpha = 255 - static_cast<int>(255 * elapsed / fadeMs);
        SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
        SDL_RenderClear(renderer_);
        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(alpha));
        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(tex);
}

void UI::showMessageAndWait(const std::string& title, const std::string& body) {
    if (!renderer_) return;

    bool waiting = true;
    while (waiting) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                waiting = false;
            }
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) // Switch B
                    waiting = false;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_b || event.key.keysym.sym == SDLK_ESCAPE)
                    waiting = false;
            }
        }

        SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
        SDL_RenderClear(renderer_);

        drawTextCentered(title, SCREEN_W / 2, SCREEN_H / 2 - 40, T().red, fontLarge_);
        drawTextCentered(body, SCREEN_W / 2, SCREEN_H / 2 + 15, T().textDim, font_);
        drawTextCentered("Press B to dismiss", SCREEN_W / 2, SCREEN_H / 2 + 65, T().textDim, fontSmall_);

        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }
}

bool UI::showConfirmDialog(const std::string& title, const std::string& body) {
    if (!renderer_) return false;

    int result = -1; // -1 = undecided
    while (result < 0) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                result = 0;
            }
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) // Switch A = confirm
                    result = 1;
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) // Switch B = cancel
                    result = 0;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_RETURN)
                    result = 1;
                if (event.key.keysym.sym == SDLK_b || event.key.keysym.sym == SDLK_ESCAPE)
                    result = 0;
            }
        }

        SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
        SDL_RenderClear(renderer_);

        drawTextCentered(title, SCREEN_W / 2, SCREEN_H / 2 - 40, T().red, fontLarge_);
        drawTextCentered(body, SCREEN_W / 2, SCREEN_H / 2 + 15, T().textDim, font_);
        drawTextCentered("A: Continue   B: Cancel", SCREEN_W / 2, SCREEN_H / 2 + 65, T().textDim, fontSmall_);

        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }
    return result == 1;
}

void UI::showWorking(const std::string& msg) {
    if (!renderer_) return;

    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);

    // Dark card behind gear + message
    constexpr int POP_W = 400;
    constexpr int POP_H = 160;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;
    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().textDim, 2);

    // Draw gear icon
    int gearCX = SCREEN_W / 2;
    int gearCY = popY + 58;
    constexpr int OUTER_R = 28;
    constexpr int INNER_R = 18;
    constexpr int HOLE_R  = 9;
    constexpr int TEETH    = 8;
    constexpr int TOOTH_W  = 12;
    constexpr int TOOTH_H  = 14;

    // Filled circle helper (scanline)
    auto fillCircle = [&](int cx, int cy, int r, SDL_Color c) {
        SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
        for (int dy = -r; dy <= r; dy++) {
            int dx = static_cast<int>(std::sqrt(r * r - dy * dy));
            SDL_RenderDrawLine(renderer_, cx - dx, cy + dy, cx + dx, cy + dy);
        }
    };

    // Tooth rectangles around the gear (8 teeth at 45-degree intervals)
    SDL_Color gearColor = T().arrow;
    SDL_SetRenderDrawColor(renderer_, gearColor.r, gearColor.g, gearColor.b, gearColor.a);
    for (int i = 0; i < TEETH; i++) {
        double angle = i * (3.14159265 * 2.0 / TEETH);
        int tx = gearCX + static_cast<int>((INNER_R + TOOTH_H / 2) * std::cos(angle));
        int ty = gearCY + static_cast<int>((INNER_R + TOOTH_H / 2) * std::sin(angle));
        SDL_Rect tooth = {tx - TOOTH_W / 2, ty - TOOTH_W / 2, TOOTH_W, TOOTH_W};
        SDL_RenderFillRect(renderer_, &tooth);
    }

    // Gear body circle
    fillCircle(gearCX, gearCY, OUTER_R, gearColor);

    // Center hole
    fillCircle(gearCX, gearCY, HOLE_R, T().panelBg);

    // Message text below gear
    drawTextCentered(msg, SCREEN_W / 2, popY + POP_H - 32, T().text, font_);

    SDL_RenderPresent(renderer_);
}

void UI::run(const std::string& basePath, const std::string& savePath) {
    basePath_ = basePath;
    savePath_ = savePath;

    // Load persisted theme
    themeIndex_ = loadThemeIndex(basePath_);
    theme_ = &getTheme(themeIndex_);

    // All games in menu order
    constexpr GameType allGames[] = {
        GameType::GP, GameType::GE, GameType::Sw, GameType::Sh,
        GameType::BD, GameType::SP, GameType::LA, GameType::S,
        GameType::V, GameType::ZA, GameType::FR, GameType::LG
    };

#ifdef __SWITCH__
    if (appletMode_) {
        // Applet mode: skip profile, bank-only access
        screen_ = AppScreen::GameSelector;
        availableGames_.assign(std::begin(allGames), std::end(allGames));
        showWorking("Loading game icons...");
        loadGameIcons();
    } else {
        showWorking("Loading profiles...");
        if (account_.init() && account_.loadProfiles(renderer_)) {
            screen_ = AppScreen::ProfileSelector;
        } else {
            screen_ = AppScreen::GameSelector;
            availableGames_.assign(std::begin(allGames), std::end(allGames));
            showWorking("Loading game icons...");
            loadGameIcons();
        }
    }
#else
    screen_ = AppScreen::GameSelector;
    availableGames_.assign(std::begin(allGames), std::end(allGames));
    showWorking("Loading game icons...");
    loadGameIcons();
#endif

    bool running = true;

    while (running) {
        // About popup intercepts input from any screen
        if (showAbout_) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running = false; break; }
                if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK ||
                        event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
                        showAbout_ = false;
                }
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_MINUS ||
                        event.key.keysym.sym == SDLK_b ||
                        event.key.keysym.sym == SDLK_ESCAPE)
                        showAbout_ = false;
                }
            }
            // Draw the underlying screen, then about popup on top
            if (screen_ == AppScreen::ProfileSelector) drawProfileSelectorFrame();
            else if (screen_ == AppScreen::GameSelector) drawGameSelectorFrame();
            else if (screen_ == AppScreen::BankSelector) drawBankSelectorFrame();
            else drawFrame();
            drawAboutPopup();
            SDL_RenderPresent(renderer_);
            SDL_Delay(16);
            continue;
        }

        // Theme selector intercepts input from any screen
        if (showThemeSelector_) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running = false; break; }
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
                        case SDL_CONTROLLER_BUTTON_DPAD_UP:
                            themeSelCursor_ = (themeSelCursor_ + THEME_COUNT - 1) % THEME_COUNT;
                            theme_ = &getTheme(themeSelCursor_);
                            break;
                        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                            themeSelCursor_ = (themeSelCursor_ + 1) % THEME_COUNT;
                            theme_ = &getTheme(themeSelCursor_);
                            break;
                        case SDL_CONTROLLER_BUTTON_B: // Switch A = confirm
                            themeIndex_ = themeSelCursor_;
                            theme_ = &getTheme(themeIndex_);
                            saveThemeIndex(basePath_, themeIndex_);
                            showThemeSelector_ = false;
                            showMenu_ = false;
                            break;
                        case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
                        case SDL_CONTROLLER_BUTTON_X: // Switch Y = cancel
                            themeIndex_ = themeSelOriginal_;
                            theme_ = &getTheme(themeIndex_);
                            showThemeSelector_ = false;
                            break;
                    }
                }
                if (event.type == SDL_KEYDOWN) {
                    switch (event.key.keysym.sym) {
                        case SDLK_UP:
                            themeSelCursor_ = (themeSelCursor_ + THEME_COUNT - 1) % THEME_COUNT;
                            theme_ = &getTheme(themeSelCursor_);
                            break;
                        case SDLK_DOWN:
                            themeSelCursor_ = (themeSelCursor_ + 1) % THEME_COUNT;
                            theme_ = &getTheme(themeSelCursor_);
                            break;
                        case SDLK_a:
                        case SDLK_RETURN:
                            themeIndex_ = themeSelCursor_;
                            theme_ = &getTheme(themeIndex_);
                            saveThemeIndex(basePath_, themeIndex_);
                            showThemeSelector_ = false;
                            showMenu_ = false;
                            break;
                        case SDLK_b:
                        case SDLK_ESCAPE:
                        case SDLK_y:
                            themeIndex_ = themeSelOriginal_;
                            theme_ = &getTheme(themeIndex_);
                            showThemeSelector_ = false;
                            break;
                    }
                }
            }
            // Joystick repeat
            if (stickDirY_ != 0) {
                uint32_t now = SDL_GetTicks();
                uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
                if (now - stickMoveTime_ >= delay) {
                    themeSelCursor_ = (themeSelCursor_ + (stickDirY_ > 0 ? 1 : THEME_COUNT - 1)) % THEME_COUNT;
                    theme_ = &getTheme(themeSelCursor_);
                    stickMoveTime_ = now;
                    stickMoved_ = true;
                }
            }
            // Draw the underlying screen, then theme popup on top
            if (screen_ == AppScreen::ProfileSelector) drawProfileSelectorFrame();
            else if (screen_ == AppScreen::GameSelector) drawGameSelectorFrame();
            else if (screen_ == AppScreen::BankSelector) drawBankSelectorFrame();
            else drawFrame();
            drawThemeSelectorPopup();
            SDL_RenderPresent(renderer_);
            SDL_Delay(16);
            continue;
        }

        AppScreen screenBefore = screen_;
        if (screen_ == AppScreen::ProfileSelector) {
            handleProfileSelectorInput(running);
            if (screen_ == screenBefore) drawProfileSelectorFrame();
        } else if (screen_ == AppScreen::GameSelector) {
            handleGameSelectorInput(running);
            if (screen_ == screenBefore) drawGameSelectorFrame();
        } else if (screen_ == AppScreen::BankSelector) {
            handleBankSelectorInput(running);
            if (screen_ == screenBefore) drawBankSelectorFrame();
        } else {
            handleInput(running);
            if (saveNow_) {
                if (!saveBankFiles()) {
                    saveNow_ = false;
                    running = true;
                } else {
                    if (!appletMode_) {
                        showWorking("Saving...");
                        if (save_.isLoaded())
                            save_.save(savePath_);
                        account_.commitSave();
                    }
                    saveNow_ = false;
                }
            }
            if (screen_ == screenBefore) drawFrame();
        }
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }

    account_.unmountSave();
    account_.shutdown();
}

void UI::selectGame(GameType game) {
    selectedGame_ = game;
    save_.setGameType(game);
    bankLeft_.setGameType(game);

    if (appletMode_) {
        // Reset left bank state for new game
        leftBankName_.clear();
        leftBankPath_.clear();
        bankSelTarget_ = Panel::Bank;
    }

    if (!appletMode_) {
        showWorking("Loading save data...");

#ifdef __SWITCH__
        if (selectedProfile_ >= 0) {
            std::string mountPath = account_.mountSave(selectedProfile_, game);
            if (mountPath.empty()) {
                showMessageAndWait("Mount Error", "Failed to mount save data.");
                return;
            }
            savePath_ = mountPath + saveFileNameOf(game);

            // Check space and backup save files
            size_t saveSize = AccountManager::calculateDirSize(mountPath);
            bool doBackup = true;

            struct statvfs vfs;
            if (statvfs("sdmc:/", &vfs) == 0) {
                size_t freeSpace = (size_t)vfs.f_bavail * vfs.f_bsize;
                if (freeSpace < saveSize * 2) {
                    std::string msg = "Free: " + formatSize(freeSpace) +
                        ", Need: " + formatSize(saveSize) +
                        ".  Continue without backup?";
                    if (!showConfirmDialog("Low Storage", msg)) {
                        account_.unmountSave();
                        return;
                    }
                    doBackup = false;
                }
            }

            if (doBackup) {
                std::string backupDir = buildBackupDir(game);
                bool ok = AccountManager::backupSaveDir(mountPath, backupDir);
                if (!ok) {
                    if (!showConfirmDialog("Backup Failed",
                            "Could not back up save data.\nContinue without backup?")) {
                        account_.unmountSave();
                        return;
                    }
                }
            }
        } else {
            savePath_ = basePath_ + "main";
        }
#else
        // PC testing: different save file names per game
        if (isSV(game))
            savePath_ = basePath_ + "main_sv";
        else if (isSwSh(game))
            savePath_ = basePath_ + "main_swsh";
        else if (isBDSP(game))
            savePath_ = basePath_ + "main_bdsp";
        else if (game == GameType::LA)
            savePath_ = basePath_ + "main_la";
        else if (isLGPE(game))
            savePath_ = basePath_ + "main_lgpe";
        else if (isFRLG(game))
            savePath_ = basePath_ + "main_frlg";
        else
            savePath_ = basePath_ + "main";
#endif

        save_.load(savePath_);

        // Debug: verify encryption round-trip (encrypt(decrypt(file)) == file)
        if (!isBDSP(game) && !isLGPE(game) && !isFRLG(game)) {
            std::string rtResult = save_.verifyRoundTrip();
            if (rtResult != "OK")
                showMessageAndWait("Round-Trip Check", rtResult);
        }
    }

    bankManager_.init(basePath_, game);

    // Reset bank selector state
    bankSelCursor_ = 0;
    bankSelScroll_ = 0;

    screen_ = AppScreen::BankSelector;
}

std::string UI::buildBackupDir(GameType game) const {
    std::string profileName = "Unknown";
    if (selectedProfile_ >= 0 && selectedProfile_ < account_.profileCount())
        profileName = account_.profiles()[selectedProfile_].pathSafeName;

    std::string dir = basePath_;
    dir += "backups/";
    dir += profileName;
    dir += "/";
    dir += gamePathNameOf(game);
    dir += "/";

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char timestamp[64];
    std::snprintf(timestamp, sizeof(timestamp), "%s_%04d-%02d-%02d_%02d-%02d-%02d",
                  profileName.c_str(),
                  t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                  t->tm_hour, t->tm_min, t->tm_sec);
    dir += timestamp;
    dir += "/";
    return dir;
}

bool UI::saveBankFiles() {
    showWorking("Saving...");
    if (appletMode_) {
        if (!leftBankPath_.empty()) bankLeft_.save(leftBankPath_);
        if (!activeBankPath_.empty()) bank_.save(activeBankPath_);
    } else {
        if (!activeBankPath_.empty()) bank_.save(activeBankPath_);
    }
    return true;
}
