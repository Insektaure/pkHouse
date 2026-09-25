// Pokemon summary (UI 2.0).
//
// Laid out after external/UI_2.0 "Pokémon summary (X)", which is the exported
// card (ui_card.cpp) on screen. The card's QR code is only for the export, so
// its place in the portrait panel holds the full ribbon and mark list instead.
//
// Everything the old detail popup showed is here: sprite, shiny/alpha, ball,
// species, level, gender, dex number, OT with TID/SID, HT, nature, ability,
// held item, moves with types, every ribbon and mark by name, IVs and EVs with
// their values, PID, EC, raw ID and TSV. The card adds nickname and form,
// types, what the nature raises and lowers, tera type, the Gigantamax and egg
// badges, origin, met date, location and language.

#include "ui.h"
#include "ui_util.h"
#include "i18n.h"
#include "species_converter.h"
#include "species_types.h"
#include "met_info.h"
#include "move_types.h"
#include "form_names.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// Layout, 1280x720. Header above y 72, footer from UI::FOOTER_Y.
constexpr int MARGIN_L = 40;
constexpr int MARGIN_R = 1240;

// Prefixed because these functions are UI members: an unprefixed PANEL_Y
// here would silently lose to the box view's UI::PANEL_Y.
constexpr int SUM_PANEL_X = 40;           // portrait panel
constexpr int SUM_PANEL_Y = 81;
constexpr int SUM_PANEL_W = 611;
constexpr int SUM_PANEL_H = 466;
constexpr int SUM_PANEL_PAD = 19;

constexpr int SPRITE_X = SUM_PANEL_X + SUM_PANEL_PAD;
constexpr int SPRITE_Y = 155;
constexpr int SPRITE_BOX = 200;

// Where the card has its QR tile.
constexpr int RIB_X = 313;
constexpr int RIB_Y = SUM_PANEL_Y + 18;
constexpr int RIB_W = SUM_PANEL_X + SUM_PANEL_W - 18 - RIB_X;
constexpr int RIB_H = SUM_PANEL_Y + SUM_PANEL_H - 18 - RIB_Y;
constexpr int RIB_ROW_H = 26;
constexpr int RIB_LIST_OFS = 38;       // list start below the tile top

constexpr int COL_X = 676;            // right column
constexpr int COL_W = MARGIN_R - COL_X;

constexpr int MOVE_W = 277;
constexpr int MOVE_H = 48;
constexpr int MOVE_GAP = 10;
constexpr int MOVE_Y[2] = {146, 204};

constexpr int STATS_LABEL_Y = 276;
constexpr int IV_CX = 815;
constexpr int IV_CY = 420;
constexpr int IV_RAD = 70;
constexpr int EV_X = 976;
constexpr int EV_ROW_Y = 304;
constexpr int EV_ROW_STEP = 30;

constexpr int PROV_RULE_Y = 566;
constexpr int PROV_LABEL_Y = 580;
constexpr int PROV_VALUE_Y = 597;
constexpr int TECH_Y = 634;

int baselineTop(TTF_Font* f, int baseline) {
    return baseline - TTF_FontAscent(f);
}

// "Nature: " -> "Nature". The old popup's prefixes double as the labels, so
// every language keeps the wording it already had.
std::string stripPrefix(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == ':'))
        s.pop_back();
    // Full-width colon (U+FF1A), used by the CJK strings.
    const std::string fw = "\xef\xbc\x9a";
    if (s.size() >= fw.size() && s.compare(s.size() - fw.size(), fw.size(), fw) == 0)
        s.erase(s.size() - fw.size());
    while (!s.empty() && s.back() == ' ')
        s.pop_back();
    return s;
}

} // anonymous namespace

// --- Entry point -------------------------------------------------------------------

void UI::drawDetailPopup(const Pokemon& pkm, const ButtonHint* hints, int hintCount,
                         const std::string& where) {
    // Full screen: whatever was under it is painted over.
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);
    drawRect(0, 0, SCREEN_W, ACCENT_RULE_H, T().accent);

    drawSummaryHeader(pkm, where);
    drawSummaryPortrait(pkm);
    drawSummaryAttributes(pkm);
    drawSummaryMoves(pkm);
    drawSummaryIVs(pkm);
    drawSummaryEVs(pkm);
    drawSummaryProvenance(pkm);

    // The caller's keys, plus the D-pad when the ribbon list is longer than
    // its tile and can be scrolled.
    ButtonHint all[8];
    int n = 0;
    for (int i = 0; i < hintCount && n < 7; i++)
        all[n++] = hints[i];
    if ((int)pkm.getRibbonsAndMarks().size() > SUM_RIBBON_ROWS)
        all[n++] = {HINT_DPAD, StrKey::HintRibbons};
    drawFooterBar(all, n);
}

