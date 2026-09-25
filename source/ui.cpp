#include "ui.h"
#include "ui_util.h"
#include "led.h"
#include "i18n.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <switch.h>

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
    // NOTE: PlSharedFontType_Standard covers Latin, Cyrillic, and Japanese glyphs.
    // Korean (PlSharedFontType_KO) and Chinese (PlSharedFontType_ChineseSimplified /
    // PlSharedFontType_ChineseTraditional) require loading separate system fonts.
    // SDL_ttf does not support font fallback chains, so supporting these languages
    // would require switching the primary font based on the active language.
    //
    // The console's own font is the only one: pkHouse ships none. Without it
    // nothing can be drawn, so init fails and main explains why on the text
    // console, which needs no font file.
    //
    // pl:u, not pl:s: since system 16.0.0 the shared font is only handed out
    // through pl:u (see libnx pl.h). The old pl:s call failed silently on
    // current firmware, and the bundled copy of this font covered for it.
    PlFontData fontData{};
    if (R_SUCCEEDED(plInitialize(PlServiceType_User)) &&
        R_SUCCEEDED(plGetSharedFontByType(&fontData, PlSharedFontType_Standard)) &&
        fontData.address && fontData.size) {
        fontData_     = fontData.address;
        fontDataSize_ = fontData.size;
        font_      = TTF_OpenFontRW(SDL_RWFromMem(fontData.address, fontData.size), 1, 18);
        fontSmall_ = TTF_OpenFontRW(SDL_RWFromMem(fontData.address, fontData.size), 1, 14);
        fontLarge_ = TTF_OpenFontRW(SDL_RWFromMem(fontData.address, fontData.size), 1, 28);
    }
    if (!font_ || !fontSmall_ || !fontLarge_) {
        initError_ = "The console's system font could not be loaded.";
        shutdown();
        return false;
    }

    // Load status icons
    {
        const char* iconDir = "romfs:/icons/";
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
        iconDynamax_    = loadIcon("dynamax.png");
        iconHouse_       = loadIcon("house.png");
        iconGlobe_       = loadIcon("globe.png");
        iconBank_        = loadIcon("bank.png");
        iconCheck_       = loadIcon("check.png");
        iconTrash_       = loadIcon("trash.png");
        iconWarn_        = loadIcon("warn.png");
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
    clearTextCache();
    freeGameIcons();
    account_.freeTextures();
    freeSprites();
    freeShapeCache();
    if (backdrop_) { SDL_DestroyTexture(backdrop_); backdrop_ = nullptr; }
    freeCardPreview();
    closeUiFonts();
    if (fontLarge_) TTF_CloseFont(fontLarge_);
    if (fontSmall_) TTF_CloseFont(fontSmall_);
    if (font_) TTF_CloseFont(font_);
    if (pad_) SDL_GameControllerClose(pad_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    plExit();
}

void UI::showSplash() {
    if (!renderer_) return;

    const char* splashPath = "romfs:/splash.png";

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

void UI::showWorking(const std::string& msg, bool writing, float progress) {
    if (!renderer_) return;
    markDirty(); // Force redraw after the operation returns

    // What was on screen, dimmed (cached across the updates of one operation).
    drawDialogBackdrop();

    // The dialog: a spinner badge and the message, then the bar and the
    // warning when they apply.
    constexpr int W = 520, PAD = 24, BADGE = 48;
    TTF_Font* fTitle = uiFont(20, true);
    TTF_Font* fNote  = uiFont(13, true);
    const int textX = PAD + BADGE + 16, textW = W - textX - PAD;
    const auto lines = wrapText(msg, fTitle, textW, 2);
    const int titleH = static_cast<int>(lines.size()) * 26;
    int h = PAD + std::max(BADGE, titleH) + PAD;
    if (progress >= 0.0f) h += 30;
    const auto noteLines = writing ? wrapText(i18n::get(StrKey::WorkNoClose), fNote, W - 2 * PAD - 44, 2)
                                   : std::vector<std::string>();
    const int noteH = noteLines.empty() ? 0 : 18 + static_cast<int>(noteLines.size()) * 18;
    if (noteH) h += noteH + 16;

    const int x = (SCREEN_W - W) / 2, y = (SCREEN_H - h) / 2;
    fillRounded(x + 3, y + 6, W, h, 18, SDL_Color{0, 0, 0, 90});
    fillRounded(x, y, W, h, 18, T().panelBg);
    strokeRounded(x, y, W, h, 18, 1, T().panelBorder);

    // Spinner: a faint ring with one bright quarter.
    {
        const int cx = x + PAD + BADGE / 2, cy = y + PAD + BADGE / 2;
        fillDisc(cx, cy, BADGE / 2, T().badgeBg);
        constexpr int R = 12, STROKE = 3;
        strokeRounded(cx - R, cy - R, R * 2, R * 2, R, STROKE,
                      SDL_Color{T().accent.r, T().accent.g, T().accent.b, 70});
        if (SDL_Texture* q = cornerTexture(R, STROKE)) {
            SDL_SetTextureColorMod(q, T().accent.r, T().accent.g, T().accent.b);
            SDL_SetTextureAlphaMod(q, 255);
            SDL_Rect dst = {cx, cy - R, R, R};
            SDL_RenderCopyEx(renderer_, q, nullptr, &dst, 0, nullptr, SDL_FLIP_HORIZONTAL);
        }
    }

    int ty = y + PAD + std::max(0, (BADGE - titleH) / 2) + 1;
    for (const auto& l : lines) { drawText(l, x + textX, ty, T().text, fTitle); ty += 26; }
    int by = y + PAD + std::max(BADGE, titleH) + PAD;

    if (progress >= 0.0f) {
        const float p = std::clamp(progress, 0.0f, 1.0f);
        TTF_Font* f = uiFont(14, true);
        const std::string pct = std::to_string(static_cast<int>(p * 100.0f + 0.5f)) + "%";
        const int barW = W - 2 * PAD - 56;
        fillRounded(x + PAD, by, barW, 10, 5, T().bg);
        fillRounded(x + PAD, by, std::max(10, static_cast<int>(barW * p)), 10, 5, T().accent);
        drawText(pct, x + W - PAD - textWidth(pct, f), by + 5 - TTF_FontHeight(f) / 2, T().text, f);
        by += 30;
    }

    if (noteH) {
        const SDL_Color w = T().statusWarn;
        fillRounded(x + PAD, by, W - 2 * PAD, noteH, 10, SDL_Color{w.r, w.g, w.b, 40});
        fillDisc(x + PAD + 16, by + noteH / 2, 4, T().accent);
        for (size_t i = 0; i < noteLines.size(); i++)
            drawText(noteLines[i], x + PAD + 30, by + 9 + static_cast<int>(i) * 18, T().accent, fNote);
    }

    SDL_RenderPresent(renderer_);
}

void UI::drawCurrentScreen() {
    switch (screen_) {
        case AppScreen::ProfileSelector: drawProfileSelectorFrame(); break;
        case AppScreen::GameSelector:    drawGameSelectorFrame();    break;
        case AppScreen::BankSelector:    drawBankSelectorFrame();    break;
        case AppScreen::GtsHub:          drawGtsHubFrame();          break;
        case AppScreen::GtsBrowse:       drawGtsBrowseFrame();       break;
        case AppScreen::MainView:        drawFrame();                break;
    }
}

void UI::run(const std::string& basePath, const std::string& savePath) {
    basePath_ = basePath;
    savePath_ = savePath;

    // Load persisted theme
    themeIndex_ = loadThemeIndex(basePath_);
    theme_ = &getTheme(themeIndex_);

    // All games in menu order

    if (appletMode_) {
        // Applet mode: skip profile, bank-only access
        screen_ = AppScreen::GameSelector;
        availableGames_.assign(std::begin(ALL_GAMES), std::end(ALL_GAMES));
        refreshBankCounts();
        loadGameIcons();   // shows its own progress
    } else {
        showWorking(i18n::get(StrKey::LoadingProfiles));
        if (account_.init() && account_.loadProfiles(renderer_)) {
            loadProfileSaveCounts();
            screen_ = AppScreen::ProfileSelector;
        } else {
            screen_ = AppScreen::GameSelector;
            availableGames_.assign(std::begin(ALL_GAMES), std::end(ALL_GAMES));
            refreshBankCounts();
            loadGameIcons();   // shows its own progress
        }
    }

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
                        { showAbout_ = false; markDirty(); }
                }
            }
            if (!showAbout_) continue; // dismissed — let main draw section handle it
            if (dirty_) {
                if (theme_ != lastTheme_) { clearTextCache(); lastTheme_ = theme_; }
                // Draw the underlying screen, then about popup on top
                drawCurrentScreen();
                drawAboutPopup();
                SDL_RenderPresent(renderer_);
                dirty_ = false;
            }
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
                    markDirty();
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
                    markDirty();
                }
            }
            if (!showThemeSelector_) continue; // dismissed — let main draw section handle it
            if (dirty_) {
                if (theme_ != lastTheme_) { clearTextCache(); lastTheme_ = theme_; }
                // Draw the underlying screen, then theme popup on top
                drawCurrentScreen();
                drawThemeSelectorPopup();
                SDL_RenderPresent(renderer_);
                dirty_ = false;
            }
            SDL_Delay(16);
            continue;
        }

        // Language selector intercepts input from any screen
        if (showLanguageSelector_) {
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
                    markDirty();
                    int langCount = (int)langList_.size();
                    switch (event.cbutton.button) {
                        case SDL_CONTROLLER_BUTTON_DPAD_UP:
                            langSelCursor_ = (langSelCursor_ + langCount - 1) % langCount;
                            break;
                        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                            langSelCursor_ = (langSelCursor_ + 1) % langCount;
                            break;
                        case SDL_CONTROLLER_BUTTON_B: { // Switch A = confirm
                            std::string newLang = langList_[langSelCursor_];
                            i18n::init(newLang);
                            clearTextCache();
                            // Persist choice
                            std::string path = basePath_ + "language.txt";
                            FILE* f = std::fopen(path.c_str(), "w");
                            if (f) { std::fputs(newLang.c_str(), f); std::fclose(f); }
                            showLanguageSelector_ = false;
                            showMenu_ = false;
                            break;
                        }
                        case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
                        case SDL_CONTROLLER_BUTTON_X: // Switch Y = cancel
                            showLanguageSelector_ = false;
                            break;
                    }
                }
            }
            // Joystick repeat
            if (stickDirY_ != 0 && !langList_.empty()) {
                uint32_t now = SDL_GetTicks();
                uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
                if (now - stickMoveTime_ >= delay) {
                    int langCount = (int)langList_.size();
                    langSelCursor_ = (langSelCursor_ + (stickDirY_ > 0 ? 1 : langCount - 1)) % langCount;
                    stickMoveTime_ = now;
                    stickMoved_ = true;
                    markDirty();
                }
            }
            if (!showLanguageSelector_) continue;
            if (dirty_) {
                if (theme_ != lastTheme_) { clearTextCache(); lastTheme_ = theme_; }
                drawCurrentScreen();
                drawLanguageSelectorPopup();
                SDL_RenderPresent(renderer_);
                dirty_ = false;
            }
            SDL_Delay(16);
            continue;
        }

        AppScreen screenBefore = screen_;
        if (screen_ == AppScreen::ProfileSelector) {
            handleProfileSelectorInput(running);
        } else if (screen_ == AppScreen::GameSelector) {
            handleGameSelectorInput(running);
        } else if (screen_ == AppScreen::BankSelector) {
            handleBankSelectorInput(running);
        } else if (screen_ == AppScreen::GtsHub) {
            handleGtsHubInput(running);
        } else if (screen_ == AppScreen::GtsBrowse) {
            handleGtsBrowseInput(running);
        } else {
            handleInput(running);
            if (saveNow_) {
                if (!saveBankFiles()) {
                    saveNow_ = false;
                    running = true;
                } else {
                    if (!isDualBankMode()) {
                        showWorking(i18n::get(StrKey::Saving), true);
                        ledBlink();
                        if (save_.isLoaded())
                            save_.save(savePath_);
                        account_.commitSave();
                        ledOff();
                    }
                    saveNow_ = false;
                }
            }
        }
        // Screen transition always triggers redraw
        if (screen_ != screenBefore) {
            markDirty();
            // A fresh visit to the bank picker starts without a stale preview.
            if (screen_ == AppScreen::BankSelector)
                clearBankPreview();
        }
        // The bank picker previews the highlighted bank once the cursor rests.
        if (screen_ == AppScreen::BankSelector && updateBankPreview())
            markDirty();
        // The card browser reads the highlighted card once the cursor settles.
        // This has to tick from the loop: markDirty() called from inside a draw
        // would be cleared again by the dirty_ = false below.
        if (showCardList_) {
            int previewBefore = cardPreviewIdx_;
            updateCardPreview();
            if (cardPreviewIdx_ != previewBefore)
                markDirty();
        }
        if (showGtsDeposit_) {
            int previewBefore = gtsCardPreviewIdx_;
            updateGtsCardPreview();
            if (gtsCardPreviewIdx_ != previewBefore)
                markDirty();
        }
        // If a popup just activated, skip drawing here — the popup branch
        // will handle it next iteration with dirty_ still set.
        if (dirty_ && !showAbout_ && !showThemeSelector_ && !showLanguageSelector_) {
            if (theme_ != lastTheme_) {
                clearTextCache();
                lastTheme_ = theme_;
            }
            drawCurrentScreen();
            SDL_RenderPresent(renderer_);
            dirty_ = false;
            screenDrawn_ = true;
            frameGen_++;   // what dialogs dim has changed
        }
        SDL_Delay(16);
    }

    account_.unmountSave();
    account_.shutdown();
    Gts::shutdown();
}

