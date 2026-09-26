// Card importer: browse cards/<GameFamily>/*.png, decode the QR on the one you
// pick, validate it, and place the Pokemon at the cursor.
//
// Listing is deliberately cheap. Pulling a QR out of a 1280x720 image takes
// long enough that decoding every file up front would stall the browser, so
// rows are labelled from the filename and only the chosen card is decoded.

#include "ui.h"
#include "card_import.h"
#include "species_converter.h"
#include "met_info.h"
#include "move_types.h"
#include "i18n.h"
#include "ui_util.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <sys/stat.h>

// --- Preview ------------------------------------------------------------------

namespace {

// One place mapping a validation result to the string shown for it, used by
// both the preview pane and the failure dialog.
const char* cardResultKey(CardPayload::Result r) {
    switch (r) {
        case CardPayload::Result::UnsupportedVersion: return StrKey::CardNewerVersion;
        case CardPayload::Result::Truncated:
        case CardPayload::Result::BadChecksum:
        case CardPayload::Result::BadPokemonChecksum: return StrKey::CardDamaged;
        case CardPayload::Result::UnknownGame:
        case CardPayload::Result::BadSize:
        case CardPayload::Result::WrongGame:
        case CardPayload::Result::NotPresent:
        case CardPayload::Result::Implausible:        return StrKey::CardInvalid;
        default:                                      return StrKey::CardNoData;
    }
}

} // anonymous namespace

void UI::freeCardPreview() {
    cardPreview_ = CardPayload::Parsed{};
    cardPreviewIdx_ = -1;
    cardRowInfo_.clear();
}

// Decodes the highlighted card once the cursor has been still for a moment.
// Only the QR region is scanned here; a card that needs a full-image search
// says so rather than stalling the browser on every cursor move.
void UI::updateCardPreview() {
    if (cardList_.empty() || cardListCursor_ < 0
        || cardListCursor_ >= static_cast<int>(cardList_.size())) {
        freeCardPreview();
        return;
    }
    if (cardPreviewIdx_ == cardListCursor_)
        return;
    if (SDL_GetTicks() - cardPreviewSince_ < CARD_PREVIEW_DELAY_MS)
        return;

    cardPreview_ = decodeCard(cardList_[cardListCursor_].path, selectedGame_,
                              /*fastPathOnly=*/true);
    rememberCardRow(cardListCursor_, cardPreview_);
    // Record the row either way, so a card that will not read is not retried
    // on every frame.
    cardPreviewIdx_ = cardListCursor_;
}

