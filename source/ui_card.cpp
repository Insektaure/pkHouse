// Shareable Pokemon card renderer.
//
// Draws a fixed 1280x720 card into an offscreen render target and writes it to
// <basePath>/cards/<GameFamily>/ as a PNG. The card deliberately does not
// follow the app theme: an exported card should look the same wherever it
// ends up shared.

#include "ui.h"
#include "species_converter.h"
#include "species_types.h"
#include "met_info.h"
#include "move_types.h"
#include "form_names.h"
#include "card_payload.h"
#include "led.h"
#include "qrcodegen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>

namespace {

constexpr int CARD_W = 1280;
constexpr int CARD_H = 720;

// Layout
constexpr int MARGIN_L   = 46;
constexpr int MARGIN_R   = 1234;
constexpr int TOP_RULE_H = 8;

constexpr int PANEL_X = 46;
constexpr int PANEL_Y = 138;
constexpr int PANEL_W = 600;
constexpr int PANEL_H = 444;

// The portrait panel is split: sprite on the left, QR on the right. 340 is not
// arbitrary -- a full Pokemon payload encodes to a version 15 QR (77 modules),
// and 340 / (77 + 8 quiet) lands exactly on 4 pixels per module, which is the
// smallest that reliably survives recompression.
constexpr int QR_BOX     = 340;
constexpr int PANEL_PAD  = 17;
constexpr int CONTENT_Y  = 202;
constexpr int SPRITE_BOX = 210;

// The importer decodes only this region; keep the two definitions in step.
static_assert(CARD_W == CardLayout::WIDTH && CARD_H == CardLayout::HEIGHT,
              "card size must match CardLayout");
static_assert(PANEL_X + PANEL_W - PANEL_PAD - QR_BOX == CardLayout::QR_X,
              "QR x must match CardLayout::QR_X");
static_assert(CONTENT_Y == CardLayout::QR_Y, "QR y must match CardLayout::QR_Y");
static_assert(QR_BOX == CardLayout::QR_SIZE, "QR size must match CardLayout::QR_SIZE");

constexpr int COL_X    = 676;          // right column
constexpr int COL_W    = MARGIN_R - COL_X;
constexpr int ATTR_COLS = 4;
constexpr int ATTR_W   = COL_W / ATTR_COLS;

constexpr int FOOTER_RULE_Y = 601;

// --- Palette -----------------------------------------------------------------
//
// Taken from the app's Default theme (see theme.cpp) so a card looks like the
// app it came from. It stays a fixed set rather than following the active
// theme: a card is shared, and it should look the same wherever it lands.

constexpr SDL_Color C_BG        = { 30,  30,  40, 255};  // theme bg
constexpr SDL_Color C_PANEL     = { 45,  45,  60, 255};  // theme panelBg
constexpr SDL_Color C_BORDER    = { 58,  58,  78, 255};
constexpr SDL_Color C_RULE      = { 58,  58,  78, 255};
constexpr SDL_Color C_TRACK     = { 57,  57,  76, 255};
constexpr SDL_Color C_TEXT      = {240, 240, 240, 255};  // theme text
constexpr SDL_Color C_DIM       = {160, 160, 170, 255};  // theme textDim
constexpr SDL_Color C_LABEL     = {148, 148, 173, 255};  // 4.6:1 on C_PANEL
constexpr SDL_Color C_ACCENT    = {255, 220,  50, 255};  // theme cursor
constexpr SDL_Color C_GOLD      = {255, 215,   0, 255};  // theme shiny
constexpr SDL_Color C_GOLD_BG   = { 77,  71,  51, 255};  // gold at 15% over C_PANEL
constexpr SDL_Color C_TEAL      = {100, 200, 220, 255};  // theme selected
constexpr SDL_Color C_WARN      = {220, 120, 120, 255};  // theme red, lifted for 4.5:1
constexpr SDL_Color C_DEXNUM    = { 63,  63,  85, 255};
constexpr SDL_Color C_MALE      = {100, 150, 255, 255};  // theme genderMale
constexpr SDL_Color C_FEMALE    = {255, 130, 150, 255};  // theme genderFemale

// The QR tile is deliberately outside the palette. Scanners need real black on
// real white with a quiet zone, so it keeps its own ground whatever the rest of
// the card is painted in.
constexpr SDL_Color C_QR_BG     = {255, 255, 255, 255};

// Standard type colours, indexed by the modern type IDs used in move_types.h.
constexpr SDL_Color TYPE_COLORS[18] = {
    {168, 167, 122, 255}, {194,  46,  40, 255}, {169, 143, 243, 255},
    {163,  62, 161, 255}, {226, 191, 101, 255}, {182, 161,  54, 255},
    {166, 185,  26, 255}, {115,  87, 151, 255}, {183, 183, 206, 255},
    {238, 129,  48, 255}, { 99, 144, 240, 255}, {122, 199,  76, 255},
    {247, 208,  44, 255}, {249,  85, 135, 255}, {150, 217, 214, 255},
    {111,  53, 252, 255}, {112,  87,  70, 255}, {214, 133, 173, 255},
};

constexpr const char* TYPE_NAMES[18] = {
    "NORMAL", "FIGHTING", "FLYING", "POISON", "GROUND", "ROCK",
    "BUG", "GHOST", "STEEL", "FIRE", "WATER", "GRASS",
    "ELECTRIC", "PSYCHIC", "ICE", "DRAGON", "DARK", "FAIRY",
};

// PKHeX Ball enum (PKHeX.Core/Game/Enums/Ball.cs)
constexpr const char* BALL_NAMES[38] = {
    "", "Master Ball", "Ultra Ball", "Great Ball", "Poke Ball", "Safari Ball",
    "Net Ball", "Dive Ball", "Nest Ball", "Repeat Ball", "Timer Ball",
    "Luxury Ball", "Premier Ball", "Dusk Ball", "Heal Ball", "Quick Ball",
    "Cherish Ball", "Fast Ball", "Level Ball", "Lure Ball", "Heavy Ball",
    "Love Ball", "Friend Ball", "Moon Ball", "Sport Ball", "Dream Ball",
    "Beast Ball", "Strange Ball", "Poke Ball", "Great Ball", "Ultra Ball",
    "Feather Ball", "Wing Ball", "Jet Ball", "Heavy Ball", "Leaden Ball",
    "Gigaton Ball", "Origin Ball",
};

// Nature stat order: index 0-4 maps to these stats.
constexpr const char* NATURE_STATS[5] = {"Atk", "Def", "Spe", "SpA", "SpD"};

// Pokemon language byte -> 3-letter tag.
const char* languageTag(uint8_t lang) {
    switch (lang) {
        case 1:  return "JPN";
        case 2:  return "ENG";
        case 3:  return "FRE";
        case 4:  return "ITA";
        case 5:  return "GER";
        case 7:  return "SPA";
        case 8:  return "KOR";
        case 9:  return "CHS";
        case 10: return "CHT";
        default: return "---";
    }
}

// --- Drawing primitives ------------------------------------------------------

void fillRect(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

// Filled rounded rectangle, drawn scanline by scanline.
void fillRoundRect(SDL_Renderer* r, int x, int y, int w, int h, int rad, SDL_Color c) {
    if (w <= 0 || h <= 0) return;
    rad = std::min(rad, std::min(w, h) / 2);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int i = 0; i < h; i++) {
        int inset = 0;
        if (rad > 0) {
            int dy = -1;
            if (i < rad)            dy = rad - 1 - i;
            else if (i >= h - rad)  dy = i - (h - rad);
            if (dy >= 0)
                inset = rad - static_cast<int>(std::sqrt(static_cast<double>(rad * rad - dy * dy)));
        }
        SDL_RenderDrawLine(r, x + inset, y + i, x + w - 1 - inset, y + i);
    }
}

// Rounded rectangle with a 1px border, painted as border-then-fill.
void panelRect(SDL_Renderer* r, int x, int y, int w, int h, int rad,
               SDL_Color fill, SDL_Color border) {
    fillRoundRect(r, x, y, w, h, rad, border);
    fillRoundRect(r, x + 1, y + 1, w - 2, h - 2, rad - 1, fill);
}

void fillCircle(SDL_Renderer* r, int cx, int cy, int rad, SDL_Color c) {
    if (rad <= 0) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int dy = -rad; dy <= rad; dy++) {
        int dx = static_cast<int>(std::sqrt(static_cast<double>(rad * rad - dy * dy)));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

enum Align { AlignL, AlignC, AlignR };

int textW(TTF_Font* f, const std::string& s) {
    if (!f || s.empty()) return 0;
    int w = 0, h = 0;
    TTF_SizeUTF8(f, s.c_str(), &w, &h);
    return w;
}

void drawTx(SDL_Renderer* r, TTF_Font* f, const std::string& s,
            int x, int y, SDL_Color c, Align align = AlignL) {
    if (!f || s.empty()) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s.c_str(), c);
    if (!surf) return;
    SDL_Rect dst = {x, y, surf->w, surf->h};
    if (align == AlignC)      dst.x = x - surf->w / 2;
    else if (align == AlignR) dst.x = x - surf->w;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;
    SDL_RenderCopy(r, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

// Draws text scaled down (never up) to fit inside a box, anchored to the box's
// bottom edge and to whichever side alignRight selects. Used for the dex
// number, which sits in a corner of the portrait panel rather than behind the
// sprite so that all of it stays readable.
void drawTxFit(SDL_Renderer* r, TTF_Font* f, const std::string& s,
               int bx, int by, int bw, int bh, SDL_Color c, bool alignRight = false) {
    if (!f || s.empty() || bw <= 0 || bh <= 0) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s.c_str(), c);
    if (!surf) return;
    int w = surf->w, h = surf->h;
    if (w > 0 && h > 0) {
        // Shrink on whichever axis overflows the most, keeping the aspect ratio.
        if (w * bh > h * bw) {
            h = h * bw / w;
            w = bw;
        } else if (h > bh) {
            w = w * bh / h;
            h = bh;
        }
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;
    SDL_Rect dst = {alignRight ? bx + bw - w : bx, by + bh - h, w, h};
    SDL_RenderCopy(r, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

// Width of a string once letter spacing is applied, matching drawTracked.
int trackedW(TTF_Font* f, const std::string& s, int track = 1) {
    int w = 0;
    for (char ch : s) {
        char buf[2] = {ch, '\0'};
        w += textW(f, buf) + track;
    }
    return w > 0 ? w - track : 0;
}

// Micro labels are drawn glyph by glyph so they can carry letter spacing,
// which SDL_ttf has no setting for. ASCII only, which is all the labels use.
void drawTracked(SDL_Renderer* r, TTF_Font* f, const std::string& s,
                 int x, int y, SDL_Color c, int track = 1) {
    if (!f) return;
    for (char ch : s) {
        char buf[2] = {ch, '\0'};
        if (ch != ' ')
            drawTx(r, f, buf, x, y, c);
        x += textW(f, buf) + track;
    }
}

// Scales a texture to fit inside a box, preserving aspect ratio.
void blitFit(SDL_Renderer* r, SDL_Texture* tex, int bx, int by, int bw, int bh) {
    if (!tex) return;
    int tw = 0, th = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
    if (tw <= 0 || th <= 0) return;
    float scale = std::min(static_cast<float>(bw) / tw, static_cast<float>(bh) / th);
    int dw = static_cast<int>(tw * scale);
    int dh = static_cast<int>(th * scale);
    SDL_Rect dst = {bx + (bw - dw) / 2, by + (bh - dh) / 2, dw, dh};
    SDL_RenderCopy(r, tex, nullptr, &dst);
}

// Renders a QR code into a box, pure black on pure white, with at least four
// modules of quiet zone. Module edges are kept on whole pixels: scaling a small
// code up afterwards is what actually breaks scanners, far more than JPEG does.
void drawQr(SDL_Renderer* r, const uint8_t* qrcode, int bx, int by, int box) {
    const int size = qrcodegen_getSize(qrcode);
    if (size <= 0) return;

    int scale = box / (size + 8);
    if (scale < 1) scale = 1;
    const int drawn = size * scale;
    const int ox = bx + (box - drawn) / 2;
    const int oy = by + (box - drawn) / 2;

    // Batch each horizontal run of dark modules into one rect.
    std::vector<SDL_Rect> rects;
    rects.reserve(static_cast<size_t>(size) * size / 4);
    for (int y = 0; y < size; y++) {
        int runStart = -1;
        for (int x = 0; x <= size; x++) {
            const bool dark = x < size && qrcodegen_getModule(qrcode, x, y);
            if (dark && runStart < 0) {
                runStart = x;
            } else if (!dark && runStart >= 0) {
                rects.push_back({ox + runStart * scale, oy + y * scale,
                                 (x - runStart) * scale, scale});
                runStart = -1;
            }
        }
    }
    if (rects.empty()) return;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderFillRects(r, rects.data(), static_cast<int>(rects.size()));
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
}

} // anonymous namespace

// --- Fonts -------------------------------------------------------------------

bool UI::openCardFonts(CardFonts& f) {
    auto open = [&](int size, bool bold) -> TTF_Font* {
        TTF_Font* font = nullptr;
        if (fontData_ && fontDataSize_)
            font = TTF_OpenFontRW(SDL_RWFromMem(fontData_, fontDataSize_), 1, size);
        if (!font)
            font = TTF_OpenFont("romfs:/fonts/default.ttf", size);
        if (font && bold)
            TTF_SetFontStyle(font, TTF_STYLE_BOLD);
        return font;
    };

    f.title     = open(46, true);
    f.level     = open(34, true);
    f.dex       = open(24, true);
    f.value     = open(20, true);
    f.move      = open(18, false);
    f.body      = open(16, false);
    f.small     = open(13, false);
    f.micro     = open(11, true);
    f.watermark = open(150, true);

    if (!f.title || !f.value || !f.body || !f.micro) {
        closeCardFonts(f);
        return false;
    }
    return true;
}

void UI::closeCardFonts(CardFonts& f) {
    for (TTF_Font* font : {f.title, f.level, f.dex, f.value, f.move,
                           f.body, f.small, f.micro, f.watermark}) {
        if (font) TTF_CloseFont(font);
    }
    f = CardFonts{};
}

// --- Header ------------------------------------------------------------------

void UI::drawCardHeader(const Pokemon& pkm, const CardFonts& f) {
    SDL_Renderer* r = renderer_;

    fillRect(r, 0, 0, CARD_W, TOP_RULE_H, C_ACCENT);

    const bool shiny = pkm.isShiny();
    uint16_t species = pkm.isEgg() ? 0 : pkm.species();

    // Species name, preceded by the shiny mark when there is one.
    int nameX = MARGIN_L;
    if (shiny && iconShiny_) {
        SDL_SetTextureColorMod(iconShiny_, C_GOLD.r, C_GOLD.g, C_GOLD.b);
        SDL_Rect dst = {nameX, 52, 30, 30};
        SDL_RenderCopy(r, iconShiny_, nullptr, &dst);
        SDL_SetTextureColorMod(iconShiny_, 255, 255, 255);
        nameX += 42;
    }
    drawTx(r, f.title, SpeciesName::get(species), nameX, 38,
           shiny ? C_GOLD : C_TEXT);

    // Subtitle: nickname and/or form name.
    std::string subtitle;
    if (!pkm.isEgg()) {
        std::string nick = pkm.nickname();
        if (pkm.isNicknamed() && !nick.empty() && nick != SpeciesName::get(species))
            subtitle = "\xe2\x80\x9c" + nick + "\xe2\x80\x9d";
        if (const char* form = getFormName(species, pkm.form())) {
            if (!subtitle.empty()) subtitle += "  \xc2\xb7  ";
            subtitle += form;
        }
    }
    drawTx(r, f.small, subtitle, MARGIN_L, 98, C_DIM);

    // Right cluster, laid out right to left so every part keeps its spacing.
    int rx = MARGIN_R;

    char dexBuf[16];
    std::snprintf(dexBuf, sizeof(dexBuf), "#%04u", static_cast<unsigned>(species));
    drawTx(r, f.dex, dexBuf, rx, 64, C_TEXT, AlignR);
    rx -= textW(f.dex, dexBuf) + 20;

    fillRect(r, rx, 58, 1, 38, C_RULE);
    rx -= 20;

    uint8_t ballId = pkm.ball();
    if (ballId > 0 && ballId < 38) {
        drawTx(r, f.body, BALL_NAMES[ballId], rx, 70, C_TEXT, AlignR);
        rx -= textW(f.body, BALL_NAMES[ballId]) + 8;
        if (SDL_Texture* ballTex = getBallSprite(ballId)) {
            SDL_Rect dst = {rx - 26, 67, 26, 26};
            SDL_RenderCopy(r, ballTex, nullptr, &dst);
        }
        rx -= 26 + 22;
    }

    uint8_t gender = pkm.gender();
    if (gender == 0 || gender == 1) {
        const bool male = gender == 0;
        SDL_Color tint = male ? C_MALE : C_FEMALE;
        SDL_Color bg = {tint.r, tint.g, tint.b, 26};
        fillRoundRect(r, rx - 38, 58, 38, 38, 10, bg);
        drawTx(r, f.value, male ? "\xe2\x99\x82" : "\xe2\x99\x80", rx - 19, 64, tint, AlignC);
        rx -= 38 + 22;
    }

    std::string lvl = std::to_string(pkm.level());
    drawTx(r, f.level, lvl, rx, 58, C_TEXT, AlignR);
    rx -= textW(f.level, lvl) + 8;
    drawTx(r, f.small, "Lv.", rx, 78, C_DIM, AlignR);
}

// --- Portrait panel ----------------------------------------------------------

void UI::drawCardPortrait(const Pokemon& pkm, const CardFonts& f) {
    SDL_Renderer* r = renderer_;
    panelRect(r, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 16, C_PANEL, C_BORDER);

    const bool egg = pkm.isEgg();
    const uint16_t species = egg ? 0 : pkm.species();

    // Encode the Pokemon before laying anything out: without a QR the sprite
    // takes the whole panel instead of sharing it.
    static constexpr int QR_MAX_VERSION = 20;
    static constexpr int QR_BUF = qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION);
    uint8_t qrcode[QR_BUF];
    bool hasQr = false;
    {
        std::vector<uint8_t> payload = CardPayload::build(pkm, selectedGame_);
        if (!payload.empty() && payload.size() <= static_cast<size_t>(QR_BUF)) {
            uint8_t temp[QR_BUF];
            std::memcpy(temp, payload.data(), payload.size());
            hasQr = qrcodegen_encodeBinary(temp, payload.size(), qrcode,
                                           qrcodegen_Ecc_MEDIUM, 1, QR_MAX_VERSION,
                                           qrcodegen_Mask_AUTO, true);
        }
    }

    const int spriteBox = hasQr ? SPRITE_BOX : 300;
    const int spriteX   = hasQr ? PANEL_X + PANEL_PAD
                                : PANEL_X + (PANEL_W - spriteBox) / 2;

    // Status badges: shiny, alpha, Gigantamax, egg. The first two are flat mask
    // icons that take the badge's tint. The other two are real artwork, drawn
    // in their own colours when set and only tinted down when they are not;
    // neither is square, so both are fitted rather than dropped into the
    // icons' box.
    struct Badge { bool active; SDL_Texture* icon; const char* glyph; bool artwork; };
    // The glyph is the fallback for a texture that failed to load, so a missing
    // romfs file leaves the row legible rather than blank.
    const Badge badges[4] = {
        {pkm.isShiny(),        iconShiny_,    "S", false},
        {pkm.isAlpha(),        iconAlpha_,    "A", false},
        {pkm.canGigantamax(),  iconDynamax_,  "G", true },
        {egg,                  getSprite(0),  "E", true },
    };
    constexpr SDL_Color C_BADGE_OFF = {120, 120, 144, 255};
    for (int i = 0; i < 4; i++) {
        int bx = PANEL_X + PANEL_PAD + i * 48;
        int by = PANEL_Y + 18;
        const bool on = badges[i].active;
        panelRect(r, bx, by, 40, 40, 12,
                  on ? C_GOLD_BG : C_BG, on ? C_GOLD : C_BORDER);
        SDL_Color tint = on ? C_GOLD : C_BADGE_OFF;
        if (badges[i].icon) {
            const bool keepColours = badges[i].artwork && on;
            if (!keepColours)
                SDL_SetTextureColorMod(badges[i].icon, tint.r, tint.g, tint.b);
            if (badges[i].artwork) {
                blitFit(r, badges[i].icon, bx + 5, by + 5, 30, 30);
            } else {
                SDL_Rect dst = {bx + 10, by + 10, 20, 20};
                SDL_RenderCopy(r, badges[i].icon, nullptr, &dst);
            }
            SDL_SetTextureColorMod(badges[i].icon, 255, 255, 255);
        } else if (badges[i].glyph) {
            drawTx(r, f.value, badges[i].glyph, bx + 20, by + 7, tint, AlignC);
        }
    }

    // Species sprite, form and shiny aware.
    SDL_Texture* sprite = nullptr;
    if (egg) {
        sprite = getSprite(0);
    } else if (pkm.isShiny()) {
        sprite = getShinySprite(species, pkm.form());
        if (!sprite) sprite = getSprite(species, pkm.form());
    } else {
        sprite = getSprite(species, pkm.form());
    }
    blitFit(r, sprite, spriteX, CONTENT_Y, spriteBox, spriteBox);

    // QR code, on its own pure white tile so the quiet zone really is white.
    if (hasQr) {
        const int qx = PANEL_X + PANEL_W - PANEL_PAD - QR_BOX;
        panelRect(r, qx, CONTENT_Y, QR_BOX, QR_BOX, 10, C_QR_BG, C_BORDER);
        drawQr(r, qrcode, qx, CONTENT_Y, QR_BOX);
        const int capRight = PANEL_X + PANEL_W - PANEL_PAD;
        drawTracked(r, f.micro, "POKEMON DATA",
                    capRight - trackedW(f.micro, "POKEMON DATA"),
                    CONTENT_Y + QR_BOX + 6, C_LABEL);
    }

    // Type chips: stacked under the sprite when the QR shares the panel,
    // side by side when the sprite has it to itself.
    int chipsBottom = hasQr ? CONTENT_Y + spriteBox + 14 : PANEL_Y + PANEL_H - 44;
    if (!egg) {
        SpeciesTypes::Pair types = SpeciesTypes::get(selectedGame_, species, pkm.form());
        int cx = spriteX;
        int cy = chipsBottom;
        for (uint8_t t : {types.t1, types.t2}) {
            if (t >= 18) continue;
            const char* label = TYPE_NAMES[t];
            int chipW = 28 + textW(f.micro, label) + 14;
            panelRect(r, cx, cy, chipW, 26, 13, C_BG, C_BORDER);
            fillCircle(r, cx + 16, cy + 13, 5, TYPE_COLORS[t]);
            drawTx(r, f.micro, label, cx + 28, cy + 8, C_TEXT);
            if (hasQr) { cy += 32; chipsBottom = cy; }
            else       { cx += chipW + 10; }
        }
    }

    // National dex number, in the panel's bottom corner rather than behind the
    // sprite, so none of it is hidden. It takes whatever room the type chips
    // leave: bottom-left in the split layout, bottom-right when the sprite has
    // the panel to itself and the chips run along the bottom-left.
    if (!egg) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(species));
        const int boxBottom = PANEL_Y + PANEL_H - PANEL_PAD;
        const int boxTop = hasQr ? chipsBottom + 8 : CONTENT_Y + spriteBox + 8;
        if (boxBottom - boxTop >= 24) {
            if (hasQr)
                drawTxFit(r, f.watermark, buf, spriteX, boxTop,
                          spriteBox, boxBottom - boxTop, C_DEXNUM);
            else
                drawTxFit(r, f.watermark, buf, PANEL_X + PANEL_W / 2, boxTop,
                          PANEL_W / 2 - PANEL_PAD, boxBottom - boxTop, C_DEXNUM, true);
        }
    }
}

// --- Attribute row -----------------------------------------------------------

void UI::drawCardAttributes(const Pokemon& pkm, const CardFonts& f) {
    SDL_Renderer* r = renderer_;
    constexpr int LABEL_Y = 138;
    constexpr int VALUE_Y = 154;

    int col = 0;
    auto column = [&](const char* label) {
        int x = COL_X + col * ATTR_W;
        drawTracked(r, f.micro, label, x, LABEL_Y, C_LABEL);
        col++;
        return x;
    };

    // Nature, with the stat it raises and lowers underneath.
    {
        int x = column("NATURE");
        uint8_t nature = pkm.nature();
        drawTx(r, f.value, NatureName::get(nature), x, VALUE_Y, C_TEXT);
        int up = nature / 5, down = nature % 5;
        if (up != down) {
            std::string mod = std::string("+") + NATURE_STATS[up] + " -" + NATURE_STATS[down];
            drawTx(r, f.small, mod, x, VALUE_Y + 27, C_DIM);
        }
    }

    // Ability
    {
        int x = column("ABILITY");
        drawTx(r, f.value, AbilityName::get(pkm.ability()), x, VALUE_Y, C_TEXT);
    }

    // Held item
    {
        int x = column("HELD ITEM");
        uint16_t item = pkm.heldItem();
        panelRect(r, x, VALUE_Y + 3, 20, 20, 6, C_BG, C_BORDER);
        drawTx(r, f.value, item != 0 ? ItemName::get(item) : "---",
               x + 28, VALUE_Y, item != 0 ? C_TEXT : C_DIM);
    }

    // Tera type: Scarlet/Violet only; the byte means something else elsewhere.
    uint8_t tera = pkm.teraType();
    if (tera <= 17) {
        int x = column("TERA TYPE");
        fillCircle(r, x + 10, VALUE_Y + 13, 10, TYPE_COLORS[tera]);
        drawTx(r, f.value, TYPE_NAMES[tera], x + 28, VALUE_Y, C_TEXT);
    } else if (tera == Pokemon::TERA_STELLAR) {
        int x = column("TERA TYPE");
        fillCircle(r, x + 10, VALUE_Y + 13, 10, C_TEAL);
        drawTx(r, f.value, "STELLAR", x + 28, VALUE_Y, C_TEXT);
    }
}

// --- Moves -------------------------------------------------------------------

void UI::drawCardMoves(const Pokemon& pkm, const CardFonts& f) {
    SDL_Renderer* r = renderer_;
    constexpr int MOVE_W = 274;
    constexpr int MOVE_H = 40;
    constexpr int ROW_Y[2] = {211, 263};

    const uint16_t moves[4] = {pkm.move1(), pkm.move2(), pkm.move3(), pkm.move4()};
    for (int i = 0; i < 4; i++) {
        int x = COL_X + (i % 2) * (MOVE_W + 10);
        int y = ROW_Y[i / 2];
        panelRect(r, x, y, MOVE_W, MOVE_H, 10, C_PANEL, C_BORDER);

        constexpr int ICON = 24;
        constexpr int NAME_X = 10 + ICON + 8;

        if (moves[i] == 0) {
            drawTx(r, f.move, "---", x + NAME_X, y + 9, C_DIM);
            continue;
        }
        uint8_t type = getMoveType(moves[i], selectedGame_);

        // The same type icons the detail view uses; the coloured dot stays as a
        // fallback so a missing icon still marks the move's type.
        SDL_Texture* typeTex = (type < 18) ? getTypeSprite(type) : nullptr;
        if (typeTex) {
            SDL_Rect dst = {x + 10, y + (MOVE_H - ICON) / 2, ICON, ICON};
            SDL_RenderCopy(r, typeTex, nullptr, &dst);
        } else if (type < 18) {
            fillCircle(r, x + 10 + ICON / 2, y + MOVE_H / 2, 9, TYPE_COLORS[type]);
        }

        drawTx(r, f.move, MoveName::get(moves[i]), x + NAME_X, y + 9, C_TEXT);
        if (type < 18)
            drawTx(r, f.micro, TYPE_NAMES[type], x + MOVE_W - 14, y + 14, C_LABEL, AlignR);
    }
}

// --- IV radar ----------------------------------------------------------------

void UI::drawCardIVs(const Pokemon& pkm, const CardFonts& f) {
    SDL_Renderer* r = renderer_;
    drawTracked(r, f.micro, "IVS", COL_X, 329, C_LABEL);

    // Clockwise from the top, matching the detail popup.
    const int values[6] = {pkm.ivHp(), pkm.ivAtk(), pkm.ivDef(),
                           pkm.ivSpe(), pkm.ivSpD(), pkm.ivSpA()};
    const char* labels[6] = {"HP", "ATK", "DEF", "SPE", "SPD", "SPA"};
    static constexpr double COS[6] = {0.0, 0.866025, 0.866025, 0.0, -0.866025, -0.866025};
    static constexpr double SIN[6] = {-1.0, -0.5, 0.5, 1.0, 0.5, -0.5};

    constexpr int CX = COL_X + 130;
    constexpr int CY = 452;
    constexpr int RAD = 78;

    auto vertex = [&](int i, double scale, int& x, int& y) {
        x = CX + static_cast<int>(RAD * scale * COS[i]);
        y = CY + static_cast<int>(RAD * scale * SIN[i]);
    };

    // Guide rings at 50% and 100%, plus spokes.
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (double scale : {0.5, 1.0}) {
        SDL_SetRenderDrawColor(r, C_BORDER.r, C_BORDER.g, C_BORDER.b, 255);
        for (int i = 0; i < 6; i++) {
            int x0, y0, x1, y1;
            vertex(i, scale, x0, y0);
            vertex((i + 1) % 6, scale, x1, y1);
            SDL_RenderDrawLine(r, x0, y0, x1, y1);
        }
    }
    SDL_SetRenderDrawColor(r, C_BORDER.r, C_BORDER.g, C_BORDER.b, 255);
    for (int i = 0; i < 6; i++) {
        int x, y;
        vertex(i, 1.0, x, y);
        SDL_RenderDrawLine(r, CX, CY, x, y);
    }

    // Data polygon, filled as a triangle fan so concave shapes still work.
    SDL_Point pts[6];
    for (int i = 0; i < 6; i++) {
        double frac = std::min(1.0, values[i] / 31.0);
        vertex(i, frac, pts[i].x, pts[i].y);
    }
    SDL_SetRenderDrawColor(r, C_TEAL.r, C_TEAL.g, C_TEAL.b, 38);
    for (int i = 0; i < 6; i++) {
        int j = (i + 1) % 6;
        int minY = std::min({CY, pts[i].y, pts[j].y});
        int maxY = std::max({CY, pts[i].y, pts[j].y});
        SDL_Point tri[3] = {{CX, CY}, pts[i], pts[j]};
        for (int y = minY; y <= maxY; y++) {
            int lo = 1 << 30, hi = -(1 << 30);
            for (int e = 0; e < 3; e++) {
                const SDL_Point& a = tri[e];
                const SDL_Point& b = tri[(e + 1) % 3];
                if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
                    int x = a.x + static_cast<int>(
                        static_cast<long long>(y - a.y) * (b.x - a.x) / (b.y - a.y));
                    lo = std::min(lo, x);
                    hi = std::max(hi, x);
                }
            }
            if (lo <= hi) SDL_RenderDrawLine(r, lo, y, hi, y);
        }
    }
    SDL_SetRenderDrawColor(r, C_TEAL.r, C_TEAL.g, C_TEAL.b, 230);
    for (int i = 0; i < 6; i++) {
        int j = (i + 1) % 6;
        SDL_RenderDrawLine(r, pts[i].x, pts[i].y, pts[j].x, pts[j].y);
    }

