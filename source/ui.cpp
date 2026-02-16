#include "ui.h"
#include "species_converter.h"
#include <algorithm>
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
        SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
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
        SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
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

        SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
        SDL_RenderClear(renderer_);

        drawTextCentered(title, SCREEN_W / 2, SCREEN_H / 2 - 40, COLOR_RED, fontLarge_);
        drawTextCentered(body, SCREEN_W / 2, SCREEN_H / 2 + 15, COLOR_TEXT_DIM, font_);
        drawTextCentered("Press B to dismiss", SCREEN_W / 2, SCREEN_H / 2 + 65, COLOR_TEXT_DIM, fontSmall_);

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

        SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
        SDL_RenderClear(renderer_);

        drawTextCentered(title, SCREEN_W / 2, SCREEN_H / 2 - 40, COLOR_RED, fontLarge_);
        drawTextCentered(body, SCREEN_W / 2, SCREEN_H / 2 + 15, COLOR_TEXT_DIM, font_);
        drawTextCentered("A: Continue   B: Cancel", SCREEN_W / 2, SCREEN_H / 2 + 65, COLOR_TEXT_DIM, fontSmall_);

        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }
    return result == 1;
}

void UI::showWorking(const std::string& msg) {
    if (!renderer_) return;

    SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer_);

    // Dark card behind gear + message
    constexpr int POP_W = 400;
    constexpr int POP_H = 160;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;
    drawRect(popX, popY, POP_W, POP_H, COLOR_PANEL_BG);
    drawRectOutline(popX, popY, POP_W, POP_H, COLOR_TEXT_DIM, 2);

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
    SDL_Color gearColor = COLOR_ARROW;
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
    fillCircle(gearCX, gearCY, HOLE_R, COLOR_PANEL_BG);

    // Message text below gear
    drawTextCentered(msg, SCREEN_W / 2, popY + POP_H - 32, COLOR_TEXT, font_);

    SDL_RenderPresent(renderer_);
}

void UI::run(const std::string& basePath, const std::string& savePath) {
    basePath_ = basePath;
    savePath_ = savePath;

    // All games in menu order
    constexpr GameType allGames[] = {
        GameType::GP, GameType::GE, GameType::Sw, GameType::Sh,
        GameType::BD, GameType::SP, GameType::LA, GameType::S,
        GameType::V, GameType::ZA
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
                showWorking("Saving...");
                if (appletMode_) {
                    if (!leftBankPath_.empty())
                        bankLeft_.save(leftBankPath_);
                    if (!activeBankPath_.empty())
                        bank_.save(activeBankPath_);
                } else {
                    if (save_.isLoaded())
                        save_.save(savePath_);
                    account_.commitSave();
                    if (!activeBankPath_.empty())
                        bank_.save(activeBankPath_);
                }
                saveNow_ = false;
            }
            if (screen_ == screenBefore) drawFrame();
        }
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }

    account_.unmountSave();
    account_.shutdown();
}

// --- Profile Selector ---

void UI::drawProfileSelectorFrame() {
    SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer_);

    drawTextCentered("Select Profile", SCREEN_W / 2, 40, COLOR_TEXT, font_);

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
            drawRect(cardX, cardY, CARD_W, CARD_H, {60, 60, 80, 255});
            drawRectOutline(cardX, cardY, CARD_W, CARD_H, COLOR_CURSOR, 3);
        } else {
            drawRect(cardX, cardY, CARD_W, CARD_H, COLOR_PANEL_BG);
        }

        int iconX = cardX + (CARD_W - ICON_SIZE) / 2;
        int iconY = cardY + 10;

        if (profiles[i].iconTexture) {
            SDL_Rect dst = {iconX, iconY, ICON_SIZE, ICON_SIZE};
            SDL_RenderCopy(renderer_, profiles[i].iconTexture, nullptr, &dst);
        } else {
            drawRect(iconX, iconY, ICON_SIZE, ICON_SIZE, {80, 80, 120, 255});
            if (!profiles[i].nickname.empty()) {
                std::string initial(1, profiles[i].nickname[0]);
                drawTextCentered(initial, iconX + ICON_SIZE / 2, iconY + ICON_SIZE / 2,
                                 COLOR_TEXT, font_);
            }
        }

        std::string name = profiles[i].nickname;
        if (name.length() > 14) name = name.substr(0, 13) + ".";
        drawTextCentered(name, cardX + CARD_W / 2, cardY + ICON_SIZE + 24, COLOR_TEXT, fontSmall_);
    }

    drawStatusBar("A:Select  -:About  +:Quit");
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

        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                    profileSelCursor_ = (profileSelCursor_ + count - 1) % count;
                    break;
                case SDLK_RIGHT:
                    profileSelCursor_ = (profileSelCursor_ + 1) % count;
                    break;
                case SDLK_a:
                case SDLK_RETURN:
                    selectProfile(profileSelCursor_);
                    break;
                case SDLK_MINUS:
                    showAbout_ = true;
                    break;
                case SDLK_ESCAPE:
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
        GameType::V, GameType::ZA
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

    gameSelCursor_ = 0;
    showWorking("Loading game icons...");
    loadGameIcons();
    screen_ = AppScreen::GameSelector;
}

// --- Game Icons ---

void UI::loadGameIcons() {
    freeGameIcons();
#ifdef __SWITCH__
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
#endif
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
    SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer_);

    if (appletMode_) {
        drawTextCentered("Select Game (Dual Bank)", SCREEN_W / 2, 30, COLOR_TEXT, font_);
        drawTextCentered("Use title override mode to transfer between save and bank",
                         SCREEN_W / 2, 55, COLOR_TEXT_DIM, fontSmall_);
    } else {
        drawTextCentered("Select Game", SCREEN_W / 2, 40, COLOR_TEXT, font_);
    }

    int numGames = (int)availableGames_.size();
    constexpr int COLS = 5;
    constexpr int CARD_W = 160;
    constexpr int CARD_H = 200;
    constexpr int CARD_GAP = 20;
    constexpr int ICON_SIZE = 128;

    int rows = (numGames + COLS - 1) / COLS;
    int totalH = rows * CARD_H + (rows - 1) * CARD_GAP;
    int gridStartY = (SCREEN_H - totalH) / 2;

    for (int i = 0; i < numGames; i++) {
        int r = i / COLS;
        int c = i % COLS;

        // Center each row: count items in this row
        int rowItems = std::min(COLS, numGames - r * COLS);
        int rowW = rowItems * CARD_W + (rowItems - 1) * CARD_GAP;
        int rowStartX = (SCREEN_W - rowW) / 2;

        int cardX = rowStartX + c * (CARD_W + CARD_GAP);
        int cardY = gridStartY + r * (CARD_H + CARD_GAP);

        // Card background
        if (i == gameSelCursor_) {
            drawRect(cardX, cardY, CARD_W, CARD_H, {60, 60, 80, 255});
            drawRectOutline(cardX, cardY, CARD_W, CARD_H, COLOR_CURSOR, 3);
        } else {
            drawRect(cardX, cardY, CARD_W, CARD_H, COLOR_PANEL_BG);
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
            drawRect(iconX, iconY, ICON_SIZE, ICON_SIZE, {80, 80, 120, 255});
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
            }
            drawTextCentered(abbr, iconX + ICON_SIZE / 2, iconY + ICON_SIZE / 2,
                             COLOR_TEXT, font_);
        }

        // Game name below icon
        std::string name = gameDisplayNameOf(availableGames_[i]);
        // Strip "Pokemon " prefix for brevity
        if (name.substr(0, 8) == "Pokemon ")
            name = name.substr(8);
        if (name.length() > 20) name = name.substr(0, 19) + ".";
        drawTextCentered(name, cardX + CARD_W / 2, cardY + ICON_SIZE + 30,
                         COLOR_TEXT, fontSmall_);
    }

    if (selectedProfile_ >= 0) {
        drawStatusBar("A:Select  B:Back  -:About  +:Quit");
        std::string profileLabel = account_.profiles()[selectedProfile_].nickname;
        int tw = 0, th = 0;
        TTF_SizeUTF8(fontSmall_, profileLabel.c_str(), &tw, &th);
        drawText(profileLabel, SCREEN_W - tw - 15, SCREEN_H - 30, {255, 215, 0, 255}, fontSmall_);
    } else {
        drawStatusBar("A:Select  B:Quit  -:About");
    }
    if (appletMode_) {
        const char* modeLabel = "Dual Bank Mode";
        int tw = 0, th = 0;
        TTF_SizeUTF8(fontSmall_, modeLabel, &tw, &th);
        drawText(modeLabel, SCREEN_W - tw - 15, SCREEN_H - 30, {255, 215, 0, 255}, fontSmall_);
    }
}

void UI::handleGameSelectorInput(bool& running) {
    int numGames = (int)availableGames_.size();
    if (numGames == 0) return;

    constexpr int COLS = 5;

    auto moveGrid = [&](int dx, int dy) {
        int col = gameSelCursor_ % COLS;
        int row = gameSelCursor_ / COLS;
        int totalRows = (numGames + COLS - 1) / COLS;

        col += dx;
        row += dy;

        // Wrap columns within the row
        int rowItems = std::min(COLS, numGames - row * COLS);
        if (rowItems <= 0) rowItems = COLS; // fallback for row wrap
        if (col < 0) col = rowItems - 1;
        if (col >= rowItems) col = 0;

        // Wrap rows
        if (row < 0) row = totalRows - 1;
        if (row >= totalRows) row = 0;

        int newCursor = row * COLS + col;
        if (newCursor >= numGames)
            newCursor = numGames - 1;
        gameSelCursor_ = newCursor;
    };

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
            return;
        }

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
                case SDL_CONTROLLER_BUTTON_BACK: // - = about
                    showAbout_ = true;
                    break;
                case SDL_CONTROLLER_BUTTON_START:
                    running = false;
                    break;
            }
        }

        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                    moveGrid(-1, 0);
                    break;
                case SDLK_RIGHT:
                    moveGrid(1, 0);
                    break;
                case SDLK_UP:
                    moveGrid(0, -1);
                    break;
                case SDLK_DOWN:
                    moveGrid(0, 1);
                    break;
                case SDLK_a:
                case SDLK_RETURN:
                    selectGame(availableGames_[gameSelCursor_]);
                    break;
                case SDLK_MINUS:
                    showAbout_ = true;
                    break;
                case SDLK_b:
                case SDLK_ESCAPE:
                    if (selectedProfile_ >= 0)
                        screen_ = AppScreen::ProfileSelector;
                    else
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
        }
    }
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
                    auto fmt = [](size_t bytes) -> std::string {
                        if (bytes >= 1024 * 1024)
                            return std::to_string(bytes / (1024 * 1024)) + " MB";
                        return std::to_string(bytes / 1024) + " KB";
                    };
                    std::string msg = "Free: " + fmt(freeSpace) +
                        ", Need: " + fmt(saveSize) +
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
        else
            savePath_ = basePath_ + "main";
