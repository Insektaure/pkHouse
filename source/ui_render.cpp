#include "ui.h"
#include "i18n.h"
#include "species_converter.h"
#include "move_types.h"
#include "met_info.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// --- Sprites ---

static uint32_t spriteKey(uint16_t id, uint8_t form) {
    return uint32_t(id) | (uint32_t(form) << 16);
}

static SDL_Texture* loadSprite(const char* dir, uint16_t nationalId, uint8_t form,
                               SDL_Renderer* renderer) {
    char filename[64];
    // Try form-specific sprite first (e.g. 019-1.png)
    if (form != 0) {
        std::snprintf(filename, sizeof(filename), "%03d-%d.png", nationalId, form);
        std::string path;
        path = std::string("romfs:/") + dir + "/" + filename;
        SDL_Surface* surf = IMG_Load(path.c_str());
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
            return tex;
        }
    }

    // Fall back to base sprite (e.g. 019.png)
    std::snprintf(filename, sizeof(filename), "%03d.png", nationalId);
    std::string path;
    path = std::string("romfs:/") + dir + "/" + filename;
    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    return tex;
}

SDL_Texture* UI::getSprite(uint16_t nationalId, uint8_t form) {
    uint32_t key = spriteKey(nationalId, form);
    auto it = spriteCache_.find(key);
    if (it != spriteCache_.end())
        return it->second;

    SDL_Texture* tex = loadSprite("sprites", nationalId, form, renderer_);
    spriteCache_[key] = tex;
    return tex;
}

SDL_Texture* UI::getShinySprite(uint16_t nationalId, uint8_t form) {
    uint32_t key = spriteKey(nationalId, form);
    auto it = shinySpriteCache_.find(key);
    if (it != shinySpriteCache_.end())
        return it->second;

    SDL_Texture* tex = loadSprite("sprites_shiny", nationalId, form, renderer_);
    shinySpriteCache_[key] = tex;
    return tex;
}

void UI::freeSprites() {
    for (auto& [id, tex] : spriteCache_) {
        if (tex)
            SDL_DestroyTexture(tex);
    }
    spriteCache_.clear();
    for (auto& [id, tex] : shinySpriteCache_) {
        if (tex)
            SDL_DestroyTexture(tex);
    }
    shinySpriteCache_.clear();
    for (auto& [name, tex] : ribbonSpriteCache_) {
        if (tex)
            SDL_DestroyTexture(tex);
    }
    ribbonSpriteCache_.clear();
    for (auto& [id, tex] : ballSpriteCache_) {
        if (tex)
            SDL_DestroyTexture(tex);
    }
    ballSpriteCache_.clear();
    for (auto& [id, tex] : typeSpriteCache_) {
        if (tex)
            SDL_DestroyTexture(tex);
    }
    typeSpriteCache_.clear();
    if (iconShiny_)      { SDL_DestroyTexture(iconShiny_);      iconShiny_ = nullptr; }
    if (iconAlpha_)      { SDL_DestroyTexture(iconAlpha_);      iconAlpha_ = nullptr; }
    if (iconShinyAlpha_) { SDL_DestroyTexture(iconShinyAlpha_); iconShinyAlpha_ = nullptr; }
    if (iconDynamax_)    { SDL_DestroyTexture(iconDynamax_);    iconDynamax_ = nullptr; }
    if (iconHouse_)      { SDL_DestroyTexture(iconHouse_);      iconHouse_ = nullptr; }
    if (iconBoxFull_)     { SDL_DestroyTexture(iconBoxFull_);     iconBoxFull_ = nullptr; }
    if (iconBoxEmpty_)    { SDL_DestroyTexture(iconBoxEmpty_);    iconBoxEmpty_ = nullptr; }
    if (iconBoxNonEmpty_) { SDL_DestroyTexture(iconBoxNonEmpty_); iconBoxNonEmpty_ = nullptr; }
}

SDL_Texture* UI::getRibbonSprite(const std::string& filename) {
    auto it = ribbonSpriteCache_.find(filename);
    if (it != ribbonSpriteCache_.end())
        return it->second;

    std::string path;
    path = "romfs:/ribbons/" + filename + ".png";
    SDL_Texture* tex = IMG_LoadTexture(renderer_, path.c_str());
    ribbonSpriteCache_[filename] = tex; // cache even if null
    return tex;
}

SDL_Texture* UI::getBallSprite(uint8_t ballId) {
    auto it = ballSpriteCache_.find(ballId);
    if (it != ballSpriteCache_.end())
        return it->second;

    std::string path;
    path = "romfs:/balls/_ball" + std::to_string(ballId) + ".png";
    SDL_Texture* tex = IMG_LoadTexture(renderer_, path.c_str());
    ballSpriteCache_[ballId] = tex;
    return tex;
}

SDL_Texture* UI::getTypeSprite(uint8_t typeId) {
    auto it = typeSpriteCache_.find(typeId);
    if (it != typeSpriteCache_.end())
        return it->second;

    char filename[32];
    std::snprintf(filename, sizeof(filename), "type_icon_s_%02d.png", typeId);
    std::string path;
    path = std::string("romfs:/types/") + filename;
    SDL_Texture* tex = IMG_LoadTexture(renderer_, path.c_str());
    typeSpriteCache_[typeId] = tex;
    return tex;
}

// --- Rendering ---

void UI::drawRect(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer_, &r);
}

void UI::drawRectOutline(int x, int y, int w, int h, SDL_Color color, int thickness) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int t = 0; t < thickness; t++) {
        SDL_Rect r = {x + t, y + t, w - 2*t, h - 2*t};
        SDL_RenderDrawRect(renderer_, &r);
    }
}

static uint32_t packColor(SDL_Color c) {
    return (uint32_t(c.r) << 24) | (uint32_t(c.g) << 16) | (uint32_t(c.b) << 8) | c.a;
}

const UI::TextCacheEntry& UI::getTextEntry(const std::string& text, TTF_Font* f, SDL_Color color) {
    TextCacheKey key{text, f, packColor(color)};
    auto it = textCache_.find(key);
    if (it != textCache_.end())
        return it->second;

    // Cap cache size to limit GPU memory on Switch
    if (textCache_.size() >= 512)
        clearTextCache();

    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, text.c_str(), color);
    TextCacheEntry entry{};
    if (surf) {
        entry.tex = SDL_CreateTextureFromSurface(renderer_, surf);
        entry.w = surf->w;
        entry.h = surf->h;
        SDL_FreeSurface(surf);
    }
    return textCache_.emplace(std::move(key), entry).first->second;
}

void UI::clearTextCache() {
    for (auto& [k, e] : textCache_) {
        if (e.tex)
            SDL_DestroyTexture(e.tex);
    }
    textCache_.clear();
}

void UI::drawText(const std::string& text, int x, int y, SDL_Color color, TTF_Font* f) {
    if (!f || text.empty()) return;
    const auto& entry = getTextEntry(text, f, color);
    if (!entry.tex) return;
    SDL_Rect dst = {x, y, entry.w, entry.h};
    SDL_RenderCopy(renderer_, entry.tex, nullptr, &dst);
}

void UI::drawTextCentered(const std::string& text, int cx, int cy, SDL_Color color, TTF_Font* f) {
    if (!f || text.empty()) return;
    const auto& entry = getTextEntry(text, f, color);
    if (!entry.tex) return;
    SDL_Rect dst = {cx - entry.w/2, cy - entry.h/2, entry.w, entry.h};
    SDL_RenderCopy(renderer_, entry.tex, nullptr, &dst);
}


// --- hint bar ------------------------------------------------------------------
//
// A row of "button does thing" pairs along the bottom, each button drawn as the
// key it sits on rather than spelled out in the sentence.

namespace {

// A disc with a feathered rim: the interior is one rect per row, and only the
// pixels the circle partly covers are drawn individually, faded by how much of
// them it covers.
void hintCircle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c) {
    if (radius <= 0) return;
    for (int py = cy - radius - 1; py <= cy + radius + 1; py++) {
        const double dy = py + 0.5 - cy;
        const double inner = (radius - 0.5) * (radius - 0.5) - dy * dy;
        // floor(halfWidth - 0.5), because the pixel at offset n sits at n + 0.5
        // from the centre. Truncating instead takes one pixel too many, and it
        // is exactly the one that should have been feathered.
        const int solid = inner > 0.0
            ? static_cast<int>(std::floor(std::sqrt(inner) - 0.5)) : -1;
        if (solid >= 0) {
            SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
            SDL_Rect span = {cx - solid, py, solid * 2 + 1, 1};
            SDL_RenderFillRect(r, &span);
        }
        for (int px = cx - radius - 1; px <= cx + radius + 1; px++) {
            if (solid >= 0 && px >= cx - solid && px <= cx + solid) continue;
            const double dx = px + 0.5 - cx;
            const double covered = radius + 0.5 - std::sqrt(dx * dx + dy * dy);
            if (covered <= 0.0) continue;
            SDL_SetRenderDrawColor(r, c.r, c.g, c.b,
                static_cast<Uint8>(c.a * (covered >= 1.0 ? 1.0 : covered) + 0.5));
            SDL_RenderDrawPoint(r, px, py);
        }
    }
}

// A pill, for a button whose name is more than one character. Shrinking the text
// to fit a circle is the other way round, but the fonts here are opened at fixed
// sizes, and widening the key is what a real controller does anyway.
void hintPill(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c) {
    const int radius = h / 2;
    hintCircle(r, x + radius, y + radius, radius, c);
    hintCircle(r, x + w - radius - 1, y + radius, radius, c);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect middle = {x + radius, y, w - radius * 2, h};
    SDL_RenderFillRect(r, &middle);
}

constexpr int HINT_H   = 26;   // key height
constexpr int HINT_GAP = 7;    // key to its label
constexpr int HINT_SEP = 20;   // hint to hint

} // anonymous namespace

// How wide a key is: a circle for one character, a pill for more.
int UI::measureButtonHint(const char* button, const std::string& label) {
    int keyW = HINT_H;
    if (std::strcmp(button, HINT_DPAD) != 0) {
        const int textW = getTextEntry(button, fontSmall_, T().text).w;
        if (textW + 14 > keyW) keyW = textW + 14;
    }
    return keyW + HINT_GAP + getTextEntry(label, fontSmall_, T().statusText).w;
}

int UI::drawButtonHint(int x, int y, const char* button, const std::string& label) {
    const bool dpad = std::strcmp(button, HINT_DPAD) == 0;

    int keyW = HINT_H;
    if (!dpad) {
        const int textW = getTextEntry(button, fontSmall_, T().text).w;
        if (textW + 14 > keyW) keyW = textW + 14;
    }

    const int cy = y + HINT_H / 2;
    if (keyW == HINT_H)
        hintCircle(renderer_, x + HINT_H / 2, cy, HINT_H / 2, T().panelBg);
    else
        hintPill(renderer_, x, y, keyW, HINT_H, T().panelBg);

    if (dpad) {
        // A cross, not the words "D-Pad" - which is "Steuerkreuz" in German
        // and would not fit in a key at any size.
        const int cx = x + HINT_H / 2;
        constexpr int ARM = 9, THICK = 3;
        drawRect(cx - THICK / 2, cy - ARM / 2 - 2, THICK, ARM + 4, T().text);
        drawRect(cx - ARM / 2 - 2, cy - THICK / 2, ARM + 4, THICK, T().text);
    } else {
        drawTextCentered(button, x + keyW / 2, cy, T().text, fontSmall_);
    }

    drawText(label, x + keyW + HINT_GAP, cy - 8, T().statusText, fontSmall_);
    return measureButtonHint(button, label);
}

void UI::drawHintBar(const ButtonHint* hints, int count,
                     const std::string& message, int rightReserve) {
    drawRect(0, SCREEN_H - 35, SCREEN_W, 35, T().statusBarBg);

    const int y = SCREEN_H - 35 + (35 - HINT_H) / 2;
    int x = 15;

    if (!message.empty()) {
        drawText(message, x, SCREEN_H - 26, T().statusText, fontSmall_);
        x += getTextEntry(message, fontSmall_, T().statusText).w + HINT_SEP;
    }

    // How many keys actually fit. Measured rather than assumed: the labels are
    // translated, and the widest language is not the one this was laid out in.
    // Anything that would run under whatever the caller draws on the right is
    // dropped, which loses a hint but never overlaps.
    const int limit = SCREEN_W - 15 - (rightReserve > 0 ? rightReserve + HINT_SEP : 0);
    int shown = 0, width = x;
    for (int i = 0; i < count; i++) {
        const int w = measureButtonHint(hints[i].button, i18n::get(hints[i].labelKey))
                    + (i ? HINT_SEP : 0);
        if (width + w > limit) break;
        width += w;
        shown++;
    }

    for (int i = 0; i < shown; i++) {
        if (i) x += HINT_SEP;
        x += drawButtonHint(x, y, hints[i].button, i18n::get(hints[i].labelKey));
    }
}

void UI::drawStatusBar(const std::string& msg) {
    drawRect(0, SCREEN_H - 35, SCREEN_W, 35, T().statusBarBg);
    drawText(msg, 15, SCREEN_H - 26, T().statusText, fontSmall_);
}

// --- Box view (UI 2.0) ------------------------------------------------------------
//
// Laid out after external/UI_2.0 "Box view (Save <-> Bank)". Everything below
// the top bar is either one of the two box panels, the info strip for the
// Pokemon under the cursor, or the footer.

namespace {

std::string utf16ToUtf8(const std::u16string& s) {
    std::string out;
    for (char16_t c : s) {
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

// ASCII upper case, for the small caps panel tags. Anything outside ASCII is
// left alone rather than guessed at.
std::string upperAscii(std::string s) {
    for (char& c : s)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return s;
}

// Labels like "Dual Bank | " were written to sit before the game name in the
// old status bar; the top bar has its own separators.
std::string trimLabel(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '|'))
        s.pop_back();
    return s;
}

// Draws `text` so its baseline sits on `baseline`, which is what lines up runs
// of different sizes on one row.
int baselineTop(TTF_Font* f, int baseline) {
    return baseline - TTF_FontAscent(f);
}

} // anonymous namespace

