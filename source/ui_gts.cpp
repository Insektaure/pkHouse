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

// Rows in the search filter, in the order they are drawn.
enum FilterRow {
    ROW_SPECIES = 0, ROW_FAMILY, ROW_SHINY, ROW_EGG, ROW_MIN_IVS,
    ROW_LEGAL, ROW_SORT, ROW_RESET, ROW_SEARCH, ROW_COUNT
};

// The IV totals worth offering. 186 is six perfect, 150 is roughly five.
constexpr int IV_STEPS[] = {-1, 90, 120, 150, 180, 186};
constexpr int IV_STEP_COUNT = static_cast<int>(sizeof(IV_STEPS) / sizeof(IV_STEPS[0]));

int ivStepIndex(int value) {
    for (int i = 0; i < IV_STEP_COUNT; i++)
        if (IV_STEPS[i] == value) return i;
    return 0;
}

// Cycles a tri-state filter: any -> yes -> no -> any.
int cycleTri(int value, int dir) {
    int v = value + 1;              // -1,0,1 -> 0,1,2
    v = (v + dir + 3) % 3;
    return v - 1;
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

// A pill: a half-disc at each end and a rect between them.
void fillPill(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c) {
    const int radius = h / 2;
    fillCircle(r, x + radius, y + radius, radius, c);
    fillCircle(r, x + w - radius - 1, y + radius, radius, c);

    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect middle = {x + radius, y, w - radius * 2, h};
    SDL_RenderFillRect(r, &middle);
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

// --- The row above the game icons ---------------------------------------------
// Drawn rather than shipped as a PNG, so the band is navy and amber in the
// default and follows the palette everywhere else.
void UI::drawGtsRow(int y, int h) {
    constexpr int ROW_W = 1160;
    const int x = (SCREEN_W - ROW_W) / 2;
    const bool focused = gameSelOnGts_;

    drawRect(x, y, ROW_W, h, focused ? T().menuHighlight : T().panelBg);

    // --- the globe, behind everything ---------------------------------------
    //
    // Bigger than the band on purpose: what shows is a slice through the middle
    // of a sphere, which is why it reads as a globe rather than as a circle.
    // Clipped so the parts above and below simply are not drawn.
    {
        SDL_Rect clip = {x, y, ROW_W, h};
        SDL_RenderSetClipRect(renderer_, &clip);

        const int gcx = x + 853;
        const int gcy = y + h / 2;
        constexpr int R = 136;

        const SDL_Color wire = withAlpha(T().text, 38);
        const SDL_Color rim  = withAlpha(T().text, 60);

        ellipseOutline(renderer_, gcx, gcy, R, R, rim);
        // Meridians: the same circle squeezed, so they crowd towards the edges.
        for (int i = 1; i <= 3; i++)
            ellipseOutline(renderer_, gcx, gcy, R * i / 4, R, wire);
        // Parallels.
        for (int dy = -R + 24; dy < R; dy += 34)
            chord(renderer_, gcx, gcy, R, dy, wire);

        // --- the link, over the globe ---------------------------------------
        //
        // Two points and a path between them: the whole idea of the screen in
        // one motif. Dotted rather than solid so it reads as a route.
        const SDL_Color trail = withAlpha(T().goldLabel, 200);
        const int ax = x + 722, ay = y + 35;   // near point
        const int bx = x + 968, by = y + 12;   // far point
        const int cxp = x + 845, cyp = y - 18; // control, above the band

        for (int step = 0; step <= 40; step++) {
            if (step % 2 != 0) continue;       // every other sample: a dotted line
            const double s = step / 40.0, inv = 1.0 - s;
            const int px = static_cast<int>(inv * inv * ax + 2 * inv * s * cxp + s * s * bx);
            const int py = static_cast<int>(inv * inv * ay + 2 * inv * s * cyp + s * s * by);
            fillCircle(renderer_, px, py, 1, trail);
        }
        fillCircle(renderer_, ax, ay, 4, T().goldLabel);
        fillCircle(renderer_, bx, by, 3, T().goldLabel);

        // A scatter of stars, at fixed offsets so the band never shimmers.
        static constexpr int STARS[][2] = {
            {612, 12}, {664, 34}, {700, 8}, {1006, 16}, {1032, 38},
            {760, 6}, {930, 42}, {1046, 6},
        };
        const SDL_Color star = withAlpha(T().text, 90);
        for (const auto& s : STARS)
            drawRect(x + s[0], y + s[1], 2, 2, star);

        SDL_RenderSetClipRect(renderer_, nullptr);
    }

    // --- the mark ------------------------------------------------------------
    {
        const int cx = x + 24, cy = y + 24;
        fillCircle(renderer_, cx, cy, 20, T().goldLabel);

        // A globe cut out of the disc, in the band's own colour.
        const SDL_Color cut = focused ? T().menuHighlight : T().panelBg;
        ellipseOutline(renderer_, cx, cy, 12, 12, cut);
        ellipseOutline(renderer_, cx, cy, 5, 12, cut);
        chord(renderer_, cx, cy, 12, 0, cut);
        chord(renderer_, cx, cy, 12, -7, cut);
        chord(renderer_, cx, cy, 12, 7, cut);
    }

    // --- what it is ----------------------------------------------------------
    drawText(i18n::get(StrKey::GtsTitle), x + 57, y + 6, T().text, font_);
    drawText(i18n::get(StrKey::GtsSubtitle), x + 57, y + 27, T().textDim, fontSmall_);

    // --- the way in ----------------------------------------------------------
    {
        constexpr int PILL_W = 81, PILL_H = 32;
        const int px = x + 1071, py = y + 8;
        fillPill(renderer_, px, py, PILL_W, PILL_H, T().goldLabel);
        drawTextCentered(i18n::get(StrKey::GtsOpen), px + 32, py + PILL_H / 2,
                         T().textOnBadge, fontSmall_);
        drawTextCentered(">", px + 63, py + PILL_H / 2, T().textOnBadge, fontSmall_);
    }

    if (focused)
        drawRectOutline(x, y, ROW_W, h, T().cursor, 3);
}

// --- Hub ----------------------------------------------------------------------

void UI::drawGtsHubFrame() {
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);

    drawTextCentered(i18n::get(StrKey::GtsTitle), SCREEN_W / 2, 40, T().goldLabel, fontLarge_);

    constexpr int MARGIN = 60;
    constexpr int GAP    = 24;
    constexpr int TOP    = 120;
    constexpr int BOT    = 640;
    const int totalH = BOT - TOP;

    // Left: one big button, because browsing is what almost everyone wants and
    // it should not need to be looked for.
    const int leftW = 620;
    const int leftX = MARGIN;
    {
        const bool focused = (gtsHubCursor_ == 0);
        drawRect(leftX, TOP, leftW, totalH, focused ? T().menuHighlight : T().panelBg);
        drawRect(leftX, TOP, 6, totalH, T().goldLabel);
        if (focused)
            drawRectOutline(leftX, TOP, leftW, totalH, T().cursor, 3);

        drawTextCentered(i18n::get(StrKey::GtsBrowseBtn),
                         leftX + leftW / 2, TOP + totalH / 2 - 14, T().text, fontLarge_);
        drawTextCentered(i18n::get(StrKey::GtsBrowseHint),
                         leftX + leftW / 2, TOP + totalH / 2 + 22, T().textDim, fontSmall_);
    }

    // Right: two stacked buttons.
    const int rightX = leftX + leftW + GAP;
    const int rightW = SCREEN_W - rightX - MARGIN;
    const int halfH  = (totalH - GAP) / 2;

    struct HubButton { int cursor; const char* label; const char* hint; };
    const HubButton buttons[] = {
        {1, StrKey::GtsSearchBtn,  StrKey::GtsSearchHint},
        {2, StrKey::GtsDepositBtn, StrKey::GtsDepositHint},
    };

    for (int i = 0; i < 2; i++) {
        const int by = TOP + i * (halfH + GAP);
        const bool focused = (gtsHubCursor_ == buttons[i].cursor);

        drawRect(rightX, by, rightW, halfH, focused ? T().menuHighlight : T().panelBg);
        drawRect(rightX, by, 6, halfH, T().goldLabel);
        if (focused)
            drawRectOutline(rightX, by, rightW, halfH, T().cursor, 3);

        drawTextCentered(i18n::get(buttons[i].label),
                         rightX + rightW / 2, by + halfH / 2 - 12, T().text, font_);
        drawTextCentered(i18n::get(buttons[i].hint),
                         rightX + rightW / 2, by + halfH / 2 + 16, T().textDim, fontSmall_);
    }

    drawStatusBar(i18n::get(StrKey::StatusGtsHub));

    if (showGtsFilter_)  drawGtsFilterPopup();
    if (showGtsDeposit_) drawGtsDepositPopup();
    if (showSpeciesLetterPicker_) drawSpeciesLetterPicker();
    if (showSpeciesListPicker_)   drawSpeciesListPicker();
}

void UI::handleGtsHubInput(bool& running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) { running = false; return; }
        if (event.type == SDL_CONTROLLERBUTTONDOWN) markDirty();

        // Popups get the event instead, innermost first.
        if (showSpeciesListPicker_)   { handleSpeciesListPickerInput(event);   continue; }
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
                    // "everything legal" until Search changes it.
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

            case SDL_CONTROLLER_BUTTON_X: // Switch Y = theme
                showThemeSelector_ = true;
                themeSelCursor_ = themeIndex_;
                themeSelOriginal_ = themeIndex_;
                break;

            case SDL_CONTROLLER_BUTTON_START:
                running = false;
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
                                                                : StrKey::GtsNoResults));
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