#endif

        save_.load(savePath_);

        // Debug: verify encryption round-trip (encrypt(decrypt(file)) == file)
        if (!isBDSP(game) && !isLGPE(game)) {
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

// --- Bank Selector ---

void UI::drawBankSelectorFrame() {
    SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer_);

    // Show split view: bank selector on one side, other panel visible
    // - Normal mode: save on left, selector on right (when save is loaded)
    // - Applet mode: other bank on opposite side (when a bank is loaded)
    bool splitView = appletMode_ ? !activeBankName_.empty() : save_.isLoaded();

    // Determine selector area
    int selCenterX = SCREEN_W / 2;
    int selAreaX = 0;
    int selAreaW = SCREEN_W;
    if (splitView) {
        if (appletMode_ && bankSelTarget_ == Panel::Game) {
            // Applet: selector on left, keep right bank visible
            selCenterX = PANEL_X_L + PANEL_W / 2;
            selAreaX = PANEL_X_L;
            selAreaW = PANEL_W;
            std::string rightBoxName = activeBankName_ + " - " + bank_.getBoxName(bankBox_);
            drawPanel(PANEL_X_R, rightBoxName, bankBox_, bank_.boxCount(),
                      false, nullptr, &bank_, bankBox_, Panel::Bank);
        } else {
            // Normal mode or applet switching right bank: selector on right
            selCenterX = PANEL_X_R + PANEL_W / 2;
            selAreaX = PANEL_X_R;
            selAreaW = PANEL_W;
            // Draw left panel (save or left bank)
            if (appletMode_) {
                if (!leftBankName_.empty()) {
                    std::string leftBoxName = leftBankName_ + " - " + bankLeft_.getBoxName(gameBox_);
                    drawPanel(PANEL_X_L, leftBoxName, gameBox_, bankLeft_.boxCount(),
                              false, nullptr, &bankLeft_, gameBox_, Panel::Game);
                } else {
                    drawPanel(PANEL_X_L, "(No Bank Loaded)", 0, 1,
                              false, nullptr, nullptr, 0, Panel::Game);
                }
            } else {
                std::string gameBoxName = save_.getBoxName(gameBox_);
                drawPanel(PANEL_X_L, gameBoxName, gameBox_, save_.boxCount(),
                          false, &save_, nullptr, gameBox_, Panel::Game);
            }
        }
    }

    // Title
    if (appletMode_) {
        const char* side = (bankSelTarget_ == Panel::Game) ? "Left" : "Right";
        drawTextCentered(std::string("Select ") + side + " Bank",
                         selCenterX, 25, COLOR_TEXT, font_);
    } else {
        drawTextCentered("Select Bank", selCenterX,
                         splitView ? 25 : 40, COLOR_TEXT, font_);
    }

    const auto& banks = bankManager_.list();

    if (banks.empty()) {
        drawTextCentered("No banks found.",
                         selCenterX, SCREEN_H / 2 - 10, COLOR_TEXT_DIM, font_);
        drawTextCentered("Press X to create one.",
                         selCenterX, SCREEN_H / 2 + 15, COLOR_TEXT_DIM, fontSmall_);
    } else {
        // List area
        int LIST_W = splitView ? (PANEL_W - 30) : 800;
        int LIST_X = splitView ? (selAreaX + 15) : (SCREEN_W - LIST_W) / 2;
        int LIST_Y = splitView ? 50 : 80;
        int LIST_BOTTOM = 580;
        int ROW_H = 50;
        int visibleRows = (LIST_BOTTOM - LIST_Y) / ROW_H;

        // Clamp scroll
        int maxScroll = std::max(0, (int)banks.size() - visibleRows);
        if (bankSelScroll_ > maxScroll) bankSelScroll_ = maxScroll;
        if (bankSelScroll_ < 0) bankSelScroll_ = 0;

        // Scroll arrows
        if (bankSelScroll_ > 0) {
            drawTextCentered("^", selCenterX, LIST_Y - 12, COLOR_ARROW, font_);
        }
        if (bankSelScroll_ + visibleRows < (int)banks.size()) {
            drawTextCentered("v", selCenterX, LIST_BOTTOM + 4, COLOR_ARROW, font_);
        }

        for (int i = 0; i < visibleRows && (bankSelScroll_ + i) < (int)banks.size(); i++) {
            int idx = bankSelScroll_ + i;
            int rowY = LIST_Y + i * ROW_H;

            // Highlighted row
            if (idx == bankSelCursor_) {
                drawRect(LIST_X, rowY, LIST_W, ROW_H - 4, {60, 60, 80, 255});
                drawRectOutline(LIST_X, rowY, LIST_W, ROW_H - 4, COLOR_CURSOR, 2);
            }

            // Bank name (left-aligned)
            drawText(banks[idx].name, LIST_X + 20, rowY + (ROW_H - 4) / 2 - 9,
                     COLOR_TEXT, font_);

            // Slot count (right-aligned)
            int maxSlots = isLGPE(selectedGame_) ? 1000 : isBDSP(selectedGame_) ? 1200 : 960;
            std::string slotStr = std::to_string(banks[idx].occupiedSlots) + "/" + std::to_string(maxSlots);
            int tw = 0, th = 0;
            TTF_SizeUTF8(font_, slotStr.c_str(), &tw, &th);
            drawText(slotStr, LIST_X + LIST_W - 20 - tw, rowY + (ROW_H - 4) / 2 - 9,
                     COLOR_TEXT_DIM, font_);
        }
    }

    // Status bar
    drawStatusBar("A:Open  X:New  Y:Rename  +:Delete  B:Back  -:About");

    // Profile | Game name (bottom right, gold)
    {
        std::string label;
        if (appletMode_)
            label = "Dual Bank | ";
        else if (selectedProfile_ >= 0 && selectedProfile_ < account_.profileCount())
            label = account_.profiles()[selectedProfile_].nickname + " | ";
        label += gameDisplayNameOf(selectedGame_);
        int tw = 0, th = 0;
        TTF_SizeUTF8(fontSmall_, label.c_str(), &tw, &th);
        drawText(label, SCREEN_W - tw - 15, SCREEN_H - 30, {255, 215, 0, 255}, fontSmall_);
    }

    // Delete confirmation overlay
    if (showDeleteConfirm_) {
        drawDeleteConfirmPopup();
    }

    // Text input overlay
    if (showTextInput_) {
        drawTextInputPopup();
    }
}

void UI::handleBankSelectorInput(bool& running) {
    const auto& banks = bankManager_.list();
    int bankCount = (int)banks.size();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
            return;
        }

        // Text input popup takes priority
        if (showTextInput_) {
            handleTextInputEvent(event);
            continue;
        }

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
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    if (bankCount > 0) {
                        bankSelCursor_ = (bankSelCursor_ - 1 + bankCount) % bankCount;
                        // Adjust scroll to keep cursor visible
                        if (bankSelCursor_ < bankSelScroll_)
                            bankSelScroll_ = bankSelCursor_;
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    if (bankCount > 0) {
                        bankSelCursor_ = (bankSelCursor_ + 1) % bankCount;
                        int visibleRows = (580 - 80) / 50;
                        if (bankSelCursor_ >= bankSelScroll_ + visibleRows)
                            bankSelScroll_ = bankSelCursor_ - visibleRows + 1;
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_B: // Switch A = open
                    if (bankCount > 0)
                        openSelectedBank();
                    break;
                case SDL_CONTROLLER_BUTTON_A: // Switch B = back
                    if (!activeBankName_.empty()) {
                        // Already have a bank loaded — return to main view
                        screen_ = AppScreen::MainView;
                    } else {
                        account_.unmountSave();
                        screen_ = AppScreen::GameSelector;
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_Y: // Switch X = new
                    beginTextInput(TextInputPurpose::CreateBank);
                    break;
                case SDL_CONTROLLER_BUTTON_X: // Switch Y = rename
                    if (bankCount > 0)
                        beginTextInput(TextInputPurpose::RenameBank);
                    break;
                case SDL_CONTROLLER_BUTTON_BACK: // - = about
                    showAbout_ = true;
                    break;
                case SDL_CONTROLLER_BUTTON_START: // + = delete
                    if (bankCount > 0)
                        showDeleteConfirm_ = true;
                    break;
            }
        }

        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_UP:
                    if (bankCount > 0) {
                        bankSelCursor_ = (bankSelCursor_ - 1 + bankCount) % bankCount;
                        if (bankSelCursor_ < bankSelScroll_)
                            bankSelScroll_ = bankSelCursor_;
                    }
                    break;
                case SDLK_DOWN:
                    if (bankCount > 0) {
                        bankSelCursor_ = (bankSelCursor_ + 1) % bankCount;
                        int visibleRows = (580 - 80) / 50;
                        if (bankSelCursor_ >= bankSelScroll_ + visibleRows)
                            bankSelScroll_ = bankSelCursor_ - visibleRows + 1;
                    }
                    break;
                case SDLK_a:
                case SDLK_RETURN:
                    if (bankCount > 0)
                        openSelectedBank();
                    break;
                case SDLK_x:
                    beginTextInput(TextInputPurpose::CreateBank);
                    break;
                case SDLK_y:
                    if (bankCount > 0)
                        beginTextInput(TextInputPurpose::RenameBank);
                    break;
                case SDLK_DELETE:
                case SDLK_EQUALS: // + key
                    if (bankCount > 0)
                        showDeleteConfirm_ = true;
                    break;
                case SDLK_MINUS:
                    showAbout_ = true;
                    break;
                case SDLK_b:
                case SDLK_ESCAPE:
                    if (!activeBankName_.empty()) {
                        screen_ = AppScreen::MainView;
                    } else {
                        account_.unmountSave();
                        screen_ = AppScreen::GameSelector;
                    }
                    break;
            }
        }
    }

    // Joystick repeat navigation
    if ((stickDirY_ != 0) && bankCount > 0 && !showTextInput_ && !showDeleteConfirm_) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay) {
            if (stickDirY_ < 0) {
                bankSelCursor_ = (bankSelCursor_ - 1 + bankCount) % bankCount;
                if (bankSelCursor_ < bankSelScroll_)
                    bankSelScroll_ = bankSelCursor_;
            } else {
                bankSelCursor_ = (bankSelCursor_ + 1) % bankCount;
                int visibleRows = (580 - 80) / 50;
                if (bankSelCursor_ >= bankSelScroll_ + visibleRows)
                    bankSelScroll_ = bankSelCursor_ - visibleRows + 1;
            }
            stickMoveTime_ = now;
            stickMoved_ = true;
        }
    }
}

