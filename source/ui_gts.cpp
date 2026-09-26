// The online GTS: a public board reached from the row above the game icons.
//
// It sits outside the game/bank flow on purpose. Nothing has been selected at
// that point, so the board shows every game's deposits at once and what you
// take off it is saved as a card rather than dropped straight into a save,
// which is also what makes it useful, since a card can be imported into
// whichever game you open next.
//
// Every request here BLOCKS behind showWorking(). Each one is a single call the
// user asked for by pressing a button, and one page of the grid is exactly one
// request: see the static_assert tying GTS_PER_PAGE to Gts::PAGE_SIZE.

#include "ui.h"
#include "gts.h"
#include "card_payload.h"
#include "card_import.h"
#include "i18n.h"
#include "species_converter.h"
#include "form_names.h"
#include "met_info.h"
#include "ui_util.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// The game families the board knows, in the order they appear in the filter.
// The representative GameType is only used to look a display name up, so any
// member of the family will do.
struct FamilyOption {
    const char* key;      // what the board calls it; "" means every family
    GameType    rep;
};

constexpr FamilyOption FAMILIES[] = {
    {"",     GameType::ZA},
    {"za",   GameType::ZA},
    {"sv",   GameType::S},
    {"swsh", GameType::Sw},
    {"bdsp", GameType::BD},
    {"la",   GameType::LA},
    {"lgpe", GameType::GP},
    {"frlg", GameType::FR},
};
constexpr int FAMILY_COUNT = static_cast<int>(sizeof(FAMILIES) / sizeof(FAMILIES[0]));

int familyIndex(const std::string& key) {
    for (int i = 0; i < FAMILY_COUNT; i++)
        if (key == FAMILIES[i].key) return i;
    return 0;
}

// Rows in the search filter. Reset and Search are keys (X / Y) now, as on
// the box search, rather than rows.
enum FilterRow {
    ROW_SPECIES = 0, ROW_FAMILY, ROW_SHINY, ROW_EGG, ROW_MIN_IVS,
    ROW_LEGAL, ROW_SORT, ROW_ALPHA, ROW_BALL, ROW_COUNT
};

// Drawn as two columns; the D-pad walks them as drawn.
constexpr int GTS_LEFT_ROWS[]  = {ROW_SPECIES, ROW_FAMILY, ROW_BALL, ROW_MIN_IVS};
constexpr int GTS_RIGHT_ROWS[] = {ROW_SHINY, ROW_EGG, ROW_ALPHA, ROW_LEGAL, ROW_SORT};

// The balls the filter offers: any, then every ball that has a name, in
// the games' own order.
std::vector<int> ballChoices() {
    std::vector<int> v = {-1};
    for (int b = 1; b <= 37; b++)
        if (BallName::get(static_cast<uint8_t>(b))[0] != '\0') v.push_back(b);
    return v;
}

int stepBall(int ball, int dir) {
    const auto v = ballChoices();
    int i = 0;
    for (int k = 0; k < (int)v.size(); k++) if (v[k] == ball) { i = k; break; }
    return v[(i + dir + (int)v.size()) % (int)v.size()];
}
constexpr int GTS_LEFT_N  = static_cast<int>(sizeof(GTS_LEFT_ROWS) / sizeof(int));
constexpr int GTS_RIGHT_N = static_cast<int>(sizeof(GTS_RIGHT_ROWS) / sizeof(int));

// Where a row sits: column 0 / 1 and its place in it.
void gtsRowPos(int row, int& col, int& idx) {
    for (int i = 0; i < GTS_LEFT_N; i++)  if (GTS_LEFT_ROWS[i] == row)  { col = 0; idx = i; return; }
    for (int i = 0; i < GTS_RIGHT_N; i++) if (GTS_RIGHT_ROWS[i] == row) { col = 1; idx = i; return; }
    col = 0; idx = 0;
}

// A tri-state filter as the segments show it: any, yes, no; A steps right.
int nextTri(int v) { return v < 0 ? 1 : (v == 1 ? 0 : -1); }
int triSegment(int v) { return v < 0 ? 0 : (v == 1 ? 1 : 2); }

// The IV totals worth offering. 186 is six perfect, 150 is roughly five.
constexpr int IV_STEPS[] = {-1, 90, 120, 150, 180, 186};
constexpr int IV_STEP_COUNT = static_cast<int>(sizeof(IV_STEPS) / sizeof(IV_STEPS[0]));

int ivStepIndex(int value) {
    for (int i = 0; i < IV_STEP_COUNT; i++)
        if (IV_STEPS[i] == value) return i;
    return 0;
}

// --- primitives for the band's artwork ---------------------------------------
//
// Scanline shapes rather than a texture: the band is drawn only when the game
// selector redraws, which is on input, and a few hundred filled rects is
// nothing next to keeping a 1160x48 PNG in romfs and in VRAM for one screen.

// A disc with a feathered rim.
void fillCircle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c) {
    if (radius <= 0) return;

    for (int py = cy - radius - 1; py <= cy + radius + 1; py++) {
        const double dy = py + 0.5 - cy;

        const double inner = (radius - 0.5) * (radius - 0.5) - dy * dy;
        const int solid = inner > 0.0
            ? static_cast<int>(std::floor(std::sqrt(inner) - 0.5))
            : -1;
        if (solid >= 0) {
            SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
            SDL_Rect span = {cx - solid, py, solid * 2 + 1, 1};
            SDL_RenderFillRect(r, &span);
        }

        for (int px = cx - radius - 1; px <= cx + radius + 1; px++) {
            if (solid >= 0 && px >= cx - solid && px <= cx + solid)
                continue;   // already covered by the span above

            const double dx = px + 0.5 - cx;
            const double covered = radius + 0.5 - std::sqrt(dx * dx + dy * dy);
            if (covered <= 0.0) continue;

            const double fraction = covered >= 1.0 ? 1.0 : covered;
            SDL_SetRenderDrawColor(r, c.r, c.g, c.b,
                                   static_cast<Uint8>(c.a * fraction + 0.5));
            SDL_RenderDrawPoint(r, px, py);
        }
    }
}

// One outline of an ellipse, plotted by walking y. Used for the globe: its
// outline is a circle, and each meridian is the same ellipse squeezed
// horizontally, which is what makes a flat drawing read as a sphere.
void ellipseOutline(SDL_Renderer* r, int cx, int cy, int a, int b, SDL_Color c) {
    if (a <= 0 || b <= 0) return;

    auto plot = [&](double px, int py) {
        const int left = static_cast<int>(std::floor(px));
        const double frac = px - left;
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b,
                               static_cast<Uint8>(c.a * (1.0 - frac) + 0.5));
        SDL_RenderDrawPoint(r, left, py);
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b,
                               static_cast<Uint8>(c.a * frac + 0.5));
        SDL_RenderDrawPoint(r, left + 1, py);
    };

    for (int dy = -b; dy <= b; dy++) {
        const double k = 1.0 - static_cast<double>(dy * dy) / (static_cast<double>(b) * b);
        if (k < 0.0) continue;
        const double dx = a * std::sqrt(k);
        plot(cx - dx, cy + dy);
        plot(cx + dx, cy + dy);
    }
}

// A latitude line: a horizontal chord of the sphere. Axis-aligned, so there is
// nothing to smooth.
void chord(SDL_Renderer* r, int cx, int cy, int radius, int dy, SDL_Color c) {
    const double k = 1.0 - static_cast<double>(dy * dy) / (static_cast<double>(radius) * radius);
    if (k < 0.0) return;
    const int dx = static_cast<int>(radius * std::sqrt(k));
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
}

// A stroke of a given thickness, laid down as overlapping discs.
void thickLine(SDL_Renderer* r, int x1, int y1, int x2, int y2, int thickness, SDL_Color c) {
    const double dx = x2 - x1, dy = y2 - y1;
    const int steps = static_cast<int>(std::max(std::abs(dx), std::abs(dy))) + 1;
    for (int i = 0; i <= steps; i++) {
        const double s = static_cast<double>(i) / steps;
        fillCircle(r, static_cast<int>(x1 + dx * s + 0.5),
                      static_cast<int>(y1 + dy * s + 0.5), thickness / 2, c);
    }
}

SDL_Color withAlpha(SDL_Color c, uint8_t a) { return {c.r, c.g, c.b, a}; }

} // anonymous namespace

// --- Entering -----------------------------------------------------------------

void UI::enterGts() {
    // Brought up here rather than at launch: socketInitializeDefault() reserves
    // a buffer pool and most sessions never open the board.
    const bool ready = Gts::init(basePath_);

    if (!Gts::configured()) {
        // Said plainly rather than failing at the first request: an unset
        // address is a setting, not a fault, and the message has to say which
        // file to create because there is nowhere in the app to type it.
        showMessageAndWait(i18n::get(StrKey::GtsNotConfigured),
                           i18n::get(StrKey::GtsNotConfiguredBody));
        return;
    }

    if (!ready) {
        showMessageAndWait(i18n::get(StrKey::GtsConnectFailed), Gts::lastError());
        return;
    }

    showWorking(i18n::get(StrKey::GtsConnecting));
    if (!Gts::hello()) {
        showMessageAndWait(i18n::get(StrKey::GtsConnectFailed), Gts::lastError());
        return;
    }

    gtsHubCursor_ = 0;
    showGtsFilter_ = false;
    showGtsDeposit_ = false;
    screen_ = AppScreen::GtsHub;
    markDirty();
}

// --- the motif ----------------------------------------------------------------

// A wireframe sphere, clipped to `clip`.
//
// Drawn larger than whatever it sits in, so what shows is a slice through the
// middle: that is what makes a flat drawing read as a globe rather than as a
// circle with lines on it. The meridians are the same circle squeezed
// horizontally, so they crowd towards the edges the way longitude does.
void UI::drawGtsGlobe(const SDL_Rect& clip, int cx, int cy, int radius, uint8_t wire) {
    SDL_RenderSetClipRect(renderer_, &clip);

    const SDL_Color line = withAlpha(T().text, wire);
    const SDL_Color rim  = withAlpha(T().text, static_cast<uint8_t>(
        std::min(255, wire * 3 / 2)));

    ellipseOutline(renderer_, cx, cy, radius, radius, rim);
    for (int i = 1; i <= 3; i++)
        ellipseOutline(renderer_, cx, cy, radius * i / 4, radius, line);
    for (int dy = -radius + radius / 4; dy < radius; dy += radius / 4)
        chord(renderer_, cx, cy, radius, dy, line);

    SDL_RenderSetClipRect(renderer_, nullptr);
}