    // Vertex dots: gold on a perfect 31, teal otherwise.
    for (int i = 0; i < 6; i++)
        fillCircle(r, pts[i].x, pts[i].y, 4, values[i] == 31 ? C_GOLD : C_TEAL);

    // Axis labels, pushed just outside the hexagon.
    for (int i = 0; i < 6; i++) {
        int lx = CX + static_cast<int>((RAD + 26) * COS[i]);
        int ly = CY + static_cast<int>((RAD + 18) * SIN[i]) - 7;
        drawTx(r, f.micro, labels[i], lx, ly, C_LABEL, AlignC);
    }

    // "n x 31" badge for the number of perfect IVs.
    int perfect = 0;
    for (int v : values) if (v == 31) perfect++;
    if (perfect > 0) {
        std::string badge = std::to_string(perfect) + " \xc3\x97 31";
        int w = textW(f.micro, badge) + 24;
        panelRect(r, COL_X + 204, 320, w, 22, 11, C_GOLD_BG, C_GOLD_BG);
        drawTx(r, f.micro, badge, COL_X + 204 + w / 2, 326, C_GOLD, AlignC);
    }
}

// --- EV bars -----------------------------------------------------------------

void UI::drawCardEVs(const Pokemon& pkm, const CardFonts& f) {
    SDL_Renderer* r = renderer_;
    constexpr int EV_X    = 962;
    constexpr int BAR_X   = 1002;
    constexpr int BAR_W   = 192;
    constexpr int ROW_Y   = 382;
    constexpr int ROW_STEP = 29;

    // Showdown ordering, which is what people expect to read on a spread.
    const int evs[6] = {pkm.evHp(), pkm.evAtk(), pkm.evDef(),
                        pkm.evSpA(), pkm.evSpD(), pkm.evSpe()};
    const char* labels[6] = {"HP", "ATK", "DEF", "SPA", "SPD", "SPE"};

    int total = 0;
    for (int v : evs) total += v;

    drawTracked(r, f.micro, "EVS", EV_X, 357, C_LABEL);
    drawTx(r, f.small, std::to_string(total) + " / 510", MARGIN_R, 353,
           total > 510 ? C_WARN : C_DIM, AlignR);

    for (int i = 0; i < 6; i++) {
        int y = ROW_Y + i * ROW_STEP;
        drawTx(r, f.micro, labels[i], EV_X, y + 3, C_LABEL);
        fillRoundRect(r, BAR_X, y + 4, BAR_W, 8, 4, C_TRACK);
        int filled = evs[i] > 0 ? std::max(6, BAR_W * std::min(evs[i], 252) / 252) : 0;
        if (filled > 0)
            fillRoundRect(r, BAR_X, y + 4, filled, 8, 4, C_TEAL);
        drawTx(r, f.small, std::to_string(evs[i]), MARGIN_R, y, C_TEXT, AlignR);
    }
}