SDL_Rect UI::slotRect(Panel panelId, int col, int row) const {
    const int panelX = (panelId == Panel::Game) ? PANEL_X_L : PANEL_X_R;
    const int cols = gridCols();
    const int gridW = cols * CELL_W + (cols - 1) * CELL_GAP;
    const int x0 = panelX + (PANEL_W - gridW) / 2;
    return {x0 + col * (CELL_W + CELL_GAP), GRID_Y + row * (CELL_H + CELL_GAP),
            CELL_W, CELL_H};
}

SDL_Texture* UI::spriteFor(uint16_t species, uint8_t form, bool shiny, bool egg) {
    if (egg) return getSprite(0);
    if (shiny)
        if (SDL_Texture* t = getShinySprite(species, form)) return t;
    return getSprite(species, form);
}

void UI::drawSpriteFit(SDL_Texture* tex, int cx, int cy, int size, Uint8 alpha) {
    if (!tex) return;
    int tw = 0, th = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
    int dw = size, dh = size;
    if (tw > 0 && th > 0) {
        const float scale = std::min(static_cast<float>(size) / tw,
                                     static_cast<float>(size) / th);
        dw = static_cast<int>(tw * scale);
        dh = static_cast<int>(th * scale);
    }
    SDL_Rect dst = {cx - dw / 2, cy - dh / 2, dw, dh};
    if (alpha != 255) SDL_SetTextureAlphaMod(tex, alpha);
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    if (alpha != 255) SDL_SetTextureAlphaMod(tex, 255);
}

const std::vector<UI::SlotDisplay>& UI::getSlotDisplays(Panel panel, int box) {
    BoxDisplayKey key{panel, box};
    auto it = slotDisplayCache_.find(key);
    if (it != slotDisplayCache_.end())
        return it->second;

    // Cap cache size
    // Big enough for every box of both panels, so the overview - which reads
    // them all - does not evict the boxes the main view is showing.
    if (slotDisplayCache_.size() >= 128)
        slotDisplayCache_.clear();

    int slots = maxSlots();
    std::vector<SlotDisplay> displays(slots);
    for (int s = 0; s < slots; s++) {
        Pokemon pkm = getPokemonAt(box, s, panel);
        auto& sd = displays[s];
        if (pkm.isEmpty()) {
            sd.empty = true;
            continue;
        }
        sd.empty   = false;
        sd.egg     = pkm.isEgg();
        sd.shiny   = pkm.isShiny();
        sd.alpha   = pkm.isAlpha();
        sd.gender  = pkm.gender();
        sd.species = pkm.species();
        sd.form    = pkm.form();
        sd.level   = pkm.level();
        sd.name    = pkm.displayName();
        if (sd.name.length() > 10)
            sd.name = sd.name.substr(0, 9) + ".";
    }
    return slotDisplayCache_.emplace(key, std::move(displays)).first->second;
}

void UI::invalidateSlotDisplay(Panel panel, int box) {
    slotDisplayCache_.erase(BoxDisplayKey{panel, box});
}

void UI::drawSlot(int x, int y, const SlotDisplay& sd, bool isCursor, int selectOrder,
                  int highlightState, bool isParty) {
    // Cursor: a ring outside the cell with a gap, so it never covers the
    // markers drawn on the cell's own rim.
    if (isCursor)
        strokeRounded(x - 6, y - 6, CELL_W + 12, CELL_H + 12, CELL_RADIUS + 5, 3, T().accent);

    if (sd.empty) {
        fillRounded(x, y, CELL_W, CELL_H, CELL_RADIUS,
                      isCursor ? T().cellCursor : T().slotEmpty);
        dashRounded(x, y, CELL_W, CELL_H, CELL_RADIUS, T().cellEmptyBorder);
    } else {
        SDL_Color bg = sd.egg ? T().slotEgg : T().slotFull;
        fillRounded(x, y, CELL_W, CELL_H, CELL_RADIUS, isCursor ? T().cellCursor : bg);
        strokeRounded(x, y, CELL_W, CELL_H, CELL_RADIUS, 1, T().cellBorder);
    }

    // Rim markers, weakest first so the strongest one shows: LGPE party
    // member, search match, then multi-select.
    const SDL_Color selColor = positionPreserve_ ? T().selectedPos : T().selected;
    if (isParty)
        strokeRounded(x, y, CELL_W, CELL_H, CELL_RADIUS, 2, T().partyMark);
    if (highlightState == 1)
        strokeRounded(x, y, CELL_W, CELL_H, CELL_RADIUS, 2, T().searchMatch);
    if (selectOrder > 0)
        strokeRounded(x, y, CELL_W, CELL_H, CELL_RADIUS, 2, selColor);

    if (!sd.empty) {
        drawSpriteFit(spriteFor(sd.species, sd.form, sd.shiny, sd.egg),
                      x + CELL_W / 2, y + CELL_H / 2, SPRITE_SIZE);

        // Shiny top-left, alpha top-right. The shiny icon is white and takes
        // the accent colour here.
        constexpr int ICON = 13;
        if (sd.shiny && iconShiny_) {
            SDL_SetTextureColorMod(iconShiny_, T().accent.r, T().accent.g, T().accent.b);
            SDL_Rect dst = {x + 7, y + 7, ICON, ICON};
            SDL_RenderCopy(renderer_, iconShiny_, nullptr, &dst);
            SDL_SetTextureColorMod(iconShiny_, 255, 255, 255);
        }
        if (sd.alpha && iconAlpha_) {
            SDL_Rect dst = {x + CELL_W - 7 - ICON, y + 7, ICON, ICON};
            SDL_RenderCopy(renderer_, iconAlpha_, nullptr, &dst);
        }
    }

    // Selection order, bottom-right.
    if (selectOrder > 0) {
        const int cx = x + CELL_W - 13, cy = y + CELL_H - 13;
        fillDisc(cx, cy, 10, selColor);
        drawTextCentered(std::to_string(selectOrder), cx, cy, T().textOnBadge,
                         uiFont(13, true));
    }

    // Search dim overlay (drawn last so it covers everything)
    if (highlightState == -1)
        fillRounded(x, y, CELL_W, CELL_H, CELL_RADIUS, T().searchDim);
}

void UI::drawBoxPanel(Panel panelId, bool isActive) {
    const bool left = (panelId == Panel::Game);
    const int px = left ? PANEL_X_L : PANEL_X_R;
    const int box = left ? gameBox_ : bankBox_;
    const SDL_Color accent = left ? T().accentSave : T().accentBank;

    // What this panel shows: the save, or one of the two banks.
    std::string tag, title;
    int boxCount = 1;
    bool loaded = true;
    if (left && !isDualBankMode()) {
        tag = i18n::get(StrKey::TagSave) + std::string(" \xc2\xb7 ") + gameDisplayNameOf(selectedGame_);
        title = save_.getBoxName(box);
        boxCount = save_.boxCount();
    } else if (left && leftBankName_.empty()) {
        tag = i18n::get(StrKey::TagBank);
        title = i18n::get(StrKey::NoBankLoaded);
        loaded = false;
    } else {
        const Bank& b = left ? bankLeft_ : bank_;
        tag = i18n::get(StrKey::TagBank) + std::string(" \xc2\xb7 ")
            + (left ? leftBankName_ : activeBankName_);
        title = b.getBoxName(box);
        boxCount = b.boxCount();
    }

    // Panel
    fillRounded(px, PANEL_Y, PANEL_W, PANEL_H, PANEL_RADIUS, T().panelBg);
    strokeRounded(px, PANEL_Y, PANEL_W, PANEL_H, PANEL_RADIUS, 1, T().panelBorder);

    // L / R keys. They move the box of whichever panel the cursor is in, so
    // only that panel's pair is lit.
    constexpr int KEY_W = 56, KEY_H = 44, KEY_INSET = 17;
    const SDL_Color keyText = isActive ? T().text : T().textMuted;
    TTF_Font* keyFont = uiFont(16, true);
    const int keyY = PANEL_Y + KEY_INSET;
    const int keyCy = keyY + KEY_H / 2;
    for (int side = 0; side < 2; side++) {
        const int kx = side == 0 ? px + KEY_INSET : px + PANEL_W - KEY_INSET - KEY_W;
        fillRounded(kx, keyY, KEY_W, KEY_H, 8, T().buttonBg);
        strokeRounded(kx, keyY, KEY_W, KEY_H, 8, 1, T().buttonBorder);
        const int kcx = kx + KEY_W / 2;
        if (side == 0) {
            fillArrow(kcx - 8, keyCy, 8, -1, keyText);
            drawTextCentered("L", kcx + 5, keyCy, keyText, keyFont);
        } else {
            drawTextCentered("R", kcx - 5, keyCy, keyText, keyFont);
            fillArrow(kcx + 8, keyCy, 8, +1, keyText);
        }
    }

    // Tag and title, centred between the keys.
    const int cx = px + PANEL_W / 2;
    const int maxW = PANEL_W - 2 * (KEY_INSET + KEY_W + 12);
    {
        TTF_Font* f = uiFont(12, true);
        constexpr int TRACK = 2;
        // fitText measures without tracking, so shrink its budget until the
        // tracked result fits, always cutting from the full tag.
        const std::string full = upperAscii(tag);
        std::string t = full;
        for (int budget = textWidth(full, f); budget > 0 && measureTextTracked(t, f, TRACK) > maxW; ) {
            budget -= 8;
            t = fitText(full, f, budget);
        }
        const int w = measureTextTracked(t, f, TRACK);
        drawTextTracked(t, cx - w / 2, PANEL_Y + 15, accent, f, TRACK);
    }

    const auto& displays = getSlotDisplays(panelId, box);
    {
        TTF_Font* fTitle = uiFont(22, true);
        TTF_Font* fCount = uiFont(15);
        std::string count;
        if (loaded) {
            int filled = 0;
            for (const auto& sd : displays)
                if (!sd.empty) filled++;
            count = std::to_string(filled) + " / " + std::to_string((int)displays.size());
        }
        const int countW = count.empty() ? 0 : textWidth(count, fCount) + 8;
        const std::string t = fitText(title, fTitle, maxW - countW);
        const int titleW = textWidth(t, fTitle);
        const int x0 = cx - (titleW + countW) / 2;
        const int baseline = PANEL_Y + 56;
        drawText(t, x0, baselineTop(fTitle, baseline), T().text, fTitle);
        if (!count.empty())
            drawText(count, x0 + titleW + 8, baselineTop(fCount, baseline), T().textMuted, fCount);
    }

    // Grid
    const int cols = gridCols();
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < cols; col++) {
            const int slot = row * cols + col;
            if (slot >= (int)displays.size()) continue;
            const auto& sd = displays[slot];
            const SDL_Rect r = slotRect(panelId, col, row);

            bool isCursor = isActive && cursor_.col == col && cursor_.row == row;
            int selOrder = 0;
            if (!selectedSlots_.empty()
                && panelId == selectedPanel_ && box == selectedBox_) {
                for (int i = 0; i < (int)selectedSlots_.size(); i++) {
                    if (selectedSlots_[i] == slot) {
                        selOrder = i + 1;
                        break;
                    }
                }
            }
            int hlState = 0;
            if (searchHighlightActive_ && !sd.empty)
                hlState = isSearchMatch(panelId, box, slot) ? 1 : -1;
            bool partySlot = left && !isDualBankMode() && save_.isLGPEPartySlot(box, slot);
            drawSlot(r.x, r.y, sd, isCursor, selOrder, hlState, partySlot);
        }
    }

    // Page dots, one per box, the current one drawn as a pill.
    if (loaded && boxCount > 1) {
        constexpr int DOT = 5, PILL = 14, GAP = 5;
        const int total = boxCount * (DOT + GAP) - GAP + (PILL - DOT);
        if (total <= PANEL_W - 40) {
            int x = cx - total / 2;
            for (int i = 0; i < boxCount; i++) {
                const bool cur = (i == box);
                const int w = cur ? PILL : DOT;
                fillRounded(x, DOTS_Y, w, DOT, DOT / 2, cur ? accent : T().dot);
                x += w + GAP;
            }
        } else {
            // More boxes than dots fit: say where we are instead.
            drawTextCentered(std::to_string(box + 1) + " / " + std::to_string(boxCount),
                             cx, DOTS_Y + 2, T().textMuted, uiFont(13));
        }
    }
}

void UI::drawTopBar() {
    drawRect(0, 0, SCREEN_W, ACCENT_RULE_H, T().accent);
    const int cy = ACCENT_RULE_H + (TOPBAR_H - ACCENT_RULE_H) / 2 - 4;

    // Logo
    int x = 32;
    if (iconHouse_) {
        SDL_SetTextureColorMod(iconHouse_, T().accent.r, T().accent.g, T().accent.b);
        SDL_Rect dst = {x, cy - 12, 24, 24};
        SDL_RenderCopy(renderer_, iconHouse_, nullptr, &dst);
        x += 24 + 12;
    }
    TTF_Font* fLogo = uiFont(24, true);
    const int baseline = cy + 8;
    drawText("pkHouse", x, baselineTop(fLogo, baseline), T().text, fLogo);
    x += textWidth("pkHouse", fLogo) + 16;

    drawRect(x, cy - 14, 1, 28, T().divider);
    x += 17;

    // Game, then whose save it is (or which bank mode we are in).
    TTF_Font* fGame = uiFont(18, true);
    const std::string game = gameDisplayNameOf(selectedGame_);
    drawText(game, x, baselineTop(fGame, baseline), T().text, fGame);
    x += textWidth(game, fGame) + 10;

    std::string who;
    if (allBanksMode_) {
        who = trimLabel(i18n::get(StrKey::LabelAllBanks));
    } else if (isDualBankMode()) {
        who = trimLabel(i18n::get(StrKey::LabelDualBank));
    } else {
        TrainerInfo ti = save_.getTrainerInfo();
        if (ti.valid) {
            const uint32_t tid = isFRLG(selectedGame_) ? (ti.id32 & 0xFFFF) : (ti.id32 % 1000000);
            who = utf16ToUtf8(ti.otName) + " \xc2\xb7 " + std::to_string(tid);
        } else if (selectedProfile_ >= 0 && selectedProfile_ < account_.profileCount()) {
            who = account_.profiles()[selectedProfile_].nickname;
        }
    }
    TTF_Font* fWho = uiFont(15);
    if (!who.empty())
        drawText(who, x, baselineTop(fWho, baseline), T().textDim, fWho);

    // Save state, right-aligned.
    const std::string status = unsavedChanges_ ? i18n::get(StrKey::StatusUnsaved)
                             : isDualBankMode() ? i18n::get(StrKey::StatusBanksClean)
                             : i18n::get(StrKey::StatusSaveClean);
    const int sw = textWidth(status, fWho);
    const int sx = SCREEN_W - 32 - sw;
    drawText(status, sx, baselineTop(fWho, baseline), T().text, fWho);
    fillDisc(sx - 14, cy, 4, unsavedChanges_ ? T().statusWarn : T().statusOk);
}

