#include "ui.h"
#include "i18n.h"
#include "led.h"
#include "species_converter.h"
#include "form_names.h"
#include "personal_za.h"
#include "personal_sv.h"
#include "personal_swsh.h"
#include "personal_bdsp.h"
#include "personal_la.h"
#include "personal_gg.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <sys/stat.h>

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
        // ZL / ZR flip between the two tabs. Edge-triggered, so the trigger
        // that opened the overview does nothing until it is pressed again.
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
            bool pressed = event.caxis.value > TRIGGER_DEADZONE;
            if (pressed && !zlPressed_) {
                switchBoxViewPanel(Panel::Game);
                markDirty();
            }
            zlPressed_ = pressed;
        }
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
            bool pressed = event.caxis.value > TRIGGER_DEADZONE;
            if (pressed && !zrPressed_) {
                switchBoxViewPanel(Panel::Bank);
                markDirty();
            }
            zrPressed_ = pressed;
        }
    }
    // Can rename if viewing a bank panel (always in applet, only Bank panel in normal)
    auto canRenameBox = [&]() -> Bank* {
        if (isDualBankMode())
            return (boxViewPanel_ == Panel::Game) ? &bankLeft_ : &bank_;
        return (boxViewPanel_ == Panel::Bank) ? &bank_ : nullptr;
    };

    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_B: // Switch A = confirm
                closeBoxView(true);
                break;
            case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
                closeBoxView(false);
                break;
            case SDL_CONTROLLER_BUTTON_X: // Switch Y = rename
            {
                Bank* b = canRenameBox();
                if (b) {
                    renamingBoxIdx_ = boxViewCursor_;
                    renamingBoxBank_ = b;
                    beginTextInput(TextInputPurpose::RenameBoxName);
                }
                break;
            }
            case SDL_CONTROLLER_BUTTON_DPAD_UP:    moveBoxViewCursor(0, -1); break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:   moveBoxViewCursor(0, +1); break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:   moveBoxViewCursor(-1, 0); break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:  moveBoxViewCursor(+1, 0); break;
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

        // Any button/key event dirties the screen
        if (event.type == SDL_CONTROLLERBUTTONDOWN ||
            event.type == SDL_CONTROLLERBUTTONUP)
            markDirty();

        if (showMenu_)               { handleMenuInput(event, running); continue; }
        if (showSpeciesLetterPicker_){ handleSpeciesLetterPickerInput(event); continue; }
        if (showSearchFilter_)       { handleSearchFilterInput(event); continue; }
        if (showSearchResults_)      { handleSearchResultsInput(event); continue; }
        if (showWondercardList_)     { handleWondercardListInput(event); continue; }
        if (showCardList_)           { handleCardListInput(event); continue; }
        if (showBoxView_)            { handleBoxViewInput(event); continue; }
        if (showDetail_)             { handleDetailInput(event); continue; }

        handleNormalInput(event);
    }

    handleStickRepeat();
    handleBumperRepeat();
}

// Releasing is destructive (8b): red, A held (HOLD_MS), the Pokemon shown
// when there is one, and what can or cannot bring it back.
ConfirmStyle UI::releaseStyle(Panel from, const Pokemon* pkm, const std::string& where) {
    ConfirmStyle st;
    st.danger = true;
    st.hold = true;
    st.confirmKey = StrKey::DlgHoldRelease;
    const bool fromSave = from == Panel::Game && !isDualBankMode();
    // A save was backed up when it was opened; a bank never is.
    st.note = i18n::get(fromSave && lastBackup_ == BackupOutcome::Saved
                        ? StrKey::DlgBackupNote : StrKey::DlgCantUndo);
    if (pkm) {
        const Pokemon copy = *pkm;
        st.objectH = 62;
        st.drawObject = [this, copy, where](int x, int y, int w) {
            drawPokemonObjectRow(copy, where, x, y, w);
        };
    }
    return st;
}

