#include "ui.h"
#include "species_converter.h"
#include <algorithm>
#include <cmath>
#include <cctype>

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
            int menuCount = appletMode_ ? 7 : 6;
            auto menuConfirm = [&]() {
                // 0=Theme, 1=Search (both modes)
                if (menuSelection_ == 0) {
                    showThemeSelector_ = true;
                    themeSelCursor_ = themeIndex_;
                    themeSelOriginal_ = themeIndex_;
                    return;
                }
                if (menuSelection_ == 1) {
                    showMenu_ = false;
                    showSearchFilter_ = true;
                    searchFilterCursor_ = 0;
                    searchFilter_ = SearchFilter{};
                    clearSearchHighlight();
                    return;
                }
                int sel = menuSelection_ - 2; // shift for Theme + Search at index 0-1
                if (appletMode_) {
                    // sel: 0=Switch Left Bank, 1=Switch Right Bank, 2=Change Game,
                    // 3=Save Banks, 4=Quit
                    if (sel == 0) {
                        // Need at least 1 bank available that isn't loaded on the right
                        int avail = 0;
                        for (auto& b : bankManager_.list())
                            if (b.name != activeBankName_) avail++;
                        if (avail == 0) {
                            showMenu_ = false;
                            if (!showConfirmDialog("No Banks Available",
                                    "Create a new bank?")) return;
                            if (!saveBankFiles()) return;
                            bankManager_.refresh();
                            bankSelTarget_ = Panel::Game;
                            screen_ = AppScreen::BankSelector;
                            beginTextInput(TextInputPurpose::CreateBank);
                            return;
                        }
                        if (!saveBankFiles()) { showMenu_ = false; return; }
                        bankManager_.refresh();
                        bankSelTarget_ = Panel::Game;
                        screen_ = AppScreen::BankSelector;
                        showMenu_ = false;
                    } else if (sel == 1) {
                        // Need at least 1 bank available that isn't loaded on the left
                        int avail = 0;
                        for (auto& b : bankManager_.list())
                            if (b.name != leftBankName_) avail++;
                        if (avail == 0) {
                            showMenu_ = false;
                            if (!showConfirmDialog("No Banks Available",
                                    "Create a new bank?")) return;
                            if (!saveBankFiles()) return;
                            bankManager_.refresh();
                            bankSelTarget_ = Panel::Bank;
                            screen_ = AppScreen::BankSelector;
                            beginTextInput(TextInputPurpose::CreateBank);
                            return;
                        }
                        if (!saveBankFiles()) { showMenu_ = false; return; }
                        bankManager_.refresh();
                        bankSelTarget_ = Panel::Bank;
                        screen_ = AppScreen::BankSelector;
                        showMenu_ = false;
                    } else if (sel == 2) {
                        // Change Game — save banks and return to game selector
                        if (!saveBankFiles()) { showMenu_ = false; return; }
                        leftBankName_.clear();
                        leftBankPath_.clear();
                        activeBankName_.clear();
                        activeBankPath_.clear();
                        screen_ = AppScreen::GameSelector;
                        showMenu_ = false;
                    } else if (sel == 3) {
                        saveBankFiles();
                        showMenu_ = false;
                    } else {
                        running = false;
                    }
                } else {
                    // sel: 0=Switch Bank, 1=Change Game, 2=Save & Quit, 3=Quit Without Saving
                    if (sel == 0) {
                        if (!saveBankFiles()) { showMenu_ = false; return; }
                        showWorking("Saving...");
                        if (save_.isLoaded())
                            save_.save(savePath_);
                        account_.commitSave();
                        bankManager_.refresh();
                        screen_ = AppScreen::BankSelector;
                        showMenu_ = false;
                    } else if (sel == 1) {
                        // Change Game — save everything, unmount, go to game selector
                        if (!saveBankFiles()) { showMenu_ = false; return; }
                        showWorking("Saving...");
                        if (save_.isLoaded())
                            save_.save(savePath_);
                        account_.commitSave();
                        account_.unmountSave();
                        activeBankName_.clear();
                        activeBankPath_.clear();
                        screen_ = AppScreen::GameSelector;
                        showMenu_ = false;
                    } else if (sel == 2) {
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

        // While search filter is open, handle its input only
        if (showSearchFilter_) {
            handleSearchFilterInput(event);
            continue;
        }

        // While search results are open, handle their input only
        if (showSearchResults_) {
            handleSearchResultsInput(event);
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
                    if (!yHeld_) { actionSelect(); refreshHighlightSet(); }
                    break;
                case SDL_CONTROLLER_BUTTON_A: // Switch B (bottom) = SDL A
                    if (!yHeld_) { actionCancel(); refreshHighlightSet(); }
                    break;
                case SDL_CONTROLLER_BUTTON_Y: // Switch X (top) = SDL Y
                {
                    if (yHeld_) break;
                    if (holding_) {
                        int count = heldMulti_.empty() ? 1 : (int)heldMulti_.size();
                        std::string msg = "Delete " + std::to_string(count) + " Pokemon?";
                        if (showConfirmDialog("Delete Pokemon", msg)) {
                            heldMulti_.clear();
                            heldMultiSlots_.clear();
                            heldPkm_ = Pokemon{};
                            swapHistory_.clear();
                            holding_ = false;
                            positionPreserve_ = false;
                            refreshHighlightSet();
                        }
                    } else {
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
                    if (!yHeld_) {
                        switchBox(-1);
                        lHeld_ = true;
                        bumperRepeatTime_ = SDL_GetTicks();
                        bumperMoved_ = false;
                    }
                    break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                    if (!yHeld_) {
                        switchBox(+1);
                        rHeld_ = true;
                        bumperRepeatTime_ = SDL_GetTicks();
                        bumperMoved_ = false;
                    }
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
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                    lHeld_ = false;
                    break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                    rHeld_ = false;
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
                case SDLK_RETURN: if (!yHeld_) { actionSelect(); refreshHighlightSet(); } break;
                case SDLK_b:
                case SDLK_ESCAPE: if (!yHeld_) { actionCancel(); refreshHighlightSet(); } break;
                case SDLK_x:
                {
                    if (yHeld_) break;
                    if (holding_) {
                        int count = heldMulti_.empty() ? 1 : (int)heldMulti_.size();
                        std::string msg = "Delete " + std::to_string(count) + " Pokemon?";
                        if (showConfirmDialog("Delete Pokemon", msg)) {
                            heldMulti_.clear();
                            heldMultiSlots_.clear();
                            heldPkm_ = Pokemon{};
                            swapHistory_.clear();
                            holding_ = false;
                            positionPreserve_ = false;
                            refreshHighlightSet();
                        }
                    } else {
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
            if (showSearchFilter_) {
                bool alpha = (selectedGame_ == GameType::LA || selectedGame_ == GameType::ZA);
                if (stickDirY_ != 0) {
                    int dir = stickDirY_ > 0 ? 1 : -1;
                    searchFilterCursor_ = (searchFilterCursor_ + dir + 11) % 11;
                    if (!alpha && searchFilterCursor_ == 4)
                        searchFilterCursor_ = (searchFilterCursor_ + dir + 11) % 11;
                }
                if (stickDirX_ != 0 && searchFilterCursor_ == 6)
                    searchLevelFocus_ = stickDirX_ > 0 ? 1 : 0;
                if (stickDirX_ != 0 && searchFilterCursor_ == 8)
                    searchFilter_.mode = stickDirX_ > 0 ? SearchMode::Highlight : SearchMode::List;
            } else if (showSearchResults_) {
                if (stickDirY_ != 0 && !searchResults_.empty()) {
                    searchResultCursor_ += stickDirY_ > 0 ? 1 : -1;
                    if (searchResultCursor_ < 0) searchResultCursor_ = 0;
                    if (searchResultCursor_ >= (int)searchResults_.size())
                        searchResultCursor_ = (int)searchResults_.size() - 1;
                    int visibleRows = 12;
                    if (searchResultCursor_ < searchResultScroll_)
                        searchResultScroll_ = searchResultCursor_;
                    if (searchResultCursor_ >= searchResultScroll_ + visibleRows)
                        searchResultScroll_ = searchResultCursor_ - visibleRows + 1;
                }
            } else if (showBoxView_) {
                if (stickDirX_ != 0) moveBoxViewCursor(stickDirX_, 0);
                if (stickDirY_ != 0) moveBoxViewCursor(0, stickDirY_);
            } else if (showMenu_) {
                if (stickDirY_ != 0)
                {
                    int mc = appletMode_ ? 7 : 6;
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

    // L/R shoulder button repeat
    if (lHeld_ || rHeld_) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = bumperMoved_ ? BUMPER_REPEAT_DELAY : BUMPER_INITIAL_DELAY;
        if (now - bumperRepeatTime_ >= delay) {
            if (showSearchResults_ && !searchResults_.empty()) {
                // Page through search results
                int dir = lHeld_ ? -10 : 10;
                searchResultCursor_ += dir;
                if (searchResultCursor_ < 0) searchResultCursor_ = 0;
                if (searchResultCursor_ >= (int)searchResults_.size())
                    searchResultCursor_ = (int)searchResults_.size() - 1;
                int visibleRows = 12;
                if (searchResultCursor_ < searchResultScroll_)
                    searchResultScroll_ = searchResultCursor_;
                if (searchResultCursor_ >= searchResultScroll_ + visibleRows)
                    searchResultScroll_ = searchResultCursor_ - visibleRows + 1;
            } else if (!showDetail_ && !showMenu_ && !showSearchFilter_ &&
                       !showBoxView_) {
                // Box switching
                if (lHeld_) switchBox(-1);
                if (rHeld_) switchBox(+1);
            }
            bumperRepeatTime_ = now;
            bumperMoved_ = true;
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
    // Dismiss search highlight
    if (searchHighlightActive_ && !holding_ && selectedSlots_.empty()) {
        clearSearchHighlight();
        return;
    }

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

// --- Search/Filter ---

void UI::handleSearchFilterInput(const SDL_Event& event) {
    // Delegate to text input if active (PC only)
    if (showTextInput_) {
        handleTextInputEvent(event);
        return;
    }

    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
            int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
            updateStick(lx, ly);
        }
        return;
    }

    bool hasAlpha = (selectedGame_ == GameType::LA || selectedGame_ == GameType::ZA);

    auto moveFilterCursor = [&](int dir) {
        searchFilterCursor_ = (searchFilterCursor_ + dir + 11) % 11;
        if (!hasAlpha && searchFilterCursor_ == 4)
            searchFilterCursor_ = (searchFilterCursor_ + dir + 11) % 11;
    };

    auto confirmAction = [&]() {
        switch (searchFilterCursor_) {
            case 0: beginTextInput(TextInputPurpose::SearchSpecies); break;
            case 1: beginTextInput(TextInputPurpose::SearchOT); break;
            case 2: searchFilter_.filterShiny = !searchFilter_.filterShiny; break;
            case 3: searchFilter_.filterEgg = !searchFilter_.filterEgg; break;
            case 4: searchFilter_.filterAlpha = !searchFilter_.filterAlpha; break;
            case 5:
                searchFilter_.gender = static_cast<GenderFilter>(
                    (static_cast<int>(searchFilter_.gender) + 1) % 4);
                break;
            case 6:
                if (searchLevelFocus_ == 0)
                    beginTextInput(TextInputPurpose::SearchLevelMin);
                else
                    beginTextInput(TextInputPurpose::SearchLevelMax);
                break;
            case 7:
                searchFilter_.perfectIVs = static_cast<PerfectIVFilter>(
                    (static_cast<int>(searchFilter_.perfectIVs) + 1) % 3);
                break;
            case 8:
                searchFilter_.mode = (searchFilter_.mode == SearchMode::List)
                    ? SearchMode::Highlight : SearchMode::List;
                break;
            case 9: searchFilter_ = SearchFilter{}; break;
            case 10: executeSearch(); break;
        }
    };

    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                moveFilterCursor(-1);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                moveFilterCursor(1);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                if (searchFilterCursor_ == 6) searchLevelFocus_ = 0;
                else if (searchFilterCursor_ == 8) searchFilter_.mode = SearchMode::List;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                if (searchFilterCursor_ == 6) searchLevelFocus_ = 1;
                else if (searchFilterCursor_ == 8) searchFilter_.mode = SearchMode::Highlight;
                break;
            case SDL_CONTROLLER_BUTTON_B: // Switch A = confirm
                confirmAction();
                break;
            case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
                showSearchFilter_ = false;
                break;
        }
    }
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_UP:
                moveFilterCursor(-1);
                break;
            case SDLK_DOWN:
                moveFilterCursor(1);
                break;
            case SDLK_LEFT:
                if (searchFilterCursor_ == 6) searchLevelFocus_ = 0;
                else if (searchFilterCursor_ == 8) searchFilter_.mode = SearchMode::List;
                break;
            case SDLK_RIGHT:
                if (searchFilterCursor_ == 6) searchLevelFocus_ = 1;
                else if (searchFilterCursor_ == 8) searchFilter_.mode = SearchMode::Highlight;
                break;
            case SDLK_a:
            case SDLK_RETURN:
                confirmAction();
                break;
            case SDLK_b:
            case SDLK_ESCAPE:
                showSearchFilter_ = false;
                break;
        }
    }
}

void UI::handleSearchResultsInput(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
            int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
            updateStick(lx, ly);
        }
        return;
    }

    auto navigate = [&](int dir) {
        if (searchResults_.empty()) return;
        searchResultCursor_ += dir;
        if (searchResultCursor_ < 0) searchResultCursor_ = 0;
        if (searchResultCursor_ >= (int)searchResults_.size())
            searchResultCursor_ = (int)searchResults_.size() - 1;
        int visibleRows = 12;
        if (searchResultCursor_ < searchResultScroll_)
            searchResultScroll_ = searchResultCursor_;
        if (searchResultCursor_ >= searchResultScroll_ + visibleRows)
            searchResultScroll_ = searchResultCursor_ - visibleRows + 1;
    };

    auto jumpToResult = [&]() {
        if (searchResults_.empty()) return;
        const auto& r = searchResults_[searchResultCursor_];
        cursor_.panel = r.panel;
        cursor_.box = r.box;
        cursor_.col = r.slot % gridCols();
        cursor_.row = r.slot / gridCols();
        if (r.panel == Panel::Game)
            gameBox_ = r.box;
        else
            bankBox_ = r.box;
        showSearchResults_ = false;
    };

    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:   navigate(-1); break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  navigate(1);  break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                navigate(-10);
                lHeld_ = true;
                bumperRepeatTime_ = SDL_GetTicks();
                bumperMoved_ = false;
                break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                navigate(10);
                rHeld_ = true;
                bumperRepeatTime_ = SDL_GetTicks();
                bumperMoved_ = false;
                break;
            case SDL_CONTROLLER_BUTTON_B: // Switch A = jump
                jumpToResult();
                break;
            case SDL_CONTROLLER_BUTTON_A: // Switch B = close
                showSearchResults_ = false;
                break;
            case SDL_CONTROLLER_BUTTON_Y: // Switch X = back to filter
                showSearchResults_ = false;
                showSearchFilter_ = true;
                break;
        }
    }
    if (event.type == SDL_CONTROLLERBUTTONUP) {
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  lHeld_ = false; break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: rHeld_ = false; break;
        }
    }
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_UP:   navigate(-1); break;
            case SDLK_DOWN: navigate(1);  break;
            case SDLK_q:    navigate(-10); break;
            case SDLK_e:    navigate(10);  break;
            case SDLK_a:
            case SDLK_RETURN:
                jumpToResult();
                break;
            case SDLK_b:
            case SDLK_ESCAPE:
                showSearchResults_ = false;
                break;
            case SDLK_x:
                showSearchResults_ = false;
                showSearchFilter_ = true;
                break;
        }
    }
}