void UI::scrollDetailRibbons(int dir, const Pokemon& pkm) {
    const int total = static_cast<int>(pkm.getRibbonsAndMarks().size());
    const int maxScroll = std::max(0, total - SUM_RIBBON_ROWS);
    detailRibbonScroll_ = std::max(0, std::min(maxScroll, detailRibbonScroll_ + dir));
}

std::string UI::detailWhere() {
    const bool save = cursor_.panel == Panel::Game && !isDualBankMode();
    std::string where = i18n::get(save ? StrKey::TabSave : StrKey::TabBank);
    where += " \xc2\xb7 " + panelBoxName(cursor_.panel, cursor_.box);

    // Position among the box's occupied slots, which is what L/R steps through.
    const auto& disp = getSlotDisplays(cursor_.panel, cursor_.box);
    const int cur = cursor_.slot(gridCols());
    int rank = 0, filled = 0;
    for (int s = 0; s < (int)disp.size(); s++) {
        if (disp[s].empty) continue;
        filled++;
        if (s <= cur) rank = filled;
    }
    if (filled > 0)
        where += " \xc2\xb7 " + std::to_string(rank) + " / " + std::to_string(filled);
    return where;
}

// --- Header ------------------------------------------------------------------------

void UI::drawSummaryHeader(const Pokemon& pkm, const std::string& where) {
    const int baseline = 57;
    const bool shiny = pkm.isShiny();
    const uint16_t species = pkm.species();

    // Species, preceded by the shiny mark.
    int x = MARGIN_L;
    if (shiny && iconShiny_) {
        SDL_SetTextureColorMod(iconShiny_, T().accent.r, T().accent.g, T().accent.b);
        SDL_Rect dst = {x, 29, 26, 26};
        SDL_RenderCopy(renderer_, iconShiny_, nullptr, &dst);
        SDL_SetTextureColorMod(iconShiny_, 255, 255, 255);
        x += 26 + 14;
    }
    TTF_Font* fName = uiFont(40, true);
    const std::string name = fitText(SpeciesName::get(species), fName, 380);
    drawText(name, x, baselineTop(fName, baseline), shiny ? T().accent : T().text, fName);
    x += textWidth(name, fName) + 14;

    // Nickname and/or form, as on the card.
    std::string subtitle;
    if (!pkm.isEgg()) {
        const std::string nick = pkm.nickname();
        if (pkm.isNicknamed() && !nick.empty() && nick != SpeciesName::get(species))
            subtitle = "\xe2\x80\x9c" + nick + "\xe2\x80\x9d";
        if (const char* form = getFormName(species, pkm.form())) {
            if (!subtitle.empty()) subtitle += " \xc2\xb7 ";
            subtitle += form;
        }
    }
    TTF_Font* fSub = uiFont(16);
    if (!subtitle.empty()) {
        subtitle = fitText(subtitle, fSub, 220);
        drawText(subtitle, x, baselineTop(fSub, baseline - 4), T().textDim, fSub);
        x += textWidth(subtitle, fSub) + 14;
    }

    // Right cluster, laid out right to left: dex number, rule, ball, gender,
    // level. Measured first so the `where` chip can stop short of it.
    int rx = MARGIN_R;
    TTF_Font* fDex = uiFont(28, true);
    char dexBuf[16];
    std::snprintf(dexBuf, sizeof(dexBuf), "#%04u", static_cast<unsigned>(species));
    rx -= textWidth(dexBuf, fDex);
    drawText(dexBuf, rx, baselineTop(fDex, baseline - 4), T().text, fDex);
    rx -= 22;
    drawRect(rx, 22, 1, 40, T().divider);
    rx -= 22;

    TTF_Font* fBall = uiFont(17);
    const uint8_t ballId = pkm.ball();
    if (const char* ballName = BallName::get(ballId); ballName[0] != '\0') {
        rx -= textWidth(ballName, fBall);
        drawText(ballName, rx, baselineTop(fBall, baseline - 9), T().text, fBall);
        rx -= 8;
        if (SDL_Texture* ballTex = getBallSprite(ballId)) {
            rx -= 22;
            SDL_Rect dst = {rx, 31, 22, 22};
            SDL_RenderCopy(renderer_, ballTex, nullptr, &dst);
        }
        rx -= 22;
    }

    const uint8_t gender = pkm.gender();
    if (gender == 0 || gender == 1) {
        const SDL_Color tint = gender == 0 ? T().genderMale : T().genderFemale;
        rx -= 38;
        fillRounded(rx, 23, 38, 38, 10, SDL_Color{tint.r, tint.g, tint.b, 40});
        drawTextCentered(gender == 0 ? "\xe2\x99\x82" : "\xe2\x99\x80", rx + 19, 42, tint,
                         uiFont(22, true));
        rx -= 22;
    }

    TTF_Font* fLv = uiFont(36, true);
    TTF_Font* fLvLabel = uiFont(15);
    const std::string lvl = std::to_string(pkm.level());
    rx -= textWidth(lvl, fLv);
    drawText(lvl, rx, baselineTop(fLv, baseline), T().text, fLv);
    const std::string lvLabel = i18n::get(StrKey::LvPrefix);
    rx -= 6 + textWidth(lvLabel, fLvLabel);
    drawText(lvLabel, rx, baselineTop(fLvLabel, baseline), T().textDim, fLvLabel);

    // Where this Pokemon is.
    if (!where.empty()) {
        TTF_Font* fChip = uiFont(13, true);
        const int room = rx - 24 - x - 20;
        if (room > 40) {
            const std::string w = fitText(where, fChip, room);
            const int cw = textWidth(w, fChip) + 20;
            fillRounded(x, 28, cw, 28, 8, T().buttonBg);
            drawText(w, x + 10, 42 - TTF_FontHeight(fChip) / 2, T().textDim, fChip);
        }
    }
}