void UI::handleDetailInput(const SDL_Event& event) {
    auto tryRelease = [&]() {
        int box = cursor_.box;
        int slot = cursor_.slot(gridCols());
        // Block releasing LGPE party members
        if (save_.isLGPEPartySlot(box, slot) && cursor_.panel == Panel::Game) {
            showMessageAndWait(i18n::get(StrKey::PartyPokemon),
                i18n::get(StrKey::CantReleaseParty));
            return;
        }
        Pokemon pkm = getPokemonAt(box, slot, cursor_.panel);
        if (pkm.isEmpty()) return;
        const bool fromSave = cursor_.panel == Panel::Game && !isDualBankMode();
        if (showConfirmDialog(i18n::fmt(StrKey::ReleaseConfirm, pkm.displayName()),
                              i18n::get(fromSave ? StrKey::DlgReleaseSaveSub : StrKey::DlgReleaseBankSub),
                              releaseStyle(cursor_.panel, &pkm, detailWhere()))) {
            clearPokemonAt(box, slot, cursor_.panel);
            // Remove from multi-select if selected
            if (!selectedSlots_.empty() && cursor_.panel == selectedPanel_
                && box == selectedBox_) {
                auto it = std::find(selectedSlots_.begin(),
                                    selectedSlots_.end(), slot);
                if (it != selectedSlots_.end())
                    selectedSlots_.erase(it);
            }
            showDetail_ = false;
            refreshHighlightSet();
        }
    };
    // Navigate to prev/next non-empty Pokemon in the box
    auto detailNav = [&](int dir) {
        int cols = gridCols();
        int slots = maxSlots();
        int cur = cursor_.slot(cols);
        for (int step = 1; step < slots; step++) {
            int next = (cur + dir * step % slots + slots) % slots;
            Pokemon pkm = getPokemonAt(cursor_.box, next, cursor_.panel);
            if (!pkm.isEmpty()) {
                cursor_.col = next % cols;
                cursor_.row = next / cols;
                detailRibbonScroll_ = 0;
                return;
            }
        }
    };
    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_A: // Switch B
                showDetail_ = false;
                break;
            case SDL_CONTROLLER_BUTTON_Y: { // Switch X — export
                Pokemon pkm = getPokemonAt(cursor_.box, cursor_.slot(gridCols()), cursor_.panel);
                if (!pkm.isEmpty()) {
                    std::string name = exportPokemon(pkm);
                    if (!name.empty())
                        showMessageAndWait(i18n::get(StrKey::Exported), name, DialogKind::Success);
                    else
                        showMessageAndWait(i18n::get(StrKey::ExportFailed), i18n::get(StrKey::CouldNotWrite));
                }
                break;
            }
            case SDL_CONTROLLER_BUTTON_X: { // Switch Y — save shareable card
                Pokemon pkm = getPokemonAt(cursor_.box, cursor_.slot(gridCols()), cursor_.panel);
                if (!pkm.isEmpty()) {
                    showWorking(i18n::get(StrKey::SavingCard), true);
                    std::string name = exportPokemonCard(pkm);
                    if (!name.empty())
                        showMessageAndWait(i18n::get(StrKey::Exported), name, DialogKind::Success);
                    else
                        showMessageAndWait(i18n::get(StrKey::ExportFailed),
                                           i18n::get(StrKey::CouldNotWrite));
                }
                break;
            }
            case SDL_CONTROLLER_BUTTON_B: // Switch A
                tryRelease();
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                scrollDetailRibbons(event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP ? -1 : +1,
                                    getPokemonAt(cursor_.box, cursor_.slot(gridCols()), cursor_.panel));
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                detailNav(-1);
                lHeld_ = true;
                bumperRepeatTime_ = SDL_GetTicks();
                bumperMoved_ = false;
                break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                detailNav(1);
                rHeld_ = true;
                bumperRepeatTime_ = SDL_GetTicks();
                bumperMoved_ = false;
                break;
        }
    }
    if (event.type == SDL_CONTROLLERBUTTONUP) {
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  lHeld_ = false; break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: rHeld_ = false; break;
        }
    }
}

void UI::handleNormalInput(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
            int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
            updateStick(lx, ly);
        }
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
            bool pressed = event.caxis.value > TRIGGER_DEADZONE;
            if (pressed && !zlPressed_ &&
                !(isDualBankMode() && leftBankName_.empty())) {
                openBoxView(Panel::Game);
                markDirty();
            }
            zlPressed_ = pressed;
        }
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
            bool pressed = event.caxis.value > TRIGGER_DEADZONE;
            if (pressed && !zrPressed_) {
                openBoxView(Panel::Bank);
                markDirty();
            }
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
                    if (heldFromLGPEParty_) {
                        showMessageAndWait(i18n::get(StrKey::PartyPokemon),
                            i18n::get(StrKey::CantReleaseParty));
                        break;
                    }
                    // Where it was picked up from decides what the dialog says.
                    const bool multi = !heldMulti_.empty();
                    const Panel from = multi ? heldMultiSource_
                                     : !swapHistory_.empty() ? swapHistory_.front().panel : cursor_.panel;
                    const std::string title = multi && heldMulti_.size() > 1
                        ? i18n::fmt(StrKey::ReleaseMultiConfirm, std::to_string(heldMulti_.size()))
                        : i18n::fmt(StrKey::ReleaseConfirm,
                                    (multi ? heldMulti_[0] : heldPkm_).displayName());
                    const Pokemon* one = multi ? (heldMulti_.size() == 1 ? &heldMulti_[0] : nullptr) : &heldPkm_;
                    const bool fromSave = from == Panel::Game && !isDualBankMode();
                    const std::string where = i18n::get(fromSave ? StrKey::TabSave : StrKey::TabBank);
                    if (showConfirmDialog(title,
                            i18n::get(fromSave ? StrKey::DlgReleaseSaveSub : StrKey::DlgReleaseBankSub),
                            releaseStyle(from, one, where))) {
                        heldMulti_.clear();
                        heldMultiSlots_.clear();
                        heldPkm_ = Pokemon{};
                        swapHistory_.clear();
                        holding_ = false;
                        positionPreserve_ = false;
                        heldFromLGPEParty_ = false;
                        lgpeHeldPartyIdx_ = -1;
                        refreshHighlightSet();
                    }
                } else {
                    Pokemon pkm = getPokemonAt(cursor_.box, cursor_.slot(gridCols()), cursor_.panel);
                    if (!pkm.isEmpty()) {
                        showDetail_ = true;
                        detailRibbonScroll_ = 0;
                    }
                }
                break;
            }
            case SDL_CONTROLLER_BUTTON_X: // Switch Y (left) = SDL X
                beginYPress();
                break;
            case SDL_CONTROLLER_BUTTON_START: // + (open menu)
                if (!yHeld_) {
                    openMenu();
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

}

