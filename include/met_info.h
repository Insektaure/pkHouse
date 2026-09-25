#pragma once
#include "game_type.h"
#include <cstdint>
#include <string>

// Met location names, loaded lazily per game family from
// romfs:/data/locations_<bankFolderName>_en.txt (see tools/gen_locations.py).
namespace LocationName {

    // Returns the location name, or "" when the id is unknown or the table is
    // missing. Loading is lazy and cached; switching game family reloads.
    const std::string& get(GameType game, uint16_t locationId);

} // namespace LocationName

// Origin game names, keyed by the PKHeX GameVersion value stored on the
// Pokemon (see Pokemon::originVersion).
namespace VersionName {

    // Returns the game's display name, or "" when the value is unknown.
    const char* get(uint8_t version);

} // namespace VersionName

// Poke Ball names, keyed by the PKHeX Ball value stored on the Pokemon
// (see Pokemon::ball).
namespace BallName {

    // Returns the ball's display name, or "" when the value is unknown.
    const char* get(uint8_t ball);

} // namespace BallName

// Upper-case English type names, indexed by PKHeX MoveType (0 Normal .. 17
// Fairy). Used as small labels, which is why they are not translated.
namespace TypeName {
    const char* get(uint8_t type);   // "" when out of range
}

// Short names of the five stats a nature can raise or lower, in nature order
// (a nature's id / 5 is the stat it raises, id % 5 the one it lowers).
namespace NatureStat {
    const char* get(int index);      // "Atk", "Def", "Spe", "SpA", "SpD"
}

// Pokemon language byte -> three-letter tag ("ENG", "JPN", ...).
namespace LanguageTag {
    const char* get(uint8_t lang);   // "---" when unknown
}
