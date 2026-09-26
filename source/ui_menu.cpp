// Menu (+), UI 2.0
// Left: tools and where to go; right: settings and how to leave.
//
// What is in it follows the same rules as the old list: Wondercard only for
// games that have them (not FireRed / LeafGreen), the exports only with a
// selection, and the "go to" and "leave" items of the mode - save + bank or
// dual bank. The actions are the old ones, unchanged.

#include "ui.h"
#include "ui_util.h"
#include "i18n.h"
#include "led.h"
#include <algorithm>
#include <cstdio>

namespace {

// Prefixed: these functions are UI members, and an unprefixed FOOTER_H (for
// one) would silently be UI::FOOTER_H instead.
constexpr int MN_MENU_W = 880;
constexpr int MN_HEADER_H = 72;
constexpr int MN_FOOTER_H = 58;
constexpr int MN_COL_L_X = 30, MN_COL_L_W = 410;   // relative to the card
constexpr int MN_COL_R_X = 474, MN_COL_R_W = 376;
constexpr int MN_ROW_H = 58, MN_ROW_STEP = 64;
constexpr int MN_TAG_H = 28;

} // anonymous namespace

SDL_Texture* UI::uiIcon(const std::string& name) {
    auto it = uiIcons_.find(name);
    if (it != uiIcons_.end()) return it->second;
    SDL_Texture* tex = IMG_LoadTexture(renderer_, ("romfs:/icons/" + name + ".png").c_str());
    uiIcons_[name] = tex;   // cached even when missing
    return tex;
}

std::vector<UI::MenuItem> UI::menuItems() const {
    std::vector<MenuItem> items;
    const bool dual = isDualBankMode();
    // Left: tools, then which bank.
    items.push_back({MenuId::Search, 0});
    if (gameInfo(selectedGame_).hasWondercards)
        items.push_back({MenuId::Wondercard, 0});
    if (!selectedSlots_.empty()) {
        items.push_back({MenuId::ExportPk, 0});
        items.push_back({MenuId::ExportCards, 0});
    }
    items.push_back({MenuId::ImportCard, 0});
    if (dual) {
        items.push_back({MenuId::SwitchLeft, 0});
        items.push_back({MenuId::SwitchRight, 0});
    } else {
        items.push_back({MenuId::SwitchBank, 0});
    }
    // Right: settings, then every way out, from the safest to the most final:
    // save, change game (saves), change game without saving, quit without
    // saving. Theme is shown even with a single theme, so the row is in place
    // as more are added.
    items.push_back({MenuId::Theme, 1});
    items.push_back({MenuId::Language, 1});
    items.push_back({dual ? MenuId::SaveBanks : MenuId::SaveQuit, 1});
    items.push_back({MenuId::ChangeGame, 1});
    items.push_back({MenuId::ChangeGameNoSave, 1});
    items.push_back({MenuId::QuitNoSave, 1});
    return items;
}

// "Discard your changes?" when leaving without saving would drop some; true
// when there are none, or when confirmed. Cancelling leaves the menu open.
bool UI::confirmDiscard() {
    if (!unsavedChanges_) return true;
    ConfirmStyle st;
    st.warning = true;
    st.confirmKey = StrKey::DlgDiscard;
    showMenu_ = false;
    const bool ok = showConfirmDialog(i18n::get(StrKey::DlgDiscardTitle),
        i18n::get(isDualBankMode() ? StrKey::DlgDiscardBodyBanks : StrKey::DlgDiscardBodySave), st);
    if (!ok) showMenu_ = true;   // back to the menu, as it was
    return ok;
}

// "Quit pkHouse?" before closing: PLUS opens the menu on some screens and
// quits on others, so one press in the wrong place must not end the session.
// `saving` says the changes are written first (Save & quit).
bool UI::confirmQuit(bool saving) {
    ConfirmStyle st;
    st.confirmKey = StrKey::MenuQuit;
    return showConfirmDialog(i18n::get(StrKey::DlgQuitTitle),
                             i18n::get(saving ? StrKey::DlgQuitSaveBody : StrKey::DlgQuitBody), st);
}

