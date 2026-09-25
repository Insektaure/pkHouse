#include "ui.h"
#include <algorithm>
#include <cmath>
#include <vector>

// --- Fonts ---------------------------------------------------------------------

TTF_Font* UI::uiFont(int size, bool bold) {
    const int key = size * 2 + (bold ? 1 : 0);
    auto it = uiFonts_.find(key);
    if (it != uiFonts_.end())
        return it->second;

    TTF_Font* f = nullptr;
    if (fontData_)
        f = TTF_OpenFontRW(SDL_RWFromMem(fontData_, static_cast<int>(fontDataSize_)), 1, size);
    if (!f)
        f = TTF_OpenFont("romfs:/fonts/default.ttf", size);
    if (f && bold)
        TTF_SetFontStyle(f, TTF_STYLE_BOLD);

    // A size that will not open falls back to the regular small font rather
    // than leaving every caller to check for null.
    if (!f)
        return fontSmall_;
    uiFonts_[key] = f;
    return f;
}

void UI::closeUiFonts() {
    for (auto& [key, f] : uiFonts_)
        if (f) TTF_CloseFont(f);
    uiFonts_.clear();
}

int UI::textWidth(const std::string& text, TTF_Font* f) {
    if (!f || text.empty()) return 0;
    int w = 0, h = 0;
    TTF_SizeUTF8(f, text.c_str(), &w, &h);
    return w;
}

namespace {

// Byte length of the UTF-8 sequence starting with `lead`.
int utf8Len(unsigned char lead) {
    if (lead < 0x80)           return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

} // anonymous namespace

std::string UI::fitText(const std::string& text, TTF_Font* f, int maxW) {
    if (textWidth(text, f) <= maxW) return text;
    // Code point boundaries, so a cut never lands inside a multi-byte glyph.
    std::vector<size_t> ends;
    for (size_t i = 0; i < text.size(); ) {
        i += utf8Len(static_cast<unsigned char>(text[i]));
        ends.push_back(std::min(i, text.size()));
    }
    for (int n = static_cast<int>(ends.size()) - 1; n > 0; n--) {
        std::string cut = text.substr(0, ends[n - 1]) + "...";
        if (textWidth(cut, f) <= maxW) return cut;
    }
    return "...";
}

int UI::measureTextTracked(const std::string& text, TTF_Font* f, int tracking) {
    int w = 0, n = 0;
    for (size_t i = 0; i < text.size(); ) {
        const int len = utf8Len(static_cast<unsigned char>(text[i]));
        w += textWidth(text.substr(i, len), f);
        i += len;
        n++;
    }
    return n > 1 ? w + tracking * (n - 1) : w;
}

int UI::drawTextTracked(const std::string& text, int x, int y, SDL_Color color,
                        TTF_Font* f, int tracking) {
    const int startX = x;
    for (size_t i = 0; i < text.size(); ) {
        const int len = utf8Len(static_cast<unsigned char>(text[i]));
        const std::string cp = text.substr(i, len);
        drawText(cp, x, y, color, f);
        x += textWidth(cp, f);
        i += len;
        if (i < text.size()) x += tracking;
    }
    return x - startX;
}

// --- Circular icons ------------------------------------------------------------

// Blits a square icon as a disc. SDL has no circular clip, so the square is
// masked by painting everything outside the circle back in the known background
// colour, which works because every place this is used sits on a flat opaque
// fill. The type icons are full-bleed squares whose glyphs all stay inside 88%
// of the radius, so nothing meaningful is cropped.
//
// Pixels straddling the boundary are painted with partial alpha in proportion
// to how much of them falls outside, which gives a smooth edge instead of a
// stepped one. Coverage is measured from the distance to the centre rather than
// per scanline, so the top and bottom of the disc are as clean as the sides.
// The per-pixel loop is fine here: this runs once per card, not per frame.
void blitDisc(SDL_Renderer* r, SDL_Texture* tex, int cx, int cy, int rad, SDL_Color bg) {
    if (!tex || rad <= 0) return;

    SDL_Rect dst = {cx - rad, cy - rad, rad * 2, rad * 2};
    SDL_RenderCopy(r, tex, nullptr, &dst);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int py = cy - rad; py < cy + rad; py++) {
        const double dy = py + 0.5 - cy;
        for (int px = cx - rad; px < cx + rad; px++) {
            const double dx = px + 0.5 - cx;
            const double dist = std::sqrt(dx * dx + dy * dy);

            // 1 = wholly inside the disc, 0 = wholly outside, between = the
            // fraction of the pixel the disc covers.
            const double covered = rad + 0.5 - dist;
            if (covered >= 1.0) continue;

            const int alpha = (covered <= 0.0)
                ? 255
                : static_cast<int>((1.0 - covered) * 255.0 + 0.5);
            SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, static_cast<Uint8>(alpha));
            SDL_RenderDrawPoint(r, px, py);
        }
    }
}