// --- Portrait panel ----------------------------------------------------------------

void UI::drawSummaryPortrait(const Pokemon& pkm) {
    fillRounded(SUM_PANEL_X, SUM_PANEL_Y, SUM_PANEL_W, SUM_PANEL_H, 16, T().panelBg);
    strokeRounded(SUM_PANEL_X, SUM_PANEL_Y, SUM_PANEL_W, SUM_PANEL_H, 16, 1, T().panelBorder);

    const bool egg = pkm.isEgg();
    const uint16_t species = pkm.species();

    // Status badges: shiny, alpha, Gigantamax, egg. Shiny and alpha are flat
    // masks that take the badge's tint; the other two are artwork, drawn in
    // their own colours when set and tinted down when not.
    struct Badge { bool on; SDL_Texture* icon; bool artwork; };
    const Badge badges[4] = {
        {pkm.isShiny(),       iconShiny_,   false},
        {pkm.isAlpha(),       iconAlpha_,   false},
        {pkm.canGigantamax(), iconDynamax_, true },
        {egg,                 getSprite(0), true },
    };
    for (int i = 0; i < 4; i++) {
        const int bx = SPRITE_X + i * 48, by = SUM_PANEL_Y + 18;
        const bool on = badges[i].on;
        fillRounded(bx, by, 40, 40, 10, on ? T().badgeBg : T().bg);
        strokeRounded(bx, by, 40, 40, 10, 1, on ? T().accent : T().panelBorder);
        SDL_Texture* icon = badges[i].icon;
        if (!icon) continue;
        const SDL_Color tint = on ? T().accent : T().textMuted;
        const bool keepColours = badges[i].artwork && on;
        if (!keepColours) SDL_SetTextureColorMod(icon, tint.r, tint.g, tint.b);
        if (!on) SDL_SetTextureAlphaMod(icon, 150);
        if (badges[i].artwork) {
            drawSpriteFit(icon, bx + 20, by + 20, 28);
        } else {
            SDL_Rect dst = {bx + 10, by + 10, 20, 20};
            SDL_RenderCopy(renderer_, icon, nullptr, &dst);
        }
        SDL_SetTextureColorMod(icon, 255, 255, 255);
        SDL_SetTextureAlphaMod(icon, 255);
    }

    // Sprite
    fillRounded(SPRITE_X, SPRITE_Y, SPRITE_BOX, SPRITE_BOX, 24, T().slotFull);
    drawSpriteFit(spriteFor(species, pkm.form(), pkm.isShiny(), egg),
                  SPRITE_X + SPRITE_BOX / 2, SPRITE_Y + SPRITE_BOX / 2, SPRITE_BOX - 24);

    // Type chips, with the game's own type icons.
    int chipsBottom = SPRITE_Y + SPRITE_BOX + 16;
    if (!egg) {
        constexpr int CHIP_H = 32, ICON_R = 10;
        TTF_Font* f = uiFont(12, true);
        const SpeciesTypes::Pair types = SpeciesTypes::get(selectedGame_, species, pkm.form());
        int cx = SPRITE_X;
        const int cy = chipsBottom;
        for (uint8_t t : {types.t1, types.t2}) {
            if (t >= 18) continue;
            if (t == types.t2 && types.t2 == types.t1 && cx != SPRITE_X) continue;
            const char* label = TypeName::get(t);
            const int chipW = 6 + ICON_R * 2 + 8 + measureTextTracked(label, f, 1) + 14;
            fillRounded(cx, cy, chipW, CHIP_H, CHIP_H / 2, T().bg);
            strokeRounded(cx, cy, chipW, CHIP_H, CHIP_H / 2, 1, T().panelBorder);
            if (SDL_Texture* tex = getTypeSprite(t))
                blitDisc(renderer_, tex, cx + 6 + ICON_R, cy + CHIP_H / 2, ICON_R, T().bg);
            drawTextTracked(label, cx + 6 + ICON_R * 2 + 8,
                            cy + CHIP_H / 2 - TTF_FontHeight(f) / 2, T().text, f, 1);
            cx += chipW + 8;
            if (cx > RIB_X - 40) break;
        }
        chipsBottom = cy + CHIP_H;
    }

    // National dex number as a watermark in the corner under the sprite, sized
    // down until it fits the room the chips leave.
    {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(species));
        const int boxTop = chipsBottom + 6, boxBottom = SUM_PANEL_Y + SUM_PANEL_H - 6;
        const int boxW = RIB_X - 16 - SPRITE_X;
        for (int size : {130, 110, 90, 72}) {
            TTF_Font* f = uiFont(size, true);
            const int h = TTF_FontAscent(f);
            if (textWidth(buf, f) <= boxW && h <= boxBottom - boxTop) {
                drawText(buf, SPRITE_X - 4, boxBottom - TTF_FontAscent(f) - 4 + TTF_FontDescent(f) / 2,
                         T().panelBorder, f);
                break;
            }
        }
    }

    drawSummaryRibbons(pkm, RIB_X, RIB_Y, RIB_W, RIB_H);
}

