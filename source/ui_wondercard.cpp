#include "ui.h"
#include "species_converter.h"
#include <algorithm>
#include <cstring>

// --- Wondercard Screen ---

void UI::drawWondercardScreen() {
    // Full-screen overlay
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    const auto& cards = wcManager_.list();

    // Layout constants
    constexpr int POP_W = 1200;
    constexpr int POP_H = 650;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);

    drawTextCentered("Inject Wondercard", popX + POP_W / 2, popY + 22, T().text, font_);

    if (cards.empty()) {
        drawTextCentered("No wondercard files found.", popX + POP_W / 2, popY + POP_H / 2, T().textDim, font_);

        // Show hint about folder location
        std::string hint = "Place files in: wondercards/" +
                           std::string(bankFolderNameOf(selectedGame_)) + "/";
        drawTextCentered(hint, popX + POP_W / 2, popY + POP_H / 2 + 30, T().textDim, fontSmall_);

        drawTextCentered("B:Back", popX + POP_W / 2, popY + POP_H - 20, T().textDim, fontSmall_);
        return;
    }

    // --- Left panel: scrollable list ---
    constexpr int LIST_X = 20;
    constexpr int LIST_W = 450;
    constexpr int LIST_Y = 50;
    constexpr int ROW_H = 36;
    int listStartX = popX + LIST_X;
    int listStartY = popY + LIST_Y;
    int visibleRows = (POP_H - LIST_Y - 40) / ROW_H;

    // Ensure scroll keeps cursor visible
    if (wcCursor_ < wcScroll_)
        wcScroll_ = wcCursor_;
    if (wcCursor_ >= wcScroll_ + visibleRows)
        wcScroll_ = wcCursor_ - visibleRows + 1;

    int count = static_cast<int>(cards.size());

    for (int i = 0; i < visibleRows && (wcScroll_ + i) < count; i++) {
        int idx = wcScroll_ + i;
        int rowY = listStartY + i * ROW_H;
        const auto& card = cards[idx];

        // Highlight selected row
        if (idx == wcCursor_) {
            drawRect(listStartX, rowY, LIST_W, ROW_H - 2, T().menuHighlight);
            drawRectOutline(listStartX, rowY, LIST_W, ROW_H - 2, T().cursor, 2);
        }

        // Filename
        drawText(card.filename, listStartX + 10, rowY + 8, T().text, font_);
    }

    // Scroll indicators
    if (wcScroll_ > 0) {
        drawTextCentered("^", listStartX + LIST_W / 2, listStartY - 8, T().textDim, fontSmall_);
    }
    if (wcScroll_ + visibleRows < count) {
        drawTextCentered("v", listStartX + LIST_W / 2, listStartY + visibleRows * ROW_H + 4, T().textDim, fontSmall_);
    }

    // --- Right panel: preview ---
    constexpr int PREV_X = 500;
    int prevStartX = popX + PREV_X;
    int prevStartY = popY + LIST_Y;

    // Load the full wondercard for preview (cached)
    const auto& selCard = cards[wcCursor_];
    if (wcPreviewIdx_ != wcCursor_) {
        wcPreviewValid_ = WondercardManager::loadCard(selCard.fullPath, wcPreview_);
        wcPreviewIdx_ = wcCursor_;
    }
    const Wondercard& wc = wcPreview_;
    bool loaded = wcPreviewValid_;

    // Large sprite
    constexpr int LARGE_SPRITE = 96;
    SDL_Texture* largeSprite = getSprite(selCard.speciesNational);
    if (largeSprite) {
        int texW, texH;
        SDL_QueryTexture(largeSprite, nullptr, nullptr, &texW, &texH);
        float scale = std::min(static_cast<float>(LARGE_SPRITE) / texW,
                               static_cast<float>(LARGE_SPRITE) / texH);
        int dstW = static_cast<int>(texW * scale);
        int dstH = static_cast<int>(texH * scale);
        int dx = prevStartX + (LARGE_SPRITE - dstW) / 2;
        int dy = prevStartY + (LARGE_SPRITE - dstH) / 2;
        SDL_Rect dst = {dx, dy, dstW, dstH};
        SDL_RenderCopy(renderer_, largeSprite, nullptr, &dst);
    }

    // Shiny indicator
    if (selCard.shinyType == WCShinyType::AlwaysStar ||
        selCard.shinyType == WCShinyType::AlwaysSquare) {
        if (iconShiny_) {
            SDL_Rect iconDst = {prevStartX + LARGE_SPRITE + 4, prevStartY, 20, 20};
            SDL_RenderCopy(renderer_, iconShiny_, nullptr, &iconDst);
        }
    }

    // Species name + level
    int infoX = prevStartX + LARGE_SPRITE + 30;
    int infoY = prevStartY + 4;
    std::string specName = SpeciesName::get(selCard.speciesNational);
    SDL_Color snColor = (selCard.shinyType == WCShinyType::AlwaysStar ||
                         selCard.shinyType == WCShinyType::AlwaysSquare)
                        ? T().shiny : T().text;
    drawText(specName, infoX, infoY, snColor, font_);

    std::string lvlStr = selCard.level > 0
        ? "  Lv." + std::to_string(selCard.level)
        : "  Lv.Random";
    int nameW = 0;
    TTF_SizeUTF8(font_, specName.c_str(), &nameW, nullptr);
    drawText(lvlStr, infoX + nameW, infoY, T().text, font_);

    infoY += 28;

    // Nature
    std::string natureStr = "Nature: " +
        (selCard.nature == 0xFF ? std::string("Random") : NatureName::get(selCard.nature));
    drawText(natureStr, infoX, infoY, T().textDim, font_);
    infoY += 24;

    // Shiny type
    const char* shinyLabel = "None";
    switch (selCard.shinyType) {
        case WCShinyType::Never:       shinyLabel = "Never"; break;
        case WCShinyType::Random:      shinyLabel = "Random"; break;
        case WCShinyType::AlwaysStar:  shinyLabel = "Star"; break;
        case WCShinyType::AlwaysSquare: shinyLabel = "Square"; break;
        case WCShinyType::FixedValue:  shinyLabel = "Fixed"; break;
    }
    drawText(std::string("Shiny: ") + shinyLabel, infoX, infoY, T().textDim, font_);
    infoY += 24;

    // Format
    drawText("Format: " + selCard.formatLabel, infoX, infoY, T().textDim, font_);

    if (loaded) {
        // Moves (below the sprite area)
        int detailY = prevStartY + LARGE_SPRITE + 20;
        drawText("Moves", prevStartX, detailY, T().text, font_);
        detailY += 26;
        for (int i = 0; i < 4; i++) {
            uint16_t m = wc.move(i);
            std::string moveStr = m != 0 ? "- " + MoveName::get(m) : "- ---";
            drawText(moveStr, prevStartX + 10, detailY, T().textDim, font_);
            detailY += 22;
        }

        detailY += 8;

        // IVs
        drawText("IVs", prevStartX, detailY, T().text, font_);
        detailY += 24;
        static const char* statNames[6] = {"HP", "Atk", "Def", "Spe", "SpA", "SpD"};
        std::string ivLine;
        for (int i = 0; i < 6; i++) {
            uint8_t iv = wc.iv(i);
            if (i > 0) ivLine += "  ";
            ivLine += statNames[i];
            ivLine += ":";
            if (iv <= 31) {
                ivLine += std::to_string(iv);
            } else if (iv == 0xFC) {
                ivLine += "1*";
            } else if (iv == 0xFD) {
                ivLine += "2*";
            } else if (iv == 0xFE) {
                ivLine += "3*";
            } else {
                ivLine += "?";
            }
        }
        drawText(ivLine, prevStartX + 10, detailY, T().textDim, fontSmall_);
        detailY += 22;

        // EVs (if any are nonzero)
        bool hasEVs = false;
        for (int i = 0; i < 6; i++) {
            if (wc.ev(i) != 0) { hasEVs = true; break; }
        }
        if (hasEVs) {
            detailY += 4;
            drawText("EVs", prevStartX, detailY, T().text, font_);
            detailY += 24;
            std::string evLine;
            for (int i = 0; i < 6; i++) {
                if (i > 0) evLine += "  ";
                evLine += statNames[i];
                evLine += ":";
                evLine += std::to_string(wc.ev(i));
            }
            drawText(evLine, prevStartX + 10, detailY, T().textDim, fontSmall_);
            detailY += 22;
        }

        // Gender
        uint8_t g = wc.gender();
        std::string gStr = "Gender: ";
        if (g == 0) gStr += "Male";
        else if (g == 1) gStr += "Female";
        else if (g == 2) gStr += "Genderless";
        else gStr += "Random";
        detailY += 4;
        drawText(gStr, prevStartX, detailY, T().textDim, font_);

        // Held item
        uint16_t item = wc.heldItem();
        if (item != 0) {
            detailY += 24;
            drawText("Held Item: #" + std::to_string(item), prevStartX, detailY, T().textDim, font_);
        }
    }

    // Bottom hint
    drawTextCentered("A:Inject  B:Back", popX + POP_W / 2, popY + POP_H - 20, T().textDim, fontSmall_);
}