// Two points and a dotted route between them.
//
// `lift` is how far above the straight line the route bows. A dot every other
// sample, so it reads as a route rather than as a wire.
void UI::drawGtsLink(int ax, int ay, int bx, int by, int lift, uint8_t trail,
                     int dotA, int dotB) {
    const SDL_Color dotted = withAlpha(T().goldLabel, trail);
    const int cx = (ax + bx) / 2;
    const int cy = (ay + by) / 2 - lift;

    for (int step = 0; step <= 40; step += 2) {
        const double s = step / 40.0, inv = 1.0 - s;
        const int px = static_cast<int>(inv * inv * ax + 2 * inv * s * cx + s * s * bx);
        const int py = static_cast<int>(inv * inv * ay + 2 * inv * s * cy + s * s * by);
        fillCircle(renderer_, px, py, 1, dotted);
    }
    if (dotA > 0) fillCircle(renderer_, ax, ay, dotA, T().goldLabel);
    if (dotB > 0) fillCircle(renderer_, bx, by, dotB, T().goldLabel);
}

// A scatter of stars, at positions derived from `seed` rather than from a
// random source: the same call has to put them in the same place every redraw,
// or the background shimmers whenever anything else changes.
void UI::drawGtsStars(const SDL_Rect& area, int count, uint32_t seed) {
    const SDL_Color star = withAlpha(T().text, 90);
    uint32_t state = seed | 1u;
    for (int i = 0; i < count; i++) {
        state = state * 1664525u + 1013904223u;
        const int sx = area.x + static_cast<int>((state >> 16) % static_cast<uint32_t>(area.w));
        state = state * 1664525u + 1013904223u;
        const int sy = area.y + static_cast<int>((state >> 16) % static_cast<uint32_t>(area.h));
        drawRect(sx, sy, 2, 2, star);
    }
}

// --- The row above the game icons ---------------------------------------------
// Drawn rather than shipped as a PNG, so the band is navy and amber in the
// default and follows the palette everywhere else.
// --- Hub ----------------------------------------------------------------------

// UI 2.0: the same three choices as before - Browse, Search, Deposit -
// with the same labels and keys, restyled. Nothing on it changes while it is
// open: no board contents, no counts.
void UI::drawGtsHubFrame() {
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);

    // Prefixed: UI has constants of its own that a plain name would resolve to.
    constexpr int HUB_L = 32, HUB_R = SCREEN_W - 32;
    constexpr int HUB_TOP = 84, HUB_BOT = FOOTER_Y - 18;
    constexpr int HUB_GAP = 22;
    constexpr int HUB_LEFT_W = 742;

    // --- Top bar: the logo, then the GTS mark with its title and subtitle. ---
    {
        drawRect(0, 0, SCREEN_W, ACCENT_RULE_H, T().accent);
        const int cy = 42;
        int x = drawLogo(32, cy) + 16;
        drawRect(x, cy - 14, 1, 28, T().divider);
        x += 17;

        const int mx = x + 20;
        fillCircle(renderer_, mx, cy, 20, T().accent);
        ellipseOutline(renderer_, mx, cy, 11, 11, T().keyCapText);
        ellipseOutline(renderer_, mx, cy,  5, 11, T().keyCapText);
        chord(renderer_, mx, cy, 11, 0, T().keyCapText);
        x += 40 + 12;

        TTF_Font* fT = uiFont(20, true);
        TTF_Font* fS = uiFont(13);
        drawText(i18n::get(StrKey::GtsTitle), x, cy - 22, T().text, fT);
        drawText(i18n::get(StrKey::GtsSubtitle), x, cy + 4, T().textDim, fS);

        // The current profile, as on the game selector.
        drawProfileChip(SCREEN_W - 32, cy);
    }

    auto panel = [&](const SDL_Rect& r, bool focused) {
        if (focused)
            strokeRounded(r.x - 6, r.y - 6, r.w + 12, r.h + 12, 24, 3, T().accent);
        fillRounded(r.x, r.y, r.w, r.h, 18, focused ? T().slotFull : T().panelBg);
        strokeRounded(r.x, r.y, r.w, r.h, 18, 1, T().panelBorder);
    };

    // --- Browse: the whole left column. ---
    {
        const SDL_Rect r = {HUB_L, HUB_TOP, HUB_LEFT_W, HUB_BOT - HUB_TOP};
        const bool focused = gtsHubCursor_ == 0;
        panel(r, focused);

        // The globe, under the heading, inside the rounded corners.
        const SDL_Rect art = {r.x + 18, r.y + 120, r.w - 36, r.h - 138};
        drawGtsStars(art, 18, 0x60BEu);
        drawGtsGlobe(art, r.x + r.w / 2, art.y + art.h / 2 + 10, 215, focused ? 40 : 28);
        SDL_RenderSetClipRect(renderer_, &art);
        drawGtsLink(r.x + 150, art.y + art.h - 70, r.x + r.w - 130, art.y + 60, 70,
                    focused ? 170 : 110, 5, 4);
        SDL_RenderSetClipRect(renderer_, nullptr);

        // A board of listings, as the mark.
        const int ix = r.x + 26, iy = r.y + 26;
        fillRounded(ix, iy, 60, 60, 14, T().badgeBg);
        for (int k = 0; k < 4; k++) {
            const int sx = ix + 16 + (k % 2) * 16, sy = iy + 16 + (k / 2) * 16;
            strokeRounded(sx, sy, 12, 12, 3, 2, T().accent);
        }
        TTF_Font* fT = uiFont(28, true);
        TTF_Font* fS = uiFont(16);
        const int tx = ix + 60 + 16, tw = r.x + r.w - 26 - tx;
        drawText(fitText(i18n::get(StrKey::GtsBrowseBtn), fT, tw), tx, iy + 2, T().text, fT);
        drawText(fitText(i18n::get(StrKey::GtsBrowseHint), fS, tw), tx, iy + 38, T().textDim, fS);
    }

    // --- Search and Deposit, stacked on the right. ---
    const int rx = HUB_L + HUB_LEFT_W + HUB_GAP + 4, rw = HUB_R - rx;
    const int halfH = (HUB_BOT - HUB_TOP - HUB_GAP) / 2;
    struct HubButton { int cursor; const char* label; const char* hint; };
    const HubButton buttons[] = {
        {1, StrKey::GtsSearchBtn,  StrKey::GtsSearchHint},
        {2, StrKey::GtsDepositBtn, StrKey::GtsDepositHint},
    };
    for (int i = 0; i < 2; i++) {
        const SDL_Rect r = {rx, HUB_TOP + i * (halfH + HUB_GAP), rw, halfH};
        const bool focused = gtsHubCursor_ == buttons[i].cursor;
        panel(r, focused);

        // A route in the bottom corner, pointing the way the action goes:
        // searching brings something in, depositing sends something out.
        {
            const SDL_Rect clip = {r.x + 12, r.y + 12, r.w - 24, r.h - 24};
            SDL_RenderSetClipRect(renderer_, &clip);
            const uint8_t trail = focused ? 150 : 90;
            const int outX = r.x + r.w - 44, inX = r.x + r.w - 154, base = r.y + r.h - 34;
            if (i == 0) drawGtsLink(outX, base - 20, inX, base, 18, trail, 3, 5);   // in
            else        drawGtsLink(inX, base, outX, base - 20, 18, trail, 5, 3);   // out
            SDL_RenderSetClipRect(renderer_, nullptr);
        }

        // The mark: a disc with the action's symbol.
        const int cx = r.x + 54, cy = r.y + 54;
        fillCircle(renderer_, cx, cy, 30, T().accent);
        const SDL_Color ink = T().keyCapText;
        if (i == 0) {
            if (SDL_Texture* ic = uiIcon("search")) {
                SDL_SetTextureColorMod(ic, ink.r, ink.g, ink.b);
                SDL_Rect dst = {cx - 12, cy - 12, 24, 24};
                SDL_RenderCopy(renderer_, ic, nullptr, &dst);
                SDL_SetTextureColorMod(ic, 255, 255, 255);
            }
        } else {
            // An arrow leaving a line: something of yours going out.
            thickLine(renderer_, cx, cy + 6, cx, cy - 11, 4, ink);
            thickLine(renderer_, cx, cy - 12, cx - 8, cy - 4, 4, ink);
            thickLine(renderer_, cx, cy - 12, cx + 8, cy - 4, 4, ink);
            drawRect(cx - 11, cy + 10, 22, 3, ink);
        }

        TTF_Font* fT = uiFont(24, true);
        TTF_Font* fS = uiFont(15);
        const int tx = r.x + 100, tw = r.x + r.w - 50 - tx;
        drawText(fitText(i18n::get(buttons[i].label), fT, tw), tx, cy - 30, T().text, fT);
        drawText(fitText(i18n::get(buttons[i].hint), fS, tw), tx, cy + 4, T().textDim, fS);
        fillArrow(static_cast<float>(r.x + r.w - 28), static_cast<float>(cy), 6, ArrowDir::Right,
                  focused ? T().text : T().textDim);
    }

    {
        const ButtonHint hints[] = {
            {"A", StrKey::HintSelect3}, {"B", StrKey::HintBack2},
        };
        drawFooterBar(hints, 2);
    }

    if (showGtsFilter_)  drawGtsFilterPopup();
    if (showGtsDeposit_) drawGtsDepositPopup();
    if (showSpeciesLetterPicker_) drawSpeciesLetterPicker();
}

void UI::handleGtsHubInput(bool& running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) { running = false; return; }
        if (event.type == SDL_CONTROLLERBUTTONDOWN) markDirty();

        // Popups get the event instead, innermost first.
        if (showSpeciesLetterPicker_) { handleSpeciesLetterPickerInput(event); continue; }
        if (showGtsFilter_)           { handleGtsFilterInput(event);           continue; }
        if (showGtsDeposit_)          { handleGtsDepositInput(event);          continue; }

        if (event.type == SDL_CONTROLLERAXISMOTION) {
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                updateStick(SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX),
                            SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY));
            }
            continue;
        }

        if (event.type != SDL_CONTROLLERBUTTONDOWN) continue;

        switch (event.cbutton.button) {
            // Left button is one tall panel facing two stacked ones, so left
            // and right cross between the columns and up/down moves within the
            // right-hand one.
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  gtsHubCursor_ = 0; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: if (gtsHubCursor_ == 0) gtsHubCursor_ = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:    if (gtsHubCursor_ == 2) gtsHubCursor_ = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  if (gtsHubCursor_ == 1) gtsHubCursor_ = 2; break;

            case SDL_CONTROLLER_BUTTON_B: // Switch A = select
                if (gtsHubCursor_ == 0) {
                    // Browse: whatever the filter currently says, which is
                    // everything the scanner has judged until Search changes it.
                    if (gtsLoadPage(0)) {
                        gtsCursor_ = 0;
                        screen_ = AppScreen::GtsBrowse;
                    }
                } else if (gtsHubCursor_ == 1) {
                    showGtsFilter_ = true;
                    gtsFilterCursor_ = ROW_SPECIES;
                } else {
                    gtsScanAllCards();
                    if (gtsCards_.empty()) {
                        showMessageAndWait(i18n::get(StrKey::GtsNoCards),
                                           i18n::get(StrKey::GtsNoCardsBody));
                    } else {
                        showGtsDeposit_ = true;
                        gtsCardCursor_ = 0;
                        gtsCardScroll_ = 0;
                        gtsCardPreviewIdx_ = -1;
                        gtsCardPreview_ = CardPayload::Parsed{};
                        gtsCardPreviewSince_ = SDL_GetTicks();
                    }
                }
                break;

            case SDL_CONTROLLER_BUTTON_A: // Switch B = back
                screen_ = AppScreen::GameSelector;
                break;

            case SDL_CONTROLLER_BUTTON_START:
                if (confirmQuit()) running = false;
                break;
        }
    }

    handleStickRepeat();
}

