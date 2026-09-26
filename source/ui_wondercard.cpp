// Wondercards (UI 2.0).
//
// Everything the old list showed is here: shiny, sprite, species, level, the
// "takes your OT" marker, invalid files and their filename, the file itself,
// and the empty state with where to put the files. The card title and region
// are read from the filename ("0001 LGPE - PokeCenter Chansey (JPN).wb7full");
// a name that does not follow that pattern shows as it is. Injection itself
// (injectWondercard) is unchanged.

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
constexpr int WC_W = 1120, WC_H = 620;
constexpr int WC_HEAD = 68, WC_FOOT = 52;
constexpr int WC_LIST_W = 680;
constexpr int WC_ROW_H = 52, WC_PITCH = 58;
constexpr int WC_PANE_W = 356;
constexpr int WC_EMPTY_W = 760, WC_EMPTY_H = 380;

struct WcName {
    std::string title;
    std::string region;   // "JPN", or empty
};

// Title and region out of the filename, both optional.
WcName parseWcFilename(const std::string& filename) {
    std::string stem = filename;
    const size_t dot = stem.rfind('.');
    if (dot != std::string::npos && dot > 0) stem.resize(dot);

    WcName n;
    // A trailing "(JPN)": short, capitals, digits and separators only.
    if (!stem.empty() && stem.back() == ')') {
        const size_t open = stem.rfind('(');
        if (open != std::string::npos) {
            const std::string inner = stem.substr(open + 1, stem.size() - open - 2);
            bool ok = !inner.empty() && inner.size() <= 12;
            for (char c : inner)
                if (!(std::isupper(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) ||
                      c == '-' || c == '/' || c == ',' || c == ' '))
                    ok = false;
            if (ok) {
                n.region = inner;
                stem.resize(open);
                while (!stem.empty() && stem.back() == ' ') stem.pop_back();
            }
        }
    }
    // "0001 LGPE - Title": what follows the first " - ".
    const size_t sep = stem.find(" - ");
    if (sep != std::string::npos && sep + 3 < stem.size()) {
        n.title = stem.substr(sep + 3);
    } else {
        size_t i = 0;
        while (i < stem.size() && std::isdigit(static_cast<unsigned char>(stem[i]))) i++;
        n.title = (i > 0 && i < stem.size() && stem[i] == ' ') ? stem.substr(i + 1) : stem;
    }
    return n;
}

std::string cardNumber(uint16_t id) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04u", static_cast<unsigned>(id));
    return buf;
}

} // anonymous namespace

