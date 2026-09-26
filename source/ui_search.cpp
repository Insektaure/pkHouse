// Search (UI 2.0).
//
// Every filter the old popup had is here: species, OT name, shiny, egg, alpha
// (games with alphas), gender, level range, perfect IVs, ribbons / marks, and
// list or highlight. Perfect IVs gains the steps between the old "1+" and
// "6IV". Matching itself (matchesSearchFilter, executeSearch) is unchanged.

#include "ui.h"
#include "ui_util.h"
#include "i18n.h"
#include "species_converter.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {

// Prefixed: these functions are UI members, where an unprefixed name could
// silently resolve to a UI constant instead.
constexpr int SF_W = 960;
constexpr int SF_HEADER_H = 66;
constexpr int SF_ROW_H = 52, SF_ROW_STEP = 64;
constexpr int SF_COL_W = 452;
constexpr int SF_LABEL_W = 150;
constexpr int SF_BAR_H = 76;          // filters + Reset / Search
constexpr int SF_FOOT_H = 50;

constexpr int SP_W = 1120;
constexpr int SP_CARD_W = 260, SP_CARD_H = 60, SP_PITCH_X = 270, SP_PITCH_Y = 70;

constexpr int SR_W = 1000;
constexpr int SR_ROW_H = 64, SR_PITCH = 72;

} // anonymous namespace

// --- Filter rows -------------------------------------------------------------------

std::vector<int> UI::searchRows() const {
    std::vector<int> rows = {SR_SPECIES, SR_OT, SR_GENDER, SR_LEVEL, SR_SHINY, SR_EGG};
    if (gameInfo(selectedGame_).hasAlphaForms) rows.push_back(SR_ALPHA);
    rows.push_back(SR_IVS);
    rows.push_back(SR_RIBBONS);
    rows.push_back(SR_MODE);
    return rows;
}

// The popup is two columns over a full-width "show results" row, and the
// D-pad walks it as drawn: up / down within a column (wrapping through the
// wide row), left / right between the columns. Values change only with A
// (and ZL / ZR on the level steppers), so the D-pad never edits anything.
void UI::searchFilterColumns(std::vector<int>& left, std::vector<int>& right) const {
    left.clear();
    right.clear();
    for (int r : searchRows()) {
        if (r == SR_MODE) continue;
        (r <= SR_LEVEL ? left : right).push_back(r);
    }
}

void UI::setSearchFilterRow(int row) {
    const auto rows = searchRows();
    for (size_t i = 0; i < rows.size(); i++)
        if (rows[i] == row) { searchFilterCursor_ = static_cast<int>(i); return; }
}

void UI::moveSearchFilterCursor(int dir) {
    const auto rows = searchRows();
    const int row = rows[std::clamp(searchFilterCursor_, 0, (int)rows.size() - 1)];
    std::vector<int> left, right;
    searchFilterColumns(left, right);
    if (row == SR_MODE) {
        // Back into the column it was left from.
        const auto& col = searchFilterCol_ == 0 ? left : right;
        setSearchFilterRow(dir > 0 ? col.front() : col.back());
        return;
    }
    searchFilterCol_ = row <= SR_LEVEL ? 0 : 1;
    const auto& col = searchFilterCol_ == 0 ? left : right;
    const int i = static_cast<int>(std::find(col.begin(), col.end(), row) - col.begin());
    const int next = i + dir;
    setSearchFilterRow(next < 0 || next >= (int)col.size() ? SR_MODE : col[next]);
}

void UI::switchSearchFilterColumn(int dir) {
    const auto rows = searchRows();
    const int row = rows[std::clamp(searchFilterCursor_, 0, (int)rows.size() - 1)];
    if (row == SR_MODE) return;   // one wide row, nothing beside it
    const int colNow = row <= SR_LEVEL ? 0 : 1;
    const int colNext = std::clamp(colNow + dir, 0, 1);
    if (colNext == colNow) return;
    std::vector<int> left, right;
    searchFilterColumns(left, right);
    const auto& from = colNow == 0 ? left : right;
    const auto& to = colNext == 0 ? left : right;
    const int i = static_cast<int>(std::find(from.begin(), from.end(), row) - from.begin());
    setSearchFilterRow(to[std::min(i, (int)to.size() - 1)]);
    searchFilterCol_ = colNext;
}

void UI::adjustSearchRow(int dir) {
    const auto rows = searchRows();
    const int row = rows[std::clamp(searchFilterCursor_, 0, (int)rows.size() - 1)];
    auto step = [&](int value, int count) { return (value + dir + count) % count; };
    switch (row) {
        case SR_GENDER:
            searchFilter_.gender = static_cast<GenderFilter>(step(static_cast<int>(searchFilter_.gender), 4));
            break;
        case SR_LEVEL: {
            // The focused end moves by one; the two ends never cross.
            int lo = searchFilter_.levelMin > 0 ? searchFilter_.levelMin : 1;
            int hi = searchFilter_.levelMax > 0 ? searchFilter_.levelMax : 100;
            if (searchLevelFocus_ == 0) lo = std::clamp(lo + dir, 1, hi);
            else                        hi = std::clamp(hi + dir, lo, 100);
            // 1 and 100 are "no bound", as the old empty fields were.
            searchFilter_.levelMin = lo > 1 ? lo : 0;
            searchFilter_.levelMax = hi < 100 ? hi : 0;
            break;
        }
        case SR_SHINY:   searchFilter_.filterShiny = !searchFilter_.filterShiny; break;
        case SR_EGG:     searchFilter_.filterEgg = !searchFilter_.filterEgg; break;
        case SR_ALPHA:   searchFilter_.filterAlpha = !searchFilter_.filterAlpha; break;
        case SR_IVS:     searchFilter_.minPerfectIVs = step(searchFilter_.minPerfectIVs, 7); break;
        case SR_RIBBONS:
            searchFilter_.ribbonFilter = static_cast<RibbonFilter>(step(static_cast<int>(searchFilter_.ribbonFilter), 4));
            break;
        case SR_MODE:
            searchFilter_.mode = searchFilter_.mode == SearchMode::List ? SearchMode::Highlight : SearchMode::List;
            break;
        default: break;   // species and OT open something instead
    }
}

