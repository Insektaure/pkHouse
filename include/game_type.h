#pragma once
#include <cstdint>

// Supported game types
enum class GameType { ZA, S, V, Sw, Sh, BD, SP, LA };

inline bool isSV(GameType g) { return g == GameType::S || g == GameType::V; }
inline bool isSwSh(GameType g) { return g == GameType::Sw || g == GameType::Sh; }
inline bool isBDSP(GameType g) { return g == GameType::BD || g == GameType::SP; }

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
    }
    return 0;
}

// Save file name per game (for dynamic save loading)
inline const char* saveFileNameOf(GameType g) {
    if (isBDSP(g)) return "SaveData.bin";
    return "main";
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
    }
    return "";
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
    }
    return "Unknown";
}