// Compact summary of what the highlighted card actually contains. Everything
// here comes from the decoded payload, never from the filename.
void UI::drawCardPreviewPane(const CardPayload::Parsed& parsed, bool pending,
                             int paneX, int paneY, int paneW, int paneH) {
    if (pending) {
        drawTextCentered("...", paneX + paneW / 2, paneY + paneH / 2 - 9, T().textDim, font_);
        return;
    }
    if (parsed.result != CardPayload::Result::Ok) {
        // A fast-path miss is not a verdict: the whole image still gets scanned
        // when the user commits, so say that rather than calling it broken.
        const char* key = (parsed.result == CardPayload::Result::NotACard)
                        ? StrKey::CardPressAToRead
                        : cardResultKey(parsed.result);
        drawTextCentered(i18n::get(key), paneX + paneW / 2, paneY + paneH / 2 - 20,
                         T().textDim, fontSmall_);
        if (parsed.result == CardPayload::Result::WrongGame && parsed.gameKnown)
            drawTextCentered(gameInfo(parsed.game).bankGroupName,
                             paneX + paneW / 2, paneY + paneH / 2 + 2, T().red, fontSmall_);
        return;
    }

    const Pokemon& pkm = parsed.pkm;
    const uint16_t species = pkm.isEgg() ? 0 : pkm.species();
    int y = paneY;

    // Sprite, with the name block beside it.
    constexpr int SPR = 88;
    SDL_Texture* sprite = nullptr;
    if (pkm.isEgg()) {
        sprite = getSprite(0);
    } else if (pkm.isShiny()) {
        sprite = getShinySprite(species, pkm.form());
        if (!sprite) sprite = getSprite(species, pkm.form());
    } else {
        sprite = getSprite(species, pkm.form());
    }
    if (sprite) {
        int tw = 0, th = 0;
        SDL_QueryTexture(sprite, nullptr, nullptr, &tw, &th);
        int dw = SPR, dh = SPR;
        if (tw > 0 && th > 0) {
            float scale = std::min(static_cast<float>(SPR) / tw, static_cast<float>(SPR) / th);
            dw = static_cast<int>(tw * scale);
            dh = static_cast<int>(th * scale);
        }
        SDL_Rect dst = {paneX + (SPR - dw) / 2, y + (SPR - dh) / 2, dw, dh};
        SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
    }

    int infoX = paneX + SPR + 10;
    drawText(SpeciesName::get(species), infoX, y + 4,
             pkm.isShiny() ? T().shiny : T().text, font_);

    std::string line = i18n::get(StrKey::LvPrefix) + std::to_string(pkm.level());
    uint8_t g = pkm.gender();
    if (g == 0)      line += "  \xe2\x99\x82";
    else if (g == 1) line += "  \xe2\x99\x80";
    drawText(line, infoX, y + 30, T().textDim, fontSmall_);

    char dexBuf[16];
    std::snprintf(dexBuf, sizeof(dexBuf), "#%04u", static_cast<unsigned>(species));
    drawText(dexBuf, infoX, y + 50, T().textDim, fontSmall_);

    if (pkm.isShiny() && iconShiny_) {
        SDL_Rect d = {infoX, y + 68, 16, 16};
        SDL_RenderCopy(renderer_, iconShiny_, nullptr, &d);
    }
    if (pkm.isAlpha() && iconAlpha_) {
        SDL_Rect d = {infoX + 20, y + 68, 16, 16};
        SDL_RenderCopy(renderer_, iconAlpha_, nullptr, &d);
    }

    y += SPR + 12;

    auto field = [&](const char* label, const std::string& value) {
        drawText(label, paneX, y, T().textDim, fontSmall_);
        drawText(value, paneX + 84, y, T().text, fontSmall_);
        y += 21;
    };
    field(i18n::get(StrKey::NaturePrefix).c_str(), NatureName::get(pkm.nature()));
    field(i18n::get(StrKey::AbilityPrefix).c_str(), AbilityName::get(pkm.ability()));
    uint16_t item = pkm.heldItem();
    field(i18n::get(StrKey::HeldItemPrefix).c_str(),
          item != 0 ? ItemName::get(item) : i18n::get(StrKey::NoneItem));

    y += 8;

    // Moves, one per row with its type icon.
    const uint16_t moves[4] = {pkm.move1(), pkm.move2(), pkm.move3(), pkm.move4()};
    for (int i = 0; i < 4; i++) {
        if (moves[i] == 0) {
            drawText("---", paneX + 26, y + 2, T().textDim, fontSmall_);
        } else {
            uint8_t mtype = getMoveType(moves[i], pkm.gameType_);
            if (SDL_Texture* tex = getTypeSprite(mtype)) {
                SDL_Rect d = {paneX, y, 22, 22};
                SDL_RenderCopy(renderer_, tex, nullptr, &d);
            }
            drawText(MoveName::get(moves[i]), paneX + 26, y + 2, T().text, fontSmall_);
        }
        y += 24;
    }

    y += 8;

    char ivBuf[64];
    std::snprintf(ivBuf, sizeof(ivBuf), "%d/%d/%d/%d/%d/%d",
                  pkm.ivHp(), pkm.ivAtk(), pkm.ivDef(),
                  pkm.ivSpA(), pkm.ivSpD(), pkm.ivSpe());
    field(i18n::get(StrKey::IVs).c_str(), ivBuf);
    field(i18n::get(StrKey::OTPrefix).c_str(),
          pkm.otName() + "  " + std::to_string(pkm.displayTid()));
    const char* origin = VersionName::get(pkm.originVersion());
    if (origin[0] != '\0')
        field("", origin);
}

// --- List popup (UI 2.0) --------------------------------------------------
//
// Rows are labelled from the filename, as before; a row whose card has been
// read once (the preview decodes the highlighted one) shows what it holds.

namespace {

constexpr int CI_W = 1120, CI_H = 620;
constexpr int CI_HEAD = 68, CI_FOOT = 52;
constexpr int CI_LIST_W = 472;
constexpr int CI_ROW_H = 60, CI_PITCH = 66;
constexpr int CI_EMPTY_W = 760, CI_EMPTY_H = 380;

// "Rhyhorn - LGPE - [SA]": the name, the game tag and the flags.
struct CardLabel {
    std::string name;
    std::string tag;
    bool shiny = false, alpha = false, egg = false;
};

CardLabel parseCardLabel(const std::string& label) {
    CardLabel out;
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        const size_t sep = label.find(" - ", start);
        parts.push_back(label.substr(start, sep == std::string::npos ? std::string::npos : sep - start));
        if (sep == std::string::npos) break;
        start = sep + 3;
    }
    out.name = parts[0];
    for (size_t i = 1; i < parts.size(); i++) {
        // Flags as the exporter writes them: "[S]", "[A]", "[E]" or any mix.
        const std::string& p = parts[i];
        if (p.size() >= 3 && p.front() == '[' && p.back() == ']'
            && p.find_first_not_of("SAE", 1) == p.size() - 1) {
            out.shiny = p.find('S') != std::string::npos;
            out.alpha = p.find('A') != std::string::npos;
            out.egg   = p.find('E') != std::string::npos;
            continue;
        }
        bool code = !parts[i].empty() && parts[i].size() <= 6;
        for (char c : parts[i])
            if (!std::isalnum(static_cast<unsigned char>(c))) code = false;
        if (code && out.tag.empty()) out.tag = parts[i];
        else { out.name = label; out.tag.clear(); out.shiny = out.alpha = out.egg = false; break; }  // not our shape
    }
    return out;
}