// Every ribbon and mark by name, which the card has no room for.
void UI::drawSummaryRibbons(const Pokemon& pkm, int x, int y, int w, int h) {
    fillRounded(x, y, w, h, 12, T().bg);
    strokeRounded(x, y, w, h, 12, 1, T().panelBorder);

    const auto ribbons = pkm.getRibbonsAndMarks();

    // "10 ribbons · 2 marks", leaving out a kind there is none of; the
    // generic title only when the list is empty.
    int marks = 0;
    for (const auto& r : ribbons) if (r.isMark) marks++;
    const int plain = static_cast<int>(ribbons.size()) - marks;
    std::string title;
    if (plain > 0)
        title = i18n::fmt(plain == 1 ? StrKey::SumRibbonOne : StrKey::SumRibbonMany,
                          std::to_string(plain));
    if (marks > 0) {
        if (!title.empty()) title += " \xc2\xb7 ";
        title += i18n::fmt(marks == 1 ? StrKey::SumMarkOne : StrKey::SumMarkMany,
                           std::to_string(marks));
    }
    if (title.empty())
        title = i18n::fmt(StrKey::RibbonsMarks, "0");
    TTF_Font* fLabel = uiFont(11, true);
    title = toUpperUtf8(title);
    drawTextTracked(fitText(title, fLabel, w - 32), x + 16, y + 16, T().textDim, fLabel, 2);

    TTF_Font* fName = uiFont(14);
    if (ribbons.empty()) {
        const std::string none = i18n::get(StrKey::SumNoRibbons);
        drawTextCentered(none, x + w / 2, y + h / 2, T().textMuted, fName);
        return;
    }

    const int total = static_cast<int>(ribbons.size());
    const int maxScroll = std::max(0, total - SUM_RIBBON_ROWS);
    detailRibbonScroll_ = std::max(0, std::min(detailRibbonScroll_, maxScroll));

    const bool scrolls = maxScroll > 0;
    const int nameRight = x + w - (scrolls ? 26 : 16);
    for (int row = 0; row < SUM_RIBBON_ROWS; row++) {
        const int i = detailRibbonScroll_ + row;
        if (i >= total) break;
        const int ry = y + RIB_LIST_OFS + row * RIB_ROW_H;
        const int rcy = ry + RIB_ROW_H / 2;
        constexpr int ICON = 20;
        if (SDL_Texture* tex = getRibbonSprite(ribbons[i].filename)) {
            SDL_Rect dst = {x + 16, rcy - ICON / 2, ICON, ICON};
            SDL_RenderCopy(renderer_, tex, nullptr, &dst);
        }
        // Marks in the dimmer colour, so the two kinds stay apart in one list.
        const int nx = x + 16 + ICON + 10;
        drawText(fitText(ribbons[i].name, fName, nameRight - nx), nx,
                 rcy - TTF_FontHeight(fName) / 2,
                 ribbons[i].isMark ? T().textDim : T().text, fName);
    }

    // Scrollbar when the list is longer than the tile.
    if (scrolls) {
        const int trackY = y + RIB_LIST_OFS, trackH = SUM_RIBBON_ROWS * RIB_ROW_H;
        const int thumbH = std::max(24, trackH * SUM_RIBBON_ROWS / total);
        const int thumbY = trackY + (trackH - thumbH) * detailRibbonScroll_ / maxScroll;
        fillRounded(x + w - 14, trackY, 4, trackH, 2, T().buttonBg);
        fillRounded(x + w - 14, thumbY, 4, thumbH, 2, T().textMuted);
    }
}