void UI::drawInfoStrip() {
    fillRounded(INFO_X, INFO_Y, INFO_W, INFO_H, 12, T().panelBg);
    strokeRounded(INFO_X, INFO_Y, INFO_W, INFO_H, 12, 1, T().panelBorder);

    // While holding, the strip describes what is in hand - the slot under the
    // cursor is where it is about to go, not what you are looking at.
    Pokemon pkm;
    if (holding_)
        pkm = heldMulti_.empty() ? heldPkm_ : heldMulti_[0];
    else
        pkm = getPokemonAt(cursor_.box, cursor_.slot(gridCols()), cursor_.panel);

    const int midY = INFO_Y + INFO_H / 2;
    if (pkm.isEmpty()) {
        TTF_Font* f = uiFont(15);
        drawText(i18n::get(StrKey::InfoEmptySlot), INFO_X + 24,
                 midY - TTF_FontHeight(f) / 2, T().textMuted, f);
        return;
    }

    // Portrait
    constexpr int PORTRAIT = 56;
    const int portX = INFO_X + 20, portY = midY - PORTRAIT / 2;
    fillRounded(portX, portY, PORTRAIT, PORTRAIT, 10, T().slotFull);
    drawSpriteFit(spriteFor(pkm.species(), pkm.form(), pkm.isShiny(), pkm.isEgg()),
                  portX + PORTRAIT / 2, portY + PORTRAIT / 2, 48);

    // Identity: marks, name, gender, level; then nature, ability, ball.
    const int idX = portX + PORTRAIT + 20;
    const int idRight = 400;
    {
        TTF_Font* fName = uiFont(20, true);
        TTF_Font* fLv   = uiFont(15);
        const int baseline = INFO_Y + 38;
        int x = idX;
        constexpr int ICON = 16;
        if (pkm.isShiny() && iconShiny_) {
            SDL_SetTextureColorMod(iconShiny_, T().accent.r, T().accent.g, T().accent.b);
            SDL_Rect dst = {x, baseline - ICON, ICON, ICON};
            SDL_RenderCopy(renderer_, iconShiny_, nullptr, &dst);
            SDL_SetTextureColorMod(iconShiny_, 255, 255, 255);
            x += ICON + 6;
        }
        if (pkm.isAlpha() && iconAlpha_) {
            SDL_Rect dst = {x, baseline - ICON, ICON, ICON};
            SDL_RenderCopy(renderer_, iconAlpha_, nullptr, &dst);
            x += ICON + 6;
        }

        std::string lv, gender;
        SDL_Color gColor = T().text;
        if (!pkm.isEgg()) {
            lv = i18n::get(StrKey::LvPrefix) + std::to_string(pkm.level());
            const uint8_t g = pkm.gender();
            if (g == 0) { gender = "\xe2\x99\x82"; gColor = T().genderMale; }
            if (g == 1) { gender = "\xe2\x99\x80"; gColor = T().genderFemale; }
        }
        const int tailW = (gender.empty() ? 0 : textWidth(gender, fName) + 8)
                        + (lv.empty() ? 0 : textWidth(lv, fLv) + 8);
        const std::string name = fitText(pkm.displayName(), fName, idRight - 16 - x - tailW);
        drawText(name, x, baselineTop(fName, baseline), T().text, fName);
        x += textWidth(name, fName) + 8;
        if (!gender.empty()) {
            drawText(gender, x, baselineTop(fName, baseline), gColor, fName);
            x += textWidth(gender, fName) + 8;
        }
        if (!lv.empty())
            drawText(lv, x, baselineTop(fLv, baseline), T().textDim, fLv);

        // Nature · Ability · [ball icon] Ball. The ball name is what gets
        // cut when the line is too long; nature and ability always show.
        TTF_Font* fSub = uiFont(14);
        const int subTop = baselineTop(fSub, INFO_Y + 62);
        const int subRight = idRight - 16;
        std::string sub = NatureName::get(pkm.nature()) + " \xc2\xb7 " + AbilityName::get(pkm.ability());
        const char* ball = BallName::get(pkm.ball());
        if (ball[0] != '\0')
            sub += " \xc2\xb7 ";
        sub = fitText(sub, fSub, subRight - idX);
        drawText(sub, idX, subTop, T().textDim, fSub);
        int bx = idX + textWidth(sub, fSub);
        if (ball[0] != '\0' && bx < subRight) {
            constexpr int BALL = 16;
            const int subCy = subTop + TTF_FontHeight(fSub) / 2;
            if (SDL_Texture* ballTex = getBallSprite(pkm.ball())) {
                SDL_Rect dst = {bx, subCy - BALL / 2, BALL, BALL};
                SDL_RenderCopy(renderer_, ballTex, nullptr, &dst);
                bx += BALL + 4;
            }
            if (bx < subRight)
                drawText(fitText(ball, fSub, subRight - bx), bx, subTop, T().textDim, fSub);
        }
    }

    drawRect(idRight, INFO_Y + 14, 1, INFO_H - 28, T().divider);

    // Moves, 2 x 2, with the type icons the card export uses.
    {
        constexpr int PILL_W = 194, PILL_H = 26, ICON_R = 9;
        const int x0 = idRight + 20, y0 = INFO_Y + 12;
        TTF_Font* fMove = uiFont(14);
        const uint16_t moves[4] = {pkm.move1(), pkm.move2(), pkm.move3(), pkm.move4()};
        for (int i = 0; i < 4; i++) {
            const int mx = x0 + (i % 2) * (PILL_W + 12);
            const int my = y0 + (i / 2) * (PILL_H + 8);
            const int mcy = my + PILL_H / 2;
            fillRounded(mx, my, PILL_W, PILL_H, 6, T().buttonBg);
            const int nameX = mx + 8 + ICON_R * 2 + 8;
            const int nameY = mcy - TTF_FontHeight(fMove) / 2;
            if (moves[i] == 0) {
                drawText("---", nameX, nameY, T().textMuted, fMove);
                continue;
            }
            const uint8_t type = getMoveType(moves[i], pkm.gameType_);
            if (type < 18)
                if (SDL_Texture* typeTex = getTypeSprite(type))
                    blitCircular(renderer_, typeTex, mx + 8 + ICON_R, mcy, ICON_R, T().buttonBg);
            drawText(fitText(MoveName::get(moves[i]), fMove, mx + PILL_W - 8 - nameX),
                     nameX, nameY, T().text, fMove);
        }
    }

    const int statsX = idRight + 20 + 2 * 194 + 12 + 20;
    drawRect(statsX, INFO_Y + 14, 1, INFO_H - 28, T().divider);

    // Perfect IVs and EV total.
    {
        const int ivs[6] = {pkm.ivHp(), pkm.ivAtk(), pkm.ivDef(),
                            pkm.ivSpA(), pkm.ivSpD(), pkm.ivSpe()};
        int perfect = 0;
        for (int v : ivs) if (v == 31) perfect++;
        const int evs = pkm.evHp() + pkm.evAtk() + pkm.evDef()
                      + pkm.evSpA() + pkm.evSpD() + pkm.evSpe();

        TTF_Font* fBadge = uiFont(12, true);
        const std::string badge = i18n::fmt(StrKey::InfoPerfectIvs, std::to_string(perfect));
        const int bw = measureTextTracked(badge, fBadge, 1) + 20;
        const int bx = statsX + 20, by = INFO_Y + 20;
        fillRounded(bx, by, bw, 22, 11, T().badgeBg);
        drawTextTracked(badge, bx + 10, by + 11 - TTF_FontHeight(fBadge) / 2,
                        T().accent, fBadge, 1);

        TTF_Font* fEv = uiFont(14);
        drawText(i18n::fmt(StrKey::InfoEvTotal, std::to_string(evs)), bx,
                 baselineTop(fEv, INFO_Y + 62), T().textDim, fEv);
    }

    // Trainer and origin, right-aligned.
    {
        const int right = INFO_X + INFO_W - 20;
        const int maxW = right - (statsX + 150);
        TTF_Font* fOt = uiFont(16, true);
        const std::string ot = fitText(i18n::fmt(StrKey::InfoOt, pkm.otName(),
                                                 std::to_string(pkm.displayTid())), fOt, maxW);
        drawText(ot, right - textWidth(ot, fOt), baselineTop(fOt, INFO_Y + 36), T().text, fOt);

        std::string origin = VersionName::get(pkm.originVersion());
        const std::string& loc = LocationName::get(selectedGame_, pkm.metLocation());
        if (!loc.empty())
            origin += origin.empty() ? loc : " \xc2\xb7 " + loc;
        TTF_Font* fOrig = uiFont(13);
        origin = fitText(origin, fOrig, maxW);
        drawText(origin, right - textWidth(origin, fOrig), baselineTop(fOrig, INFO_Y + 60),
                 T().textDim, fOrig);
    }
}

// --- Footer ----------------------------------------------------------------------

int UI::drawFooterKey(int x, int cy, const char* button, bool measureOnly) {
    constexpr int CAP = 24;   // circle diameter
    constexpr int PILL_H = 20;
    TTF_Font* f = uiFont(13, true);

    // "ZL ZR" is two caps side by side.
    const std::string all = button;
    int w = 0;
    size_t start = 0;
    while (start <= all.size()) {
        size_t sp = all.find(' ', start);
        if (sp == std::string::npos) sp = all.size();
        const std::string cap = all.substr(start, sp - start);
        start = sp + 1;
        if (cap.empty()) continue;
        if (w) w += 6;

        const int kx = x + w;
        if (cap == HINT_DPAD) {
            if (!measureOnly) {
                fillDisc(kx + CAP / 2, cy, CAP / 2, T().keyCap);
                constexpr int ARM = 13, THICK = 3;
                drawRect(kx + CAP / 2 - THICK / 2, cy - ARM / 2, THICK, ARM, T().keyCapText);
                drawRect(kx + CAP / 2 - ARM / 2, cy - THICK / 2, ARM, THICK, T().keyCapText);
            }
            w += CAP;
        } else if (cap.size() == 1) {
            if (!measureOnly) {
                fillDisc(kx + CAP / 2, cy, CAP / 2, T().keyCap);
                drawTextCentered(cap, kx + CAP / 2, cy, T().keyCapText, f);
            }
            w += CAP;
        } else {
            const int pw = textWidth(cap, f) + 12;
            if (!measureOnly) {
                fillRounded(kx, cy - PILL_H / 2, pw, PILL_H, 5, T().keyCap);
                drawTextCentered(cap, kx + pw / 2, cy, T().keyCapText, f);
            }
            w += pw;
        }
    }
    return w;
}

void UI::drawFooterBar(const ButtonHint* hints, int count, const std::string& message,
                       int rightReserve) {
    drawRect(0, FOOTER_Y, SCREEN_W, FOOTER_H, T().bg);
    drawRect(0, FOOTER_Y, SCREEN_W, 1, T().panelBorder);
    const int cy = FOOTER_Y + FOOTER_H / 2;

    // Version, right-aligned, unless the caller has its own use for that end
    // of the bar. Measured first either way so the hints stop short of it.
    int verW = rightReserve;
    if (rightReserve < 0) {
        TTF_Font* fVerName = uiFont(13, true);
        TTF_Font* fVer     = uiFont(13);
        const std::string ver = " v" APP_VERSION;
        verW = measureTextTracked("PKHOUSE", fVerName, 2) + textWidth(ver, fVer);
        int vx = SCREEN_W - 32 - verW;
        const int vy = cy - TTF_FontHeight(fVer) / 2;
        vx += drawTextTracked("PKHOUSE", vx, vy, T().textMuted, fVerName, 2);
        drawText(ver, vx, vy, T().textMuted, fVer);
    }

    TTF_Font* fLabel = uiFont(15, true);
    int x = 32;
    const int limit = SCREEN_W - 32 - verW - 24;
    if (!message.empty()) {
        TTF_Font* fMsg = uiFont(15);
        const std::string m = fitText(message, fMsg, limit - x);
        drawText(m, x, cy - TTF_FontHeight(fMsg) / 2, T().statusText, fMsg);
        x += textWidth(m, fMsg) + 24;
    }

    // Labels are translated, so what fits is measured, not assumed; hints that
    // would run into the version are dropped from the end.
    for (int i = 0; i < count; i++) {
        const std::string label = i18n::get(hints[i].labelKey);
        const int w = drawFooterKey(x, cy, hints[i].button, true) + 8 + textWidth(label, fLabel);
        if (x + w > limit) break;
        x += drawFooterKey(x, cy, hints[i].button, false) + 8;
        drawText(label, x, cy - TTF_FontHeight(fLabel) / 2, T().text, fLabel);
        x += textWidth(label, fLabel) + 22;
    }
}

