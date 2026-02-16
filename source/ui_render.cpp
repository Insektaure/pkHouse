#include "ui.h"
#include "species_converter.h"
#include <cmath>
#include <cstdio>

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