// --- Right column ------------------------------------------------------------------

void UI::drawSummaryAttributes(const Pokemon& pkm) {
    const uint8_t tera = pkm.teraType();
    const bool hasTera = tera <= 17 || tera == Pokemon::TERA_STELLAR;
    const int cols = hasTera ? 4 : 3;
    const int colW = COL_W / cols;

    TTF_Font* fLabel = uiFont(11, true);
    auto column = [&](int i, const std::string& label) {
        const int x = COL_X + i * colW;
        drawTextTracked(toUpperUtf8(label), x, 82, T().textDim, fLabel, 2);
        return x;
    };
    // A value that does not fit at 20 drops to 16 before it is cut.
    auto value = [&](const std::string& v, int x, int maxW, SDL_Color c) {
        TTF_Font* f = uiFont(20, true);
        if (textWidth(v, f) > maxW) f = uiFont(16, true);
        drawText(fitText(v, f, maxW), x, baselineTop(f, 120), c, f);
    };

    // Nature, with the stat it raises and lowers underneath.
    {
        const int x = column(0, stripPrefix(i18n::get(StrKey::NaturePrefix)));
        const uint8_t nature = pkm.nature();
        value(NatureName::get(nature), x, colW - 12, T().text);
        const int up = nature / 5, down = nature % 5;
        if (up != down) {
            TTF_Font* f = uiFont(13);
            drawText(std::string("+") + NatureStat::get(up) + "  \xe2\x88\x92" + NatureStat::get(down),
                     x, 125, T().textDim, f);
        }
    }

    {
        const int x = column(1, stripPrefix(i18n::get(StrKey::AbilityPrefix)));
        value(AbilityName::get(pkm.ability()), x, colW - 12, T().text);
    }

    {
        const int x = column(2, stripPrefix(i18n::get(StrKey::HeldItemPrefix)));
        const uint16_t item = pkm.heldItem();
        value(item != 0 ? ItemName::get(item) : i18n::get(StrKey::NoneItem), x,
              (cols == 3 ? MARGIN_R - x : colW - 12), item != 0 ? T().text : T().textMuted);
    }

    // Tera type: Scarlet/Violet only; the byte means something else elsewhere.
    if (hasTera) {
        const int x = column(3, i18n::get(StrKey::SumTera));
        const uint8_t icon = tera <= 17 ? tera : Pokemon::TERA_STELLAR;
        if (SDL_Texture* tex = getTypeSprite(icon))
            blitDisc(renderer_, tex, x + 11, 110, 11, T().bg);
        value(tera <= 17 ? TypeName::get(tera) : "STELLAR", x + 28, MARGIN_R - x - 28, T().text);
    }
}

