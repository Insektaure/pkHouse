#include "met_info.h"
#include <cstdlib>
#include <fstream>
#include <unordered_map>

// --- Met locations -----------------------------------------------------------

static std::unordered_map<uint16_t, std::string> s_locations;
static std::string s_loadedFamily;
static const std::string s_empty;

static void ensureLoaded(GameType game) {
    const std::string family = gameInfo(game).bankFolderName;
    if (family == s_loadedFamily)
        return;

    s_locations.clear();
    s_loadedFamily = family;

    std::ifstream file("romfs:/data/locations_" + family + "_en.txt");
    if (!file.is_open())
        return;

    // Each line is "<id>\t<name>".
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        size_t tab = line.find('\t');
        if (tab == std::string::npos || tab + 1 >= line.size())
            continue;
        unsigned long id = std::strtoul(line.c_str(), nullptr, 10);
        if (id > 0xFFFF)
            continue;
        s_locations.emplace(static_cast<uint16_t>(id), line.substr(tab + 1));
    }
}

const std::string& LocationName::get(GameType game, uint16_t locationId) {
    ensureLoaded(game);
    auto it = s_locations.find(locationId);
    return it != s_locations.end() ? it->second : s_empty;
}

// --- Origin games ------------------------------------------------------------

// Values are PKHeX GameVersion (PKHeX.Core/Game/Enums/GameVersion.cs).
const char* VersionName::get(uint8_t version) {
    switch (version) {
        case 1:  return "Sapphire";
        case 2:  return "Ruby";
        case 3:  return "Emerald";
        case 4:  return "FireRed";
        case 5:  return "LeafGreen";
        case 7:  return "HeartGold";
        case 8:  return "SoulSilver";
        case 10: return "Diamond";
        case 11: return "Pearl";
        case 12: return "Platinum";
        case 15: return "Colosseum/XD";
        case 16: return "Battle Revolution";
        case 20: return "White";
        case 21: return "Black";
        case 22: return "White 2";
        case 23: return "Black 2";
        case 24: return "X";
        case 25: return "Y";
        case 26: return "Alpha Sapphire";
        case 27: return "Omega Ruby";
        case 30: return "Sun";
        case 31: return "Moon";
        case 32: return "Ultra Sun";
        case 33: return "Ultra Moon";
        case 34: return "GO";
        case 35: return "Red";
        case 36: return "Green";
        case 37: return "Blue";
        case 38: return "Yellow";
        case 39: return "Gold";
        case 40: return "Silver";
        case 41: return "Crystal";
        case 42: return "Let's Go Pikachu";
        case 43: return "Let's Go Eevee";
        case 44: return "Sword";
        case 45: return "Shield";
        case 47: return "Legends: Arceus";
        case 48: return "Brilliant Diamond";
        case 49: return "Shining Pearl";
        case 50: return "Scarlet";
        case 51: return "Violet";
        case 52: return "Legends: Z-A";
        default: return "";
    }
}
