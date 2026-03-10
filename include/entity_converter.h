#pragma once
#include "pokemon.h"
#include "game_type.h"
#include <cstdint>
#include <string>

// Cross-generation Pokemon transfer with legality enforcement.
// Ported from PKHeX.Core EntityConverter + TransferVerifier.

enum class ConvertResult {
    Success,
    IncompatibleSpecies,   // Species not present in destination game
    IncompatibleForm,      // Form not valid in destination game
    NoTransferRoute,       // No conversion path exists
};

namespace EntityConverter {

    // Check if a Pokemon can be transferred to the destination game.
    ConvertResult canTransfer(const Pokemon& src, GameType srcGame, GameType destGame);

    // Convert a Pokemon from srcGame format to destGame format.
    // Only call if canTransfer() returned Success.
    // Returns a new Pokemon with data in the destination format.
    Pokemon convert(const Pokemon& src, GameType srcGame, GameType destGame);

    // Convert a Pokemon to the universal bank canonical format (PA9).
    // srcGame is the game the Pokemon originates from.
    Pokemon toCanonical(const Pokemon& src, GameType srcGame);

    // Convert a Pokemon from canonical format (PA9) to a target game format.
    Pokemon fromCanonical(const Pokemon& src, GameType destGame);

    // Human-readable reason for a failed conversion.
    const char* resultString(ConvertResult r);

    // Check species+form presence in a specific game.
    bool isPresentInGame(GameType game, uint16_t species, uint8_t form);

} // namespace EntityConverter