void UI::drawSummaryMoves(const Pokemon& pkm) {
    TTF_Font* fName = uiFont(19);
    TTF_Font* fType = uiFont(11, true);
    const uint16_t moves[4] = {pkm.move1(), pkm.move2(), pkm.move3(), pkm.move4()};
    for (int i = 0; i < 4; i++) {
        const int x = COL_X + (i % 2) * (MOVE_W + MOVE_GAP);
        const int w = (i % 2) ? MARGIN_R - x : MOVE_W;
        const int y = MOVE_Y[i / 2];
        const int cy = y + MOVE_H / 2;
        fillRounded(x, y, w, MOVE_H, 10, T().panelBg);
        strokeRounded(x, y, w, MOVE_H, 10, 1, T().panelBorder);

        constexpr int ICON_R = 12;
        const int nameX = x + 12 + ICON_R * 2 + 10;
        if (moves[i] == 0) {
            drawText("---", nameX, cy - TTF_FontHeight(fName) / 2, T().textMuted, fName);
            continue;
        }
        const uint8_t type = getMoveType(moves[i], pkm.gameType_);
        int typeW = 0;
        if (type < 18) {
            if (SDL_Texture* tex = getTypeSprite(type))
                blitDisc(renderer_, tex, x + 12 + ICON_R, cy, ICON_R, T().panelBg);
            const char* label = TypeName::get(type);
            typeW = measureTextTracked(label, fType, 1);
            drawTextTracked(label, x + w - 15 - typeW, cy - TTF_FontHeight(fType) / 2,
                            T().textDim, fType, 1);
            typeW += 12;
        }
        drawText(fitText(MoveName::get(moves[i]), fName, x + w - 15 - typeW - nameX),
                 nameX, cy - TTF_FontHeight(fName) / 2, T().text, fName);
    }
}