void UI::drawFrame() {
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);

    // Sync cursor box to the active panel
    if (cursor_.panel == Panel::Game)
        gameBox_ = cursor_.box;
    else
        bankBox_ = cursor_.box;

    // The overview covers the whole screen, and no other popup can be opened
    // on top of it, so the box view underneath would only be painted over.
    if (showBoxView_) {
        drawBoxViewOverlay();
        return;
    }

    drawTopBar();
    drawBoxPanel(Panel::Game, cursor_.panel == Panel::Game);
    drawBoxPanel(Panel::Bank, cursor_.panel == Panel::Bank);
    drawInfoStrip();

    // Footer.
    //
    // Not a fixed row: what it says depends on what is in hand. Holding
    // something, mid drag-select or with matches highlighted, the useful
    // buttons are different ones and a line of text leads them.
    {
        std::string message;
        ButtonHint hints[7];
        int n = 0;

        // Same keys and labels as before the 2.0 redesign, so nobody has to
        // relearn the controls; only the way they are drawn changed.
        if (searchHighlightActive_ && !holding_ && selectedSlots_.empty() && !yHeld_) {
            message = i18n::fmt(StrKey::MsgSearch, std::to_string(searchResults_.size()));
            hints[n++] = {"B", StrKey::HintClear};
            hints[n++] = {"+", StrKey::HintMenu};
        } else if (holding_ && !heldMulti_.empty()) {
            message = i18n::fmt(StrKey::MsgHoldingMulti, std::to_string(heldMulti_.size()),
                      positionPreserve_ ? i18n::get(StrKey::KeepPositions) : "");
            hints[n++] = {"A", StrKey::HintPlace};
            hints[n++] = {"B", StrKey::HintReturn};
            hints[n++] = {"X", StrKey::HintDelete};
        } else if (holding_) {
            const std::string heldName = heldPkm_.displayName();
            if (!heldName.empty())
                message = i18n::fmt(StrKey::MsgHoldingSingle, heldName,
                                    std::to_string(heldPkm_.level()));
            hints[n++] = {"A", StrKey::HintPlace};
            hints[n++] = {"B", StrKey::HintReturn};
            hints[n++] = {"X", StrKey::HintDelete};
        } else if (yHeld_ && yDragActive_) {
            // No keys: the instruction is to let go of one already held.
            message = i18n::fmt(StrKey::StatusDrag, std::to_string(selectedSlots_.size()));
        } else if (!selectedSlots_.empty()) {
            message = i18n::fmt(StrKey::MsgSelected, std::to_string(selectedSlots_.size()),
                      positionPreserve_ ? i18n::get(StrKey::KeepPositions) : "");
            hints[n++] = {"A", StrKey::HintPickUp};
            hints[n++] = {"Y", StrKey::HintToggleDrag};
            hints[n++] = {"B", StrKey::HintClear};
        } else {
            hints[n++] = {HINT_DPAD, StrKey::HintMove};
            hints[n++] = {"L/R", StrKey::HintBox};
            hints[n++] = {"A", StrKey::HintPickPlace};
            hints[n++] = {"Y", StrKey::HintSelect};
            hints[n++] = {"YY", StrKey::HintAll};
            hints[n++] = {"B", StrKey::HintCancel};
            hints[n++] = {"X", StrKey::HintDetail};
        }

        drawFooterBar(hints, n, message);
    }

    // Held Pokemon overlay (draw on top of panels, under popups)
    if (holding_)
        drawHeldOverlay();

    // Detail popup overlay
    if (showDetail_) {
        Pokemon pkm = getPokemonAt(cursor_.box, cursor_.slot(gridCols()), cursor_.panel);
        if (pkm.isEmpty()) {
            showDetail_ = false;
        } else {
            drawDetailPopup(pkm);
        }
    }

    // Menu popup overlay
    if (showMenu_) {
        drawMenuPopup();
    }

    // Search popups
    if (showSearchFilter_) {
        drawSearchFilterPopup();
    }
    if (showSearchResults_) {
        drawSearchResultsPopup();
    }

    // Species picker overlays (drawn on top of search filter)
    if (showSpeciesLetterPicker_) {
        drawSpeciesLetterPicker();
    }
    if (showSpeciesListPicker_) {
        drawSpeciesListPicker();
    }

    // Wondercard list popup
    if (showWondercardList_) {
        drawWondercardListPopup();
    }

    // Card import list popup
    if (showCardList_) {
        drawCardListPopup();
    }
}

// --- Polygon rendering helpers for radar charts ---
// Unit vectors for a regular hexagon (top, top-right, bottom-right, bottom, bottom-left, top-left)
static constexpr double HEX_COS[6] = { 0.0,  0.866025,  0.866025, 0.0, -0.866025, -0.866025 };
static constexpr double HEX_SIN[6] = {-1.0, -0.5,       0.5,      1.0,  0.5,      -0.5      };

namespace {

void fillConvexPolygon(SDL_Renderer* renderer, const SDL_Point pts[], int count, SDL_Color color) {
    if (count < 3) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    int minY = pts[0].y, maxY = pts[0].y;
    for (int i = 1; i < count; i++) {
        if (pts[i].y < minY) minY = pts[i].y;
        if (pts[i].y > maxY) maxY = pts[i].y;
    }

    for (int y = minY; y <= maxY; y++) {
        int minX = 99999, maxX = -99999;
        for (int i = 0; i < count; i++) {
            int j = (i + 1) % count;
            int y0 = pts[i].y, y1 = pts[j].y;
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                int x = pts[i].x + (int)((long long)(y - y0) * (pts[j].x - pts[i].x) / (y1 - y0));
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
            }
        }
        if (minX <= maxX)
            SDL_RenderDrawLine(renderer, minX, y, maxX, y);
    }
}

void drawPolygonOutline(SDL_Renderer* renderer, const SDL_Point pts[], int count, SDL_Color color) {
    if (count < 2) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int i = 0; i < count; i++) {
        int j = (i + 1) % count;
        SDL_RenderDrawLine(renderer, pts[i].x, pts[i].y, pts[j].x, pts[j].y);
    }
}

} // anonymous namespace

void UI::drawRadarChart(int cx, int cy, int radius, const int values[6], int maxVal) {
    const std::string labels[6] = {i18n::get(StrKey::StatHP), i18n::get(StrKey::StatAtk),
        i18n::get(StrKey::StatDef), i18n::get(StrKey::StatSpe), i18n::get(StrKey::StatSpD), i18n::get(StrKey::StatSpA)};
    constexpr int N = 6;
    constexpr int LABEL_MARGIN = 12;

    // Compute hex vertices (starting from top, clockwise)
    SDL_Point outer[N];
    for (int i = 0; i < N; i++) {
        outer[i].x = cx + static_cast<int>(radius * HEX_COS[i]);
        outer[i].y = cy + static_cast<int>(radius * HEX_SIN[i]);
    }

    // Guide lines from center to each vertex
    SDL_Color guide = {T().textDim.r, T().textDim.g, T().textDim.b, 50};
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, guide.r, guide.g, guide.b, guide.a);
    for (int i = 0; i < N; i++)
        SDL_RenderDrawLine(renderer_, cx, cy, outer[i].x, outer[i].y);

    // Intermediate ring at 50%
    SDL_Point mid[N];
    for (int i = 0; i < N; i++) {
        mid[i].x = cx + static_cast<int>(radius * 0.5 * HEX_COS[i]);
        mid[i].y = cy + static_cast<int>(radius * 0.5 * HEX_SIN[i]);
    }
    drawPolygonOutline(renderer_, mid, N, guide);

    // Outer hex border
    drawPolygonOutline(renderer_, outer, N, T().textDim);

    // Compute data polygon vertices
    SDL_Point data[N];
    for (int i = 0; i < N; i++) {
        double frac = maxVal > 0 ? std::min(1.0, static_cast<double>(values[i]) / maxVal) : 0.0;
        double r = radius * frac;
        data[i].x = cx + static_cast<int>(r * HEX_COS[i]);
        data[i].y = cy + static_cast<int>(r * HEX_SIN[i]);
    }

    // Fill data polygon using triangle fan from center (handles concave shapes)
    SDL_Color fill = {T().cursor.r, T().cursor.g, T().cursor.b, 60};
    for (int i = 0; i < N; i++) {
        int j = (i + 1) % N;
        SDL_Point tri[3] = {{cx, cy}, data[i], data[j]};
        fillConvexPolygon(renderer_, tri, 3, fill);
    }

    // Data polygon outline
    SDL_Color outline = {T().cursor.r, T().cursor.g, T().cursor.b, 200};
    drawPolygonOutline(renderer_, data, N, outline);

    // Small dots at each data vertex
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, outline.r, outline.g, outline.b, outline.a);
    for (int i = 0; i < N; i++) {
        SDL_Rect dot = {data[i].x - 2, data[i].y - 2, 5, 5};
        SDL_RenderFillRect(renderer_, &dot);
    }

    // Labels and values around the chart
    for (int i = 0; i < N; i++) {
        int lx = cx + static_cast<int>((radius + LABEL_MARGIN) * HEX_COS[i]);
        int ly = cy + static_cast<int>((radius + LABEL_MARGIN) * HEX_SIN[i]);

        const std::string& name = labels[i];
        std::string valStr = std::to_string(values[i]);

        SDL_Color nameColor = T().goldLabel;
        SDL_Color valColor = (values[i] >= maxVal) ? T().shiny
                           : (values[i] == 0)      ? T().textDim
                           :                          T().text;

        auto& ne = getTextEntry(name, fontSmall_, nameColor);
        int nw = ne.w, nh = ne.h;
        auto& ve = getTextEntry(valStr, fontSmall_, valColor);
        int vw = ve.w, vh = ve.h;

        if (i == 0) { // Top: centered, name then value downward
            drawText(name, lx - nw / 2, ly - nh * 2 - 2, nameColor, fontSmall_);
            drawText(valStr, lx - vw / 2, ly - vh, valColor, fontSmall_);
        } else if (i == 3) { // Bottom: centered, value then name downward
            drawText(valStr, lx - vw / 2, ly, valColor, fontSmall_);
            drawText(name, lx - nw / 2, ly + vh + 2, nameColor, fontSmall_);
        } else if (i == 1 || i == 2) { // Right: left-aligned
            drawText(name, lx + 4, ly - nh, nameColor, fontSmall_);
            drawText(valStr, lx + 4, ly + 2, valColor, fontSmall_);
        } else { // Left (4, 5): right-aligned
            drawText(name, lx - nw - 4, ly - nh, nameColor, fontSmall_);
            drawText(valStr, lx - vw - 4, ly + 2, valColor, fontSmall_);
        }
    }
}