// --- Loading ------------------------------------------------------------------

bool UI::gtsLoadPage(int offset) {
    showWorking(i18n::get(StrKey::GtsLoading));

    Gts::Page page;
    if (!Gts::search(gtsFilter_, offset, page)) {
        showMessageAndWait(i18n::get(StrKey::GtsFailed), Gts::lastError());
        return false;
    }

    // An empty first page is "nothing matched", not a failure; an empty later
    // page means the board shrank under us, so step back rather than showing a
    // blank grid with no way to tell why.
    if (page.entries.empty()) {
        if (offset == 0) {
            showMessageAndWait(i18n::get(StrKey::GtsTitle),
                               i18n::get(gtsFilter_.isDefault() ? StrKey::GtsEmpty
                                                                : StrKey::GtsNoResults),
                               DialogKind::Info);
            return false;
        }
        return false;
    }

    gtsPage_ = std::move(page);
    gtsOffset_ = offset;

    // A new page means new listings; nothing cached is reachable from it.
    gtsFetched_.clear();
    gtsFetchedGame_.clear();
    if (gtsCursor_ >= static_cast<int>(gtsPage_.entries.size()))
        gtsCursor_ = static_cast<int>(gtsPage_.entries.size()) - 1;
    markDirty();
    return true;
}

// --- Browse grid --------------------------------------------------------------

std::string UI::gtsEntryLabel(const Gts::Entry& e) const {
    if (e.egg) return i18n::get(StrKey::Egg);
    if (!e.nickname.empty()) return e.nickname;
    std::string name = SpeciesName::get(e.species);
    return name.empty() ? std::to_string(e.species) : name;
}

// The board's full legality report for a listing, in a dialog of its own: the
// info strip only has room for its start. It is already in the listing, so
// this asks the board nothing and counts nothing. Up / down scroll a report
// longer than the dialog.
void UI::showGtsReport(const Gts::Entry& e) {
    if (!renderer_ || e.legalityReport.empty()) return;
    markDirty();

    constexpr int RP_W = 760, RP_PAD = 24, RP_BADGE = 56, RP_LINE = 21, RP_VISIBLE = 14, RP_BTN_H = 52;
    TTF_Font* fTitle = uiFont(22, true);
    TTF_Font* fTag   = uiFont(11, true);
    TTF_Font* fBody  = uiFont(15);
    const int textX = RP_PAD + RP_BADGE + 16, textW = RP_W - textX - RP_PAD;

    // Every line of the report, wrapped; nothing cut.
    std::vector<std::string> lines;
    {
        std::string para;
        auto flush = [&]() {
            while (!para.empty() && (para.back() == ' ' || para.back() == '\r' || para.back() == '\t')) para.pop_back();
            if (para.empty()) { if (!lines.empty() && !lines.back().empty()) lines.push_back(std::string()); }
            else for (auto& l : wrapText(para, fBody, textW, 1000)) lines.push_back(l);
            para.clear();
        };
        for (char ch : e.legalityReport) {
            if (ch == '\n') flush();
            else para.push_back(ch == '\t' ? ' ' : ch);
        }
        flush();
        while (!lines.empty() && lines.back().empty()) lines.pop_back();
    }
    const int total = static_cast<int>(lines.size());
    const int shown = std::min(total, RP_VISIBLE);
    const int maxScroll = std::max(0, total - RP_VISIBLE);

    const char* verdictKey = e.legality == "illegal" ? StrKey::GtsVerdictIllegal
                           : e.legality == "pending" ? StrKey::GtsVerdictPending
                           : e.legality == "legal"   ? StrKey::GtsVerdictLegal
                                                     : StrKey::GtsVerdictUnknown;
    const SDL_Color vc = gtsVerdictColor(e);
    const std::string title = fitText(gtsEntryLabel(e), fTitle, textW);

    int scroll = 0;
    bool waiting = true, redraw = true;
    while (waiting) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) waiting = false;
            if (event.type != SDL_CONTROLLERBUTTONDOWN) continue;
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_B:   // Switch A
                case SDL_CONTROLLER_BUTTON_A:   // Switch B
                    waiting = false;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    if (scroll > 0) { scroll--; redraw = true; }
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    if (scroll < maxScroll) { scroll++; redraw = true; }
                    break;
            }
        }
        if (redraw && waiting) {
            drawDialogBackdrop();
            const int headH = 18 + TTF_FontHeight(fTitle) + 10;
            const int h = RP_PAD + headH + shown * RP_LINE + 20 + RP_BTN_H + RP_PAD;
            const int x = (SCREEN_W - RP_W) / 2, y = (SCREEN_H - h) / 2;
            fillRounded(x, y, RP_W, h, 20, T().panelBg);
            strokeRounded(x, y, RP_W, h, 20, 1, T().panelBorder);

            // Badge in the verdict's colour, then the verdict and the listing.
            const int bcx = x + RP_PAD + RP_BADGE / 2, bcy = y + RP_PAD + RP_BADGE / 2;
            fillDisc(bcx, bcy, RP_BADGE / 2, SDL_Color{vc.r, vc.g, vc.b, 50});
            if (iconWarn_) {
                SDL_SetTextureColorMod(iconWarn_, vc.r, vc.g, vc.b);
                SDL_Rect dst = {bcx - 13, bcy - 14, 26, 26};
                SDL_RenderCopy(renderer_, iconWarn_, nullptr, &dst);
                SDL_SetTextureColorMod(iconWarn_, 255, 255, 255);
            }
            int ty = y + RP_PAD;
            drawTextTracked(toUpperUtf8(i18n::get(verdictKey)), x + textX, ty, vc, fTag, 2);
            ty += 18;
            drawText(title, x + textX, ty, T().text, fTitle);
            ty += TTF_FontHeight(fTitle) + 10;

            for (int i = 0; i < shown; i++)
                drawText(lines[scroll + i], x + textX, ty + i * RP_LINE, T().textDim, fBody);
            if (maxScroll > 0) {
                const int trackY = ty, trackH = shown * RP_LINE;
                const int thumbH = std::max(24, trackH * RP_VISIBLE / total);
                const int thumbY = trackY + (trackH - thumbH) * scroll / maxScroll;
                fillRounded(x + RP_W - 14, trackY, 4, trackH, 2, T().buttonBg);
                fillRounded(x + RP_W - 14, thumbY, 4, thumbH, 2, T().textMuted);
            }

            // OK, and how to scroll when there is more.
            const int by = y + h - RP_PAD - RP_BTN_H;
            TTF_Font* fBtn = uiFont(16, true);
            const std::string ok = i18n::get(StrKey::DlgOk);
            const int bw = std::max(160, 24 + 10 + textWidth(ok, fBtn) + 48);
            const int bx = x + RP_W - RP_PAD - bw, bcy2 = by + RP_BTN_H / 2;
            fillRounded(bx, by, bw, RP_BTN_H, 12, T().accent);
            const int gx = bx + (bw - (24 + 10 + textWidth(ok, fBtn))) / 2;
            fillDisc(gx + 12, bcy2, 12, T().keyCapText);
            drawTextCentered("A", gx + 12, bcy2, T().accent, uiFont(13, true));
            drawText(ok, gx + 34, bcy2 - TTF_FontHeight(fBtn) / 2, T().keyCapText, fBtn);
            if (maxScroll > 0) {
                TTF_Font* fH = uiFont(15, true);
                const int hx = x + RP_PAD;
                const int kw = drawFooterKey(hx, bcy2, HINT_DPAD, false);
                drawText(i18n::get(StrKey::HintScroll), hx + kw + 8, bcy2 - TTF_FontHeight(fH) / 2, T().text, fH);
            }
            SDL_RenderPresent(renderer_);
            redraw = false;
        }
        SDL_Delay(16);
    }
}

// Whether A on this listing shows its report rather than fetching it: any
// listing that has not passed, when the board sent a report for it. The board
// refuses to hand those over, so the report is what there is to see.
bool UI::gtsOpensReport(const Gts::Entry& e) const {
    return e.legality != "legal" && !e.legalityReport.empty();
}

// One colour per verdict, shared by the pill in the info strip and the frame
// on the cell, so the two always agree.
SDL_Color UI::gtsVerdictColor(const Gts::Entry& e) const {
    if (e.legality == "legal")   return T().statusOk;
    if (e.legality == "pending") return T().statusWarn;
    if (e.legality == "illegal") return T().red;
    return T().textDim;
}