void UI::drawSummaryIVs(const Pokemon& pkm) {
    TTF_Font* fLabel = uiFont(11, true);
    drawTextTracked(toUpperUtf8(i18n::get(StrKey::IVs)), COL_X, STATS_LABEL_Y, T().textDim, fLabel, 2);

    // Clockwise from the top, as the old popup had them.
    const int values[6] = {pkm.ivHp(), pkm.ivAtk(), pkm.ivDef(),
                           pkm.ivSpe(), pkm.ivSpD(), pkm.ivSpA()};
    const char* keys[6] = {StrKey::StatHP, StrKey::StatAtk, StrKey::StatDef,
                           StrKey::StatSpe, StrKey::StatSpD, StrKey::StatSpA};
    static constexpr double COS[6] = {0.0, 0.866025, 0.866025, 0.0, -0.866025, -0.866025};
    static constexpr double SIN[6] = {-1.0, -0.5, 0.5, 1.0, 0.5, -0.5};

    // "n x 31" badge, right-aligned over the chart.
    int perfect = 0;
    for (int v : values) if (v == 31) perfect++;
    {
        TTF_Font* f = uiFont(12, true);
        const std::string badge = i18n::fmt(StrKey::InfoPerfectIvs, std::to_string(perfect));
        const int bw = measureTextTracked(badge, f, 1) + 20;
        const int bx = EV_X - 20 - bw, by = STATS_LABEL_Y - 6;
        fillRounded(bx, by, bw, 24, 12, T().badgeBg);
        drawTextTracked(badge, bx + 10, by + 12 - TTF_FontHeight(f) / 2, T().accent, f, 1);
    }

    auto vertex = [&](int i, double scale) {
        return SDL_FPoint{static_cast<float>(IV_CX + IV_RAD * scale * COS[i]),
                          static_cast<float>(IV_CY + IV_RAD * scale * SIN[i])};
    };

    // Guide rings at thirds, and spokes.
    SDL_SetRenderDrawColor(renderer_, T().panelBorder.r, T().panelBorder.g, T().panelBorder.b, 255);
    for (double scale : {1.0 / 3, 2.0 / 3, 1.0}) {
        for (int i = 0; i < 6; i++) {
            const SDL_FPoint a = vertex(i, scale), b = vertex((i + 1) % 6, scale);
            SDL_RenderDrawLineF(renderer_, a.x, a.y, b.x, b.y);
        }
    }
    for (int i = 0; i < 6; i++) {
        const SDL_FPoint a = vertex(i, 1.0);
        SDL_RenderDrawLineF(renderer_, IV_CX, IV_CY, a.x, a.y);
    }

    // The data, as a fan from the centre so a concave shape still fills right.
    SDL_FPoint pts[6];
    for (int i = 0; i < 6; i++)
        pts[i] = vertex(i, std::min(1.0, values[i] / 31.0));
    const SDL_Color teal = T().accentBank;
    const SDL_Color fill = {teal.r, teal.g, teal.b, 70};
    SDL_Vertex v[18];
    for (int i = 0; i < 6; i++) {
        v[i * 3 + 0] = {{static_cast<float>(IV_CX), static_cast<float>(IV_CY)}, fill, {0, 0}};
        v[i * 3 + 1] = {pts[i], fill, {0, 0}};
        v[i * 3 + 2] = {pts[(i + 1) % 6], fill, {0, 0}};
    }
    SDL_RenderGeometry(renderer_, nullptr, v, 18, nullptr, 0);
    SDL_SetRenderDrawColor(renderer_, teal.r, teal.g, teal.b, 230);
    for (int i = 0; i < 6; i++)
        SDL_RenderDrawLineF(renderer_, pts[i].x, pts[i].y, pts[(i + 1) % 6].x, pts[(i + 1) % 6].y);

    // Dots: gold on a perfect 31.
    for (int i = 0; i < 6; i++)
        fillDisc(static_cast<int>(pts[i].x + 0.5f), static_cast<int>(pts[i].y + 0.5f), 4,
                 values[i] == 31 ? T().accent : teal);

    // Labels with their values, just outside the hexagon: "HP 31" on one line
    // when it fits, the name over the value when a translated name is too
    // long for the room beside the chart.
    TTF_Font* f = uiFont(12, true);
    const int lh = TTF_FontHeight(f);
    for (int i = 0; i < 6; i++) {
        const std::string name = toUpperUtf8(i18n::get(keys[i]));
        const std::string val = std::to_string(values[i]);
        const int lx = IV_CX + static_cast<int>((IV_RAD + 14) * COS[i]);
        const int ly = IV_CY + static_cast<int>((IV_RAD + 14) * SIN[i]);

        // Room on the label's side: up to the EV column on the right, the
        // portrait panel on the left; the top and bottom labels are centred.
        int room = 150;
        if (COS[i] > 0.1)  room = EV_X - 10 - lx;
        if (COS[i] < -0.1) room = lx - (SUM_PANEL_X + SUM_PANEL_W + 8);

        const std::string one = name + " " + val;
        const bool twoLines = textWidth(one, f) > room;
        const std::string line1 = twoLines ? fitText(name, f, room) : one;
        const int w1 = textWidth(line1, f), w2 = textWidth(val, f);
        const int blockH = twoLines ? lh * 2 : lh;

        auto left = [&](int w) {
            if (COS[i] > 0.1)  return lx;
            if (COS[i] < -0.1) return lx - w;
            return lx - w / 2;
        };
        int ty = ly - blockH / 2;
        if (SIN[i] < -0.9) ty = ly - blockH;
        if (SIN[i] > 0.9)  ty = ly;
        drawText(line1, left(w1), ty, T().textDim, f);
        if (twoLines)
            drawText(val, left(w2), ty + lh, T().text, f);
    }
}

void UI::drawSummaryEVs(const Pokemon& pkm) {
    // Showdown order, which is what people expect to read on a spread.
    const int evs[6] = {pkm.evHp(), pkm.evAtk(), pkm.evDef(),
                        pkm.evSpA(), pkm.evSpD(), pkm.evSpe()};
    const char* keys[6] = {StrKey::StatHP, StrKey::StatAtk, StrKey::StatDef,
                           StrKey::StatSpA, StrKey::StatSpD, StrKey::StatSpe};
    int total = 0;
    for (int v : evs) total += v;

    TTF_Font* fLabel = uiFont(11, true);
    drawTextTracked(toUpperUtf8(i18n::get(StrKey::EVs)), EV_X, STATS_LABEL_Y, T().textDim, fLabel, 2);
    TTF_Font* fTotal = uiFont(13);
    const std::string totalStr = std::to_string(total) + " / 510";
    drawText(totalStr, MARGIN_R - textWidth(totalStr, fTotal), STATS_LABEL_Y - 2,
             total > 510 ? T().red : T().textDim, fTotal);

    TTF_Font* fStat = uiFont(13, true);
    TTF_Font* fVal  = uiFont(13, true);
    int labelW = 0;
    for (const char* k : keys)
        labelW = std::max(labelW, textWidth(toUpperUtf8(i18n::get(k)), fStat));
    const int barX = EV_X + labelW + 14;
    const int barW = MARGIN_R - 42 - barX;

    for (int i = 0; i < 6; i++) {
        const int y = EV_ROW_Y + i * EV_ROW_STEP;
        const int cy = y + 8;
        drawText(toUpperUtf8(i18n::get(keys[i])), EV_X, cy - TTF_FontHeight(fStat) / 2, T().text, fStat);
        fillRounded(barX, cy - 4, barW, 8, 4, T().buttonBg);
        const int filled = evs[i] > 0 ? std::max(8, barW * std::min(evs[i], 252) / 252) : 0;
        if (filled > 0)
            fillRounded(barX, cy - 4, filled, 8, 4, T().accentBank);
        const std::string v = std::to_string(evs[i]);
        drawText(v, MARGIN_R - textWidth(v, fVal), cy - TTF_FontHeight(fVal) / 2,
                 evs[i] > 0 ? T().text : T().textMuted, fVal);
    }
}