// First two characters, whole UTF-8 sequences, for a tile with no sprite yet.
std::string initials(const std::string& s) {
    size_t i = 0;
    for (int n = 0; n < 2 && i < s.size(); n++) {
        i++;
        while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) i++;
    }
    return s.substr(0, i);
}

std::string stripLabel(std::string l) {
    while (!l.empty() && (l.back() == ':' || l.back() == ' ')) l.pop_back();
    return l;
}

} // anonymous namespace

void UI::rememberCardRow(int idx, const CardPayload::Parsed& parsed) {
    if (cardRowInfo_.size() != cardList_.size()) cardRowInfo_.assign(cardList_.size(), CardRowInfo{});
    if (idx < 0 || idx >= static_cast<int>(cardRowInfo_.size()) || parsed.result != CardPayload::Result::Ok)
        return;
    const Pokemon& pkm = parsed.pkm;
    CardRowInfo& r = cardRowInfo_[idx];
    r.known   = true;
    r.species = pkm.species();
    r.form    = pkm.form();
    r.shiny   = pkm.isShiny();
    r.egg     = pkm.isEgg();
    r.alpha   = pkm.isAlpha();
    r.gender  = pkm.gender();
    r.level   = pkm.level();
    r.ot      = pkm.otName();
    r.version = VersionName::get(pkm.originVersion());
}