// --- Rounded shapes ------------------------------------------------------------
//
// SDL2 has no rounded rectangles. Each corner is a small white texture holding
// the top-left quarter of a disc (or of a ring, for outlines), with its edge
// anti-aliased by supersampling; the four corners are that texture flipped, and
// the straight parts are plain rects. Colour comes from the texture's colour and
// alpha mods, so one texture serves every colour.

SDL_Texture* UI::cornerTexture(int radius, int stroke) {
    const uint32_t key = uint32_t(radius) | (uint32_t(stroke) << 16);
    auto it = cornerCache_.find(key);
    if (it != cornerCache_.end())
        return it->second;

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, radius, radius, 32,
                                                       SDL_PIXELFORMAT_RGBA32);
    if (!surf) {
        cornerCache_[key] = nullptr;
        return nullptr;
    }

    constexpr int SS = 4;  // 4x4 samples per pixel
    const double outer = radius;
    const double inner = stroke > 0 ? radius - stroke : -1.0;
    Uint32* px = static_cast<Uint32*>(surf->pixels);
    const int pitch = surf->pitch / 4;
    for (int y = 0; y < radius; y++) {
        for (int x = 0; x < radius; x++) {
            int hits = 0;
            for (int sy = 0; sy < SS; sy++) {
                for (int sx = 0; sx < SS; sx++) {
                    // Distance from the corner's centre of curvature, which is
                    // the bottom-right of this top-left quarter.
                    const double dx = radius - (x + (sx + 0.5) / SS);
                    const double dy = radius - (y + (sy + 0.5) / SS);
                    const double d = std::sqrt(dx * dx + dy * dy);
                    if (d <= outer && d > inner) hits++;
                }
            }
            const Uint8 a = static_cast<Uint8>(hits * 255 / (SS * SS));
            px[y * pitch + x] = SDL_MapRGBA(surf->format, 255, 255, 255, a);
        }
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_FreeSurface(surf);
    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    cornerCache_[key] = tex;
    return tex;
}

void UI::freeShapeCache() {
    for (auto& [key, tex] : cornerCache_)
        if (tex) SDL_DestroyTexture(tex);
    cornerCache_.clear();
}