// UI 2.0 laid out like the box view: one wide panel with the page as
// its "box", the same cells, and the info strip under it. Everything shown
// comes from the listing the board sends; the payload itself is only fetched
// when a listing is opened (A), exactly as before.
void UI::drawGtsBrowseFrame() {
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);

    // --- Top bar: logo, the GTS mark, "Online GTS > Board", the profile. ---
    {
        drawRect(0, 0, SCREEN_W, ACCENT_RULE_H, T().accent);
        const int cy = 38;
        int x = drawLogo(32, cy) + 16;
        drawRect(x, cy - 14, 1, 28, T().divider);
        x += 17;
        const int mx = x + 16;
        fillCircle(renderer_, mx, cy, 16, T().accent);
        ellipseOutline(renderer_, mx, cy, 9, 9, T().keyCapText);
        ellipseOutline(renderer_, mx, cy, 4, 9, T().keyCapText);
        chord(renderer_, mx, cy, 9, 0, T().keyCapText);
        x += 32 + 12;
        TTF_Font* fT = uiFont(18, true);
        const std::string gts = i18n::get(StrKey::GtsTitle);
        drawText(gts, x, cy - TTF_FontHeight(fT) / 2, T().textDim, fT);
        x += textWidth(gts, fT) + 14;
        fillArrow(static_cast<float>(x), static_cast<float>(cy), 4, ArrowDir::Right, T().textMuted);
        x += 14;
        drawText(i18n::get(StrKey::GtsBoard), x, cy - TTF_FontHeight(fT) / 2, T().text, fT);
        drawProfileChip(SCREEN_W - 32, cy);
    }

    const int count = static_cast<int>(gtsPage_.entries.size());
    constexpr int GB_X = INFO_X, GB_W = INFO_W;

    // --- The panel: L / R page keys, what is shown, which page. ---
    {
        fillRounded(GB_X, PANEL_Y, GB_W, PANEL_H, PANEL_RADIUS, T().panelBg);
        strokeRounded(GB_X, PANEL_Y, GB_W, PANEL_H, PANEL_RADIUS, 1, T().panelBorder);

        constexpr int KEY_W = 56, KEY_H = 44, KEY_INSET = 17;
        TTF_Font* keyFont = uiFont(16, true);
        const int keyY = PANEL_Y + KEY_INSET, keyCy = keyY + KEY_H / 2;
        for (int side = 0; side < 2; side++) {
            // Lit only when there is a page that way.
            const bool can = side == 0 ? gtsOffset_ >= Gts::PAGE_SIZE : gtsPage_.more;
            const SDL_Color keyText = can ? T().text : T().textMuted;
            const int kx = side == 0 ? GB_X + KEY_INSET : GB_X + GB_W - KEY_INSET - KEY_W;
            fillRounded(kx, keyY, KEY_W, KEY_H, 8, T().buttonBg);
            strokeRounded(kx, keyY, KEY_W, KEY_H, 8, 1, T().buttonBorder);
            const int kcx = kx + KEY_W / 2;
            if (side == 0) {
                fillArrow(kcx - 8, keyCy, 8, ArrowDir::Left, keyText);
                drawTextCentered("L", kcx + 5, keyCy, keyText, keyFont);
            } else {
                drawTextCentered("R", kcx - 5, keyCy, keyText, keyFont);
                fillArrow(kcx + 8, keyCy, 8, ArrowDir::Right, keyText);
            }
        }

        // Tag: the board, narrowed to what the search asked for, as the old
        // title was.
        const int cx = GB_X + GB_W / 2;
        const int maxW = GB_W - 2 * (KEY_INSET + KEY_W + 12);
        std::string tag = i18n::get(StrKey::GtsTitle);
        if (gtsFilter_.species != 0 && !gtsFilter_.speciesName.empty())
            tag += " \xc2\xb7 " + gtsFilter_.speciesName;
        else if (!gtsFilter_.family.empty())
            tag += " \xc2\xb7 " + std::string(bankGroupNameOf(FAMILIES[familyIndex(gtsFilter_.family)].rep));
        {
            TTF_Font* f = uiFont(12, true);
            const std::string full = toUpperUtf8(tag);
            std::string t = full;
            for (int budget = textWidth(full, f); budget > 0 && measureTextTracked(t, f, 2) > maxW; ) {
                budget -= 8;
                t = fitText(full, f, budget);
            }
            drawTextTracked(t, cx - measureTextTracked(t, f, 2) / 2, PANEL_Y + 15, T().accent, f, 2);
        }
        {
            TTF_Font* fTitle = uiFont(22, true);
            TTF_Font* fCount = uiFont(15);
            const std::string title = i18n::fmt(StrKey::GtsPage, std::to_string(gtsOffset_ / Gts::PAGE_SIZE + 1));
            const std::string cnt = std::to_string(count) + " / " + std::to_string(GTS_PER_PAGE);
            const int tw = textWidth(title, fTitle), cw = textWidth(cnt, fCount) + 8;
            const int x0 = cx - (tw + cw) / 2, baseline = PANEL_Y + 56;
            drawText(title, x0, baseline - TTF_FontAscent(fTitle), T().text, fTitle);
            drawText(cnt, x0 + tw + 8, baseline - TTF_FontAscent(fCount), T().textMuted, fCount);
        }
    }

    // --- The grid: box cells. A listing the board has not passed as legal
    // keeps its frame, the search-match rim, as before.
    {
        const int gridW = GTS_COLS * CELL_W + (GTS_COLS - 1) * CELL_GAP;
        const int x0 = GB_X + (GB_W - gridW) / 2;
        for (int i = 0; i < GTS_PER_PAGE; i++) {
            const int x = x0 + (i % GTS_COLS) * (CELL_W + CELL_GAP);
            const int y = GRID_Y + (i / GTS_COLS) * (CELL_H + CELL_GAP);
            SlotDisplay sd;
            bool framed = false;
            SDL_Color rim{};
            if (i < count) {
                const Gts::Entry& e = gtsPage_.entries[i];
                sd.empty   = false;
                sd.egg     = e.egg;
                sd.shiny   = e.shiny;
                sd.alpha   = e.alpha;
                sd.species = e.species;
                sd.form    = e.form;
                // Framed in the colour of its verdict pill in the info strip.
                if (e.legality != "legal") { framed = true; rim = gtsVerdictColor(e); }
            }
            drawSlot(x, y, sd, i == gtsCursor_ && !gtsDetail_, 0, 0, false);
            if (framed) strokeRounded(x, y, CELL_W, CELL_H, CELL_RADIUS, 2, rim);
        }
    }

    // --- Info strip: the listing under the cursor. ---
    fillRounded(INFO_X, INFO_Y, INFO_W, INFO_H, 12, T().panelBg);
    strokeRounded(INFO_X, INFO_Y, INFO_W, INFO_H, 12, 1, T().panelBorder);
    const int midY = INFO_Y + INFO_H / 2;
    if (gtsCursor_ >= count) {
        TTF_Font* f = uiFont(15);
        drawText(i18n::get(StrKey::InfoEmptySlot), INFO_X + 24, midY - TTF_FontHeight(f) / 2, T().textMuted, f);
    } else {
        const Gts::Entry& e = gtsPage_.entries[gtsCursor_];
        auto top = [](TTF_Font* f, int baseline) { return baseline - TTF_FontAscent(f); };

        // Portrait
        constexpr int PORTRAIT = 56;
        const int portX = INFO_X + 20, portY = midY - PORTRAIT / 2;
        fillRounded(portX, portY, PORTRAIT, PORTRAIT, 10, e.egg ? T().slotEgg : T().slotFull);
        drawSpriteFit(spriteFor(e.species, e.form, e.shiny, e.egg), portX + PORTRAIT / 2, portY + PORTRAIT / 2, 48);

        // Identity: marks, name, gender, level; then nature, ability, ball.
        const int idX = portX + PORTRAIT + 20, idRight = 400;
        {
            TTF_Font* fName = uiFont(20, true);
            TTF_Font* fLv   = uiFont(15);
            const int baseline = INFO_Y + 38;
            int x = idX;
            constexpr int ICON = 16;
            if (e.shiny && iconShiny_) {
                SDL_SetTextureColorMod(iconShiny_, T().accent.r, T().accent.g, T().accent.b);
                SDL_Rect dst = {x, baseline - ICON, ICON, ICON};
                SDL_RenderCopy(renderer_, iconShiny_, nullptr, &dst);
                SDL_SetTextureColorMod(iconShiny_, 255, 255, 255);
                x += ICON + 6;
            }
            if (e.alpha && iconAlpha_) {
                SDL_Rect dst = {x, baseline - ICON, ICON, ICON};
                SDL_RenderCopy(renderer_, iconAlpha_, nullptr, &dst);
                x += ICON + 6;
            }
            std::string lv, gender;
            SDL_Color gc = T().text;
            if (!e.egg) {
                if (e.level > 0) lv = i18n::get(StrKey::LvPrefix) + std::to_string(e.level);
                if (e.gender == 0) { gender = "\xe2\x99\x82"; gc = T().genderMale; }
                if (e.gender == 1) { gender = "\xe2\x99\x80"; gc = T().genderFemale; }
            }
            const int tailW = (gender.empty() ? 0 : textWidth(gender, fName) + 8)
                            + (lv.empty() ? 0 : textWidth(lv, fLv) + 8);
            const std::string name = fitText(gtsEntryLabel(e), fName, idRight - 16 - x - tailW);
            drawText(name, x, top(fName, baseline), T().text, fName);
            x += textWidth(name, fName) + 8;
            if (!gender.empty()) { drawText(gender, x, top(fName, baseline), gc, fName); x += textWidth(gender, fName) + 8; }
            if (!lv.empty()) drawText(lv, x, top(fLv, baseline), T().textDim, fLv);

            TTF_Font* fSub = uiFont(14);
            const int subTop = top(fSub, INFO_Y + 62), subRight = idRight - 16;
            std::string sub;
            if (!e.egg) {
                sub = NatureName::get(e.nature);
                if (e.ability != 0) sub += " \xc2\xb7 " + AbilityName::get(e.ability);
            }
            const char* ball = BallName::get(e.ball);
            if (ball[0] != '\0' && !sub.empty()) sub += " \xc2\xb7 ";
            sub = fitText(sub, fSub, subRight - idX);
            drawText(sub, idX, subTop, T().textDim, fSub);
            int bx = idX + textWidth(sub, fSub);
            if (ball[0] != '\0' && bx < subRight) {
                constexpr int BALL = 16;
                const int subCy = subTop + TTF_FontHeight(fSub) / 2;
                if (SDL_Texture* ballTex = getBallSprite(e.ball)) {
                    SDL_Rect dst = {bx, subCy - BALL / 2, BALL, BALL};
                    SDL_RenderCopy(renderer_, ballTex, nullptr, &dst);
                    bx += BALL + 4;
                }
                if (bx < subRight) drawText(fitText(ball, fSub, subRight - bx), bx, subTop, T().textDim, fSub);
            }
        }
        drawRect(idRight, INFO_Y + 14, 1, INFO_H - 28, T().divider);

        // Middle: the verdict and the game it can go back into; then why it
        // was rejected, or what it holds.
        const int statsX = 840;
        {
            const int x0 = idRight + 20, right = statsX - 20;
            const char* verdictKey = StrKey::GtsVerdictLegal;
            if (e.legality == "pending")      verdictKey = StrKey::GtsVerdictPending;
            else if (e.legality == "illegal") verdictKey = StrKey::GtsVerdictIllegal;
            else if (e.legality != "legal")   verdictKey = StrKey::GtsVerdictUnknown;
            const SDL_Color vc = gtsVerdictColor(e);
            TTF_Font* fV = uiFont(11, true);
            const std::string verdict = toUpperUtf8(i18n::get(verdictKey));
            const int vw = measureTextTracked(verdict, fV, 1) + 16 + 10;
            const int vy = INFO_Y + 16;
            fillRounded(x0, vy, vw, 24, 7, SDL_Color{vc.r, vc.g, vc.b, 40});
            fillDisc(x0 + 10, vy + 12, 3, vc);
            drawTextTracked(verdict, x0 + 18, vy + 12 - TTF_FontHeight(fV) / 2, vc, fV, 1);
            if (e.gameKnown) {
                TTF_Font* fG = uiFont(15, true);
                drawText(fitText(bankGroupNameOf(e.game), fG, right - (x0 + vw + 12)), x0 + vw + 12,
                         vy + 12 - TTF_FontHeight(fG) / 2, T().text, fG);
            }

            TTF_Font* fL = uiFont(14);
            const int lineTop = top(fL, INFO_Y + 62);
            if (!e.legalityReport.empty()) {
                // A rejected Pokemon's spread is beside the point; why it was
                // rejected is the whole of what you need, on the one line.
                std::string report;
                for (char ch : e.legalityReport) {
                    if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
                    if (ch == ' ' && (report.empty() || report.back() == ' ')) continue;
                    report.push_back(ch);
                }
                while (!report.empty() && report.back() == ' ') report.pop_back();
                drawText(fitText(report, fL, right - x0), x0, lineTop, T().red, fL);
            } else if (e.heldItem != 0) {
                std::string label = i18n::get(StrKey::HeldItemPrefix);
                while (!label.empty() && (label.back() == ' ' || label.back() == ':')) label.pop_back();
                drawText(fitText(label + " \xc2\xb7 " + ItemName::get(e.heldItem), fL, right - x0),
                         x0, lineTop, T().textDim, fL);
            }
        }
        drawRect(statsX, INFO_Y + 14, 1, INFO_H - 28, T().divider);

        // IV and EV totals, each with a bar.
        {
            const int x0 = statsX + 20, barW = 150;
            TTF_Font* f = uiFont(13, true);
            auto total = [&](const std::string& label, int value, int max, int baseline) {
                drawText(label, x0, top(f, baseline), T().textDim, f);
                const int by = baseline + 5;
                fillRounded(x0, by, barW, 5, 2, T().bg);
                const int w = barW * std::clamp(value, 0, max) / max;
                if (w > 0) fillRounded(x0, by, std::max(w, 5), 5, 2, value >= max ? T().accent : T().accentBank);
            };
            if (!e.egg) {
                total(i18n::fmt(StrKey::InfoIvTotal, std::to_string(e.ivTotal)), e.ivTotal, 186, INFO_Y + 30);
                total(i18n::fmt(StrKey::InfoEvTotal, std::to_string(e.evTotal)), e.evTotal, 510, INFO_Y + 62);
            }
        }

        // Trainer, then the depositor's note or how many have kept it.
        {
            const int right = INFO_X + INFO_W - 20;
            const int maxW = right - (statsX + 20 + 150 + 20);
            TTF_Font* fOt = uiFont(16, true);
            if (!e.otName.empty()) {
                const std::string ot = fitText(i18n::fmt(StrKey::ChipOt, e.otName), fOt, maxW);
                drawText(ot, right - textWidth(ot, fOt), top(fOt, INFO_Y + 36), T().text, fOt);
            }
            TTF_Font* fN = uiFont(13);
            const bool hasNote = !e.note.empty();
            const std::string line = fitText(hasNote ? e.note
                                                     : i18n::fmt(StrKey::GtsDownloads, std::to_string(e.downloads)),
                                             fN, maxW);
            drawText(line, right - textWidth(line, fN), top(fN, INFO_Y + 60),
                     hasNote ? T().text : T().textDim, fN);
        }
    }

    if (gtsDetail_) {
        const ButtonHint hints[] = {
            {"Y", StrKey::HintSaveAsCard}, {"B", StrKey::HintClose},
        };
        drawFooterBar(hints, 2);
        // The chip names the game family: on the board you are in no game,
        // and the family decides which saves can ever take this Pokemon.
        drawDetailPopup(gtsDetailPkm_, hints, 2,
                        std::string("GTS \xc2\xb7 ") + gameInfo(gtsDetailGame_).gameTag);
    } else {
        const Gts::Entry* cur = gtsCursor_ < count ? &gtsPage_.entries[gtsCursor_] : nullptr;
        const bool report = cur && !cur->legalityReport.empty();
        // On an illegal listing A already shows the reason; X is not repeated.
        const bool aIsReport = cur && gtsOpensReport(*cur);
        std::vector<ButtonHint> hints = {{"A", aIsReport ? StrKey::HintReason : StrKey::HintOpen2}};
        if (report && !aIsReport) hints.push_back({"X", StrKey::HintReason});
        hints.push_back({"L R", StrKey::HintPage2});
        hints.push_back({"B", StrKey::HintBack2});
        drawFooterBar(hints.data(), static_cast<int>(hints.size()));
    }
}

