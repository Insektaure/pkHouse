#pragma once
#include "game_type.h"
#include "pokemon.h"
#include <cstdint>
#include <vector>

// Binary payload carried by the QR code on an exported Pokemon card.
//
// The card is the transport: metadata embedded in image files does not survive
// the chat apps and image hosts people actually share through, whereas a QR
// code survives re-encoding, resizing and a photo of a screen. The payload is
// the whole Pokemon, so a card can be decoded straight back into a bank slot.
//
//   Offset  Size  Field
//   ------  ----  ----------------------------------------------------------
//   0       4     Magic "PKHC"
//   4       1     Payload format version (currently 1)
//   5       1     GameType enum value (see game_type.h) - tells the reader
//                 which PKM format the body is in
//   6       2     Body length in bytes, little endian
//   8       2     CRC-16/CCITT-FALSE over the body, little endian
//   10      N     Decrypted party-size Pokemon data, exactly as the .pk export
//                 writes it
//
// Sizes run from 110 bytes (FireRed/LeafGreen) to 386 (Legends: Arceus), which
// fits a version 15 QR code at ECC level M with room to spare.
namespace CardPayload {

constexpr uint8_t MAGIC[4]   = {'P', 'K', 'H', 'C'};
constexpr uint8_t VERSION    = 1;
constexpr int     HEADER_SIZE = 10;
constexpr int     MAX_SIZE   = HEADER_SIZE + PokeCrypto::MAX_PARTY_SIZE;

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no final xor).
uint16_t crc16(const uint8_t* data, size_t len);

// Serialises a Pokemon into the payload above. Returns an empty vector when
// the game type reports no usable party size.
std::vector<uint8_t> build(const Pokemon& pkm, GameType game);

// Why a payload was rejected, in the order the checks run.
enum class Result {
    Ok,
    NotACard,            // magic missing - not a pkHouse card at all
    UnsupportedVersion,  // written by a newer pkHouse
    Truncated,           // length field disagrees with the data
    BadChecksum,         // our CRC-16 over the body
    UnknownGame,         // game byte outside the GameType enum
    BadSize,             // body length is not this format's party size
    WrongGame,           // a card for a different game family
    BadPokemonChecksum,  // the checksum the games themselves store
    NotPresent,          // species/form does not exist in the target game
    Implausible,         // failed the range checks
};

struct Parsed {
    Result   result = Result::NotACard;
    GameType game   = GameType::ZA;  // the card's own game; meaningful once the
                                     // result is UnknownGame or later
    bool     gameKnown = false;      // true once the game byte has been trusted
    Pokemon  pkm;                    // only valid when result == Ok
};

// Parses and validates a payload for the game currently open.
//
// The folder a card sits in is a convenience, not a guarantee: a file dropped
// into the wrong game folder is caught here, because the payload carries the
// family it was exported from and that byte is covered by the CRC.
Parsed parse(const uint8_t* data, size_t len, GameType target);

} // namespace CardPayload