namespace {

void drawCorners(SDL_Renderer* r, SDL_Texture* tex, int x, int y, int w, int h,
                 int radius, SDL_Color c) {
    SDL_SetTextureColorMod(tex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(tex, c.a);
    const SDL_Rect tl = {x, y, radius, radius};
    const SDL_Rect tr = {x + w - radius, y, radius, radius};
    const SDL_Rect bl = {x, y + h - radius, radius, radius};
    const SDL_Rect br = {x + w - radius, y + h - radius, radius, radius};
    SDL_RenderCopyEx(r, tex, nullptr, &tl, 0, nullptr, SDL_FLIP_NONE);
    SDL_RenderCopyEx(r, tex, nullptr, &tr, 0, nullptr, SDL_FLIP_HORIZONTAL);
    SDL_RenderCopyEx(r, tex, nullptr, &bl, 0, nullptr, SDL_FLIP_VERTICAL);
    SDL_RenderCopyEx(r, tex, nullptr, &br, 0, nullptr,
                     static_cast<SDL_RendererFlip>(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL));
}

} // anonymous namespace

void UI::fillRounded(int x, int y, int w, int h, int radius, SDL_Color c) {
    if (w <= 0 || h <= 0) return;
    radius = std::min({radius, w / 2, h / 2});
    SDL_Texture* tex = radius > 0 ? cornerTexture(radius, 0) : nullptr;
    if (!tex) {
        drawRect(x, y, w, h, c);
        return;
    }
    drawCorners(renderer_, tex, x, y, w, h, radius, c);
    drawRect(x + radius, y, w - 2 * radius, h, c);
    drawRect(x, y + radius, radius, h - 2 * radius, c);
    drawRect(x + w - radius, y + radius, radius, h - 2 * radius, c);
}

void UI::strokeRounded(int x, int y, int w, int h, int radius, int stroke, SDL_Color c) {
    if (w <= 0 || h <= 0 || stroke <= 0) return;
    radius = std::min({radius, w / 2, h / 2});
    SDL_Texture* tex = radius > 0 ? cornerTexture(radius, stroke) : nullptr;
    if (!tex) {
        drawRectOutline(x, y, w, h, c, stroke);
        return;
    }
    drawCorners(renderer_, tex, x, y, w, h, radius, c);
    drawRect(x + radius, y, w - 2 * radius, stroke, c);
    drawRect(x + radius, y + h - stroke, w - 2 * radius, stroke, c);
    drawRect(x, y + radius, stroke, h - 2 * radius, c);
    drawRect(x + w - stroke, y + radius, stroke, h - 2 * radius, c);
}

void UI::dashRounded(int x, int y, int w, int h, int radius, SDL_Color c,
                       int dash, int gap) {
    if (w <= 0 || h <= 0) return;
    radius = std::min({radius, w / 2, h / 2});
    if (SDL_Texture* tex = radius > 0 ? cornerTexture(radius, 1) : nullptr)
        drawCorners(renderer_, tex, x, y, w, h, radius, c);

    // Dashes laid from the middle of each edge outwards, so both ends of an
    // edge meet its corners the same way.
    auto hLine = [&](int yy) {
        const int x0 = x + radius, x1 = x + w - radius;
        const int period = dash + gap;
        const int len = x1 - x0;
        const int offset = (len % period) / 2;
        for (int px = x0 + offset; px < x1; px += period)
            drawRect(px, yy, std::min(dash, x1 - px), 1, c);
    };
    auto vLine = [&](int xx) {
        const int y0 = y + radius, y1 = y + h - radius;
        const int period = dash + gap;
        const int len = y1 - y0;
        const int offset = (len % period) / 2;
        for (int py = y0 + offset; py < y1; py += period)
            drawRect(xx, py, 1, std::min(dash, y1 - py), c);
    };
    hLine(y);
    hLine(y + h - 1);
    vLine(x);
    vLine(x + w - 1);
}

void UI::fillDisc(int cx, int cy, int radius, SDL_Color c) {
    if (radius <= 0) return;
    fillRounded(cx - radius, cy - radius, radius * 2, radius * 2, radius, c);
}

void UI::fillArrow(float cx, float cy, float size, ArrowDir dir, SDL_Color c) {
    const float h = size / 2.0f;
    SDL_FPoint p[3];
    switch (dir) {
        case ArrowDir::Left:  p[0] = {cx - h, cy}; p[1] = {cx + h, cy - h}; p[2] = {cx + h, cy + h}; break;
        case ArrowDir::Right: p[0] = {cx + h, cy}; p[1] = {cx - h, cy - h}; p[2] = {cx - h, cy + h}; break;
        case ArrowDir::Up:    p[0] = {cx, cy - h}; p[1] = {cx - h, cy + h}; p[2] = {cx + h, cy + h}; break;
        case ArrowDir::Down:  p[0] = {cx, cy + h}; p[1] = {cx - h, cy - h}; p[2] = {cx + h, cy - h}; break;
    }
    SDL_Vertex v[3];
    for (int i = 0; i < 3; i++)
        v[i] = {p[i], c, {0, 0}};
    SDL_RenderGeometry(renderer_, nullptr, v, 3, nullptr, 0);
}