void UI::drawCardListPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, SDL_Color{T().bg.r, T().bg.g, T().bg.b, 190});
    if (cardRowInfo_.size() != cardList_.size()) cardRowInfo_.assign(cardList_.size(), CardRowInfo{});

    const int n = static_cast<int>(cardList_.size());
    const bool empty = n == 0;
    const int W = empty ? CI_EMPTY_W : CI_W, H = empty ? CI_EMPTY_H : CI_H;
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

    // --- Header ---
    {
        const int cy = y + CI_HEAD / 2;
        tintedIcon(uiIcon("import"), {x + 26, cy - 12, 24, 24}, T().accent);
        TTF_Font* fT = uiFont(24, true);
        const std::string title = i18n::get(StrKey::CiTitle);
        drawText(title, x + 62, cy - TTF_FontHeight(fT) / 2, T().text, fT);
        int cx = x + 62 + textWidth(title, fT) + 14;
        if (!empty) {
            TTF_Font* fC = uiFont(13, true);
            const std::string count = n == 1 ? i18n::get(StrKey::CiCountOne)
                                             : i18n::fmt(StrKey::CiCount, std::to_string(n));
            const SDL_Color fc = T().statusOk;
            const int fw = textWidth(count, fC) + 20;
            fillRounded(cx, cy - 13, fw, 26, 8, SDL_Color{fc.r, fc.g, fc.b, 36});
            drawTextCentered(count, cx + fw / 2, cy, fc, fC);
            cx += fw + 14;
        }
        TTF_Font* fS = uiFont(15);
        drawText(fitText(i18n::fmt(StrKey::CiSubtitle, std::string(bankGroupNameOf(selectedGame_))), fS, x + W - 26 - cx),
                 cx, cy - TTF_FontHeight(fS) / 2, T().textDim, fS);
    }
    drawRect(x, y + CI_HEAD, W, 1, T().panelBorder);

    const int footY = y + H - CI_FOOT;
    drawRect(x, footY, W, 1, T().panelBorder);
    TTF_Font* fHint = uiFont(15, true);
    auto footer = [&](const std::vector<std::pair<const char*, const char*>>& hints) {
        const int cy = footY + CI_FOOT / 2;
        int hx = x + 25;
        for (const auto& h : hints) {
            hx += drawFooterKey(hx, cy, h.first, false) + 8;
            const std::string& l = i18n::get(h.second);
            drawText(l, hx, cy - TTF_FontHeight(fHint) / 2, T().text, fHint);
            hx += textWidth(l, fHint) + 22;
        }
    };

    // --- Empty: say so, and where the cards go. ---
    if (empty) {
        const int cx = x + W / 2, top = y + CI_HEAD + 30;
        fillDisc(cx, top + 36, 32, T().buttonBg);
        tintedIcon(uiIcon("import"), {cx - 14, top + 22, 28, 28}, T().textDim);
        drawTextCentered(i18n::get(StrKey::NoCardsFound), cx, top + 100, T().text, uiFont(20, true));
        drawTextCentered(i18n::get(StrKey::PlaceCardsIn), cx, top + 136, T().textDim, uiFont(15));
        const std::string path = "sdmc:/switch/pkHouse/cards/" + std::string(bankFolderNameOf(selectedGame_)) + "/";
        TTF_Font* fP = uiFont(15, true);
        const int pw = std::min(W - 80, textWidth(path, fP) + 32);
        fillRounded(cx - pw / 2, top + 158, pw, 40, 10, T().bg);
        drawTextCentered(fitText(path, fP, pw - 24), cx, top + 178, T().text, fP);
        footer({{"B", StrKey::HintClose}});
        return;
    }

    cardListCursor_ = std::clamp(cardListCursor_, 0, n - 1);
    cardListScroll_ = std::clamp(cardListScroll_, 0, std::max(0, n - CARD_VISIBLE_ROWS));

    // --- List ---
    const int top = y + CI_HEAD + 18;
    const bool scrolls = n > CARD_VISIBLE_ROWS;
    const int lx = x + 26, lw = CI_LIST_W - 26 - (scrolls ? 22 : 6);
    TTF_Font* fName = uiFont(17, true);
    TTF_Font* fLv   = uiFont(13);
    TTF_Font* fSub  = uiFont(13);
    for (int i = cardListScroll_; i < n && i < cardListScroll_ + CARD_VISIBLE_ROWS; i++) {
        const CardLabel lab = parseCardLabel(cardList_[i].label);
        const CardRowInfo& info = cardRowInfo_[i];
        const int ry = top + (i - cardListScroll_) * CI_PITCH, cy = ry + CI_ROW_H / 2;
        const bool cur = i == cardListCursor_;
        if (cur) {
            strokeRounded(lx - 6, ry - 6, lw + 12, CI_ROW_H + 12, 16, 3, T().accent);
            fillRounded(lx, ry, lw, CI_ROW_H, 11, T().slotFull);
        }
        const bool shiny = info.known ? info.shiny : lab.shiny;
        const bool alpha = info.known ? info.alpha : lab.alpha;
        const bool egg   = info.known ? info.egg   : lab.egg;

        // Sprite once read, the name's first letters until then.
        const int tx = lx + 12;
        fillRounded(tx, cy - 20, 40, 40, 9, T().slotFull);
        if (info.known) {
            drawSpriteFit(spriteFor(info.species, info.form, info.shiny, info.egg), tx + 20, cy, 36);
        } else if (cardList_[i].hasSpecies) {
            // Named by a newer export: the sprite is known from the filename.
            drawSpriteFit(spriteFor(cardList_[i].species, cardList_[i].form, lab.shiny,
                                    lab.egg || cardList_[i].species == 0), tx + 20, cy, 36);
        } else {
            drawTextCentered(initials(lab.name), tx + 20, cy, shiny ? T().accent : T().textDim, uiFont(15, true));
        }
        // Marks on the tile's corners: shiny top left, alpha top right, egg
        // bottom right.
        if (shiny) tintedIcon(iconShiny_, {tx - 5, cy - 25, 12, 12}, T().accent);
        if (alpha) tintedIcon(iconAlpha_, {tx + 40 - 7, cy - 25, 12, 12}, T().accent);
        if (egg)   drawSpriteFit(getSprite(0), tx + 40 - 1, cy + 20 - 1, 16);

        int right = lx + lw - 14;
        if (!lab.tag.empty()) {
            const int w = measureTextTracked(lab.tag, fTag, 1) + 14;
            right -= w;
            fillRounded(right, cy - 11, w, 22, 6, T().bg);
            drawTextTracked(lab.tag, right + 7, cy - TTF_FontHeight(fTag) / 2, T().textDim, fTag, 1);
            right -= 8;
        }

        const int nx = tx + 54, nw = right - nx - 6;
        if (info.known) {
            std::string name = SpeciesName::get(info.species);
            if (info.egg) name += " - " + i18n::get(StrKey::Egg);
            std::string gender;
            SDL_Color gc = T().text;
            if (!info.egg && info.gender == 0) { gender = "\xe2\x99\x82"; gc = T().genderMale; }
            if (!info.egg && info.gender == 1) { gender = "\xe2\x99\x80"; gc = T().genderFemale; }
            const std::string lv = info.egg ? std::string() : i18n::get(StrKey::LvPrefix) + std::to_string(info.level);
            const int tail = (gender.empty() ? 0 : textWidth(gender, fName) + 6) + (lv.empty() ? 0 : textWidth(lv, fLv) + 8);
            const std::string nm = fitText(name, fName, nw - tail);
            drawText(nm, nx, cy - 21, T().text, fName);
            int ex = nx + textWidth(nm, fName) + 6;
            if (!gender.empty()) { drawText(gender, ex, cy - 21, gc, fName); ex += textWidth(gender, fName) + 8; }
            if (!lv.empty()) drawText(lv, ex, cy - 18, T().textDim, fLv);
            std::string sub = i18n::fmt(StrKey::ChipOt, info.ot);
            if (!info.version.empty()) sub += " \xc2\xb7 " + info.version;
            drawText(fitText(sub, fSub, nw), nx, cy + 3, T().textDim, fSub);
        } else {
            drawText(fitText(lab.name, fName, nw), nx, cy - 21, T().text, fName);
            drawText(fitText(cardList_[i].filename, fSub, nw), nx, cy + 3, T().textMuted, fSub);
        }
    }
    int listEnd = top + std::min(n, CARD_VISIBLE_ROWS) * CI_PITCH;
    if (scrolls) {
        const int trackH = CARD_VISIBLE_ROWS * CI_PITCH - 6;
        const int thumbH = std::max(30, trackH * CARD_VISIBLE_ROWS / n);
        const int thumbY = top + (trackH - thumbH) * cardListScroll_ / std::max(1, n - CARD_VISIBLE_ROWS);
        const int sx = x + CI_LIST_W - 14;
        fillRounded(sx, top, 5, trackH, 2, T().buttonBg);
        fillRounded(sx, thumbY, 5, thumbH, 2, T().textMuted);
    }
    // A lone card: say where more come from.
    if (n == 1) {
        const int bx = lx, by = listEnd + 8, bw = lw;
        TTF_Font* fH = uiFont(15, true);
        TTF_Font* fB = uiFont(13);
        const auto lines = wrapText(i18n::get(StrKey::CiOneBody), fB, bw - 36, 3);
        const int bh = 20 + TTF_FontHeight(fH) + 8 + static_cast<int>(lines.size()) * 19 + 16;
        strokeRounded(bx, by, bw, bh, 12, 1, T().panelBorder);
        drawText(fitText(i18n::get(StrKey::CiOneTitle), fH, bw - 36), bx + 18, by + 20, T().text, fH);
        int ly = by + 20 + TTF_FontHeight(fH) + 8;
        for (const auto& l : lines) { drawText(l, bx + 18, ly, T().textDim, fB); ly += 19; }
    }
    drawRect(x + CI_LIST_W, y + CI_HEAD, 1, footY - y - CI_HEAD, T().panelBorder);

    // --- The highlighted card ---
    const int px = x + CI_LIST_W + 24, pw = x + W - 24 - px;
    const int paneTop = y + CI_HEAD + 20, btnY = footY - 22 - 52;
    const bool pending = cardPreviewIdx_ != cardListCursor_;
    const CardPayload::Parsed& parsed = cardPreview_;
    const bool ok = !pending && parsed.result == CardPayload::Result::Ok;
    auto button = [&](const std::string& label) {
        fillRounded(px, btnY, pw, 52, 14, T().accent);
        TTF_Font* fB = uiFont(17, true);
        const std::string l = fitText(label, fB, pw - 80);
        const int w = 24 + 10 + textWidth(l, fB);
        const int bx = px + (pw - w) / 2, bcy = btnY + 26;
        fillDisc(bx + 12, bcy, 12, T().keyCapText);
        drawTextCentered("A", bx + 12, bcy, T().accent, uiFont(13, true));
        drawText(l, bx + 34, bcy - TTF_FontHeight(fB) / 2, T().keyCapText, fB);
    };

    if (!ok) {
        // Not read yet, or read and refused. A fast-path miss is not a
        // verdict: A still scans the whole image, so it says that instead.
        const int cx = px + pw / 2, cy = paneTop + (btnY - paneTop) / 2 - 30;
        if (pending) {
            drawTextCentered("...", cx, cy, T().textDim, uiFont(24, true));
        } else {
            const bool miss = parsed.result == CardPayload::Result::NotACard;
            fillDisc(cx, cy - 20, 30, T().buttonBg);
            tintedIcon(uiIcon(miss ? "card" : "warn"), {cx - 13, cy - 33, 26, 26},
                       miss ? T().textDim : T().genderFemale);
            TTF_Font* fM = uiFont(16, true);
            const auto lines = wrapText(i18n::get(miss ? StrKey::CardPressAToRead : cardResultKey(parsed.result)),
                                        fM, pw - 40, 3);
            int ly = cy + 30;
            for (const auto& l : lines) { drawTextCentered(l, cx, ly, T().text, fM); ly += 22; }
            if (parsed.result == CardPayload::Result::WrongGame && parsed.gameKnown)
                drawTextCentered(gameInfo(parsed.game).bankGroupName, cx, ly + 6, T().red, uiFont(15, true));
            button(i18n::get(StrKey::HintImport));
        }
    } else {
        const Pokemon& pkm = parsed.pkm;
        const bool egg = pkm.isEgg();
        const uint16_t species = egg ? 0 : pkm.species();
        int py = paneTop;

        // Portrait, name, level and dex number, perfect IVs.
        fillRounded(px, py, 92, 92, 18, T().slotFull);
        drawSpriteFit(spriteFor(pkm.species(), pkm.form(), pkm.isShiny(), egg), px + 46, py + 46, 80);
        if (pkm.isShiny()) tintedIcon(iconShiny_, {px - 8, py - 8, 18, 18}, T().accent);
        if (pkm.isAlpha()) tintedIcon(iconAlpha_, {px + 92 - 10, py - 8, 18, 18}, T().accent);
        if (egg)           drawSpriteFit(getSprite(0), px + 92 - 1, py + 92 - 1, 24);

        const int ivs[6] = {pkm.ivHp(), pkm.ivAtk(), pkm.ivDef(), pkm.ivSpA(), pkm.ivSpD(), pkm.ivSpe()};
        int perfect = 0;
        for (int v : ivs) if (v == 31) perfect++;
        TTF_Font* fP = uiFont(12, true);
        const std::string badge = i18n::fmt(StrKey::InfoPerfectIvs, std::to_string(perfect));
        const int bw = measureTextTracked(badge, fP, 1) + 20;
        fillRounded(px + pw - bw, py + 34, bw, 24, 12, T().badgeBg);
        drawTextTracked(badge, px + pw - bw + 10, py + 46 - TTF_FontHeight(fP) / 2, T().accent, fP, 1);

        const int tx = px + 108, tw = pw - 108 - bw - 12;
        TTF_Font* fBig = uiFont(28, true);
        std::string gender;
        SDL_Color gc = T().text;
        if (!egg && pkm.gender() == 0) { gender = "\xe2\x99\x82"; gc = T().genderMale; }
        if (!egg && pkm.gender() == 1) { gender = "\xe2\x99\x80"; gc = T().genderFemale; }
        std::string name = SpeciesName::get(species);
        if (egg) name = SpeciesName::get(pkm.species()) + " - " + i18n::get(StrKey::Egg);
        const std::string nm = fitText(name, fBig, tw - (gender.empty() ? 0 : textWidth(gender, fBig) + 8));
        drawText(nm, tx, py + 14, pkm.isShiny() ? T().accent : T().text, fBig);
        if (!gender.empty()) drawText(gender, tx + textWidth(nm, fBig) + 8, py + 14, gc, fBig);
        char dex[16];
        std::snprintf(dex, sizeof(dex), "#%04u", static_cast<unsigned>(pkm.species()));
        TTF_Font* fL = uiFont(15, true);
        const std::string lv = i18n::get(StrKey::LvPrefix) + std::to_string(pkm.level());
        drawText(lv, tx, py + 56, T().textDim, fL);
        drawText(dex, tx + textWidth(lv, fL) + 14, py + 56, T().textDim, fL);
        py += 92 + 22;

        // Nature, ability, held item.
        {
            const int colW = pw / 3;
            TTF_Font* fV = uiFont(17, true);
            const uint16_t item = pkm.heldItem();
            const struct { const char* key; std::string value; bool dim; } cols[3] = {
                {StrKey::NaturePrefix,   NatureName::get(pkm.nature()),   false},
                {StrKey::AbilityPrefix,  AbilityName::get(pkm.ability()), false},
                {StrKey::HeldItemPrefix, item ? ItemName::get(item) : i18n::get(StrKey::NoneItem), item == 0},
            };
            for (int c = 0; c < 3; c++) {
                const int cx = px + c * colW;
                drawTextTracked(toUpperUtf8(stripLabel(i18n::get(cols[c].key))), cx, py, T().textDim, fTag, 2);
                drawText(fitText(cols[c].value, fV, colW - 12), cx, py + 18, cols[c].dim ? T().textMuted : T().text, fV);
            }
            py += 50;
        }

        // Moves, two by two, with their type.
        {
            TTF_Font* fM = uiFont(16);
            TTF_Font* fType = uiFont(10, true);
            const uint16_t moves[4] = {pkm.move1(), pkm.move2(), pkm.move3(), pkm.move4()};
            const int mw = (pw - 8) / 2, mh = 40;
            for (int i = 0; i < 4; i++) {
                const int mx = px + (i % 2) * (mw + 8), my = py + (i / 2) * (mh + 8), mcy = my + mh / 2;
                fillRounded(mx, my, mw, mh, 10, T().bg);
                strokeRounded(mx, my, mw, mh, 10, 1, T().panelBorder);
                const int nameX = mx + 14 + 16 + 10;
                if (moves[i] == 0) {
                    drawText("---", nameX, mcy - TTF_FontHeight(fM) / 2, T().textMuted, fM);
                    continue;
                }
                const uint8_t type = getMoveType(moves[i], pkm.gameType_);
                int typeW = 0;
                if (type < 18) {
                    if (SDL_Texture* tex = getTypeSprite(type))
                        blitDisc(renderer_, tex, mx + 14 + 8, mcy, 8, T().bg);
                    const char* label = TypeName::get(type);
                    typeW = measureTextTracked(label, fType, 1);
                    drawTextTracked(label, mx + mw - 12 - typeW, mcy - TTF_FontHeight(fType) / 2, T().textDim, fType, 1);
                    typeW += 10;
                }
                drawText(fitText(MoveName::get(moves[i]), fM, mx + mw - 12 - typeW - nameX),
                         nameX, mcy - TTF_FontHeight(fM) / 2, T().text, fM);
            }
            py += 2 * mh + 8 + 20;
        }

        // IVs as bars, gold on a 31.
        {
            drawTextTracked(toUpperUtf8(i18n::get(StrKey::IVs)), px, py, T().textDim, fTag, 2);
            py += 22;
            const char* keys[6] = {StrKey::StatHP, StrKey::StatAtk, StrKey::StatDef,
                                   StrKey::StatSpA, StrKey::StatSpD, StrKey::StatSpe};
            const int gap = 10, cw = (pw - 5 * gap) / 6;
            TTF_Font* fS = uiFont(11, true);
            TTF_Font* fN = uiFont(13, true);
            for (int i = 0; i < 6; i++) {
                const int cx = px + i * (cw + gap);
                const std::string v = std::to_string(ivs[i]);
                const int vw = textWidth(v, fN);
                drawText(fitText(toUpperUtf8(i18n::get(keys[i])), fS, cw - vw - 6), cx, py + 1, T().textDim, fS);
                drawText(v, cx + cw - vw, py - 1, ivs[i] == 0 ? T().textMuted : T().text, fN);
                fillRounded(cx, py + 19, cw, 6, 3, T().bg);
                const int fw = cw * std::min(ivs[i], 31) / 31;
                if (fw > 0) fillRounded(cx, py + 19, std::max(fw, 6), 6, 3, ivs[i] == 31 ? T().accent : T().accentBank);
            }
            py += 44;
        }

        // Trainer.
        {
            TTF_Font* fO = uiFont(15, true);
            TTF_Font* fD = uiFont(14);
            const std::string ot = stripLabel(i18n::get(StrKey::OTPrefix));
            drawTextTracked(toUpperUtf8(ot), px, py + 3, T().textDim, fTag, 2);
            int ox = px + measureTextTracked(toUpperUtf8(ot), fTag, 2) + 12;
            const std::string otName = pkm.otName();
            drawText(otName, ox, py, T().text, fO);
            ox += textWidth(otName, fO) + 12;
            const std::string tid = std::to_string(pkm.displayTid());
            drawText(tid, ox, py + 1, T().textDim, fD);
            ox += textWidth(tid, fD);
            const char* origin = VersionName::get(pkm.originVersion());
            if (origin[0] != '\0')
                drawText(fitText(std::string("  \xc2\xb7  ") + origin, fD, px + pw - ox), ox, py + 1, T().textDim, fD);
        }

        button(i18n::fmt(StrKey::CiImport, name));
    }

    // --- Footer ---
    std::vector<std::pair<const char*, const char*>> hints = {{"A", StrKey::HintImport}};
    hints.push_back({"B", StrKey::HintClose});
    footer(hints);
    TTF_Font* fc = uiFont(14, true);
    const std::string pos = std::to_string(cardListCursor_ + 1) + " / " + std::to_string(n);
    drawText(pos, x + W - 25 - textWidth(pos, fc), footY + CI_FOOT / 2 - TTF_FontHeight(fc) / 2, T().textDim, fc);
}