bool UI::matchesSearchFilter(const Pokemon& pkm,
                             const std::string& filterSpecies,
                             const std::string& filterOT) const {
    if (pkm.isEmpty()) return false;

    auto toLower = [](const std::string& s) {
        std::string out = s;
        for (auto& c : out) c = std::tolower(static_cast<unsigned char>(c));
        return out;
    };

    if (!filterSpecies.empty()) {
        std::string name = toLower(SpeciesName::get(pkm.species()));
        if (name.find(filterSpecies) == std::string::npos)
            return false;
    }

    if (!filterOT.empty()) {
        std::string ot = toLower(pkm.otName());
        if (ot.find(filterOT) == std::string::npos)
            return false;
    }

    if (searchFilter_.filterShiny && !pkm.isShiny()) return false;
    if (searchFilter_.filterEgg && !pkm.isEgg()) return false;
    if (searchFilter_.filterAlpha && !pkm.isAlpha()) return false;

    if (searchFilter_.gender != GenderFilter::Any) {
        uint8_t g = pkm.gender();
        if (searchFilter_.gender == GenderFilter::Male && g != 0) return false;
        if (searchFilter_.gender == GenderFilter::Female && g != 1) return false;
        if (searchFilter_.gender == GenderFilter::Genderless && g != 2) return false;
    }

    if (!pkm.isEgg()) {
        uint8_t lv = pkm.level();
        if (searchFilter_.levelMin > 0 && lv < searchFilter_.levelMin) return false;
        if (searchFilter_.levelMax > 0 && lv > searchFilter_.levelMax) return false;
    }

    if (searchFilter_.perfectIVs != PerfectIVFilter::Off) {
        int perfect = 0;
        if (pkm.ivHp()  == 31) perfect++;
        if (pkm.ivAtk() == 31) perfect++;
        if (pkm.ivDef() == 31) perfect++;
        if (pkm.ivSpe() == 31) perfect++;
        if (pkm.ivSpA() == 31) perfect++;
        if (pkm.ivSpD() == 31) perfect++;
        if (searchFilter_.perfectIVs == PerfectIVFilter::AtLeastOne && perfect == 0) return false;
        if (searchFilter_.perfectIVs == PerfectIVFilter::All6 && perfect < 6) return false;
    }

    return true;
}