void UI::selectGame(GameType game) {
    selectedGame_ = game;
    invalidateAllSlotDisplays();
    availableSpecies_.clear(); // rebuild on next species picker open
    save_.setGameType(game);
    bankLeft_.setGameType(game);

    if (isDualBankMode()) {
        // Reset left bank state for new game
        leftBankName_.clear();
        leftBankPath_.clear();
        bankSelTarget_ = Panel::Bank;
    }

    lastBackup_ = BackupOutcome::None;
    lastBackupDir_.clear();

    if (!isDualBankMode()) {
        const bool withSave = selectedProfile_ >= 0;
        const std::string backupDir = withSave ? buildBackupDir(game) : std::string();
        if (withSave)
            drawBackupProgress(game, backupDir, 0, 0.0f);
        else
            showWorking(i18n::get(StrKey::LoadingSaveData));

        if (withSave) {
            std::string mountPath = account_.mountSave(selectedProfile_, game);
            if (mountPath.empty()) {
                showMessageAndWait(i18n::get(StrKey::MountError), i18n::get(StrKey::FailedMountSave));
                return;
            }
            savePath_ = mountPath + saveFileNameOf(game);

            // Check space and backup save files
            drawBackupProgress(game, backupDir, 1, 0.0f);
            size_t saveSize = AccountManager::calculateDirSize(mountPath);
            bool doBackup = true;

            struct statvfs vfs;
            if (statvfs("sdmc:/", &vfs) == 0) {
                size_t freeSpace = (size_t)vfs.f_bavail * vfs.f_bsize;
                if (freeSpace < saveSize * 2) {
                    std::string msg = i18n::fmt(StrKey::LowStorageBody, formatSize(freeSpace), formatSize(saveSize));
                    ConfirmStyle st;
                    st.warning = true;
                    st.confirmKey = StrKey::DlgContinue;
                    if (!showConfirmDialog(i18n::get(StrKey::LowStorage), msg, st)) {
                        account_.unmountSave();
                        return;
                    }
                    doBackup = false;
                    lastBackup_ = BackupOutcome::Skipped;
                }
            }

            if (doBackup) {
                // Redrawn at most every 50 ms: the copy is quick, and drawing
                // after every 64 KB would cost more than the copy itself.
                size_t written = 0;
                uint32_t lastDraw = 0;
                drawBackupProgress(game, backupDir, 2, 0.0f);
                ledBlink();
                bool ok = AccountManager::backupSaveDir(mountPath, backupDir,
                    [&](size_t n) {
                        written += n;
                        const uint32_t now = SDL_GetTicks();
                        if (now - lastDraw >= 50) {
                            lastDraw = now;
                            drawBackupProgress(game, backupDir, 2, saveSize ?
                                std::min(1.0f, static_cast<float>(written) / saveSize) : 1.0f);
                        }
                    });
                ledOff();
                lastBackup_ = ok ? BackupOutcome::Saved : BackupOutcome::Failed;
                lastBackupDir_ = backupDir;
                lastBackupWhen_ = time(nullptr);
                if (!ok) {
                    ConfirmStyle st;
                    st.warning = true;
                    st.confirmKey = StrKey::DlgContinue;
                    if (!showConfirmDialog(i18n::get(StrKey::BackupFailed),
                            i18n::get(StrKey::BackupFailedBody), st)) {
                        account_.unmountSave();
                        return;
                    }
                }
            }
        } else {
            savePath_ = basePath_ + "main";
        }

        if (withSave)
            drawBackupProgress(game, backupDir, 3, 1.0f);
        save_.load(savePath_);

        // Debug: verify encryption round-trip (encrypt(decrypt(file)) == file)
        if (!isBDSP(game) && !isLGPE(game) && !isFRLG(game)) {
            std::string rtResult = save_.verifyRoundTrip();
            if (rtResult != "OK")
                showMessageAndWait(i18n::get(StrKey::RoundTripCheck), rtResult);
        }
    }

    bankManager_.init(basePath_, game, bankListProgress());

    // cards/ is read from as well as written to, so it is created up front like
    // banks/ rather than on first export: someone handed a card needs a folder
    // already sitting there with the right name to drop it into.
    {
        std::string cardsParent = basePath_ + "cards/";
        mkdir(cardsParent.c_str(), 0755);
        mkdir((cardsParent + bankFolderNameOf(game) + "/").c_str(), 0755);
    }

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
    showWorking(i18n::get(StrKey::Saving), true);
    ledBlink();
    // Written now: the "Last edited" dates follow without asking the SD card.
    const time_t now = time(nullptr);
    if (isDualBankMode()) {
        if (!leftBankPath_.empty()) { bankLeft_.save(leftBankPath_); leftBankModified_ = now; }
        if (!activeBankPath_.empty()) { bank_.save(activeBankPath_); activeBankModified_ = now; }
    } else {
        if (!activeBankPath_.empty()) { bank_.save(activeBankPath_); activeBankModified_ = now; }
    }
    ledOff();
    // Every caller outside dual-bank mode writes the save right after this.
    unsavedChanges_ = false;
    return true;
}
