#include "ui.h"
#include "species_converter.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

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

    if (IMG_Init(IMG_INIT_PNG) == 0) {
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
#else
    font_ = TTF_OpenFont("romfs/fonts/default.ttf", 18);
    fontSmall_ = TTF_OpenFont("romfs/fonts/default.ttf", 14);
#endif

    if (!font_ || !fontSmall_) {
        if (!font_)
            font_ = TTF_OpenFont("romfs:/fonts/default.ttf", 18);
        if (!fontSmall_)
            fontSmall_ = TTF_OpenFont("romfs:/fonts/default.ttf", 14);
    }

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
    freeSprites();
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

bool UI::run(SaveFile& save, Bank& bank,
             const std::string& savePath, const std::string& bankPath) {
    bool running = true;
    bool shouldSave = true;
    while (running) {
        handleInput(save, bank, running, shouldSave);
        if (saveNow_) {
            if (save.isLoaded())
                save.save(savePath);
            bank.save(bankPath);
            saveNow_ = false;
        }
        drawFrame(save, bank);
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }
    return shouldSave;
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
    if (eggSprite_) {
        SDL_DestroyTexture(eggSprite_);
        eggSprite_ = nullptr;
    }
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

void UI::drawSlot(int x, int y, const Pokemon& pkm, bool isCursor) {
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

    // Cursor highlight
    if (isCursor)
        drawRectOutline(x, y, CELL_W, CELL_H, COLOR_CURSOR, 3);

    if (!pkm.isEmpty()) {
        uint16_t species = pkm.species();

        // Draw sprite centered in top portion of cell
        SDL_Texture* sprite = nullptr;
        if (pkm.isEgg()) {
            // Use species 0 sprite for egg, or fallback
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
}

void UI::drawPanel(int panelX, const std::string& boxName, int boxIdx,
                   int totalBoxes, bool isActive, SaveFile* save, Bank* bank, int box) {
    // Panel background
    drawRect(panelX, 0, PANEL_W, SCREEN_H - 35, COLOR_PANEL_BG);

    // Box name header with arrows
    SDL_Color hdrColor = isActive ? COLOR_BOX_NAME : COLOR_TEXT_DIM;
    drawTextCentered("<", panelX + 20, BOX_HDR_Y + BOX_HDR_H / 2, COLOR_ARROW, font_);
    drawTextCentered(boxName + " (" + std::to_string(boxIdx + 1) + "/" + std::to_string(totalBoxes) + ")",
                     panelX + PANEL_W / 2, BOX_HDR_Y + BOX_HDR_H / 2, hdrColor, font_);
    drawTextCentered(">", panelX + PANEL_W - 20, BOX_HDR_Y + BOX_HDR_H / 2, COLOR_ARROW, font_);

    // Grid: 6 columns x 5 rows
    int gridStartX = panelX + (PANEL_W - (6 * (CELL_W + CELL_PAD) - CELL_PAD)) / 2;
    int gridStartY = GRID_Y;

    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 6; col++) {
            int slot = row * 6 + col;
            int cellX = gridStartX + col * (CELL_W + CELL_PAD);
            int cellY = gridStartY + row * (CELL_H + CELL_PAD);

            Pokemon pkm;
            if (save)
                pkm = save->getBoxSlot(box, slot);
            else if (bank)
                pkm = bank->getSlot(box, slot);

            bool isCursor = isActive && cursor_.col == col && cursor_.row == row;
            drawSlot(cellX, cellY, pkm, isCursor);
        }
    }
}

void UI::drawFrame(SaveFile& save, Bank& bank) {
    // Clear screen
    SDL_SetRenderDrawColor(renderer_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer_);

    // Sync cursor box to the active panel
    if (cursor_.panel == Panel::Game)
        gameBox_ = cursor_.box;
    else
        bankBox_ = cursor_.box;

    // Left panel: Game boxes
    bool leftActive = (cursor_.panel == Panel::Game);
    std::string gameBoxName = save.getBoxName(gameBox_);
    drawPanel(PANEL_X_L, gameBoxName, gameBox_, SaveFile::BOX_COUNT,
              leftActive, &save, nullptr, gameBox_);

    // Right panel: Bank boxes
    bool rightActive = (cursor_.panel == Panel::Bank);
    std::string bankBoxName = bank.getBoxName(bankBox_);
    drawPanel(PANEL_X_R, bankBoxName, bankBox_, Bank::BOX_COUNT,
              rightActive, nullptr, &bank, bankBox_);

    // Status bar
    std::string statusMsg = "D-Pad:Move  L/R:Box  A:Pick/Place  B:Cancel  Y:Detail  +:Menu  -:Quit";
    if (holding_) {
        std::string heldName = heldPkm_.displayName();
        if (!heldName.empty()) {
            statusMsg = "Holding: " + heldName + " Lv." + std::to_string(heldPkm_.level()) +
                        "  |  A:Place  B:Return";
        }
    }
    drawStatusBar(statusMsg);

    // Detail popup overlay
    if (showDetail_) {
        Pokemon pkm = getPokemonAt(cursor_.box, cursor_.slot(), cursor_.panel, save, bank);
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

    // OT + TID
    std::string otStr = "OT: " + pkm.otName() + " | TID: " + std::to_string(pkm.tid());
    drawText(otStr, infoX, infoY, COLOR_TEXT_DIM, fontSmall_);
    infoY += 22;

    // Nature
    std::string natureStr = "Nature: " + NatureName::get(pkm.nature());
    drawText(natureStr, infoX, infoY, COLOR_TEXT_DIM, fontSmall_);
    infoY += 22;

    // Ability
    std::string abilityStr = "Ability: " + AbilityName::get(pkm.ability());
    drawText(abilityStr, infoX, infoY, COLOR_TEXT_DIM, fontSmall_);

    // --- Below sprite: Moves ---
    int movesX = popX + 30;
    int movesY = sprY + LARGE_SPRITE + 20;

    drawText("Moves", movesX, movesY, COLOR_TEXT, font_);
    movesY += 26;

    uint16_t moves[4] = {pkm.move1(), pkm.move2(), pkm.move3(), pkm.move4()};
    for (int i = 0; i < 4; i++) {
        if (moves[i] != 0) {
            std::string moveStr = "- " + MoveName::get(moves[i]);
            drawText(moveStr, movesX + 10, movesY, COLOR_TEXT_DIM, fontSmall_);
        } else {
            drawText("- ---", movesX + 10, movesY, COLOR_TEXT_DIM, fontSmall_);
        }
        movesY += 20;
    }

    // --- Right column: IVs and EVs ---
    int statsX = popX + POP_W / 2 + 20;
    int statsY = popY + 20;

    // IVs header
    drawText("IVs", statsX, statsY, COLOR_TEXT, font_);
    statsY += 26;

    const char* statLabels[] = {"HP", "Atk", "Def", "SpA", "SpD", "Spe"};
    int ivs[] = {pkm.ivHp(), pkm.ivAtk(), pkm.ivDef(), pkm.ivSpA(), pkm.ivSpD(), pkm.ivSpe()};

    for (int i = 0; i < 6; i++) {
        std::string line = std::string(statLabels[i]) + ": " + std::to_string(ivs[i]);
        SDL_Color ivColor = (ivs[i] == 31) ? COLOR_SHINY : COLOR_TEXT_DIM;
        drawText(line, statsX + 10, statsY, ivColor, fontSmall_);
        statsY += 20;
    }

    statsY += 10;

    // EVs header
    drawText("EVs", statsX, statsY, COLOR_TEXT, font_);
    statsY += 26;

    int evs[] = {pkm.evHp(), pkm.evAtk(), pkm.evDef(), pkm.evSpA(), pkm.evSpD(), pkm.evSpe()};

    for (int i = 0; i < 6; i++) {
        std::string line = std::string(statLabels[i]) + ": " + std::to_string(evs[i]);
        SDL_Color evColor = (evs[i] > 0) ? COLOR_TEXT : COLOR_TEXT_DIM;
        drawText(line, statsX + 10, statsY, evColor, fontSmall_);
        statsY += 20;
    }

    // Close hint at bottom
    drawTextCentered("B / Y: Close", popX + POP_W / 2, popY + POP_H - 20, COLOR_TEXT_DIM, fontSmall_);
}

void UI::drawMenuPopup() {
    // Semi-transparent dark overlay
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 160});

    // Popup rect centered
    constexpr int POP_W = 350;
    constexpr int POP_H = 220;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, COLOR_PANEL_BG);
    drawRectOutline(popX, popY, POP_W, POP_H, COLOR_CURSOR, 2);

    // Title
    drawTextCentered("Menu", popX + POP_W / 2, popY + 22, COLOR_TEXT, font_);

    // Menu options
    const char* labels[] = {"Save", "Save & Quit", "Quit Without Saving"};
    int rowH = 36;
    int startY = popY + 50;

    for (int i = 0; i < 3; i++) {
        int rowY = startY + i * rowH;
        if (i == menuSelection_) {
            drawRect(popX + 20, rowY, POP_W - 40, rowH - 4, {60, 60, 80, 255});
            drawRectOutline(popX + 20, rowY, POP_W - 40, rowH - 4, COLOR_CURSOR, 2);
        }
        drawTextCentered(labels[i], popX + POP_W / 2, rowY + (rowH - 4) / 2, COLOR_TEXT, font_);
    }

    // Hint at bottom
    drawTextCentered("A:Confirm  B:Cancel", popX + POP_W / 2, popY + POP_H - 18, COLOR_TEXT_DIM, fontSmall_);
}