void UI::drawDetailPopup(const Pokemon& pkm, const char* footerKey,
                         const char* badge) {
    // Semi-transparent dark overlay
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    // Popup rect centered. Grow by one line when the optional HT row is shown
    // so the Moves/Ribbons region keeps the same layout as the no-HT case.
    constexpr int POP_W = 900;
    const int POP_H = 550 + (pkm.hasHandlingTrainer() ? 28 : 0);
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);

    // Top-right, in the accent colour, because on the board this is the one
    // thing that decides whether the Pokemon is any use to you.
    if (badge != nullptr && badge[0] != '\0') {
        const auto& be = getTextEntry(badge, fontSmall_, T().goldLabel);
        const int bw = be.w + 20;
        const int bx = popX + POP_W - bw - 14;
        drawRect(bx, popY + 12, bw, 24, T().menuHighlight);
        drawTextCentered(badge, bx + bw / 2, popY + 24, T().goldLabel, fontSmall_);
    }

    // Large sprite (128x128) top-left
    constexpr int LARGE_SPRITE = 128;
    int sprX = popX + 20;
    int sprY = popY + 20;

    SDL_Texture* sprite = nullptr;
    uint8_t pkmForm = pkm.form();
    if (pkm.isEgg()) {
        sprite = getSprite(0);
    } else if (pkm.isShiny()) {
        sprite = getShinySprite(pkm.species(), pkmForm);
        if (!sprite) sprite = getSprite(pkm.species(), pkmForm);
    } else {
        sprite = getSprite(pkm.species(), pkmForm);
    }
    if (sprite) {
        int texW, texH;
        SDL_QueryTexture(sprite, nullptr, nullptr, &texW, &texH);
        int dstW, dstH;
        if (texW > 0 && texH > 0) {
            float scale = std::min(static_cast<float>(LARGE_SPRITE) / texW,
                                   static_cast<float>(LARGE_SPRITE) / texH);
            dstW = static_cast<int>(texW * scale);
            dstH = static_cast<int>(texH * scale);
        } else {
            dstW = LARGE_SPRITE;
            dstH = LARGE_SPRITE;
        }
        int dx = sprX + (LARGE_SPRITE - dstW) / 2;
        int dy = sprY + (LARGE_SPRITE - dstH) / 2;
        SDL_Rect dst = {dx, dy, dstW, dstH};
        SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
    }

    // Shiny/Alpha icon next to sprite
    SDL_Texture* statusIcon = nullptr;
    if (pkm.isShiny() && pkm.isAlpha())
        statusIcon = iconShinyAlpha_;
    else if (pkm.isShiny())
        statusIcon = iconShiny_;
    else if (pkm.isAlpha())
        statusIcon = iconAlpha_;
    if (statusIcon) {
        SDL_Rect iconDst = {sprX + LARGE_SPRITE + 4, sprY, 20, 20};
        SDL_RenderCopy(renderer_, statusIcon, nullptr, &iconDst);
    }

    // --- Left column info (next to sprite) ---
    int infoX = sprX + LARGE_SPRITE + 30;
    int infoY = sprY + 4;

    // Ball icon + Species name + level + gender
    constexpr int BALL_SZ = 24;
    int nameStartX = infoX;
    uint8_t ballId = pkm.ball();
    if (ballId > 0) {
        SDL_Texture* ballTex = getBallSprite(ballId);
        if (ballTex) {
            int textH = TTF_FontHeight(font_);
            int by = infoY + (textH - BALL_SZ) / 2;
            SDL_Rect ballDst = {infoX, by, BALL_SZ, BALL_SZ};
            SDL_RenderCopy(renderer_, ballTex, nullptr, &ballDst);
            nameStartX += BALL_SZ + 4;
        }
    }
    std::string specName = SpeciesName::get(pkm.species());
    SDL_Color nameColor = pkm.isShiny() ? T().shiny : T().text;
    drawText(specName, nameStartX, infoY, nameColor, font_);

    std::string lvlStr = "  " + i18n::get(StrKey::LvPrefix) + std::to_string(pkm.level());
    int nameW = getTextEntry(specName, font_, nameColor).w;
    drawText(lvlStr, nameStartX + nameW, infoY, T().text, font_);

    // Gender symbol
    uint8_t g = pkm.gender();
    int afterLvl = nameStartX + nameW;
    int lvlW = getTextEntry(lvlStr, font_, T().text).w;
    afterLvl += lvlW + 4;
    if (g == 0)
        drawText("\xe2\x99\x82", afterLvl, infoY, T().genderMale, font_);
    else if (g == 1)
        drawText("\xe2\x99\x80", afterLvl, infoY, T().genderFemale, font_);

    infoY += 30;

    // National dex ID
    std::string idStr = i18n::get(StrKey::NationalDexPrefix) + std::to_string(pkm.species());
    drawText(idStr, infoX, infoY, T().textDim, font_);
    infoY += 28;

    // OT + TID/SID
    std::string otStr = i18n::get(StrKey::OTPrefix) + pkm.otName() + " | " + i18n::get(StrKey::TIDPrefix) + std::to_string(pkm.displayTid())
                        + " | " + i18n::get(StrKey::SIDPrefix) + std::to_string(pkm.displaySid());
    drawText(otStr, infoX, infoY, T().textDim, font_);
    infoY += 28;

    // HT (handling trainer) — only for formats that store one
    if (pkm.hasHandlingTrainer()) {
        std::string ht = pkm.htName();
        std::string htStr = i18n::get(StrKey::HTPrefix) +
                            (ht.empty() ? i18n::get(StrKey::NoneItem) : ht);
        drawText(htStr, infoX, infoY, T().textDim, font_);
        infoY += 28;
    }

    // Nature
    std::string natureStr = i18n::get(StrKey::NaturePrefix) + NatureName::get(pkm.nature());
    drawText(natureStr, infoX, infoY, T().textDim, font_);
    infoY += 28;

    // Ability
    std::string abilityStr = i18n::get(StrKey::AbilityPrefix) + AbilityName::get(pkm.ability());
    drawText(abilityStr, infoX, infoY, T().textDim, font_);
    infoY += 28;

    // Held item
    uint16_t item = pkm.heldItem();
    std::string itemStr = i18n::get(StrKey::HeldItemPrefix) + (item != 0 ? ItemName::get(item) : i18n::get(StrKey::NoneItem));
    drawText(itemStr, infoX, infoY, T().textDim, font_);
    int infoBottom = infoY + 28; // baseline below the last info line

    // --- Below sprite: Moves ---
    // Start below whichever extends lower: the sprite or the info column.
    // The optional HT line can push the info column past the sprite's bottom.
    int movesX = popX + 30;
    int movesY = std::max(sprY + LARGE_SPRITE + 46, infoBottom);

    drawText(i18n::get(StrKey::Moves), movesX, movesY, T().text, font_);
    movesY += 30;

    constexpr int TYPE_ICON_W = 25;
    constexpr int TYPE_ICON_H = 25;
    constexpr int MOVE_ROW_H = 28;
    constexpr int MOVE_COL_W = 230;
    int textH = TTF_FontHeight(font_);
    uint16_t moves[4] = {pkm.move1(), pkm.move2(), pkm.move3(), pkm.move4()};
    for (int i = 0; i < 4; i++) {
        int col = i % 2;
        int row = i / 2;
        int mx = movesX + 10 + col * MOVE_COL_W;
        int my = movesY + row * MOVE_ROW_H;
        int iconY = my + (MOVE_ROW_H - TYPE_ICON_H) / 2;
        int txtY  = my + (MOVE_ROW_H - textH) / 2;
        if (moves[i] != 0) {
            uint8_t mtype = getMoveType(moves[i], pkm.gameType_);
            SDL_Texture* typeTex = getTypeSprite(mtype);
            if (typeTex) {
                SDL_Rect typeDst = {mx, iconY, TYPE_ICON_W, TYPE_ICON_H};
                SDL_RenderCopy(renderer_, typeTex, nullptr, &typeDst);
            }
            drawText(MoveName::get(moves[i]), mx + TYPE_ICON_W + 6, txtY, T().textDim, font_);
        } else {
            drawText("---", mx + TYPE_ICON_W + 6, txtY, T().textDim, font_);
        }
    }
    movesY += MOVE_ROW_H * 2;

    // --- Ribbons & Marks below moves ---
    auto ribbons = pkm.getRibbonsAndMarks();
    if (!ribbons.empty()) {
        movesY += 16;
        std::string ribTitle = i18n::fmt(StrKey::RibbonsMarks, std::to_string(ribbons.size()));
        drawText(ribTitle, movesX, movesY, T().text, font_);
        movesY += 30;

        // Two columns, small font with sprite icons
        int col1X = movesX + 4;
        int col2X = movesX + 230;
        int ribbonY = movesY;
        constexpr int RIB_ROW_H = 26;
        constexpr int ICON_SZ = 18;
        constexpr int ICON_PAD = 4;
        int maxY = popY + POP_H - 74;
        int col = 0;

        for (size_t i = 0; i < ribbons.size(); i++) {
            int x = (col == 0) ? col1X : col2X;
            if (ribbonY + RIB_ROW_H > maxY) {
                int remaining = static_cast<int>(ribbons.size() - i);
                drawText(i18n::fmt(StrKey::MoreRibbons, std::to_string(remaining)), x, ribbonY, T().textDim, font_);
                break;
            }

            // Center both icon and text vertically within the row
            int textH = TTF_FontHeight(font_);
            int contentH = std::max(ICON_SZ, textH);
            int baseY = ribbonY + (RIB_ROW_H - contentH) / 2;
            int iconY = baseY + (contentH - ICON_SZ) / 2;
            int textY = baseY + (contentH - textH) / 2;

            SDL_Texture* ribTex = getRibbonSprite(ribbons[i].filename);
            if (ribTex) {
                SDL_Rect dst = {x, iconY, ICON_SZ, ICON_SZ};
                SDL_RenderCopy(renderer_, ribTex, nullptr, &dst);
            }

            drawText(ribbons[i].name, x + ICON_SZ + ICON_PAD, textY, T().textDim, font_);

            col++;
            if (col >= 2) {
                col = 0;
                ribbonY += RIB_ROW_H;
            }
        }
    }

    // --- Right column: IV and EV radar charts ---
    // Order: HP, Atk, Def, Spe, SpD, SpA (clockwise from top)
    int chartCX = popX + POP_W * 3 / 4;
    constexpr int CHART_RADIUS = 65;

    // IVs radar chart
    drawTextCentered(i18n::get(StrKey::IVs), chartCX, popY + 18, T().text, font_);
    int ivsRadar[] = {pkm.ivHp(), pkm.ivAtk(), pkm.ivDef(), pkm.ivSpe(), pkm.ivSpD(), pkm.ivSpA()};
    drawRadarChart(chartCX, popY + 150, CHART_RADIUS, ivsRadar, 31);

    // EVs radar chart
    drawTextCentered(i18n::get(StrKey::EVs), chartCX, popY + 283, T().text, font_);
    int evsRadar[] = {pkm.evHp(), pkm.evAtk(), pkm.evDef(), pkm.evSpe(), pkm.evSpD(), pkm.evSpA()};
    drawRadarChart(chartCX, popY + 415, CHART_RADIUS, evsRadar, 252);

    // PID, EC, ID, TSV (bottom-left, small font, two lines)
    uint16_t tsv = (pkm.tid() ^ pkm.sid()) >> 4;
    char techBuf1[64], techBuf2[64];
    snprintf(techBuf1, sizeof(techBuf1), "PID: %08X   EC: %08X",
             pkm.pid(), pkm.encryptionConstant());
    snprintf(techBuf2, sizeof(techBuf2), "ID: %05u/%05u   TSV: %04u",
             pkm.tid(), pkm.sid(), tsv);
    drawText(techBuf1, popX + 20, popY + POP_H - 66, T().textDim, fontSmall_);
    drawText(techBuf2, popX + 20, popY + POP_H - 50, T().textDim, fontSmall_);

    // Close hint at bottom
    drawTextCentered(i18n::get(footerKey ? footerKey : StrKey::DetailFooter),
                     popX + POP_W / 2, popY + POP_H - 20, T().textDim, fontSmall_);
}

void UI::drawMenuPopup() {
    // Semi-transparent dark overlay
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    // Menu items differ by mode and game
    // SV/SwSh games get a "Wondercard" item after Search
    bool hasWC = gameInfo(selectedGame_).hasWondercards;
    bool hasExport = !selectedSlots_.empty();
    int menuCount;
    if (isDualBankMode())
        menuCount = hasWC ? 10 : 9;
    else
        menuCount = hasWC ? 9 : 8;
    if (hasExport) menuCount += 2;  // Export Selected + Export Cards

    constexpr int POP_W = 380;
    int POP_H = 50 + menuCount * 36 + 30;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);

    drawTextCentered(i18n::get(StrKey::MenuTitle), popX + POP_W / 2, popY + 22, T().text, font_);

    static char exportBuf[64];
    static char cardsBuf[64];
    if (hasExport) {
        std::snprintf(exportBuf, sizeof(exportBuf), "%s", i18n::fmt(StrKey::MenuExportSelected, std::to_string((int)selectedSlots_.size())).c_str());
        std::snprintf(cardsBuf, sizeof(cardsBuf), "%s", i18n::fmt(StrKey::MenuExportCards, std::to_string((int)selectedSlots_.size())).c_str());
    }

    const std::string labelsNormal[] = {
        i18n::get(StrKey::MenuTheme),
        i18n::get(StrKey::MenuLanguage),
        i18n::get(StrKey::MenuSearch),
        i18n::get(StrKey::MenuWondercard),
        exportBuf,
        cardsBuf,
        i18n::get(StrKey::MenuImportCard),
        i18n::get(StrKey::MenuSwitchBank),
        i18n::get(StrKey::MenuChangeGame),
        i18n::get(StrKey::MenuSaveQuit),
        i18n::get(StrKey::MenuQuitNoSave)
    };
    const std::string labelsApplet[] = {
        i18n::get(StrKey::MenuTheme),
        i18n::get(StrKey::MenuLanguage),
        i18n::get(StrKey::MenuSearch),
        i18n::get(StrKey::MenuWondercard),
        exportBuf,
        cardsBuf,
        i18n::get(StrKey::MenuImportCard),
        i18n::get(StrKey::MenuSwitchLeft),
        i18n::get(StrKey::MenuSwitchRight),
        i18n::get(StrKey::MenuChangeGame),
        i18n::get(StrKey::MenuSaveBanks),
        i18n::get(StrKey::MenuQuit)
    };
    // Build label list, skipping conditional items
    std::string visibleLabels[14];
    const std::string* allLabels = isDualBankMode() ? labelsApplet : labelsNormal;
    int allCount = isDualBankMode() ? 12 : 11;
    int vi = 0;
    for (int i = 0; i < allCount; i++) {
        if (!hasWC && i == 3) continue;     // skip Wondercard
        if (!hasExport && i == 4) continue; // skip Export Selected
        if (!hasExport && i == 5) continue; // skip Export Cards
        visibleLabels[vi++] = allLabels[i];
    }

    int rowH = 36;
    int startY = popY + 50;

    for (int i = 0; i < menuCount; i++) {
        int rowY = startY + i * rowH;
        if (i == menuSelection_) {
            drawRect(popX + 20, rowY, POP_W - 40, rowH - 4, T().menuHighlight);
            drawRectOutline(popX + 20, rowY, POP_W - 40, rowH - 4, T().cursor, 2);
        }
        drawTextCentered(visibleLabels[i], popX + POP_W / 2, rowY + (rowH - 4) / 2, T().text, font_);
    }

    drawTextCentered(i18n::get(StrKey::AConfirmBCancelMenu), popX + POP_W / 2, popY + POP_H - 18, T().textDim, fontSmall_);
}

void UI::drawThemeSelectorPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    constexpr int POP_W = 380;
    int POP_H = 50 + THEME_COUNT * 36 + 30;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);

    drawTextCentered(i18n::get(StrKey::SelectTheme), popX + POP_W / 2, popY + 22, T().text, font_);

    int rowH = 36;
    int startY = popY + 50;

    for (int i = 0; i < THEME_COUNT; i++) {
        int rowY = startY + i * rowH;
        if (i == themeSelCursor_) {
            drawRect(popX + 20, rowY, POP_W - 40, rowH - 4, T().menuHighlight);
            drawRectOutline(popX + 20, rowY, POP_W - 40, rowH - 4, T().cursor, 2);
        }
        std::string label = getThemeName(i);
        if (i == themeSelOriginal_) label = "* " + label + " *";
        drawTextCentered(label, popX + POP_W / 2, rowY + (rowH - 4) / 2, T().text, font_);
    }

    drawTextCentered(i18n::get(StrKey::ASelectBCancel), popX + POP_W / 2, popY + POP_H - 18, T().textDim, fontSmall_);
}

void UI::drawLanguageSelectorPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    int langCount = (int)langList_.size();
    constexpr int POP_W = 380;
    int POP_H = 50 + langCount * 36 + 30;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);

    drawTextCentered(i18n::get(StrKey::SelectLanguage), popX + POP_W / 2, popY + 22, T().text, font_);

    int rowH = 36;
    int startY = popY + 50;

    for (int i = 0; i < langCount; i++) {
        int rowY = startY + i * rowH;
        if (i == langSelCursor_) {
            drawRect(popX + 20, rowY, POP_W - 40, rowH - 4, T().menuHighlight);
            drawRectOutline(popX + 20, rowY, POP_W - 40, rowH - 4, T().cursor, 2);
        }
        std::string label = langDisplayName(langList_[i]);
        if (langList_[i] == i18n::currentLang()) label = "* " + label + " *";
        drawTextCentered(label, popX + POP_W / 2, rowY + (rowH - 4) / 2, T().text, font_);
    }

    drawTextCentered(i18n::get(StrKey::ASelectBCancel), popX + POP_W / 2, popY + POP_H - 18, T().textDim, fontSmall_);
}