// Wraps in both directions, and never parks on the empty tail of a short last
// page. A member rather than a lambda because the stick repeat lives in
// handleStickRepeat() and the two must agree.
void UI::moveGtsCursor(int dx, int dy) {
    const int count = static_cast<int>(gtsPage_.entries.size());

    int col = gtsCursor_ % GTS_COLS;
    int row = gtsCursor_ / GTS_COLS;
    col += dx;
    row += dy;
    if (col < 0) col = GTS_COLS - 1;
    if (col >= GTS_COLS) col = 0;
    if (row < 0) row = GTS_ROWS - 1;
    if (row >= GTS_ROWS) row = 0;

    int idx = row * GTS_COLS + col;
    if (count > 0 && idx >= count) idx = count - 1;
    gtsCursor_ = idx;
}

void UI::handleGtsBrowseInput(bool& running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) { running = false; return; }
        if (event.type == SDL_CONTROLLERBUTTONDOWN) markDirty();

        if (event.type == SDL_CONTROLLERAXISMOTION) {
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                updateStick(SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX),
                            SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY));
            }
            continue;
        }
        if (event.type != SDL_CONTROLLERBUTTONDOWN) continue;

        // The detail popup owns the buttons while it is up.
        if (gtsDetail_) {
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A:      // Switch B = close
                case SDL_CONTROLLER_BUTTON_B:      // Switch A = close
                    gtsDetail_ = false;
                    break;
                case SDL_CONTROLLER_BUTTON_X:      // Switch Y = save as card
                    gtsSaveCurrentAsCard();
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    scrollDetailRibbons(-1, gtsDetailPkm_);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    scrollDetailRibbons(+1, gtsDetailPkm_);
                    break;
            }
            continue;
        }

        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  moveGtsCursor(-1, 0); break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: moveGtsCursor(+1, 0); break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:    moveGtsCursor(0, -1); break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  moveGtsCursor(0, +1); break;

            case SDL_CONTROLLER_BUTTON_B: // Switch A = open
                // The board refuses to hand over an illegal Pokemon, so
                // opening one would only ever end in that error. Its report
                // is what there is to see.
                // The board only hands over a Pokemon that passed its check.
                // Anything else would end in its refusal, so A shows what
                // there is instead: the report when there is one, otherwise
                // why it cannot be opened.
                if (gtsCursor_ < static_cast<int>(gtsPage_.entries.size())) {
                    const Gts::Entry& e = gtsPage_.entries[gtsCursor_];
                    if (gtsOpensReport(e))
                        showGtsReport(e);
                    else if (e.legality != "legal")
                        showMessageAndWait(gtsEntryLabel(e),
                            i18n::get(e.legality == "pending" ? StrKey::GtsNotDownloadablePending
                                                              : StrKey::GtsNotDownloadableUnchecked),
                            DialogKind::Info);
                    else
                        gtsOpenDetail();
                }
                break;

            case SDL_CONTROLLER_BUTTON_Y: // Switch X = the full legality report
                if (gtsCursor_ < static_cast<int>(gtsPage_.entries.size()))
                    showGtsReport(gtsPage_.entries[gtsCursor_]);
                break;

            case SDL_CONTROLLER_BUTTON_A: // Switch B = back to the hub
                screen_ = AppScreen::GtsHub;
                break;

            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                if (gtsOffset_ >= Gts::PAGE_SIZE)
                    gtsLoadPage(gtsOffset_ - Gts::PAGE_SIZE);
                break;

            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                if (gtsPage_.more)
                    gtsLoadPage(gtsOffset_ + Gts::PAGE_SIZE);
                break;

            case SDL_CONTROLLER_BUTTON_START:
                if (confirmQuit()) running = false;
                break;
        }
    }

    // One dispatcher owns the repeat timing and knows which popup is in front;
    // a second copy here would move whatever is behind the one on screen.
    handleStickRepeat();
}

// --- Detail and card export ---------------------------------------------------

void UI::gtsOpenDetail() {
    if (gtsCursor_ < 0 || gtsCursor_ >= static_cast<int>(gtsPage_.entries.size()))
        return;

    const Gts::Entry& e = gtsPage_.entries[gtsCursor_];

    // Already looked at on this page: show it again without asking the board.
    auto cached = gtsFetched_.find(e.id);
    if (cached != gtsFetched_.end()) {
        gtsDetailPkm_  = cached->second;
        gtsDetailGame_ = gtsFetchedGame_[e.id];
        gtsDetailId_   = e.id;
        detailRibbonScroll_ = 0;
        gtsDetail_ = true;
        markDirty();
        return;
    }

    // The listing carries only what the grid draws, so the whole Pokemon is
    // fetched here.
    Pokemon pkm;
    GameType game = GameType::ZA;
    if (!Gts::fetch(e.id, pkm, game)) {
        showMessageAndWait(i18n::get(StrKey::GtsFailed), Gts::lastError());
        return;
    }

    gtsFetched_[e.id] = pkm;
    gtsFetchedGame_[e.id] = game;

    gtsDetailPkm_  = pkm;
    gtsDetailGame_ = game;
    gtsDetailId_   = e.id;
    detailRibbonScroll_ = 0;
    gtsDetail_ = true;
    markDirty();
}

void UI::gtsSaveCurrentAsCard() {
    if (!gtsDetail_) return;

    // The card renderer writes into cards/<family>/, and which family that is
    // comes from the Pokemon itself rather than from whatever game happens to
    // be selected - nothing is selected here.
    showWorking(i18n::get(StrKey::SavingCard), true);

    const GameType previous = selectedGame_;
    selectedGame_ = gtsDetailGame_;

    const std::string filename = exportPokemonCard(gtsDetailPkm_);

    selectedGame_ = previous;

    if (filename.empty()) {
        showMessageAndWait(i18n::get(StrKey::ExportFailed), i18n::get(StrKey::CouldNotWrite));
        return;
    }
    // Now, and only now, is this a download: somebody kept it. Once per listing,
    // because saving the same card again overwrites the same file and is not a
    // second person taking it.
    if (!gtsDetailId_.empty() && gtsCounted_.insert(gtsDetailId_).second)
        Gts::markDownloaded(gtsDetailId_);

    showMessageAndWait(i18n::get(StrKey::Exported),
                       std::string(bankFolderNameOf(gtsDetailGame_)) + "/" + filename, DialogKind::Success);
}

// --- Search filter ------------------------------------------------------------