void UI::drawGtsSlot(int x, int y, const Gts::Entry& e, bool isCursor) {
    drawRect(x, y, GTS_CELL_W, GTS_CELL_H, e.egg ? T().slotEgg : T().slotFull);

    // Anything the board has not judged yet is framed rather than hidden. The
    // default filter never shows these, so a frame here means the user asked
    // to see them and should be able to tell which is which at a glance.
    if (e.legality != "legal")
        drawRectOutline(x + 1, y + 1, GTS_CELL_W - 2, GTS_CELL_H - 2, T().searchMatch, 2);

    if (isCursor)
        drawRectOutline(x, y, GTS_CELL_W, GTS_CELL_H, T().cursor, 3);

    SDL_Texture* sprite = nullptr;
    if (e.egg) {
        sprite = getSprite(0);
    } else if (e.shiny) {
        sprite = getShinySprite(e.species, e.form);
        if (!sprite) sprite = getSprite(e.species, e.form);
    } else {
        sprite = getSprite(e.species, e.form);
    }

    if (sprite) {
        int texW = 0, texH = 0;
        SDL_QueryTexture(sprite, nullptr, nullptr, &texW, &texH);
        int dstW = GTS_SPRITE, dstH = GTS_SPRITE;
        if (texW > 0 && texH > 0) {
            float scale = std::min(static_cast<float>(GTS_SPRITE) / texW,
                                   static_cast<float>(GTS_SPRITE) / texH);
            dstW = static_cast<int>(texW * scale);
            dstH = static_cast<int>(texH * scale);
        }
        SDL_Rect dst = { x + (GTS_CELL_W - dstW) / 2, y + 2 + (GTS_SPRITE - dstH) / 2, dstW, dstH };
        SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
    }

    if (e.shiny && iconShiny_) {
        SDL_Rect dst = { x + GTS_CELL_W - 20, y + 3, 16, 16 };
        SDL_RenderCopy(renderer_, iconShiny_, nullptr, &dst);
    }

    // Name, then level and gender on one line beneath it.
    std::string label = gtsEntryLabel(e);
    if (label.size() > 11) label = label.substr(0, 10) + ".";
    drawTextCentered(label, x + GTS_CELL_W / 2, y + GTS_SPRITE + 10, T().text, fontSmall_);

    if (!e.egg) {
        std::string line;
        if (e.level > 0) line = "Lv" + std::to_string(e.level);
        SDL_Color lineColor = T().textDim;
        if (e.gender == 0)      { line += line.empty() ? "" : " "; line += "M"; lineColor = T().genderMale; }
        else if (e.gender == 1) { line += line.empty() ? "" : " "; line += "F"; lineColor = T().genderFemale; }
        if (!line.empty())
            drawTextCentered(line, x + GTS_CELL_W / 2, y + GTS_SPRITE + 26, lineColor, fontSmall_);
    }
}