void UI::drawSearchFilterPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    bool hasAlpha = gameInfo(selectedGame_).hasAlphaForms;
    int rowCount = hasAlpha ? 12 : 11;

    constexpr int POP_W = 600;
    constexpr int ROW_H = 36;
    int POP_H = 50 + rowCount * ROW_H + 30;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);

    drawTextCentered(i18n::get(StrKey::SearchFilter), popX + POP_W / 2, popY + 22, T().text, font_);

    int startY = popY + 50;
    int labelX = popX + 30;
    int valueX = popX + 230;

    int visualRow = 0;
    for (int i = 0; i < 12; i++) {
        if (i == 4 && !hasAlpha) continue;

        int rowY = startY + visualRow * ROW_H;

        if (i == searchFilterCursor_) {
            drawRect(popX + 20, rowY, POP_W - 40, ROW_H - 4, T().menuHighlight);
            drawRectOutline(popX + 20, rowY, POP_W - 40, ROW_H - 4, T().cursor, 2);
        }

        int textY = rowY + (ROW_H - 4) / 2 - 9;

        switch (i) {
            case 0: {
                drawText(i18n::get(StrKey::FilterSpecies), labelX, textY, T().text, font_);
                if (searchFilter_.speciesId > 0) {
                    SDL_Texture* spr = getSprite(searchFilter_.speciesId);
                    if (spr) {
                        int sprSize = ROW_H - 6;
                        SDL_Rect dst = { valueX, rowY + 2, sprSize, sprSize };
                        SDL_RenderCopy(renderer_, spr, nullptr, &dst);
                        drawText(searchFilter_.speciesName, valueX + sprSize + 6, textY, T().text, font_);
                    } else {
                        drawText(searchFilter_.speciesName, valueX, textY, T().text, font_);
                    }
                } else {
                    drawText(i18n::get(StrKey::FilterAny), valueX, textY, T().textDim, font_);
                }
                break;
            }
            case 1:
                drawText(i18n::get(StrKey::FilterOT), labelX, textY, T().text, font_);
                drawText(searchFilter_.otName.empty() ? i18n::get(StrKey::FilterAny) : searchFilter_.otName,
                         valueX, textY, searchFilter_.otName.empty() ? T().textDim : T().text, font_);
                break;
            case 2:
                drawText(i18n::get(StrKey::FilterShiny), labelX, textY, T().text, font_);
                drawText(searchFilter_.filterShiny ? i18n::get(StrKey::FilterYes) : i18n::get(StrKey::FilterOff),
                         valueX, textY, searchFilter_.filterShiny ? T().shiny : T().textDim, font_);
                break;
            case 3:
                drawText(i18n::get(StrKey::FilterEgg), labelX, textY, T().text, font_);
                drawText(searchFilter_.filterEgg ? i18n::get(StrKey::FilterYes) : i18n::get(StrKey::FilterOff),
                         valueX, textY, searchFilter_.filterEgg ? T().text : T().textDim, font_);
                break;
            case 4:
                drawText(i18n::get(StrKey::FilterAlpha), labelX, textY, T().text, font_);
                drawText(searchFilter_.filterAlpha ? i18n::get(StrKey::FilterYes) : i18n::get(StrKey::FilterOff),
                         valueX, textY, searchFilter_.filterAlpha ? T().text : T().textDim, font_);
                break;
            case 5: {
                drawText(i18n::get(StrKey::FilterGender), labelX, textY, T().text, font_);
                const char* g = i18n::get(StrKey::GenderAny).c_str();
                if (searchFilter_.gender == GenderFilter::Male)        g = i18n::get(StrKey::GenderMale).c_str();
                else if (searchFilter_.gender == GenderFilter::Female)  g = i18n::get(StrKey::GenderFemale).c_str();
                else if (searchFilter_.gender == GenderFilter::Genderless) g = i18n::get(StrKey::GenderGenderless).c_str();
                drawText(g, valueX, textY, T().text, font_);
                break;
            }
            case 6: {
                drawText(i18n::get(StrKey::FilterLevel), labelX, textY, T().text, font_);
                std::string minStr = searchFilter_.levelMin > 0 ? std::to_string(searchFilter_.levelMin) : "-";
                std::string maxStr = searchFilter_.levelMax > 0 ? std::to_string(searchFilter_.levelMax) : "-";
                SDL_Color minC = (searchFilterCursor_ == 6 && searchLevelFocus_ == 0) ? T().cursor : T().text;
                SDL_Color maxC = (searchFilterCursor_ == 6 && searchLevelFocus_ == 1) ? T().cursor : T().text;
                drawText("[" + minStr + "]", valueX, textY, minC, font_);
                drawText("-", valueX + 60, textY, T().textDim, font_);
                drawText("[" + maxStr + "]", valueX + 80, textY, maxC, font_);
                break;
            }
            case 7: {
                drawText(i18n::get(StrKey::FilterPerfectIVs), labelX, textY, T().text, font_);
                const char* iv = i18n::get(StrKey::FilterOff).c_str();
                if (searchFilter_.perfectIVs == PerfectIVFilter::AtLeastOne) iv = i18n::get(StrKey::IVOnePlus).c_str();
                else if (searchFilter_.perfectIVs == PerfectIVFilter::All6)  iv = i18n::get(StrKey::IVSix).c_str();
                drawText(iv, valueX, textY, T().text, font_);
                break;
            }
            case 8: {
                drawText(i18n::get(StrKey::FilterRibbons), labelX, textY, T().text, font_);
                const char* rf = i18n::get(StrKey::FilterOff).c_str();
                if (searchFilter_.ribbonFilter == RibbonFilter::HasRibbon) rf = i18n::get(StrKey::RibbonHasRibbon).c_str();
                else if (searchFilter_.ribbonFilter == RibbonFilter::HasMark) rf = i18n::get(StrKey::RibbonHasMark).c_str();
                else if (searchFilter_.ribbonFilter == RibbonFilter::HasAny)  rf = i18n::get(StrKey::RibbonHasAny).c_str();
                drawText(rf, valueX, textY,
                         searchFilter_.ribbonFilter != RibbonFilter::Off ? T().text : T().textDim, font_);
                break;
            }
            case 9: {
                drawText(i18n::get(StrKey::FilterMode), labelX, textY, T().text, font_);
                bool isList = (searchFilter_.mode == SearchMode::List);
                drawText(isList ? i18n::get(StrKey::ModeListOn) : i18n::get(StrKey::ModeListOff),
                         valueX, textY, isList ? T().text : T().textDim, font_);
                drawText(!isList ? i18n::get(StrKey::ModeHighlightOn) : i18n::get(StrKey::ModeHighlightOff),
                         valueX + 100, textY, !isList ? T().searchMatch : T().textDim, font_);
                break;
            }
            case 10:
                drawTextCentered(i18n::get(StrKey::FilterReset), popX + POP_W / 2, rowY + (ROW_H - 4) / 2, T().textDim, font_);
                break;
            case 11:
                drawTextCentered(i18n::get(StrKey::FilterSearch), popX + POP_W / 2, rowY + (ROW_H - 4) / 2, T().text, font_);
                break;
        }
        visualRow++;
    }

    drawTextCentered(i18n::get(StrKey::FilterFooter), popX + POP_W / 2, popY + POP_H - 18, T().textDim, fontSmall_);
}

void UI::drawSearchResultsPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    constexpr int POP_W = 900;
    constexpr int POP_H = 550;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);

    std::string title = i18n::fmt(StrKey::SearchResultsTitle, std::to_string(searchResults_.size()));
    drawTextCentered(title, popX + POP_W / 2, popY + 22, T().text, font_);

    if (searchResults_.empty()) {
        drawTextCentered(i18n::get(StrKey::NoPokemonFound), popX + POP_W / 2, popY + POP_H / 2, T().textDim, font_);
    } else {
        constexpr int ROW_H = 36;
        int listY = popY + 50;
        int listBottom = popY + POP_H - 40;
        int visibleRows = (listBottom - listY) / ROW_H;
        int listX = popX + 20;
        int listW = POP_W - 40;

        int maxScroll = std::max(0, (int)searchResults_.size() - visibleRows);
        if (searchResultScroll_ > maxScroll) searchResultScroll_ = maxScroll;

        if (searchResultScroll_ > 0)
            drawTextCentered("^", popX + POP_W / 2, listY - 12, T().arrow, fontSmall_);
        if (searchResultScroll_ + visibleRows < (int)searchResults_.size())
            drawTextCentered("v", popX + POP_W / 2, listBottom + 2, T().arrow, fontSmall_);

        for (int i = 0; i < visibleRows && (searchResultScroll_ + i) < (int)searchResults_.size(); i++) {
            int idx = searchResultScroll_ + i;
            const auto& r = searchResults_[idx];
            int rowY = listY + i * ROW_H;

            if (idx == searchResultCursor_) {
                drawRect(listX, rowY, listW, ROW_H - 4, T().menuHighlight);
                drawRectOutline(listX, rowY, listW, ROW_H - 4, T().cursor, 2);
            }

            int textY = rowY + (ROW_H - 4) / 2 - 9;
            int x = listX + 10;

            // Status badges (fixed-width area for up to three badges)
            {
                int bx = x;
                if (r.isShiny) { drawText(i18n::get(StrKey::BadgeShiny), bx, textY, T().shiny, font_); bx += 35; }
                if (r.isAlpha) { drawText(i18n::get(StrKey::BadgeAlpha), bx, textY, T().text, font_); bx += 35; }
                if (r.isEgg)   { drawText(i18n::get(StrKey::BadgeEgg), bx, textY, T().textDim, font_); }
            }
            x += 105;

            // Species name
            std::string name = r.isEgg ? i18n::get(StrKey::Egg) : r.speciesName;
            if (name.length() > 14) name = name.substr(0, 13) + ".";
            drawText(name, x, textY, r.isShiny ? T().shiny : T().text, font_);
            x += 170;

            // Level
            std::string lvlStr = r.isEgg ? i18n::get(StrKey::Egg) : i18n::get(StrKey::LvPrefix) + std::to_string(r.level);
            drawText(lvlStr, x, textY, T().textDim, font_);
            x += 70;

            // Gender
            if (r.gender == 0)
                drawText("\xe2\x99\x82", x, textY, T().genderMale, font_);
            else if (r.gender == 1)
                drawText("\xe2\x99\x80", x, textY, T().genderFemale, font_);
            x += 30;

            // Location
            std::string loc;
            if (isDualBankMode())
                loc = (r.panel == Panel::Game ? i18n::get(StrKey::LocLeft) : i18n::get(StrKey::LocRight));
            else
                loc = (r.panel == Panel::Game ? i18n::get(StrKey::LocSave) : i18n::get(StrKey::LocBank));
            loc += " " + i18n::get(StrKey::BoxLabel) + " " + std::to_string(r.box + 1) + " " + i18n::get(StrKey::SlotLabel) + " " + std::to_string(r.slot + 1);
            drawText(loc, x, textY, T().textDim, font_);
        }
    }

    std::string footer = searchResults_.empty()
        ? i18n::get(StrKey::ResultsFooterEmpty)
        : i18n::get(StrKey::ResultsFooter);
    drawTextCentered(footer, popX + POP_W / 2, popY + POP_H - 18, T().textDim, fontSmall_);
}

void UI::drawSpeciesLetterPicker() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    constexpr int POP_W = 1080;
    constexpr int POP_H = 620;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);

    drawTextCentered(i18n::get(StrKey::SelectLetter), popX + POP_W / 2, popY + 22, T().text, font_);

    constexpr int COLS = 2;
    constexpr int TOTAL_ITEMS = 27; // "-" + A-Z
    constexpr int ROW_H = 56;
    constexpr int COL_W = 500;
    constexpr int PAD = 6;
    int gridX = popX + (POP_W - COLS * COL_W) / 2;
    int gridY = popY + 50;
    int gridH = POP_H - 50 - 30;
    int visibleRows = gridH / ROW_H;
    int totalRows = (TOTAL_ITEMS + COLS - 1) / COLS;

    // Auto-scroll to keep cursor visible
    int cursorRow = speciesLetterCursor_ / COLS;
    if (cursorRow < speciesLetterScroll_)
        speciesLetterScroll_ = cursorRow;
    if (cursorRow >= speciesLetterScroll_ + visibleRows)
        speciesLetterScroll_ = cursorRow - visibleRows + 1;

    // Draw scrollbar
    if (totalRows > visibleRows) {
        int sbX = popX + POP_W - 20;
        int sbH = gridH;
        int thumbH = std::max(20, sbH * visibleRows / totalRows);
        int thumbY = gridY + (sbH - thumbH) * speciesLetterScroll_ / (totalRows - visibleRows);
        drawRect(sbX, gridY, 6, sbH, T().textDim);
        drawRect(sbX, thumbY, 6, thumbH, T().text);
    }

    for (int r = 0; r < visibleRows && (speciesLetterScroll_ + r) < totalRows; r++) {
        int row = speciesLetterScroll_ + r;
        for (int c = 0; c < COLS; c++) {
            int idx = row * COLS + c;
            if (idx >= TOTAL_ITEMS) break;

            int cellX = gridX + c * COL_W + PAD;
            int cellY = gridY + r * ROW_H + PAD;
            int cellW = COL_W - PAD * 2;
            int cellH = ROW_H - PAD * 2;

            bool hasSpecies = letterHasSpecies(idx);

            if (idx == speciesLetterCursor_) {
                drawRect(cellX, cellY, cellW, cellH, T().menuHighlight);
                drawRectOutline(cellX, cellY, cellW, cellH, T().cursor, 2);
            } else if (hasSpecies) {
                // Light background for available letters
                SDL_Color bg = T().panelBg;
                bg.r = std::min(255, bg.r + 15);
                bg.g = std::min(255, bg.g + 15);
                bg.b = std::min(255, bg.b + 15);
                drawRect(cellX, cellY, cellW, cellH, bg);
            } else {
                // Dimmed background for unavailable letters
                drawRect(cellX, cellY, cellW, cellH, T().panelBg);
            }

            std::string label = (idx == 0) ? "-" : std::string(1, 'A' + idx - 1);
            drawText(label, cellX + 20, cellY + cellH / 2 - 9,
                     hasSpecies ? T().text : T().textDim, font_);
        }
    }

    drawTextCentered(i18n::get(StrKey::ASelectBBack), popX + POP_W / 2, popY + POP_H - 18, T().textDim, fontSmall_);
}