// The next or previous theme, straight away and saved. Shared by the menu's
// Theme row and L / R in About.
void UI::stepTheme(int dir) {
    const int next = (themeIndex_ + dir + THEME_COUNT) % THEME_COUNT;
    if (next == themeIndex_) return;   // one theme: nothing to change or save
    themeIndex_ = next;
    theme_ = &getTheme(themeIndex_);   // the text cache follows (see run())
    saveThemeIndex(basePath_, themeIndex_);
    // A dialog's dimmed backdrop is cached per frame; the one behind About
    // has to be painted again, in the new colours.
    frameGen_++;
    markDirty();
}

void UI::openMenu() {
    showMenu_ = true;
    menuSelection_ = 0;
}

void UI::moveMenuCursor(int dx, int dy) {
    const auto items = menuItems();
    if (items.empty()) return;
    menuSelection_ = std::clamp(menuSelection_, 0, (int)items.size() - 1);
    const MenuItem cur = items[menuSelection_];

    // Left / right switch the language or the theme at once - the menu
    // redraws in it - and keep the choice...
    if (dx != 0 && cur.id == MenuId::Language) {
        const auto langs = i18n::availableLangs();
        if (langs.empty()) return;
        int i = 0;
        for (int k = 0; k < (int)langs.size(); k++)
            if (langs[k] == i18n::currentLang()) { i = k; break; }
        const std::string newLang = langs[(i + dx + (int)langs.size()) % (int)langs.size()];
        i18n::init(newLang);
        clearTextCache();
        // Persist choice: a few bytes, once per press.
        std::string path = basePath_ + "language.txt";
        FILE* f = std::fopen(path.c_str(), "w");
        if (f) { std::fputs(newLang.c_str(), f); std::fclose(f); }
        return;
    }
    if (dx != 0 && cur.id == MenuId::Theme) {
        stepTheme(dx);
        return;
    }

    // ...otherwise they move between the columns, to the same row or the
    // last one there.
    std::vector<int> col[2];
    for (int i = 0; i < (int)items.size(); i++) col[items[i].column].push_back(i);
    const int c = cur.column;
    const int pos = static_cast<int>(std::find(col[c].begin(), col[c].end(), menuSelection_) - col[c].begin());
    if (dx != 0) {
        const int other = 1 - c;
        if (col[other].empty()) return;
        menuSelection_ = col[other][std::min(pos, (int)col[other].size() - 1)];
    } else if (dy != 0) {
        const int n = static_cast<int>(col[c].size());
        menuSelection_ = col[c][(pos + dy + n) % n];
    }
}