void UI::drawWondercardListPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, SDL_Color{T().bg.r, T().bg.g, T().bg.b, 190});

    const int n = static_cast<int>(wcList_.size());
    const bool empty = n == 0;
    const int W = empty ? WC_EMPTY_W : WC_W, H = empty ? WC_EMPTY_H : WC_H;
    const int x = (SCREEN_W - W) / 2, y = (SCREEN_H - H) / 2;
    fillRounded(x, y, W, H, 22, T().panelBg);
    strokeRounded(x, y, W, H, 22, 1, T().panelBorder);

    TTF_Font* fTag = uiFont(11, true);
    auto tintedIcon = [&](SDL_Texture* ic, SDL_Rect dst, SDL_Color c) {
        if (!ic) return;
        SDL_SetTextureColorMod(ic, c.r, c.g, c.b);
        SDL_RenderCopy(renderer_, ic, nullptr, &dst);
        SDL_SetTextureColorMod(ic, 255, 255, 255);
    };

    // --- Header: title, how many, the game and its file types, the counts. ---
    {
        const int cy = y + WC_HEAD / 2;
        tintedIcon(uiIcon("gift"), {x + 26, cy - 12, 24, 24}, T().accent);
        TTF_Font* fT = uiFont(24, true);
        const std::string title = i18n::get(StrKey::WcTitle);
        drawText(title, x + 62, cy - TTF_FontHeight(fT) / 2, T().text, fT);
        int cx = x + 62 + textWidth(title, fT) + 14;

        if (!empty) {
            TTF_Font* fC = uiFont(13, true);
            const std::string found = i18n::fmt(StrKey::SrFound, std::to_string(n));
            const SDL_Color fc = T().statusOk;
            const int fw = textWidth(found, fC) + 20;
            fillRounded(cx, cy - 13, fw, 26, 8, SDL_Color{fc.r, fc.g, fc.b, 36});
            drawTextCentered(found, cx + fw / 2, cy, fc, fC);
            cx += fw + 14;
        }

        // Counts, right-aligned: shiny, and the cards that take your OT.
        int shiny = 0, yourOT = 0;
        for (const auto& wc : wcList_) {
            if (!wc.valid) continue;
            if (wc.isShiny) shiny++;
            if (!wc.hasOT) yourOT++;
        }
        TTF_Font* fN = uiFont(14, true);
        int rx = x + W - 26;
        if (yourOT > 0) {
            const std::string s = i18n::fmt(StrKey::WcYourOTCount, std::to_string(yourOT));
            rx -= textWidth(s, fN);
            drawText(s, rx, cy - TTF_FontHeight(fN) / 2, T().textDim, fN);
            rx -= 16;
            fillRounded(rx, cy - 5, 10, 10, 3, T().genderFemale);
            rx -= 18;
        }
        if (shiny > 0) {
            const std::string s = i18n::fmt(StrKey::WcShinyCount, std::to_string(shiny));
            rx -= textWidth(s, fN);
            drawText(s, rx, cy - TTF_FontHeight(fN) / 2, T().textDim, fN);
            rx -= 20;
            tintedIcon(iconShiny_, {rx, cy - 7, 14, 14}, T().accent);
            rx -= 18;
        }

        TTF_Font* fS = uiFont(15);
        const std::string scope = std::string(gameDisplayNameOf(selectedGame_)) + " \xc2\xb7 "
                                + gameInfo(selectedGame_).wcExtensionHint;
        if (rx > cx) drawText(fitText(scope, fS, rx - cx), cx, cy - TTF_FontHeight(fS) / 2, T().textDim, fS);
    }
    drawRect(x, y + WC_HEAD, W, 1, T().panelBorder);

    const int footY = y + H - WC_FOOT;
    drawRect(x, footY, W, 1, T().panelBorder);
    TTF_Font* fHint = uiFont(15, true);
    auto footer = [&](const std::vector<std::pair<const char*, const char*>>& hints) {
        const int cy = footY + WC_FOOT / 2;
        int hx = x + 25;
        for (const auto& h : hints) {
            hx += drawFooterKey(hx, cy, h.first, false) + 8;
            const std::string& l = i18n::get(h.second);
            drawText(l, hx, cy - TTF_FontHeight(fHint) / 2, T().text, fHint);
            hx += textWidth(l, fHint) + 22;
        }
    };

    // --- Empty: say so, and where the files go. ---
    if (empty) {
        const int cx = x + W / 2, top = y + WC_HEAD + 30;
        fillDisc(cx, top + 36, 32, T().buttonBg);
        tintedIcon(uiIcon("gift"), {cx - 14, top + 22, 28, 28}, T().textDim);
        drawTextCentered(i18n::get(StrKey::NoWCFound), cx, top + 100, T().text, uiFont(20, true));
        TTF_Font* fS = uiFont(15);
        drawTextCentered(i18n::fmt(StrKey::PlaceFilesIn, std::string(gameInfo(selectedGame_).wcExtensionHint)),
                         cx, top + 136, T().textDim, fS);
        const std::string path = "sdmc:/switch/pkHouse/wondercards/" + std::string(bankFolderNameOf(selectedGame_)) + "/";
        TTF_Font* fP = uiFont(15, true);
        const int pw = std::min(W - 80, textWidth(path, fP) + 32);
        fillRounded(cx - pw / 2, top + 158, pw, 40, 10, T().bg);
        drawTextCentered(fitText(path, fP, pw - 24), cx, top + 178, T().text, fP);
        footer({{"B", StrKey::HintClose}});
        return;
    }

    wcListCursor_ = std::clamp(wcListCursor_, 0, n - 1);
    wcListScroll_ = std::clamp(wcListScroll_, 0, std::max(0, n - WC_VISIBLE_ROWS));

    // --- List ---
    const int top = y + WC_HEAD + 18;
    const bool scrolls = n > WC_VISIBLE_ROWS;
    const int lx = x + 26, lw = WC_LIST_W - 26 - (scrolls ? 22 : 6);
    TTF_Font* fNum  = uiFont(13, true);
    TTF_Font* fName = uiFont(17, true);
    TTF_Font* fLv   = uiFont(13);
    TTF_Font* fSub  = uiFont(13);
    for (int i = wcListScroll_; i < n && i < wcListScroll_ + WC_VISIBLE_ROWS; i++) {
        const auto& wc = wcList_[i];
        const int ry = top + (i - wcListScroll_) * WC_PITCH, cy = ry + WC_ROW_H / 2;
        const bool cur = i == wcListCursor_;
        if (cur) {
            strokeRounded(lx - 6, ry - 6, lw + 12, WC_ROW_H + 12, 16, 3, T().accent);
            fillRounded(lx, ry, lw, WC_ROW_H, 11, T().slotFull);
        }

        // Card number
        const std::string num = wc.valid ? cardNumber(wc.cardID) : "----";
        fillRounded(lx + 10, cy - 13, 50, 26, 7, T().bg);
        drawTextCentered(num, lx + 35, cy, T().textDim, fNum);

        // Sprite tile, with the shiny star on its corner.
        const int tx = lx + 72;
        fillRounded(tx, cy - 19, 38, 38, 9, T().slotFull);
        if (wc.valid) {
            SDL_Texture* sp = wc.isShiny ? getShinySprite(wc.species) : nullptr;
            if (!sp) sp = getSprite(wc.species);
            drawSpriteFit(sp, tx + 19, cy, 34);
            if (wc.isShiny) tintedIcon(iconShiny_, {tx - 5, cy - 24, 12, 12}, T().accent);
        } else {
            tintedIcon(uiIcon("warn"), {tx + 9, cy - 10, 20, 20}, T().genderFemale);
        }

        // Right: region, and the cards that take your OT.
        int right = lx + lw - 16;
        const WcName parsed = parseWcFilename(wc.filename);
        auto tag = [&](const std::string& text, SDL_Color fg, SDL_Color bg) {
            const int w = measureTextTracked(text, fTag, 1) + 14;
            right -= w;
            fillRounded(right, cy - 11, w, 22, 6, bg);
            drawTextTracked(text, right + 7, cy - TTF_FontHeight(fTag) / 2, fg, fTag, 1);
            right -= 8;
        };
        if (wc.valid && !parsed.region.empty())
            tag(parsed.region, T().textDim, T().bg);
        if (wc.valid && !wc.hasOT) {
            const SDL_Color p = T().genderFemale;
            tag(toUpperUtf8(i18n::get(StrKey::WcYourOT)), p, SDL_Color{p.r, p.g, p.b, 40});
        }

        // Name and level over the card title (or, when invalid, the file).
        const int nx = tx + 52, nw = right - nx - 6;
        if (wc.valid) {
            const std::string lv = i18n::get(StrKey::LvPrefix) + std::to_string(wc.level);
            const std::string nm = fitText(SpeciesName::get(wc.species), fName, nw - textWidth(lv, fLv) - 8);
            drawText(nm, nx, cy - 20, T().text, fName);
            drawText(lv, nx + textWidth(nm, fName) + 8, cy - 17, T().textDim, fLv);
            drawText(fitText(parsed.title, fSub, nw), nx, cy + 3, T().textDim, fSub);
        } else {
            drawText(fitText(i18n::get(StrKey::InvalidWC), fName, nw), nx, cy - 20, T().genderFemale, fName);
            drawText(fitText(wc.filename, fSub, nw), nx, cy + 3, T().textDim, fSub);
        }
    }
    if (scrolls) {
        const int trackY = top, trackH = WC_VISIBLE_ROWS * WC_PITCH - 6;
        const int thumbH = std::max(30, trackH * WC_VISIBLE_ROWS / n);
        const int thumbY = trackY + (trackH - thumbH) * wcListScroll_ / std::max(1, n - WC_VISIBLE_ROWS);
        const int sx = x + WC_LIST_W - 14;
        fillRounded(sx, trackY, 5, trackH, 2, T().buttonBg);
        fillRounded(sx, thumbY, 5, thumbH, 2, T().textMuted);
    }
    drawRect(x + WC_LIST_W, y + WC_HEAD, 1, footY - y - WC_HEAD, T().panelBorder);

    // --- The highlighted card ---
    {
        const auto& wc = wcList_[wcListCursor_];
        const WcName parsed = parseWcFilename(wc.filename);
        const int px = x + WC_LIST_W + 22, pw = WC_PANE_W;
        int py = y + WC_HEAD + 20;

        fillRounded(px, py, 104, 104, 18, T().slotFull);
        if (wc.valid) {
            SDL_Texture* sp = wc.isShiny ? getShinySprite(wc.species) : nullptr;
            if (!sp) sp = getSprite(wc.species);
            drawSpriteFit(sp, px + 52, py + 52, 92);
            if (wc.isShiny) tintedIcon(iconShiny_, {px + 8, py + 8, 18, 18}, T().accent);
        } else {
            tintedIcon(uiIcon("warn"), {px + 32, py + 32, 40, 40}, T().genderFemale);
        }
        const int tx = px + 122, tw = pw - 122;
        TTF_Font* fBig = uiFont(28, true);
        if (wc.valid) {
            drawTextTracked(toUpperUtf8(i18n::fmt(StrKey::WcCard, cardNumber(wc.cardID))), tx, py + 16, T().accent, fTag, 2);
            drawText(fitText(SpeciesName::get(wc.species), fBig, tw), tx, py + 34, T().text, fBig);
            drawText(i18n::fmt(StrKey::WcLevel, std::to_string(wc.level)), tx, py + 74, T().textDim, uiFont(15, true));
        } else {
            drawText(fitText(i18n::get(StrKey::InvalidWC), fBig, tw), tx, py + 34, T().genderFemale, fBig);
        }
        py += 104 + 20;

        if (wc.valid) {
            TTF_Font* fT = uiFont(18, true);
            for (const auto& l : wrapText(parsed.title, fT, pw, 2)) {
                drawText(l, px, py, T().text, fT);
                py += 24;
            }
            py += 10;

            // Facts: shiny, whose OT, region, and the slot it goes into.
            struct Fact { const char* label; std::string value; SDL_Color c; };
            std::vector<Fact> facts;
            facts.push_back({StrKey::WcShiny, i18n::get(wc.isShiny ? StrKey::FilterYes : StrKey::GtsFilterNo), T().text});
            facts.push_back({StrKey::WcTrainer, i18n::get(wc.hasOT ? StrKey::WcEventOT : StrKey::WcYourOT),
                             wc.hasOT ? T().text : T().genderFemale});
            if (!parsed.region.empty()) facts.push_back({StrKey::WcRegion, parsed.region, T().text});
            const Panel panel = cursor_.panel;
            const int box = panel == Panel::Game ? gameBox_ : bankBox_;
            facts.push_back({StrKey::WcInto,
                             i18n::fmt(StrKey::SrSlot, panelBoxName(panel, box), std::to_string(cursor_.slot(gridCols()) + 1)),
                             T().text});
            TTF_Font* fL = uiFont(14);
            TTF_Font* fV = uiFont(14, true);
            const int boxH = 14 + static_cast<int>(facts.size()) * 27 + 6;
            fillRounded(px, py, pw, boxH, 12, T().bg);
            int fy = py + 14;
            for (const auto& f : facts) {
                drawText(fitText(i18n::get(f.label), fL, 100), px + 14, fy, T().textDim, fL);
                drawText(fitText(f.value, fV, pw - 134), px + 120, fy, f.c, fV);
                fy += 27;
            }
            py += boxH + 18;
        } else {
            TTF_Font* fB = uiFont(15);
            std::string body = i18n::get(StrKey::InvalidWCBody);
            std::replace(body.begin(), body.end(), '\n', ' ');
            for (const auto& l : wrapText(body, fB, pw, 3)) {
                drawText(l, px, py, T().textDim, fB);
                py += 22;
            }
            py += 16;
        }

        // The file
        drawTextTracked(toUpperUtf8(i18n::get(StrKey::WcFile)), px, py, T().textDim, fTag, 2);
        py += 20;
        TTF_Font* fF = uiFont(13);
        for (const auto& l : wrapText(wc.filename, fF, pw, 2)) {
            drawText(l, px, py, T().text, fF);
            py += 18;
        }

        // Inject. A card that takes your OT can only go into a save; when the
        // cursor is somewhere else the button says why instead (A still shows
        // the same message it always did).
        if (wc.valid) {
            const int bh = 52, by = footY - 22 - bh;
            const bool blocked = !wc.hasOT && (isDualBankMode() || cursor_.panel == Panel::Bank);
            if (blocked) {
                fillRounded(px, by, pw, bh, 14, T().buttonBg);
                strokeRounded(px, by, pw, bh, 14, 1, T().buttonBorder);
                std::string why = i18n::get(isDualBankMode() ? StrKey::CannotInjectBody : StrKey::CannotInjectBankBody);
                std::replace(why.begin(), why.end(), '\n', ' ');
                TTF_Font* fW = uiFont(13);
                const auto lines = wrapText(why, fW, pw - 56, 2);
                tintedIcon(uiIcon("warn"), {px + 16, by + bh / 2 - 9, 18, 18}, T().genderFemale);
                int wy = by + bh / 2 - static_cast<int>(lines.size()) * 9;
                for (const auto& l : lines) {
                    drawText(l, px + 44, wy, T().textDim, fW);
                    wy += 18;
                }
            } else {
                fillRounded(px, by, pw, bh, 14, T().accent);
                TTF_Font* fB = uiFont(17, true);
                const std::string label = fitText(i18n::fmt(StrKey::WcInject, SpeciesName::get(wc.species)), fB, pw - 80);
                const int w = 24 + 10 + textWidth(label, fB);
                const int bx = px + (pw - w) / 2, bcy = by + bh / 2;
                fillDisc(bx + 12, bcy, 12, T().keyCapText);
                drawTextCentered("A", bx + 12, bcy, T().accent, uiFont(13, true));
                drawText(label, bx + 34, bcy - TTF_FontHeight(fB) / 2, T().keyCapText, fB);
            }
        }
    }

    // --- Footer ---
    std::vector<std::pair<const char*, const char*>> hints = {{"A", StrKey::HintInject}};
    if (n > 1) hints.push_back({"L R", StrKey::HintJump10});
    hints.push_back({"B", StrKey::HintCancel});
    footer(hints);
    TTF_Font* fc = uiFont(14, true);
    const std::string pos = std::to_string(wcListCursor_ + 1) + " / " + std::to_string(n);
    drawText(pos, x + W - 25 - textWidth(pos, fc), footY + WC_FOOT / 2 - TTF_FontHeight(fc) / 2, T().textDim, fc);
}
