#pragma once
#include "pokemon.h"
#include "game_type.h"
#include <string>

// Cross-generation Pokemon conversion engine.
// Uses PA9 (Gen9/ZA) as canonical hub format.
// Source → toCanonical() → PA9 → fromCanonical() → Destination
namespace CrossGen {

// Check if a Pokemon can be transferred to the destination game.
// Returns empty string on success, or an error message on failure.
std::string canTransfer(const Pokemon& src, GameType srcGame, GameType dstGame);

// Convert a Pokemon from source game format to destination game format.
// Caller should check canTransfer() first.
// Returns the converted Pokemon with gameType_ set to dstGame.
Pokemon convert(const Pokemon& src, GameType srcGame, GameType dstGame);

// Returns a short game tag for display (e.g. "ZA", "Sw", "BD")
const char* gameTag(GameType g);

} // namespace CrossGen
