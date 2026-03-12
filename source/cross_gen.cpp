#include "cross_gen.h"
#include "move_legal.h"
#include "species_converter.h"
#include "personal_za.h"
#include "personal_sv.h"
#include "personal_swsh.h"
#include "personal_bdsp.h"
#include "personal_la.h"
#include "personal_gg.h"
#include <cstring>
#include <algorithm>

namespace CrossGen {

// --- Helpers ---

static uint16_t readU16(const uint8_t* d, int ofs) {
    uint16_t v; std::memcpy(&v, d + ofs, 2); return v;
}
static uint32_t readU32(const uint8_t* d, int ofs) {
    uint32_t v; std::memcpy(&v, d + ofs, 4); return v;
}
static void writeU16(uint8_t* d, int ofs, uint16_t v) {
    std::memcpy(d + ofs, &v, 2);
}
static void writeU32(uint8_t* d, int ofs, uint32_t v) {
    std::memcpy(d + ofs, &v, 4);
}

// Check if species+form is present in a game (base HP > 0 means valid entry)
static bool isPresentInGame(uint16_t natSpecies, uint8_t form, GameType game) {
    if (natSpecies == 0) return false;

    if (game == GameType::ZA) {
        uint16_t intSpec = SpeciesConverter::getInternal9(natSpecies);
        int idx = PersonalZA::getEntryIndex(intSpec, form);
        return PersonalZA::BASE_STATS[idx].hp != 0;
    }
    if (isSV(game)) {
        int idx = PersonalSV::getEntryIndex(natSpecies, form);
        return PersonalSV::BASE_STATS[idx].hp != 0;
    }
    if (isSwSh(game)) {
        int idx = PersonalSWSH::getEntryIndex(natSpecies, form);
        return PersonalSWSH::BASE_STATS[idx].hp != 0;
    }
    if (isBDSP(game)) {
        int idx = PersonalBDSP::getEntryIndex(natSpecies, form);
        return PersonalBDSP::BASE_STATS[idx].hp != 0;
    }
    if (game == GameType::LA) {
        int idx = PersonalLA::getEntryIndex(natSpecies, form);
        return PersonalLA::BASE_STATS[idx].hp != 0;
    }
    if (isLGPE(game)) {
        int idx = PersonalGG::getEntryIndex(natSpecies, form);
        return PersonalGG::BASE_STATS[idx].hp != 0;
    }
    if (isFRLG(game)) {
        return natSpecies <= MoveLegal::MAX_SPECIES_FRLG;
    }
    return false;
}

// Get national species from Pokemon in its native format
static uint16_t getNationalSpecies(const Pokemon& pkm, GameType game) {
    if (isFRLG(game))
        return SpeciesConverter::getNational3(readU16(pkm.data.data(), 0x20));
    if (isSwSh(game) || isBDSP(game) || game == GameType::LA || isLGPE(game))
        return readU16(pkm.data.data(), 0x08); // national directly
    // Gen9 (ZA/SV): internal ID
    return SpeciesConverter::getNational9(readU16(pkm.data.data(), 0x08));
}

// Get form from Pokemon in its native format
static uint8_t getForm(const Pokemon& pkm, GameType game) {
    if (isFRLG(game)) return 0;
    if (isLGPE(game)) return pkm.data[0x1D] >> 3;
    return pkm.data[0x24];
}

// Check Gigantamax factor (PK8 at 0x16 bit 4)
static bool hasGigantamaxFactor(const Pokemon& pkm, GameType game) {
    if (!isSwSh(game) && !isBDSP(game)) return false;
    return (pkm.data[0x16] & 0x10) != 0;
}

// Check Hyper Training flags (0x126 for Gen8+)
static uint8_t hyperTrainFlags(const Pokemon& pkm, GameType game) {
    if (isFRLG(game) || isLGPE(game)) return 0;
    return pkm.data[0x126];
}

// --- canTransfer ---

std::string canTransfer(const Pokemon& src, GameType srcGame, GameType dstGame) {
    if (srcGame == dstGame) return "";
    // Same-family (paired) games: no conversion needed
    if (pairedGame(srcGame) == dstGame) return "";

    uint16_t natSpecies = getNationalSpecies(src, srcGame);
    uint8_t form = getForm(src, srcGame);

    // Species must exist in destination
    if (!isPresentInGame(natSpecies, form, dstGame))
        return "This Pokemon doesn't exist in " + std::string(gameDisplayNameOf(dstGame));

    // BDSP specific blocks
    if (isBDSP(dstGame)) {
        // Spinda always blocked
        if (natSpecies == 327)
            return "Spinda cannot be transferred to BDSP";
        // Nincada blocked unless from BDSP
        if (natSpecies == 290 && !isBDSP(srcGame))
            return "Nincada can only enter BDSP from BDSP";
        // Gigantamax Pikachu/Eevee/Meowth blocked
        if (hasGigantamaxFactor(src, srcGame)) {
            if (natSpecies == 25 || natSpecies == 133 || natSpecies == 52)
                return "Gigantamax Pokemon can't enter BDSP";
        }
        // Hyper Trained non-L100 from SV/ZA
        if ((isSV(srcGame) || srcGame == GameType::ZA) &&
            hyperTrainFlags(src, srcGame) != 0) {
            uint8_t level = src.level();
            if (level < 100)
                return "Hyper Trained Pokemon must be Lv100 to enter BDSP";
        }
    }

    // PLA specific blocks
    if (dstGame == GameType::LA) {
        if (hasGigantamaxFactor(src, srcGame)) {
            if (natSpecies == 25 || natSpecies == 133)
                return "Gigantamax Pokemon can't enter Legends: Arceus";
        }
    }

    // LGPE restrictions
    if (isLGPE(dstGame)) {
        // LGPE can only receive from LGPE (one-way out to SwSh)
        if (!isLGPE(srcGame))
            return "Only LGPE Pokemon can return to Let's Go games";
    }
    if (isLGPE(srcGame) && !isLGPE(dstGame) && !isSwSh(dstGame)) {
        // LGPE can only send to SwSh
        return "Let's Go Pokemon can only transfer to Sword/Shield";
    }

    // LGPE starter forms can't leave
    if (isLGPE(srcGame) && form == 8) {
        if (natSpecies == 25 || natSpecies == 133)
            return "Partner Pikachu/Eevee can't leave Let's Go";
    }

    // FRLG can only hold Gen1-3 species
    if (isFRLG(dstGame) && natSpecies > 386)
        return "Only Gen 1-3 Pokemon can enter FireRed/LeafGreen";

    return "";
}

// --- Format Conversion Functions ---

// Copy common Block A fields (0x00-0x07 header shared across Gen8/9)
static void copyHeader(const uint8_t* src, uint8_t* dst) {
    std::memcpy(dst, src, 8); // EC(4) + reserved(2) + checksum(2)
}

// Sanitize moves for destination game
static void sanitizeMoves(uint8_t* data, int moveOfs, GameType dstGame) {
    uint16_t maxMove = 0;
    const uint8_t* dummied = nullptr;
    int dummiedLen = 0;

    if (isSwSh(dstGame)) {
        maxMove = MoveLegal::MAX_MOVE_SWSH;
        dummied = MoveLegal::DUMMIED_8;
        dummiedLen = MoveLegal::DUMMIED_8_LEN;
    } else if (isBDSP(dstGame)) {
        maxMove = MoveLegal::MAX_MOVE_BDSP;
        dummied = MoveLegal::DUMMIED_8B;
        dummiedLen = MoveLegal::DUMMIED_8B_LEN;
    } else if (dstGame == GameType::LA) {
        maxMove = MoveLegal::MAX_MOVE_LA;
        dummied = MoveLegal::DUMMIED_8A;
        dummiedLen = MoveLegal::DUMMIED_8A_LEN;
    } else if (isSV(dstGame)) {
        maxMove = MoveLegal::MAX_MOVE_SV;
        dummied = MoveLegal::DUMMIED_9;
        dummiedLen = MoveLegal::DUMMIED_9_LEN;
    } else if (dstGame == GameType::ZA) {
        maxMove = MoveLegal::MAX_MOVE_ZA;
        dummied = MoveLegal::DUMMIED_9A;
        dummiedLen = MoveLegal::DUMMIED_9A_LEN;
    } else if (isLGPE(dstGame)) {
        maxMove = MoveLegal::MAX_MOVE_LGPE;
    } else if (isFRLG(dstGame)) {
        maxMove = MoveLegal::MAX_MOVE_FRLG;
    }

    // Clear invalid moves and compact
    uint16_t moves[4] = {};
    int count = 0;
    for (int i = 0; i < 4; i++) {
        uint16_t m = readU16(data, moveOfs + i * 2);
        if (m == 0) continue;
        if (m > maxMove) continue;
        if (dummied && MoveLegal::isDummied(dummied, dummiedLen, m)) continue;
        moves[count++] = m;
    }
    for (int i = 0; i < 4; i++)
        writeU16(data, moveOfs + i * 2, i < count ? moves[i] : 0);
}

// Sanitize held item
static void sanitizeItem(uint8_t* data, int itemOfs, GameType dstGame) {
    uint16_t item = readU16(data, itemOfs);
    uint16_t maxItem = 0;
    if (isSwSh(dstGame))      maxItem = MoveLegal::MAX_ITEM_SWSH;
    else if (isBDSP(dstGame)) maxItem = MoveLegal::MAX_ITEM_BDSP;
    else if (dstGame == GameType::LA) maxItem = MoveLegal::MAX_ITEM_LA;
    else if (isSV(dstGame))   maxItem = MoveLegal::MAX_ITEM_SV;
    else if (dstGame == GameType::ZA) maxItem = MoveLegal::MAX_ITEM_ZA;
    else if (isLGPE(dstGame)) maxItem = MoveLegal::MAX_ITEM_LGPE;
    else if (isFRLG(dstGame)) maxItem = MoveLegal::MAX_ITEM_FRLG;

    if (maxItem > 0 && item > maxItem)
        writeU16(data, itemOfs, 0);
}

// Ensure not fainted: set HP to 1 if 0, clear status condition
static void ensureNotFainted(uint8_t* data, GameType game) {
    if (isFRLG(game)) {
        // PK3 party: Status at 0x50, Level at 0x54, HP at 0x56
        // For stored PK3, party stats aren't present — skip
        return;
    }

    // Status condition: 0x14-0x17 for PB7, handled in convert
    // For Gen8/9: Stat_HPCurrent at party offset
    int hpOfs, statusOfs;
    if (isLGPE(game)) {
        hpOfs = 0xF0;     // Stat_HPCurrent
        statusOfs = 0xE8;  // Status_Condition (unused in LGPE but clear it)
    } else if (game == GameType::LA) {
        hpOfs = 0x92;      // Stat_HPCurrent (PA8)
        statusOfs = -1;
    } else {
        // Gen8/9 (PK8/PB8/PK9/PA9)
        hpOfs = 0x8A;      // Stat_HPCurrent
        statusOfs = -1;
    }

    // Set HP to 1 if it's 0 (prevent fainted state)
    uint16_t hp = readU16(data, hpOfs);
    if (hp == 0)
        writeU16(data, hpOfs, 1);

    if (statusOfs >= 0)
        writeU32(data, statusOfs, 0);
}

// --- PK8/PB8 <-> PA9 conversion (same 0x158 size, Block C differs) ---

static void convertPK8toPA9(const uint8_t* src, uint8_t* dst) {
    // Start with a copy — most fields are at the same offsets
    std::memcpy(dst, src, PokeCrypto::SIZE_9PARTY);

    // Species: PK8 stores national dex, PA9 stores Gen9 internal
    uint16_t natSpec = readU16(src, 0x08);
    writeU16(dst, 0x08, SpeciesConverter::getInternal9(natSpec));

    // Gender bits: PK8 bits 2:3 @ 0x22, PA9 bits 1:2 @ 0x22
    uint8_t genderByte = src[0x22];
    uint8_t gender = (genderByte >> 2) & 3;
    uint8_t fateful = genderByte & 1;
    dst[0x22] = fateful | (gender << 1);

    // Clear IsAlpha (PA9 0x23, unused in PK8 — may contain garbage)
    dst[0x23] = 0;

    // Height/Weight/Scale: PK8 0x50/0x51/0x52 -> PA9 0x48/0x49/0x4A
    // PK8 0x48-0x4B was Sociability (u32) — clear before writing scalars
    dst[0x48] = src[0x50];
    dst[0x49] = src[0x51];
    dst[0x4A] = src[0x52];
    dst[0x4B] = 0; // PA9 LevelBoost — clear PK8 Sociability residue
    dst[0x50] = 0;
    dst[0x51] = 0;
    dst[0x52] = 0;

    // Block C remapping (after 0xCD diverges)
    // Version: 0xDE (PK8) -> 0xCE (PA9)
    dst[0xCE] = src[0xDE];
    // BattleVersion: 0xDF -> 0xCF
    dst[0xCF] = src[0xDF];
    // FormArgument: 0xE4 (4 bytes) -> 0xD0 (4 bytes)
    writeU32(dst, 0xD0, readU32(src, 0xE4));
    // AffixedRibbon: 0xE8 -> 0xD4
    dst[0xD4] = src[0xE8];
    // Language: 0xE2 -> 0xD5
    dst[0xD5] = src[0xE2];

    // Clear Gen8-specific fields
    dst[0x16] &= ~0x10; // CanGigantamax bit

    // Status_Condition: PK8 at 0x94, PA9 at 0x90. Fix offset mismatch.
    writeU32(dst, 0x90, readU32(src, 0x94)); // Move to correct PA9 offset
    std::memset(dst + 0x94, 0, 4);           // Clear PK8 residue in PlusFlags1 area

    // DynamaxLevel: PK8 0x90 was already overwritten above by Status_Condition

    // ObedienceLevel: PA9 0x11F, set to current level
    dst[0x11F] = dst[0x148];

    // Zero out the Block C area that doesn't map (0xD6-0xE8 in PA9 is different)
    // These are fields that don't have a direct PK8 equivalent
    std::memset(dst + 0xD6, 0, 0xF8 - 0xD6);
}

static void convertPA9toPK8(const uint8_t* src, uint8_t* dst) {
    std::memcpy(dst, src, PokeCrypto::SIZE_9PARTY);

    // Species: PA9 Gen9 internal -> PK8 national
    uint16_t intSpec = readU16(src, 0x08);
    writeU16(dst, 0x08, SpeciesConverter::getNational9(intSpec));

    // Gender bits: PA9 bits 1:2 -> PK8 bits 2:3
    uint8_t genderByte = src[0x22];
    uint8_t gender = (genderByte >> 1) & 3;
    uint8_t fateful = genderByte & 1;
    dst[0x22] = fateful | (gender << 2);

    // Height/Weight/Scale: PA9 0x48/0x49/0x4A -> PK8 0x50/0x51/0x52
    dst[0x50] = src[0x48];
    dst[0x51] = src[0x49];
    dst[0x52] = src[0x4A];
    dst[0x48] = 0;
    dst[0x49] = 0;
    dst[0x4A] = 0;

    // Status_Condition: PA9 at 0x90, PK8 at 0x94. Fix offset mismatch.
    writeU32(dst, 0x94, readU32(src, 0x90));
    dst[0x90] = 0; // DynamaxLevel — clear PA9 status residue

    // Clear PK8 PokeJob flags + Fullness/Enjoyment (0xCE-0xDD)
    // After memcpy these contain PA9 Block C data (Version, FormArgument, etc.)
    std::memset(dst + 0xCE, 0, 0xDE - 0xCE);

    // Block C remapping (reverse)
    dst[0xDE] = src[0xCE]; // Version
    dst[0xDF] = src[0xCF]; // BattleVersion
    writeU32(dst, 0xE4, readU32(src, 0xD0)); // FormArgument
    dst[0xE8] = src[0xD4]; // AffixedRibbon
    dst[0xE2] = src[0xD5]; // Language
}

// --- PA8 <-> PA9 (different block sizes: 0x58 vs 0x50) ---

static void convertPA8toPA9(const uint8_t* src, uint8_t* dst) {
    std::memset(dst, 0, PokeCrypto::SIZE_9PARTY);

    // Block A header (shared offsets 0x00-0x07)
    copyHeader(src, dst);

    // Species: PA8 national -> PA9 Gen9 internal
    uint16_t natSpec = readU16(src, 0x08);
    writeU16(dst, 0x08, SpeciesConverter::getInternal9(natSpec));

    // HeldItem: 0x0A
    writeU16(dst, 0x0A, readU16(src, 0x0A));
    // TID/SID: 0x0C-0x0F
    writeU32(dst, 0x0C, readU32(src, 0x0C));
    // EXP: 0x10
    writeU32(dst, 0x10, readU32(src, 0x10));
    // Ability: 0x14
    writeU16(dst, 0x14, readU16(src, 0x14));
    // AbilityNumber + misc: 0x16-0x17
    dst[0x16] = src[0x16];
    dst[0x17] = src[0x17];
    // PID: 0x1C
    writeU32(dst, 0x1C, readU32(src, 0x1C));
    // Nature/StatNature: 0x20-0x21
    dst[0x20] = src[0x20];
    dst[0x21] = src[0x21];

    // Gender/Fateful: PA8 uses bits 2:3 for gender, PA9 uses bits 1:2
    uint8_t pa8Gender = src[0x22];
    uint8_t gender = (pa8Gender >> 2) & 3;
    uint8_t fateful = pa8Gender & 1;
    dst[0x22] = fateful | (gender << 1);

    // IsAlpha: PA8 0x16 bit 5 -> PA9 0x23
    dst[0x23] = ((src[0x16] >> 5) & 1) ? 1 : 0;

    // Form: 0x24
    dst[0x24] = src[0x24];

    // EVs: 0x26-0x2B (same in both)
    std::memcpy(dst + 0x26, src + 0x26, 6);

    // Contest stats: 0x2C-0x31 (same)
    std::memcpy(dst + 0x2C, src + 0x2C, 6);

    // Ribbons: 0x34-0x47 (same bit layout)
    std::memcpy(dst + 0x34, src + 0x34, 0x14);

    // Height/Weight/Scale: PA8 0x50/0x51/0x52 -> PA9 0x48/0x49/0x4A
    dst[0x48] = src[0x50];
    dst[0x49] = src[0x51];
    dst[0x4A] = src[0x52];

    // Moves: PA8 0x54 -> PA9 0x72
    std::memcpy(dst + 0x72, src + 0x54, 8); // 4 moves x 2 bytes
    // Move PP: PA8 0x5C -> PA9 0x7A
    std::memcpy(dst + 0x7A, src + 0x5C, 4);

    // Nickname: PA8 0x60 (26 bytes) -> PA9 0x58 (26 bytes)
    std::memcpy(dst + 0x58, src + 0x60, 26);

    // PP Ups: PA8 0x86 -> PA9 0x7E
    std::memcpy(dst + 0x7E, src + 0x86, 4);

    // Relearn Moves: PA8 0x8A -> PA9 0x82
    std::memcpy(dst + 0x82, src + 0x8A, 8);

    // IV32: PA8 0x94 -> PA9 0x8C
    writeU32(dst, 0x8C, readU32(src, 0x94));

    // HT Name: PA8 0xB8 -> PA9 0xA8
    std::memcpy(dst + 0xA8, src + 0xB8, 26);

    // HT Friendship: PA8 0xD8 -> PA9 0xC8
    dst[0xC8] = src[0xD8];
    // HT Memory: PA8 0xD9-0xDC -> PA9 0xC9-0xCC
    dst[0xC9] = src[0xD9]; // HT_MemoryIntensity
    dst[0xCA] = src[0xDA]; // HT_Memory
    dst[0xCB] = src[0xDB]; // HT_MemoryFeeling
    writeU16(dst, 0xCC, readU16(src, 0xDC)); // HT_MemoryVariable
    // HT Gender: PA8 0xD2 -> PA9 0xC2
    dst[0xC2] = src[0xD2];
    // HT Language: PA8 0xD3 -> PA9 0xC3
    dst[0xC3] = src[0xD3];
    // CurrentHandler: PA8 0xD4 -> PA9 0xC4
    dst[0xC4] = src[0xD4];

    // OT Name: PA8 0x110 -> PA9 0xF8
    std::memcpy(dst + 0xF8, src + 0x110, 26);

    // OT Memory layout (critical mismatch):
    // PA9: [Intensity 0x113][Memory 0x114][TextVar 0x116-0x117][Feeling 0x118]
    // PA8: [Intensity 0x12B][Memory 0x12C][TextVar 0x12E-0x12F][Feeling 0x130]
    dst[0x113] = src[0x12B]; // Intensity
    dst[0x114] = src[0x12C]; // Memory
    writeU16(dst, 0x116, readU16(src, 0x12E)); // TextVar
    dst[0x118] = src[0x130]; // Feeling

    // HyperTrainFlags: PA8 0x13E -> PA9 0x126
    dst[0x126] = src[0x13E];
    // Markings: PA8 0x18 -> PA9 0x18 (same offset)
    writeU16(dst, 0x18, readU16(src, 0x18));
    // Pokerus: PA8 0x32 -> PA9 0x32 (same offset)
    dst[0x32] = src[0x32];

    // Level: PA8 calculates from EXP, PA9 stores at 0x148
    Pokemon tempSrc;
    std::memcpy(tempSrc.data.data(), src, PokeCrypto::SIZE_8APARTY);
    tempSrc.gameType_ = GameType::LA;
    dst[0x148] = tempSrc.level();

    // Ball: PA8 0x137 -> PA9 0x124
    dst[0x124] = src[0x137];

    // OT Friendship: PA8 0x12A -> PA9 0x112
    dst[0x112] = src[0x12A];

    // Egg/Met locations: PA8 0x138/0x13A -> PA9 0x120/0x122
    writeU16(dst, 0x120, readU16(src, 0x138)); // EggLocation
    writeU16(dst, 0x122, readU16(src, 0x13A)); // MetLocation
    // Egg dates: PA8 0x131/0x132/0x133 -> PA9 0x119/0x11A/0x11B
    dst[0x119] = src[0x131]; // EggYear
    dst[0x11A] = src[0x132]; // EggMonth
    dst[0x11B] = src[0x133]; // EggDay

    // Met dates: PA8 0x134/0x135/0x136 -> PA9 0x11C/0x11D/0x11E
    dst[0x11C] = src[0x134]; // MetYear
    dst[0x11D] = src[0x135]; // MetMonth
    dst[0x11E] = src[0x136]; // MetDay

    // Version: PA8 0xEE -> PA9 0xCE
    dst[0xCE] = src[0xEE];
    // BattleVersion: PA8 0xEF -> PA9 0xCF
    dst[0xCF] = src[0xEF];

    // Language: PA8 0xF2 -> PA9 0xD5
    dst[0xD5] = src[0xF2];

    // FormArgument: PA8 0xF4 -> PA9 0xD0
    writeU32(dst, 0xD0, readU32(src, 0xF4));
    // AffixedRibbon: PA8 0xF8 -> PA9 0xD4
    dst[0xD4] = src[0xF8];

    // HT_ID: PA8 0xD6 -> PA9 0xC6
    writeU16(dst, 0xC6, readU16(src, 0xD6));

    // MetLevel/OTGender: PA8 0x13D -> PA9 0x125
    dst[0x125] = src[0x13D];

    // Stat_HPCurrent: PA8 0x92 -> PA9 0x8A
    writeU16(dst, 0x8A, readU16(src, 0x92));

    // ObedienceLevel: set to current level
    dst[0x11F] = dst[0x148];
}

static void convertPA9toPA8(const uint8_t* src, uint8_t* dst) {
    std::memset(dst, 0, PokeCrypto::SIZE_8APARTY);

    copyHeader(src, dst);

    // Species: PA9 Gen9 internal -> PA8 national
    uint16_t intSpec = readU16(src, 0x08);
    writeU16(dst, 0x08, SpeciesConverter::getNational9(intSpec));

    writeU16(dst, 0x0A, readU16(src, 0x0A)); // HeldItem
    writeU32(dst, 0x0C, readU32(src, 0x0C)); // TID/SID
    writeU32(dst, 0x10, readU32(src, 0x10)); // EXP
    writeU16(dst, 0x14, readU16(src, 0x14)); // Ability
    dst[0x16] = src[0x16];
    dst[0x17] = src[0x17];
    writeU32(dst, 0x1C, readU32(src, 0x1C)); // PID
    dst[0x20] = src[0x20]; // Nature
    dst[0x21] = src[0x21]; // StatNature

    // Gender: PA9 bits 1:2 -> PA8 bits 2:3
    uint8_t gender = (src[0x22] >> 1) & 3;
    uint8_t fateful = src[0x22] & 1;
    dst[0x22] = fateful | (gender << 2);

    // IsAlpha: PA9 0x23 -> PA8 0x16 bit 5
    if (src[0x23] != 0)
        dst[0x16] |= (1 << 5);

    dst[0x24] = src[0x24]; // Form
    std::memcpy(dst + 0x26, src + 0x26, 6); // EVs
    std::memcpy(dst + 0x2C, src + 0x2C, 6); // Contest
    std::memcpy(dst + 0x34, src + 0x34, 0x14); // Ribbons

    // Height/Weight/Scale: PA9 0x48/0x49/0x4A -> PA8 0x50/0x51/0x52
    dst[0x50] = src[0x48];
    dst[0x51] = src[0x49];
    dst[0x52] = src[0x4A];

    // Moves: PA9 0x72 -> PA8 0x54
    std::memcpy(dst + 0x54, src + 0x72, 8);
    // Move PP: PA9 0x7A -> PA8 0x5C
    std::memcpy(dst + 0x5C, src + 0x7A, 4);

    // Nickname: PA9 0x58 -> PA8 0x60
    std::memcpy(dst + 0x60, src + 0x58, 26);

    // PP Ups: PA9 0x7E -> PA8 0x86
    std::memcpy(dst + 0x86, src + 0x7E, 4);
    // Relearn: PA9 0x82 -> PA8 0x8A
    std::memcpy(dst + 0x8A, src + 0x82, 8);
    // IV32: PA9 0x8C -> PA8 0x94
    writeU32(dst, 0x94, readU32(src, 0x8C));

    // HT Name: PA9 0xA8 -> PA8 0xB8
    std::memcpy(dst + 0xB8, src + 0xA8, 26);
    // HT Friendship: PA9 0xC8 -> PA8 0xD8
    dst[0xD8] = src[0xC8];
    // HT Memory: PA9 0xC9-0xCC -> PA8 0xD9-0xDC
    dst[0xD9] = src[0xC9]; // HT_MemoryIntensity
    dst[0xDA] = src[0xCA]; // HT_Memory
    dst[0xDB] = src[0xCB]; // HT_MemoryFeeling
    writeU16(dst, 0xDC, readU16(src, 0xCC)); // HT_MemoryVariable
    // HT Gender: PA9 0xC2 -> PA8 0xD2
    dst[0xD2] = src[0xC2];
    // HT Language: PA9 0xC3 -> PA8 0xD3
    dst[0xD3] = src[0xC3];
    // CurrentHandler: PA9 0xC4 -> PA8 0xD4
    dst[0xD4] = src[0xC4];

    // OT Name: PA9 0xF8 -> PA8 0x110
    std::memcpy(dst + 0x110, src + 0xF8, 26);

    // OT Memory (reverse of PA8->PA9)
    dst[0x12B] = src[0x113]; // Intensity
    dst[0x12C] = src[0x114]; // Memory
    writeU16(dst, 0x12E, readU16(src, 0x116)); // TextVar
    dst[0x130] = src[0x118]; // Feeling

    // HyperTrainFlags: PA9 0x126 -> PA8 0x13E
    dst[0x13E] = src[0x126];
    // Markings: PA9 0x18 -> PA8 0x18
    writeU16(dst, 0x18, readU16(src, 0x18));
    // Pokerus: PA9 0x32 -> PA8 0x32
    dst[0x32] = src[0x32];

    // Ball: PA9 0x124 -> PA8 0x137
    dst[0x137] = src[0x124];

    // OT Friendship: PA9 0x112 -> PA8 0x12A
    dst[0x12A] = src[0x112];

    // Egg/Met locations: PA9 0x120/0x122 -> PA8 0x138/0x13A
    writeU16(dst, 0x138, readU16(src, 0x120)); // EggLocation
    writeU16(dst, 0x13A, readU16(src, 0x122)); // MetLocation

    // Egg dates: PA9 0x119/0x11A/0x11B -> PA8 0x131/0x132/0x133
    dst[0x131] = src[0x119]; // EggYear
    dst[0x132] = src[0x11A]; // EggMonth
    dst[0x133] = src[0x11B]; // EggDay

    // Met dates: PA9 0x11C/0x11D/0x11E -> PA8 0x134/0x135/0x136
    dst[0x134] = src[0x11C]; // MetYear
    dst[0x135] = src[0x11D]; // MetMonth
    dst[0x136] = src[0x11E]; // MetDay

    // Version: PA9 0xCE -> PA8 0xEE
    dst[0xEE] = src[0xCE];
    // BattleVersion: PA9 0xCF -> PA8 0xEF
    dst[0xEF] = src[0xCF];
    // Language: PA9 0xD5 -> PA8 0xF2
    dst[0xF2] = src[0xD5];

    // FormArgument: PA9 0xD0 -> PA8 0xF4
    writeU32(dst, 0xF4, readU32(src, 0xD0));
    // AffixedRibbon: PA9 0xD4 -> PA8 0xF8
    dst[0xF8] = src[0xD4];

    // HT_ID: PA9 0xC6 -> PA8 0xD6
    writeU16(dst, 0xD6, readU16(src, 0xC6));

    // MetLevel/OTGender: PA9 0x125 -> PA8 0x13D
    dst[0x13D] = src[0x125];

    // Stat_HPCurrent: PA9 0x8A -> PA8 0x92
    writeU16(dst, 0x92, readU16(src, 0x8A));
}

// --- PB7 <-> PA9 (completely different layout) ---

static void convertPB7toPA9(const uint8_t* src, uint8_t* dst) {
    std::memset(dst, 0, PokeCrypto::SIZE_9PARTY);

    // EC: 0x00 in both
    writeU32(dst, 0x00, readU32(src, 0x00));
    // Species: PB7 0x08 national -> PA9 0x08 Gen9 internal
    uint16_t natSpec = readU16(src, 0x08);
    writeU16(dst, 0x08, SpeciesConverter::getInternal9(natSpec));
    // HeldItem: 0x0A
    writeU16(dst, 0x0A, readU16(src, 0x0A));
    // TID/SID: 0x0C
    writeU32(dst, 0x0C, readU32(src, 0x0C));
    // EXP: 0x10
    writeU32(dst, 0x10, readU32(src, 0x10));
    // Ability: PB7 u8 0x14 -> PA9 u16 0x14
    writeU16(dst, 0x14, (uint16_t)src[0x14]);
    // PID: PB7 0x18 -> PA9 0x1C
    writeU32(dst, 0x1C, readU32(src, 0x18));
    // Nature: PB7 0x1C -> PA9 0x20 (also set StatNature to same)
    dst[0x20] = src[0x1C];
    dst[0x21] = src[0x1C]; // StatNature = Nature

    // Gender/Form/Fateful packed byte: PB7 0x1D
    // PB7: bit 0 = fateful, bits 1-2 = gender, bits 3-7 = form
    uint8_t packed = src[0x1D];
    uint8_t pb7Fateful = packed & 1;
    uint8_t pb7Gender = (packed >> 1) & 3;
    uint8_t pb7Form = (packed >> 3) & 0x1F;
    // PA9: 0x22 = fateful | (gender << 1), 0x24 = form
    dst[0x22] = pb7Fateful | (pb7Gender << 1);
    dst[0x24] = pb7Form;

    // EVs: PB7 0x1E-0x23 -> PA9 0x26-0x2B
    std::memcpy(dst + 0x26, src + 0x1E, 6);

    // Ribbons: PB7 0x30-0x36 -> PA9 0x34-0x3B (same bit layout for first 56)
    std::memcpy(dst + 0x34, src + 0x30, 7);

    // FormArgument: PB7 0x3C -> PA9 0xD0 (u32)
    writeU32(dst, 0xD0, readU32(src, 0x3C));

    // Moves: PB7 0x5A -> PA9 0x72
    std::memcpy(dst + 0x72, src + 0x5A, 8);
    // Move PP: PB7 0x62 -> PA9 0x7A
    std::memcpy(dst + 0x7A, src + 0x62, 4);
    // PP Ups: PB7 0x66 -> PA9 0x7E
    std::memcpy(dst + 0x7E, src + 0x66, 4);
    // Relearn: PB7 0x6A -> PA9 0x82
    std::memcpy(dst + 0x82, src + 0x6A, 8);

    // IV32: PB7 0x74 -> PA9 0x8C
    writeU32(dst, 0x8C, readU32(src, 0x74));

    // Nickname: PB7 0x40 (26 bytes) -> PA9 0x58 (26 bytes)
    std::memcpy(dst + 0x58, src + 0x40, 26);

    // HT Name: PB7 0x78 (26 bytes) -> PA9 0xA8 (26 bytes)
    std::memcpy(dst + 0xA8, src + 0x78, 26);

    // OT Name: PB7 0xB0 (26 bytes) -> PA9 0xF8 (26 bytes)
    std::memcpy(dst + 0xF8, src + 0xB0, 26);

    // OT Friendship: PB7 0xCA -> PA9 0x112
    dst[0x112] = src[0xCA];
    // HT Friendship: PB7 0xA2 -> PA9 0xC8
    dst[0xC8] = src[0xA2];
    // HT Memory: PB7 0xA4-0xA8 -> PA9 0xC9-0xCC
    dst[0xC9] = src[0xA4]; // HT_MemoryIntensity
    dst[0xCA] = src[0xA5]; // HT_Memory
    dst[0xCB] = src[0xA6]; // HT_MemoryFeeling
    writeU16(dst, 0xCC, readU16(src, 0xA8)); // HT_MemoryVariable
    // Pokerus: PB7 0x2B -> PA9 0x32
    dst[0x32] = src[0x2B];

    // Ball: PB7 0xDC -> PA9 0x124
    dst[0x124] = src[0xDC];

    // Met Location: PB7 0xDA -> PA9 0x122
    writeU16(dst, 0x122, readU16(src, 0xDA));
    // Egg Location: PB7 0xD8 -> PA9 0x120
    writeU16(dst, 0x120, readU16(src, 0xD8));
    // Met dates: PB7 0xD4/0xD5/0xD6 -> PA9 0x11C/0x11D/0x11E
    dst[0x11C] = src[0xD4]; // MetYear
    dst[0x11D] = src[0xD5]; // MetMonth
    dst[0x11E] = src[0xD6]; // MetDay
    // Egg dates: PB7 0xD1/0xD2/0xD3 -> PA9 0x119/0x11A/0x11B
    dst[0x119] = src[0xD1]; // EggYear
    dst[0x11A] = src[0xD2]; // EggMonth
    dst[0x11B] = src[0xD3]; // EggDay

    // CurrentHandler: PB7 0x93 -> PA9 0xC4
    dst[0xC4] = src[0x93];
    // HT Gender: PB7 0x92 -> PA9 0xC2
    dst[0xC2] = src[0x92];
    // AbilityNumber: PB7 0x15 -> PA9 0x16 (lower 3 bits)
    dst[0x16] = (dst[0x16] & ~7) | (src[0x15] & 7);
    // HyperTrainFlags: PB7 0xDE -> PA9 0x126
    dst[0x126] = src[0xDE];
    // Markings: PB7 0x16 -> PA9 0x18 (different offsets, both u16)
    writeU16(dst, 0x18, readU16(src, 0x16));

    // Version: PB7 0xDF -> PA9 0xCE  (GP=42, GE=43)
    dst[0xCE] = src[0xDF];

    // Language: PB7 0xE3 -> PA9 0xD5
    dst[0xD5] = src[0xE3];

    // Level: PB7 0xEC -> PA9 0x148
    dst[0x148] = src[0xEC];

    // Height/Weight scalars: PB7 0x3A/0x3B -> PA9 0x48/0x49
    dst[0x48] = src[0x3A];
    dst[0x49] = src[0x3B];

    // RibbonCountMemoryContest: PB7 0x38 -> PA9 0x3C
    dst[0x3C] = src[0x38];
    // RibbonCountMemoryBattle: PB7 0x39 -> PA9 0x3D
    dst[0x3D] = src[0x39];

    // MetLevel/OTGender: PB7 0xDD -> PA9 0x125
    dst[0x125] = src[0xDD];

    // ObedienceLevel: set to current level
    dst[0x11F] = dst[0x148];
}

static void convertPA9toPB7(const uint8_t* src, uint8_t* dst) {
    std::memset(dst, 0, PokeCrypto::SIZE_6PARTY);

    // EC
    writeU32(dst, 0x00, readU32(src, 0x00));
    // Species: PA9 Gen9 internal -> PB7 national
    uint16_t intSpec = readU16(src, 0x08);
    writeU16(dst, 0x08, SpeciesConverter::getNational9(intSpec));
    // HeldItem
    writeU16(dst, 0x0A, readU16(src, 0x0A));
    // TID/SID
    writeU32(dst, 0x0C, readU32(src, 0x0C));
    // EXP
    writeU32(dst, 0x10, readU32(src, 0x10));
    // Ability: PA9 u16 -> PB7 u8
    dst[0x14] = (uint8_t)readU16(src, 0x14);
    // PID: PA9 0x1C -> PB7 0x18
    writeU32(dst, 0x18, readU32(src, 0x1C));
    // Nature: PA9 0x20 -> PB7 0x1C
    dst[0x1C] = src[0x20];

    // Pack gender/form/fateful: PA9 -> PB7 0x1D
    uint8_t fateful = src[0x22] & 1;
    uint8_t gender = (src[0x22] >> 1) & 3;
    uint8_t form = src[0x24];
    dst[0x1D] = fateful | (gender << 1) | ((form & 0x1F) << 3);

    // EVs: PA9 0x26 -> PB7 0x1E
    std::memcpy(dst + 0x1E, src + 0x26, 6);
    // Ribbons
    std::memcpy(dst + 0x30, src + 0x34, 7);
    // FormArgument: PA9 0xD0 -> PB7 0x3C (u32)
    writeU32(dst, 0x3C, readU32(src, 0xD0));

    // Height/Weight: PA9 0x48/0x49 -> PB7 0x3A/0x3B
    dst[0x3A] = src[0x48];
    dst[0x3B] = src[0x49];

    // Moves: PA9 0x72 -> PB7 0x5A
    std::memcpy(dst + 0x5A, src + 0x72, 8);
    std::memcpy(dst + 0x62, src + 0x7A, 4); // PP
    std::memcpy(dst + 0x66, src + 0x7E, 4); // PP Ups
    std::memcpy(dst + 0x6A, src + 0x82, 8); // Relearn

    // IV32: PA9 0x8C -> PB7 0x74
    writeU32(dst, 0x74, readU32(src, 0x8C));

    // Nickname: PA9 0x58 -> PB7 0x40 (26 bytes)
    std::memcpy(dst + 0x40, src + 0x58, 26);
    // HT Name: PA9 0xA8 -> PB7 0x78 (26 bytes)
    std::memcpy(dst + 0x78, src + 0xA8, 26);
    // OT Name: PA9 0xF8 -> PB7 0xB0 (26 bytes)
    std::memcpy(dst + 0xB0, src + 0xF8, 26);

    // OT Friendship: PA9 0x112 -> PB7 0xCA
    dst[0xCA] = src[0x112];
    // HT Friendship: PA9 0xC8 -> PB7 0xA2
    dst[0xA2] = src[0xC8];
    // HT Memory: PA9 0xC9-0xCC -> PB7 0xA4-0xA8
    dst[0xA4] = src[0xC9]; // HT_MemoryIntensity
    dst[0xA5] = src[0xCA]; // HT_Memory
    dst[0xA6] = src[0xCB]; // HT_MemoryFeeling
    writeU16(dst, 0xA8, readU16(src, 0xCC)); // HT_MemoryVariable
    // Pokerus: PA9 0x32 -> PB7 0x2B
    dst[0x2B] = src[0x32];

    // Ball
    dst[0xDC] = src[0x124];
    // Met/Egg location: PA9 0x122/0x120 -> PB7 0xDA/0xD8
    writeU16(dst, 0xDA, readU16(src, 0x122)); // MetLocation
    writeU16(dst, 0xD8, readU16(src, 0x120)); // EggLocation
    // Met dates: PA9 0x11C/0x11D/0x11E -> PB7 0xD4/0xD5/0xD6
    dst[0xD4] = src[0x11C]; // MetYear
    dst[0xD5] = src[0x11D]; // MetMonth
    dst[0xD6] = src[0x11E]; // MetDay
    // Egg dates: PA9 0x119/0x11A/0x11B -> PB7 0xD1/0xD2/0xD3
    dst[0xD1] = src[0x119]; // EggYear
    dst[0xD2] = src[0x11A]; // EggMonth
    dst[0xD3] = src[0x11B]; // EggDay
    // CurrentHandler: PA9 0xC4 -> PB7 0x93
    dst[0x93] = src[0xC4];
    // HT Gender: PA9 0xC2 -> PB7 0x92
    dst[0x92] = src[0xC2];
    // AbilityNumber: PA9 0x16 -> PB7 0x15
    dst[0x15] = (dst[0x15] & ~7) | (src[0x16] & 7);
    // HyperTrainFlags: PA9 0x126 -> PB7 0xDE
    dst[0xDE] = src[0x126];
    // Markings: PA9 0x18 -> PB7 0x16
    writeU16(dst, 0x16, readU16(src, 0x18));
    // RibbonCountMemoryContest: PA9 0x3C -> PB7 0x38
    dst[0x38] = src[0x3C];
    // RibbonCountMemoryBattle: PA9 0x3D -> PB7 0x39
    dst[0x39] = src[0x3D];
    // MetLevel/OTGender: PA9 0x125 -> PB7 0xDD
    dst[0xDD] = src[0x125];
    // Version
    dst[0xDF] = src[0xCE];
    // Language
    dst[0xE3] = src[0xD5];
    // Level
    dst[0xEC] = src[0x148];
}

// --- PK3 <-> PA9 (radical remap) ---

// Encode a UTF-8 string to Gen3 encoding (reverse of G3_EN table)
static void writeGen3String(uint8_t* data, int offset, int maxBytes, const std::string& str) {
    // Simple ASCII subset mapping (reverse lookup)
    int pos = 0;
    for (size_t i = 0; i < str.size() && pos < maxBytes - 1; i++) {
        char c = str[i];
        uint8_t enc = 0xFF; // terminator
        if (c >= 'A' && c <= 'Z') enc = 0xBB + (c - 'A');
        else if (c >= 'a' && c <= 'z') enc = 0xD5 + (c - 'a');
        else if (c >= '0' && c <= '9') enc = 0xA1 + (c - '0');
        else if (c == ' ') enc = 0x00;
        else if (c == '.') enc = 0xAD;
        else if (c == '-') enc = 0xAE;
        else if (c == '!') enc = 0xAB;
        else if (c == '?') enc = 0xAC;
        else continue; // skip unsupported characters
        data[offset + pos++] = enc;
    }
    // Fill remainder with terminators
    while (pos < maxBytes)
        data[offset + pos++] = 0xFF;
}

static void convertPK3toPA9(const uint8_t* src, uint8_t* dst) {
    std::memset(dst, 0, PokeCrypto::SIZE_9PARTY);

    // PID doubles as EC in Gen3
    uint32_t pid = readU32(src, 0x00);
    writeU32(dst, 0x00, pid); // EC
    writeU32(dst, 0x1C, pid); // PID

    // Species: Gen3 internal -> national -> Gen9 internal
    uint16_t gen3Spec = readU16(src, 0x20);
    uint16_t natSpec = SpeciesConverter::getNational3(gen3Spec);
    writeU16(dst, 0x08, SpeciesConverter::getInternal9(natSpec));

    // HeldItem: 0x22
    writeU16(dst, 0x0A, readU16(src, 0x22));

    // TID/SID: 0x04/0x06
    writeU16(dst, 0x0C, readU16(src, 0x04));
    writeU16(dst, 0x0E, readU16(src, 0x06));

    // EXP: 0x24
    writeU32(dst, 0x10, readU32(src, 0x24));

    // Nature: PID % 25
    dst[0x20] = pid % 25;
    dst[0x21] = pid % 25; // StatNature same

    // Form: Gen3 doesn't store form (always 0)
    dst[0x24] = 0;

    // EVs: 0x38-0x3D -> 0x26-0x2B (direct copy, HP/Atk/Def/Spe/SpA/SpD order matches)
    std::memcpy(dst + 0x26, src + 0x38, 6);

    // Contest stats: PK3 0x3E-0x43 -> PA9 0x2C-0x31
    std::memcpy(dst + 0x2C, src + 0x3E, 6);

    // Pokerus: PK3 0x44 -> PA9 0x32
    dst[0x32] = src[0x44];

    // Moves: 0x2C-0x33 -> 0x72-0x79
    std::memcpy(dst + 0x72, src + 0x2C, 8);

    // PP: 0x34-0x37 -> 0x7A-0x7D
    std::memcpy(dst + 0x7A, src + 0x34, 4);

    // PP Ups: packed byte at 0x28, 2 bits per move
    uint8_t ppUps = src[0x28];
    dst[0x7E] = ppUps & 3;
    dst[0x7F] = (ppUps >> 2) & 3;
    dst[0x80] = (ppUps >> 4) & 3;
    dst[0x81] = (ppUps >> 6) & 3;

    // IVs: 0x48 -> 0x8C (mask off Gen3 bit 30=IsEgg, bit 31=AbilityBit)
    uint32_t iv32 = readU32(src, 0x48) & 0x3FFFFFFF;
    writeU32(dst, 0x8C, iv32);

    // Build temp Pokemon for field extraction (nickname, OT name, gender)
    Pokemon tempSrc;
    std::memcpy(tempSrc.data.data(), src, PokeCrypto::SIZE_3STORED);
    tempSrc.gameType_ = GameType::FR;

    // Gender: derive from PID + species ratio via Gen3 accessor
    uint8_t gender = tempSrc.gender(); // 0=male, 1=female, 2=genderless
    uint8_t fateful = (readU32(src, 0x4C) >> 31) & 1;
    dst[0x22] = fateful | (gender << 1); // PA9 format: bit 0=fateful, bits 1:2=gender

    // Ribbons: Gen3 0x4C -> PA9 shared ribbon bits
    {
        uint32_t rib3 = readU32(src, 0x4C);
        if ((rib3 >> 15) & 1) dst[0x34] |= (1 << 1); // ChampionG3
        if ((rib3 >> 19) & 1) dst[0x34] |= (1 << 7); // Effort
        if ((rib3 >> 18) & 1) dst[0x36] |= (1 << 2); // Artist
        if ((rib3 >> 23) & 1) dst[0x36] |= (1 << 6); // Country
        if ((rib3 >> 24) & 1) dst[0x36] |= (1 << 7); // National
        if ((rib3 >> 25) & 1) dst[0x37] |= (1 << 0); // Earth
        if ((rib3 >> 26) & 1) dst[0x37] |= (1 << 1); // World
        if ((rib3 >> 20) & 1) dst[0x38] |= (1 << 1); // ChampionBattle
        if ((rib3 >> 21) & 1) dst[0x38] |= (1 << 2); // ChampionRegional
        if ((rib3 >> 22) & 1) dst[0x38] |= (1 << 3); // ChampionNational
    }

    // OT Friendship: PK3 0x29 -> PA9 0x112
    dst[0x112] = src[0x29];

    std::string nick = tempSrc.nickname();
    // Write as UTF-16LE
    for (size_t i = 0; i < nick.size() && i < 12; i++) {
        uint16_t ch = (uint8_t)nick[i]; // ASCII subset
        writeU16(dst, 0x58 + i * 2, ch);
    }

    // OT Name: Gen3 at 0x14 -> UTF-16LE at 0xF8
    std::string otname = tempSrc.otName();
    for (size_t i = 0; i < otname.size() && i < 12; i++) {
        uint16_t ch = (uint8_t)otname[i];
        writeU16(dst, 0xF8 + i * 2, ch);
    }

    // Origins word at 0x46: MetLevel(0-6), Version(7-10), Ball(11-14), OTGender(15)
    uint16_t origins = readU16(src, 0x46);
    uint8_t ball = (origins >> 11) & 0xF;
    dst[0x124] = ball;
    // Version
    uint8_t version = (origins >> 7) & 0xF;
    dst[0xCE] = version;
    // Met level
    uint8_t metLevel = origins & 0x7F;
    // OT gender: bit 15 of origins -> PA9 0x125 bit 7
    uint8_t otGender = (origins >> 15) & 1;
    // Store met level (lower 7 bits) + OT gender (bit 7) at PA9 0x125
    dst[0x125] = (otGender << 7) | (metLevel & 0x7F);
    // Current level in party stat area
    dst[0x148] = tempSrc.level();

    // Language: PK3 0x12 -> PA9 0xD5
    dst[0xD5] = src[0x12];

    // MetLocation: PK3 0x45 -> PA9 0x122 (Gen3 has u8 met location)
    writeU16(dst, 0x122, (uint16_t)src[0x45]);

    // Markings: PK3 0x1B -> PA9 0x18 (Gen3 has u8, PA9 u16)
    writeU16(dst, 0x18, (uint16_t)src[0x1B]);

    // Ability: derive from AbilityBit (IV32 bit 31) + species
    {
        uint8_t abilityBit = (readU32(src, 0x48) >> 31) & 1;
        uint16_t intSpec9 = readU16(dst, 0x08);
        // Look up ability from personal data using the species+form
        int idx = PersonalZA::getEntryIndex(intSpec9, 0);
        auto& ab = PersonalZA::ABILITIES[idx];
        uint16_t ability = abilityBit ? ab.ab1 : ab.ab0;
        writeU16(dst, 0x14, ability);
        // AbilityNumber: 1 for slot 0, 2 for slot 1
        dst[0x16] = abilityBit ? 2 : 1;
    }

    // ObedienceLevel: set to current level
    dst[0x11F] = dst[0x148];

    // Set isNicknamed flag in IV32
    {
        uint32_t niv = readU32(dst, 0x8C);
        niv |= (1u << 31); // Set nicknamed bit (Gen3 always stores nickname)
        writeU32(dst, 0x8C, niv);
    }
}

static void convertPA9toPK3(const uint8_t* src, uint8_t* dst) {
    std::memset(dst, 0, PokeCrypto::SIZE_3STORED);

    // PID = EC
    uint32_t pid = readU32(src, 0x1C);
    writeU32(dst, 0x00, pid);

    // TID/SID
    writeU16(dst, 0x04, readU16(src, 0x0C));
    writeU16(dst, 0x06, readU16(src, 0x0E));

    // Species: PA9 Gen9 internal -> national -> Gen3 internal
    uint16_t intSpec = readU16(src, 0x08);
    uint16_t natSpec = SpeciesConverter::getNational9(intSpec);
    writeU16(dst, 0x20, SpeciesConverter::getInternal3(natSpec));

    // HeldItem
    writeU16(dst, 0x22, readU16(src, 0x0A));

    // EXP
    writeU32(dst, 0x24, readU32(src, 0x10));

    // OT Friendship: PA9 0x112 -> PK3 0x29
    dst[0x29] = src[0x112];

    // PP Ups: pack into single byte at 0x28
    uint8_t ppUps = (src[0x7E] & 3) | ((src[0x7F] & 3) << 2) |
                    ((src[0x80] & 3) << 4) | ((src[0x81] & 3) << 6);
    dst[0x28] = ppUps;

    // Moves
    std::memcpy(dst + 0x2C, src + 0x72, 8);
    // PP
    std::memcpy(dst + 0x34, src + 0x7A, 4);

    // EVs
    std::memcpy(dst + 0x38, src + 0x26, 6);

    // Contest stats: PA9 0x2C-0x31 -> PK3 0x3E-0x43
    std::memcpy(dst + 0x3E, src + 0x2C, 6);

    // Pokerus: PA9 0x32 -> PK3 0x44
    dst[0x44] = src[0x32];

    // IVs: mask off PA9 IsNicknamed (bit 31) — PK3 bit 31 is AbilityBit (set below)
    writeU32(dst, 0x48, readU32(src, 0x8C) & 0x7FFFFFFF);

    // Nickname: UTF-16LE at 0x58 -> Gen3 encoding at 0x08
    Pokemon tempPA9;
    std::memcpy(tempPA9.data.data(), src, PokeCrypto::SIZE_9PARTY);
    tempPA9.gameType_ = GameType::ZA;
    std::string nick = tempPA9.nickname();
    writeGen3String(dst, 0x08, 10, nick);

    // OT Name: UTF-16LE at 0xF8 -> Gen3 at 0x14
    std::string otname = tempPA9.otName();
    writeGen3String(dst, 0x14, 7, otname);

    // Origins word at 0x46: MetLevel(0-6), Version(7-10), Ball(11-14), OTGender(15)
    uint8_t ball = src[0x124];
    uint8_t version = src[0xCE];
    uint8_t metLevel = src[0x125] & 0x7F;
    uint8_t otGender = (src[0x125] >> 7) & 1;
    uint16_t origins = (metLevel & 0x7F) | ((version & 0xF) << 7) |
                       ((ball & 0xF) << 11) | (otGender << 15);
    writeU16(dst, 0x46, origins);

    // Language
    dst[0x12] = src[0xD5];

    // MetLocation: PA9 0x122 -> PK3 0x45 (truncate u16 to u8)
    dst[0x45] = (uint8_t)readU16(src, 0x122);

    // Markings: PA9 0x18 -> PK3 0x1B (truncate u16 to u8)
    dst[0x1B] = (uint8_t)readU16(src, 0x18);

    // AbilityBit: set IV32 bit 31 based on AbilityNumber
    {
        uint32_t pk3iv = readU32(dst, 0x48);
        uint8_t abilityNum = src[0x16] & 7;
        if (abilityNum == 2) // slot 1
            pk3iv |= (1u << 31);
        else
            pk3iv &= ~(1u << 31);
        writeU32(dst, 0x48, pk3iv);
    }

    // Ribbons: map shared ribbons between PA9 and Gen3 uint32 at 0x4C
    // Gen3 0x4C bits: 15=ChampionG3, 18=Artist, 19=Effort, 20=ChampionBattle,
    //   21=ChampionRegional, 22=ChampionNational, 23=Country, 24=National,
    //   25=Earth, 26=World, 31=FatefulEncounter
    uint32_t rib3 = 0;
    if ((src[0x34] >> 1) & 1) rib3 |= (1 << 15); // ChampionG3
    if ((src[0x34] >> 7) & 1) rib3 |= (1 << 19); // Effort
    if ((src[0x36] >> 2) & 1) rib3 |= (1 << 18); // Artist
    if ((src[0x36] >> 6) & 1) rib3 |= (1 << 23); // Country
    if ((src[0x36] >> 7) & 1) rib3 |= (1 << 24); // National
    if ((src[0x37] >> 0) & 1) rib3 |= (1 << 25); // Earth
    if ((src[0x37] >> 1) & 1) rib3 |= (1 << 26); // World
    if ((src[0x38] >> 1) & 1) rib3 |= (1 << 20); // ChampionBattle
    if ((src[0x38] >> 2) & 1) rib3 |= (1 << 21); // ChampionRegional
    if ((src[0x38] >> 3) & 1) rib3 |= (1 << 22); // ChampionNational
    if (src[0x22] & 1) rib3 |= (1u << 31);           // FatefulEncounter
    writeU32(dst, 0x4C, rib3);
}

// --- Main convert function ---

// Convert source to PA9 canonical
static Pokemon toCanonical(const Pokemon& src, GameType srcGame) {
    Pokemon canonical;
    canonical.gameType_ = GameType::ZA;

    const uint8_t* srcData = src.data.data();
    uint8_t* dstData = canonical.data.data();

    if (srcGame == GameType::ZA || isSV(srcGame)) {
        // Already in PA9/PK9 format — copy directly
        std::memcpy(dstData, srcData, PokeCrypto::SIZE_9PARTY);
        if (isSV(srcGame)) {
            // SV uses same format as ZA, just ensure species is Gen9 internal
            // (it already should be)
        }
    } else if (isSwSh(srcGame) || isBDSP(srcGame)) {
        convertPK8toPA9(srcData, dstData);
    } else if (srcGame == GameType::LA) {
        convertPA8toPA9(srcData, dstData);
    } else if (isLGPE(srcGame)) {
        convertPB7toPA9(srcData, dstData);
    } else if (isFRLG(srcGame)) {
        convertPK3toPA9(srcData, dstData);
    }

    return canonical;
}

// Convert PA9 canonical to destination format
static Pokemon fromCanonical(const Pokemon& canonical, GameType dstGame, GameType srcGame) {
    Pokemon result;
    result.gameType_ = dstGame;

    const uint8_t* srcData = canonical.data.data();
    uint8_t* dstData = result.data.data();

    if (dstGame == GameType::ZA || isSV(dstGame)) {
        std::memcpy(dstData, srcData, PokeCrypto::SIZE_9PARTY);
    } else if (isSwSh(dstGame) || isBDSP(dstGame)) {
        convertPA9toPK8(srcData, dstData);
    } else if (dstGame == GameType::LA) {
        convertPA9toPA8(srcData, dstData);
    } else if (isLGPE(dstGame)) {
        convertPA9toPB7(srcData, dstData);
    } else if (isFRLG(dstGame)) {
        convertPA9toPK3(srcData, dstData);
    }

    // --- Post-conversion sanitization ---

    // Move sanitization
    int moveOfs = 0x72; // PA9/PK8/PB8 default
    if (isLGPE(dstGame)) moveOfs = 0x5A;
    else if (dstGame == GameType::LA) moveOfs = 0x54;
    else if (isFRLG(dstGame)) moveOfs = 0x2C;
    sanitizeMoves(dstData, moveOfs, dstGame);

    // Item sanitization
    int itemOfs = 0x0A;
    if (isFRLG(dstGame)) itemOfs = 0x22;
    sanitizeItem(dstData, itemOfs, dstGame);

    // Strange Ball handling: PLA<->SwSh/BDSP
    if (dstGame == GameType::LA && (isSwSh(srcGame) || isBDSP(srcGame) || isSV(srcGame) || srcGame == GameType::ZA)) {
        // Replace ball with Strange Ball
        dstData[0x137] = MoveLegal::STRANGE_BALL;
    }
    if (isBDSP(dstGame) && srcGame == GameType::LA) {
        int ballOfs = 0x124;
        dstData[ballOfs] = MoveLegal::STRANGE_BALL;
    }

    // Ensure not fainted
    ensureNotFainted(dstData, dstGame);

    // Refresh checksum
    result.refreshChecksum();

    return result;
}

Pokemon convert(const Pokemon& src, GameType srcGame, GameType dstGame) {
    if (srcGame == dstGame || pairedGame(srcGame) == dstGame) {
        // Same game family — no conversion needed
        Pokemon copy = src;
        copy.gameType_ = dstGame;
        return copy;
    }

    Pokemon canonical = toCanonical(src, srcGame);
    return fromCanonical(canonical, dstGame, srcGame);
}

const char* gameTag(GameType g) {
    switch (g) {
        case GameType::ZA: return "ZA";
        case GameType::S:  return "S";
        case GameType::V:  return "V";
        case GameType::Sw: return "Sw";
        case GameType::Sh: return "Sh";
        case GameType::BD: return "BD";
        case GameType::SP: return "SP";
        case GameType::LA: return "LA";
        case GameType::GP: return "GP";
        case GameType::GE: return "GE";
        case GameType::FR: return "FR";
        case GameType::LG: return "LG";
    }
    return "??";
}

} // namespace CrossGen