// --- Provenance and identifiers ----------------------------------------------------

void UI::drawSummaryProvenance(const Pokemon& pkm) {
    drawRect(MARGIN_L, PROV_RULE_Y, MARGIN_R - MARGIN_L, 1, T().panelBorder);

    TTF_Font* fLabel = uiFont(11, true);
    TTF_Font* fValue = uiFont(16, true);
    TTF_Font* fTrail = uiFont(13);

    // Fields packed left to right, skipped when unknown.
    int fx = MARGIN_L;
    auto field = [&](const std::string& label, const std::string& value,
                     const std::string& trailing = std::string()) {
        if (value.empty() || fx >= MARGIN_R - 40) return;
        const std::string lab = toUpperUtf8(label);
        drawTextTracked(lab, fx, PROV_LABEL_Y, T().textDim, fLabel, 2);
        const std::string v = fitText(value, fValue, MARGIN_R - fx);
        drawText(v, fx, baselineTop(fValue, PROV_VALUE_Y + 16), T().text, fValue);
        int w = textWidth(v, fValue);
        if (!trailing.empty() && fx + w + 8 < MARGIN_R - 20) {
            const std::string t = fitText(trailing, fTrail, MARGIN_R - (fx + w + 8));
            drawText(t, fx + w + 8, baselineTop(fTrail, PROV_VALUE_Y + 16), T().textDim, fTrail);
            w += 8 + textWidth(t, fTrail);
        }
        fx += std::max(measureTextTracked(lab, fLabel, 2), w) + 36;
    };

    // OT with the displayed TID / SID, as the old popup had them.
    field(stripPrefix(i18n::get(StrKey::OTPrefix)), pkm.otName(),
          std::to_string(pkm.displayTid()) + " / " + std::to_string(pkm.displaySid()));
    if (pkm.hasHandlingTrainer()) {
        const std::string ht = pkm.htName();
        field(stripPrefix(i18n::get(StrKey::HTPrefix)), ht.empty() ? i18n::get(StrKey::NoneItem) : ht);
    }
    field(i18n::get(StrKey::SumOrigin), VersionName::get(pkm.originVersion()));
    if (pkm.metYear() != 0) {
        char date[16];
        std::snprintf(date, sizeof(date), "%04u-%02u-%02u",
                      static_cast<unsigned>(pkm.metYear()),
                      static_cast<unsigned>(pkm.metMonth()),
                      static_cast<unsigned>(pkm.metDay()));
        field(i18n::get(StrKey::SumMet), date);
    }
    field(i18n::get(StrKey::SumLocation), LocationName::get(selectedGame_, pkm.metLocation()));
    field(i18n::get(StrKey::SumLang), LanguageTag::get(pkm.language()));

    // Identifiers, one quiet line: PID, EC, raw 16-bit IDs, TSV.
    const unsigned tsv = static_cast<unsigned>((pkm.tid() ^ pkm.sid()) >> 4);
    char tech[128];
    std::snprintf(tech, sizeof(tech), "PID %08X    EC %08X    ID %05u / %05u    TSV %04u",
                  static_cast<unsigned>(pkm.pid()), static_cast<unsigned>(pkm.encryptionConstant()),
                  static_cast<unsigned>(pkm.tid()), static_cast<unsigned>(pkm.sid()), tsv);
    drawText(tech, MARGIN_L, TECH_Y, T().textMuted, uiFont(13));
}
