// Dialogs (UI 2.0), after external/UI_2.0 "8a" and "8b".
//
// The rules:
//  - the title names the action and the object ("Delete bank "azerty"?");
//  - every action is a button carrying its key; A does what its label says,
//    B always cancels;
//  - press A when nothing is lost, hold A (HOLD_MS) when Pokemon would be,
//    so a double-tap cannot destroy anything;
//  - only destructive actions are red; errors are amber.

#include "ui.h"
#include "ui_util.h"
#include "i18n.h"
#include "species_converter.h"
#include <algorithm>
#include <cstdio>

namespace {

constexpr int DLG_W   = 520;
constexpr int DLG_PAD = 24;
constexpr int BADGE   = 48;
constexpr int BTN_H   = 52;

// Body text: explicit line breaks kept, each paragraph wrapped.
std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t nl = s.find('\n', start);
        if (nl == std::string::npos) nl = s.size();
        out.push_back(s.substr(start, nl - start));
        start = nl + 1;
    }
    return out;
}

} // anonymous namespace

void UI::drawDialogBackdrop() {
    // Reused while no frame has been presented since and the screen is the
    // same one: a blocking operation, or a dialog redrawing for its hold.
    if (backdrop_ && backdropGen_ == frameGen_ && backdropScreen_ == screen_) {
        SDL_RenderCopy(renderer_, backdrop_, nullptr, nullptr);
        return;
    }

    if (!backdrop_) {
        backdrop_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_TARGET, SCREEN_W, SCREEN_H);
    }
    const bool cached = backdrop_ && SDL_SetRenderTarget(renderer_, backdrop_) == 0;

    // What was on screen, dimmed. Before anything has been shown (loading
    // profiles at start) there is nothing to dim: just the ground and logo.
    if (screenDrawn_) {
        drawCurrentScreen();
    } else {
        SDL_SetRenderDrawColor(renderer_, T().bg.r, T().bg.g, T().bg.b, 255);
        SDL_RenderClear(renderer_);
        drawRect(0, 0, SCREEN_W, ACCENT_RULE_H, T().accent);
        drawLogo(32, 40);
    }
    drawRect(0, 0, SCREEN_W, SCREEN_H, SDL_Color{T().bg.r, T().bg.g, T().bg.b, 190});

    // Without a render target (it should not happen) it was drawn straight to
    // the screen, and simply is not reused.
    if (cached) {
        SDL_SetRenderTarget(renderer_, nullptr);
        SDL_RenderCopy(renderer_, backdrop_, nullptr, nullptr);
        backdropGen_ = frameGen_;
        backdropScreen_ = screen_;
    }
}