// --- Import confirmation -----------------------------------------------------

// Modal preview of the decoded Pokemon, reusing the detail popup so the card
// you are about to accept is shown exactly as any other Pokemon would be.
bool UI::showCardImportConfirm(const Pokemon& pkm) {
    if (!renderer_) return false;
    markDirty();

    detailRibbonScroll_ = 0;
    int result = -1;
    bool redraw = true;   // only after something changed, like every other screen
    while (result < 0) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                result = 0;
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                redraw = true;
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_B: result = 1; break; // Switch A
                    case SDL_CONTROLLER_BUTTON_A: result = 0; break; // Switch B
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:   scrollDetailRibbons(-1, pkm); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: scrollDetailRibbons(+1, pkm); break;
                }
            }
        }

        if (redraw && result < 0) {
            const ButtonHint hints[] = {
                {"A", StrKey::HintImport}, {"B", StrKey::HintCancel},
            };
            drawDetailPopup(pkm, hints, 2, std::string());
            SDL_RenderPresent(renderer_);
            redraw = false;
        }
        SDL_Delay(16);
    }
    return result == 1;
}

// --- Input -------------------------------------------------------------------

void UI::handleCardListInput(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
            event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
            int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
            updateStick(lx, ly);
        }
        return;
    }
    if (event.type != SDL_CONTROLLERBUTTONDOWN)
        return;

    const int count = static_cast<int>(cardList_.size());
    const int visibleRows = CARD_VISIBLE_ROWS;

    auto move = [&](int delta) {
        if (count == 0) return;
        cardListCursor_ += delta;
        if (cardListCursor_ < 0) cardListCursor_ = count - 1;
        if (cardListCursor_ >= count) cardListCursor_ = 0;
        if (cardListCursor_ < cardListScroll_)
            cardListScroll_ = cardListCursor_;
        else if (cardListCursor_ >= cardListScroll_ + visibleRows)
            cardListScroll_ = cardListCursor_ - visibleRows + 1;
        cardPreviewSince_ = SDL_GetTicks();
        markDirty();
    };

    switch (event.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:   move(-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: move(1);  break;
        case SDL_CONTROLLER_BUTTON_B:  // Switch A
            if (count > 0 && cardListCursor_ < count) {
                CardFile card = cardList_[cardListCursor_];
                importCard(card);
            }
            break;
        case SDL_CONTROLLER_BUTTON_A:  // Switch B
            showCardList_ = false;
            freeCardPreview();
            markDirty();
            break;
    }
}