void UI::openSelectedBank() {
    const auto& banks = bankManager_.list();
    if (bankSelCursor_ < 0 || bankSelCursor_ >= (int)banks.size())
        return;

    const std::string& name = banks[bankSelCursor_].name;

    // Prevent loading same bank on both sides in applet mode
    if (appletMode_) {
        if (bankSelTarget_ == Panel::Game && name == activeBankName_) {
            showMessageAndWait("Already Open",
                "This bank is already open on the right panel.");
            return;
        }
        if (bankSelTarget_ == Panel::Bank && name == leftBankName_) {
            showMessageAndWait("Already Open",
                "This bank is already open on the left panel.");
            return;
        }
    }

    showWorking("Loading bank...");

    if (appletMode_ && bankSelTarget_ == Panel::Game) {
        leftBankPath_ = bankManager_.loadBank(name, bankLeft_);
        bankLeft_.setGameType(selectedGame_);
        leftBankName_ = name;
    } else {
        activeBankPath_ = bankManager_.loadBank(name, bank_);
        bank_.setGameType(selectedGame_);
        activeBankName_ = name;
    }

    // In applet mode, after loading the right bank, chain to left bank selector
    // (only if there's at least one other bank to choose from)
    if (appletMode_ && bankSelTarget_ == Panel::Bank && leftBankName_.empty()
        && (int)banks.size() > 1) {
        bankSelTarget_ = Panel::Game;
        bankSelCursor_ = 0;
        bankSelScroll_ = 0;
        return;  // stay on BankSelector screen
    }

    screen_ = AppScreen::MainView;

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
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 160});

    constexpr int POP_W = 500;
    constexpr int POP_H = 180;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, COLOR_PANEL_BG);
    drawRectOutline(popX, popY, POP_W, POP_H, COLOR_RED, 2);

    const auto& banks = bankManager_.list();
    std::string bankName = (bankSelCursor_ >= 0 && bankSelCursor_ < (int)banks.size())
        ? banks[bankSelCursor_].name : "";

    drawTextCentered("Delete \"" + bankName + "\"?",
                     popX + POP_W / 2, popY + 50, COLOR_TEXT, font_);
    drawTextCentered("This cannot be undone!",
                     popX + POP_W / 2, popY + 85, COLOR_RED, fontSmall_);
    drawTextCentered("A:Confirm  B:Cancel",
                     popX + POP_W / 2, popY + POP_H - 25, COLOR_TEXT_DIM, fontSmall_);
}

void UI::handleDeleteConfirmEvent(const SDL_Event& event) {
    auto tryDelete = [&]() {
        const auto& banks = bankManager_.list();
        if (bankSelCursor_ < 0 || bankSelCursor_ >= (int)banks.size())
            return;
        const std::string& name = banks[bankSelCursor_].name;
        // Cannot delete a bank that is currently loaded
        if (name == activeBankName_ || (appletMode_ && name == leftBankName_)) {
            showDeleteConfirm_ = false;
            showMessageAndWait("Cannot Delete", "This bank is currently loaded.");
            return;
        }
        showWorking("Deleting bank...");
        bankManager_.deleteBank(name);
        int newCount = (int)bankManager_.list().size();
        if (bankSelCursor_ >= newCount && newCount > 0)
            bankSelCursor_ = newCount - 1;
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
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_a:
            case SDLK_RETURN:
                tryDelete();
                break;
            case SDLK_b:
            case SDLK_ESCAPE:
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
    }

#ifdef __SWITCH__
    SwkbdConfig kbd;
    swkbdCreate(&kbd, 0);
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetStringLenMax(&kbd, 32);
    if (purpose == TextInputPurpose::CreateBank)
        swkbdConfigSetHeaderText(&kbd, "Enter bank name");
    else
        swkbdConfigSetHeaderText(&kbd, "Rename bank");
    if (purpose == TextInputPurpose::RenameBank && !renamingBankName_.empty())
        swkbdConfigSetInitialText(&kbd, renamingBankName_.c_str());
    char result[64] = {};
    Result rc = swkbdShow(&kbd, result, sizeof(result));
    swkbdClose(&kbd);
    if (R_SUCCEEDED(rc) && result[0])
        commitTextInput(result);
#else
    showTextInput_ = true;
    SDL_StartTextInput();
#endif
}

void UI::drawTextInputPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 160});

    constexpr int POP_W = 500;
    constexpr int POP_H = 180;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, COLOR_PANEL_BG);
    drawRectOutline(popX, popY, POP_W, POP_H, COLOR_CURSOR, 2);

    const char* title = (textInputPurpose_ == TextInputPurpose::CreateBank)
        ? "New Bank Name" : "Rename Bank";
    drawTextCentered(title, popX + POP_W / 2, popY + 30, COLOR_TEXT, font_);

    // Text field background
    int fieldX = popX + 40;
    int fieldY = popY + 60;
    int fieldW = POP_W - 80;
    int fieldH = 36;
    drawRect(fieldX, fieldY, fieldW, fieldH, {30, 30, 40, 255});
    drawRectOutline(fieldX, fieldY, fieldW, fieldH, COLOR_TEXT_DIM, 1);

    // Text content
    if (!textInputBuffer_.empty()) {
        drawText(textInputBuffer_, fieldX + 8, fieldY + 8, COLOR_TEXT, font_);
    }

    // Blinking cursor
    int cursorX = fieldX + 8;
    if (!textInputBuffer_.empty() && textInputCursorPos_ > 0) {
        std::string beforeCursor = textInputBuffer_.substr(0, textInputCursorPos_);
        int tw = 0, th = 0;
        TTF_SizeUTF8(font_, beforeCursor.c_str(), &tw, &th);
        cursorX += tw;
    }
    // Blink every 500ms
    if ((SDL_GetTicks() / 500) % 2 == 0) {
        drawRect(cursorX, fieldY + 6, 2, fieldH - 12, COLOR_TEXT);
    }

    drawTextCentered("Enter:Confirm  Escape:Cancel",
                     popX + POP_W / 2, popY + POP_H - 25, COLOR_TEXT_DIM, fontSmall_);
}

void UI::handleTextInputEvent(const SDL_Event& event) {
    if (event.type == SDL_TEXTINPUT) {
        if ((int)textInputBuffer_.size() < 32) {
            std::string input = event.text.text;
            textInputBuffer_.insert(textInputCursorPos_, input);
            textInputCursorPos_ += (int)input.size();
        }
        return;
    }

    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_RETURN:
                SDL_StopTextInput();
                showTextInput_ = false;
                if (!textInputBuffer_.empty())
                    commitTextInput(textInputBuffer_);
                break;
            case SDLK_ESCAPE:
                SDL_StopTextInput();
                showTextInput_ = false;
                break;
            case SDLK_BACKSPACE:
                if (textInputCursorPos_ > 0) {
                    textInputBuffer_.erase(textInputCursorPos_ - 1, 1);
                    textInputCursorPos_--;
                }
                break;
            case SDLK_DELETE:
                if (textInputCursorPos_ < (int)textInputBuffer_.size()) {
                    textInputBuffer_.erase(textInputCursorPos_, 1);
                }
                break;
            case SDLK_LEFT:
                if (textInputCursorPos_ > 0)
                    textInputCursorPos_--;
                break;
            case SDLK_RIGHT:
                if (textInputCursorPos_ < (int)textInputBuffer_.size())
                    textInputCursorPos_++;
                break;
        }
    }
}

void UI::commitTextInput(const std::string& text) {
    if (textInputPurpose_ == TextInputPurpose::CreateBank) {
        showWorking("Creating bank...");
        if (bankManager_.createBank(text)) {
            // Select the newly created bank
            const auto& banks = bankManager_.list();
            for (int i = 0; i < (int)banks.size(); i++) {
                if (banks[i].name == text) {
                    bankSelCursor_ = i;
                    break;
                }
            }
        }
    } else if (textInputPurpose_ == TextInputPurpose::RenameBank) {
        showWorking("Renaming bank...");
        if (bankManager_.renameBank(renamingBankName_, text)) {
            // Select the renamed bank
            const auto& banks = bankManager_.list();
            for (int i = 0; i < (int)banks.size(); i++) {
                if (banks[i].name == text) {
                    bankSelCursor_ = i;
                    break;
                }
            }
        }
    }
}

// --- Sprites ---

SDL_Texture* UI::getSprite(uint16_t nationalId) {
    // Check cache
    auto it = spriteCache_.find(nationalId);
    if (it != spriteCache_.end())
        return it->second;

    // Build path: sprites are named 001.png to 1025.png
    char filename[64];
    std::snprintf(filename, sizeof(filename), "%03d.png", nationalId);

    std::string path;
#ifdef __SWITCH__
    path = std::string("romfs:/sprites/") + filename;
#else
    path = std::string("romfs/sprites/") + filename;
#endif

    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) {
        // Cache nullptr so we don't retry
        spriteCache_[nationalId] = nullptr;
        return nullptr;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_FreeSurface(surf);

    spriteCache_[nationalId] = tex;
    return tex;
}

void UI::freeSprites() {
    for (auto& [id, tex] : spriteCache_) {
        if (tex)
            SDL_DestroyTexture(tex);
    }
    spriteCache_.clear();
    if (iconShiny_)      { SDL_DestroyTexture(iconShiny_);      iconShiny_ = nullptr; }
    if (iconAlpha_)      { SDL_DestroyTexture(iconAlpha_);      iconAlpha_ = nullptr; }
    if (iconShinyAlpha_) { SDL_DestroyTexture(iconShinyAlpha_); iconShinyAlpha_ = nullptr; }
}

// --- Rendering ---

void UI::drawRect(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer_, &r);
}

void UI::drawRectOutline(int x, int y, int w, int h, SDL_Color color, int thickness) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int t = 0; t < thickness; t++) {
        SDL_Rect r = {x + t, y + t, w - 2*t, h - 2*t};
        SDL_RenderDrawRect(renderer_, &r);
    }
}