void UI::drawDialog(DialogIcon icon, const std::string& title, const std::string& body,
                    int objectH, const std::function<void(int, int, int)>& drawObject,
                    const std::string& note, const std::vector<DialogButton>& buttons,
                    const std::string& footnote) {
    TTF_Font* fTitle = uiFont(22, true);
    TTF_Font* fBody  = uiFont(15);
    TTF_Font* fNote  = uiFont(14, true);
    TTF_Font* fFoot  = uiFont(13);
    TTF_Font* fBtn   = uiFont(16, true);
    TTF_Font* fKey   = uiFont(13, true);

    const int textX = DLG_PAD + BADGE + 16;
    const int textW = DLG_W - textX - DLG_PAD;
    const int innerW = DLG_W - 2 * DLG_PAD;

    // Measure everything first: the dialog is as tall as what it says.
    const auto titleLines = wrapText(title, fTitle, textW, 2);
    std::vector<std::string> bodyLines;
    for (const auto& para : splitLines(body)) {
        if (para.empty()) { bodyLines.push_back(std::string()); continue; }
        for (const auto& l : wrapText(para, fBody, textW, 3)) bodyLines.push_back(l);
        if (bodyLines.size() >= 7) break;
    }
    while (!bodyLines.empty() && bodyLines.back().empty()) bodyLines.pop_back();
    const auto noteLines = note.empty() ? std::vector<std::string>()
                                        : wrapText(note, fNote, innerW - 16, 2);
    const auto footLines = footnote.empty() ? std::vector<std::string>()
                                            : wrapText(footnote, fFoot, innerW, 2);

    const int titleH = static_cast<int>(titleLines.size()) * 28;
    const int bodyH  = bodyLines.empty() ? 0 : 6 + static_cast<int>(bodyLines.size()) * 21;
    const int headH  = std::max(BADGE, titleH + bodyH);
    int h = DLG_PAD + headH;
    if (objectH > 0)       h += 18 + objectH;
    if (!noteLines.empty()) h += 16 + static_cast<int>(noteLines.size()) * 20;
    h += 20 + BTN_H;
    if (!footLines.empty()) h += 14 + static_cast<int>(footLines.size()) * 17;
    h += DLG_PAD;

    const int x = (SCREEN_W - DLG_W) / 2, y = (SCREEN_H - h) / 2;
    fillRounded(x + 3, y + 6, DLG_W, h, 20, SDL_Color{0, 0, 0, 90});
    fillRounded(x, y, DLG_W, h, 20, T().panelBg);
    strokeRounded(x, y, DLG_W, h, 20, 1, T().panelBorder);

    // Badge
    {
        const int cx = x + DLG_PAD + BADGE / 2, cy = y + DLG_PAD + BADGE / 2;
        const SDL_Color red = T().red, ok = T().statusOk, warn = T().statusWarn, info = T().accentBank;
        switch (icon) {
            case DialogIcon::Trash:
                fillDisc(cx, cy, BADGE / 2, SDL_Color{red.r, red.g, red.b, 50});
                if (iconTrash_) {
                    SDL_SetTextureColorMod(iconTrash_, red.r, red.g, red.b);
                    SDL_Rect dst = {cx - 12, cy - 12, 24, 24};
                    SDL_RenderCopy(renderer_, iconTrash_, nullptr, &dst);
                    SDL_SetTextureColorMod(iconTrash_, 255, 255, 255);
                }
                break;
            case DialogIcon::Warning:
                fillDisc(cx, cy, BADGE / 2, SDL_Color{warn.r, warn.g, warn.b, 50});
                if (iconWarn_) {
                    SDL_SetTextureColorMod(iconWarn_, warn.r, warn.g, warn.b);
                    SDL_Rect dst = {cx - 13, cy - 14, 26, 26};
                    SDL_RenderCopy(renderer_, iconWarn_, nullptr, &dst);
                    SDL_SetTextureColorMod(iconWarn_, 255, 255, 255);
                }
                break;
            case DialogIcon::Success:
                drawCheckDisc(cx, cy, BADGE / 2, SDL_Color{ok.r, ok.g, ok.b, 50}, ok);
                break;
            case DialogIcon::Info:
                fillDisc(cx, cy, BADGE / 2, SDL_Color{info.r, info.g, info.b, 50});
                drawTextCentered("i", cx, cy, info, uiFont(22, true));
                break;
        }
    }

    // Title and body, centred on the badge when they are shorter than it.
    int ty = y + DLG_PAD + std::max(0, (BADGE - titleH - bodyH) / 2);
    for (const auto& l : titleLines) { drawText(l, x + textX, ty, T().text, fTitle); ty += 28; }
    if (!bodyLines.empty()) ty += 6;
    for (const auto& l : bodyLines) { drawText(l, x + textX, ty, T().textDim, fBody); ty += 21; }
    int cy = y + DLG_PAD + headH;

    if (objectH > 0) {
        cy += 18;
        fillRounded(x + DLG_PAD, cy, innerW, objectH, 12, T().bg);
        if (drawObject) drawObject(x + DLG_PAD, cy, innerW);
        cy += objectH;
    }

    if (!noteLines.empty()) {
        cy += 16;
        fillDisc(x + DLG_PAD + 4, cy + 10, 4, T().red);
        for (const auto& l : noteLines) { drawText(l, x + DLG_PAD + 16, cy, T().red, fNote); cy += 20; }
    }

    // Buttons, sharing the width.
    cy += 20;
    const int n = static_cast<int>(buttons.size());
    constexpr int GAP = 12;
    const int bw = n > 0 ? (innerW - (n - 1) * GAP) / n : innerW;
    for (int i = 0; i < n; i++) {
        const DialogButton& b = buttons[i];
        const int bx = x + DLG_PAD + i * (bw + GAP);
        const SDL_Color red = T().red;
        SDL_Color textC = T().text, keyBg = T().keyCap, keyText = T().keyCapText;
        switch (b.style) {
            case ButtonStyle::Neutral:
                fillRounded(bx, cy, bw, BTN_H, 12, T().buttonBg);
                strokeRounded(bx, cy, bw, BTN_H, 12, 1, T().buttonBorder);
                break;
            case ButtonStyle::Primary:
                fillRounded(bx, cy, bw, BTN_H, 12, T().accent);
                textC = T().keyCapText; keyBg = T().keyCapText; keyText = T().accent;
                break;
            case ButtonStyle::Danger:
                fillRounded(bx, cy, bw, BTN_H, 12, red);
                break;
            case ButtonStyle::Hold: {
                // Dark red at rest; fills with red while A is held.
                fillRounded(bx, cy, bw, BTN_H, 12, SDL_Color{red.r, red.g, red.b, 60});
                strokeRounded(bx, cy, bw, BTN_H, 12, 1, SDL_Color{red.r, red.g, red.b, 140});
                const float p = std::clamp(b.held, 0.0f, 1.0f);
                if (p > 0.0f) {
                    const SDL_Rect clip = {bx, cy, static_cast<int>(bw * p), BTN_H};
                    SDL_RenderSetClipRect(renderer_, &clip);
                    fillRounded(bx, cy, bw, BTN_H, 12, red);
                    SDL_RenderSetClipRect(renderer_, nullptr);
                }
                break;
            }
        }
        const std::string label = fitText(b.label, fBtn, bw - 24 - 12 - 24);
        const int lw = 24 + 10 + textWidth(label, fBtn);
        const int gx = bx + (bw - lw) / 2, gcy = cy + BTN_H / 2;
        fillDisc(gx + 12, gcy, 12, keyBg);
        drawTextCentered(b.key, gx + 12, gcy, keyText, fKey);
        drawText(label, gx + 34, gcy - TTF_FontHeight(fBtn) / 2, textC, fBtn);
    }
    cy += BTN_H;

    if (!footLines.empty()) {
        cy += 14;
        for (const auto& l : footLines) {
            drawTextCentered(l, x + DLG_W / 2, cy + 8, T().textMuted, fFoot);
            cy += 17;
        }
    }
}