void UI::handleStickRepeat() {
    if (stickDirX_ == 0 && stickDirY_ == 0) return;

    uint32_t now = SDL_GetTicks();
    uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
    if (now - stickMoveTime_ < delay) return;

    markDirty();
    if (showSpeciesLetterPicker_) {
        const int dx = stickDirX_ > 0 ? 1 : (stickDirX_ < 0 ? -1 : 0);
        const int dy = stickDirY_ > 0 ? 1 : (stickDirY_ < 0 ? -1 : 0);
        moveSpeciesPicker(dx, dy);
    } else if (screen_ == AppScreen::GtsHub) {
        if (showGtsFilter_) {
            if (stickDirY_ != 0) {
                int dir = stickDirY_ > 0 ? 1 : -1;
                gtsFilterCursor_ = (gtsFilterCursor_ + dir + GTS_FILTER_ROWS) % GTS_FILTER_ROWS;
            }
        } else if (showGtsDeposit_) {
            if (stickDirY_ != 0 && !gtsCards_.empty()) {
                int count = static_cast<int>(gtsCards_.size());
                gtsCardCursor_ = (gtsCardCursor_ + (stickDirY_ > 0 ? 1 : count - 1)) % count;
                // Restart the settle timer, so scrolling does not decode a QR
                // code for every row it passes over.
                gtsCardPreviewSince_ = SDL_GetTicks();
            }
        } else {
            if (stickDirX_ < 0) gtsHubCursor_ = 0;
            if (stickDirX_ > 0 && gtsHubCursor_ == 0) gtsHubCursor_ = 1;
            if (stickDirY_ < 0 && gtsHubCursor_ == 2) gtsHubCursor_ = 1;
            if (stickDirY_ > 0 && gtsHubCursor_ == 1) gtsHubCursor_ = 2;
        }
    } else if (screen_ == AppScreen::GtsBrowse) {
        if (!gtsDetail_) {
            if (stickDirX_ != 0) moveGtsCursor(stickDirX_, 0);
            if (stickDirY_ != 0) moveGtsCursor(0, stickDirY_);
        }
    } else if (showSearchFilter_) {
        if (stickDirY_ != 0)      moveSearchFilterCursor(stickDirY_ > 0 ? 1 : -1);
        else if (stickDirX_ != 0) switchSearchFilterColumn(stickDirX_ > 0 ? 1 : -1);
    } else if (showSearchResults_) {
        if (stickDirY_ != 0) moveSearchResult(stickDirY_ > 0 ? 1 : -1);
    } else if (showCardList_) {
        if (stickDirY_ != 0 && !cardList_.empty()) {
            int count = static_cast<int>(cardList_.size());
            cardListCursor_ += stickDirY_ > 0 ? 1 : -1;
            if (cardListCursor_ < 0) cardListCursor_ = count - 1;
            if (cardListCursor_ >= count) cardListCursor_ = 0;
            constexpr int ROW_H = 36;
            int visibleRows = (550 - 40 - 50) / ROW_H;
            if (cardListCursor_ < cardListScroll_)
                cardListScroll_ = cardListCursor_;
            else if (cardListCursor_ >= cardListScroll_ + visibleRows)
                cardListScroll_ = cardListCursor_ - visibleRows + 1;
            cardPreviewSince_ = SDL_GetTicks();
        }
    } else if (showWondercardList_) {
        if (stickDirY_ != 0 && !wcList_.empty()) {
            int count = static_cast<int>(wcList_.size());
            wcListCursor_ += stickDirY_ > 0 ? 1 : -1;
            if (wcListCursor_ < 0) wcListCursor_ = count - 1;
            if (wcListCursor_ >= count) wcListCursor_ = 0;
            const int visibleRows = WC_VISIBLE_ROWS;
            if (wcListCursor_ < wcListScroll_)
                wcListScroll_ = wcListCursor_;
            else if (wcListCursor_ >= wcListScroll_ + visibleRows)
                wcListScroll_ = wcListCursor_ - visibleRows + 1;
        }
    } else if (showBoxView_) {
        if (stickDirX_ != 0) moveBoxViewCursor(stickDirX_, 0);
        if (stickDirY_ != 0) moveBoxViewCursor(0, stickDirY_);
    } else if (showMenu_) {
        if (stickDirY_ != 0)      moveMenuCursor(0, stickDirY_);
        else if (stickDirX_ != 0) moveMenuCursor(stickDirX_, 0);
    } else if (!showDetail_) {
        if (stickDirX_ != 0) moveCursor(stickDirX_, 0);
        if (stickDirY_ != 0) moveCursor(0, stickDirY_);
    }
    stickMoveTime_ = now;
    stickMoved_ = true;
}