void UI::handleMenuInput(const SDL_Event& event, bool& running) {
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
            int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
            updateStick(lx, ly);
        }
    }
    if (event.type != SDL_CONTROLLERBUTTONDOWN) return;
    switch (event.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    moveMenuCursor(0, -1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  moveMenuCursor(0, +1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  moveMenuCursor(-1, 0); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: moveMenuCursor(+1, 0); break;
        case SDL_CONTROLLER_BUTTON_B: { // Switch A = confirm
            const auto items = menuItems();
            if (menuSelection_ >= 0 && menuSelection_ < (int)items.size())
                menuActivate(items[menuSelection_].id, running);
            break;
        }
        case SDL_CONTROLLER_BUTTON_A:      // Switch B = close
        case SDL_CONTROLLER_BUTTON_START:  // + closes it too, as it opened it
            showMenu_ = false;
            break;
    }
}

// The old menu's actions, one per item.
void UI::menuActivate(MenuId id, bool& running) {
    switch (id) {
    case MenuId::Theme:
    case MenuId::Language:
        // Nothing to confirm: left / right already switched it.
        return;
    case MenuId::Search:
        showMenu_ = false;
        showSearchFilter_ = true;
        searchFilterCursor_ = 0;
        searchFilterCol_ = 0;
        searchFilter_ = SearchFilter{};
        clearSearchHighlight();
        return;
    case MenuId::Wondercard:
        showMenu_ = false;
        wcList_ = scanWondercards(basePath_, selectedGame_);
        wcListCursor_ = 0;
        wcListScroll_ = 0;
        showWondercardList_ = true;
        return;
    case MenuId::ExportPk: {
        showMenu_ = false;
        int exported = 0;
        int failed = 0;
        for (int slot : selectedSlots_) {
            Pokemon pkm = getPokemonAt(selectedBox_, slot, selectedPanel_);
            if (!pkm.isEmpty()) {
                std::string name = exportPokemon(pkm);
                if (!name.empty()) exported++;
                else failed++;
            }
        }
        std::string body = i18n::fmt(StrKey::PokemonExported, std::to_string(exported));
        if (failed > 0) body += "\n" + i18n::fmt(StrKey::ExportFailedCount, std::to_string(failed));
        showMessageAndWait(i18n::get(StrKey::ExportComplete), body, DialogKind::Success);
        return;
    }
    case MenuId::ExportCards: {
        // The same selection, written as PNG cards instead.
        showMenu_ = false;
        int total = 0;
        for (int slot : selectedSlots_)
            if (!getPokemonAt(selectedBox_, slot, selectedPanel_).isEmpty())
                total++;

        // Open the card fonts once for the whole run rather than per card.
        CardFonts fonts;
        if (!openCardFonts(fonts)) {
            showMessageAndWait(i18n::get(StrKey::ExportFailed),
                               i18n::get(StrKey::CouldNotWrite));
            return;
        }
        int exported = 0, failed = 0, done = 0;
        for (int slot : selectedSlots_) {
            Pokemon pkm = getPokemonAt(selectedBox_, slot, selectedPanel_);
            if (pkm.isEmpty()) continue;
            done++;
            showWorking(i18n::fmt(StrKey::SavingCards,
                                  std::to_string(done), std::to_string(total)),
                        true, total ? static_cast<float>(done - 1) / total : 0.0f);
            if (!renderPokemonCard(pkm, fonts).empty()) exported++;
            else                                        failed++;
        }
        closeCardFonts(fonts);

        std::string body = i18n::fmt(StrKey::CardsExported, std::to_string(exported));
        if (failed > 0) body += "\n" + i18n::fmt(StrKey::ExportFailedCount, std::to_string(failed));
        showMessageAndWait(i18n::get(StrKey::ExportComplete), body, DialogKind::Success);
        return;
    }
    case MenuId::ImportCard:
        showMenu_ = false;
        cardList_ = scanCards(basePath_, selectedGame_);
        cardListCursor_ = 0;
        cardListScroll_ = 0;
        freeCardPreview();
        cardPreviewSince_ = SDL_GetTicks();
        showCardList_ = true;
        return;

    case MenuId::SwitchLeft:
    case MenuId::SwitchRight: {
        // Need at least 1 bank available that isn't loaded on the other side
        const bool left = id == MenuId::SwitchLeft;
        const std::string& other = left ? activeBankName_ : leftBankName_;
        int avail = 0;
        for (auto& b : bankManager_.list())
            if (b.name != other) avail++;
        if (avail == 0) {
            showMenu_ = false;
            ConfirmStyle st;
            st.confirmKey = StrKey::DlgCreateBank;
            if (!showConfirmDialog(i18n::get(StrKey::NoBanksAvailable),
                    i18n::get(StrKey::CreateNewBank), st)) return;
            if (!saveBankFiles()) return;
            bankManager_.refresh(bankListProgress());
            bankSelTarget_ = left ? Panel::Game : Panel::Bank;
            screen_ = AppScreen::BankSelector;
            beginTextInput(TextInputPurpose::CreateBank);
            return;
        }
        if (!saveBankFiles()) { showMenu_ = false; return; }
        bankManager_.refresh(bankListProgress());
        bankSelTarget_ = left ? Panel::Game : Panel::Bank;
        screen_ = AppScreen::BankSelector;
        showMenu_ = false;
        return;
    }
    case MenuId::SwitchBank:
        if (!saveBankFiles()) { showMenu_ = false; return; }
        showWorking(i18n::get(StrKey::Saving), true);
        ledBlink();
        if (save_.isLoaded())
            save_.save(savePath_);
        account_.commitSave();
        ledOff();
        bankManager_.refresh(bankListProgress());
        screen_ = AppScreen::BankSelector;
        showMenu_ = false;
        return;
    case MenuId::ChangeGame:
        if (isDualBankMode()) {
            // Change Game — save banks and return to game selector
            if (!saveBankFiles()) { showMenu_ = false; return; }
            leftBankName_.clear();
            leftBankPath_.clear();
        } else {
            // Change Game — save everything, unmount, go to game selector
            if (!saveBankFiles()) { showMenu_ = false; return; }
            showWorking(i18n::get(StrKey::Saving), true);
            ledBlink();
            if (save_.isLoaded())
                save_.save(savePath_);
            account_.commitSave();
            ledOff();
            account_.unmountSave();
        }
        activeBankName_.clear();
        activeBankPath_.clear();
        allBanksMode_ = false;
        gameSelOnAllBanks_ = false;
        refreshBankCounts();   // new banks, and the backup just made
        screen_ = AppScreen::GameSelector;
        showMenu_ = false;
        return;

    case MenuId::SaveBanks:
        saveBankFiles();
        showMenu_ = false;
        return;
    case MenuId::SaveQuit:
        showMenu_ = false;
        if (!confirmQuit(true)) { showMenu_ = true; return; }
        saveNow_ = true;
        running = false;
        return;
    case MenuId::QuitNoSave:
        // Quitting drops unsaved changes: say so first when there are any;
        // that question is the confirmation, so it is not asked twice.
        if (unsavedChanges_) {
            if (!confirmDiscard()) return;
        } else {
            showMenu_ = false;
            if (!confirmQuit(false)) { showMenu_ = true; return; }
        }
        running = false;
        return;

    case MenuId::ChangeGameNoSave:
        // Back to the games without writing anything: the save and the banks
        // stay as they are on the SD card, and what was changed in memory is
        // dropped. Asked first only when there is something to drop.
        if (!confirmDiscard()) return;
        if (isDualBankMode()) {
            leftBankName_.clear();
            leftBankPath_.clear();
        } else {
            account_.unmountSave();   // never committed: nothing was written
        }
        activeBankName_.clear();
        activeBankPath_.clear();
        allBanksMode_ = false;
        gameSelOnAllBanks_ = false;
        unsavedChanges_ = false;
        refreshBankCounts();
        screen_ = AppScreen::GameSelector;
        showMenu_ = false;
        return;
    }
}

// --- Drawing -------------------------------------------------------------------

void UI::drawMenuPopup() {
    // Over the box view it opened from, dimmed.
    drawRect(0, 0, SCREEN_W, SCREEN_H, SDL_Color{T().bg.r, T().bg.g, T().bg.b, 190});

    const auto items = menuItems();
    menuSelection_ = std::clamp(menuSelection_, 0, (int)items.size() - 1);
    const MenuId curId = items[menuSelection_].id;
    const bool dual = isDualBankMode();

    // Heights first: the card is as tall as its longer column.
    auto rowH = [&](MenuId id) {
        switch (id) {
            case MenuId::Theme:      return 100;
            case MenuId::Language:   return 50;
            case MenuId::SaveQuit:
            case MenuId::SaveBanks:  return 52;
            case MenuId::ChangeGame:
            case MenuId::ChangeGameNoSave:
            case MenuId::QuitNoSave:       return 46;
            default:                 return MN_ROW_H;
        }
    };
    auto isGoTo = [](MenuId id) {
        return id == MenuId::SwitchBank || id == MenuId::SwitchLeft ||
               id == MenuId::SwitchRight;
    };
    auto isLeave = [](MenuId id) {
        return id == MenuId::SaveQuit || id == MenuId::SaveBanks || id == MenuId::ChangeGame ||
               id == MenuId::ChangeGameNoSave || id == MenuId::QuitNoSave;
    };

    // The note that goes with the save button, measured first so the layout
    // can leave it room right under that button.
    const std::string note = dual
        ? i18n::get(unsavedChanges_ ? StrKey::MenuNoteDualDirty : StrKey::MenuNoteDualClean)
        : i18n::get(StrKey::MenuNoteSave);
    TTF_Font* fNote = uiFont(12);
    const auto noteLines = wrapText(note, fNote, MN_COL_R_W, 2);
    const int noteH = static_cast<int>(noteLines.size()) * 16;
    int noteY = -1;

    // Lay the rows out, recording each one's place.
    struct Placed { MenuId id; int x, y, w, h; };
    std::vector<Placed> placed;
    struct Tag { int x, y; const char* key; };
    std::vector<Tag> tags;
    int colBottom[2] = {0, 0};
    for (int column = 0; column < 2; column++) {
        int yy = MN_HEADER_H + 20;
        bool inSecond = false, first = true;
        const int cx = column == 0 ? MN_COL_L_X : MN_COL_R_X, cw = column == 0 ? MN_COL_L_W : MN_COL_R_W;
        for (const auto& it : items) {
            if (it.column != column) continue;
            const bool second = column == 0 ? isGoTo(it.id) : isLeave(it.id);
            if (first || (second && !inSecond)) {
                if (!first) yy += 12;
                tags.push_back({cx, yy, column == 0 ? (second ? StrKey::MenuGoTo : StrKey::MenuTools)
                                                    : (second ? StrKey::MenuLeave : StrKey::MenuSettings)});
                yy += MN_TAG_H;
                inSecond = second;
                first = false;
            }
            const int h = rowH(it.id);
            placed.push_back({it.id, cx, yy, cw, h});
            yy += (h == MN_ROW_H ? MN_ROW_STEP : h + 8);
            if (it.id == MenuId::SaveQuit || it.id == MenuId::SaveBanks) {
                noteY = yy - 2;
                yy += noteH + 8;
            }
        }
        colBottom[column] = yy;
    }

    const int H = std::max(colBottom[0], colBottom[1]) + 10 + MN_FOOTER_H;
    const int x = (SCREEN_W - MN_MENU_W) / 2, y = (SCREEN_H - H) / 2;
    fillRounded(x, y, MN_MENU_W, H, 22, T().panelBg);
    strokeRounded(x, y, MN_MENU_W, H, 22, 1, T().panelBorder);

    // --- Header: title, what is open, and whether it is saved ---------------------
    {
        TTF_Font* fTitle = uiFont(26, true);
        TTF_Font* fSub   = uiFont(15);
        const int base = y + 46;
        drawText(i18n::get(StrKey::MenuTitle), x + 26, base - TTF_FontAscent(fTitle), T().text, fTitle);
        const int tw = textWidth(i18n::get(StrKey::MenuTitle), fTitle);

        TTF_Font* fChip = uiFont(13, true);
        const std::string status = unsavedChanges_ ? i18n::get(StrKey::StatusUnsaved)
                                 : dual ? i18n::get(StrKey::StatusBanksClean)
                                        : i18n::get(StrKey::StatusSaveClean);
        const SDL_Color sc = unsavedChanges_ ? T().accent : T().statusOk;
        const int cw = 14 + 8 + 8 + textWidth(status, fChip) + 14;
        const int chx = x + MN_MENU_W - 26 - cw, chy = y + 22;
        fillRounded(chx, chy, cw, 32, 9, unsavedChanges_ ? T().badgeBg : SDL_Color{sc.r, sc.g, sc.b, 36});
        fillDisc(chx + 14, chy + 16, 4, sc);
        drawText(status, chx + 14 + 8 + 8, chy + 16 - TTF_FontHeight(fChip) / 2, sc, fChip);

        std::string sub = gameDisplayNameOf(selectedGame_);
        if (allBanksMode_)                sub += " \xc2\xb7 " + i18n::get(StrKey::AllBanks);
        else if (!activeBankName_.empty()) sub += " \xc2\xb7 " + activeBankName_;
        const int subX = x + 26 + tw + 14;
        drawText(fitText(sub, fSub, chx - 16 - subX), subX, base - TTF_FontAscent(fSub), T().textDim, fSub);
    }
    drawRect(x, y + MN_HEADER_H, MN_MENU_W, 1, T().panelBorder);

    TTF_Font* fTag = uiFont(11, true);
    for (const auto& t : tags)
        drawTextTracked(toUpperUtf8(i18n::get(t.key)), x + t.x, y + t.y + 4, T().accentBank, fTag, 2);

    // --- Rows ---------------------------------------------------------------------
    TTF_Font* fName  = uiFont(17, true);
    TTF_Font* fDesc  = uiFont(12);
    TTF_Font* fValue = uiFont(14);
    auto icon = [&](const char* name, int ix, int icy, SDL_Color c) {
        if (SDL_Texture* t = uiIcon(name)) {
            SDL_SetTextureColorMod(t, c.r, c.g, c.b);
            SDL_Rect dst = {ix, icy - 10, 20, 20};
            SDL_RenderCopy(renderer_, t, nullptr, &dst);
            SDL_SetTextureColorMod(t, 255, 255, 255);
        }
    };
    for (const auto& p : placed) {
        const int rx = x + p.x, ry = y + p.y, rcy = ry + p.h / 2;
        const bool cur = p.id == curId;

        if (isLeave(p.id)) {
            // Buttons: the saving one in the accent, change game (which also
            // saves) plain, the two that drop changes red.
            const bool primary = p.id == MenuId::SaveQuit || p.id == MenuId::SaveBanks;
            const bool danger  = p.id == MenuId::QuitNoSave || p.id == MenuId::ChangeGameNoSave;
            if (cur) strokeRounded(rx - 6, ry - 6, p.w + 12, p.h + 12, 18, 3, T().accent);
            const SDL_Color red = T().red;
            if (primary) {
                fillRounded(rx, ry, p.w, p.h, 12, T().accent);
            } else {
                fillRounded(rx, ry, p.w, p.h, 12, danger ? SDL_Color{red.r, red.g, red.b, 30} : T().bg);
                strokeRounded(rx, ry, p.w, p.h, 12, 1, danger ? SDL_Color{red.r, red.g, red.b, 150} : T().buttonBorder);
            }
            const char* key = p.id == MenuId::SaveQuit ? StrKey::MenuSaveQuit
                            : p.id == MenuId::SaveBanks ? StrKey::MenuSaveBanks
                            : p.id == MenuId::ChangeGame ? StrKey::MenuChangeGame
                            : p.id == MenuId::ChangeGameNoSave ? StrKey::MenuChangeGameNoSave
                                                               : StrKey::MenuQuitNoSave;
            TTF_Font* f = uiFont(primary ? 17 : 15, true);
            const std::string label = fitText(i18n::get(key), f, p.w - 60);
            const SDL_Color tc = primary ? T().keyCapText : danger ? red : T().text;
            const int lw = textWidth(label, f) + (primary ? 28 : 0);
            int lx = rx + (p.w - lw) / 2;
            if (primary) { icon("save", lx, rcy, tc); lx += 28; }
            drawText(label, lx, rcy - TTF_FontHeight(f) / 2, tc, f);
            continue;
        }

        if (cur) {
            strokeRounded(rx - 6, ry - 6, p.w + 12, p.h + 12, 18, 3, T().accent);
            fillRounded(rx, ry, p.w, p.h, 12, T().slotFull);
        }

        if (p.id == MenuId::Language) {
            icon("globe", rx + 14, rcy, T().text);
            drawText(i18n::get(StrKey::MenuLanguage), rx + 46, rcy - TTF_FontHeight(fName) / 2, T().text, fName);
            // ◀ name ▶: the language in use, switched as you go.
            const std::string code = i18n::currentLang();
            TTF_Font* fL = uiFont(14, true);
            const int bw = 32, bh = 30, right = rx + p.w - 14;
            const int nameW = 110;
            const int b2 = right - bw, b1 = b2 - nameW - bw;
            for (int side = 0; side < 2; side++) {
                const int bx = side ? b2 : b1;
                fillRounded(bx, rcy - bh / 2, bw, bh, 8, T().buttonBg);
                strokeRounded(bx, rcy - bh / 2, bw, bh, 8, 1, T().buttonBorder);
                fillArrow(bx + bw / 2.0f, rcy, 8, side ? ArrowDir::Right : ArrowDir::Left, T().text);
            }
            drawTextCentered(fitText(langDisplayName(code), fL, nameW - 8), b1 + bw + nameW / 2, rcy, T().text, fL);
            continue;
        }

        if (p.id == MenuId::Theme) {
            const int top = ry + 12;
            icon("palette", rx + 14, top + 12, T().text);
            drawText(i18n::get(StrKey::MenuTheme), rx + 46, top + 12 - TTF_FontHeight(fName) / 2, T().text, fName);
            const int shown = themeIndex_;
            const std::string tname = getThemeName(shown);
            TTF_Font* fT = uiFont(14, true);
            drawText(tname, rx + p.w - 14 - textWidth(tname, fT), top + 12 - TTF_FontHeight(fT) / 2, T().text, fT);
            // One swatch per theme, in its accent; the one shown ringed.
            const int sy = ry + 66;
            int sx = rx + 56;
            for (int t = 0; t < THEME_COUNT; t++, sx += 42) {
                const Theme& th = getTheme(t);
                fillDisc(sx + 16, sy, 16, th.panelBg);
                fillDisc(sx + 16, sy, 7, th.accent);
                if (t == shown) strokeRounded(sx - 2, sy - 18, 36, 36, 18, 3, T().text);
            }
            continue;
        }

        // Tools and destinations.
        const char* iconName = "search";
        const char* titleKey = StrKey::MenuSearch;
        const char* descKey = nullptr;
        std::string title, value, badge;
        switch (p.id) {
            case MenuId::Search:      iconName = "search"; descKey = StrKey::MenuDescSearch; break;
            case MenuId::Wondercard:  iconName = "gift";   titleKey = StrKey::MenuWondercard; descKey = StrKey::MenuDescWondercard; break;
            case MenuId::ExportPk:    iconName = "export"; descKey = StrKey::MenuDescExportPk;
                title = i18n::fmt(StrKey::MenuExportSelected, std::to_string(selectedSlots_.size())); break;
            case MenuId::ExportCards: iconName = "card";   descKey = StrKey::MenuDescExportCards;
                title = i18n::fmt(StrKey::MenuExportCards, std::to_string(selectedSlots_.size())); break;
            case MenuId::ImportCard:  iconName = "import"; titleKey = StrKey::MenuImportCard; descKey = StrKey::MenuDescImport; break;
            case MenuId::SwitchBank:  iconName = "bank";   titleKey = StrKey::MenuSwitchBank; value = activeBankName_; break;
            case MenuId::SwitchLeft:  iconName = "bank";   titleKey = StrKey::MenuSwitchBank; value = leftBankName_;
                badge = i18n::get(StrKey::BsTagLeft); break;
            case MenuId::SwitchRight: iconName = "bank";   titleKey = StrKey::MenuSwitchBank; value = activeBankName_;
                badge = i18n::get(StrKey::BsTagRight); break;
            default: break;
        }
        if (title.empty()) title = i18n::get(titleKey);
        if (SDL_Texture* t = uiIcon(iconName)) {
            SDL_SetTextureColorMod(t, T().text.r, T().text.g, T().text.b);
            SDL_Rect dst = {rx + 14, rcy - 10, 20, 20};
            SDL_RenderCopy(renderer_, t, nullptr, &dst);
            SDL_SetTextureColorMod(t, 255, 255, 255);
        }
        // Right: a value, then a chevron.
        const int chevX = rx + p.w - 16;
        fillArrow(static_cast<float>(chevX), rcy, 6, ArrowDir::Right, T().textDim);
        int valueW = 0;
        if (!value.empty()) {
            const std::string v = fitText(value, fValue, 150);
            valueW = textWidth(v, fValue);
            drawText(v, chevX - 14 - valueW, rcy - TTF_FontHeight(fValue) / 2, T().textDim, fValue);
            valueW += 14;
        }
        const int tx = rx + 46, room = chevX - 14 - valueW - tx;
        const int nameY = descKey ? ry + 9 : rcy - TTF_FontHeight(fName) / 2;
        int badgeW = 0;
        if (!badge.empty()) {
            TTF_Font* fB = uiFont(11, true);
            badgeW = measureTextTracked(badge, fB, 2) + 16 + 10;
        }
        const std::string t = fitText(title, fName, room - badgeW);
        drawText(t, tx, nameY, T().text, fName);
        if (!badge.empty()) {
            // Centred on the capitals of the title, not on its line box: the
            // box keeps room for descenders, so its middle sits below the
            // letters. Capitals are about 0.72 of the font size tall.
            TTF_Font* fB = uiFont(11, true);
            const SDL_Color teal = T().accentBank;
            const int bx = tx + textWidth(t, fName) + 10, bw = badgeW - 10, bh = 20;
            const int capMid = nameY + TTF_FontAscent(fName) - (17 * 72 / 100) / 2;
            fillRounded(bx, capMid - bh / 2, bw, bh, 6, SDL_Color{teal.r, teal.g, teal.b, 40});
            const int badgeBase = capMid + (11 * 72 / 100) / 2;
            drawTextTracked(badge, bx + 8, badgeBase - TTF_FontAscent(fB), teal, fB, 2);
        }
        if (descKey)
            drawText(fitText(i18n::get(descKey), fDesc, room), tx, ry + 33, T().textDim, fDesc);
    }

    // Note under the save button.
    if (noteY >= 0) {
        int ny = y + noteY;
        for (const auto& l : noteLines) {
            drawTextCentered(l, x + MN_COL_R_X + MN_COL_R_W / 2, ny + 8, T().textMuted, fNote);
            ny += 16;
        }
    }

    // --- Footer: what the keys do here ------------------------------------------------
    {
        const int fy = y + H - MN_FOOTER_H;
        drawRect(x, fy, MN_MENU_W, 1, T().panelBorder);
        const int cy = fy + MN_FOOTER_H / 2;
        TTF_Font* f = uiFont(15, true);
        // On Language and Theme left / right is all there is.
        struct Hint { const char* keys; const char* label; };
        std::vector<Hint> hints;
        if (curId == MenuId::Language || curId == MenuId::Theme) {
            hints.push_back({HINT_DPAD, StrKey::HintChange});
        } else {
            hints.push_back({"A", StrKey::HintSelect2});
        }
        hints.push_back({"B +", StrKey::HintClose});
        int hx = x + 26;
        for (const auto& h : hints) {
            hx += drawFooterKey(hx, cy, h.keys, false) + 8;
            const std::string& l = i18n::get(h.label);
            drawText(l, hx, cy - TTF_FontHeight(f) / 2, T().text, f);
            hx += textWidth(l, f) + 22;
        }
    }
}