void UI::drawText(const std::string& text, int x, int y, SDL_Color color, TTF_Font* f) {
    if (!f || text.empty()) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, text.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void UI::drawTextCentered(const std::string& text, int cx, int cy, SDL_Color color, TTF_Font* f) {
    if (!f || text.empty()) return;
    int tw, th;
    TTF_SizeUTF8(f, text.c_str(), &tw, &th);
    drawText(text, cx - tw/2, cy - th/2, color, f);
}

void UI::drawStatusBar(const std::string& msg) {
    drawRect(0, SCREEN_H - 35, SCREEN_W, 35, {20, 20, 30, 255});
    drawText(msg, 15, SCREEN_H - 30, COLOR_STATUS, fontSmall_);
}

void UI::drawSlot(int x, int y, const Pokemon& pkm, bool isCursor, int selectOrder) {
    SDL_Color bgColor;
    if (pkm.isEmpty()) {
        bgColor = COLOR_SLOT_EMPTY;
    } else if (pkm.isEgg()) {
        bgColor = COLOR_SLOT_EGG;
    } else {
        bgColor = COLOR_SLOT_FULL;
    }

    // Slot background
    drawRect(x, y, CELL_W, CELL_H, bgColor);

    // Selection outline (drawn before cursor so cursor overlays it)
    if (selectOrder > 0) {
        SDL_Color selColor = positionPreserve_ ? COLOR_SELECTED_POS : COLOR_SELECTED;
        drawRectOutline(x + 1, y + 1, CELL_W - 2, CELL_H - 2, selColor, 2);
    }

    // Cursor highlight
    if (isCursor)
        drawRectOutline(x, y, CELL_W, CELL_H, COLOR_CURSOR, 3);

    if (!pkm.isEmpty()) {
        uint16_t species = pkm.species();

        // Draw sprite centered in top portion of cell
        SDL_Texture* sprite = nullptr;
        if (pkm.isEgg()) {
            sprite = getSprite(0);
        } else {
            sprite = getSprite(species);
        }

        if (sprite) {
            int texW, texH;
            SDL_QueryTexture(sprite, nullptr, nullptr, &texW, &texH);

            int dstW, dstH;
            if (texW > 0 && texH > 0) {
                float scale = std::min(static_cast<float>(SPRITE_SIZE) / texW,
                                       static_cast<float>(SPRITE_SIZE) / texH);
                dstW = static_cast<int>(texW * scale);
                dstH = static_cast<int>(texH * scale);
            } else {
                dstW = SPRITE_SIZE;
                dstH = SPRITE_SIZE;
            }

            int sprX = x + (CELL_W - dstW) / 2;
            int sprY = y + 4 + (SPRITE_SIZE - dstH) / 2;
            SDL_Rect dst = {sprX, sprY, dstW, dstH};
            SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
        }

        // Species name below sprite
        std::string name = pkm.displayName();
        if (name.length() > 10)
            name = name.substr(0, 9) + ".";

        SDL_Color nameColor = pkm.isShiny() ? COLOR_SHINY : COLOR_TEXT;
        drawTextCentered(name, x + CELL_W / 2, y + SPRITE_SIZE + 10, nameColor, fontSmall_);

        // Level at the bottom
        if (!pkm.isEgg()) {
            std::string lvlStr = "Lv." + std::to_string(pkm.level());
            drawTextCentered(lvlStr, x + CELL_W / 2, y + CELL_H - 12, COLOR_TEXT_DIM, fontSmall_);
        }

        // Gender indicator (top-right corner)
        uint8_t g = pkm.gender();
        if (g == 0)
            drawText("\xe2\x99\x82", x + CELL_W - 16, y + 2, {100, 150, 255, 255}, fontSmall_);
        else if (g == 1)
            drawText("\xe2\x99\x80", x + CELL_W - 16, y + 2, {255, 130, 150, 255}, fontSmall_);

        // Shiny / Alpha icon (top-left corner)
        SDL_Texture* statusIcon = nullptr;
        if (pkm.isShiny() && pkm.isAlpha())
            statusIcon = iconShinyAlpha_;
        else if (pkm.isShiny())
            statusIcon = iconShiny_;
        else if (pkm.isAlpha())
            statusIcon = iconAlpha_;

        if (statusIcon) {
            SDL_Rect iconDst = {x + 2, y + 2, 14, 14};
            SDL_RenderCopy(renderer_, statusIcon, nullptr, &iconDst);
        }
    }

    // Numbered badge on top of everything
    if (selectOrder > 0) {
        std::string num = std::to_string(selectOrder);
        int tw = 0, th = 0;
        TTF_SizeUTF8(font_, num.c_str(), &tw, &th);
        int badgeR = std::max(tw, th) / 2 + 6;
        int cx = x + CELL_W / 2;
        int cy = y + CELL_H / 2;

        SDL_Color badgeColor = positionPreserve_ ? COLOR_SELECTED_POS : COLOR_SELECTED;
        SDL_SetRenderDrawColor(renderer_, badgeColor.r, badgeColor.g, badgeColor.b, 220);
        for (int dy2 = -badgeR; dy2 <= badgeR; dy2++) {
            int dx2 = static_cast<int>(std::sqrt(badgeR * badgeR - dy2 * dy2));
            SDL_RenderDrawLine(renderer_, cx - dx2, cy + dy2, cx + dx2, cy + dy2);
        }
        drawTextCentered(num, cx, cy, {0, 0, 0, 255}, font_);
    }
}

void UI::drawPanel(int panelX, const std::string& boxName, int boxIdx,
                   int totalBoxes, bool isActive, SaveFile* save, Bank* bank, int box,
                   Panel panelId) {
    // Panel background
    drawRect(panelX, 0, PANEL_W, SCREEN_H - 35, COLOR_PANEL_BG);

    // Box name header with arrows
    SDL_Color hdrColor = isActive ? COLOR_BOX_NAME : COLOR_TEXT_DIM;
    drawTextCentered("<", panelX + 20, BOX_HDR_Y + BOX_HDR_H / 2, COLOR_ARROW, font_);
    drawTextCentered(boxName + " (" + std::to_string(boxIdx + 1) + "/" + std::to_string(totalBoxes) + ")",
                     panelX + PANEL_W / 2, BOX_HDR_Y + BOX_HDR_H / 2, hdrColor, font_);
    drawTextCentered(">", panelX + PANEL_W - 20, BOX_HDR_Y + BOX_HDR_H / 2, COLOR_ARROW, font_);

    // Grid: dynamic columns x 5 rows
    int cols = gridCols();
    int gridStartX = panelX + (PANEL_W - (cols * (CELL_W + CELL_PAD) - CELL_PAD)) / 2;
    int gridStartY = GRID_Y;

    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < cols; col++) {
            int slot = row * cols + col;
            int cellX = gridStartX + col * (CELL_W + CELL_PAD);
            int cellY = gridStartY + row * (CELL_H + CELL_PAD);

            Pokemon pkm;
            if (save)
                pkm = save->getBoxSlot(box, slot);
            else if (bank)
                pkm = bank->getSlot(box, slot);

            bool isCursor = isActive && cursor_.col == col && cursor_.row == row;
            int selOrder = 0;
            if (!selectedSlots_.empty()
                && panelId == selectedPanel_ && box == selectedBox_) {
                for (int i = 0; i < (int)selectedSlots_.size(); i++) {
                    if (selectedSlots_[i] == slot) {
                        selOrder = i + 1;
                        break;
                    }
                }
            }
            drawSlot(cellX, cellY, pkm, isCursor, selOrder);
        }
    }
}

void UI::drawFrame() {
    // Clear screen
    SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer_);

    // Sync cursor box to the active panel
    if (cursor_.panel == Panel::Game)
        gameBox_ = cursor_.box;
    else
        bankBox_ = cursor_.box;

    if (appletMode_) {
        // Dual-bank mode: two bank panels side by side
        bool leftActive = (cursor_.panel == Panel::Game);
        bool rightActive = (cursor_.panel == Panel::Bank);

        // Left panel: left bank
        std::string leftBoxName;
        if (!leftBankName_.empty())
            leftBoxName = leftBankName_ + " - " + bankLeft_.getBoxName(gameBox_);
        else
            leftBoxName = "(No Bank Loaded)";
        drawPanel(PANEL_X_L, leftBoxName, gameBox_,
                  leftBankName_.empty() ? 1 : bankLeft_.boxCount(),
                  leftActive, nullptr,
                  leftBankName_.empty() ? nullptr : &bankLeft_,
                  gameBox_, Panel::Game);

        // Right panel: right bank
        std::string rightBoxName = activeBankName_ + " - " + bank_.getBoxName(bankBox_);
        drawPanel(PANEL_X_R, rightBoxName, bankBox_, bank_.boxCount(),
                  rightActive, nullptr, &bank_, bankBox_, Panel::Bank);
    } else {
        // Normal mode: save + bank
        bool leftActive = (cursor_.panel == Panel::Game);
        std::string gameBoxName = save_.getBoxName(gameBox_);
        drawPanel(PANEL_X_L, gameBoxName, gameBox_, save_.boxCount(),
                  leftActive, &save_, nullptr, gameBox_, Panel::Game);

        bool rightActive = (cursor_.panel == Panel::Bank);
        std::string bankBoxName = activeBankName_ + " - " + bank_.getBoxName(bankBox_);
        drawPanel(PANEL_X_R, bankBoxName, bankBox_, bank_.boxCount(),
                  rightActive, nullptr, &bank_, bankBox_, Panel::Bank);
    }

    // Status bar
    std::string statusMsg = "D-Pad:Move  L/R:Box  A:Pick/Place  Y:Select  YY:All  B:Cancel  X:Detail";
    if (holding_ && !heldMulti_.empty()) {
        statusMsg = "Holding " + std::to_string(heldMulti_.size()) +
                    " Pokemon" + (positionPreserve_ ? " (keep positions)" : "") +
                    "  |  A:Place  B:Return";
    } else if (holding_) {
        std::string heldName = heldPkm_.displayName();
        if (!heldName.empty()) {
            statusMsg = "Holding: " + heldName + " Lv." + std::to_string(heldPkm_.level()) +
                        "  |  A:Place  B:Return";
        }
    } else if (yHeld_ && yDragActive_) {
        statusMsg = "Drag selecting: " + std::to_string(selectedSlots_.size()) +
                    " slots  |  Release Y to confirm";
    } else if (!selectedSlots_.empty()) {
        statusMsg = std::to_string(selectedSlots_.size()) +
                    " selected" + (positionPreserve_ ? " (keep positions)" : "") +
                    "  |  A:Pick up  Y:Toggle/Drag  B:Clear";
    }
    drawStatusBar(statusMsg);

    // Profile | Game name (bottom right, gold)
    {
        std::string label;
        if (appletMode_)
            label = "Dual Bank | ";
        else if (selectedProfile_ >= 0 && selectedProfile_ < account_.profileCount())
            label = account_.profiles()[selectedProfile_].nickname + " | ";
        label += gameDisplayNameOf(selectedGame_);
        int tw = 0, th = 0;
        TTF_SizeUTF8(fontSmall_, label.c_str(), &tw, &th);
        drawText(label, SCREEN_W - tw - 15, SCREEN_H - 30, {255, 215, 0, 255}, fontSmall_);
    }

    // Held Pokemon overlay (draw on top of panels, under popups)
    if (holding_)
        drawHeldOverlay();

    // Detail popup overlay
    if (showDetail_) {
        Pokemon pkm = getPokemonAt(cursor_.box, cursor_.slot(gridCols()), cursor_.panel);
        if (pkm.isEmpty()) {
            showDetail_ = false;
        } else {
            drawDetailPopup(pkm);
        }
    }

    // Menu popup overlay
    if (showMenu_) {
        drawMenuPopup();
    }

    // Box view overlay
    if (showBoxView_) {
        drawBoxViewOverlay();
    }
}