void UI::handleBumperRepeat() {
    if (!lHeld_ && !rHeld_) return;

    uint32_t now = SDL_GetTicks();
    uint32_t delay = bumperMoved_ ? BUMPER_REPEAT_DELAY : BUMPER_INITIAL_DELAY;
    if (now - bumperRepeatTime_ < delay) return;

    markDirty();
    if (showSearchResults_ && !searchResults_.empty()) {
        // Page through search results
        moveSearchResult(lHeld_ ? -10 : 10);
    } else if (showDetail_) {
        // Navigate to prev/next non-empty slot
        int dir = lHeld_ ? -1 : 1;
        int cols = gridCols();
        int slots = maxSlots();
        int cur = cursor_.slot(cols);
        for (int step = 1; step < slots; step++) {
            int next = (cur + dir * step % slots + slots) % slots;
            Pokemon pkm = getPokemonAt(cursor_.box, next, cursor_.panel);
            if (!pkm.isEmpty()) {
                cursor_.col = next % cols;
                cursor_.row = next / cols;
                detailRibbonScroll_ = 0;
                break;
            }
        }
    } else if (!showMenu_ && !showSearchFilter_ &&
               !showBoxView_) {
        // Box switching
        if (lHeld_) switchBox(-1);
        if (rHeld_) switchBox(+1);
    }
    bumperRepeatTime_ = now;
    bumperMoved_ = true;
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

    // Horizontal: crossing panel boundary or wrapping within single panel
    if (cursor_.col < 0) {
        if (cursor_.panel == Panel::Bank &&
            !(isDualBankMode() && leftBankName_.empty())) {
            cursor_.panel = Panel::Game;
            cursor_.col = maxCol;
            cursor_.box = gameBox_;
        } else if (cursor_.panel == Panel::Game) {
            cursor_.panel = Panel::Bank;
            cursor_.col = maxCol;
            cursor_.box = bankBox_;
        } else {
            cursor_.col = maxCol; // wrap within same panel
        }
    }
    if (cursor_.col > maxCol) {
        if (cursor_.panel == Panel::Game) {
            cursor_.panel = Panel::Bank;
            cursor_.col = 0;
            cursor_.box = bankBox_;
        } else if (!(isDualBankMode() && leftBankName_.empty())) {
            cursor_.panel = Panel::Game;
            cursor_.col = 0;
            cursor_.box = gameBox_;
        } else {
            cursor_.col = 0; // wrap within same panel
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
        if (isDualBankMode())
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
    if (panel == Panel::Preview)
        return previewBankPath_.empty() ? Pokemon{} : previewBank_.getSlot(box, slot);
    if (panel == Panel::Game) {
        if (isDualBankMode()) {
            if (leftBankName_.empty()) return Pokemon{};
            return bankLeft_.getSlot(box, slot);
        }
        return save_.getBoxSlot(box, slot);
    }
    return bank_.getSlot(box, slot);
}

void UI::setPokemonAt(int box, int slot, Panel panel, const Pokemon& pkm) {
    if (panel == Panel::Game) {
        if (isDualBankMode()) {
            if (leftBankName_.empty()) return;
            bankLeft_.setSlot(box, slot, pkm);
        } else
            save_.setBoxSlot(box, slot, pkm);
    } else {
        bank_.setSlot(box, slot, pkm);
    }
    invalidateSlotDisplay(panel, box);
    unsavedChanges_ = true;
}

void UI::clearPokemonAt(int box, int slot, Panel panel) {
    if (panel == Panel::Game) {
        if (isDualBankMode()) {
            if (leftBankName_.empty()) return;
            bankLeft_.clearSlot(box, slot);
        } else
            save_.clearBoxSlot(box, slot);
    } else {
        bank_.clearSlot(box, slot);
    }
    invalidateSlotDisplay(panel, box);
    unsavedChanges_ = true;
}

void UI::actionSelect() {
    // Block interaction on empty left panel in applet mode
    if (isDualBankMode() && cursor_.panel == Panel::Game && leftBankName_.empty())
        return;

    int box = cursor_.box;
    int slot = cursor_.slot(gridCols());

    // Multi-select pick up
    if (!holding_ && !selectedSlots_.empty()) {
        heldMulti_.clear();
        heldMultiSlots_.clear();
        heldMultiSource_ = selectedPanel_;
        heldMultiBox_ = selectedBox_;

        // Check if any selected slot is an LGPE party member; backup indices
        heldFromLGPEParty_ = false;
        lgpePartyBackup_ = save_.lgpePartyIndices();
        if (selectedPanel_ == Panel::Game) {
            for (int s : selectedSlots_) {
                if (save_.isLGPEPartySlot(selectedBox_, s)) {
                    heldFromLGPEParty_ = true;
                    break;
                }
            }
        }

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
        // Block LGPE party Pokemon from moving to bank
        if (heldFromLGPEParty_ && cursor_.panel == Panel::Bank) {
            showMessageAndWait(i18n::get(StrKey::PartyPokemon),
                i18n::get(StrKey::CantMovePartyBank));
            return;
        }
        // Helper: update LGPE party pointer when a Pokemon is placed at a new slot
        auto updatePartyPtr = [&](int origSlot, int newBox, int newSlot) {
            if (!heldFromLGPEParty_ || cursor_.panel != Panel::Game) return;
            // Find which party index the original slot belongs to (using backup)
            uint16_t origFlat = static_cast<uint16_t>(
                heldMultiBox_ * save_.slotsPerBox() + origSlot);
            for (int p = 0; p < 6; p++) {
                if (lgpePartyBackup_[p] == origFlat) {
                    uint16_t newFlat = static_cast<uint16_t>(
                        newBox * save_.slotsPerBox() + newSlot);
                    save_.setLGPEPartyPointer(p, newFlat);
                    break;
                }
            }
        };

        if (positionPreserve_) {
            // Position-preserving: place each Pokemon at its original slot index
            for (int i = 0; i < (int)heldMulti_.size(); i++) {
                int targetSlot = heldMultiSlots_[i];
                if (!getPokemonAt(box, targetSlot, cursor_.panel).isEmpty()) {
                    showMessageAndWait(i18n::get(StrKey::SlotsOccupied),
                        i18n::get(StrKey::SlotsOccupiedBody));
                    return;
                }
            }
            for (int i = 0; i < (int)heldMulti_.size(); i++) {
                setPokemonAt(box, heldMultiSlots_[i], cursor_.panel, heldMulti_[i]);
                updatePartyPtr(heldMultiSlots_[i], box, heldMultiSlots_[i]);
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
                showMessageAndWait(i18n::get(StrKey::NotEnoughSpaceSlots),
                    i18n::fmt(StrKey::NeedEmptySlots, std::to_string(heldMulti_.size()), std::to_string(emptyCount)));
                return;
            }
            int placed = 0;
            for (int s = 0; s < slotsInBox && placed < (int)heldMulti_.size(); s++) {
                if (getPokemonAt(box, s, cursor_.panel).isEmpty()) {
                    setPokemonAt(box, s, cursor_.panel, heldMulti_[placed]);
                    updatePartyPtr(heldMultiSlots_[placed], box, s);
                    placed++;
                }
            }
        }
        heldMulti_.clear();
        heldMultiSlots_.clear();
        holding_ = false;
        positionPreserve_ = false;
        heldFromLGPEParty_ = false;
        lgpeHeldPartyIdx_ = -1;
        return;
    }

    // Single pick/place
    if (!holding_) {
        Pokemon pkm = getPokemonAt(box, slot, cursor_.panel);
        if (pkm.isEmpty())
            return;

        heldPkm_ = pkm;
        holding_ = true;
        lgpeHeldPartyIdx_ = (cursor_.panel == Panel::Game)
            ? save_.lgpePartyIndexOf(box, slot) : -1;
        heldFromLGPEParty_ = (lgpeHeldPartyIdx_ >= 0);
        lgpePartyBackup_ = save_.lgpePartyIndices();
        swapHistory_.clear();
        swapHistory_.push_back({pkm, cursor_.panel, box, slot});

        clearPokemonAt(box, slot, cursor_.panel);
    } else {
        // Block LGPE party Pokemon from moving to bank
        if (heldFromLGPEParty_ && cursor_.panel == Panel::Bank) {
            showMessageAndWait(i18n::get(StrKey::PartyPokemon),
                i18n::get(StrKey::CantMovePartyBank));
            return;
        }

        Pokemon target = getPokemonAt(box, slot, cursor_.panel);

        if (target.isEmpty()) {
            // Place on empty — commit, clear history
            setPokemonAt(box, slot, cursor_.panel, heldPkm_);
            // Update party pointer to follow the Pokemon
            if (lgpeHeldPartyIdx_ >= 0 && cursor_.panel == Panel::Game) {
                uint16_t newFlat = static_cast<uint16_t>(
                    box * save_.slotsPerBox() + slot);
                save_.setLGPEPartyPointer(lgpeHeldPartyIdx_, newFlat);
            }
            holding_ = false;
            heldPkm_ = Pokemon{};
            swapHistory_.clear();
            heldFromLGPEParty_ = false;
            lgpeHeldPartyIdx_ = -1;
        } else {
            // Swap: check if target is also a party member BEFORE modifying
            int targetPartyIdx = (cursor_.panel == Panel::Game)
                ? save_.lgpePartyIndexOf(box, slot) : -1;

            swapHistory_.push_back({target, cursor_.panel, box, slot});
            setPokemonAt(box, slot, cursor_.panel, heldPkm_);

            // Update party pointer for the placed Pokemon
            if (lgpeHeldPartyIdx_ >= 0 && cursor_.panel == Panel::Game) {
                uint16_t newFlat = static_cast<uint16_t>(
                    box * save_.slotsPerBox() + slot);
                save_.setLGPEPartyPointer(lgpeHeldPartyIdx_, newFlat);
            }
            // Target was a party member but held Pokemon was not (cross-panel swap):
            // the party Pokemon is now held, so invalidate its pointer until placed.
            if (targetPartyIdx >= 0 && lgpeHeldPartyIdx_ < 0) {
                save_.setLGPEPartyPointer(targetPartyIdx, SaveFile::LGPE_SLOT_EMPTY);
            }

            heldPkm_ = target;
            lgpeHeldPartyIdx_ = targetPartyIdx;
            heldFromLGPEParty_ = (targetPartyIdx >= 0);
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
        heldFromLGPEParty_ = false;
        lgpeHeldPartyIdx_ = -1;
        save_.setLGPEPartyIndices(lgpePartyBackup_);
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
    heldFromLGPEParty_ = false;
    lgpeHeldPartyIdx_ = -1;
    save_.setLGPEPartyIndices(lgpePartyBackup_);
}

void UI::toggleSelect() {
    if (holding_)
        return; // can't multi-select while holding
    if (isDualBankMode() && cursor_.panel == Panel::Game && leftBankName_.empty())
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
    if (isDualBankMode() && cursor_.panel == Panel::Game && leftBankName_.empty())
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
    if (isDualBankMode() && cursor_.panel == Panel::Game && leftBankName_.empty())
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

// --- Species Picker ---

void UI::buildAvailableSpeciesList() {
    availableSpecies_.clear();

    // The GTS carries deposits from every game at once, so filtering its
    // picker by one game's personal table would hide most of the board. Any
    // species with a name is offerable there.
    if (speciesPickerForGts_) {
        for (uint16_t i = 1; i <= 1024; i++) {
            if (!SpeciesName::get(i).empty())
                availableSpecies_.push_back(i);
        }
        return;
    }

    // Use IsPresentInGame flags from personal tables (extracted from PKHeX binary data)
    auto isAvailable = [&](uint16_t species) -> bool {
        if (selectedGame_ == GameType::ZA) {
            return species < PERSONAL_ZA_COUNT && PersonalZA::IS_PRESENT[species];
        } else if (isSV(selectedGame_)) {
            return species < PERSONAL_SV_COUNT && PersonalSV::IS_PRESENT[species];
        } else if (isSwSh(selectedGame_)) {
            return species < PersonalSWSH::NUM_ENTRIES && PersonalSWSH::IS_PRESENT[species];
        } else if (selectedGame_ == GameType::LA) {
            return species < PERSONAL_LA_COUNT && PersonalLA::IS_PRESENT[species];
        } else if (isBDSP(selectedGame_)) {
            // BDSP: all species within table range are present
            return species >= 1 && species < PERSONAL_BDSP_COUNT;
        } else if (isLGPE(selectedGame_)) {
            // LGPE: no IsPresentInGame flag; check base stats
            if (species >= PERSONAL_GG_COUNT) return false;
            auto s = PersonalGG::BASE_STATS[species];
            return (s.hp | s.atk | s.def | s.spe | s.spa | s.spd) != 0;
        } else if (isFRLG(selectedGame_)) {
            return species >= 1 && species <= 386;
        }
        return false;
    };

    int maxSpecies = 1024;
    for (uint16_t i = 1; i <= maxSpecies; i++) {
        if (isAvailable(i))
            availableSpecies_.push_back(i);
    }
}

void UI::buildSpeciesListForLetter(int letterIndex) {
    speciesPickerList_.clear();
    if (letterIndex < 1 || letterIndex > 26) return;

    char letter = 'A' + (letterIndex - 1);
    for (uint16_t id : availableSpecies_) {
        const std::string& name = SpeciesName::get(id);
        if (!name.empty() && std::toupper(static_cast<unsigned char>(name[0])) == letter)
            speciesPickerList_.push_back(id);
    }
    // Sort alphabetically by name
    std::sort(speciesPickerList_.begin(), speciesPickerList_.end(),
        [](uint16_t a, uint16_t b) {
            return SpeciesName::get(a) < SpeciesName::get(b);
        });
}

bool UI::letterHasSpecies(int letterIndex) const {
    if (letterIndex == 0) return true; // "-" is always valid (clear filter)
    if (letterIndex < 1 || letterIndex > 26) return false;
    char letter = 'A' + (letterIndex - 1);
    for (uint16_t id : availableSpecies_) {
        const std::string& name = SpeciesName::get(id);
        if (!name.empty() && std::toupper(static_cast<unsigned char>(name[0])) == letter)
            return true;
    }
    return false;
}



// --- Search/Filter ---



bool UI::matchesSearchFilter(const Pokemon& pkm,
                             const std::string& filterSpecies,
                             const std::string& filterOT) const {
    if (pkm.isEmpty()) return false;

    auto toLower = [](const std::string& s) {
        std::string out = s;
        for (auto& c : out) c = std::tolower(static_cast<unsigned char>(c));
        return out;
    };

    // Species filter: exact match by ID if set, otherwise substring match
    if (searchFilter_.speciesId > 0) {
        if (pkm.species() != searchFilter_.speciesId)
            return false;
    } else if (!filterSpecies.empty()) {
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

    // At least N perfect IVs. 1 and 6 are the old "1+" and "6IV".
    if (searchFilter_.minPerfectIVs > 0) {
        int perfect = 0;
        if (pkm.ivHp()  == 31) perfect++;
        if (pkm.ivAtk() == 31) perfect++;
        if (pkm.ivDef() == 31) perfect++;
        if (pkm.ivSpe() == 31) perfect++;
        if (pkm.ivSpA() == 31) perfect++;
        if (pkm.ivSpD() == 31) perfect++;
        if (perfect < searchFilter_.minPerfectIVs) return false;
    }

    if (searchFilter_.ribbonFilter != RibbonFilter::Off) {
        auto ribbons = pkm.getRibbonsAndMarks();
        if (searchFilter_.ribbonFilter == RibbonFilter::HasAny) {
            if (ribbons.empty()) return false;
        } else if (searchFilter_.ribbonFilter == RibbonFilter::HasRibbon) {
            bool found = false;
            for (const auto& r : ribbons) { if (!r.isMark) { found = true; break; } }
            if (!found) return false;
        } else if (searchFilter_.ribbonFilter == RibbonFilter::HasMark) {
            bool found = false;
            for (const auto& r : ribbons) { if (r.isMark) { found = true; break; } }
            if (!found) return false;
        }
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
            if (isDualBankMode()) {
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
                    r.species = pkm.species();
                    r.form = pkm.form();
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

void UI::switchBoxViewPanel(Panel panel) {
    if (panel == boxViewPanel_)
        return;
    // Same rule as opening it with ZL: no left bank, nothing to show.
    if (panel == Panel::Game && isDualBankMode() && leftBankName_.empty())
        return;
    boxViewPanel_ = panel;
    boxViewCursor_ = (panel == Panel::Game) ? gameBox_ : bankBox_;
}

void UI::closeBoxView(bool navigate) {
    showBoxView_ = false;
    zlPressed_ = false;
    zrPressed_ = false;
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
        totalBoxes = (isDualBankMode()) ? bankLeft_.boxCount() : save_.boxCount();
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
            if (isDualBankMode()) {
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
                    searchResults_.push_back({panel, b, s, pkm.species(), pkm.form(),
                        SpeciesName::get(pkm.species()),
                        pkm.level(), pkm.isShiny(), pkm.isEgg(), pkm.isAlpha(),
                        pkm.gender(), pkm.otName()});
                }
            }
        }
    };

    scanPanel(Panel::Game);
    scanPanel(Panel::Bank);
}

// --- Wondercard List ---

void UI::handleWondercardListInput(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
            int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
            updateStick(lx, ly);
        }
    }

    int count = static_cast<int>(wcList_.size());
    if (count == 0) {
        // Only B to close
        if (event.type == SDL_CONTROLLERBUTTONDOWN && event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
            showWondercardList_ = false;
        return;
    }

    auto scrollIntoView = [&]() {
        const int visibleRows = WC_VISIBLE_ROWS;
        if (wcListCursor_ < wcListScroll_)
            wcListScroll_ = wcListCursor_;
        else if (wcListCursor_ >= wcListScroll_ + visibleRows)
            wcListScroll_ = wcListCursor_ - visibleRows + 1;
    };

    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                if (wcListCursor_ > 0) wcListCursor_--;
                else wcListCursor_ = count - 1;
                scrollIntoView();
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                if (wcListCursor_ < count - 1) wcListCursor_++;
                else wcListCursor_ = 0;
                scrollIntoView();
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: // L = page up
                wcListCursor_ = std::max(0, wcListCursor_ - 10);
                scrollIntoView();
                break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: // R = page down
                wcListCursor_ = std::min(count - 1, wcListCursor_ + 10);
                scrollIntoView();
                break;
            case SDL_CONTROLLER_BUTTON_B: // Switch A = confirm/inject
                injectWondercard(wcList_[wcListCursor_]);
                break;
            case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
                showWondercardList_ = false;
                break;
        }
    }
}

void UI::injectWondercard(const WCInfo& info) {
    if (!info.valid) {
        showMessageAndWait(i18n::get(StrKey::InvalidWC), i18n::get(StrKey::InvalidWCBody));
        return;
    }

    // Determine target slot (where cursor is)
    Panel panel = cursor_.panel;
    int box = (panel == Panel::Game) ? gameBox_ : bankBox_;
    int slot = cursor_.slot(gridCols());

    // If !hasOT and target is bank panel: show restriction
    if (!info.hasOT) {
        if (isDualBankMode()) {
            showMessageAndWait(i18n::get(StrKey::CannotInject),
                i18n::get(StrKey::CannotInjectBody));
            return;
        }
        if (panel == Panel::Bank) {
            showMessageAndWait(i18n::get(StrKey::CannotInjectBank),
                i18n::get(StrKey::CannotInjectBankBody));
            return;
        }
    }

    // Check if target slot is empty
    Pokemon existing = getPokemonAt(box, slot, panel);
    if (!existing.isEmpty()) {
        showMessageAndWait(i18n::get(StrKey::SlotOccupied),
            i18n::get(StrKey::SlotOccupiedBody));
        return;
    }

    // Get trainer info
    TrainerInfo trainer;
    if (!info.hasOT || !isDualBankMode()) {
        trainer = save_.getTrainerInfo();
        if (!trainer.valid) {
            trainer.id32 = 0;
            trainer.gender = 0;
            trainer.language = 2;
            trainer.valid = true;
        }
    }
    if (!trainer.valid) {
        trainer.id32 = 0;
        trainer.gender = 0;
        trainer.language = 2;
        trainer.otName = std::u16string(u"Player", 6);
        trainer.valid = true;
    }

    Pokemon pkm;
    uint16_t natId;

    if (isSwSh(selectedGame_)) {
        // WC8 path
        WC8 wc;
        if (!wc.loadFromFile(info.path)) {
            showWondercardList_ = false;
            showMessageAndWait(i18n::get(StrKey::Error), i18n::get(StrKey::FailedLoadWC));
            return;
        }

        // SW=44, SH=45
        trainer.gameVersion = (selectedGame_ == GameType::Sh) ? 45 : 44;

        pkm = wc.convertToPK8(trainer);
        natId = wc.species(); // already national dex
    } else if (selectedGame_ == GameType::ZA) {
        // WA9 path (Legends: Z-A)
        WA9 wc;
        if (!wc.loadFromFile(info.path)) {
            showWondercardList_ = false;
            showMessageAndWait(i18n::get(StrKey::Error), i18n::get(StrKey::FailedLoadWC));
            return;
        }

        trainer.gameVersion = 52; // ZA
        pkm = wc.convertToPA9(trainer);
        natId = SpeciesConverter::getNational9(wc.speciesInternal());
    } else if (isBDSP(selectedGame_)) {
        // WB8 path (Brilliant Diamond / Shining Pearl)
        WB8 wc;
        if (!wc.loadFromFile(info.path)) {
            showWondercardList_ = false;
            showMessageAndWait(i18n::get(StrKey::Error), i18n::get(StrKey::FailedLoadWC));
            return;
        }

        trainer.gameVersion = (selectedGame_ == GameType::BD) ? 48 : 49;
        pkm = wc.convertToPB8(trainer);
        natId = wc.species(); // already national dex
    } else if (selectedGame_ == GameType::LA) {
        // WA8 path (Legends: Arceus)
        WA8 wc;
        if (!wc.loadFromFile(info.path)) {
            showWondercardList_ = false;
            showMessageAndWait(i18n::get(StrKey::Error), i18n::get(StrKey::FailedLoadWC));
            return;
        }

        trainer.gameVersion = 47; // PLA
        pkm = wc.convertToPA8(trainer);
        natId = wc.species(); // already national dex
    } else if (isLGPE(selectedGame_)) {
        // WB7 path (Let's Go Pikachu/Eevee)
        WB7 wc;
        if (!wc.loadFromFile(info.path)) {
            showWondercardList_ = false;
            showMessageAndWait(i18n::get(StrKey::Error), i18n::get(StrKey::FailedLoadWC));
            return;
        }

        trainer.gameVersion = (selectedGame_ == GameType::GP) ? 42 : 43;
        pkm = wc.convertToPB7(trainer);
        natId = wc.species(); // already national dex
    } else {
        // WC9 path (Scarlet/Violet)
        WC9 wc;
        if (!wc.loadFromFile(info.path)) {
            showWondercardList_ = false;
            showMessageAndWait(i18n::get(StrKey::Error), i18n::get(StrKey::FailedLoadWC));
            return;
        }

        // SL=50, VL=51
        trainer.gameVersion = (selectedGame_ == GameType::V) ? 51 : 50;

        pkm = wc.convertToPK9(trainer);
        natId = SpeciesConverter::getNational9(wc.speciesInternal());
    }

    // Ensure correct game type for the panel
    if (panel == Panel::Game && !isDualBankMode())
        pkm.gameType_ = selectedGame_;
    else if (isSwSh(selectedGame_))
        pkm.gameType_ = GameType::Sw;
    else if (selectedGame_ == GameType::ZA)
        pkm.gameType_ = GameType::ZA;
    else if (isBDSP(selectedGame_))
        pkm.gameType_ = GameType::BD;
    else if (selectedGame_ == GameType::LA)
        pkm.gameType_ = GameType::LA;
    else if (isLGPE(selectedGame_))
        pkm.gameType_ = selectedGame_;
    else
        pkm.gameType_ = GameType::S;

    // Place the pokemon
    setPokemonAt(box, slot, panel, pkm);

    showWondercardList_ = false;

    std::string panelName = (panel == Panel::Game)
        ? (isDualBankMode() ? i18n::get(StrKey::LocLeft) : i18n::get(StrKey::LocSave))
        : (isDualBankMode() ? i18n::get(StrKey::LocRight) : i18n::get(StrKey::LocBank));
    showMessageAndWait(i18n::get(StrKey::Injected),
        i18n::fmt(StrKey::InjectedBody, SpeciesName::get(natId), panelName,
                  std::to_string(box + 1), std::to_string(slot + 1)), DialogKind::Success);
}

std::string UI::exportPokemon(const Pokemon& pkm) {
    if (pkm.isEmpty()) return "";

    // Build export directory: basePath/export/{bankFolder}/
    std::string dir = basePath_ + "export/";
    std::string gameDir = dir + bankFolderNameOf(selectedGame_) + "/";

    // PKHeX naming: {species:0000} - {form} - {flags} - {name} - {checksum:X4}{EC:X8}.{ext}
    char buf[512];
    uint16_t sp = pkm.species();
    uint8_t fm = pkm.form();
    const char* ext = pkFileExtension(selectedGame_);

    std::string formStr;
    if (fm != 0) {
        const char* formName = getFormName(sp, fm);
        if (formName)
            formStr = std::string(" - ") + formName;
        else {
            char fb[16];
            std::snprintf(fb, sizeof(fb), " - %02u", fm);
            formStr = fb;
        }
    }

    std::string tags;
    {
        std::string flags;
        if (pkm.isShiny()) flags += "S";
        if (pkm.isAlpha()) flags += "A";
        if (pkm.isEgg())   flags += "E";
        if (!flags.empty()) tags = " - [" + flags + "]";
    }
    std::string nick = SpeciesName::get(sp);

    // Checksum at 0x06 for modern, 0x1C for PK3
    uint16_t chk = isFRLG(selectedGame_) ? pkm.readU16(0x1C) : pkm.readU16(0x06);
    uint32_t ec = pkm.encryptionConstant();

    // Short game tag
    const char* gameTag = gameInfo(selectedGame_).gameTag;

    std::snprintf(buf, sizeof(buf), "%s - %04u%s%s - %s - %04X%08X.%s",
                  gameTag, sp, formStr.c_str(), tags.c_str(), nick.c_str(), chk, ec, ext);

    // Sanitize filename: replace filesystem-unsafe chars
    std::string filename = buf;
    for (char& c : filename) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    }

    std::string fullPath = gameDir + filename;

    // Create directories only when we're about to write
    mkdir(dir.c_str(), 0755);
    mkdir(gameDir.c_str(), 0755);

    // Write decrypted party-size data
    int size = pkPartySize(selectedGame_);
    FILE* f = std::fopen(fullPath.c_str(), "wb");
    if (!f) return "";
    std::fwrite(pkm.data.data(), 1, size, f);
    std::fclose(f);

    return filename;
}
