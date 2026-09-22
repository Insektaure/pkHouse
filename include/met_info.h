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
