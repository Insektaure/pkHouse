#pragma once
#include <cstdint>

// Supported game types (sequential enum used as array index)
enum class GameType { ZA, S, V, Sw, Sh, BD, SP, LA, GP, GE, FR, LG };
static constexpr int GAME_TYPE_COUNT = 12;

inline bool isSV(GameType g) { return g == GameType::S || g == GameType::V; }
inline bool isSwSh(GameType g) { return g == GameType::Sw || g == GameType::Sh; }
inline bool isBDSP(GameType g) { return g == GameType::BD || g == GameType::SP; }
inline bool isLGPE(GameType g) { return g == GameType::GP || g == GameType::GE; }
inline bool isFRLG(GameType g) { return g == GameType::FR || g == GameType::LG; }

// Per-game constant table. One entry per GameType enum value.
struct GameInfo {
    uint64_t    titleId;
    const char* saveFileName;
    const char* displayName;
    const char* bankGroupName;
    const char* bankFolderName;
    const char* gamePathName;
    const char* pkExtension;
    int         pkPartySize;
    int         boxCount;
    int         slotsPerBox;
    int         saveSlotSize;   // slot size in save file (stored + gap)
    int         saveGapSize;    // gap bytes at end of each save slot
    int         bankSlotSize;   // slot size in bank file (decrypted)
    bool        hasWondercards;
    bool        hasAlphaForms;
    const char* wcExtensionHint;
    const char* gameTag;        // short tag for export filenames
};

// Lookup by GameType (direct array index).
inline const GameInfo& gameInfo(GameType g) {
    static constexpr GameInfo INFO[GAME_TYPE_COUNT] = {
        // ZA
        {0x0100F43008C44000, "main",            "Pokemon Legends: Z-A",          "Legends: Z-A",
         "LegendsZA",        "LegendsZA",        "pa9", 0x158, 32, 30, 0x198, 0x40, 0x158,
         true, true, ".wa9", "ZA"},
        // S
        {0x0100A3D008C5C000, "main",            "Pokemon Scarlet",               "Scarlet / Violet",
         "ScarletViolet",    "Scarlet",           "pk9", 0x158, 32, 30, 0x158, 0, 0x158,
         true, false, ".wc9", "SV"},
        // V
        {0x01008F6008C5E000, "main",            "Pokemon Violet",                "Scarlet / Violet",
         "ScarletViolet",    "Violet",            "pk9", 0x158, 32, 30, 0x158, 0, 0x158,
         true, false, ".wc9", "SV"},
        // Sw
        {0x0100ABF008968000, "main",            "Pokemon Sword",                 "Sword / Shield",
         "SwordShield",      "Sword",             "pk8", 0x158, 32, 30, 0x158, 0, 0x158,
         true, false, ".wc8", "SwSh"},
        // Sh
        {0x01008DB008C2C000, "main",            "Pokemon Shield",                "Sword / Shield",
         "SwordShield",      "Shield",            "pk8", 0x158, 32, 30, 0x158, 0, 0x158,
         true, false, ".wc8", "SwSh"},
        // BD
        {0x0100000011D90000, "SaveData.bin",    "Pokemon Brilliant Diamond",     "Brilliant Diamond / Shining Pearl",
         "BDSP",             "BrilliantDiamond",  "pb8", 0x158, 40, 30, 0x158, 0, 0x158,
         true, false, ".wb8", "BDSP"},
        // SP
        {0x010018E011D92000, "SaveData.bin",    "Pokemon Shining Pearl",         "Brilliant Diamond / Shining Pearl",
         "BDSP",             "ShiningPearl",      "pb8", 0x158, 40, 30, 0x158, 0, 0x158,
         true, false, ".wb8", "BDSP"},
        // LA
        {0x01001F5010DFA000, "main",            "Pokemon Legends: Arceus",       "Legends: Arceus",
         "LegendsArceus",    "LegendsArceus",     "pa8", 0x178, 32, 30, 0x168, 0, 0x178,
         true, true, ".wa8", "LA"},
        // GP
        {0x010003F003A34000, "savedata.bin",    "Pokemon Let's Go Pikachu",      "Let's Go Pikachu / Let's Go Eevee",
         "LetsGo",           "LetsGoPikachu",     "pb7", 0x104, 40, 25, 0x104, 0, 0x104,
         true, false, ".wb7/.wb7full", "LGPE"},
        // GE
        {0x0100187003A36000, "savedata.bin",    "Pokemon Let's Go Eevee",        "Let's Go Pikachu / Let's Go Eevee",
         "LetsGo",           "LetsGoEevee",       "pb7", 0x104, 40, 25, 0x104, 0, 0x104,
         true, false, ".wb7/.wb7full", "LGPE"},
        // FR
        {0x0100554023408000, "FireRed_e.sav",   "Pokemon FireRed",               "FireRed / LeafGreen",
         "FireRedLeafGreen", "FireRed",           "pk3", 100,   14, 30, 80,    0, 80,
         false, false, "", "FRLG"},
        // LG
        {0x010034D02340E000, "LeafGreen_e.sav", "Pokemon LeafGreen",             "FireRed / LeafGreen",
         "FireRedLeafGreen", "LeafGreen",         "pk3", 100,   14, 30, 80,    0, 80,
         false, false, "", "FRLG"},
    };
    return INFO[static_cast<int>(g)];
}

// Returns the paired game (same bank folder), or the game itself if unpaired
inline GameType pairedGame(GameType g) {
    switch (g) {
        case GameType::S:  return GameType::V;
        case GameType::V:  return GameType::S;
        case GameType::Sw: return GameType::Sh;
        case GameType::Sh: return GameType::Sw;
        case GameType::BD: return GameType::SP;
        case GameType::SP: return GameType::BD;
        case GameType::GP: return GameType::GE;
        case GameType::GE: return GameType::GP;
        case GameType::FR: return GameType::LG;
        case GameType::LG: return GameType::FR;
        default: return g;
    }
}

// Convenience accessors (thin wrappers so existing callers don't change)
inline uint64_t    titleIdOf(GameType g)        { return gameInfo(g).titleId; }
inline const char* saveFileNameOf(GameType g)   { return gameInfo(g).saveFileName; }
inline const char* gameDisplayNameOf(GameType g){ return gameInfo(g).displayName; }
inline const char* bankGroupNameOf(GameType g)  { return gameInfo(g).bankGroupName; }
inline const char* bankFolderNameOf(GameType g) { return gameInfo(g).bankFolderName; }
inline const char* gamePathNameOf(GameType g)   { return gameInfo(g).gamePathName; }
inline const char* pkFileExtension(GameType g)  { return gameInfo(g).pkExtension; }
inline int         pkPartySize(GameType g)      { return gameInfo(g).pkPartySize; }
