// Sort & tidy (menu +, with the cursor on a bank): one box or the whole bank,
// in one of BankSort's orders. The sort happens in memory; the banks are only
// written when saved, so leaving without saving undoes it.

#include "ui.h"
#include "ui_util.h"
#include "i18n.h"
#include "bank_sort.h"

#include <algorithm>

namespace {

constexpr int SD_W = 700, SD_H = 640, SD_PAD = 32;
constexpr int SD_HEAD = 84, SD_SCOPE_H = 56, SD_ROW_H = 44, SD_FOOT = 58;

// Rows: 0 is the scope, then one per order.
constexpr const char* ORDER_KEYS[BankSort::ORDER_COUNT] = {
    StrKey::SortOrderDex, StrKey::SortOrderNational, StrKey::SortOrderName, StrKey::SortOrderShiny, StrKey::SortOrderLevel,
    StrKey::SortOrderIvs, StrKey::SortOrderCompact, StrKey::SortOrderLiving,
};

} // anonymous namespace

bool UI::canSortHere() const {
    if (holding_) return false;
    if (cursor_.panel == Panel::Bank) return !activeBankName_.empty();
    return isDualBankMode() && cursor_.panel == Panel::Game && !leftBankName_.empty();
}

Bank* UI::sortTargetBank() {
    if (!canSortHere()) return nullptr;
    return cursor_.panel == Panel::Bank ? &bank_ : &bankLeft_;
}