void UI::drawDetailPopup(const Pokemon& pkm) {
    // Semi-transparent dark overlay
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 160});

    // Popup rect centered
    constexpr int POP_W = 900;
    constexpr int POP_H = 550;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, COLOR_PANEL_BG);
    drawRectOutline(popX, popY, POP_W, POP_H, COLOR_CURSOR, 2);

    // Large sprite (128x128) top-left
    constexpr int LARGE_SPRITE = 128;
    int sprX = popX + 20;
    int sprY = popY + 20;

    SDL_Texture* sprite = getSprite(pkm.isEgg() ? 0 : pkm.species());
    if (sprite) {
        int texW, texH;
        SDL_QueryTexture(sprite, nullptr, nullptr, &texW, &texH);
        int dstW, dstH;
        if (texW > 0 && texH > 0) {
            float scale = std::min(static_cast<float>(LARGE_SPRITE) / texW,
                                   static_cast<float>(LARGE_SPRITE) / texH);
            dstW = static_cast<int>(texW * scale);
            dstH = static_cast<int>(texH * scale);
        } else {
            dstW = LARGE_SPRITE;
            dstH = LARGE_SPRITE;
        }
        int dx = sprX + (LARGE_SPRITE - dstW) / 2;
        int dy = sprY + (LARGE_SPRITE - dstH) / 2;
        SDL_Rect dst = {dx, dy, dstW, dstH};
        SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
    }

    // Shiny/Alpha icon next to sprite
    SDL_Texture* statusIcon = nullptr;
    if (pkm.isShiny() && pkm.isAlpha())
        statusIcon = iconShinyAlpha_;
    else if (pkm.isShiny())
        statusIcon = iconShiny_;
    else if (pkm.isAlpha())
        statusIcon = iconAlpha_;
    if (statusIcon) {
        SDL_Rect iconDst = {sprX + LARGE_SPRITE + 4, sprY, 20, 20};
        SDL_RenderCopy(renderer_, statusIcon, nullptr, &iconDst);
    }

    // --- Left column info (next to sprite) ---
    int infoX = sprX + LARGE_SPRITE + 30;
    int infoY = sprY + 4;

    // Species name + level + gender
    std::string specName = SpeciesName::get(pkm.species());
    SDL_Color nameColor = pkm.isShiny() ? COLOR_SHINY : COLOR_TEXT;
    drawText(specName, infoX, infoY, nameColor, font_);

    std::string lvlStr = "  Lv." + std::to_string(pkm.level());
    int nameW = 0, nameH = 0;
    TTF_SizeUTF8(font_, specName.c_str(), &nameW, &nameH);
    drawText(lvlStr, infoX + nameW, infoY, COLOR_TEXT, font_);

    // Gender symbol
    uint8_t g = pkm.gender();
    int afterLvl = infoX + nameW;
    int lvlW = 0;
    TTF_SizeUTF8(font_, lvlStr.c_str(), &lvlW, nullptr);
    afterLvl += lvlW + 4;
    if (g == 0)
        drawText("\xe2\x99\x82", afterLvl, infoY, {100, 150, 255, 255}, font_);
    else if (g == 1)
        drawText("\xe2\x99\x80", afterLvl, infoY, {255, 130, 150, 255}, font_);

    infoY += 30;

    // National dex ID
    std::string idStr = "National #" + std::to_string(pkm.species());
    drawText(idStr, infoX, infoY, COLOR_TEXT_DIM, font_);
    infoY += 28;

    // OT + TID
    std::string otStr = "OT: " + pkm.otName() + " | TID: " + std::to_string(pkm.tid());
    drawText(otStr, infoX, infoY, COLOR_TEXT_DIM, font_);
    infoY += 28;

    // Nature
    std::string natureStr = "Nature: " + NatureName::get(pkm.nature());
    drawText(natureStr, infoX, infoY, COLOR_TEXT_DIM, font_);
    infoY += 28;

    // Ability
    std::string abilityStr = "Ability: " + AbilityName::get(pkm.ability());
    drawText(abilityStr, infoX, infoY, COLOR_TEXT_DIM, font_);

    // --- Below sprite: Moves ---
    int movesX = popX + 30;
    int movesY = sprY + LARGE_SPRITE + 20;

    drawText("Moves", movesX, movesY, COLOR_TEXT, font_);
    movesY += 30;

    uint16_t moves[4] = {pkm.move1(), pkm.move2(), pkm.move3(), pkm.move4()};
    for (int i = 0; i < 4; i++) {
        if (moves[i] != 0) {
            std::string moveStr = "- " + MoveName::get(moves[i]);
            drawText(moveStr, movesX + 10, movesY, COLOR_TEXT_DIM, font_);
        } else {
            drawText("- ---", movesX + 10, movesY, COLOR_TEXT_DIM, font_);
        }
        movesY += 26;
    }

    // --- Right column: IVs and EVs ---
    int statsX = popX + POP_W / 2 + 20;
    int statsY = popY + 20;

    // IVs header
    drawText("IVs", statsX, statsY, COLOR_TEXT, font_);
    statsY += 30;

    const char* statLabels[] = {"HP", "Atk", "Def", "SpA", "SpD", "Spe"};
    int ivs[] = {pkm.ivHp(), pkm.ivAtk(), pkm.ivDef(), pkm.ivSpA(), pkm.ivSpD(), pkm.ivSpe()};

    for (int i = 0; i < 6; i++) {
        std::string line = std::string(statLabels[i]) + ": " + std::to_string(ivs[i]);
        SDL_Color ivColor = (ivs[i] == 31) ? COLOR_SHINY : COLOR_TEXT_DIM;
        drawText(line, statsX + 10, statsY, ivColor, font_);
        statsY += 26;
    }

    statsY += 10;

    // EVs header
    drawText("EVs", statsX, statsY, COLOR_TEXT, font_);
    statsY += 30;

    int evs[] = {pkm.evHp(), pkm.evAtk(), pkm.evDef(), pkm.evSpA(), pkm.evSpD(), pkm.evSpe()};

    for (int i = 0; i < 6; i++) {
        std::string line = std::string(statLabels[i]) + ": " + std::to_string(evs[i]);
        SDL_Color evColor = (evs[i] > 0) ? COLOR_TEXT : COLOR_TEXT_DIM;
        drawText(line, statsX + 10, statsY, evColor, font_);
        statsY += 26;
    }

    // Close hint at bottom
    drawTextCentered("B / X: Close", popX + POP_W / 2, popY + POP_H - 20, COLOR_TEXT_DIM, fontSmall_);
}

void UI::drawMenuPopup() {
    // Semi-transparent dark overlay
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 160});

    // Menu items differ by mode
    // Applet: Switch Left Bank / Switch Right Bank / Change Game / Save Banks / Quit
    // Normal: Switch Bank / Change Game / Save & Quit / Quit Without Saving
    int menuCount = appletMode_ ? 5 : 4;

    constexpr int POP_W = 380;
    int POP_H = appletMode_ ? 296 : 256;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, COLOR_PANEL_BG);
    drawRectOutline(popX, popY, POP_W, POP_H, COLOR_CURSOR, 2);

    drawTextCentered("Menu", popX + POP_W / 2, popY + 22, COLOR_TEXT, font_);

    const char* labelsNormal[] = {
        "Switch Bank",
        "Change Game",
        "Save & Quit",
        "Quit Without Saving"
    };
    const char* labelsApplet[] = {
        "Switch Left Bank",
        "Switch Right Bank",
        "Change Game",
        "Save Banks",
        "Quit"
    };
    const char** labels = appletMode_ ? labelsApplet : labelsNormal;
    int rowH = 36;
    int startY = popY + 50;

    for (int i = 0; i < menuCount; i++) {
        int rowY = startY + i * rowH;
        if (i == menuSelection_) {
            drawRect(popX + 20, rowY, POP_W - 40, rowH - 4, {60, 60, 80, 255});
            drawRectOutline(popX + 20, rowY, POP_W - 40, rowH - 4, COLOR_CURSOR, 2);
        }
        drawTextCentered(labels[i], popX + POP_W / 2, rowY + (rowH - 4) / 2, COLOR_TEXT, font_);
    }

    drawTextCentered("A:Confirm  B:Cancel", popX + POP_W / 2, popY + POP_H - 18, COLOR_TEXT_DIM, fontSmall_);
}

void UI::drawAboutPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 187});

    constexpr int POP_W = 700;
    constexpr int POP_H = 490;
    int px = (SCREEN_W - POP_W) / 2;
    int py = (SCREEN_H - POP_H) / 2;

    drawRect(px, py, POP_W, POP_H, COLOR_PANEL_BG);
    drawRectOutline(px, py, POP_W, POP_H, {0x30, 0x30, 0x55, 255}, 2);

    int cx = px + POP_W / 2;
    int y = py + 25;

    // Title
    drawTextCentered("pkHouse - Local Bank System", cx, y, COLOR_SHINY, fontLarge_);
    y += 38;

    // Version / author
    drawTextCentered("v" APP_VERSION " - Developed by " APP_AUTHOR, cx, y, COLOR_TEXT_DIM, fontSmall_);
    y += 22;
    drawTextCentered("github.com/Insektaure", cx, y, COLOR_TEXT_DIM, fontSmall_);
    y += 30;

    // Divider
    SDL_SetRenderDrawColor(renderer_, 0x30, 0x30, 0x55, 255);
    SDL_RenderDrawLine(renderer_, px + 30, y, px + POP_W - 30, y);
    y += 20;

    // Description
    drawTextCentered("Pokemon Box Manager for Nintendo Switch", cx, y, COLOR_TEXT, font_);
    y += 28;
    drawTextCentered("Move Pokemon between save files and banks.", cx, y, COLOR_TEXT, font_);
    y += 28;
    drawTextCentered("Supported Games", cx, y, COLOR_SELECTED, font_);
    y += 24;
    drawTextCentered("Let's Go Pikachu/Eevee (1.0.2)  -  Sword/Shield (1.3.2)", cx, y, COLOR_TEXT_DIM, fontSmall_);
    y += 20;
    drawTextCentered("Brilliant Diamond/Shining Pearl (1.3.0)  -  Legends: Arceus (1.1.1)", cx, y, COLOR_TEXT_DIM, fontSmall_);
    y += 20;
    drawTextCentered("Scarlet/Violet (4.0.0)  -  Legends: Z-A (2.0.1)", cx, y, COLOR_TEXT_DIM, fontSmall_);
    y += 30;

    // Divider
    SDL_SetRenderDrawColor(renderer_, 0x30, 0x30, 0x55, 255);
    SDL_RenderDrawLine(renderer_, px + 30, y, px + POP_W - 30, y);
    y += 20;

    // Credits
    drawTextCentered("Based on PKHeX by kwsch & PokeCrypto research.", cx, y, {0x88, 0x88, 0x88, 255}, fontSmall_);
    y += 20;
    drawTextCentered("Save backup & write logic based on JKSV by J-D-K.", cx, y, {0x88, 0x88, 0x88, 255}, fontSmall_);
    y += 35;

    // Controls
    drawTextCentered("Controls", cx, y, COLOR_SELECTED, font_);
    y += 28;
    drawText("A: Pick/Place    B: Cancel    X: Details    Y: Multi-select", px + 50, y, COLOR_TEXT_DIM, fontSmall_);
    y += 20;
    drawText("L/R: Switch Box    ZL/ZR: Box View    +: Menu    -: About", px + 50, y, COLOR_TEXT_DIM, fontSmall_);

    // Footer
    drawTextCentered("Press - or B to close", cx, py + POP_H - 22, COLOR_TEXT_DIM, fontSmall_);
}