void UI::drawGtsBrowseFrame() {
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);

    const int gridW = GTS_COLS * GTS_CELL_W + (GTS_COLS - 1) * GTS_CELL_PAD;
    const int gridX = (SCREEN_W - gridW) / 2;
    constexpr int GRID_TOP = GTS_GRID_TOP;

    // Header: what is being shown, and where in the board we are.
    {
        std::string title = i18n::get(StrKey::GtsTitle);
        if (gtsFilter_.species != 0 && !gtsFilter_.speciesName.empty())
            title += "  -  " + gtsFilter_.speciesName;
        else if (!gtsFilter_.family.empty())
            title += "  -  " + std::string(bankGroupNameOf(FAMILIES[familyIndex(gtsFilter_.family)].rep));
        drawText(title, gridX, 18, T().goldLabel, font_);

        const int page = gtsOffset_ / Gts::PAGE_SIZE + 1;
        std::string right = i18n::fmt(StrKey::GtsPage, std::to_string(page));
        const auto& te = getTextEntry(right, fontSmall_, T().textDim);
        drawText(right, gridX + gridW - te.w, 24, T().textDim, fontSmall_);
    }

    const int count = static_cast<int>(gtsPage_.entries.size());
    for (int i = 0; i < GTS_PER_PAGE; i++) {
        const int col = i % GTS_COLS;
        const int row = i / GTS_COLS;
        const int x = gridX + col * (GTS_CELL_W + GTS_CELL_PAD);
        const int y = GRID_TOP + row * (GTS_CELL_H + GTS_CELL_PAD);

        if (i >= count) {
            // The tail of a short last page. Drawn as empty slots rather than
            // left blank so the grid keeps its shape.
            drawRect(x, y, GTS_CELL_W, GTS_CELL_H, T().slotEmpty);
            if (i == gtsCursor_)
                drawRectOutline(x, y, GTS_CELL_W, GTS_CELL_H, T().cursor, 3);
            continue;
        }
        drawGtsSlot(x, y, gtsPage_.entries[i], i == gtsCursor_);
    }

    // One line about whatever the cursor is on, under the grid.
    if (gtsCursor_ < count) {
        const Gts::Entry& e = gtsPage_.entries[gtsCursor_];
        const int infoY = GRID_TOP + GTS_ROWS * (GTS_CELL_H + GTS_CELL_PAD) + 6;

        // The verdict, first and in colour. A frame around a 118px cell cannot
        // carry this, and with "legal only" turned off the grid is a mix.
        const char* verdictKey = StrKey::GtsVerdictLegal;
        SDL_Color verdictColor = T().text;
        if (e.legality == "pending") {
            verdictKey = StrKey::GtsVerdictPending;
            verdictColor = T().goldLabel;
        } else if (e.legality == "illegal") {
            verdictKey = StrKey::GtsVerdictIllegal;
            verdictColor = T().red;
        } else if (e.legality != "legal") {
            verdictKey = StrKey::GtsVerdictUnknown;
            verdictColor = T().textDim;
        }

        const std::string& verdict = i18n::get(verdictKey);
        drawText(verdict, gridX, infoY, verdictColor, fontSmall_);
        const int factsX = gridX + getTextEntry(verdict, fontSmall_, verdictColor).w + 18;

        std::string facts;
        auto add = [&facts](const std::string& part) {
            if (part.empty()) return;
            if (!facts.empty()) facts += "  -  ";
            facts += part;
        };

        // Which game it can ever be imported into: the hard gate, since a card
        // only goes back into the family it came from.
        if (e.gameKnown) add(bankGroupNameOf(e.game));

        if (!e.egg) {
            add(NatureName::get(e.nature));
            if (e.ability != 0) add(AbilityName::get(e.ability));
        }
        if (e.ivTotal > 0) add(i18n::get(StrKey::IVs) + " " + std::to_string(e.ivTotal) + "/186");
        if (e.evTotal > 0) add(i18n::get(StrKey::EVs) + " " + std::to_string(e.evTotal) + "/510");
        if (e.heldItem != 0) add("@ " + ItemName::get(e.heldItem));

        drawText(facts, factsX, infoY, T().textDim, fontSmall_);

        // The depositor's own words when there are any, and how many people
        // have kept it when there are not. Neither is decisive, and they never
        // both need to be on screen at once.
        const bool hasNote = !e.note.empty();
        const std::string right = hasNote
            ? e.note
            : i18n::fmt(StrKey::GtsDownloads, std::to_string(e.downloads));
        const SDL_Color rightColor = hasNote ? T().text : T().textDim;
        const auto& te = getTextEntry(right, fontSmall_, rightColor);
        drawText(right, gridX + gridW - te.w, infoY, rightColor, fontSmall_);
    }

    drawStatusBar(i18n::get(gtsDetail_ ? StrKey::StatusGtsDetail : StrKey::StatusGtsBrowse));

    if (gtsDetail_)
        drawDetailPopup(gtsDetailPkm_, StrKey::StatusGtsDetail,
                        gameInfo(gtsDetailGame_).gameTag);
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
            }
            continue;
        }

        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  moveGtsCursor(-1, 0); break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: moveGtsCursor(+1, 0); break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:    moveGtsCursor(0, -1); break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  moveGtsCursor(0, +1); break;

            case SDL_CONTROLLER_BUTTON_B: // Switch A = open
                gtsOpenDetail();
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
                running = false;
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
    gtsDetail_ = true;
    markDirty();
}