// --- Input ---

void UI::handleInput(SaveFile& save, Bank& bank, bool& running, bool& shouldSave) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
            shouldSave = false;
            return;
        }

        // While menu is open, handle menu input only
        if (showMenu_) {
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        menuSelection_ = (menuSelection_ + 2) % 3;
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        menuSelection_ = (menuSelection_ + 1) % 3;
                        break;
                    case SDL_CONTROLLER_BUTTON_B: // Switch A = confirm
                        if (menuSelection_ == 0) {
                            saveNow_ = true;
                            showMenu_ = false;
                        } else if (menuSelection_ == 1) {
                            shouldSave = true;
                            running = false;
                        } else {
                            shouldSave = false;
                            running = false;
                        }
                        break;
                    case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
                        showMenu_ = false;
                        break;
                }
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP:
                        menuSelection_ = (menuSelection_ + 2) % 3;
                        break;
                    case SDLK_DOWN:
                        menuSelection_ = (menuSelection_ + 1) % 3;
                        break;
                    case SDLK_a:
                    case SDLK_RETURN:
                        if (menuSelection_ == 0) {
                            saveNow_ = true;
                            showMenu_ = false;
                        } else if (menuSelection_ == 1) {
                            shouldSave = true;
                            running = false;
                        } else {
                            shouldSave = false;
                            running = false;
                        }
                        break;
                    case SDLK_b:
                    case SDLK_ESCAPE:
                        showMenu_ = false;
                        break;
                }
            }
            continue;
        }

        // While detail popup is open, only allow closing it
        if (showDetail_) {
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_A: // Switch B
                    case SDL_CONTROLLER_BUTTON_X: // Switch Y
                        showDetail_ = false;
                        break;
                }
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_b:
                    case SDLK_ESCAPE:
                    case SDLK_y:
                        showDetail_ = false;
                        break;
                }
            }
            continue;
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_B: // Switch A (right) = SDL B
                    actionSelect(save, bank);
                    break;
                case SDL_CONTROLLER_BUTTON_A: // Switch B (bottom) = SDL A
                    actionCancel(save, bank);
                    break;
                case SDL_CONTROLLER_BUTTON_X: // Switch Y (left) = SDL X
                {
                    Pokemon pkm = getPokemonAt(cursor_.box, cursor_.slot(), cursor_.panel, save, bank);
                    if (!pkm.isEmpty())
                        showDetail_ = true;
                    break;
                }
                case SDL_CONTROLLER_BUTTON_START: // + (open menu)
                    showMenu_ = true;
                    menuSelection_ = 0;
                    break;
                case SDL_CONTROLLER_BUTTON_BACK: // - (quit without saving)
                    running = false;
                    shouldSave = false;
                    break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                    switchBox(-1);
                    break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                    switchBox(+1);
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

        // Keyboard for PC testing
        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_UP:     moveCursor(0, -1); break;
                case SDLK_DOWN:   moveCursor(0, +1); break;
                case SDLK_LEFT:   moveCursor(-1, 0); break;
                case SDLK_RIGHT:  moveCursor(+1, 0); break;
                case SDLK_a:
                case SDLK_RETURN: actionSelect(save, bank); break;
                case SDLK_b:
                case SDLK_ESCAPE: actionCancel(save, bank); break;
                case SDLK_y:
                {
                    Pokemon pkm = getPokemonAt(cursor_.box, cursor_.slot(), cursor_.panel, save, bank);
                    if (!pkm.isEmpty())
                        showDetail_ = true;
                    break;
                }
                case SDLK_q:      switchBox(-1); break;
                case SDLK_e:      switchBox(+1); break;
                case SDLK_PLUS:
                    showMenu_ = true;
                    menuSelection_ = 0;
                    break;
                case SDLK_MINUS:
                    running = false;
                    shouldSave = false;
                    break;
            }
        }
    }
}

