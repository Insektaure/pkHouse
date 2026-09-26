#pragma once
#include "card_payload.h"
#include "game_type.h"
#include <string>
#include <vector>

// A card PNG sitting on the SD card, described from its filename alone.
// Pulling a QR code out of a 1280x720 image takes a moment, so the browser
// lists files cheaply and only decodes the one the user picks.
struct CardFile {
    std::string filename;  // as it appears on disk
    std::string path;      // full path
    std::string label;     // "Greninja - SV - [S]" when the name parses, else the filename
    // From the "0658-00 - " prefix newer exports carry; without it the
    // species is only known once the card is decoded.
    bool     hasSpecies = false;
    uint16_t species = 0;  // national dex, 0 = egg
    uint8_t  form = 0;
};

// Where an exported card keeps its QR code, in card pixels. The importer
// decodes just this region when an image has a card's proportions, which is
// roughly eight times less work than scanning the whole picture. ui_card.cpp
// static_asserts its own layout against these, so the two cannot drift apart.
namespace CardLayout {
    constexpr int WIDTH   = 1280;
    constexpr int HEIGHT  = 720;
    constexpr int QR_X    = 289;
    constexpr int QR_Y    = 202;
    constexpr int QR_SIZE = 340;
}

// Lists cards/<GameFamily>/*.png for the given game, sorted by name.
std::vector<CardFile> scanCards(const std::string& basePath, GameType game);

// Loads the image, decodes its QR code and validates the payload against the
// game currently open. A file in the wrong game folder is rejected here, not
// by where it happens to sit.
//
// With fastPathOnly, only the QR region of a correctly proportioned card is
// scanned and anything else gives up immediately. The browser uses that while
// you move the cursor; committing to an import scans the whole image.
CardPayload::Parsed decodeCard(const std::string& path, GameType target,
                               bool fastPathOnly = false);