// --- Rows for the thing being acted on ---------------------------------------------

void UI::drawBankObjectRow(const BankInfo& bank, int x, int y, int w) {
    constexpr int H = 62;
    const SDL_Color teal = T().accentBank;
    fillRounded(x + 14, y + 13, 36, 36, 9, SDL_Color{teal.r, teal.g, teal.b, 45});
    if (iconBank_) {
        SDL_SetTextureColorMod(iconBank_, teal.r, teal.g, teal.b);
        SDL_Rect dst = {x + 22, y + 21, 20, 20};
        SDL_RenderCopy(renderer_, iconBank_, nullptr, &dst);
        SDL_SetTextureColorMod(iconBank_, 255, 255, 255);
    }
    TTF_Font* fName  = uiFont(16, true);
    TTF_Font* fSub   = uiFont(12);
    TTF_Font* fSlots = uiFont(14, true);
    const int cap = isLGPE(bank.game) ? 1000 : isBDSP(bank.game) ? 1200 : 960;
    const std::string slots = std::to_string(bank.occupiedSlots) + " / " + std::to_string(cap);
    const int sw = textWidth(slots, fSlots);
    drawText(slots, x + w - 16 - sw, y + H / 2 - TTF_FontHeight(fSlots) / 2, T().text, fSlots);
    const int room = w - 64 - 16 - sw - 12;
    drawText(fitText(bank.name, fName, room), x + 64, y + 12, T().text, fName);
    std::string sub = bankGroupNameOf(bank.game);
    if (bank.modified)
        sub += " \xc2\xb7 " + i18n::fmt(StrKey::DlgEdited, relativeDay(bank.modified));
    drawText(fitText(sub, fSub, room), x + 64, y + 35, T().textDim, fSub);
}

void UI::drawPokemonObjectRow(const Pokemon& pkm, const std::string& where, int x, int y, int w) {
    constexpr int H = 62, SPR = 44;
    fillRounded(x + 12, y + 9, SPR, SPR, 10, T().slotFull);
    drawSpriteFit(spriteFor(pkm.species(), pkm.form(), pkm.isShiny(), pkm.isEgg()),
                  x + 12 + SPR / 2, y + 9 + SPR / 2, SPR - 6);

    TTF_Font* fName = uiFont(16, true);
    TTF_Font* fLv   = uiFont(13);
    TTF_Font* fSub  = uiFont(12);
    int nx = x + 12 + SPR + 14;

    // "3 x 31" on the right, as in the info strip.
    int perfect = 0;
    for (int v : {pkm.ivHp(), pkm.ivAtk(), pkm.ivDef(), pkm.ivSpA(), pkm.ivSpD(), pkm.ivSpe()})
        if (v == 31) perfect++;
    TTF_Font* fBadge = uiFont(12, true);
    const std::string badge = i18n::fmt(StrKey::InfoPerfectIvs, std::to_string(perfect));
    const int bw = measureTextTracked(badge, fBadge, 1) + 20;
    const int bx = x + w - 14 - bw;
    fillRounded(bx, y + H / 2 - 11, bw, 22, 11, T().badgeBg);
    drawTextTracked(badge, bx + 10, y + H / 2 - TTF_FontHeight(fBadge) / 2, T().accent, fBadge, 1);
    const int right = bx - 10;

    if (pkm.isShiny() && iconShiny_) {
        SDL_SetTextureColorMod(iconShiny_, T().accent.r, T().accent.g, T().accent.b);
        SDL_Rect dst = {nx, y + 14, 14, 14};
        SDL_RenderCopy(renderer_, iconShiny_, nullptr, &dst);
        SDL_SetTextureColorMod(iconShiny_, 255, 255, 255);
        nx += 20;
    }
    std::string lv, gender;
    SDL_Color gc = T().text;
    if (!pkm.isEgg()) {
        lv = i18n::get(StrKey::LvPrefix) + std::to_string(pkm.level());
        if (pkm.gender() == 0) { gender = "\xe2\x99\x82"; gc = T().genderMale; }
        if (pkm.gender() == 1) { gender = "\xe2\x99\x80"; gc = T().genderFemale; }
    }
    const int tail = (gender.empty() ? 0 : textWidth(gender, fName) + 6) + (lv.empty() ? 0 : textWidth(lv, fLv) + 6);
    const std::string name = fitText(pkm.displayName(), fName, right - nx - tail);
    drawText(name, nx, y + 10, T().text, fName);
    int ex = nx + textWidth(name, fName) + 6;
    if (!gender.empty()) { drawText(gender, ex, y + 10, gc, fName); ex += textWidth(gender, fName) + 6; }
    if (!lv.empty()) drawText(lv, ex, y + 13, T().textDim, fLv);

    std::string sub = where;
    if (!pkm.otName().empty()) sub += (sub.empty() ? "" : " \xc2\xb7 ") + i18n::fmt(StrKey::InfoOt, pkm.otName(), std::to_string(pkm.displayTid()));
    drawText(fitText(sub, fSub, right - (x + 12 + SPR + 14)), x + 12 + SPR + 14, y + 36, T().textDim, fSub);
}

