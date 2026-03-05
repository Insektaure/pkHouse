#pragma once
#include "pokemon.h"
#include "game_type.h"
#include <cstdint>

// Cross-generation Pokemon format conversion.
// Routes all conversions through PK8 as the hub format.
//
// Transfer rules (matching official Pokemon HOME):
//   Gen8+ (SwSh, BDSP, PLA, SV, ZA): Bidirectional
//   LGPE: One-way forward to Gen8+ only
//   FRLG: Not supported for cross-gen
namespace PKConvert {

    // Router: convert src Pokemon to target game format.
    // Returns empty Pokemon (isEmpty()==true) if conversion impossible.
    Pokemon convertTo(const Pokemon& src, GameType target);

    // Validation
    bool canTransfer(const Pokemon& src, GameType target);
    bool isOneWayTransfer(GameType src, GameType target); // LGPE→Gen8+

    // Core conversions
    Pokemon convertPB7toPK8(const Pokemon& src);
    Pokemon convertPK8toPK9(const Pokemon& src);
    Pokemon convertPK9toPK8(const Pokemon& src);
    Pokemon convertPK8toPA8(const Pokemon& src);
    Pokemon convertPA8toPK8(const Pokemon& src);

    // Trivial adaptations (same-gen format family, validate species)
    Pokemon adaptPK8toPB8(const Pokemon& src);
    Pokemon adaptPB8toPK8(const Pokemon& src);
    Pokemon adaptPK9toPA9(const Pokemon& src);
    Pokemon adaptPA9toPK9(const Pokemon& src);

    // HOME Tracker
    uint64_t generateTracker();

} // namespace PKConvert