// UI 2.0, the box search's layout: two columns of rows, the active
// filters as chips, X Reset / Y Search. Every filter the old list had is here;
// the D-pad only moves, and A changes the row it is on.
void UI::moveGtsFilterCursor(int dy) {
    int col, idx;
    gtsRowPos(gtsFilterCursor_, col, idx);
    const int n = col == 0 ? GTS_LEFT_N : GTS_RIGHT_N;
    idx = (idx + dy + n) % n;
    gtsFilterCursor_ = col == 0 ? GTS_LEFT_ROWS[idx] : GTS_RIGHT_ROWS[idx];
}

void UI::switchGtsFilterColumn(int dx) {
    int col, idx;
    gtsRowPos(gtsFilterCursor_, col, idx);
    const int next = std::clamp(col + dx, 0, 1);
    if (next == col) return;
    gtsFilterCursor_ = next == 0 ? GTS_LEFT_ROWS[std::min(idx, GTS_LEFT_N - 1)]
                                 : GTS_RIGHT_ROWS[std::min(idx, GTS_RIGHT_N - 1)];
}

std::vector<std::string> UI::gtsFilterChips() const {
    std::vector<std::string> c;
    auto strip = [](std::string l) {
        while (!l.empty() && (l.back() == ':' || l.back() == ' ')) l.pop_back();
        return l;
    };
    const Gts::Filter& f = gtsFilter_;
    if (f.species != 0 && !f.speciesName.empty()) c.push_back(f.speciesName);
    if (!f.family.empty()) c.push_back(gameInfo(FAMILIES[familyIndex(f.family)].rep).gameTag);
    if (f.shiny == 1) c.push_back(strip(i18n::get(StrKey::FilterShiny)));
    if (f.shiny == 0) c.push_back(strip(i18n::get(StrKey::FilterShiny)) + " \xc2\xb7 " + i18n::get(StrKey::GtsFilterNo));
    if (f.egg == 1)   c.push_back(strip(i18n::get(StrKey::FilterEgg)));
    if (f.egg == 0)   c.push_back(strip(i18n::get(StrKey::FilterEgg)) + " \xc2\xb7 " + i18n::get(StrKey::GtsFilterNo));
    if (f.alpha == 1 && Gts::alphaApplies(f)) c.push_back(i18n::get(StrKey::LegendAlpha));
    if (f.alpha == 0 && Gts::alphaApplies(f))
        c.push_back(i18n::get(StrKey::LegendAlpha) + " \xc2\xb7 " + i18n::get(StrKey::GtsFilterNo));
    if (f.ball >= 0) c.push_back(BallName::get(static_cast<uint8_t>(f.ball)));
    if (f.minIvs >= 0) c.push_back(i18n::fmt(StrKey::GtsChipIvs, std::to_string(f.minIvs)));
    if (f.legality == Gts::Filter::Legality::LegalOnly) c.push_back(i18n::get(StrKey::GtsFilterLegalOnly));
    if (f.legality == Gts::Filter::Legality::All)       c.push_back(i18n::get(StrKey::GtsLegalityAll));
    if (f.byPopularity) c.push_back(i18n::get(StrKey::GtsSortPopular));
    return c;
}

void UI::drawGtsFilterPopup() {
    // handleStickRepeat() moves this cursor from ui_input.cpp and cannot see
    // the enum, so the two counts are pinned together here.
    static_assert(ROW_COUNT == GTS_FILTER_ROWS,
                  "GTS_FILTER_ROWS must match the filter's row enum");

    drawRect(0, 0, SCREEN_W, SCREEN_H, SDL_Color{T().bg.r, T().bg.g, T().bg.b, 190});

    constexpr int GF_W = 960, GF_HEAD = 66, GF_ROW_H = 52, GF_ROW_STEP = 64;
    constexpr int GF_COL_W = 452, GF_LABEL_W = 140, GF_BAR_H = 76, GF_FOOT = 50;
    const int bodyH = std::max(GTS_LEFT_N, GTS_RIGHT_N) * GF_ROW_STEP + 8;
    const int H = GF_HEAD + 20 + bodyH + GF_BAR_H + GF_FOOT;
    const int x = (SCREEN_W - GF_W) / 2, y = (SCREEN_H - H) / 2;
    fillRounded(x, y, GF_W, H, 22, T().panelBg);
    strokeRounded(x, y, GF_W, H, 22, 1, T().panelBorder);

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
        const std::string title = i18n::get(StrKey::GtsFilterTitle);
        drawText(title, x + 60, y + 33 - TTF_FontHeight(fT) / 2, T().text, fT);
        const int sx = x + 60 + textWidth(title, fT) + 14;
        drawText(fitText(i18n::get(StrKey::GtsTitle), fS, x + GF_W - 26 - sx), sx,
                 y + 36 - TTF_FontHeight(fS) / 2, T().textDim, fS);
    }
    drawRect(x, y + GF_HEAD, GF_W, 1, T().panelBorder);

    TTF_Font* fLabel = uiFont(16, true);
    TTF_Font* fVal   = uiFont(15);
    TTF_Font* fSeg   = uiFont(13, true);
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
    auto label = [&](const char* key, int rx, int cy) {
        std::string l = i18n::get(key);
        while (!l.empty() && (l.back() == ':' || l.back() == ' ')) l.pop_back();
        drawText(fitText(l, fLabel, GF_LABEL_W - 8), rx + 18, cy - TTF_FontHeight(fLabel) / 2, T().text, fLabel);
    };
    const std::string any = i18n::get(StrKey::GenderAny);
    const std::string yes = i18n::get(StrKey::FilterYes), no = i18n::get(StrKey::GtsFilterNo);

    auto drawRow = [&](int row, int rx, int ry) {
        const int cy = ry + GF_ROW_H / 2;
        const bool cur = row == gtsFilterCursor_;
        if (cur) {
            strokeRounded(rx - 6, ry - 6, GF_COL_W + 12, GF_ROW_H + 12, 18, 3, T().accent);
            fillRounded(rx, ry, GF_COL_W, GF_ROW_H, 12, T().slotFull);
        }
        const int cx = rx + GF_LABEL_W + 10, right = rx + GF_COL_W - 16, cw = right - cx;
        switch (row) {
            case ROW_SPECIES:
                label(StrKey::FilterSpecies, rx, cy);
                if (gtsFilter_.species > 0) {
                    fillRounded(cx, cy - 16, 32, 32, 8, T().slotFull);
                    drawSpriteFit(spriteFor(gtsFilter_.species, 0, false, false), cx + 16, cy, 28);
                    TTF_Font* fN = uiFont(16, true);
                    TTF_Font* fD = uiFont(12);
                    const std::string dex = "#" + std::to_string(gtsFilter_.species);
                    const std::string n = fitText(gtsFilter_.speciesName, fN, cw - 44 - textWidth(dex, fD) - 26);
                    drawText(n, cx + 42, cy - TTF_FontHeight(fN) / 2, T().text, fN);
                    drawText(dex, cx + 42 + textWidth(n, fN) + 10, cy - TTF_FontHeight(fD) / 2 + 1, T().textDim, fD);
                } else {
                    drawText(i18n::get(StrKey::SfAnySpecies), cx, cy - TTF_FontHeight(fVal) / 2, T().textMuted, fVal);
                }
                fillArrow(static_cast<float>(right - 4), cy, 6, ArrowDir::Right, T().textDim);
                break;
            case ROW_FAMILY: {
                label(StrKey::GtsFilterGame, rx, cy);
                const int idx = familyIndex(gtsFilter_.family);
                const bool all = idx == 0;
                drawText(fitText(all ? any : std::string(bankGroupNameOf(FAMILIES[idx].rep)), fVal, cw - 60),
                         cx, cy - TTF_FontHeight(fVal) / 2, all ? T().textMuted : T().text, fVal);
                // Which of the choices, since A steps through them one by one.
                TTF_Font* fC = uiFont(12);
                const std::string pos = std::to_string(idx + 1) + " / " + std::to_string(FAMILY_COUNT);
                drawText(pos, right - textWidth(pos, fC), cy - TTF_FontHeight(fC) / 2, T().textMuted, fC);
                break;
            }
            case ROW_SHINY:
                label(StrKey::FilterShiny, rx, cy);
                segmented(cx, cy, cw, {any, yes, no}, triSegment(gtsFilter_.shiny));
                break;
            case ROW_EGG:
                label(StrKey::FilterEgg, rx, cy);
                segmented(cx, cy, cw, {any, yes, no}, triSegment(gtsFilter_.egg));
                break;
            case ROW_ALPHA: {
                label(StrKey::FilterAlpha, rx, cy);
                segmented(cx, cy, cw, {any, yes, no}, triSegment(gtsFilter_.alpha));
                // Another game has no alphas: shown, but it does nothing there.
                if (!Gts::alphaApplies(gtsFilter_))
                    fillRounded(rx, ry, GF_COL_W, GF_ROW_H, 12,
                                SDL_Color{T().panelBg.r, T().panelBg.g, T().panelBg.b, 170});
                break;
            }
            case ROW_BALL: {
                label(StrKey::GtsFilterBall, rx, cy);
                if (gtsFilter_.ball < 0) {
                    drawText(any, cx, cy - TTF_FontHeight(fVal) / 2, T().textMuted, fVal);
                } else {
                    int bx = cx;
                    if (SDL_Texture* tex = getBallSprite(static_cast<uint8_t>(gtsFilter_.ball))) {
                        SDL_Rect dst = {bx, cy - 12, 24, 24};
                        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
                        bx += 32;
                    }
                    drawText(fitText(BallName::get(static_cast<uint8_t>(gtsFilter_.ball)), fVal, right - 30 - bx),
                             bx, cy - TTF_FontHeight(fVal) / 2, T().text, fVal);
                }
                // L / R step back and forth; A steps forward.
                fillArrow(static_cast<float>(right - 18), cy, 5, ArrowDir::Left, T().textDim);
                fillArrow(static_cast<float>(right - 4), cy, 5, ArrowDir::Right, T().textDim);
                break;
            }
            case ROW_MIN_IVS: {
                label(StrKey::GtsFilterMinIVs, rx, cy);
                std::vector<std::string> l = {any};
                for (int i = 1; i < IV_STEP_COUNT; i++) l.push_back(std::to_string(IV_STEPS[i]));
                segmented(cx, cy, cw, l, ivStepIndex(gtsFilter_.minIvs));
                break;
            }
            case ROW_LEGAL:
                label(StrKey::GtsFilterLegality, rx, cy);
                segmented(cx, cy, cw, {i18n::get(StrKey::GtsLegalityChecked), i18n::get(StrKey::GtsFilterLegalOnly),
                                       i18n::get(StrKey::GtsLegalityAll)},
                          static_cast<int>(gtsFilter_.legality));
                break;
            case ROW_SORT:
                label(StrKey::GtsFilterSort, rx, cy);
                segmented(cx, cy, cw, {i18n::get(StrKey::GtsSortRecent), i18n::get(StrKey::GtsSortPopular)},
                          gtsFilter_.byPopularity ? 1 : 0);
                break;
        }
    };

    const int top = y + GF_HEAD + 20;
    const int lx = x + 20, rxCol = x + GF_W - 20 - GF_COL_W;
    for (int i = 0; i < GTS_LEFT_N; i++)  drawRow(GTS_LEFT_ROWS[i],  lx,    top + i * GF_ROW_STEP);
    for (int i = 0; i < GTS_RIGHT_N; i++) drawRow(GTS_RIGHT_ROWS[i], rxCol, top + i * GF_ROW_STEP);

    // Active filters, and Reset / Search.
    {
        const int by = y + H - GF_FOOT - GF_BAR_H;
        drawRect(x, by, GF_W, 1, T().panelBorder);
        const int cy = by + GF_BAR_H / 2;
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
        int bx = button(x + GF_W - 24, "Y", StrKey::HintSearch, true);
        bx = button(bx - 12, "X", StrKey::HintReset, false);

        const auto chips = gtsFilterChips();
        if (chips.empty()) {
            TTF_Font* f = uiFont(13);
            drawText(fitText(i18n::get(StrKey::SfNoFilters), f, bx - 16 - chipsX), chipsX,
                     cy - TTF_FontHeight(f) / 2, T().textMuted, f);
        } else {
            drawSearchChips(chips, chipsX, cy, bx - 16 - chipsX);
        }
    }

    // Footer: what A does on this row.
    {
        const int fy = y + H - GF_FOOT;
        drawRect(x, fy, GF_W, 1, T().panelBorder);
        const int cy = fy + GF_FOOT / 2;
        TTF_Font* f = uiFont(15, true);
        std::vector<std::pair<const char*, const char*>> hints = {
            {"A", gtsFilterCursor_ == ROW_SPECIES ? StrKey::HintChooseSpecies : StrKey::HintChange},
        };
        if (gtsFilterCursor_ == ROW_BALL) hints.push_back({"L R", StrKey::HintChange});
        hints.push_back({"B", StrKey::HintCancel});
        int hx = x + 26;
        for (const auto& h : hints) {
            hx += drawFooterKey(hx, cy, h.first, false) + 8;
            const std::string& l = i18n::get(h.second);
            drawText(l, hx, cy - TTF_FontHeight(f) / 2, T().text, f);
            hx += textWidth(l, f) + 22;
        }
    }
}