void UI::drawBoxViewOverlay() {
    // Full-screen dark overlay
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 160});

    int totalBoxes;
    if (boxViewPanel_ == Panel::Game)
        totalBoxes = (appletMode_) ? bankLeft_.boxCount() : save_.boxCount();
    else
        totalBoxes = bank_.boxCount();
    int usedRows = (totalBoxes + BV_COLS - 1) / BV_COLS;

    // Popup dimensions
    int gridW = BV_COLS * BV_CELL_W + (BV_COLS - 1) * BV_CELL_PAD;
    int gridH = usedRows * BV_CELL_H + (usedRows - 1) * BV_CELL_PAD;
    int popW = gridW + 40;
    int popH = gridH + 80;

    int popX = (SCREEN_W - popW) / 2;
    int popY = (SCREEN_H - popH) / 2;

    // Popup background
    drawRect(popX, popY, popW, popH, COLOR_PANEL_BG);
    drawRectOutline(popX, popY, popW, popH, COLOR_CURSOR, 2);

    // Title
    const char* title;
    if (boxViewPanel_ == Panel::Game)
        title = appletMode_ ? "Left Bank Boxes" : "Save Boxes";
    else
        title = "Bank Boxes";
    drawTextCentered(title, popX + popW / 2, popY + 20, COLOR_TEXT, font_);

    // Grid of box cells
    int gridStartX = popX + 20;
    int gridStartY = popY + 45;
    int activeBox = (boxViewPanel_ == Panel::Game) ? gameBox_ : bankBox_;

    int cursorCellX = 0, cursorCellY = 0;

    for (int i = 0; i < totalBoxes; i++) {
        int col = i % BV_COLS;
        int row = i / BV_COLS;
        int cellX = gridStartX + col * (BV_CELL_W + BV_CELL_PAD);
        int cellY = gridStartY + row * (BV_CELL_H + BV_CELL_PAD);

        // Cell background — highlight the currently-active box
        SDL_Color bg = (i == activeBox) ? COLOR_SLOT_FULL : COLOR_SLOT_EMPTY;
        drawRect(cellX, cellY, BV_CELL_W, BV_CELL_H, bg);

        // Cursor outline
        if (i == boxViewCursor_) {
            drawRectOutline(cellX, cellY, BV_CELL_W, BV_CELL_H, COLOR_CURSOR, 2);
            cursorCellX = cellX;
            cursorCellY = cellY;
        }

        // Box label
        std::string boxName;
        if (boxViewPanel_ == Panel::Game)
            boxName = (appletMode_) ? bankLeft_.getBoxName(i) : save_.getBoxName(i);
        else
            boxName = bank_.getBoxName(i);

        std::string label = std::to_string(i + 1) + ": " + boxName;
        if (label.length() > 16)
            label = label.substr(0, 15) + ".";

        drawTextCentered(label, cellX + BV_CELL_W / 2, cellY + BV_CELL_H / 2,
                         COLOR_TEXT, fontSmall_);
    }

    // Footer hint
    drawTextCentered("A:Go to Box  B:Cancel  D-Pad:Navigate",
                     popX + popW / 2, popY + popH - 15, COLOR_TEXT_DIM, fontSmall_);

    // Box preview for cursor box (drawn last so it appears on top)
    drawBoxPreview(boxViewCursor_, cursorCellX, cursorCellY);
}

void UI::drawBoxPreview(int boxIdx, int anchorX, int anchorY) {
    int cols = gridCols();
    int rows = 5;

    // Preview panel dimensions
    int previewInnerW = cols * BV_MINI_CELL + (cols - 1) * BV_MINI_PAD;
    int previewInnerH = rows * BV_MINI_CELL + (rows - 1) * BV_MINI_PAD;
    int previewW = previewInnerW + 2 * BV_PREVIEW_PAD;
    int previewH = previewInnerH + 2 * BV_PREVIEW_PAD + BV_PREVIEW_HDR;

    // Position: prefer below the cell
    int prevX = anchorX;
    int prevY = anchorY + BV_CELL_H + 6;

    // Clamp to screen bounds
    if (prevX + previewW > SCREEN_W - 4)
        prevX = SCREEN_W - 4 - previewW;
    if (prevX < 4)
        prevX = 4;
    if (prevY + previewH > SCREEN_H - 4)
        prevY = anchorY - previewH - 6; // flip above
    if (prevY < 4)
        prevY = 4;

    // Background
    drawRect(prevX, prevY, previewW, previewH, {40, 40, 55, 230});
    drawRectOutline(prevX, prevY, previewW, previewH, COLOR_TEXT_DIM, 1);

    // Box name header
    std::string boxName;
    if (boxViewPanel_ == Panel::Game)
        boxName = (appletMode_) ? bankLeft_.getBoxName(boxIdx) : save_.getBoxName(boxIdx);
    else
        boxName = bank_.getBoxName(boxIdx);
    drawTextCentered(boxName, prevX + previewW / 2,
                     prevY + BV_PREVIEW_HDR / 2 + 2, COLOR_BOX_NAME, fontSmall_);

    // Mini sprite grid
    int gridX = prevX + BV_PREVIEW_PAD;
    int gridY = prevY + BV_PREVIEW_HDR;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int slot = r * cols + c;
            int sx = gridX + c * (BV_MINI_CELL + BV_MINI_PAD);
            int sy = gridY + r * (BV_MINI_CELL + BV_MINI_PAD);

            Pokemon pkm;
            if (boxViewPanel_ == Panel::Game)
                pkm = (appletMode_) ? bankLeft_.getSlot(boxIdx, slot)
                                    : save_.getBoxSlot(boxIdx, slot);
            else
                pkm = bank_.getSlot(boxIdx, slot);

            if (pkm.isEmpty()) {
                drawRect(sx, sy, BV_MINI_CELL, BV_MINI_CELL, {55, 55, 70, 180});
            } else {
                drawRect(sx, sy, BV_MINI_CELL, BV_MINI_CELL, {55, 70, 90, 200});

                SDL_Texture* sprite = getSprite(pkm.isEgg() ? 0 : pkm.species());
                if (sprite) {
                    int texW, texH;
                    SDL_QueryTexture(sprite, nullptr, nullptr, &texW, &texH);
                    float scale = std::min(float(BV_MINI_SPRITE) / texW,
                                           float(BV_MINI_SPRITE) / texH);
                    int dstW = static_cast<int>(texW * scale);
                    int dstH = static_cast<int>(texH * scale);
                    SDL_Rect dst = {
                        sx + (BV_MINI_CELL - dstW) / 2,
                        sy + (BV_MINI_CELL - dstH) / 2,
                        dstW, dstH
                    };
                    SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
                }
            }
        }
    }
}

void UI::drawHeldOverlay() {
    if (!holding_)
        return;

    // Determine which Pokemon sprite to show
    const Pokemon& pkm = heldMulti_.empty() ? heldPkm_ : heldMulti_[0];
    uint16_t species = pkm.species();
    if (species == 0)
        return;

    SDL_Texture* sprite = getSprite(pkm.isEgg() ? 0 : species);
    if (!sprite)
        return;

    // Compute cursor cell screen position (same formula as drawPanel)
    int panelX = (cursor_.panel == Panel::Game) ? PANEL_X_L : PANEL_X_R;
    int cols = gridCols();
    int gridStartX = panelX + (PANEL_W - (cols * (CELL_W + CELL_PAD) - CELL_PAD)) / 2;
    int gridStartY = GRID_Y;
    int cellX = gridStartX + cursor_.col * (CELL_W + CELL_PAD);
    int cellY = gridStartY + cursor_.row * (CELL_H + CELL_PAD);

    // Offset to create "dragging" effect
    constexpr int DRAG_OFS = 8;
    int baseX = cellX + DRAG_OFS;
    int baseY = cellY + DRAG_OFS;

    // Scale sprite to fit SPRITE_SIZE
    int texW, texH;
    SDL_QueryTexture(sprite, nullptr, nullptr, &texW, &texH);
    int dstW = SPRITE_SIZE, dstH = SPRITE_SIZE;
    if (texW > 0 && texH > 0) {
        float scale = std::min(static_cast<float>(SPRITE_SIZE) / texW,
                               static_cast<float>(SPRITE_SIZE) / texH);
        dstW = static_cast<int>(texW * scale);
        dstH = static_cast<int>(texH * scale);
    }

    int sprX = baseX + (CELL_W - dstW) / 2;
    int sprY = baseY + 4 + (SPRITE_SIZE - dstH) / 2;

    // Draw semi-transparent
    SDL_SetTextureAlphaMod(sprite, 180);
    SDL_Rect dst = {sprX, sprY, dstW, dstH};
    SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
    SDL_SetTextureAlphaMod(sprite, 255);

    // Multi-hold: draw count badge
    if (!heldMulti_.empty() && heldMulti_.size() > 1) {
        std::string num = std::to_string(heldMulti_.size());
        int tw = 0, th = 0;
        TTF_SizeUTF8(font_, num.c_str(), &tw, &th);
        int badgeR = std::max(tw, th) / 2 + 6;
        int cx = cellX + CELL_W / 2;
        int cy = cellY + CELL_H / 2;

        SDL_SetRenderDrawColor(renderer_, COLOR_SELECTED.r, COLOR_SELECTED.g, COLOR_SELECTED.b, 220);
        for (int dy = -badgeR; dy <= badgeR; dy++) {
            int dx = static_cast<int>(std::sqrt(badgeR * badgeR - dy * dy));
            SDL_RenderDrawLine(renderer_, cx - dx, cy + dy, cx + dx, cy + dy);
        }
        drawTextCentered(num, cx, cy, {0, 0, 0, 255}, font_);
    }
}

// --- Joystick ---

void UI::updateStick(int16_t axisX, int16_t axisY) {
    int newDirX = 0, newDirY = 0;
    if (axisX < -STICK_DEADZONE) newDirX = -1;
    else if (axisX > STICK_DEADZONE) newDirX = 1;
    if (axisY < -STICK_DEADZONE) newDirY = -1;
    else if (axisY > STICK_DEADZONE) newDirY = 1;

    if (newDirX != stickDirX_ || newDirY != stickDirY_) {
        stickDirX_ = newDirX;
        stickDirY_ = newDirY;
        stickMoved_ = false;
        stickMoveTime_ = 0;
    }
}

// --- Input ---

void UI::handleBoxViewInput(const SDL_Event& event) {
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
            case SDL_CONTROLLER_BUTTON_B: // Switch A = confirm
                closeBoxView(true);
                break;
            case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
                closeBoxView(false);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:    moveBoxViewCursor(0, -1); break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:   moveBoxViewCursor(0, +1); break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:   moveBoxViewCursor(-1, 0); break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:  moveBoxViewCursor(+1, 0); break;
        }
    }
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_a:
            case SDLK_RETURN: closeBoxView(true);  break;
            case SDLK_b:
            case SDLK_ESCAPE: closeBoxView(false); break;
            case SDLK_UP:     moveBoxViewCursor(0, -1); break;
            case SDLK_DOWN:   moveBoxViewCursor(0, +1); break;
            case SDLK_LEFT:   moveBoxViewCursor(-1, 0); break;
            case SDLK_RIGHT:  moveBoxViewCursor(+1, 0); break;
        }
    }
}