// --- Footer ------------------------------------------------------------------

void UI::drawCardFooter(const Pokemon& pkm, const CardFonts& f) {
    SDL_Renderer* r = renderer_;
    constexpr int LABEL_Y = 623;
    constexpr int VALUE_Y = 639;

    fillRect(r, MARGIN_L, FOOTER_RULE_Y, MARGIN_R - MARGIN_L, 1, C_RULE);

    // Provenance fields, packed left to right and skipped when unknown.
    int fx = MARGIN_L;
    auto field = [&](const char* label, const std::string& value,
                     const std::string& trailing = "") {
        if (value.empty()) return;
        drawTracked(r, f.micro, label, fx, LABEL_Y, C_LABEL);
        drawTx(r, f.body, value, fx, VALUE_Y, C_TEXT);
        int valueW = textW(f.body, value);
        if (!trailing.empty()) {
            drawTx(r, f.small, trailing, fx + valueW + 8, VALUE_Y + 3, C_DIM);
            valueW += 8 + textW(f.small, trailing);
        }
        fx += std::max(trackedW(f.micro, label, 1), valueW) + 30;
    };

    // Only the public trainer ID goes on the card.
    field("OT", pkm.otName(), std::to_string(pkm.displayTid()));
    field("ORIGIN", VersionName::get(pkm.originVersion()));

    if (pkm.metYear() != 0) {
        char date[16];
        std::snprintf(date, sizeof(date), "%04u-%02u-%02u",
                      static_cast<unsigned>(pkm.metYear()),
                      static_cast<unsigned>(pkm.metMonth()),
                      static_cast<unsigned>(pkm.metDay()));
        field("MET", date);
    }
    field("LOCATION", LocationName::get(selectedGame_, pkm.metLocation()));
    field("LANG", languageTag(pkm.language()));

    // Ribbons and marks: up to five icons, then a count.
    auto ribbons = pkm.getRibbonsAndMarks();
    if (!ribbons.empty()) {
        int rx = MARGIN_L + 2;
        int shown = std::min<int>(5, static_cast<int>(ribbons.size()));
        for (int i = 0; i < shown; i++) {
            if (SDL_Texture* tex = getRibbonSprite(ribbons[i].filename)) {
                SDL_Rect dst = {rx, 674, 18, 18};
                SDL_RenderCopy(r, tex, nullptr, &dst);
            }
            rx += 26;
        }
        int marks = 0;
        for (const auto& rb : ribbons) if (rb.isMark) marks++;
        int plain = static_cast<int>(ribbons.size()) - marks;
        std::string summary;
        if (plain > 0)
            summary = std::to_string(plain) + (plain == 1 ? " ribbon" : " ribbons");
        if (marks > 0) {
            if (!summary.empty()) summary += " \xc2\xb7 ";
            summary += std::to_string(marks) + (marks == 1 ? " mark" : " marks");
        }
        drawTx(r, f.small, summary, rx + 8, 677, C_DIM);
    }

    // Wordmark, bottom right.
    int wx = MARGIN_R;
    drawTx(r, f.small, "v" APP_VERSION, wx, 679, C_LABEL, AlignR);
    wx -= textW(f.small, "v" APP_VERSION) + 12;
    drawTracked(r, f.micro, "PKHOUSE", wx - trackedW(f.micro, "PKHOUSE", 2), 681, C_DIM, 2);
}