void UI::drawSpeciesListPicker() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    constexpr int POP_W = 1080;
    constexpr int POP_H = 620;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);

    char letter = 'A' + (speciesLetterCursor_ - 1);
    std::string title = i18n::fmt(StrKey::SpeciesDashLetter, std::string(1, letter));
    drawTextCentered(title, popX + POP_W / 2, popY + 22, T().text, font_);

    if (speciesPickerList_.empty()) {
        drawTextCentered(i18n::get(StrKey::NoSpeciesFound), popX + POP_W / 2, popY + POP_H / 2, T().textDim, font_);
    } else {
        constexpr int COLS = 3;
        constexpr int ROW_H = 56;
        constexpr int PAD = 4;
        int total = static_cast<int>(speciesPickerList_.size());
        int totalRows = (total + COLS - 1) / COLS;
        int COL_W = (POP_W - 40) / COLS;
        int gridX = popX + 20;
        int gridY = popY + 50;
        int gridH = POP_H - 50 - 30;
        int visibleRows = gridH / ROW_H;

        // Auto-scroll to keep cursor visible
        int cursorRow = speciesListCursor_ / COLS;
        if (cursorRow < speciesListScroll_)
            speciesListScroll_ = cursorRow;
        if (cursorRow >= speciesListScroll_ + visibleRows)
            speciesListScroll_ = cursorRow - visibleRows + 1;

        // Draw scrollbar
        if (totalRows > visibleRows) {
            int sbX = popX + POP_W - 20;
            int sbH = gridH;
            int thumbH = std::max(20, sbH * visibleRows / totalRows);
            int thumbY = gridY + (sbH - thumbH) * speciesListScroll_ / (totalRows - visibleRows);
            drawRect(sbX, gridY, 6, sbH, T().textDim);
            drawRect(sbX, thumbY, 6, thumbH, T().text);
        }

        constexpr int SPRITE_SZ = 40;

        for (int r = 0; r < visibleRows && (speciesListScroll_ + r) < totalRows; r++) {
            int row = speciesListScroll_ + r;
            for (int c = 0; c < COLS; c++) {
                int idx = row * COLS + c;
                if (idx >= total) break;

                uint16_t specId = speciesPickerList_[idx];
                const std::string& name = SpeciesName::get(specId);

                int cellX = gridX + c * COL_W + PAD;
                int cellY = gridY + r * ROW_H + PAD;
                int cellW = COL_W - PAD * 2;
                int cellH = ROW_H - PAD * 2;

                if (idx == speciesListCursor_) {
                    drawRect(cellX, cellY, cellW, cellH, T().menuHighlight);
                    drawRectOutline(cellX, cellY, cellW, cellH, T().cursor, 2);
                } else {
                    SDL_Color bg = T().panelBg;
                    bg.r = std::min(255, bg.r + 15);
                    bg.g = std::min(255, bg.g + 15);
                    bg.b = std::min(255, bg.b + 15);
                    drawRect(cellX, cellY, cellW, cellH, bg);
                }

                // Draw sprite (aspect-ratio preserved)
                SDL_Texture* spr = getSprite(specId);
                if (spr) {
                    int texW, texH;
                    SDL_QueryTexture(spr, nullptr, nullptr, &texW, &texH);
                    int dstW = SPRITE_SZ, dstH = SPRITE_SZ;
                    if (texW > 0 && texH > 0 && (texW != texH)) {
                        float scale = std::min(static_cast<float>(SPRITE_SZ) / texW,
                                               static_cast<float>(SPRITE_SZ) / texH);
                        dstW = static_cast<int>(texW * scale);
                        dstH = static_cast<int>(texH * scale);
                    }
                    int sprX = cellX + 6 + (SPRITE_SZ - dstW) / 2;
                    int sprY = cellY + (cellH - dstH) / 2;
                    SDL_Rect dst = { sprX, sprY, dstW, dstH };
                    SDL_RenderCopy(renderer_, spr, nullptr, &dst);
                }

                // Draw name (truncated if needed)
                std::string displayName = name;
                if (displayName.length() > 14) displayName = displayName.substr(0, 13) + ".";
                drawText(displayName, cellX + 6 + SPRITE_SZ + 6, cellY + cellH / 2 - 9, T().text, font_);
            }
        }
    }

    drawTextCentered(i18n::get(StrKey::ASelectBBack), popX + POP_W / 2, popY + POP_H - 18, T().textDim, fontSmall_);
}

void UI::drawWondercardListPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    constexpr int POP_W = 900;
    constexpr int POP_H = 550;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);

    std::string title = i18n::fmt(StrKey::WondercardsTitle, std::to_string(wcList_.size()));
    drawTextCentered(title, popX + POP_W / 2, popY + 22, T().text, font_);

    if (wcList_.empty()) {
        drawTextCentered(i18n::get(StrKey::NoWCFound), popX + POP_W / 2, popY + POP_H / 2 - 20, T().textDim, font_);
        std::string hint = i18n::fmt(StrKey::PlaceFilesIn, std::string(gameInfo(selectedGame_).wcExtensionHint));
        drawTextCentered(hint, popX + POP_W / 2, popY + POP_H / 2 + 10, T().textDim, fontSmall_);
        std::string path = "sdmc:/switch/pkHouse/wondercards/" + std::string(bankFolderNameOf(selectedGame_)) + "/";
        drawTextCentered(path, popX + POP_W / 2, popY + POP_H / 2 + 30, T().textDim, fontSmall_);
    } else {
        constexpr int ROW_H = 36;
        int listY = popY + 50;
        int listBottom = popY + POP_H - 40;
        int visibleRows = (listBottom - listY) / ROW_H;
        int listX = popX + 20;
        int listW = POP_W - 40;

        int maxScroll = std::max(0, (int)wcList_.size() - visibleRows);
        if (wcListScroll_ > maxScroll) wcListScroll_ = maxScroll;

        if (wcListScroll_ > 0)
            drawTextCentered("^", popX + POP_W / 2, listY - 12, T().arrow, fontSmall_);
        if (wcListScroll_ + visibleRows < (int)wcList_.size())
            drawTextCentered("v", popX + POP_W / 2, listBottom + 2, T().arrow, fontSmall_);

        for (int i = 0; i < visibleRows && (wcListScroll_ + i) < (int)wcList_.size(); i++) {
            int idx = wcListScroll_ + i;
            const auto& wc = wcList_[idx];
            int rowY = listY + i * ROW_H;

            if (idx == wcListCursor_) {
                drawRect(listX, rowY, listW, ROW_H - 4, T().menuHighlight);
                drawRectOutline(listX, rowY, listW, ROW_H - 4, T().cursor, 2);
            }

            int textY = rowY + (ROW_H - 4) / 2 - 9;
            int x = listX + 10;

            if (!wc.valid) {
                // Invalid entry — show filename and marker
                drawText(i18n::get(StrKey::BadgeInvalid), x, textY, T().genderFemale, font_);
                x += 110;
                std::string fn = wc.filename;
                if (fn.length() > 40) fn = fn.substr(0, 39) + ".";
                drawText(fn, x, textY, T().textDim, font_);
            } else {
                // Shiny indicator
                if (wc.isShiny) {
                    drawText(i18n::get(StrKey::BadgeShiny), x, textY, T().shiny, font_);
                }
                x += 40;

                // Sprite (shiny variant if wondercard is shiny)
                SDL_Texture* sprite = nullptr;
                if (wc.isShiny) {
                    sprite = getShinySprite(wc.species);
                    if (!sprite) sprite = getSprite(wc.species);
                } else {
                    sprite = getSprite(wc.species);
                }
                if (sprite) {
                    int tw = 0, th = 0;
                    SDL_QueryTexture(sprite, nullptr, nullptr, &tw, &th);
                    int maxH = ROW_H - 6;
                    float scale = std::min(static_cast<float>(maxH) / tw,
                                           static_cast<float>(maxH) / th);
                    int dw = static_cast<int>(tw * scale);
                    int dh = static_cast<int>(th * scale);
                    SDL_Rect dst = {x + (maxH - dw) / 2, rowY + 2 + (maxH - dh) / 2, dw, dh};
                    SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
                }
                x += ROW_H;

                // Species name
                const std::string& name = SpeciesName::get(wc.species);
                std::string displayName = name;
                if (displayName.length() > 14) displayName = displayName.substr(0, 13) + ".";
                drawText(displayName, x, textY, wc.isShiny ? T().shiny : T().text, font_);
                x += 170;

                // Level
                drawText(i18n::get(StrKey::LvPrefix) + std::to_string(wc.level), x, textY, T().textDim, font_);
                x += 70;

                // Player OT tag
                if (!wc.hasOT) {
                    drawText(i18n::get(StrKey::PlayerOTTag), x, textY, T().genderFemale, font_);
                }
                x += 120;

                // Filename (truncate to fit)
                int maxW = listX + listW - x - 5;
                std::string fn = wc.filename;
                while (fn.size() > 4) {
                    int tw = getTextEntry(fn, fontSmall_, T().textDim).w;
                    if (tw <= maxW) break;
                    fn = fn.substr(0, fn.size() - 5) + "..";
                }
                drawText(fn, x, textY, T().textDim, fontSmall_);
            }
        }
    }

    std::string footer = wcList_.empty()
        ? i18n::get(StrKey::BClose)
        : i18n::get(StrKey::WCFooter);
    drawTextCentered(footer, popX + POP_W / 2, popY + POP_H - 18, T().textDim, fontSmall_);
}

void UI::drawAboutPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlayDark);

    constexpr int POP_W = 700;
    constexpr int POP_H = 510;
    int px = (SCREEN_W - POP_W) / 2;
    int py = (SCREEN_H - POP_H) / 2;

    drawRect(px, py, POP_W, POP_H, T().panelBg);
    drawRectOutline(px, py, POP_W, POP_H, T().popupBorder, 2);

    int cx = px + POP_W / 2;
    int y = py + 25;

    // Title
    drawTextCentered(i18n::get(StrKey::AboutTitle), cx, y, T().shiny, fontLarge_);
    y += 38;

    // Version / author
    drawTextCentered("v" APP_VERSION " - Developed by " APP_AUTHOR, cx, y, T().textDim, fontSmall_);
    y += 22;
    drawTextCentered("github.com/Insektaure", cx, y, T().textDim, fontSmall_);
    y += 30;

    // Divider
    SDL_SetRenderDrawColor(renderer_, T().popupBorder.r, T().popupBorder.g, T().popupBorder.b, T().popupBorder.a);
    SDL_RenderDrawLine(renderer_, px + 30, y, px + POP_W - 30, y);
    y += 20;

    // Description
    drawTextCentered(i18n::get(StrKey::AboutDesc1), cx, y, T().text, font_);
    y += 28;
    drawTextCentered(i18n::get(StrKey::AboutDesc2), cx, y, T().text, font_);
    y += 28;
    drawTextCentered(i18n::get(StrKey::SupportedGames), cx, y, T().selected, font_);
    y += 24;
    drawTextCentered(i18n::get(StrKey::SupportedLGPE), cx, y, T().textDim, fontSmall_);
    y += 20;
    drawTextCentered(i18n::get(StrKey::SupportedSwSh), cx, y, T().textDim, fontSmall_);
    y += 20;
    drawTextCentered(i18n::get(StrKey::SupportedSVZA), cx, y, T().textDim, fontSmall_);
    y += 20;
    drawTextCentered(i18n::get(StrKey::SupportedFRLG), cx, y, T().textDim, fontSmall_);
    y += 30;

    // Divider
    SDL_SetRenderDrawColor(renderer_, T().popupBorder.r, T().popupBorder.g, T().popupBorder.b, T().popupBorder.a);
    SDL_RenderDrawLine(renderer_, px + 30, y, px + POP_W - 30, y);
    y += 20;

    // Credits
    drawTextCentered(i18n::get(StrKey::CreditPKHeX), cx, y, T().creditsText, fontSmall_);
    y += 20;
    drawTextCentered(i18n::get(StrKey::CreditJKSV), cx, y, T().creditsText, fontSmall_);
    y += 35;

    // Controls
    drawTextCentered(i18n::get(StrKey::Controls), cx, y, T().selected, font_);
    y += 28;
    drawText(i18n::get(StrKey::ControlsLine1), px + 50, y, T().textDim, fontSmall_);
    y += 20;
    drawText(i18n::get(StrKey::ControlsLine2), px + 50, y, T().textDim, fontSmall_);

    // Footer
    drawTextCentered(i18n::get(StrKey::PressMinusBClose), cx, py + POP_H - 22, T().textDim, fontSmall_);
}

// --- Box overview (ZL/ZR), UI 2.0 -------------------------------------------------
//
// Laid out after external/UI_2.0 "Box overview (ZL ZR)": the save and the bank
// as two tabs, every box of the one on show as a card with a dot per slot, and
// the sprite preview of the highlighted box on top.

int UI::panelBoxCount(Panel panel) const {
    if (panel == Panel::Bank) return bank_.boxCount();
    return isDualBankMode() ? bankLeft_.boxCount() : save_.boxCount();
}

std::string UI::panelBoxName(Panel panel, int box) const {
    if (panel == Panel::Bank) return bank_.getBoxName(box);
    return isDualBankMode() ? bankLeft_.getBoxName(box) : save_.getBoxName(box);
}

SDL_Rect UI::boxCardRect(int idx, int totalBoxes) const {
    const int rows = std::max(1, (totalBoxes + BV_COLS - 1) / BV_COLS);
    const int cardH = std::min(BV_CARD_H, (BV_BOTTOM - BV_TOP - (rows - 1) * BV_ROW_GAP) / rows);
    const int col = idx % BV_COLS, row = idx / BV_COLS;
    // Columns spread edge to edge over the same width as the info strip.
    const int x = INFO_X + col * (INFO_W - BV_CARD_W) / (BV_COLS - 1);
    const int y = BV_TOP + row * (cardH + BV_ROW_GAP);
    return {x, y, BV_CARD_W, cardH};
}