void UI::handleInput(bool& running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
            return;
        }

        // While menu is open, handle menu input only
        if (showMenu_) {
            int menuCount = appletMode_ ? 5 : 4;
            auto menuConfirm = [&]() {
                if (appletMode_) {
                    // 0=Switch Left Bank, 1=Switch Right Bank, 2=Change Game,
                    // 3=Save Banks, 4=Quit
                    if (menuSelection_ == 0) {
                        // Need at least 1 bank available that isn't loaded on the right
                        int avail = 0;
                        for (auto& b : bankManager_.list())
                            if (b.name != activeBankName_) avail++;
                        if (avail == 0) {
                            showMenu_ = false;
                            showMessageAndWait("No Banks Available",
                                "Create another bank first.");
                            return;
                        }
                        showWorking("Saving banks...");
                        if (!leftBankPath_.empty())
                            bankLeft_.save(leftBankPath_);
                        if (!activeBankPath_.empty())
                            bank_.save(activeBankPath_);
                        bankManager_.refresh();
                        bankSelTarget_ = Panel::Game;
                        screen_ = AppScreen::BankSelector;
                        showMenu_ = false;
                    } else if (menuSelection_ == 1) {
                        // Need at least 1 bank available that isn't loaded on the left
                        int avail = 0;
                        for (auto& b : bankManager_.list())
                            if (b.name != leftBankName_) avail++;
                        if (avail == 0) {
                            showMenu_ = false;
                            showMessageAndWait("No Banks Available",
                                "Create another bank first.");
                            return;
                        }
                        showWorking("Saving banks...");
                        if (!leftBankPath_.empty())
                            bankLeft_.save(leftBankPath_);
                        if (!activeBankPath_.empty())
                            bank_.save(activeBankPath_);
                        bankManager_.refresh();
                        bankSelTarget_ = Panel::Bank;
                        screen_ = AppScreen::BankSelector;
                        showMenu_ = false;
                    } else if (menuSelection_ == 2) {
                        // Change Game — save banks and return to game selector
                        showWorking("Saving banks...");
                        if (!leftBankPath_.empty())
                            bankLeft_.save(leftBankPath_);
                        if (!activeBankPath_.empty())
                            bank_.save(activeBankPath_);
                        leftBankName_.clear();
                        leftBankPath_.clear();
                        activeBankName_.clear();
                        activeBankPath_.clear();
                        screen_ = AppScreen::GameSelector;
                        showMenu_ = false;
                    } else if (menuSelection_ == 3) {
                        showWorking("Saving banks...");
                        if (!leftBankPath_.empty())
                            bankLeft_.save(leftBankPath_);
                        if (!activeBankPath_.empty())
                            bank_.save(activeBankPath_);
                        showMenu_ = false;
                    } else {
                        running = false;
                    }
                } else {
                    // 0=Switch Bank, 1=Change Game, 2=Save & Quit, 3=Quit Without Saving
                    if (menuSelection_ == 0) {
                        showWorking("Saving...");
                        if (!activeBankPath_.empty())
                            bank_.save(activeBankPath_);
                        if (save_.isLoaded())
                            save_.save(savePath_);
                        account_.commitSave();
                        bankManager_.refresh();
                        screen_ = AppScreen::BankSelector;
                        showMenu_ = false;
                    } else if (menuSelection_ == 1) {
                        // Change Game — save everything, unmount, go to game selector
                        showWorking("Saving...");
                        if (!activeBankPath_.empty())
                            bank_.save(activeBankPath_);
                        if (save_.isLoaded())
                            save_.save(savePath_);
                        account_.commitSave();
                        account_.unmountSave();
                        activeBankName_.clear();
                        activeBankPath_.clear();
                        screen_ = AppScreen::GameSelector;
                        showMenu_ = false;
                    } else if (menuSelection_ == 2) {
                        saveNow_ = true;
                        running = false;
                    } else {
                        running = false;
                    }
                }
            };
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
                        menuSelection_ = (menuSelection_ + menuCount - 1) % menuCount;
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        menuSelection_ = (menuSelection_ + 1) % menuCount;
                        break;
                    case SDL_CONTROLLER_BUTTON_B: // Switch A = confirm
                        menuConfirm();
                        break;
                    case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
                        showMenu_ = false;
                        break;
                }
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP:
                        menuSelection_ = (menuSelection_ + menuCount - 1) % menuCount;
                        break;
                    case SDLK_DOWN:
                        menuSelection_ = (menuSelection_ + 1) % menuCount;
                        break;
                    case SDLK_a:
                    case SDLK_RETURN:
                        menuConfirm();
                        break;
                    case SDLK_b:
                    case SDLK_ESCAPE:
                        showMenu_ = false;
                        break;
                }
            }
            continue;
        }

        // While box view is open, handle its input only
        if (showBoxView_) {
            handleBoxViewInput(event);
            continue;
        }

        // While detail popup is open, only allow closing it
        if (showDetail_) {
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_A: // Switch B
                    case SDL_CONTROLLER_BUTTON_Y: // Switch X
                        showDetail_ = false;
                        break;
                }
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_b:
                    case SDLK_ESCAPE:
                    case SDLK_x:
                        showDetail_ = false;
                        break;
                }
            }
            continue;
        }

        if (event.type == SDL_CONTROLLERAXISMOTION) {
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
                int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
                updateStick(lx, ly);
            }
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
                bool pressed = event.caxis.value > 16000;
                if (pressed && !zlPressed_ &&
                    !(appletMode_ && leftBankName_.empty()))
                    openBoxView(Panel::Game);
                zlPressed_ = pressed;
            }
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
                bool pressed = event.caxis.value > 16000;
                if (pressed && !zrPressed_)
                    openBoxView(Panel::Bank);
                zrPressed_ = pressed;
            }
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_B: // Switch A (right) = SDL B
                    if (!yHeld_) actionSelect();
                    break;
                case SDL_CONTROLLER_BUTTON_A: // Switch B (bottom) = SDL A
                    if (!yHeld_) actionCancel();
                    break;
                case SDL_CONTROLLER_BUTTON_Y: // Switch X (top) = SDL Y
                {
                    if (!yHeld_) {
                        Pokemon pkm = getPokemonAt(cursor_.box, cursor_.slot(gridCols()), cursor_.panel);
                        if (!pkm.isEmpty())
                            showDetail_ = true;
                    }
                    break;
                }
                case SDL_CONTROLLER_BUTTON_X: // Switch Y (left) = SDL X
                    beginYPress();
                    break;
                case SDL_CONTROLLER_BUTTON_START: // + (open menu)
                    if (!yHeld_) {
                        showMenu_ = true;
                        menuSelection_ = 0;
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_BACK: // - (about)
                    if (!yHeld_) showAbout_ = true;
                    break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                    if (!yHeld_) switchBox(-1);
                    break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                    if (!yHeld_) switchBox(+1);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    moveCursor(0, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    moveCursor(0, +1);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    moveCursor(-1, 0);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    moveCursor(+1, 0);
                    break;
            }
        }

        if (event.type == SDL_CONTROLLERBUTTONUP) {
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_X: // Switch Y released
                    endYPress();
                    break;
            }
        }

        // Keyboard for PC testing
        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_UP:     moveCursor(0, -1); break;
                case SDLK_DOWN:   moveCursor(0, +1); break;
                case SDLK_LEFT:   moveCursor(-1, 0); break;
                case SDLK_RIGHT:  moveCursor(+1, 0); break;
                case SDLK_a:
                case SDLK_RETURN: if (!yHeld_) actionSelect(); break;
                case SDLK_b:
                case SDLK_ESCAPE: if (!yHeld_) actionCancel(); break;
                case SDLK_x:
                {
                    if (!yHeld_) {
                        Pokemon pkm = getPokemonAt(cursor_.box, cursor_.slot(gridCols()), cursor_.panel);
                        if (!pkm.isEmpty())
                            showDetail_ = true;
                    }
                    break;
                }
                case SDLK_y:      beginYPress(); break;
                case SDLK_t:      if (!yHeld_) selectAll(); break;
                case SDLK_q:      if (!yHeld_) switchBox(-1); break;
                case SDLK_e:      if (!yHeld_) switchBox(+1); break;
                case SDLK_PLUS:
                    if (!yHeld_) {
                        showMenu_ = true;
                        menuSelection_ = 0;
                    }
                    break;
                case SDLK_MINUS:
                    if (!yHeld_) showAbout_ = true;
                    break;
                case SDLK_z: if (!yHeld_ && !(appletMode_ && leftBankName_.empty())) openBoxView(Panel::Game); break;
                case SDLK_c: if (!yHeld_) openBoxView(Panel::Bank); break;
            }
        }

        if (event.type == SDL_KEYUP) {
            switch (event.key.keysym.sym) {
                case SDLK_y: endYPress(); break;
            }
        }
    }

    // Joystick repeat navigation
    if (stickDirX_ != 0 || stickDirY_ != 0) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay) {
            if (showBoxView_) {
                if (stickDirX_ != 0) moveBoxViewCursor(stickDirX_, 0);
                if (stickDirY_ != 0) moveBoxViewCursor(0, stickDirY_);
            } else if (showMenu_) {
                if (stickDirY_ != 0)
                {
                    int mc = appletMode_ ? 5 : 4;
                    menuSelection_ = (menuSelection_ + (stickDirY_ > 0 ? 1 : mc - 1)) % mc;
                }
            } else if (!showDetail_) {
                if (stickDirX_ != 0) moveCursor(stickDirX_, 0);
                if (stickDirY_ != 0) moveCursor(0, stickDirY_);
            }
            stickMoveTime_ = now;
            stickMoved_ = true;
        }
    }
}

void UI::moveCursor(int dx, int dy) {
    Panel prevPanel = cursor_.panel;
    int cols = gridCols();
    int maxCol = cols - 1;
    cursor_.col += dx;
    cursor_.row += dy;

    if (yHeld_) {
        // During drag: clamp to same panel, no wrapping
        if (cursor_.col < 0) cursor_.col = 0;
        if (cursor_.col > maxCol) cursor_.col = maxCol;
        if (cursor_.row < 0) cursor_.row = 0;
        if (cursor_.row > 4) cursor_.row = 4;

        if (cursor_.col != dragAnchorCol_ || cursor_.row != dragAnchorRow_)
            yDragActive_ = true;
        updateDragSelection();
        return;
    }

    // Wrap row
    if (cursor_.row < 0) cursor_.row = 4;
    if (cursor_.row > 4) cursor_.row = 0;

    // Horizontal: crossing panel boundary
    if (cursor_.col < 0) {
        if (cursor_.panel == Panel::Bank &&
            !(appletMode_ && leftBankName_.empty())) {
            cursor_.panel = Panel::Game;
            cursor_.col = maxCol;
            cursor_.box = gameBox_;
        } else {
            cursor_.col = 0;
        }
    }
    if (cursor_.col > maxCol) {
        if (cursor_.panel == Panel::Game) {
            cursor_.panel = Panel::Bank;
            cursor_.col = 0;
            cursor_.box = bankBox_;
        } else {
            cursor_.col = maxCol;
        }
    }

    if (cursor_.panel != prevPanel)
        clearSelection();
}

void UI::switchBox(int direction) {
    if (yHeld_)
        return;
    clearSelection();
    int maxBox;
    if (cursor_.panel == Panel::Game) {
        if (appletMode_)
            maxBox = leftBankName_.empty() ? 1 : bankLeft_.boxCount();
        else
            maxBox = save_.boxCount();
    } else {
        maxBox = bank_.boxCount();
    }
    cursor_.box += direction;
    if (cursor_.box < 0) cursor_.box = maxBox - 1;
    if (cursor_.box >= maxBox) cursor_.box = 0;

    if (cursor_.panel == Panel::Game)
        gameBox_ = cursor_.box;
    else
        bankBox_ = cursor_.box;
}