void UI::moveCursor(int dx, int dy) {
    cursor_.col += dx;
    cursor_.row += dy;

    // Wrap row
    if (cursor_.row < 0) cursor_.row = 4;
    if (cursor_.row > 4) cursor_.row = 0;

    // Horizontal: crossing panel boundary
    if (cursor_.col < 0) {
        if (cursor_.panel == Panel::Bank) {
            cursor_.panel = Panel::Game;
            cursor_.col = 5;
            cursor_.box = gameBox_;
        } else {
            cursor_.col = 0;
        }
    }
    if (cursor_.col > 5) {
        if (cursor_.panel == Panel::Game) {
            cursor_.panel = Panel::Bank;
            cursor_.col = 0;
            cursor_.box = bankBox_;
        } else {
            cursor_.col = 5;
        }
    }
}

void UI::switchBox(int direction) {
    int maxBox = (cursor_.panel == Panel::Game) ? SaveFile::BOX_COUNT : Bank::BOX_COUNT;
    cursor_.box += direction;
    if (cursor_.box < 0) cursor_.box = maxBox - 1;
    if (cursor_.box >= maxBox) cursor_.box = 0;

    if (cursor_.panel == Panel::Game)
        gameBox_ = cursor_.box;
    else
        bankBox_ = cursor_.box;
}