// --- Modal message and confirmation ------------------------------------------------

void UI::showMessageAndWait(const std::string& title, const std::string& body, DialogKind kind) {
    if (!renderer_) return;
    markDirty(); // Force redraw after modal returns

    const DialogIcon icon = kind == DialogKind::Success ? DialogIcon::Success
                          : kind == DialogKind::Info    ? DialogIcon::Info
                                                        : DialogIcon::Warning;
    const std::vector<DialogButton> buttons = {{"A", i18n::get(StrKey::DlgOk)}};

    bool waiting = true, redraw = true;
    while (waiting) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) waiting = false;
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                // A says OK; B still dismisses, as it always has.
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B ||   // Switch A
                    event.cbutton.button == SDL_CONTROLLER_BUTTON_A)     // Switch B
                    waiting = false;
            }
        }
        // Drawn once: nothing in it moves.
        if (redraw && waiting) {
            drawDialogBackdrop();
            drawDialog(icon, title, body, 0, nullptr, std::string(), buttons, std::string());
            SDL_RenderPresent(renderer_);
            redraw = false;
        }
        SDL_Delay(16);
    }
}

bool UI::showConfirmDialog(const std::string& title, const std::string& body,
                           const ConfirmStyle& style) {
    if (!renderer_) return false;
    markDirty(); // Force redraw after modal returns

    const DialogIcon icon = style.danger ? DialogIcon::Trash
                          : style.warning ? DialogIcon::Warning : DialogIcon::Info;
    const std::string confirm = i18n::get(style.confirmKey ? style.confirmKey : StrKey::DlgConfirm);

    int result = -1;
    bool redraw = true;
    uint32_t holdStart = 0;   // A held since, when style.hold
    while (result < 0) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) result = 0;
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                redraw = true;
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A)        // Switch B = cancel
                    result = 0;
                else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) { // Switch A = confirm
                    if (style.hold) holdStart = SDL_GetTicks();
                    else            result = 1;
                }
            }
            // Letting go early cancels the hold, not the dialog.
            if (event.type == SDL_CONTROLLERBUTTONUP &&
                event.cbutton.button == SDL_CONTROLLER_BUTTON_B && holdStart) {
                holdStart = 0;
                redraw = true;
            }
        }

        float held = 0.0f;
        if (holdStart) {
            held = static_cast<float>(SDL_GetTicks() - holdStart) / HOLD_MS;
            if (held >= 1.0f) result = 1;
            redraw = true;   // the fill moves every frame while held
        }

        if (redraw && result < 0) {
            std::vector<DialogButton> buttons = {{"B", i18n::get(StrKey::HintCancel)}};
            if (style.hold)
                buttons.push_back({"A", i18n::get(holdStart ? StrKey::DlgKeepHolding : style.confirmKey
                                                  ? style.confirmKey : StrKey::DlgHoldDelete),
                                   ButtonStyle::Hold, held});
            else
                buttons.push_back({"A", confirm,
                                   style.danger ? ButtonStyle::Danger : ButtonStyle::Primary});
            drawDialogBackdrop();
            drawDialog(icon, title, body, style.objectH, style.drawObject, style.note, buttons,
                       style.hold ? i18n::get(StrKey::DlgHoldHint) : std::string());
            SDL_RenderPresent(renderer_);
            redraw = false;
        }
        SDL_Delay(16);
    }
    return result == 1;
}