Pokemon UI::getPokemonAt(int box, int slot, Panel panel) const {
    if (panel == Panel::Game) {
        if (appletMode_)
            return bankLeft_.getSlot(box, slot);
        return save_.getBoxSlot(box, slot);
    }
    return bank_.getSlot(box, slot);
}

void UI::setPokemonAt(int box, int slot, Panel panel, const Pokemon& pkm) {
    if (panel == Panel::Game) {
        if (appletMode_)
            bankLeft_.setSlot(box, slot, pkm);
        else
            save_.setBoxSlot(box, slot, pkm);
    } else {
        bank_.setSlot(box, slot, pkm);
    }
}

void UI::clearPokemonAt(int box, int slot, Panel panel) {
    if (panel == Panel::Game) {
        if (appletMode_)
            bankLeft_.clearSlot(box, slot);
        else
            save_.clearBoxSlot(box, slot);
    } else {
        bank_.clearSlot(box, slot);
    }
}

void UI::actionSelect() {
    // Block interaction on empty left panel in applet mode
    if (appletMode_ && cursor_.panel == Panel::Game && leftBankName_.empty())
        return;

    int box = cursor_.box;
    int slot = cursor_.slot(gridCols());

    // Multi-select pick up
    if (!holding_ && !selectedSlots_.empty()) {
        heldMulti_.clear();
        heldMultiSlots_.clear();
        heldMultiSource_ = selectedPanel_;
        heldMultiBox_ = selectedBox_;

        // Collect in selection order
        for (int s : selectedSlots_) {
            Pokemon pkm = getPokemonAt(selectedBox_, s, selectedPanel_);
            if (!pkm.isEmpty()) {
                heldMulti_.push_back(pkm);
                heldMultiSlots_.push_back(s);
                clearPokemonAt(selectedBox_, s, selectedPanel_);
            }
        }
        selectedSlots_.clear();
        if (!heldMulti_.empty())
            holding_ = true;
        return;
    }

    // Multi-select place
    if (holding_ && !heldMulti_.empty()) {
        if (positionPreserve_) {
            // Position-preserving: place each Pokemon at its original slot index
            for (int i = 0; i < (int)heldMulti_.size(); i++) {
                int targetSlot = heldMultiSlots_[i];
                if (!getPokemonAt(box, targetSlot, cursor_.panel).isEmpty()) {
                    showMessageAndWait("Slots occupied",
                        "Target box has Pokemon in the way. Needs matching slots empty.");
                    return;
                }
            }
            for (int i = 0; i < (int)heldMulti_.size(); i++) {
                setPokemonAt(box, heldMultiSlots_[i], cursor_.panel, heldMulti_[i]);
            }
        } else {
            // First-available: fill empty slots in order
            int slotsInBox = maxSlots();
            int emptyCount = 0;
            for (int s = 0; s < slotsInBox; s++) {
                if (getPokemonAt(box, s, cursor_.panel).isEmpty())
                    emptyCount++;
            }
            if (emptyCount < (int)heldMulti_.size()) {
                showMessageAndWait("Not enough space",
                    "Need " + std::to_string(heldMulti_.size()) + " empty slots, only " +
                    std::to_string(emptyCount) + " available.");
                return;
            }
            int placed = 0;
            for (int s = 0; s < slotsInBox && placed < (int)heldMulti_.size(); s++) {
                if (getPokemonAt(box, s, cursor_.panel).isEmpty()) {
                    setPokemonAt(box, s, cursor_.panel, heldMulti_[placed]);
                    placed++;
                }
            }
        }
        heldMulti_.clear();
        heldMultiSlots_.clear();
        holding_ = false;
        positionPreserve_ = false;
        return;
    }

    // Single pick/place
    if (!holding_) {
        Pokemon pkm = getPokemonAt(box, slot, cursor_.panel);
        if (pkm.isEmpty())
            return;

        heldPkm_ = pkm;
        holding_ = true;
        swapHistory_.clear();
        swapHistory_.push_back({pkm, cursor_.panel, box, slot});

        clearPokemonAt(box, slot, cursor_.panel);
    } else {
        Pokemon target = getPokemonAt(box, slot, cursor_.panel);

        if (target.isEmpty()) {
            // Place on empty — commit, clear history
            setPokemonAt(box, slot, cursor_.panel, heldPkm_);
            holding_ = false;
            heldPkm_ = Pokemon{};
            swapHistory_.clear();
        } else {
            // Swap: record what was in the target slot, then swap
            swapHistory_.push_back({target, cursor_.panel, box, slot});
            setPokemonAt(box, slot, cursor_.panel, heldPkm_);
            heldPkm_ = target;
        }
    }
}

void UI::actionCancel() {
    // Multi-hold cancel: return all to original positions
    if (holding_ && !heldMulti_.empty()) {
        for (int i = 0; i < (int)heldMulti_.size(); i++)
            setPokemonAt(heldMultiBox_, heldMultiSlots_[i], heldMultiSource_, heldMulti_[i]);
        heldMulti_.clear();
        heldMultiSlots_.clear();
        holding_ = false;
        positionPreserve_ = false;
        return;
    }

    // Clear selection (when not holding)
    if (!holding_ && !selectedSlots_.empty()) {
        selectedSlots_.clear();
        positionPreserve_ = false;
        return;
    }

    // Single hold cancel: replay swap history in reverse to restore all slots
    if (!holding_)
        return;

    for (int i = (int)swapHistory_.size() - 1; i >= 0; i--) {
        auto& rec = swapHistory_[i];
        setPokemonAt(rec.box, rec.slot, rec.panel, rec.pkm);
    }
    swapHistory_.clear();
    holding_ = false;
    heldPkm_ = Pokemon{};
}

void UI::toggleSelect() {
    if (holding_)
        return; // can't multi-select while holding
    if (appletMode_ && cursor_.panel == Panel::Game && leftBankName_.empty())
        return;

    int slot = cursor_.slot(gridCols());
    Pokemon pkm = getPokemonAt(cursor_.box, slot, cursor_.panel);
    if (pkm.isEmpty())
        return;

    // If selecting in a different panel/box, start fresh
    if (!selectedSlots_.empty() &&
        (cursor_.panel != selectedPanel_ || cursor_.box != selectedBox_)) {
        selectedSlots_.clear();
    }

    selectedPanel_ = cursor_.panel;
    selectedBox_ = cursor_.box;
    positionPreserve_ = false; // individual toggle = first-available mode

    auto it = std::find(selectedSlots_.begin(), selectedSlots_.end(), slot);
    if (it != selectedSlots_.end())
        selectedSlots_.erase(it);
    else
        selectedSlots_.push_back(slot);
}

void UI::clearSelection() {
    selectedSlots_.clear();
    if (!holding_)
        positionPreserve_ = false;
    yHeld_ = false;
    yDragActive_ = false;
}

void UI::beginYPress() {
    if (holding_)
        return;
    if (appletMode_ && cursor_.panel == Panel::Game && leftBankName_.empty())
        return;

    yHeld_ = true;
    yDragActive_ = false;
    dragAnchorCol_ = cursor_.col;
    dragAnchorRow_ = cursor_.row;
    dragPanel_ = cursor_.panel;
    dragBox_ = cursor_.box;
}

void UI::endYPress() {
    if (!yHeld_)
        return;

    if (!yDragActive_) {
        // No movement while held — check for double-tap
        uint32_t now = SDL_GetTicks();
        if (now - lastYTapTime_ <= DOUBLE_TAP_MS) {
            // Double-tap Y = select all (position-preserving)
            yHeld_ = false;
            lastYTapTime_ = 0;
            selectAll();
        } else {
            // Single tap — toggle individual slot
            yHeld_ = false;
            lastYTapTime_ = now;
            toggleSelect();
        }
    } else {
        yHeld_ = false;
        yDragActive_ = false;
    }
}

void UI::updateDragSelection() {
    int cols = gridCols();
    int minCol = std::min(dragAnchorCol_, cursor_.col);
    int maxCol = std::max(dragAnchorCol_, cursor_.col);
    int minRow = std::min(dragAnchorRow_, cursor_.row);
    int maxRow = std::max(dragAnchorRow_, cursor_.row);

    selectedSlots_.clear();
    selectedPanel_ = dragPanel_;
    selectedBox_ = dragBox_;
    positionPreserve_ = false; // drag = first-available mode

    // Add slots left-to-right, top-to-bottom (only non-empty)
    for (int r = minRow; r <= maxRow; r++) {
        for (int c = minCol; c <= maxCol; c++) {
            int slot = r * cols + c;
            Pokemon pkm = getPokemonAt(dragBox_, slot, dragPanel_);
            if (!pkm.isEmpty())
                selectedSlots_.push_back(slot);
        }
    }
}

void UI::selectAll() {
    if (holding_)
        return;
    if (appletMode_ && cursor_.panel == Panel::Game && leftBankName_.empty())
        return;

    int slots = maxSlots();

    selectedSlots_.clear();
    selectedPanel_ = cursor_.panel;
    selectedBox_ = cursor_.box;
    positionPreserve_ = true;

    for (int s = 0; s < slots; s++) {
        Pokemon pkm = getPokemonAt(cursor_.box, s, cursor_.panel);
        if (!pkm.isEmpty())
            selectedSlots_.push_back(s);
    }
}

void UI::openBoxView(Panel panel) {
    if (showDetail_ || showMenu_ || holding_ || yHeld_)
        return;
    showBoxView_ = true;
    boxViewPanel_ = panel;
    boxViewCursor_ = (panel == Panel::Game) ? gameBox_ : bankBox_;
}

void UI::closeBoxView(bool navigate) {
    showBoxView_ = false;
    if (navigate) {
        if (boxViewPanel_ == Panel::Game) {
            gameBox_ = boxViewCursor_;
            if (cursor_.panel == Panel::Game)
                cursor_.box = boxViewCursor_;
        } else {
            bankBox_ = boxViewCursor_;
            if (cursor_.panel == Panel::Bank)
                cursor_.box = boxViewCursor_;
        }
    }
}

void UI::moveBoxViewCursor(int dx, int dy) {
    int totalBoxes;
    if (boxViewPanel_ == Panel::Game) {
        totalBoxes = (appletMode_) ? bankLeft_.boxCount() : save_.boxCount();
    } else {
        totalBoxes = bank_.boxCount();
    }
    int col = boxViewCursor_ % BV_COLS;
    int row = boxViewCursor_ / BV_COLS;
    int maxRow = (totalBoxes - 1) / BV_COLS;

    col += dx;
    row += dy;

    if (col < 0) col = BV_COLS - 1;
    if (col >= BV_COLS) col = 0;
    if (row < 0) row = maxRow;
    if (row > maxRow) row = 0;

    int newIdx = row * BV_COLS + col;
    if (newIdx >= totalBoxes)
        newIdx = (dx > 0 || dy > 0) ? 0 : totalBoxes - 1;

    boxViewCursor_ = newIdx;
}
