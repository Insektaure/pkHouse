#include "ui.h"
#include "ui_util.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <sys/statvfs.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

// --- Bank Selector ---

void UI::drawBankSelectorFrame() {
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);

    // Show split view: bank selector on one side, other panel visible
    // - Normal mode: save on left, selector on right (when save is loaded)
    // - Applet mode: other bank on opposite side (when a bank is loaded)
    bool splitView = appletMode_ ? !activeBankName_.empty() : save_.isLoaded();

    // Determine selector area
    int selCenterX = SCREEN_W / 2;
    int selAreaX = 0;
    if (splitView) {
        if (appletMode_ && bankSelTarget_ == Panel::Game) {
            // Applet: selector on left, keep right bank visible
            selCenterX = PANEL_X_L + PANEL_W / 2;
            selAreaX = PANEL_X_L;
            std::string rightBoxName = activeBankName_ + " - " + bank_.getBoxName(bankBox_);
            drawPanel(PANEL_X_R, rightBoxName, bankBox_, bank_.boxCount(),
                      false, nullptr, &bank_, bankBox_, Panel::Bank);
        } else {
            // Normal mode or applet switching right bank: selector on right
            selCenterX = PANEL_X_R + PANEL_W / 2;
            selAreaX = PANEL_X_R;
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

            // Bank name (left-aligned)
            drawText(banks[idx].name, LIST_X + 20, rowY + (ROW_H - 4) / 2 - 9,
                     T().text, font_);

            // Slot count (right-aligned)
            int maxSlots = isLGPE(selectedGame_) ? 1000 : isBDSP(selectedGame_) ? 1200 : 960;
            std::string slotStr = std::to_string(banks[idx].occupiedSlots) + "/" + std::to_string(maxSlots);
            int tw = 0, th = 0;
            TTF_SizeUTF8(font_, slotStr.c_str(), &tw, &th);
            drawText(slotStr, LIST_X + LIST_W - 20 - tw, rowY + (ROW_H - 4) / 2 - 9,
                     T().textDim, font_);
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
        drawText(label, SCREEN_W - tw - 15, SCREEN_H - 26, T().goldLabel, fontSmall_);
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
                        if (appletMode_ && leftBankName_.empty()) {
                            cursor_ = Cursor{};
                            cursor_.panel = Panel::Bank;
                        }
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
                        if (appletMode_ && leftBankName_.empty()) {
                            cursor_ = Cursor{};
                            cursor_.panel = Panel::Bank;
                        }
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

#ifdef __SWITCH__
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
#else
    showTextInput_ = true;
    SDL_StartTextInput();
#endif
}

void UI::drawTextInputPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    constexpr int POP_W = 500;
    constexpr int POP_H = 180;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);

    const char* title = "Text Input";
    switch (textInputPurpose_) {
        case TextInputPurpose::CreateBank:    title = "New Bank Name"; break;
        case TextInputPurpose::RenameBank:    title = "Rename Bank"; break;
        case TextInputPurpose::RenameBoxName: title = "Rename Box"; break;
        case TextInputPurpose::SearchSpecies: title = "Species Name"; break;
        case TextInputPurpose::SearchOT:      title = "OT Name"; break;
        case TextInputPurpose::SearchLevelMin: title = "Min Level"; break;
        case TextInputPurpose::SearchLevelMax: title = "Max Level"; break;
    }
    drawTextCentered(title, popX + POP_W / 2, popY + 30, T().text, font_);

    // Text field background
    int fieldX = popX + 40;
    int fieldY = popY + 60;
    int fieldW = POP_W - 80;
    int fieldH = 36;
    drawRect(fieldX, fieldY, fieldW, fieldH, T().textFieldBg);
    drawRectOutline(fieldX, fieldY, fieldW, fieldH, T().textDim, 1);

    // Text content
    if (!textInputBuffer_.empty()) {
        drawText(textInputBuffer_, fieldX + 8, fieldY + 8, T().text, font_);
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
        drawRect(cursorX, fieldY + 6, 2, fieldH - 12, T().text);
    }

    drawTextCentered("Enter:Confirm  Escape:Cancel",
                     popX + POP_W / 2, popY + POP_H - 25, T().textDim, fontSmall_);
}

void UI::handleTextInputEvent(const SDL_Event& event) {
    if (event.type == SDL_TEXTINPUT) {
        int maxLen = (textInputPurpose_ == TextInputPurpose::RenameBoxName) ? 16 : 32;
        if ((int)textInputBuffer_.size() < maxLen) {
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
#ifdef __SWITCH__
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
#endif
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