void UI::handleWondercardInput(const SDL_Event& event) {
    const auto& cards = wcManager_.list();
    int count = static_cast<int>(cards.size());

    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                if (count > 0)
                    wcCursor_ = (wcCursor_ + count - 1) % count;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                if (count > 0)
                    wcCursor_ = (wcCursor_ + 1) % count;
                break;
            case SDL_CONTROLLER_BUTTON_B: // Switch A = confirm
                if (count > 0)
                    injectWondercard(wcCursor_);
                break;
            case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
                showWondercardScreen_ = false;
                break;
        }
    }

    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
            int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
            updateStick(lx, ly);
        }
    }

    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_UP:
                if (count > 0)
                    wcCursor_ = (wcCursor_ + count - 1) % count;
                break;
            case SDLK_DOWN:
                if (count > 0)
                    wcCursor_ = (wcCursor_ + 1) % count;
                break;
            case SDLK_a:
            case SDLK_RETURN:
                if (count > 0)
                    injectWondercard(wcCursor_);
                break;
            case SDLK_b:
            case SDLK_ESCAPE:
                showWondercardScreen_ = false;
                break;
        }
    }
}

void UI::injectWondercard(int index) {
    const auto& cards = wcManager_.list();
    if (index < 0 || index >= static_cast<int>(cards.size()))
        return;

    // Load the full wondercard
    Wondercard wc;
    if (!WondercardManager::loadCard(cards[index].fullPath, wc)) {
        showMessageAndWait("Error", "Failed to load wondercard file.");
        return;
    }

    // Block player-OT cards in bank-only mode (no save file)
    if (!wc.hasFixedOT() && appletMode_) {
        showMessageAndWait("Error",
            "This card uses player OT.\nOpen a save file to inject.");
        return;
    }

    // Inject at current cursor position (bank panel only, must be empty)
    int targetBox = (cursor_.panel == Panel::Bank) ? bankBox_ : gameBox_;
    int targetSlot = cursor_.slot(gridCols());

    // Check the slot is on the bank side
    if (cursor_.panel != Panel::Bank) {
        showMessageAndWait("Error", "Move cursor to a bank slot to inject.");
        return;
    }

    Pokemon existing = bank_.getSlot(targetBox, targetSlot);
    if (!existing.isEmpty()) {
        showMessageAndWait("Slot Occupied", "Select an empty bank slot to inject.");
        return;
    }

    // Build player info (game version always, OT/TID/SID for player-OT cards)
    WCPlayerInfo playerInfo;
    playerInfo.game = selectedGame_;
    if (!wc.hasFixedOT() && save_.isLoaded()) {
        auto ti = save_.getTrainerInfo();
        if (ti.valid) {
            playerInfo.tid16 = ti.tid16;
            playerInfo.sid16 = ti.sid16;
            playerInfo.gender = ti.gender;
            std::memcpy(playerInfo.otName, ti.otName, 0x1A);
        }
    }

    Pokemon pkm = wc.toPokemon(playerInfo);
    if (pkm.isEmpty()) {
        showMessageAndWait("Error", "Failed to convert wondercard to Pokemon.");
        return;
    }

    // Place the Pokemon
    bank_.setSlot(targetBox, targetSlot, pkm);

    // Show success and close wondercard screen
    std::string specName = SpeciesName::get(pkm.species());
    std::string msg = specName + " injected into Box " +
                      std::to_string(targetBox + 1) + " Slot " +
                      std::to_string(targetSlot + 1);
    showMessageAndWait("Injected", msg);
    showWondercardScreen_ = false;
}