// --- Moving a misfiled card --------------------------------------------------

bool UI::relocateCard(const CardFile& card, GameType correctGame, std::string& outFolder) {
    const std::string family = bankFolderNameOf(correctGame);
    const std::string dir = basePath_ + "cards/";
    const std::string gameDir = dir + family + "/";
    mkdir(dir.c_str(), 0755);
    mkdir(gameDir.c_str(), 0755);

    // Never overwrite a card already sitting there; suffix instead, the way
    // bank soft-delete does.
    std::string target = gameDir + card.filename;
    struct stat st;
    if (stat(target.c_str(), &st) == 0) {
        std::string stem = card.filename.substr(0, card.filename.size() - 4);
        for (int n = 2; n < 100; n++) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), " (%d).png", n);
            target = gameDir + stem + buf;
            if (stat(target.c_str(), &st) != 0)
                break;
        }
    }

    outFolder = "cards/" + family + "/";
    return std::rename(card.path.c_str(), target.c_str()) == 0;
}

// --- Import ------------------------------------------------------------------

void UI::importCard(const CardFile& card) {
    using R = CardPayload::Result;

    // The browser has almost always read this card already, so reuse its
    // verdict instead of decoding a second time. Only a card whose code was
    // never found needs the full-image search, and only that is slow enough to
    // be worth a progress screen.
    CardPayload::Parsed parsed;
    if (cardPreviewIdx_ == cardListCursor_ && cardPreview_.result != R::NotACard) {
        parsed = cardPreview_;
    } else {
        showWorking(i18n::get(StrKey::ReadingCard));
        parsed = decodeCard(card.path, selectedGame_);
        rememberCardRow(cardListCursor_, parsed);
    }

    // A card in the wrong folder is not an error the user can do anything with
    // unless we say where it belongs, so offer to file it correctly.
    if (parsed.result == R::WrongGame && parsed.gameKnown) {
        std::string folder = "cards/" + std::string(bankFolderNameOf(parsed.game)) + "/";
        if (!showConfirmDialog(i18n::get(StrKey::CardWrongGame),
                               i18n::fmt(StrKey::CardWrongGameBody,
                                         gameInfo(parsed.game).bankGroupName, folder)))
            return;

        std::string moved;
        if (relocateCard(card, parsed.game, moved)) {
            showMessageAndWait(i18n::get(StrKey::CardMoved),
                               i18n::fmt(StrKey::CardMovedBody, moved), DialogKind::Success);
            cardList_ = scanCards(basePath_, selectedGame_);
            cardListCursor_ = 0;
            cardListScroll_ = 0;
            freeCardPreview();
            cardPreviewSince_ = SDL_GetTicks();
        } else {
            showMessageAndWait(i18n::get(StrKey::CardImportFailed),
                               i18n::get(StrKey::CardMoveFailed));
        }
        markDirty();
        return;
    }

    if (parsed.result != R::Ok) {
        showMessageAndWait(i18n::get(StrKey::CardImportFailed),
                           i18n::get(cardResultKey(parsed.result)));
        markDirty();
        return;
    }

    // Show what actually decoded, not what the filename claimed, and let the
    // user back out before anything is written.
    if (!showCardImportConfirm(parsed.pkm)) {
        markDirty();
        return;
    }

    // Place at the cursor, exactly where a wondercard would go.
    Panel panel = cursor_.panel;
    int box  = (panel == Panel::Game) ? gameBox_ : bankBox_;
    int slot = cursor_.slot(gridCols());

    if (!getPokemonAt(box, slot, panel).isEmpty()) {
        showMessageAndWait(i18n::get(StrKey::SlotOccupied), i18n::get(StrKey::SlotOccupiedBody));
        markDirty();
        return;
    }

    // SaveFile::setBoxSlot applies the handling-trainer update and registers the
    // Pokedex entry, so an imported card lands exactly like a bank -> save move.
    setPokemonAt(box, slot, panel, parsed.pkm);
    refreshHighlightSet();

    showCardList_ = false;
    freeCardPreview();
    markDirty();

    std::string panelName = (panel == Panel::Game)
        ? (isDualBankMode() ? i18n::get(StrKey::LocLeft) : i18n::get(StrKey::LocSave))
        : (isDualBankMode() ? i18n::get(StrKey::LocRight) : i18n::get(StrKey::LocBank));
    showMessageAndWait(i18n::get(StrKey::Imported),
        i18n::fmt(StrKey::ImportedBody,
                  std::vector<std::string>{SpeciesName::get(parsed.pkm.species()), panelName,
                                           std::to_string(box + 1), std::to_string(slot + 1)}), DialogKind::Success);
}