void UI::activateSearchRow() {
    const auto rows = searchRows();
    const int row = rows[std::clamp(searchFilterCursor_, 0, (int)rows.size() - 1)];
    switch (row) {
        case SR_SPECIES:
            if (speciesPickerForGts_) {
                // The list is built per target, so a stale one from the GTS
                // would offer species this game has never seen.
                speciesPickerForGts_ = false;
                availableSpecies_.clear();
            }
            if (availableSpecies_.empty())
                buildAvailableSpeciesList();
            speciesLetterCursor_ = 0;
            speciesLetterScroll_ = 0;
            showSpeciesLetterPicker_ = true;
            break;
        case SR_OT: beginTextInput(TextInputPurpose::SearchOT); break;
        case SR_LEVEL:
            beginTextInput(searchLevelFocus_ == 0 ? TextInputPurpose::SearchLevelMin
                                                  : TextInputPurpose::SearchLevelMax);
            break;
        default: adjustSearchRow(+1); break;   // A steps a choice, as it did
    }
}

void UI::handleSearchFilterInput(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
            int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
            updateStick(lx, ly);
        }
        // ZL / ZR step the focused end of the level range. Edge-triggered.
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
            const bool isLeft = event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT;
            const bool pressed = event.caxis.value > TRIGGER_DEADZONE;
            bool& was = isLeft ? zlPressed_ : zrPressed_;
            if (pressed && !was) {
                const auto rows = searchRows();
                if (rows[std::clamp(searchFilterCursor_, 0, (int)rows.size() - 1)] == SR_LEVEL) {
                    adjustSearchRow(isLeft ? -1 : +1);
                    markDirty();
                }
            }
            was = pressed;
        }
        return;
    }
    if (event.type != SDL_CONTROLLERBUTTONDOWN) return;
    const auto rows = searchRows();
    const bool onLevel = rows[std::clamp(searchFilterCursor_, 0, (int)rows.size() - 1)] == SR_LEVEL;
    switch (event.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    moveSearchFilterCursor(-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  moveSearchFilterCursor(+1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  switchSearchFilterColumn(-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: switchSearchFilterColumn(+1); break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  if (onLevel) searchLevelFocus_ = 0; break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: if (onLevel) searchLevelFocus_ = 1; break;
        case SDL_CONTROLLER_BUTTON_B: activateSearchRow(); break;      // Switch A
        case SDL_CONTROLLER_BUTTON_Y: searchFilter_ = SearchFilter{}; break;  // Switch X = reset
        case SDL_CONTROLLER_BUTTON_X: executeSearch(); break;          // Switch Y = search
        case SDL_CONTROLLER_BUTTON_A: showSearchFilter_ = false; break; // Switch B = cancel
    }
}

std::vector<std::string> UI::searchChips() const {
    std::vector<std::string> c;
    if (searchFilter_.speciesId > 0 || !searchFilter_.speciesName.empty())
        c.push_back(searchFilter_.speciesName);
    if (!searchFilter_.otName.empty()) c.push_back(i18n::fmt(StrKey::ChipOt, searchFilter_.otName));
    if (searchFilter_.filterShiny) c.push_back(i18n::get(StrKey::LegendShiny));
    if (searchFilter_.filterEgg)   c.push_back(i18n::get(StrKey::Egg));
    if (searchFilter_.filterAlpha) c.push_back(i18n::get(StrKey::LegendAlpha));
    switch (searchFilter_.gender) {
        case GenderFilter::Male:       c.push_back("\xe2\x99\x82"); break;
        case GenderFilter::Female:     c.push_back("\xe2\x99\x80"); break;
        case GenderFilter::Genderless: c.push_back(i18n::get(StrKey::GenderGenderless)); break;
        default: break;
    }
    if (searchFilter_.levelMin > 0 || searchFilter_.levelMax > 0)
        c.push_back(i18n::fmt(StrKey::ChipLv,
            std::to_string(searchFilter_.levelMin > 0 ? searchFilter_.levelMin : 1),
            std::to_string(searchFilter_.levelMax > 0 ? searchFilter_.levelMax : 100)));
    if (searchFilter_.minPerfectIVs > 0)
        c.push_back(i18n::fmt(StrKey::ChipIvs, std::to_string(searchFilter_.minPerfectIVs)));
    switch (searchFilter_.ribbonFilter) {
        case RibbonFilter::HasRibbon: c.push_back(i18n::get(StrKey::SfRibbon)); break;
        case RibbonFilter::HasMark:   c.push_back(i18n::get(StrKey::SfMark)); break;
        case RibbonFilter::HasAny:    c.push_back(i18n::get(StrKey::RibbonHasAny)); break;
        default: break;
    }
    return c;
}

// Chips on one line; what does not fit becomes "+N".
void UI::drawSearchChips(const std::vector<std::string>& chips, int x, int cy, int maxW) {
    TTF_Font* f = uiFont(13, true);
    int cx = x;
    for (size_t i = 0; i < chips.size(); i++) {
        const int w = textWidth(chips[i], f) + 20;
        const std::string more = "+" + std::to_string(chips.size() - i);
        const int moreW = textWidth(more, f) + 20;
        const bool last = i + 1 == chips.size();
        if (cx + w > x + maxW || (!last && cx + w + 8 + moreW > x + maxW)) {
            fillRounded(cx, cy - 13, moreW, 26, 8, T().buttonBg);
            drawTextCentered(more, cx + moreW / 2, cy, T().textDim, f);
            return;
        }
        fillRounded(cx, cy - 13, w, 26, 8, T().badgeBg);
        drawTextCentered(chips[i], cx + w / 2, cy, T().accent, f);
        cx += w + 8;
    }
}

// --- Filter popup (5b) -------------------------------------------------------------

void UI::drawSearchFilterPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, SDL_Color{T().bg.r, T().bg.g, T().bg.b, 190});

    const auto rows = searchRows();
    searchFilterCursor_ = std::clamp(searchFilterCursor_, 0, (int)rows.size() - 1);
    const int curRow = rows[searchFilterCursor_];
    const bool dual = isDualBankMode();

    // Left: species, OT, gender, level. Right: the toggles and choices.
    std::vector<int> left, right;
    searchFilterColumns(left, right);
    const int colRows = std::max(left.size(), right.size());
    const int bodyH = colRows * SF_ROW_STEP + 8 + 60 + 16;
    const int H = SF_HEADER_H + 20 + bodyH + SF_BAR_H + SF_FOOT_H;
    const int x = (SCREEN_W - SF_W) / 2, y = (SCREEN_H - H) / 2;
    fillRounded(x, y, SF_W, H, 22, T().panelBg);
    strokeRounded(x, y, SF_W, H, 22, 1, T().panelBorder);

    // Header
    {
        if (SDL_Texture* ic = uiIcon("search")) {
            SDL_SetTextureColorMod(ic, T().accent.r, T().accent.g, T().accent.b);
            SDL_Rect dst = {x + 26, y + 22, 22, 22};
            SDL_RenderCopy(renderer_, ic, nullptr, &dst);
            SDL_SetTextureColorMod(ic, 255, 255, 255);
        }
        TTF_Font* fT = uiFont(24, true);
        TTF_Font* fS = uiFont(15);
        const std::string title = i18n::get(StrKey::MenuSearch);
        drawText(title, x + 60, y + 33 - TTF_FontHeight(fT) / 2, T().text, fT);
        const std::string scope = i18n::get(dual ? StrKey::SfScopeDual : StrKey::SfScopeSave)
                                + " \xc2\xb7 " + gameDisplayNameOf(selectedGame_);
        const int sx = x + 60 + textWidth(title, fT) + 14;
        drawText(fitText(scope, fS, x + SF_W - 26 - sx), sx, y + 36 - TTF_FontHeight(fS) / 2, T().textDim, fS);
    }
    drawRect(x, y + SF_HEADER_H, SF_W, 1, T().panelBorder);

    TTF_Font* fLabel = uiFont(16, true);
    TTF_Font* fVal   = uiFont(15);
    TTF_Font* fSeg   = uiFont(14, true);

    // Pieces of a row
    auto segmented = [&](int sx, int cy, int w, const std::vector<std::string>& labels, int sel) {
        const int h = 40, n = static_cast<int>(labels.size());
        fillRounded(sx, cy - h / 2, w, h, 10, T().bg);
        const int segW = (w - 8) / n;
        for (int i = 0; i < n; i++) {
            const int bx = sx + 4 + i * segW;
            if (i == sel) fillRounded(bx, cy - h / 2 + 4, segW, h - 8, 8, T().buttonBg);
            drawTextCentered(fitText(labels[i], fSeg, segW - 6), bx + segW / 2, cy,
                             i == sel ? T().text : T().textDim, fSeg);
        }
    };
    auto toggle = [&](int rx, int cy, bool on) {
        const int w = 48, h = 28;
        const int tx = rx - w;
        fillRounded(tx, cy - h / 2, w, h, h / 2, on ? T().accent : T().bg);
        strokeRounded(tx, cy - h / 2, w, h, h / 2, 1, on ? T().accent : T().buttonBorder);
        fillDisc(on ? tx + w - 14 : tx + 14, cy, 10, on ? T().keyCapText : T().textDim);
    };
    auto stepper = [&](int sx, int cy, int value, bool focused) {
        const int bw = 30, bh = 32, vw = 50;
        for (int side = 0; side < 2; side++) {
            const int bx = side ? sx + bw + 6 + vw + 6 : sx;
            fillRounded(bx, cy - bh / 2, bw, bh, 8, T().buttonBg);
            strokeRounded(bx, cy - bh / 2, bw, bh, 8, 1, T().buttonBorder);
            fillArrow(bx + bw / 2.0f, cy, 8, side ? ArrowDir::Right : ArrowDir::Left, T().text);
        }
        const int vx = sx + bw + 6;
        fillRounded(vx, cy - bh / 2, vw, bh, 8, T().bg);
        if (focused) strokeRounded(vx, cy - bh / 2, vw, bh, 8, 2, T().accent);
        drawTextCentered(std::to_string(value), vx + vw / 2, cy, T().text, uiFont(16, true));
        return bw * 2 + vw + 12;
    };

    auto drawRow = [&](int row, int rx, int ry) {
        const int cy = ry + SF_ROW_H / 2;
        const bool cur = row == curRow;
        if (cur) {
            strokeRounded(rx - 6, ry - 6, SF_COL_W + 12, SF_ROW_H + 12, 18, 3, T().accent);
            fillRounded(rx, ry, SF_COL_W, SF_ROW_H, 12, T().slotFull);
        }
        const int cx = rx + SF_LABEL_W + 10, cw = SF_COL_W - SF_LABEL_W - 26;
        const int right = rx + SF_COL_W - 16;
        // Shiny and alpha are flat masks, tinted; the egg is its sprite, as
        // on the summary badges.
        enum class LabelIcon { None, Shiny, Alpha, Egg };
        auto label = [&](const char* key, LabelIcon icon = LabelIcon::None) {
            int lx = rx + 18;
            SDL_Texture* mask = icon == LabelIcon::Shiny ? iconShiny_
                              : icon == LabelIcon::Alpha ? iconAlpha_ : nullptr;
            if (mask) {
                SDL_SetTextureColorMod(mask, T().accent.r, T().accent.g, T().accent.b);
                SDL_Rect dst = {lx, cy - 7, 14, 14};
                SDL_RenderCopy(renderer_, mask, nullptr, &dst);
                SDL_SetTextureColorMod(mask, 255, 255, 255);
                lx += 20;
            } else if (icon == LabelIcon::Egg) {
                if (SDL_Texture* egg = getSprite(0)) {
                    drawSpriteFit(egg, lx + 8, cy, 20);
                    lx += 20;
                }
            }
            // The old labels end in a colon ("Species:"); it goes here.
            std::string l = i18n::get(key);
            while (!l.empty() && (l.back() == ':' || l.back() == ' ')) l.pop_back();
            drawText(fitText(l, fLabel, SF_LABEL_W - (lx - rx) - 6), lx, cy - TTF_FontHeight(fLabel) / 2, T().text, fLabel);
        };
        auto state = [&](bool on) {
            std::string s = i18n::get(on ? StrKey::FilterYes : StrKey::FilterOff);
            drawText(s, cx, cy - TTF_FontHeight(fVal) / 2, on ? T().text : T().textDim, fVal);
        };
        switch (row) {
            case SR_SPECIES: {
                label(StrKey::FilterSpecies);
                if (searchFilter_.speciesId > 0) {
                    fillRounded(cx, cy - 16, 32, 32, 8, T().slotFull);
                    drawSpriteFit(spriteFor(searchFilter_.speciesId, 0, false, false), cx + 16, cy, 28);
                    TTF_Font* fN = uiFont(16, true);
                    const std::string dex = "#" + std::to_string(searchFilter_.speciesId);
                    TTF_Font* fD = uiFont(12);
                    const std::string n = fitText(searchFilter_.speciesName, fN, cw - 44 - textWidth(dex, fD) - 26);
                    drawText(n, cx + 42, cy - TTF_FontHeight(fN) / 2, T().text, fN);
                    drawText(dex, cx + 42 + textWidth(n, fN) + 10, cy - TTF_FontHeight(fD) / 2 + 1, T().textDim, fD);
                } else {
                    drawText(i18n::get(StrKey::SfAnySpecies), cx, cy - TTF_FontHeight(fVal) / 2, T().textMuted, fVal);
                }
                fillArrow(static_cast<float>(right - 4), cy, 6, ArrowDir::Right, T().textDim);
                break;
            }
            case SR_OT: {
                label(StrKey::FilterOT);
                const bool any = searchFilter_.otName.empty();
                drawText(fitText(any ? i18n::get(StrKey::SfAnyTrainer) : searchFilter_.otName, fVal, cw - 20),
                         cx, cy - TTF_FontHeight(fVal) / 2, any ? T().textMuted : T().text, fVal);
                fillArrow(static_cast<float>(right - 4), cy, 6, ArrowDir::Right, T().textDim);
                break;
            }
            case SR_GENDER:
                label(StrKey::FilterGender);
                segmented(cx, cy, right - cx, {i18n::get(StrKey::GenderAny), "\xe2\x99\x82", "\xe2\x99\x80", "\xe2\x80\x94"},
                          static_cast<int>(searchFilter_.gender));
                break;
            case SR_LEVEL: {
                label(StrKey::FilterLevel);
                const int lo = searchFilter_.levelMin > 0 ? searchFilter_.levelMin : 1;
                const int hi = searchFilter_.levelMax > 0 ? searchFilter_.levelMax : 100;
                int sx = cx;
                sx += stepper(sx, cy, lo, cur && searchLevelFocus_ == 0) + 10;
                TTF_Font* fTo = uiFont(14, true);
                const std::string to = i18n::get(StrKey::SfTo);
                drawText(to, sx, cy - TTF_FontHeight(fTo) / 2, T().textDim, fTo);
                sx += textWidth(to, fTo) + 10;
                stepper(sx, cy, hi, cur && searchLevelFocus_ == 1);
                break;
            }
            case SR_SHINY: label(StrKey::FilterShiny, LabelIcon::Shiny); state(searchFilter_.filterShiny); toggle(right, cy, searchFilter_.filterShiny); break;
            case SR_EGG:   label(StrKey::FilterEgg,   LabelIcon::Egg);   state(searchFilter_.filterEgg);   toggle(right, cy, searchFilter_.filterEgg);   break;
            case SR_ALPHA: label(StrKey::FilterAlpha, LabelIcon::Alpha); state(searchFilter_.filterAlpha); toggle(right, cy, searchFilter_.filterAlpha); break;
            case SR_IVS: {
                label(StrKey::FilterPerfectIVs);
                std::vector<std::string> l = {i18n::get(StrKey::FilterOff)};
                for (int i = 1; i <= 6; i++) l.push_back("\xe2\x89\xa5" + std::to_string(i));
                segmented(cx, cy, right - cx, l, searchFilter_.minPerfectIVs);
                break;
            }
            case SR_RIBBONS:
                label(StrKey::FilterRibbons);
                segmented(cx, cy, right - cx, {i18n::get(StrKey::FilterOff), i18n::get(StrKey::SfRibbon),
                                               i18n::get(StrKey::SfMark), i18n::get(StrKey::SfEither)},
                          static_cast<int>(searchFilter_.ribbonFilter));
                break;
        }
    };

    const int top = y + SF_HEADER_H + 20;
    const int lx = x + 20, rxCol = x + SF_W - 20 - SF_COL_W;
    for (size_t i = 0; i < left.size(); i++)  drawRow(left[i],  lx,    top + static_cast<int>(i) * SF_ROW_STEP);
    for (size_t i = 0; i < right.size(); i++) drawRow(right[i], rxCol, top + static_cast<int>(i) * SF_ROW_STEP);

    // How to show the results: a wide two-way choice with a line under each.
    {
        const int ry = top + colRows * SF_ROW_STEP + 8, rh = 60, rw = SF_W - 40;
        const int cy = ry + rh / 2;
        if (curRow == SR_MODE) {
            strokeRounded(lx - 6, ry - 6, rw + 12, rh + 12, 18, 3, T().accent);
            fillRounded(lx, ry, rw, rh, 12, T().slotFull);
        }
        drawText(fitText(i18n::get(StrKey::SfShowResults), fLabel, SF_LABEL_W + 20), lx + 18,
                 cy - TTF_FontHeight(fLabel) / 2, T().text, fLabel);
        const int sx = lx + SF_LABEL_W + 10, sw = 426, sh = 50;
        fillRounded(sx, cy - sh / 2, sw, sh, 10, T().bg);
        const int segW = (sw - 8) / 2;
        const int sel = searchFilter_.mode == SearchMode::List ? 0 : 1;
        TTF_Font* fSub = uiFont(12);
        const char* titles[2] = {StrKey::SfList, StrKey::SfHighlight};
        const char* subs[2] = {StrKey::SfListSub, StrKey::SfHighlightSub};
        for (int i = 0; i < 2; i++) {
            const int bx = sx + 4 + i * segW;
            if (i == sel) fillRounded(bx, cy - sh / 2 + 4, segW, sh - 8, 8, T().buttonBg);
            drawTextCentered(i18n::get(titles[i]), bx + segW / 2, cy - 8, i == sel ? T().text : T().textDim, fSeg);
            drawTextCentered(fitText(i18n::get(subs[i]), fSub, segW - 10), bx + segW / 2, cy + 10,
                             i == sel ? T().textDim : T().textMuted, fSub);
        }
    }

    // Active filters, and Reset / Search.
    {
        const int by = y + H - SF_FOOT_H - SF_BAR_H;
        drawRect(x, by, SF_W, 1, T().panelBorder);
        const int cy = by + SF_BAR_H / 2;
        TTF_Font* fTag = uiFont(11, true);
        const std::string tag = toUpperUtf8(i18n::get(StrKey::SfFilters));
        drawTextTracked(tag, x + 26, cy - TTF_FontHeight(fTag) / 2, T().textDim, fTag, 2);
        const int chipsX = x + 26 + measureTextTracked(tag, fTag, 2) + 14;

        TTF_Font* fB = uiFont(16, true);
        auto button = [&](int rightEdge, const char* key, const char* labelKey, bool primary) {
            const std::string l = i18n::get(labelKey);
            const int w = 12 + 24 + 10 + textWidth(l, fB) + 18, h = 46;
            const int bx = rightEdge - w;
            fillRounded(bx, cy - h / 2, w, h, 12, primary ? T().accent : T().buttonBg);
            if (!primary) strokeRounded(bx, cy - h / 2, w, h, 12, 1, T().buttonBorder);
            fillDisc(bx + 12 + 12, cy, 12, primary ? T().keyCapText : T().keyCap);
            drawTextCentered(key, bx + 24, cy, primary ? T().accent : T().keyCapText, uiFont(13, true));
            drawText(l, bx + 12 + 24 + 10, cy - TTF_FontHeight(fB) / 2, primary ? T().keyCapText : T().text, fB);
            return bx;
        };
        int bx = button(x + SF_W - 24, "Y", StrKey::HintSearch, true);
        bx = button(bx - 12, "X", StrKey::HintReset, false);

        const auto chips = searchChips();
        if (chips.empty()) {
            TTF_Font* f = uiFont(13);
            drawText(fitText(i18n::get(StrKey::SfNoFilters), f, bx - 16 - chipsX), chipsX,
                     cy - TTF_FontHeight(f) / 2, T().textMuted, f);
        } else {
            drawSearchChips(chips, chipsX, cy, bx - 16 - chipsX);
        }
    }

    // Footer: what A and the D-pad do on this row.
    {
        const int fy = y + H - SF_FOOT_H;
        drawRect(x, fy, SF_W, 1, T().panelBorder);
        const int cy = fy + SF_FOOT_H / 2;
        TTF_Font* f = uiFont(15, true);
        struct Hint { const char* keys; const char* label; };
        std::vector<Hint> hints;
        if (curRow == SR_SPECIES)      hints.push_back({"A", StrKey::HintChooseSpecies});
        else if (curRow == SR_OT)      hints.push_back({"A", StrKey::HintType});
        else if (curRow == SR_LEVEL) {
            hints.push_back({"ZL ZR", StrKey::HintChange});
            hints.push_back({"L R", StrKey::HintMinMax});
            hints.push_back({"A", StrKey::HintType});
        } else {
            hints.push_back({"A", StrKey::HintChange});
        }
        hints.push_back({"B", StrKey::HintCancel});
        int hx = x + 26;
        for (const auto& h : hints) {
            hx += drawFooterKey(hx, cy, h.keys, false) + 8;
            const std::string& l = i18n::get(h.label);
            drawText(l, hx, cy - TTF_FontHeight(f) / 2, T().text, f);
            hx += textWidth(l, f) + 22;
        }
    }
}