void UI::showSortDialog() {
    Bank* bank = sortTargetBank();
    if (!bank || !renderer_) return;
    const Panel panel = cursor_.panel;
    const int box = panel == Panel::Game ? gameBox_ : bankBox_;
    const std::string bankName = panel == Panel::Bank ? activeBankName_ : leftBankName_;
    bool anyNamed = false;
    for (int b = 0; b < bank->boxCount(); b++) anyNamed = anyNamed || bank->hasBoxName(b);

    int row = 1;            // 0 = scope, 1.. = orders
    int order = 0;          // the order the cursor last rested on
    bool whole = false;
    const int rows = 1 + BankSort::ORDER_COUNT;
    const int living = static_cast<int>(BankSort::Order::LivingDex);
    markDirty();
    updateStick(0, 0);

    auto draw = [&]() {
        drawDialogBackdrop();
        const int x = (SCREEN_W - SD_W) / 2, y = (SCREEN_H - SD_H) / 2;
        fillRounded(x, y, SD_W, SD_H, 20, T().panelBg);
        strokeRounded(x, y, SD_W, SD_H, 20, 1, T().panelBorder);
        const int inX = x + SD_PAD, inW = SD_W - 2 * SD_PAD;

        TTF_Font* fTitle = uiFont(22, true);
        TTF_Font* fSub   = uiFont(14);
        drawText(i18n::get(StrKey::SortTitle), inX, y + 22, T().text, fTitle);
        drawText(fitText(bankName, fSub, inW), inX, y + 52, T().textDim, fSub);
        drawRect(x, y + SD_HEAD - 8, SD_W, 1, T().panelBorder);

        // Scope: two halves; a Living Dex is always the whole bank.
        const bool forcedWhole = order == living;
        const bool isWhole = whole || forcedWhole;
        int ry = y + SD_HEAD + 4;
        {
            const bool on = row == 0;
            if (on) fillRounded(inX - 8, ry, inW + 16, SD_SCOPE_H - 8, 10, T().slotFull);
            TTF_Font* f = uiFont(16, true);
            const int half = (inW - 12) / 2, h = 36, cy = ry + (SD_SCOPE_H - 8) / 2;
            const std::string labels[2] = {
                i18n::fmt(StrKey::SortScopeBox, bank->getBoxName(box)), i18n::get(StrKey::SortScopeBank)};
            for (int k = 0; k < 2; k++) {
                const int bx = inX + k * (half + 12);
                const bool chosen = (k == 1) == isWhole;
                const bool dim = forcedWhole && k == 0;
                fillRounded(bx, cy - h / 2, half, h, 10, chosen ? T().accent : T().buttonBg);
                if (!chosen) strokeRounded(bx, cy - h / 2, half, h, 10, 1, T().buttonBorder);
                const SDL_Color c = chosen ? T().keyCapText : dim ? T().textMuted : T().text;
                drawTextCentered(fitText(labels[k], f, half - 20), bx + half / 2, cy, c, f);
            }
            ry += SD_SCOPE_H;
        }

        // Orders, as radio rows.
        TTF_Font* fRow = uiFont(17);
        for (int k = 0; k < BankSort::ORDER_COUNT; k++, ry += SD_ROW_H) {
            const bool on = row == k + 1;
            if (on) fillRounded(inX - 8, ry + 2, inW + 16, SD_ROW_H - 4, 10, T().slotFull);
            const int cy = ry + SD_ROW_H / 2;
            strokeRounded(inX + 4, cy - 10, 20, 20, 10, 2, k == order ? T().accent : T().buttonBorder);
            if (k == order) fillDisc(inX + 14, cy, 5, T().accent);
            drawText(i18n::get(ORDER_KEYS[k]), inX + 38, cy - TTF_FontHeight(fRow) / 2, T().text, fRow);
        }

        // What to know about the choice.
        std::string note;
        if (order == living)        note = i18n::get(StrKey::SortNoteLiving);
        else if (isWhole && anyNamed) note = i18n::get(StrKey::SortNoteNames);
        if (!note.empty()) {
            TTF_Font* f = uiFont(14);
            int ny = ry + 8;
            for (const auto& l : wrapText(note, f, inW, 2)) {
                drawText(l, inX, ny, T().textDim, f);
                ny += 19;
            }
        }

        // Keys.
        const int fy = y + SD_H - SD_FOOT;
        drawRect(x, fy, SD_W, 1, T().panelBorder);
        TTF_Font* fK = uiFont(15, true);
        const int kcy = fy + SD_FOOT / 2;
        int kx = inX;
        auto key = [&](const char* k, const std::string& label, SDL_Color c) {
            kx += drawFooterKey(kx, kcy, k, false) + 8;
            drawText(label, kx, kcy - TTF_FontHeight(fK) / 2, c, fK);
            kx += textWidth(label, fK) + 24;
        };
        key("A", i18n::get(StrKey::SortGo), T().accent);
        key("B", i18n::get(StrKey::HintCancel), T().text);
        key("L R", i18n::get(StrKey::SortScopeHint), T().textDim);   // works from any row
        SDL_RenderPresent(renderer_);
    };

    auto moveRow = [&](int d) {
        row = std::clamp(row + d, 0, rows - 1);
        if (row > 0) order = row - 1;
    };

    bool open = true, go = false, redraw = true;
    while (open) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { open = false; break; }
            if (event.type == SDL_CONTROLLERAXISMOTION &&
                (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                 event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)) {
                updateStick(SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX),
                            SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY));
                continue;
            }
            if (event.type != SDL_CONTROLLERBUTTONDOWN) continue;
            redraw = true;
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:   moveRow(-1); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: moveRow(+1); break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                    whole = !whole;   // the scope, from any row
                    break;
                case SDL_CONTROLLER_BUTTON_B:          // Switch A = sort
                    go = true;
                    open = false;
                    break;
                case SDL_CONTROLLER_BUTTON_A:          // Switch B = cancel
                    open = false;
                    break;
            }
        }
        // Stick: up / down between rows, repeating while held.
        if (open && stickDirY_ != 0) {
            const uint32_t now = SDL_GetTicks();
            const uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
            if (now - stickMoveTime_ >= delay) {
                moveRow(stickDirY_);
                stickMoveTime_ = now;
                stickMoved_ = true;
                redraw = true;
            }
        }
        if (redraw && open) { draw(); redraw = false; }
        SDL_Delay(16);
    }
    updateStick(0, 0);
    markDirty();
    if (!go) return;

    const BankSort::Order chosen = static_cast<BankSort::Order>(order);
    const BankSort::Result r = BankSort::sort(*bank, bank->gameType(), chosen,
                                              (whole || order == living) ? -1 : box);
    if (!r.done) {
        if (r.needed > r.capacity && r.capacity > 0) {
            const int gaps = r.species - r.placed;
            showMessageAndWait(i18n::get(StrKey::SortFullTitle),
                               i18n::fmt(StrKey::SortFullBody,
                                         {std::to_string(r.needed), std::to_string(r.pokemon),
                                          std::to_string(gaps), std::to_string(r.capacity),
                                          std::to_string(r.needed - r.capacity)}));
        } else {
            showMessageAndWait(i18n::get(StrKey::SortTitle), i18n::get(StrKey::SortNothing), DialogKind::Info);
        }
        return;
    }

    // Moved: every box of that panel is drawn again, and nothing points at a
    // slot that may now hold another Pokemon.
    invalidateAllSlotDisplays();
    clearSearchHighlight();
    selectedSlots_.clear();
    unsavedChanges_ = true;

    std::string body = i18n::fmt(StrKey::SortDoneBody, std::to_string(r.pokemon));
    if (chosen == BankSort::Order::LivingDex)
        body += "\n" + i18n::fmt(StrKey::SortDoneLiving, {std::to_string(r.placed), std::to_string(r.species),
                                                           std::to_string(r.species - r.placed)});
    showMessageAndWait(i18n::get(StrKey::SortDoneTitle), body, DialogKind::Success);
}