void UI::executeSearch() {
    searchResults_.clear();

    auto toLower = [](const std::string& s) {
        std::string out = s;
        for (auto& c : out) c = std::tolower(static_cast<unsigned char>(c));
        return out;
    };
    std::string filterSpecies = toLower(searchFilter_.speciesName);
    std::string filterOT = toLower(searchFilter_.otName);

    auto scanPanel = [&](Panel panel) {
        int boxes, slots;
        if (panel == Panel::Game) {
            if (appletMode_) {
                if (leftBankName_.empty()) return;
                boxes = bankLeft_.boxCount();
                slots = bankLeft_.slotsPerBox();
            } else {
                boxes = save_.boxCount();
                slots = save_.slotsPerBox();
            }
        } else {
            boxes = bank_.boxCount();
            slots = bank_.slotsPerBox();
        }

        for (int b = 0; b < boxes; b++) {
            for (int s = 0; s < slots; s++) {
                Pokemon pkm = getPokemonAt(b, s, panel);
                if (matchesSearchFilter(pkm, filterSpecies, filterOT)) {
                    SearchResult r;
                    r.panel = panel;
                    r.box = b;
                    r.slot = s;
                    r.speciesName = SpeciesName::get(pkm.species());
                    r.level = pkm.level();
                    r.isShiny = pkm.isShiny();
                    r.isEgg = pkm.isEgg();
                    r.isAlpha = pkm.isAlpha();
                    r.gender = pkm.gender();
                    r.otName = pkm.otName();
                    searchResults_.push_back(r);
                }
            }
        }
    };

    scanPanel(Panel::Game);
    scanPanel(Panel::Bank);

    if (searchFilter_.mode == SearchMode::Highlight) {
        searchMatchSet_.clear();
        for (const auto& r : searchResults_) {
            uint64_t key = (static_cast<uint64_t>(r.panel == Panel::Bank ? 1 : 0) << 48)
                         | (static_cast<uint64_t>(r.box) << 16)
                         | static_cast<uint64_t>(r.slot);
            searchMatchSet_.insert(key);
        }
        searchHighlightActive_ = true;
        showSearchFilter_ = false;
    } else {
        searchMatchSet_.clear();
        searchHighlightActive_ = false;
        searchResultCursor_ = 0;
        searchResultScroll_ = 0;
        showSearchFilter_ = false;
        showSearchResults_ = true;
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

bool UI::isSearchMatch(Panel panel, int box, int slot) const {
    uint64_t key = (static_cast<uint64_t>(panel == Panel::Bank ? 1 : 0) << 48)
                 | (static_cast<uint64_t>(box) << 16)
                 | static_cast<uint64_t>(slot);
    return searchMatchSet_.count(key) > 0;
}

void UI::clearSearchHighlight() {
    searchHighlightActive_ = false;
    searchMatchSet_.clear();
    searchResults_.clear();
}

void UI::refreshHighlightSet() {
    if (!searchHighlightActive_) return;
    searchMatchSet_.clear();
    searchResults_.clear();

    auto toLower = [](const std::string& s) {
        std::string out = s;
        for (auto& c : out) c = std::tolower(static_cast<unsigned char>(c));
        return out;
    };
    std::string filterSpecies = toLower(searchFilter_.speciesName);
    std::string filterOT = toLower(searchFilter_.otName);

    auto scanPanel = [&](Panel panel) {
        int boxes, slots;
        if (panel == Panel::Game) {
            if (appletMode_) {
                if (leftBankName_.empty()) return;
                boxes = bankLeft_.boxCount();
                slots = bankLeft_.slotsPerBox();
            } else {
                boxes = save_.boxCount();
                slots = save_.slotsPerBox();
            }
        } else {
            boxes = bank_.boxCount();
            slots = bank_.slotsPerBox();
        }

        for (int b = 0; b < boxes; b++) {
            for (int s = 0; s < slots; s++) {
                Pokemon pkm = getPokemonAt(b, s, panel);
                if (matchesSearchFilter(pkm, filterSpecies, filterOT)) {
                    uint64_t key = (static_cast<uint64_t>(panel == Panel::Bank ? 1 : 0) << 48)
                                 | (static_cast<uint64_t>(b) << 16)
                                 | static_cast<uint64_t>(s);
                    searchMatchSet_.insert(key);
                    searchResults_.push_back({panel, b, s, SpeciesName::get(pkm.species()),
                        pkm.level(), pkm.isShiny(), pkm.isEgg(), pkm.isAlpha(),
                        pkm.gender(), pkm.otName()});
                }
            }
        }
    };

    scanPanel(Panel::Game);
    scanPanel(Panel::Bank);
}
