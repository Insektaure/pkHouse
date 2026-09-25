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

#include <algorithm>
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

// --- List popup --------------------------------------------------------------

void UI::drawCardListPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, T().overlay);

    constexpr int POP_W = 900;
    constexpr int POP_H = 550;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;

    drawRect(popX, popY, POP_W, POP_H, T().panelBg);
    drawRectOutline(popX, popY, POP_W, POP_H, T().cursor, 2);

    std::string title = i18n::fmt(StrKey::CardsTitle, std::to_string(cardList_.size()));
    drawTextCentered(title, popX + POP_W / 2, popY + 22, T().text, font_);

    if (cardList_.empty()) {
        drawTextCentered(i18n::get(StrKey::NoCardsFound), popX + POP_W / 2,
                         popY + POP_H / 2 - 20, T().textDim, font_);
        drawTextCentered(i18n::get(StrKey::PlaceCardsIn), popX + POP_W / 2,
                         popY + POP_H / 2 + 10, T().textDim, fontSmall_);
        std::string path = "sdmc:/switch/pkHouse/cards/"
                         + std::string(bankFolderNameOf(selectedGame_)) + "/";
        drawTextCentered(path, popX + POP_W / 2, popY + POP_H / 2 + 30, T().textDim, fontSmall_);
        drawTextCentered(i18n::get(StrKey::CardListFooter), popX + POP_W / 2,
                         popY + POP_H - 22, T().textDim, fontSmall_);
        return;
    }

    // Left: the file list. Right: what the highlighted card actually contains.
    constexpr int ROW_H  = 36;
    constexpr int LIST_W = 440;
    constexpr int PANE_W = 380;

    int listX = popX + 20;
    int listY = popY + 50;
    int listBottom = popY + POP_H - 40;
    int visibleRows = (listBottom - listY) / ROW_H;

    int maxScroll = std::max(0, static_cast<int>(cardList_.size()) - visibleRows);
    if (cardListScroll_ > maxScroll) cardListScroll_ = maxScroll;

    if (cardListScroll_ > 0)
        drawTextCentered("^", listX + LIST_W / 2, listY - 12, T().arrow, fontSmall_);
    if (cardListScroll_ + visibleRows < static_cast<int>(cardList_.size()))
        drawTextCentered("v", listX + LIST_W / 2, listBottom + 2, T().arrow, fontSmall_);

    for (int i = 0; i < visibleRows && (cardListScroll_ + i) < static_cast<int>(cardList_.size()); i++) {
        int idx = cardListScroll_ + i;
        int rowY = listY + i * ROW_H;

        if (idx == cardListCursor_) {
            drawRect(listX, rowY, LIST_W, ROW_H - 4, T().menuHighlight);
            drawRectOutline(listX, rowY, LIST_W, ROW_H - 4, T().cursor, 2);
        }

        std::string label = cardList_[idx].label;
        if (label.length() > 32) label = label.substr(0, 31) + ".";
        drawText(label, listX + 10, rowY + (ROW_H - 4) / 2 - 9, T().text, font_);
    }

    // Separator, then the decoded summary.
    int paneX = popX + POP_W - 20 - PANE_W;
    drawRect(paneX - 14, listY, 1, listBottom - listY, T().popupBorder);
    drawCardPreviewPane(cardPreview_, cardPreviewIdx_ != cardListCursor_,
                        paneX, popY + 56, PANE_W, POP_H - 56 - 46);

    drawTextCentered(i18n::get(StrKey::CardListFooter), popX + POP_W / 2,
                     popY + POP_H - 22, T().textDim, fontSmall_);
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
    constexpr int ROW_H = 36;
    const int visibleRows = (550 - 40 - 50) / ROW_H;

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
                               i18n::fmt(StrKey::CardMovedBody, moved));
            cardList_ = scanCards(basePath_, selectedGame_);
            cardListCursor_ = 0;
            cardListScroll_ = 0;
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
                                           std::to_string(box + 1), std::to_string(slot + 1)}));
}