// --- Species picker (5c) -----------------------------------------------------------

// Letters are 1..26 (A..Z). The list shown is the current letter's; opening
// the picker (letter 0, as both callers do) lands on the first letter that has
// any species.
void UI::ensureSpeciesPickerLetter() {
    if (speciesLetterCursor_ < 1 || speciesLetterCursor_ > 26 || !letterHasSpecies(speciesLetterCursor_)) {
        speciesLetterCursor_ = 1;
        while (speciesLetterCursor_ <= 26 && !letterHasSpecies(speciesLetterCursor_))
            speciesLetterCursor_++;
        if (speciesLetterCursor_ > 26) speciesLetterCursor_ = 1;
        speciesPickerLetter_ = -1;
    }
    if (speciesPickerLetter_ != speciesLetterCursor_) {
        buildSpeciesListForLetter(speciesLetterCursor_);
        speciesPickerLetter_ = speciesLetterCursor_;
        speciesListCursor_ = 0;
        speciesListScroll_ = 0;
    }
}

void UI::stepSpeciesLetter(int dir) {
    ensureSpeciesPickerLetter();
    int l = speciesLetterCursor_;
    for (int i = 0; i < 26; i++) {
        l = (l - 1 + dir + 26) % 26 + 1;
        if (letterHasSpecies(l)) break;
    }
    speciesLetterCursor_ = l;
    ensureSpeciesPickerLetter();
}

