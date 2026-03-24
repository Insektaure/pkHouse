#include "ui.h"
#include "ui_util.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <sys/statvfs.h>

#include <switch.h>

// --- Bank Selector ---

void UI::drawBankSelectorFrame() {
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);

    // Show split view: bank selector on one side, other panel visible
    // - Normal mode: save on left, selector on right (when save is loaded)
    // - Applet/All-banks mode: other bank on opposite side (when a bank is loaded)
    bool splitView = isDualBankMode() ? !activeBankName_.empty() : save_.isLoaded();

    // Determine selector area
    int selCenterX = SCREEN_W / 2;
    int selAreaX = 0;
    if (splitView) {
        if (isDualBankMode() && bankSelTarget_ == Panel::Game) {
            // Dual mode: selector on left, keep right bank visible
            selCenterX = PANEL_X_L + PANEL_W / 2;
            selAreaX = PANEL_X_L;
            auto truncName = [](const std::string& s, size_t max) -> std::string {
                if (s.size() <= max) return s;
                std::string t = s.substr(0, max - 3);
                t += "(..)";
                return t;
            };
            std::string rightBoxName = truncName(activeBankName_, 16) + " - " + bank_.getBoxName(bankBox_);
            drawPanel(PANEL_X_R, rightBoxName, bankBox_, bank_.boxCount(),
                      false, nullptr, &bank_, bankBox_, Panel::Bank);
        } else {
            // Normal mode or dual switching right bank: selector on right
            selCenterX = PANEL_X_R + PANEL_W / 2;
            selAreaX = PANEL_X_R;
            auto truncName = [](const std::string& s, size_t max) -> std::string {
                if (s.size() <= max) return s;
                std::string t = s.substr(0, max - 3);
                t += "(..)";
                return t;
            };
            // Draw left panel (save or left bank)
            if (isDualBankMode()) {
                if (!leftBankName_.empty()) {
                    std::string leftBoxName = truncName(leftBankName_, 16) + " - " + bankLeft_.getBoxName(gameBox_);
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
    if (allBanksMode_) {
        if (activeBankName_.empty())
            drawTextCentered("All Banks", selCenterX,
                             splitView ? 25 : 40, T().text, font_);
        else {
            const char* side = (bankSelTarget_ == Panel::Game) ? "Left" : "Right";
            drawTextCentered(std::string("Select ") + side + " Bank",
                             selCenterX, 25, T().text, font_);
        }
    } else if (appletMode_) {
        const char* side = (bankSelTarget_ == Panel::Game) ? "Left" : "Right";
        drawTextCentered(std::string("Select ") + side + " Bank",
                         selCenterX, 25, T().text, font_);
    } else {
        drawTextCentered("Select Bank", selCenterX,
                         splitView ? 25 : 40, T().text, font_);
    }

    const auto& banks = bankManager_.list();

    if (banks.empty()) {
        drawTextCentered("No banks found.",
                         selCenterX, SCREEN_H / 2 - 10, T().textDim, font_);
        drawTextCentered("Press X to create one.",
                         selCenterX, SCREEN_H / 2 + 15, T().textDim, fontSmall_);
    } else if (bankManager_.isAllMode()) {
        // All-banks mode: grouped list with game headers
        int LIST_W = splitView ? (PANEL_W - 30) : 800;
        int LIST_X = splitView ? (selAreaX + 15) : (SCREEN_W - LIST_W) / 2;
        int LIST_Y = splitView ? 50 : 80;
        int LIST_BOTTOM = 580;
        int ROW_H = 50;
        int HDR_H = 32;
        int visiblePixels = LIST_BOTTOM - LIST_Y;

        // Build visual row list: headers interleaved with bank entries
        struct VisualRow { bool isHeader; int bankIdx; GameType game; };
        std::vector<VisualRow> vrows;
        for (int i = 0; i < (int)banks.size(); i++) {
            if (i == 0 || banks[i].game != banks[i - 1].game)
                vrows.push_back({true, -1, banks[i].game});
            vrows.push_back({false, i, banks[i].game});
        }

        // Find visual row of cursor
        int cursorVisRow = 0;
        for (int r = 0; r < (int)vrows.size(); r++) {
            if (!vrows[r].isHeader && vrows[r].bankIdx == bankSelCursor_) {
                cursorVisRow = r;
                break;
            }
        }

        // Pre-compute pixel Y positions for all visual rows
        std::vector<int> rowYPositions(vrows.size() + 1);
        rowYPositions[0] = 0;
        for (int i = 0; i < (int)vrows.size(); i++)
            rowYPositions[i + 1] = rowYPositions[i] + (vrows[i].isHeader ? HDR_H : ROW_H);
        int totalPixels = rowYPositions.back();

        // Clamp scroll to keep cursor visible
        int cursorY = rowYPositions[cursorVisRow];
        int cursorH = ROW_H;
        if (bankSelScroll_ > cursorY)
            bankSelScroll_ = cursorY;
        if (cursorY + cursorH > bankSelScroll_ + visiblePixels)
            bankSelScroll_ = cursorY + cursorH - visiblePixels;

        // If cursor's group header is just above and close, scroll up to show it
        if (cursorVisRow > 0 && vrows[cursorVisRow - 1].isHeader) {
            int hdrY = rowYPositions[cursorVisRow - 1];
            if (hdrY >= bankSelScroll_ - HDR_H && hdrY < bankSelScroll_)
                bankSelScroll_ = hdrY;
        }

        if (bankSelScroll_ > totalPixels - visiblePixels)
            bankSelScroll_ = std::max(0, totalPixels - visiblePixels);
        if (bankSelScroll_ < 0) bankSelScroll_ = 0;

        // Scroll arrows
        if (bankSelScroll_ > 0)
            drawTextCentered("^", selCenterX, LIST_Y - 12, T().arrow, font_);
        if (bankSelScroll_ + visiblePixels < totalPixels)
            drawTextCentered("v", selCenterX, LIST_BOTTOM + 4, T().arrow, font_);

        // Render visible rows
        for (int r = 0; r < (int)vrows.size(); r++) {
            int rowY = rowYPositions[r] - bankSelScroll_ + LIST_Y;
            int rowH = vrows[r].isHeader ? HDR_H : ROW_H;

            if (rowY + rowH <= LIST_Y) continue;  // above visible area
            if (rowY >= LIST_BOTTOM) break;        // below visible area

            if (vrows[r].isHeader) {
                // Group header: game name with separator line
                std::string gameName = bankGroupNameOf(vrows[r].game);
                drawText(gameName, LIST_X + 10, rowY + HDR_H / 2 - 7,
                         T().text, fontSmall_);
                // Separator line after the text
                int textW = getTextEntry(gameName, fontSmall_, T().text).w;
                drawRect(LIST_X + 10 + textW + 10, rowY + HDR_H / 2,
                         LIST_W - 30 - textW, 1, T().text);
            } else {
                int idx = vrows[r].bankIdx;

                // Highlighted row
                if (idx == bankSelCursor_) {
                    drawRect(LIST_X, rowY, LIST_W, ROW_H - 4, T().menuHighlight);
                    drawRectOutline(LIST_X, rowY, LIST_W, ROW_H - 4, T().cursor, 2);
                }

                // Bank name (left-aligned, indented under header)
                drawText(banks[idx].name, LIST_X + 30, rowY + (ROW_H - 4) / 2 - 9,
                         T().text, font_);

                // Slot count (right-aligned)
                int maxSlots = isLGPE(banks[idx].game) ? 1000 :
                               isBDSP(banks[idx].game) ? 1200 : 960;
                std::string slotStr = std::to_string(banks[idx].occupiedSlots) +
                                      "/" + std::to_string(maxSlots);
                const auto& se = getTextEntry(slotStr, font_, T().textDim);
                if (se.tex)
                    drawText(slotStr, LIST_X + LIST_W - 20 - se.w,
                             rowY + (ROW_H - 4) / 2 - 9, T().textDim, font_);
            }
        }
    } else {
        // Normal mode: flat bank list
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
            drawTextCentered("^", selCenterX, LIST_Y - 12, T().arrow, font_);
        }
        if (bankSelScroll_ + visibleRows < (int)banks.size()) {
            drawTextCentered("v", selCenterX, LIST_BOTTOM + 4, T().arrow, font_);
        }

        for (int i = 0; i < visibleRows && (bankSelScroll_ + i) < (int)banks.size(); i++) {
            int idx = bankSelScroll_ + i;
            int rowY = LIST_Y + i * ROW_H;

            // Highlighted row
            if (idx == bankSelCursor_) {
                drawRect(LIST_X, rowY, LIST_W, ROW_H - 4, T().menuHighlight);
                drawRectOutline(LIST_X, rowY, LIST_W, ROW_H - 4, T().cursor, 2);
            }

            drawText(banks[idx].name, LIST_X + 20, rowY + (ROW_H - 4) / 2 - 9,
                     T().text, font_);

            // Slot count (right-aligned)
            int maxSlots = isLGPE(selectedGame_) ? 1000 : isBDSP(selectedGame_) ? 1200 : 960;
            std::string slotStr = std::to_string(banks[idx].occupiedSlots) + "/" + std::to_string(maxSlots);
            const auto& se = getTextEntry(slotStr, font_, T().textDim);
            if (se.tex) drawText(slotStr, LIST_X + LIST_W - 20 - se.w, rowY + (ROW_H - 4) / 2 - 9,
                     T().textDim, font_);
        }
    }

    // Status bar
    if (bankManager_.isAllMode())
        drawStatusBar("A: Open  Y: Theme  B: Back  -: About");
    else
        drawStatusBar("A: Open  X: New  Y: Rename  +: Delete  B: Back  -: About");

    // Profile | Game name (bottom right, gold)
    {
        std::string label;
        if (allBanksMode_)
            label = activeBankName_.empty() ? "All Banks" :
                    "All Banks | " + std::string(gameDisplayNameOf(selectedGame_));
        else if (appletMode_)
            label = "Dual Bank | " + std::string(gameDisplayNameOf(selectedGame_));
        else {
            if (selectedProfile_ >= 0 && selectedProfile_ < account_.profileCount())
                label = account_.profiles()[selectedProfile_].nickname + " | ";
            label += gameDisplayNameOf(selectedGame_);
        }
        const auto& e = getTextEntry(label, fontSmall_, T().goldLabel);
        if (e.tex) drawText(label, SCREEN_W - e.w - 15, SCREEN_H - 26, T().goldLabel, fontSmall_);
    }

    // Delete confirmation overlay
    if (showDeleteConfirm_) {
        drawDeleteConfirmPopup();
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
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    if (bankCount > 0) {
                        int prev = bankSelCursor_;
                        bankSelCursor_ = (bankSelCursor_ - 1 + bankCount) % bankCount;
                        if (bankManager_.isAllMode()) {
                            if (bankSelCursor_ > prev)
                                bankSelScroll_ = bankManager_.totalVisualRows() * 50; // wrap to bottom, draw clamps
                        } else {
                            if (bankSelCursor_ > prev) {
                                int visibleRows = (580 - 80) / 50;
                                bankSelScroll_ = std::max(0, bankSelCursor_ - visibleRows + 1);
                            } else if (bankSelCursor_ < bankSelScroll_) {
                                bankSelScroll_ = bankSelCursor_;
                            }
                        }
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    if (bankCount > 0) {
                        int prev = bankSelCursor_;
                        bankSelCursor_ = (bankSelCursor_ + 1) % bankCount;
                        if (bankManager_.isAllMode()) {
                            if (bankSelCursor_ < prev)
                                bankSelScroll_ = 0; // wrap to top
                        } else {
                            if (bankSelCursor_ < prev) {
                                bankSelScroll_ = 0;
                            } else {
                                int visibleRows = (580 - 80) / 50;
                                if (bankSelCursor_ >= bankSelScroll_ + visibleRows)
                                    bankSelScroll_ = bankSelCursor_ - visibleRows + 1;
                            }
                        }
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
                        if (isDualBankMode() && leftBankName_.empty()) {
                            cursor_ = Cursor{};
                            cursor_.panel = Panel::Bank;
                        }
                    } else {
                        if (!allBanksMode_) {
                            account_.unmountSave();
                            int cnt = BankManager::countBanks(basePath_, selectedGame_);
                            gameBankCounts_[selectedGame_] = cnt;
                            gameBankCounts_[pairedGame(selectedGame_)] = cnt;
                        }
                        allBanksMode_ = false;
                        screen_ = AppScreen::GameSelector;
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_Y: // Switch X = new
                    if (!bankManager_.isAllMode())
                        beginTextInput(TextInputPurpose::CreateBank);
                    break;
                case SDL_CONTROLLER_BUTTON_X: // Switch Y = rename / theme
                    if (bankManager_.isAllMode()) {
                        showThemeSelector_ = true;
                        themeSelCursor_ = themeIndex_;
                        themeSelOriginal_ = themeIndex_;
                    } else if (bankCount > 0) {
                        beginTextInput(TextInputPurpose::RenameBank);
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_BACK: // - = about
                    showAbout_ = true;
                    break;
                case SDL_CONTROLLER_BUTTON_START: // + = delete
                    if (bankCount > 0 && !bankManager_.isAllMode())
                        showDeleteConfirm_ = true;
                    break;
            }
        }

    }

    // Joystick repeat navigation
    if ((stickDirY_ != 0) && bankCount > 0 && !showDeleteConfirm_) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay) {
            if (stickDirY_ < 0) {
                int prev = bankSelCursor_;
                bankSelCursor_ = (bankSelCursor_ - 1 + bankCount) % bankCount;
                if (bankManager_.isAllMode()) {
                    if (bankSelCursor_ > prev)
                        bankSelScroll_ = bankManager_.totalVisualRows() * 50;
                } else {
                    if (bankSelCursor_ > prev) {
                        int visibleRows = (580 - 80) / 50;
                        bankSelScroll_ = std::max(0, bankSelCursor_ - visibleRows + 1);
                    } else if (bankSelCursor_ < bankSelScroll_) {
                        bankSelScroll_ = bankSelCursor_;
                    }
                }
            } else {
                int prev = bankSelCursor_;
                bankSelCursor_ = (bankSelCursor_ + 1) % bankCount;
                if (bankManager_.isAllMode()) {
                    if (bankSelCursor_ < prev)
                        bankSelScroll_ = 0;
                } else {
                    if (bankSelCursor_ < prev) {
                        bankSelScroll_ = 0;
                    } else {
                        int visibleRows = (580 - 80) / 50;
                        if (bankSelCursor_ >= bankSelScroll_ + visibleRows)
                            bankSelScroll_ = bankSelCursor_ - visibleRows + 1;
                    }
                }
            }
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

    const std::string& name = banks[bankSelCursor_].name;

    // Prevent loading same bank on both sides in dual mode
    if (isDualBankMode()) {
        if (bankSelTarget_ == Panel::Game && name == activeBankName_
            && banks[bankSelCursor_].game == selectedGame_) {
            showMessageAndWait("Already Open",
                "This bank is already open on the right panel.");
            return;
        }
        if (bankSelTarget_ == Panel::Bank && name == leftBankName_
            && banks[bankSelCursor_].game == selectedGame_) {
            showMessageAndWait("Already Open",
                "This bank is already open on the left panel.");
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

    showWorking("Loading bank...");

    if (isDualBankMode() && bankSelTarget_ == Panel::Game) {
        leftBankPath_ = bankManager_.loadBank(name, bankLeft_);
        bankLeft_.setGameType(selectedGame_);
        leftBankName_ = name;
    } else {
        activeBankPath_ = bankManager_.loadBank(name, bank_);
        bank_.setGameType(selectedGame_);
        activeBankName_ = name;
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
            return;  // stay on BankSelector screen
        }
    }

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

    drawTextCentered("Delete \"" + bankName + "\"?",
                     popX + POP_W / 2, popY + 50, T().text, font_);
    drawTextCentered("This cannot be undone!",
                     popX + POP_W / 2, popY + 85, T().red, fontSmall_);
    drawTextCentered("A:Confirm  B:Cancel",
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
        swkbdConfigSetHeaderText(&kbd, "Enter bank name");
    else if (purpose == TextInputPurpose::RenameBank)
        swkbdConfigSetHeaderText(&kbd, "Rename bank");
    else if (purpose == TextInputPurpose::RenameBoxName) {
        swkbdConfigSetHeaderText(&kbd, "Rename box");
        swkbdConfigSetStringLenMax(&kbd, 16);
    } else if (purpose == TextInputPurpose::SearchSpecies)
        swkbdConfigSetHeaderText(&kbd, "Species name");
    else if (purpose == TextInputPurpose::SearchOT)
        swkbdConfigSetHeaderText(&kbd, "OT name");
    else if (purpose == TextInputPurpose::SearchLevelMin)
        swkbdConfigSetHeaderText(&kbd, "Min level");
    else if (purpose == TextInputPurpose::SearchLevelMax)
        swkbdConfigSetHeaderText(&kbd, "Max level");
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
                    showMessageAndWait("Not Enough Space",
                        "Free: " + formatSize(freeSpace) +
                        ", Need: " + formatSize(needed));
                    return;
                }
            }
        }
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
    } else if (textInputPurpose_ == TextInputPurpose::RenameBoxName) {
        if (renamingBoxBank_ && !text.empty())
            renamingBoxBank_->setBoxName(renamingBoxIdx_, text);
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