void UI::gtsSaveCurrentAsCard() {
    if (!gtsDetail_) return;

    // The card renderer writes into cards/<family>/, and which family that is
    // comes from the Pokemon itself rather than from whatever game happens to
    // be selected - nothing is selected here.
    showWorking(i18n::get(StrKey::SavingCard));

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
                       std::string(bankFolderNameOf(gtsDetailGame_)) + "/" + filename);
}

// --- Search filter ------------------------------------------------------------

void UI::drawGtsFilterPopup() {
    // handleStickRepeat() scrolls this list from ui_input.cpp and cannot see
    // the enum, so the two counts are pinned together here.
    static_assert(ROW_COUNT == GTS_FILTER_ROWS,
                  "GTS_FILTER_ROWS must match the filter's row enum");

    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    constexpr int POP_W = 620;
    constexpr int ROW_H = 40;
    const int POP_H = 56 + ROW_COUNT * ROW_H + 30;
    const int popX = (SCREEN_W - POP_W) / 2;
    const int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);
    drawTextCentered(i18n::get(StrKey::GtsFilterTitle), popX + POP_W / 2, popY + 24,
                     T().goldLabel, font_);

    const int startY = popY + 56;
    const int labelX = popX + 30;
    const int valueX = popX + 280;

    auto triText = [&](int v) -> const std::string& {
        if (v < 0) return i18n::get(StrKey::FilterAny);
        return i18n::get(v == 1 ? StrKey::FilterYes : StrKey::GtsFilterNo);
    };

    for (int i = 0; i < ROW_COUNT; i++) {
        const int rowY = startY + i * ROW_H;
        const int textY = rowY + (ROW_H - 4) / 2 - 9;

        if (i == gtsFilterCursor_) {
            drawRect(popX + 20, rowY, POP_W - 40, ROW_H - 4, T().menuHighlight);
            drawRectOutline(popX + 20, rowY, POP_W - 40, ROW_H - 4, T().cursor, 2);
        }

        switch (i) {
            case ROW_SPECIES:
                drawText(i18n::get(StrKey::FilterSpecies), labelX, textY, T().text, font_);
                if (gtsFilter_.species > 0) {
                    SDL_Texture* spr = getSprite(gtsFilter_.species);
                    if (spr) {
                        SDL_Rect dst = { valueX, rowY + 2, ROW_H - 8, ROW_H - 8 };
                        SDL_RenderCopy(renderer_, spr, nullptr, &dst);
                        drawText(gtsFilter_.speciesName, valueX + ROW_H, textY, T().text, font_);
                    } else {
                        drawText(gtsFilter_.speciesName, valueX, textY, T().text, font_);
                    }
                } else {
                    drawText(i18n::get(StrKey::FilterAny), valueX, textY, T().textDim, font_);
                }
                break;

            case ROW_FAMILY: {
                drawText(i18n::get(StrKey::GtsFilterGame), labelX, textY, T().text, font_);
                const int idx = familyIndex(gtsFilter_.family);
                if (idx == 0)
                    drawText(i18n::get(StrKey::FilterAny), valueX, textY, T().textDim, font_);
                else
                    drawText(bankGroupNameOf(FAMILIES[idx].rep), valueX, textY, T().text, fontSmall_);
                break;
            }

            case ROW_SHINY:
                drawText(i18n::get(StrKey::FilterShiny), labelX, textY, T().text, font_);
                drawText(triText(gtsFilter_.shiny), valueX, textY,
                         gtsFilter_.shiny < 0 ? T().textDim : T().text, font_);
                break;

            case ROW_EGG:
                drawText(i18n::get(StrKey::FilterEgg), labelX, textY, T().text, font_);
                drawText(triText(gtsFilter_.egg), valueX, textY,
                         gtsFilter_.egg < 0 ? T().textDim : T().text, font_);
                break;

            case ROW_MIN_IVS:
                drawText(i18n::get(StrKey::GtsFilterMinIVs), labelX, textY, T().text, font_);
                if (gtsFilter_.minIvs < 0)
                    drawText(i18n::get(StrKey::FilterAny), valueX, textY, T().textDim, font_);
                else
                    drawText(std::to_string(gtsFilter_.minIvs) + " / 186", valueX, textY, T().text, font_);
                break;

            case ROW_LEGAL:
                drawText(i18n::get(StrKey::GtsFilterLegalOnly), labelX, textY, T().text, font_);
                drawText(i18n::get(gtsFilter_.onlyLegal ? StrKey::FilterYes : StrKey::GtsFilterNo),
                         valueX, textY, T().text, font_);
                break;

            case ROW_SORT:
                drawText(i18n::get(StrKey::GtsFilterSort), labelX, textY, T().text, font_);
                drawText(i18n::get(gtsFilter_.byPopularity ? StrKey::GtsSortPopular
                                                           : StrKey::GtsSortRecent),
                         valueX, textY, T().text, font_);
                break;

            case ROW_RESET:
                drawText(i18n::get(StrKey::FilterReset), labelX, textY, T().text, font_);
                break;

            case ROW_SEARCH:
                drawText(i18n::get(StrKey::FilterSearch), labelX, textY, T().goldLabel, font_);
                break;
        }
    }

    drawTextCentered(i18n::get(StrKey::FilterFooter), popX + POP_W / 2,
                     popY + POP_H - 20, T().textDim, fontSmall_);
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

    // Left/right change a value in place; A opens a picker or runs the search.
    auto adjust = [&](int dir) {
        switch (gtsFilterCursor_) {
            case ROW_SPECIES:
                if (dir < 0 || gtsFilter_.species != 0) {
                    gtsFilter_.species = 0;
                    gtsFilter_.speciesName.clear();
                }
                break;
            case ROW_FAMILY: {
                int idx = (familyIndex(gtsFilter_.family) + dir + FAMILY_COUNT) % FAMILY_COUNT;
                gtsFilter_.family = FAMILIES[idx].key;
                break;
            }
            case ROW_SHINY: gtsFilter_.shiny = cycleTri(gtsFilter_.shiny, dir); break;
            case ROW_EGG:   gtsFilter_.egg   = cycleTri(gtsFilter_.egg, dir);   break;
            case ROW_MIN_IVS: {
                int idx = (ivStepIndex(gtsFilter_.minIvs) + dir + IV_STEP_COUNT) % IV_STEP_COUNT;
                gtsFilter_.minIvs = IV_STEPS[idx];
                break;
            }
            case ROW_LEGAL: gtsFilter_.onlyLegal = !gtsFilter_.onlyLegal; break;
            case ROW_SORT:  gtsFilter_.byPopularity = !gtsFilter_.byPopularity; break;
            default: break;
        }
    };

    switch (event.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            gtsFilterCursor_ = (gtsFilterCursor_ - 1 + ROW_COUNT) % ROW_COUNT;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            gtsFilterCursor_ = (gtsFilterCursor_ + 1) % ROW_COUNT;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  adjust(-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: adjust(+1); break;

        case SDL_CONTROLLER_BUTTON_B: // Switch A
            if (gtsFilterCursor_ == ROW_SPECIES) {
                // The same picker the box search uses; this flag is what tells
                // it which filter a confirmed pick belongs to.
                speciesPickerForGts_ = true;
                availableSpecies_.clear();
                buildAvailableSpeciesList();
                showSpeciesLetterPicker_ = true;
                speciesLetterCursor_ = 0;
                speciesLetterScroll_ = 0;
            } else if (gtsFilterCursor_ == ROW_RESET) {
                gtsFilter_ = Gts::Filter{};
            } else if (gtsFilterCursor_ == ROW_SEARCH) {
                showGtsFilter_ = false;
                if (gtsLoadPage(0)) {
                    gtsCursor_ = 0;
                    screen_ = AppScreen::GtsBrowse;
                } else {
                    // The filter stays as it was so it can be widened and tried
                    // again, rather than being thrown away on a miss.
                    showGtsFilter_ = true;
                }
            } else {
                adjust(+1);
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

    // Every family folder, not just one game's: nothing has been selected at
    // this point, and a card is worth depositing whichever game it came from.
    for (int i = 1; i < FAMILY_COUNT; i++) {
        const GameType rep = FAMILIES[i].rep;
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
}

void UI::drawGtsDepositPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    constexpr int POP_W = 1080;
    constexpr int POP_H = 560;
    constexpr int ROW_H = 34;
    const int popX = (SCREEN_W - POP_W) / 2;
    const int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);
    drawTextCentered(i18n::get(StrKey::GtsPickCard), popX + POP_W / 2, popY + 24,
                     T().goldLabel, font_);

    const int listX = popX + 20;
    const int listY = popY + 56;
    const int listW = 560;
    const int visible = (POP_H - 96) / ROW_H;

    if (gtsCardCursor_ < gtsCardScroll_)
        gtsCardScroll_ = gtsCardCursor_;
    if (gtsCardCursor_ >= gtsCardScroll_ + visible)
        gtsCardScroll_ = gtsCardCursor_ - visible + 1;

    const int total = static_cast<int>(gtsCards_.size());
    for (int i = 0; i < visible && gtsCardScroll_ + i < total; i++) {
        const int idx = gtsCardScroll_ + i;
        const int rowY = listY + i * ROW_H;

        if (idx == gtsCardCursor_) {
            drawRect(listX, rowY, listW, ROW_H - 3, T().menuHighlight);
            drawRectOutline(listX, rowY, listW, ROW_H - 3, T().cursor, 2);
        }

        // The folder tells you which game a card is for without decoding it,
        // which is the whole reason the listing is cheap.
        const GtsCard& card = gtsCards_[idx];
        drawText(card.file.label, listX + 10, rowY + 6, T().text, fontSmall_);

        std::string fam = bankFolderNameOf(card.folderGame);
        const auto& te = getTextEntry(fam, fontSmall_, T().textDim);
        drawText(fam, listX + listW - te.w - 10, rowY + 6, T().textDim, fontSmall_);
    }

    if (total > visible) {
        std::string pos = std::to_string(gtsCardCursor_ + 1) + " / " + std::to_string(total);
        drawText(pos, listX + 10, popY + POP_H - 26, T().textDim, fontSmall_);
    }

    // The right half shows what the highlighted card actually contains, read
    // from its QR code and never from its filename.
    const int paneX = listX + listW + 20;
    const int paneW = popX + POP_W - 20 - paneX;
    drawCardPreviewPane(gtsCardPreview_, gtsCardPreviewIdx_ != gtsCardCursor_,
                        paneX, listY, paneW, POP_H - 96);

    drawTextCentered(i18n::get(StrKey::StatusGtsDeposit), popX + POP_W / 2,
                     popY + POP_H - 20, T().textDim, fontSmall_);
}

void UI::handleGtsDepositInput(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            updateStick(SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX),
                        SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY));
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
    if (parsed.result != CardPayload::Result::Ok) {
        showMessageAndWait(i18n::get(StrKey::GtsFailed), i18n::get(StrKey::GtsCardUnreadable));
        return;
    }

    if (!showConfirmDialog(i18n::get(StrKey::GtsUploadConfirm),
                           i18n::fmt(StrKey::GtsUploadConfirmBody, parsed.pkm.displayName())))
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
        showMessageAndWait(i18n::get(StrKey::GtsDuplicate), i18n::get(StrKey::GtsDuplicateBody));
    } else {
        showMessageAndWait(i18n::get(StrKey::GtsDeposited), i18n::get(StrKey::GtsDepositedBody));
    }
    showGtsDeposit_ = false;
}