void UI::drawBoxViewTabs() {
    constexpr int X = 32, Y = 11, H = 50, PAD = 5, TAB_H = H - 2 * PAD;
    constexpr int CHIP_W = 28, CHIP_H = 20;
    TTF_Font* fTab  = uiFont(15, true);
    TTF_Font* fChip = uiFont(12, true);

    const std::string dot = " \xc2\xb7 ";
    std::string labels[2];
    if (isDualBankMode()) {
        labels[0] = i18n::get(StrKey::TabBank) + dot
                  + (leftBankName_.empty() ? i18n::get(StrKey::NoBankLoaded) : leftBankName_);
    } else {
        labels[0] = i18n::get(StrKey::TabSave) + dot + gameDisplayNameOf(selectedGame_);
    }
    labels[1] = i18n::get(StrKey::TabBank) + dot + activeBankName_;
    for (auto& l : labels) l = fitText(l, fTab, 260);

    // Tab widths: 12 | chip | 10 | label | 16, mirrored for the right-hand tab.
    int tabW[2];
    for (int i = 0; i < 2; i++)
        tabW[i] = 12 + CHIP_W + 10 + textWidth(labels[i], fTab) + 16;
    const int W = PAD + tabW[0] + PAD + tabW[1] + PAD;

    fillRounded(X, Y, W, H, 12, T().panelBg);
    strokeRounded(X, Y, W, H, 12, 1, T().panelBorder);

    int tx = X + PAD;
    for (int i = 0; i < 2; i++) {
        const bool active = (i == 0) == (boxViewPanel_ == Panel::Game);
        const int ty = Y + PAD, tcy = ty + TAB_H / 2;
        if (active)
            fillRounded(tx, ty, tabW[i], TAB_H, 9, T().accent);

        const SDL_Color labelColor = active ? T().keyCapText : T().textDim;
        const SDL_Color chipBg     = active ? T().bg : T().keyCap;
        const SDL_Color chipText   = active ? T().text : T().keyCapText;
        const char* chip = i == 0 ? "ZL" : "ZR";

        // The chip sits on the outer side of each tab, like the triggers.
        const int chipX = i == 0 ? tx + 12 : tx + tabW[i] - 12 - CHIP_W;
        const int labelX = i == 0 ? chipX + CHIP_W + 10 : tx + 16;
        fillRounded(chipX, tcy - CHIP_H / 2, CHIP_W, CHIP_H, 5, chipBg);
        drawTextCentered(chip, chipX + CHIP_W / 2, tcy, chipText, fChip);
        drawText(labels[i], labelX, tcy - TTF_FontHeight(fTab) / 2, labelColor, fTab);
        tx += tabW[i] + PAD;
    }
}

void UI::drawBoxViewStats() {
    int total = 0, shiny = 0, free = 0;
    const int boxes = panelBoxCount(boxViewPanel_);
    for (int b = 0; b < boxes; b++) {
        for (const auto& sd : getSlotDisplays(boxViewPanel_, b)) {
            if (sd.empty) { free++; continue; }
            total++;
            if (sd.shiny && !sd.egg) shiny++;
        }
    }

    // Right to left: "792 free slots", "* 8 shiny", "168 Pokemon".
    TTF_Font* fNum  = uiFont(15, true);
    TTF_Font* fWord = uiFont(15);
    const int baseline = 42;
    int x = SCREEN_W - 32;
    auto segment = [&](int value, const std::string& word, bool star) {
        const std::string num = std::to_string(value);
        const std::string w = " " + word;
        x -= textWidth(w, fWord);
        drawText(w, x, baseline - TTF_FontAscent(fWord), T().textDim, fWord);
        x -= textWidth(num, fNum);
        drawText(num, x, baseline - TTF_FontAscent(fNum), T().text, fNum);
        if (star && iconShiny_) {
            constexpr int ICON = 13;
            x -= ICON + 6;
            SDL_SetTextureColorMod(iconShiny_, T().accent.r, T().accent.g, T().accent.b);
            SDL_Rect dst = {x, baseline - ICON, ICON, ICON};
            SDL_RenderCopy(renderer_, iconShiny_, nullptr, &dst);
            SDL_SetTextureColorMod(iconShiny_, 255, 255, 255);
        }
        x -= 22;
    };
    segment(free,  i18n::get(StrKey::OvFreeSlots), false);
    segment(shiny, i18n::get(StrKey::OvShiny),     true);
    segment(total, i18n::get(StrKey::OvPokemon),   false);
}

int UI::drawBoxViewLegend(bool measureOnly) {
    struct Item { SDL_Color color; const char* key; };
    const Item items[] = {
        {T().miniDotFull, StrKey::LegendPokemon},
        {T().accent,      StrKey::LegendShiny},
        {T().alphaMark,   StrKey::LegendAlpha},
        {T().eggMark,     StrKey::Egg},
    };
    TTF_Font* f = uiFont(13);
    constexpr int DOT = 9;
    const int cy = FOOTER_Y + FOOTER_H / 2;

    int w = 0;
    for (const auto& it : items)
        w += (w ? 16 : 0) + DOT + 6 + textWidth(i18n::get(it.key), f);
    if (measureOnly) return w;

    int x = SCREEN_W - 32 - w;
    for (const auto& it : items) {
        const std::string& label = i18n::get(it.key);
        fillRounded(x, cy - DOT / 2, DOT, DOT, 2, it.color);
        x += DOT + 6;
        drawText(label, x, cy - TTF_FontHeight(f) / 2, T().textDim, f);
        x += textWidth(label, f) + 16;
    }
    return w;
}

void UI::drawBoxCard(int idx, const SDL_Rect& r, bool isCursor) {
    const auto& disp = getSlotDisplays(boxViewPanel_, idx);
    const int activeBox = (boxViewPanel_ == Panel::Game) ? gameBox_ : bankBox_;

    bool hasMatch = false;
    int filled = 0;
    for (int s = 0; s < (int)disp.size(); s++) {
        if (!disp[s].empty) filled++;
        if (searchHighlightActive_ && !hasMatch)
            hasMatch = isSearchMatch(boxViewPanel_, idx, s);
    }

    if (isCursor)
        strokeRounded(r.x - 6, r.y - 6, r.w + 12, r.h + 12, 17, 3, T().accent);
    fillRounded(r.x, r.y, r.w, r.h, 12, isCursor ? T().slotFull : T().panelBg);
    strokeRounded(r.x, r.y, r.w, r.h, 12, 1, hasMatch ? T().searchMatch : T().panelBorder);
    if (hasMatch)
        strokeRounded(r.x, r.y, r.w, r.h, 12, 2, T().searchMatch);

    // Name and fill. The box the panel is showing right now keeps its name in
    // the accent colour, so it is easy to find your way back to it.
    TTF_Font* fName  = uiFont(15, true);
    TTF_Font* fCount = uiFont(13);
    const std::string count = std::to_string(filled) + "/" + std::to_string((int)disp.size());
    const int countW = textWidth(count, fCount);
    const int baseline = r.y + 27;
    drawText(count, r.x + r.w - 13 - countW, baselineTop(fCount, baseline), T().textMuted, fCount);
    const std::string name = fitText(panelBoxName(boxViewPanel_, idx), fName, r.w - 26 - countW - 6);
    drawText(name, r.x + 13, baselineTop(fName, baseline),
             idx == activeBox ? T().accent : T().text, fName);

    // One dot per slot. The pitch shrinks with the card on 40-box games.
    const int cols = gridCols(), rows = 5;
    const int gridTop = r.y + 42;
    const int pitch = std::max(5, std::min(BV_DOT_PITCH, (r.y + r.h - 12 - gridTop) / rows));
    const int dot = pitch - 4;
    const int gridW = cols * pitch - (pitch - dot);
    const int gx = r.x + (r.w - gridW) / 2;
    for (int s = 0; s < rows * cols && s < (int)disp.size(); s++) {
        const auto& sd = disp[s];
        SDL_Color c = T().miniDotEmpty;
        if (!sd.empty) {
            if (sd.egg)        c = T().eggMark;
            else if (sd.shiny) c = T().accent;
            else if (sd.alpha) c = T().alphaMark;
            else               c = T().miniDotFull;
            if (searchHighlightActive_ && isSearchMatch(boxViewPanel_, idx, s))
                c = T().searchMatch;
        }
        fillRounded(gx + (s % cols) * pitch, gridTop + (s / cols) * pitch, dot, dot, 2, c);
    }
}

void UI::drawBoxViewOverlay() {
    // Full screen: nothing of the box view shows through.
    SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
    SDL_RenderClear(renderer_);
    drawRect(0, 0, SCREEN_W, ACCENT_RULE_H, T().accent);

    drawBoxViewTabs();
    drawBoxViewStats();

    const int totalBoxes = panelBoxCount(boxViewPanel_);
    SDL_Rect cursorCard{0, 0, 0, 0};
    for (int i = 0; i < totalBoxes; i++) {
        const SDL_Rect r = boxCardRect(i, totalBoxes);
        const bool isCursor = (i == boxViewCursor_);
        if (isCursor) cursorCard = r;
        drawBoxCard(i, r, isCursor);
    }

    // Same keys as before the redesign, plus ZL/ZR to flip between the tabs.
    const bool canRename = isDualBankMode() || (boxViewPanel_ == Panel::Bank);
    ButtonHint hints[5];
    int n = 0;
    hints[n++] = {"A", StrKey::HintGoToBox};
    if (canRename)
        hints[n++] = {"Y", StrKey::HintRename};
    hints[n++] = {"B", StrKey::HintCancel};
    hints[n++] = {HINT_DPAD, StrKey::HintNavigate};
    hints[n++] = {"ZL ZR", isDualBankMode() ? StrKey::HintLeftRight : StrKey::HintSaveBank};
    drawFooterBar(hints, n, std::string(), drawBoxViewLegend(true));
    drawBoxViewLegend(false);

    // Sprite preview of the highlighted box, last so it sits on top.
    if (totalBoxes > 0)
        drawBoxPreview(boxViewCursor_, cursorCard);
}

void UI::drawBoxPreview(int boxIdx, const SDL_Rect& card) {
    const int cols = gridCols();
    const int rows = 5;

    const int innerW = cols * BV_MINI_CELL + (cols - 1) * BV_MINI_PAD;
    const int innerH = rows * BV_MINI_CELL + (rows - 1) * BV_MINI_PAD;
    const int w = innerW + 2 * BV_PREVIEW_PAD;
    const int h = innerH + 2 * BV_PREVIEW_PAD + BV_PREVIEW_HDR;

    // Below the card, or above it when that would run into the footer; kept
    // inside the card area horizontally.
    int x = card.x + card.w / 2 - w / 2;
    int y = card.y + card.h + 10;
    if (y + h > FOOTER_Y - 4)
        y = card.y - h - 10;
    if (y < ACCENT_RULE_H + 4)
        y = ACCENT_RULE_H + 4;
    x = std::max(INFO_X, std::min(x, INFO_X + INFO_W - w));

    // Shadow, then the panel.
    fillRounded(x + 3, y + 5, w, h, 12, SDL_Color{0, 0, 0, 90});
    fillRounded(x, y, w, h, 12, T().buttonBg);
    strokeRounded(x, y, w, h, 12, 1, T().cellBorder);

    TTF_Font* fName = uiFont(15, true);
    drawTextCentered(fitText(panelBoxName(boxViewPanel_, boxIdx), fName, w - 20),
                     x + w / 2, y + BV_PREVIEW_PAD + BV_PREVIEW_HDR / 2 - 4, T().text, fName);

    const int gx = x + BV_PREVIEW_PAD;
    const int gy = y + BV_PREVIEW_PAD + BV_PREVIEW_HDR;
    const auto& disp = getSlotDisplays(boxViewPanel_, boxIdx);
    for (int s = 0; s < rows * cols && s < (int)disp.size(); s++) {
        const auto& sd = disp[s];
        const int sx = gx + (s % cols) * (BV_MINI_CELL + BV_MINI_PAD);
        const int sy = gy + (s / cols) * (BV_MINI_CELL + BV_MINI_PAD);
        if (sd.empty) {
            fillRounded(sx, sy, BV_MINI_CELL, BV_MINI_CELL, 6, T().slotEmpty);
            continue;
        }
        fillRounded(sx, sy, BV_MINI_CELL, BV_MINI_CELL, 6, T().slotFull);
        drawSpriteFit(spriteFor(sd.species, sd.form, sd.shiny, sd.egg),
                      sx + BV_MINI_CELL / 2, sy + BV_MINI_CELL / 2, BV_MINI_SPRITE);

        if (searchHighlightActive_) {
            if (isSearchMatch(boxViewPanel_, boxIdx, s))
                strokeRounded(sx, sy, BV_MINI_CELL, BV_MINI_CELL, 6, 2, T().searchMatch);
            else
                fillRounded(sx, sy, BV_MINI_CELL, BV_MINI_CELL, 6, T().searchDim);
        }
    }
}

void UI::drawHeldOverlay() {
    if (!holding_)
        return;

    const Pokemon& pkm = heldMulti_.empty() ? heldPkm_ : heldMulti_[0];
    if (pkm.species() == 0)
        return;
    SDL_Texture* sprite = spriteFor(pkm.species(), pkm.form(), pkm.isShiny(), pkm.isEgg());
    if (!sprite)
        return;

    // Lifted off the cursor cell, a little down and right, semi-transparent.
    const SDL_Rect cell = slotRect(cursor_.panel, cursor_.col, cursor_.row);
    constexpr int DRAG_OFS = 8;
    drawSpriteFit(sprite, cell.x + CELL_W / 2 + DRAG_OFS, cell.y + CELL_H / 2 + DRAG_OFS,
                  SPRITE_SIZE, 180);

    // Multi-hold: how many are in hand.
    if (heldMulti_.size() > 1) {
        const int cx = cell.x + CELL_W - 4, cy = cell.y + CELL_H - 4;
        fillDisc(cx, cy, 12, T().selected);
        drawTextCentered(std::to_string(heldMulti_.size()), cx, cy, T().textOnBadge,
                         uiFont(14, true));
    }
}