void UI::handleGtsFilterInput(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            updateStick(SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX),
                        SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY));
        }
        return;
    }
    if (event.type != SDL_CONTROLLERBUTTONDOWN) return;

    switch (event.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    moveGtsFilterCursor(-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  moveGtsFilterCursor(+1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  switchGtsFilterColumn(-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: switchGtsFilterColumn(+1); break;

        case SDL_CONTROLLER_BUTTON_B: // Switch A: change the row, or pick a species
            switch (gtsFilterCursor_) {
                case ROW_SPECIES:
                    // The same picker the box search uses; this flag is what
                    // tells it which filter a pick belongs to. Y there is "any
                    // species", which is how a species is cleared.
                    speciesPickerForGts_ = true;
                    availableSpecies_.clear();
                    buildAvailableSpeciesList();
                    showSpeciesLetterPicker_ = true;
                    speciesLetterCursor_ = 0;
                    speciesLetterScroll_ = 0;
                    break;
                case ROW_FAMILY: {
                    const int idx = (familyIndex(gtsFilter_.family) + 1) % FAMILY_COUNT;
                    gtsFilter_.family = FAMILIES[idx].key;
                    break;
                }
                case ROW_SHINY: gtsFilter_.shiny = nextTri(gtsFilter_.shiny); break;
                case ROW_EGG:   gtsFilter_.egg   = nextTri(gtsFilter_.egg);   break;
                case ROW_MIN_IVS:
                    gtsFilter_.minIvs = IV_STEPS[(ivStepIndex(gtsFilter_.minIvs) + 1) % IV_STEP_COUNT];
                    break;
                case ROW_LEGAL:
                    gtsFilter_.legality = static_cast<Gts::Filter::Legality>(
                        (static_cast<int>(gtsFilter_.legality) + 1) % 3);
                    break;
                case ROW_SORT: gtsFilter_.byPopularity = !gtsFilter_.byPopularity; break;
                case ROW_ALPHA:
                    if (Gts::alphaApplies(gtsFilter_)) gtsFilter_.alpha = nextTri(gtsFilter_.alpha);
                    break;
                case ROW_BALL: gtsFilter_.ball = stepBall(gtsFilter_.ball, +1); break;
            }
            break;

        // The ball list is long, so it also goes backwards.
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            if (gtsFilterCursor_ == ROW_BALL) gtsFilter_.ball = stepBall(gtsFilter_.ball, -1);
            break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            if (gtsFilterCursor_ == ROW_BALL) gtsFilter_.ball = stepBall(gtsFilter_.ball, +1);
            break;

        case SDL_CONTROLLER_BUTTON_Y: // Switch X = reset
            gtsFilter_ = Gts::Filter{};
            break;

        case SDL_CONTROLLER_BUTTON_X: // Switch Y = search
            showGtsFilter_ = false;
            if (gtsLoadPage(0)) {
                gtsCursor_ = 0;
                screen_ = AppScreen::GtsBrowse;
            } else {
                // The filter stays as it was so it can be widened and tried
                // again, rather than being thrown away on a miss.
                showGtsFilter_ = true;
            }
            break;

        case SDL_CONTROLLER_BUTTON_A: // Switch B = close
            showGtsFilter_ = false;
            break;
    }
}

// --- Deposit ------------------------------------------------------------------

void UI::gtsScanAllCards() {
    gtsCards_.clear();
    gtsCardRowInfo_.clear();

    // Every family folder, not just one game's: nothing has been selected at
    // this point, and a card is worth depositing whichever game it came from.
    // In the order the game selector and All banks use (ALL_GAMES), not the
    // filter's, so the groups read the same everywhere.
    constexpr GameType DEPOSIT_ORDER[] = {
        GameType::GP, GameType::Sw, GameType::BD, GameType::LA,
        GameType::S,  GameType::ZA, GameType::FR,
    };
    for (const GameType rep : DEPOSIT_ORDER) {
        for (const CardFile& cf : scanCards(basePath_, rep)) {
            GtsCard entry;
            entry.file = cf;
            entry.folderGame = rep;
            gtsCards_.push_back(std::move(entry));
        }
    }
}

// Decodes the highlighted card once the cursor has been still for a moment,
// the same way the importer's browser does: pulling a QR out of a 1280x720
// image takes long enough that doing it on every cursor move would stutter.
void UI::updateGtsCardPreview() {
    if (gtsCards_.empty() || gtsCardCursor_ < 0
        || gtsCardCursor_ >= static_cast<int>(gtsCards_.size())) {
        gtsCardPreview_ = CardPayload::Parsed{};
        gtsCardPreviewIdx_ = -1;
        return;
    }
    if (gtsCardPreviewIdx_ == gtsCardCursor_)
        return;
    if (SDL_GetTicks() - gtsCardPreviewSince_ < CARD_PREVIEW_DELAY_MS)
        return;

    const GtsCard& card = gtsCards_[gtsCardCursor_];
    gtsCardPreview_ = decodeCard(card.file.path, card.folderGame, true);
    gtsCardPreviewIdx_ = gtsCardCursor_;
    if (gtsCardRowInfo_.size() != gtsCards_.size()) gtsCardRowInfo_.assign(gtsCards_.size(), CardRowInfo{});
    fillCardRowInfo(gtsCardRowInfo_[gtsCardCursor_], gtsCardPreview_);
}

// UI 2.0: the card import's layout, with the cards of every game at once
// grouped by game the way All banks groups banks, ZL / ZR jumping between
// the groups. Rows are labelled from the filename; a row whose card has been
// read (the preview decodes the highlighted one) shows what it holds.
void UI::drawGtsDepositPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, SDL_Color{T().bg.r, T().bg.g, T().bg.b, 190});

    constexpr int GD_W = 1120, GD_H = 620, GD_HEAD = 68, GD_FOOT = 52, GD_LIST_W = 472;
    constexpr int GD_GROUP_H = 34, GD_ROW_H = 60, GD_ROW_STEP = 66;
    const int x = (SCREEN_W - GD_W) / 2, y = (SCREEN_H - GD_H) / 2;
    fillRounded(x, y, GD_W, GD_H, 22, T().panelBg);
    strokeRounded(x, y, GD_W, GD_H, 22, 1, T().panelBorder);

    const int total = static_cast<int>(gtsCards_.size());
    if (gtsCardRowInfo_.size() != gtsCards_.size()) gtsCardRowInfo_.assign(gtsCards_.size(), CardRowInfo{});

    // --- Header: the upload mark, the title, how many, ZL ZR Jump game. ---
    {
        const int cy = y + GD_HEAD / 2;
        const int mx = x + 38;
        fillCircle(renderer_, mx, cy, 14, T().accent);
        const SDL_Color ink = T().keyCapText;
        thickLine(renderer_, mx, cy + 4, mx, cy - 7, 3, ink);
        thickLine(renderer_, mx, cy - 8, mx - 5, cy - 3, 3, ink);
        thickLine(renderer_, mx, cy - 8, mx + 5, cy - 3, 3, ink);
        drawRect(mx - 7, cy + 7, 14, 2, ink);

        TTF_Font* fT = uiFont(24, true);
        const std::string title = i18n::get(StrKey::GtsPickCard);
        drawText(title, x + 62, cy - TTF_FontHeight(fT) / 2, T().text, fT);
        int cx = x + 62 + textWidth(title, fT) + 14;
        if (total > 0) {
            TTF_Font* fC = uiFont(13, true);
            const std::string count = total == 1 ? i18n::get(StrKey::CiCountOne)
                                                 : i18n::fmt(StrKey::CiCount, std::to_string(total));
            const SDL_Color fc = T().statusOk;
            const int fw = textWidth(count, fC) + 20;
            fillRounded(cx, cy - 13, fw, 26, 8, SDL_Color{fc.r, fc.g, fc.b, 36});
            drawTextCentered(count, cx + fw / 2, cy, fc, fC);
        }
        // ZL ZR  Jump game, as on All banks.
        TTF_Font* fK = uiFont(12, true);
        TTF_Font* fL = uiFont(13);
        const std::string jl = i18n::get(StrKey::HintJumpGame);
        constexpr int KEY_W = 28;
        int jx = x + GD_W - 26 - (KEY_W + 4 + KEY_W + 8 + textWidth(jl, fL));
        for (const char* k : {"ZL", "ZR"}) {
            fillRounded(jx, cy - 10, KEY_W, 20, 5, T().keyCap);
            drawTextCentered(k, jx + KEY_W / 2, cy, T().keyCapText, fK);
            jx += KEY_W + 4;
        }
        drawText(jl, jx + 4, cy - TTF_FontHeight(fL) / 2, T().textDim, fL);
    }
    drawRect(x, y + GD_HEAD, GD_W, 1, T().panelBorder);

    const int footY = y + GD_H - GD_FOOT;
    drawRect(x, footY, GD_W, 1, T().panelBorder);
    {
        const int cy = footY + GD_FOOT / 2;
        TTF_Font* fH = uiFont(15, true);
        const std::pair<const char*, const char*> hints[] = {
            {"A", StrKey::HintDeposit}, {"B", StrKey::HintCancel2},
        };
        int hx = x + 25;
        for (const auto& h : hints) {
            hx += drawFooterKey(hx, cy, h.first, false) + 8;
            const std::string& l = i18n::get(h.second);
            drawText(l, hx, cy - TTF_FontHeight(fH) / 2, T().text, fH);
            hx += textWidth(l, fH) + 22;
        }
        if (total > 0) {
            TTF_Font* fc = uiFont(14, true);
            const std::string pos = std::to_string(gtsCardCursor_ + 1) + " / " + std::to_string(total);
            drawText(pos, x + GD_W - 25 - textWidth(pos, fc), cy - TTF_FontHeight(fc) / 2, T().textDim, fc);
        }
    }
    if (total == 0) return;   // the hub says "no cards" before ever opening this
    gtsCardCursor_ = std::clamp(gtsCardCursor_, 0, total - 1);

    // --- List: a header per game, its cards under it. ---
    struct Row { bool header; int idx; };
    std::vector<Row> rows;
    for (int i = 0; i < total; i++) {
        if (i == 0 || gtsCards_[i].folderGame != gtsCards_[i - 1].folderGame)
            rows.push_back({true, i});
        rows.push_back({false, i});
    }
    std::vector<int> topOf(rows.size() + 1, 0);
    for (size_t i = 0; i < rows.size(); i++)
        topOf[i + 1] = topOf[i] + (rows[i].header ? GD_GROUP_H : GD_ROW_STEP);
    const int listTop = y + GD_HEAD + 14, listBottom = footY - 8;
    // Rows start 6px down so the cursor ring above the first one shows, and
    // need the same 6px below the last one: both are part of the content.
    const int viewH = listBottom - listTop, contentH = topOf.back() + 6;

    // Keep the cursor's row in view, and its header with it when it is first.
    int cur = 0;
    for (size_t i = 0; i < rows.size(); i++)
        if (!rows[i].header && rows[i].idx == gtsCardCursor_) { cur = static_cast<int>(i); break; }
    int want = topOf[cur];
    if (cur > 0 && rows[cur - 1].header) want = topOf[cur - 1];
    if (want < gtsCardScroll_) gtsCardScroll_ = want;
    if (topOf[cur] + GD_ROW_H + 12 > gtsCardScroll_ + viewH) gtsCardScroll_ = topOf[cur] + GD_ROW_H + 12 - viewH;
    gtsCardScroll_ = std::clamp(gtsCardScroll_, 0, std::max(0, contentH - viewH));

    const bool scrolls = contentH > viewH;
    const int lx = x + 26, lw = GD_LIST_W - 26 - (scrolls ? 22 : 6);
    const SDL_Rect clip = {x + 12, listTop - 6, GD_LIST_W - 12, listBottom - listTop + 6};
    SDL_RenderSetClipRect(renderer_, &clip);
    TTF_Font* fGroup = uiFont(14, true);
    TTF_Font* fCount = uiFont(12);
    for (size_t r = 0; r < rows.size(); r++) {
        const int ry = listTop + 6 + topOf[r] - gtsCardScroll_;
        if (ry + GD_ROW_STEP < listTop - 6) continue;
        if (ry > listBottom) break;
        const GtsCard& card = gtsCards_[rows[r].idx];
        if (rows[r].header) {
            int n = 0;
            for (const auto& c : gtsCards_) if (c.folderGame == card.folderGame) n++;
            const int cy = ry + GD_GROUP_H / 2 - 3;
            fillDisc(lx + 8, cy, 4, gameTint(card.folderGame));
            const std::string name = bankGroupNameOf(card.folderGame);
            drawText(name, lx + 22, cy - TTF_FontHeight(fGroup) / 2, T().text, fGroup);
            const std::string cnt = n == 1 ? i18n::get(StrKey::CiCountOne) : i18n::fmt(StrKey::CiCount, std::to_string(n));
            const int cw = textWidth(cnt, fCount);
            drawText(cnt, lx + lw - cw, cy - TTF_FontHeight(fCount) / 2, T().textMuted, fCount);
            const int dx = lx + 22 + textWidth(name, fGroup) + 12;
            if (lx + lw - cw - 12 > dx) drawRect(dx, cy, lx + lw - cw - 12 - dx, 1, T().divider);
            continue;
        }
        drawCardRow(card.file, gtsCardRowInfo_[rows[r].idx], lx, ry, lw, rows[r].idx == gtsCardCursor_);
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
    if (scrolls) {
        const int trackH = viewH;
        const int thumbH = std::max(30, trackH * viewH / contentH);
        const int thumbY = listTop + (trackH - thumbH) * gtsCardScroll_ / std::max(1, contentH - viewH);
        fillRounded(x + GD_LIST_W - 14, listTop, 5, trackH, 2, T().buttonBg);
        fillRounded(x + GD_LIST_W - 14, thumbY, 5, thumbH, 2, T().textMuted);
    }
    drawRect(x + GD_LIST_W, y + GD_HEAD, 1, footY - y - GD_HEAD, T().panelBorder);

    // --- What the highlighted card holds, read from its QR code. ---
    const int px = x + GD_LIST_W + 24;
    drawCardDetailPane(gtsCardPreview_, gtsCardPreviewIdx_ != gtsCardCursor_,
                       px, y + GD_HEAD + 20, x + GD_W - 24 - px, footY - 22 - 52,
                       StrKey::GtsDepositNamed, StrKey::HintDeposit);
}

// ZL / ZR: to the first card of the next game, or of this one and then the
// one before (wrapping), as All banks does.
void UI::jumpGtsCardGroup(int dir) {
    const int n = static_cast<int>(gtsCards_.size());
    if (n == 0) return;
    int i = std::clamp(gtsCardCursor_, 0, n - 1);
    const GameType g = gtsCards_[i].folderGame;
    if (dir > 0) {
        while (i < n && gtsCards_[i].folderGame == g) i++;
        if (i >= n) i = 0;
    } else {
        while (i > 0 && gtsCards_[i - 1].folderGame == g) i--;
        int j = i > 0 ? i - 1 : n - 1;
        const GameType prev = gtsCards_[j].folderGame;
        while (j > 0 && gtsCards_[j - 1].folderGame == prev) j--;
        i = j;
    }
    gtsCardCursor_ = i;
    gtsCardPreviewSince_ = SDL_GetTicks();
}

void UI::handleGtsDepositInput(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            updateStick(SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX),
                        SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY));
        }
        // ZL / ZR jump between games. Edge-triggered, like All banks.
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
            const bool left = event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT;
            const bool pressed = event.caxis.value > TRIGGER_DEADZONE;
            bool& was = left ? zlPressed_ : zrPressed_;
            if (pressed && !was) {
                jumpGtsCardGroup(left ? -1 : +1);
                markDirty();
            }
            was = pressed;
        }
        return;
    }
    if (event.type != SDL_CONTROLLERBUTTONDOWN) return;

    const int total = static_cast<int>(gtsCards_.size());
    if (total == 0) {
        showGtsDeposit_ = false;
        return;
    }

    auto move = [&](int dir) {
        gtsCardCursor_ = (gtsCardCursor_ + dir + total) % total;
        gtsCardPreviewSince_ = SDL_GetTicks();
    };

    switch (event.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:   move(-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: move(+1); break;
        case SDL_CONTROLLER_BUTTON_B: // Switch A = upload this one
            gtsUploadSelectedCard();
            break;
        case SDL_CONTROLLER_BUTTON_A: // Switch B = close
            showGtsDeposit_ = false;
            break;
    }
}