// --- Export ------------------------------------------------------------------

std::string UI::exportPokemonCard(const Pokemon& pkm) {
    CardFonts f;
    if (!openCardFonts(f)) return "";
    std::string name = renderPokemonCard(pkm, f);
    closeCardFonts(f);
    return name;
}

// Renders one card with fonts the caller owns. Opening the nine sizes costs
// real time, so exporting a boxful opens them once and calls this per Pokemon.
std::string UI::renderPokemonCard(const Pokemon& pkm, const CardFonts& f) {
    if (pkm.isEmpty()) return "";

    // Use a format the renderer advertises: not every backend can make a render
    // target out of an arbitrary one.
    SDL_RendererInfo info;
    Uint32 format = SDL_PIXELFORMAT_ARGB8888;
    if (SDL_GetRendererInfo(renderer_, &info) == 0 && info.num_texture_formats > 0)
        format = info.texture_formats[0];

    SDL_Texture* target = SDL_CreateTexture(renderer_, format,
                                            SDL_TEXTUREACCESS_TARGET, CARD_W, CARD_H);
    if (!target && format != SDL_PIXELFORMAT_ARGB8888)
        target = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_TARGET, CARD_W, CARD_H);
    if (!target)
        return "";

    SDL_SetRenderTarget(renderer_, target);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    fillRect(renderer_, 0, 0, CARD_W, CARD_H, C_BG);

    drawCardHeader(pkm, f);
    drawCardPortrait(pkm, f);
    drawCardAttributes(pkm, f);
    drawCardMoves(pkm, f);
    drawCardIVs(pkm, f);
    drawCardEVs(pkm, f);
    drawCardFooter(pkm, f);

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, CARD_W, CARD_H, 32,
                                                       SDL_PIXELFORMAT_RGBA32);
    bool read = surf && SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_RGBA32,
                                             surf->pixels, surf->pitch) == 0;

    SDL_SetRenderTarget(renderer_, nullptr);
    SDL_DestroyTexture(target);
    markDirty();

    if (!read) {
        if (surf) SDL_FreeSurface(surf);
        return "";
    }

    // Filename: readable, and unique through the encryption constant.
    uint16_t species = pkm.isEgg() ? 0 : pkm.species();
    std::string name = SpeciesName::get(species);
    if (const char* form = getFormName(species, pkm.form()))
        name += std::string(" (") + form + ")";

    std::string flags;
    if (pkm.isShiny()) flags += "S";
    if (pkm.isAlpha()) flags += "A";
    if (pkm.isEgg())   flags += "E";

    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s - %s%s%s%s - %08X.png",
                  name.c_str(), gameInfo(selectedGame_).gameTag,
                  flags.empty() ? "" : " - [", flags.c_str(),
                  flags.empty() ? "" : "]",
                  static_cast<unsigned>(pkm.encryptionConstant()));

    std::string filename = buf;
    for (char& c : filename) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    }

    // One folder per game family, matching banks/, export/ and wondercards/.
    // The importer leans on this: a card can only ever be listed for the family
    // it belongs to.
    std::string dir = basePath_ + "cards/";
    std::string gameDir = dir + bankFolderNameOf(selectedGame_) + "/";
    mkdir(dir.c_str(), 0755);
    mkdir(gameDir.c_str(), 0755);

    ledBlink();
    bool ok = IMG_SavePNG(surf, (gameDir + filename).c_str()) == 0;
    SDL_FreeSurface(surf);
    ledOff();

    return ok ? filename : "";
}