Pokemon UI::getPokemonAt(int box, int slot, Panel panel, SaveFile& save, Bank& bank) const {
    if (panel == Panel::Game)
        return save.getBoxSlot(box, slot);
    else
        return bank.getSlot(box, slot);
}

void UI::setPokemonAt(int box, int slot, Panel panel, const Pokemon& pkm, SaveFile& save, Bank& bank) {
    if (panel == Panel::Game)
        save.setBoxSlot(box, slot, pkm);
    else
        bank.setSlot(box, slot, pkm);
}

void UI::clearPokemonAt(int box, int slot, Panel panel, SaveFile& save, Bank& bank) {
    if (panel == Panel::Game)
        save.clearBoxSlot(box, slot);
    else
        bank.clearSlot(box, slot);
}

void UI::actionSelect(SaveFile& save, Bank& bank) {
    int box = cursor_.box;
    int slot = cursor_.slot();

    if (!holding_) {
        Pokemon pkm = getPokemonAt(box, slot, cursor_.panel, save, bank);
        if (pkm.isEmpty())
            return;

        heldPkm_ = pkm;
        heldSource_ = cursor_.panel;
        heldBox_ = box;
        heldSlot_ = slot;
        holding_ = true;

        clearPokemonAt(box, slot, cursor_.panel, save, bank);
    } else {
        Pokemon target = getPokemonAt(box, slot, cursor_.panel, save, bank);

        if (target.isEmpty()) {
            setPokemonAt(box, slot, cursor_.panel, heldPkm_, save, bank);
            holding_ = false;
            heldPkm_ = Pokemon{};
        } else {
            setPokemonAt(box, slot, cursor_.panel, heldPkm_, save, bank);
            heldPkm_ = target;
            heldSource_ = cursor_.panel;
            heldBox_ = box;
            heldSlot_ = slot;
        }
    }
}

void UI::actionCancel(SaveFile& save, Bank& bank) {
    if (!holding_)
        return;

    setPokemonAt(heldBox_, heldSlot_, heldSource_, heldPkm_, save, bank);
    holding_ = false;
    heldPkm_ = Pokemon{};
}