void UI::gtsUploadSelectedCard() {
    if (gtsCardCursor_ < 0 || gtsCardCursor_ >= static_cast<int>(gtsCards_.size()))
        return;

    const GtsCard& card = gtsCards_[gtsCardCursor_];

    // The browser's preview only ever scans the QR region, which is enough to
    // label a row and not enough to commit to an upload. This reads the whole
    // image, the same way importing one does.
    CardPayload::Parsed parsed = decodeCard(card.file.path, card.folderGame, false);
    if (gtsCardCursor_ < static_cast<int>(gtsCardRowInfo_.size()))
        fillCardRowInfo(gtsCardRowInfo_[gtsCardCursor_], parsed);
    if (parsed.result != CardPayload::Result::Ok) {
        showMessageAndWait(i18n::get(StrKey::GtsFailed), i18n::get(StrKey::GtsCardUnreadable));
        return;
    }

    ConfirmStyle st;
    st.confirmKey = StrKey::HintDeposit;
    if (!showConfirmDialog(i18n::get(StrKey::GtsUploadConfirm),
                           i18n::fmt(StrKey::GtsUploadConfirmBody, parsed.pkm.displayName()), st))
        return;

    // Rebuilt from the decoded Pokemon rather than carved out of the PNG: build()
    // is what wrote the payload in the first place, so this reproduces the same
    // bytes, and it means the upload path never has to trust the file's framing.
    std::vector<uint8_t> payload = CardPayload::build(parsed.pkm, parsed.game);
    if (payload.empty()) {
        showMessageAndWait(i18n::get(StrKey::GtsFailed), i18n::get(StrKey::GtsCardUnreadable));
        return;
    }

    showWorking(i18n::get(StrKey::GtsUploading));

    std::string id;
    bool duplicate = false;
    if (!Gts::deposit(payload, "", id, duplicate)) {
        showMessageAndWait(i18n::get(StrKey::GtsFailed), Gts::lastError());
        return;
    }

    if (duplicate) {
        // Not an error: the board deduplicates on the payload, so the same
        // Pokemon exported twice is one listing, and saying so is friendlier
        // than a second copy appearing that nobody asked for.
        showMessageAndWait(i18n::get(StrKey::GtsDuplicate), i18n::get(StrKey::GtsDuplicateBody), DialogKind::Info);
    } else {
        showMessageAndWait(i18n::get(StrKey::GtsDeposited), i18n::get(StrKey::GtsDepositedBody), DialogKind::Success);
    }
    showGtsDeposit_ = false;
}
