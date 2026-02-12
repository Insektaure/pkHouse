#pragma once
#include <cstdint>

// Supported game types
enum class GameType { ZA, S, V, Sw, Sh, BD, SP, LA, GP, GE };

inline bool isSV(GameType g) { return g == GameType::S || g == GameType::V; }
inline bool isSwSh(GameType g) { return g == GameType::Sw || g == GameType::Sh; }
inline bool isBDSP(GameType g) { return g == GameType::BD || g == GameType::SP; }
inline bool isLGPE(GameType g) { return g == GameType::GP || g == GameType::GE; }

// Switch Title IDs (for dynamic save loading)
inline uint64_t titleIdOf(GameType g) {
    switch (g) {
        case GameType::Sw: return 0x0100ABF008968000;
        case GameType::Sh: return 0x01008DB008C2C000;
        case GameType::BD: return 0x0100000011D90000;
        case GameType::SP: return 0x010018E011D92000;
        case GameType::LA: return 0x01001F5010DFA000;
        case GameType::S:  return 0x0100A3D008C5C000;
        case GameType::V:  return 0x01008F6008C5E000;
        case GameType::ZA: return 0x0100F43008C44000;
        case GameType::GP: return 0x010003F003A34000;
        case GameType::GE: return 0x0100187003A36000;
    }
    return 0;
}

// Save file name per game (for dynamic save loading)
inline const char* saveFileNameOf(GameType g) {
    if (isBDSP(g)) return "SaveData.bin";
    if (isLGPE(g)) return "savedata.bin";
    return "main"; // SCBlock games and LA all use "main"
}

// Display name for UI
inline const char* gameDisplayNameOf(GameType g) {
    switch (g) {
        case GameType::Sw: return "Pokemon Sword";
        case GameType::Sh: return "Pokemon Shield";
        case GameType::BD: return "Pokemon Brilliant Diamond";
        case GameType::SP: return "Pokemon Shining Pearl";
        case GameType::LA: return "Pokemon Legends: Arceus";
        case GameType::S:  return "Pokemon Scarlet";
        case GameType::V:  return "Pokemon Violet";
        case GameType::ZA: return "Pokemon Legends: Z-A";
        case GameType::GP: return "Pokemon Let's Go Pikachu";
        case GameType::GE: return "Pokemon Let's Go Eevee";
    }
    return "";
}

// Shared bank folder name (paired games share a folder)
inline const char* bankFolderNameOf(GameType g) {
    if (isSwSh(g)) return "SwordShield";
    if (isBDSP(g)) return "BDSP";
    if (isSV(g))   return "ScarletViolet";
    if (g == GameType::LA) return "LegendsArceus";
    if (isLGPE(g)) return "LetsGo";
    return "LegendsZA";
}

// Filesystem-safe game name for backup paths
inline const char* gamePathNameOf(GameType g) {
    switch (g) {
        case GameType::Sw: return "Sword";
        case GameType::Sh: return "Shield";
        case GameType::BD: return "BrilliantDiamond";
        case GameType::SP: return "ShiningPearl";
        case GameType::LA: return "LegendsArceus";
        case GameType::S:  return "Scarlet";
        case GameType::V:  return "Violet";
        case GameType::ZA: return "LegendsZA";
        case GameType::GP: return "LetsGoPikachu";
        case GameType::GE: return "LetsGoEevee";
    }
    return "Unknown";
}