void UI::moveSpeciesPicker(int dx, int dy) {
    ensureSpeciesPickerLetter();
    const int n = static_cast<int>(speciesPickerList_.size());
    if (n == 0) return;
    int col = speciesListCursor_ % SPECIES_COLS, row = speciesListCursor_ / SPECIES_COLS;
    const int rows = (n + SPECIES_COLS - 1) / SPECIES_COLS;
    if (dx) {
        const int inRow = std::min(SPECIES_COLS, n - row * SPECIES_COLS);
        col = (col + dx + inRow) % inRow;
    }
    if (dy) row = (row + dy + rows) % rows;
    speciesListCursor_ = std::min(row * SPECIES_COLS + col, n - 1);
    const int r = speciesListCursor_ / SPECIES_COLS;
    if (r < speciesListScroll_) speciesListScroll_ = r;
    if (r >= speciesListScroll_ + SPECIES_VISIBLE_ROWS) speciesListScroll_ = r - SPECIES_VISIBLE_ROWS + 1;
}

// A pick (or "any species", id 0) goes to whichever filter opened the picker.
void UI::pickSpecies(uint16_t id) {
    const std::string name = id ? SpeciesName::get(id) : std::string();
    if (speciesPickerForGts_) {
        gtsFilter_.species = id;
        gtsFilter_.speciesName = name;
    } else {
        searchFilter_.speciesId = id;
        searchFilter_.speciesName = name;
    }
    showSpeciesLetterPicker_ = false;
}

