#include "ui.h"
#include <algorithm>
#include <cmath>

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
                    } else if (menuSelection_ == 1) {
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
                    } else if (menuSelection_ == 2) {
                        // Change Game — save banks and return to game selector
                        if (!saveBankFiles()) { showMenu_ = false; return; }
                        leftBankName_.clear();
                        leftBankPath_.clear();
                        activeBankName_.clear();
                        activeBankPath_.clear();
                        screen_ = AppScreen::GameSelector;
                        showMenu_ = false;
                    } else if (menuSelection_ == 3) {
                        saveBankFiles();
                        showMenu_ = false;
                    } else {
                        running = false;
                    }
                } else {
                    // 0=Switch Bank, 1=Change Game, 2=Save & Quit, 3=Quit Without Saving
                    if (menuSelection_ == 0) {
                        if (!saveBankFiles()) { showMenu_ = false; return; }
                        showWorking("Saving...");
                        if (save_.isLoaded())
                            save_.save(savePath_);
                        account_.commitSave();
                        bankManager_.refresh();
                        screen_ = AppScreen::BankSelector;
                        showMenu_ = false;
                    } else if (menuSelection_ == 1) {
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