void UI::handleSpeciesLetterPickerInput(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
            int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
            updateStick(lx, ly);
        }
        return;
    }
    if (event.type != SDL_CONTROLLERBUTTONDOWN) return;
    ensureSpeciesPickerLetter();
    switch (event.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    moveSpeciesPicker(0, -1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  moveSpeciesPicker(0, +1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  moveSpeciesPicker(-1, 0); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: moveSpeciesPicker(+1, 0); break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  stepSpeciesLetter(-1); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: stepSpeciesLetter(+1); break;
        case SDL_CONTROLLER_BUTTON_B: // Switch A = pick
            if (!speciesPickerList_.empty())
                pickSpecies(speciesPickerList_[std::clamp(speciesListCursor_, 0, (int)speciesPickerList_.size() - 1)]);
            break;
        case SDL_CONTROLLER_BUTTON_X: // Switch Y = any species (the old "-")
            pickSpecies(0);
            break;
        case SDL_CONTROLLER_BUTTON_A: // Switch B = back to the search
            showSpeciesLetterPicker_ = false;
            break;
    }
}

void UI::drawSpeciesLetterPicker() {
    ensureSpeciesPickerLetter();
    drawRect(0, 0, SCREEN_W, SCREEN_H, SDL_Color{T().bg.r, T().bg.g, T().bg.b, 190});
    const int H = 640;
    const int x = (SCREEN_W - SP_W) / 2, y = (SCREEN_H - H) / 2;
    fillRounded(x, y, SP_W, H, 22, T().panelBg);
    strokeRounded(x, y, SP_W, H, 22, 1, T().panelBorder);

    // Header: back, title, count, "any species".
    {
        fillRounded(x + 25, y + 19, 38, 38, 10, T().buttonBg);
        strokeRounded(x + 25, y + 19, 38, 38, 10, 1, T().buttonBorder);
        fillArrow(x + 44.0f, y + 38.0f, 8, ArrowDir::Left, T().text);
        TTF_Font* fT = uiFont(24, true);
        TTF_Font* fS = uiFont(15);
        const std::string title = i18n::get(StrKey::SpTitle);
        drawText(title, x + 77, y + 38 - TTF_FontHeight(fT) / 2, T().text, fT);
        const std::string letter(1, static_cast<char>('A' + speciesLetterCursor_ - 1));
        drawText(i18n::fmt(StrKey::SpCount, letter, std::to_string(speciesPickerList_.size())),
                 x + 77 + textWidth(title, fT) + 12, y + 40 - TTF_FontHeight(fS) / 2, T().textDim, fS);

        TTF_Font* fB = uiFont(14, true);
        const std::string any = i18n::get(StrKey::HintAnySpecies);
        const int bw = 10 + 24 + 8 + textWidth(any, fB) + 14;
        const int bx = x + SP_W - 25 - bw;
        fillRounded(bx, y + 19, bw, 40, 10, T().buttonBg);
        strokeRounded(bx, y + 19, bw, 40, 10, 1, T().buttonBorder);
        fillDisc(bx + 22, y + 39, 12, T().keyCap);
        drawTextCentered("Y", bx + 22, y + 39, T().keyCapText, uiFont(13, true));
        drawText(any, bx + 10 + 24 + 8, y + 39 - TTF_FontHeight(fB) / 2, T().text, fB);
    }

    // Letters, L / R to move between them.
    {
        const int ly = y + 74, lh = 32;
        TTF_Font* fK = uiFont(12, true);
        TTF_Font* fL = uiFont(15, true);
        fillRounded(x + 25, ly + 6, 22, 20, 5, T().keyCap);
        drawTextCentered("L", x + 36, ly + 16, T().keyCapText, fK);
        const int lx0 = x + 55, avail = SP_W - 55 - 55;
        const int step = avail / 26;
        for (int i = 1; i <= 26; i++) {
            const int bx = lx0 + (i - 1) * step;
            const bool cur = i == speciesLetterCursor_;
            const bool has = letterHasSpecies(i);
            fillRounded(bx, ly, step - 6, lh, 7, cur ? T().accent : T().buttonBg);
            drawTextCentered(std::string(1, static_cast<char>('A' + i - 1)), bx + (step - 6) / 2, ly + lh / 2,
                             cur ? T().keyCapText : has ? T().text : T().textMuted, fL);
        }
        fillRounded(x + SP_W - 47, ly + 6, 22, 20, 5, T().keyCap);
        drawTextCentered("R", x + SP_W - 36, ly + 16, T().keyCapText, fK);
    }
    drawRect(x, y + 122, SP_W, 1, T().panelBorder);

    // The species of the letter, four to a row.
    const int gridTop = y + 132, gridBottom = y + H - 52;
    const int gx = x + 25;
    const int n = static_cast<int>(speciesPickerList_.size());
    TTF_Font* fName = uiFont(16, true);
    TTF_Font* fDex = uiFont(13);
    const SDL_Rect clip = {x + 16, gridTop - 6, SP_W - 32, gridBottom - gridTop + 6};
    SDL_RenderSetClipRect(renderer_, &clip);
    for (int i = speciesListScroll_ * SPECIES_COLS; i < n; i++) {
        const int row = i / SPECIES_COLS - speciesListScroll_;
        const int cy0 = gridTop + 6 + row * SP_PITCH_Y;
        if (cy0 > gridBottom) break;
        const int cx0 = gx + (i % SPECIES_COLS) * SP_PITCH_X;
        const bool cur = i == speciesListCursor_;
        if (cur) strokeRounded(cx0 - 5, cy0 - 5, SP_CARD_W + 10, SP_CARD_H + 10, 16, 3, T().accent);
        fillRounded(cx0, cy0, SP_CARD_W, SP_CARD_H, 10, cur ? T().slotFull : T().panelBg);
        strokeRounded(cx0, cy0, SP_CARD_W, SP_CARD_H, 10, 1, T().panelBorder);
        const uint16_t id = speciesPickerList_[i];
        fillRounded(cx0 + 12, cy0 + 10, 40, 40, 10, T().slotFull);
        drawSpriteFit(spriteFor(id, 0, false, false), cx0 + 32, cy0 + 30, 36);
        drawText(fitText(SpeciesName::get(id), fName, SP_CARD_W - 76), cx0 + 64, cy0 + 10, T().text, fName);
        char dex[8];
        std::snprintf(dex, sizeof(dex), "#%03u", static_cast<unsigned>(id));
        drawText(dex, cx0 + 64, cy0 + 33, T().textDim, fDex);
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
    if (n == 0) {
        TTF_Font* f = uiFont(15);
        drawTextCentered(i18n::get(StrKey::NoSpeciesFound), x + SP_W / 2, (gridTop + gridBottom) / 2, T().textMuted, f);
    }

    // Footer
    {
        const int fy = y + H - 52;
        drawRect(x, fy, SP_W, 1, T().panelBorder);
        const int cy = fy + 26;
        TTF_Font* f = uiFont(15, true);
        struct Hint { const char* keys; const char* label; };
        const Hint hints[] = {
            {"A", StrKey::HintSelect2}, {"L R", StrKey::HintLetter},
            {"Y", StrKey::HintAnySpecies}, {"B", StrKey::HintBackSearch},
        };
        int hx = x + 25;
        for (const auto& h : hints) {
            hx += drawFooterKey(hx, cy, h.keys, false) + 8;
            const std::string& l = i18n::get(h.label);
            drawText(l, hx, cy - TTF_FontHeight(f) / 2, T().text, f);
            hx += textWidth(l, f) + 22;
        }
    }
}

// --- Results (5d, 5e, 5f) ----------------------------------------------------------

void UI::moveSearchResult(int delta) {
    if (searchResults_.empty()) return;
    const int n = static_cast<int>(searchResults_.size());
    searchResultCursor_ = std::clamp(searchResultCursor_ + delta, 0, n - 1);
    if (searchResultCursor_ < searchResultScroll_) searchResultScroll_ = searchResultCursor_;
    if (searchResultCursor_ >= searchResultScroll_ + SEARCH_VISIBLE_ROWS)
        searchResultScroll_ = searchResultCursor_ - SEARCH_VISIBLE_ROWS + 1;
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
            case SDL_CONTROLLER_BUTTON_DPAD_UP:   moveSearchResult(-1); break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: moveSearchResult(+1); break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                moveSearchResult(-10);
                lHeld_ = true;
                bumperRepeatTime_ = SDL_GetTicks();
                bumperMoved_ = false;
                break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                moveSearchResult(10);
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
}

void UI::drawSearchResultsPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, SDL_Color{T().bg.r, T().bg.g, T().bg.b, 190});

    const int n = static_cast<int>(searchResults_.size());
    const bool dual = isDualBankMode();
    constexpr int HEAD = 76, FOOT = 52;
    int bodyH;
    if (n == 0)      bodyH = 290;
    else if (n == 1) bodyH = 20 + SR_ROW_H + 40;
    else             bodyH = 20 + std::min(n, SEARCH_VISIBLE_ROWS) * SR_PITCH + 12;
    const int H = HEAD + bodyH + FOOT;
    const int x = (SCREEN_W - SR_W) / 2, y = (SCREEN_H - H) / 2;
    fillRounded(x, y, SR_W, H, 22, T().panelBg);
    strokeRounded(x, y, SR_W, H, 22, 1, T().panelBorder);

    // Header: title, how many, the filters, and the way back to them.
    TTF_Font* fB = uiFont(15, true);
    const std::string edit = i18n::get(StrKey::HintEditFilters);
    const int ebw = 12 + 24 + 10 + textWidth(edit, fB) + 16;
    const int ebx = x + SR_W - 25 - ebw;
    {
        if (SDL_Texture* ic = uiIcon("search")) {
            SDL_SetTextureColorMod(ic, T().accent.r, T().accent.g, T().accent.b);
            SDL_Rect dst = {x + 26, y + 27, 22, 22};
            SDL_RenderCopy(renderer_, ic, nullptr, &dst);
            SDL_SetTextureColorMod(ic, 255, 255, 255);
        }
        TTF_Font* fT = uiFont(24, true);
        const std::string title = i18n::get(StrKey::SrTitle);
        const int cy = y + 38;
        drawText(title, x + 60, cy - TTF_FontHeight(fT) / 2, T().text, fT);
        int cx = x + 60 + textWidth(title, fT) + 14;

        TTF_Font* fC = uiFont(13, true);
        const std::string found = n ? i18n::fmt(StrKey::SrFound, std::to_string(n)) : i18n::get(StrKey::SrNoneFound);
        const SDL_Color fc = n ? T().statusOk : T().textDim;
        const int fw = textWidth(found, fC) + 20;
        fillRounded(cx, cy - 13, fw, 26, 8, n ? SDL_Color{fc.r, fc.g, fc.b, 36} : T().buttonBg);
        drawTextCentered(found, cx + fw / 2, cy, fc, fC);
        cx += fw + 8;
        drawSearchChips(searchChips(), cx, cy, ebx - 16 - cx);

        fillRounded(ebx, cy - 21, ebw, 42, 12, T().buttonBg);
        strokeRounded(ebx, cy - 21, ebw, 42, 12, 1, T().buttonBorder);
        fillDisc(ebx + 24, cy, 12, T().keyCap);
        drawTextCentered("X", ebx + 24, cy, T().keyCapText, uiFont(13, true));
        drawText(edit, ebx + 46, cy - TTF_FontHeight(fB) / 2, T().text, fB);
    }
    drawRect(x, y + HEAD, SR_W, 1, T().panelBorder);

    const int top = y + HEAD + 20;
    if (n == 0) {
        // Nothing: say so, where it looked, and the way out.
        const int cx = x + SR_W / 2;
        fillDisc(cx, top + 44, 32, T().buttonBg);
        if (SDL_Texture* ic = uiIcon("search")) {
            SDL_SetTextureColorMod(ic, T().textDim.r, T().textDim.g, T().textDim.b);
            SDL_Rect dst = {cx - 13, top + 31, 26, 26};
            SDL_RenderCopy(renderer_, ic, nullptr, &dst);
            SDL_SetTextureColorMod(ic, 255, 255, 255);
        }
        drawTextCentered(i18n::get(StrKey::SrNoneTitle), cx, top + 110, T().text, uiFont(20, true));
        TTF_Font* fS = uiFont(15);
        const auto lines = wrapText(i18n::get(dual ? StrKey::SrNoneBodyDual : StrKey::SrNoneBodySave), fS, 460, 3);
        int ly = top + 140;
        for (const auto& l : lines) { drawTextCentered(l, cx, ly, T().textDim, fS); ly += 22; }
        const int bw = 12 + 24 + 10 + textWidth(edit, fB) + 18;
        const int by = ly + 12;
        fillRounded(cx - bw / 2, by, bw, 48, 12, T().accent);
        fillDisc(cx - bw / 2 + 24, by + 24, 12, T().keyCapText);
        drawTextCentered("X", cx - bw / 2 + 24, by + 24, T().accent, uiFont(13, true));
        drawText(edit, cx - bw / 2 + 46, by + 24 - TTF_FontHeight(fB) / 2, T().keyCapText, fB);
    } else {
        TTF_Font* fIdx  = uiFont(13);
        TTF_Font* fName = uiFont(18, true);
        TTF_Font* fLv   = uiFont(14);
        TTF_Font* fLoc  = uiFont(14, true);
        TTF_Font* fTag  = uiFont(11, true);
        const int rx = x + 30, rw = SR_W - 60 - (n > SEARCH_VISIBLE_ROWS ? 16 : 0);
        for (int i = searchResultScroll_; i < n && i < searchResultScroll_ + SEARCH_VISIBLE_ROWS; i++) {
            const auto& r = searchResults_[i];
            const int ry = top + (i - searchResultScroll_) * SR_PITCH, cy = ry + SR_ROW_H / 2;
            const bool cur = i == searchResultCursor_;
            if (cur) strokeRounded(rx - 6, ry - 6, rw + 12, SR_ROW_H + 12, 18, 3, T().accent);
            fillRounded(rx, ry, rw, SR_ROW_H, 12, cur ? T().slotFull : T().panelBg);
            strokeRounded(rx, ry, rw, SR_ROW_H, 12, 1, T().panelBorder);

            const std::string idx = std::to_string(i + 1);
            drawText(idx, rx + 44 - textWidth(idx, fIdx), cy - TTF_FontHeight(fIdx) / 2, T().textMuted, fIdx);
            fillRounded(rx + 56, ry + 10, 44, 44, 10, T().slotFull);
            drawSpriteFit(spriteFor(r.species, r.form, r.isShiny, r.isEgg), rx + 78, ry + 32, 40);
            if (r.isShiny && iconShiny_) {
                SDL_SetTextureColorMod(iconShiny_, T().accent.r, T().accent.g, T().accent.b);
                SDL_Rect dst = {rx + 52, ry + 6, 13, 13};
                SDL_RenderCopy(renderer_, iconShiny_, nullptr, &dst);
                SDL_SetTextureColorMod(iconShiny_, 255, 255, 255);
            }
            // Alpha on the opposite corner, so a shiny alpha shows both.
            if (r.isAlpha && iconAlpha_) {
                SDL_SetTextureColorMod(iconAlpha_, T().accent.r, T().accent.g, T().accent.b);
                SDL_Rect dst = {rx + 91, ry + 6, 13, 13};
                SDL_RenderCopy(renderer_, iconAlpha_, nullptr, &dst);
                SDL_SetTextureColorMod(iconAlpha_, 255, 255, 255);
            }

            // Right: where it is, and on the cursor, what A does.
            int right = rx + rw - 18;
            if (cur) {
                TTF_Font* fG = uiFont(14, true);
                const std::string go = i18n::get(StrKey::SrGoTo);
                const int gw = textWidth(go, fG);
                drawText(go, right - gw, cy - TTF_FontHeight(fG) / 2, T().text, fG);
                fillDisc(right - gw - 16, cy, 10, T().keyCapText);
                drawTextCentered("A", right - gw - 16, cy, T().text, uiFont(12, true));
                right -= gw + 40;
            }
            const std::string loc = i18n::fmt(StrKey::SrSlot, panelBoxName(r.panel, r.box), std::to_string(r.slot + 1));
            const std::string locFit = fitText(loc, fLoc, 220);
            const int lw = textWidth(locFit, fLoc);
            drawText(locFit, right - lw, cy - TTF_FontHeight(fLoc) / 2, T().text, fLoc);
            right -= lw + 12;
            const std::string side = dual
                ? i18n::get(r.panel == Panel::Game ? StrKey::BsTagLeft : StrKey::BsTagRight)
                : toUpperUtf8(i18n::get(r.panel == Panel::Game ? StrKey::TagSave : StrKey::TagBank));
            const int tw = measureTextTracked(side, fTag, 2) + 18;
            const SDL_Color teal = T().accentBank;
            fillRounded(right - tw, cy - 11, tw, 22, 6, SDL_Color{teal.r, teal.g, teal.b, 40});
            drawTextTracked(side, right - tw + 9, cy - TTF_FontHeight(fTag) / 2, teal, fTag, 2);
            right -= tw + 16;

            // Left: the Pokemon.
            const int nx = rx + 114;
            std::string name = r.speciesName;
            if (r.isEgg) name += " - " + i18n::get(StrKey::Egg);
            std::string gender;
            SDL_Color gc = T().text;
            if (!r.isEgg && r.gender == 0) { gender = "\xe2\x99\x82"; gc = T().genderMale; }
            if (!r.isEgg && r.gender == 1) { gender = "\xe2\x99\x80"; gc = T().genderFemale; }
            const std::string lv = r.isEgg ? std::string() : i18n::get(StrKey::LvPrefix) + std::to_string(r.level);
            const int tail = (gender.empty() ? 0 : textWidth(gender, fName) + 8) + (lv.empty() ? 0 : textWidth(lv, fLv) + 8);
            const std::string nm = fitText(name, fName, right - nx - tail);
            drawText(nm, nx, cy - TTF_FontHeight(fName) / 2, T().text, fName);
            int ex = nx + textWidth(nm, fName) + 8;
            if (!gender.empty()) { drawText(gender, ex, cy - TTF_FontHeight(fName) / 2, gc, fName); ex += textWidth(gender, fName) + 8; }
            if (!lv.empty()) drawText(lv, ex, cy - TTF_FontHeight(fLv) / 2 + 1, T().textDim, fLv);
        }
        if (n > SEARCH_VISIBLE_ROWS) {
            const int trackY = top, trackH = SEARCH_VISIBLE_ROWS * SR_PITCH - 8;
            const int thumbH = std::max(30, trackH * SEARCH_VISIBLE_ROWS / n);
            const int thumbY = trackY + (trackH - thumbH) * searchResultScroll_ / std::max(1, n - SEARCH_VISIBLE_ROWS);
            fillRounded(x + SR_W - 34, trackY, 5, trackH, 2, T().buttonBg);
            fillRounded(x + SR_W - 34, thumbY, 5, thumbH, 2, T().textMuted);
        }
        if (n == 1) {
            TTF_Font* f = uiFont(13);
            drawText(i18n::get(StrKey::SrOneNote), rx, top + SR_ROW_H + 12, T().textDim, f);
        }
    }

    // Footer
    {
        const int fy = y + H - FOOT;
        drawRect(x, fy, SR_W, 1, T().panelBorder);
        const int cy = fy + FOOT / 2;
        TTF_Font* f = uiFont(15, true);
        struct Hint { const char* keys; const char* label; };
        std::vector<Hint> hints;
        if (n > 0) hints.push_back({"A", StrKey::HintGoToSlot});
        if (n > 1) hints.push_back({"L R", StrKey::HintJump10});
        hints.push_back({"X", StrKey::HintEditFilters});
        hints.push_back({"B", StrKey::HintClose});
        int hx = x + 25;
        for (const auto& h : hints) {
            hx += drawFooterKey(hx, cy, h.keys, false) + 8;
            const std::string& l = i18n::get(h.label);
            drawText(l, hx, cy - TTF_FontHeight(f) / 2, T().text, f);
            hx += textWidth(l, f) + 22;
        }
        if (n > 1) {
            TTF_Font* fc = uiFont(14, true);
            const std::string pos = std::to_string(searchResultCursor_ + 1) + " / " + std::to_string(n);
            drawText(pos, x + SR_W - 25 - textWidth(pos, fc), cy - TTF_FontHeight(fc) / 2, T().textDim, fc);
        }
    }
}
