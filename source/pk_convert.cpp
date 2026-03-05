#include "pk_convert.h"
#include "poke_crypto.h"
#include "species_converter.h"
#include "personal_swsh.h"
#include "personal_bdsp.h"
#include "personal_la.h"
#include "personal_sv.h"
#include "personal_za.h"
#include "personal_gg.h"
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cmath>

// ============================================================================
// Offset tables for field-by-field conversion
// ============================================================================

// --- PK8/PB8 (G8PKM) offsets (80-byte blocks) ---
namespace G8 {
    constexpr int EC        = 0x00;
    constexpr int CHECKSUM  = 0x06;
    constexpr int SPECIES   = 0x08;
    constexpr int HELD_ITEM = 0x0A;
    constexpr int TID16     = 0x0C;
    constexpr int SID16     = 0x0E;
    constexpr int EXP       = 0x10;
    constexpr int ABILITY   = 0x14; // u16
    constexpr int ABILITY_NUM = 0x16;
    constexpr int PID       = 0x1C;
    constexpr int NATURE    = 0x20;
    constexpr int STAT_NATURE = 0x21;
    constexpr int GENDER_BYTE = 0x22; // bits 2-3 = gender, bit 0 = fateful
    constexpr int FORM      = 0x24;
    constexpr int EV_HP     = 0x26;
    // EVs: 0x26-0x2B (HP,ATK,DEF,SPE,SPA,SPD)
    constexpr int HEIGHT_SCALAR = 0x50;
    constexpr int WEIGHT_SCALAR = 0x51;
    // Block B
    constexpr int NICKNAME  = 0x58; // 26 bytes UTF-16LE
    constexpr int MOVE1     = 0x72;
    constexpr int MOVE2     = 0x74;
    constexpr int MOVE3     = 0x76;
    constexpr int MOVE4     = 0x78;
    constexpr int PP1       = 0x7A;
    constexpr int PPUPS1    = 0x7E;
    constexpr int RELEARN1  = 0x82;
    constexpr int IV32      = 0x8C;
    constexpr int DYNAMAX_LEVEL = 0x90;
    // Block C
    constexpr int HT_NAME   = 0xA8; // 26 bytes
    constexpr int HT_GENDER = 0xC2;
    constexpr int HT_LANG   = 0xC3;
    constexpr int CUR_HANDLER = 0xC4;
    constexpr int HT_FRIENDSHIP = 0xC8;
    constexpr int VERSION   = 0xDE;
    constexpr int LANGUAGE  = 0xE2;
    // Block D
    constexpr int OT_NAME   = 0xF8; // 26 bytes
    constexpr int OT_FRIENDSHIP = 0x112;
    constexpr int EGG_YEAR  = 0x119;
    constexpr int MET_YEAR  = 0x11C;
    constexpr int EGG_LOC   = 0x120;
    constexpr int MET_LOC   = 0x122;
    constexpr int BALL      = 0x124;
    constexpr int MET_LEVEL_OT_GENDER = 0x125;
    constexpr int HYPER_TRAIN = 0x126;
    constexpr int RECORD_FLAGS = 0x127; // 14 bytes
    constexpr int TRACKER   = 0x135; // 8 bytes
    constexpr int STAT_LEVEL = 0x148;
    constexpr int STAT_HPCURRENT = 0x8A;  // u16, in stored format
    constexpr int STAT_HPMAX = 0x14A;     // u16, party extension
    constexpr int STAT_ATK  = 0x14C;
    constexpr int STAT_DEF  = 0x14E;
    constexpr int STAT_SPE  = 0x150;
    constexpr int STAT_SPA  = 0x152;
    constexpr int STAT_SPD  = 0x154;
}

// --- PA8 offsets (88-byte blocks, shifts from G8) ---
namespace G8A {
    // Block A same as G8 (0x00-0x57)
    constexpr int IS_ALPHA_BYTE = 0x16; // bit 5
    constexpr int ALPHA_MOVE = 0x3E;
    // Block B (starts at 0x60 instead of 0x58)
    constexpr int MOVE1     = 0x54; // Note: in Block A's extended area
    constexpr int MOVE2     = 0x56;
    constexpr int MOVE3     = 0x58;
    constexpr int MOVE4     = 0x5A;
    constexpr int PP1       = 0x5C;
    constexpr int NICKNAME  = 0x60; // 26 bytes
    constexpr int PPUPS1    = 0x86;
    constexpr int RELEARN1  = 0x8A;
    constexpr int IV32      = 0x94;
    constexpr int GV_HP     = 0xA4;
    // Block C (starts at 0xB8 instead of 0xA8)
    constexpr int HT_NAME   = 0xB8;
    constexpr int HT_GENDER = 0xD2;
    constexpr int HT_LANG   = 0xD3;
    constexpr int CUR_HANDLER = 0xD4;
    constexpr int HT_FRIENDSHIP = 0xD8;
    constexpr int VERSION   = 0xEE;
    constexpr int LANGUAGE  = 0xF2;
    // Block D (starts at 0x110 instead of 0xF8)
    constexpr int OT_NAME   = 0x110;
    constexpr int OT_FRIENDSHIP = 0x12A;
    constexpr int EGG_YEAR  = 0x131;
    constexpr int MET_YEAR  = 0x134;
    constexpr int BALL      = 0x137;
    constexpr int EGG_LOC   = 0x138;
    constexpr int MET_LOC   = 0x13A;
    constexpr int MET_LEVEL_OT_GENDER = 0x13D;
    constexpr int HYPER_TRAIN = 0x13E;
    constexpr int RECORD_FLAGS = 0x13F; // 14 bytes
    constexpr int TRACKER   = 0x14D; // 8 bytes
    constexpr int STAT_HPCURRENT = 0x92;  // u16, in stored format
    constexpr int AFFIXED_RIBBON = 0xF8;  // i16, -1 = None
}

// --- PK9/PA9 offsets (80-byte blocks, but different field layout) ---
namespace G9 {
    // Block A - same as G8 except:
    constexpr int GENDER_BYTE = 0x22; // bits 1-2 = gender (NOT 2-3!)
    constexpr int HEIGHT_SCALAR = 0x48;
    constexpr int WEIGHT_SCALAR = 0x49;
    constexpr int SCALE     = 0x4A;
    // Block B - same as G8:
    constexpr int TERA_TYPE_ORIG = 0x94;
    constexpr int TERA_TYPE_OVER = 0x95;
    // Block C - shifted from G8:
    constexpr int VERSION   = 0xCE;
    constexpr int LANGUAGE  = 0xD5;
    // Block D:
    constexpr int OBEDIENCE_LEVEL = 0x11F;
    constexpr int TRACKER   = 0x127; // 8 bytes
    constexpr int RECORD_FLAGS_BASE = 0x12F; // 25 bytes
    // PA9-only:
    constexpr int IS_ALPHA  = 0x23; // byte (0 or non-zero)
}

// --- PB7 offsets (56-byte blocks, Gen6/7 layout) ---
namespace G7 {
    constexpr int EC        = 0x00;
    constexpr int CHECKSUM  = 0x06;
    constexpr int SPECIES   = 0x08;
    constexpr int HELD_ITEM = 0x0A;
    constexpr int TID16     = 0x0C;
    constexpr int SID16     = 0x0E;
    constexpr int EXP       = 0x10;
    constexpr int ABILITY   = 0x14; // u8
    constexpr int ABILITY_NUM = 0x15;
    constexpr int PID       = 0x18;
    constexpr int NATURE    = 0x1C;
    constexpr int GENDER_FORM_FATEFUL = 0x1D; // bit 0=fateful, bits 1-2=gender, bits 3-7=form
    constexpr int EV_HP     = 0x1E;
    // EVs: 0x1E-0x23
    constexpr int AV_HP     = 0x24; // Awakened Values: 0x24-0x29
    constexpr int POKERUS   = 0x2B;
    constexpr int HEIGHT_ABS = 0x2C; // float
    constexpr int HEIGHT_SCALAR = 0x3A;
    constexpr int WEIGHT_SCALAR = 0x3B;
    // Block B
    constexpr int NICKNAME  = 0x40; // 26 bytes
    constexpr int MOVE1     = 0x5A;
    constexpr int MOVE2     = 0x5C;
    constexpr int MOVE3     = 0x5E;
    constexpr int MOVE4     = 0x60;
    constexpr int PP1       = 0x62;
    constexpr int PPUPS1    = 0x66;
    constexpr int RELEARN1  = 0x6A;
    constexpr int IV32      = 0x74;
    // Block C
    constexpr int HT_NAME   = 0x78; // 26 bytes
    constexpr int HT_GENDER = 0x92;
    constexpr int CUR_HANDLER = 0x93;
    constexpr int HT_FRIENDSHIP = 0xA2;
    // Block D
    constexpr int OT_NAME   = 0xB0; // 26 bytes
    constexpr int OT_FRIENDSHIP = 0xCA;
    constexpr int EGG_YEAR  = 0xD1;
    constexpr int MET_YEAR  = 0xD4;
    constexpr int EGG_LOC   = 0xD8;
    constexpr int MET_LOC   = 0xDA;
    constexpr int BALL      = 0xDC;
    constexpr int MET_LEVEL_OT_GENDER = 0xDD;
    constexpr int HYPER_TRAIN = 0xDE;
    constexpr int VERSION   = 0xDF;
    constexpr int LANGUAGE  = 0xE3;
    constexpr int WEIGHT_ABS = 0xE4; // float
    constexpr int STAT_LEVEL = 0xEC;
}

// ============================================================================
// Helper functions
// ============================================================================

static inline void writeU16(uint8_t* d, int ofs, uint16_t v) {
    std::memcpy(d + ofs, &v, 2);
}
static inline void writeU32(uint8_t* d, int ofs, uint32_t v) {
    std::memcpy(d + ofs, &v, 4);
}
static inline void writeU64(uint8_t* d, int ofs, uint64_t v) {
    std::memcpy(d + ofs, &v, 8);
}
static inline uint16_t readU16(const uint8_t* d, int ofs) {
    uint16_t v; std::memcpy(&v, d + ofs, 2); return v;
}
static inline uint32_t readU32(const uint8_t* d, int ofs) {
    uint32_t v; std::memcpy(&v, d + ofs, 4); return v;
}
static inline uint64_t readU64(const uint8_t* d, int ofs) {
    uint64_t v; std::memcpy(&v, d + ofs, 8); return v;
}
static inline void writeFloat(uint8_t* d, int ofs, float v) {
    std::memcpy(d + ofs, &v, 4);
}
static inline float readFloat(const uint8_t* d, int ofs) {
    float v; std::memcpy(&v, d + ofs, 4); return v;
}

// Copy 26 bytes of UTF-16LE name data (13 chars + terminator)
static inline void copyName(uint8_t* dst, int dstOfs, const uint8_t* src, int srcOfs) {
    std::memcpy(dst + dstOfs, src + srcOfs, 26);
}

// Compute PK8/PK9 checksum over 0x08..storedSize
static uint16_t computeChecksum(const uint8_t* data, int storedSize) {
    uint16_t chk = 0;
    for (int i = 8; i < storedSize; i += 2)
        chk += readU16(data, i);
    return chk;
}

// Nature modifiers: index 0-4 = ATK, DEF, SPE, SPA, SPD
// Returns multiplier as x/10 (9 = decreased, 10 = neutral, 11 = increased)
static void getNatureMods(uint8_t statNature, int mods[5]) {
    for (int i = 0; i < 5; i++) mods[i] = 10;
    if (statNature >= 25) return;
    int up = statNature / 5, down = statNature % 5;
    if (up != down) { mods[up] = 11; mods[down] = 9; }
}

// Compute and write party stats for PK8/PB8 output (G8PKM format).
// Matches PKHeX ResetPartyStats: uses HyperTrain flags, standard formula, nature modifiers.
// HyperTrainFlags at 0x126: bit0=HP, bit1=ATK, bit2=DEF, bit3=SPA, bit4=SPD, bit5=SPE
static void computeAndSetPartyStatsPK8(uint8_t* d, uint16_t species, uint8_t form) {
    auto bs = PersonalSWSH::getBaseStats(species, form);
    uint32_t iv32 = readU32(d, G8::IV32);
    uint8_t ht = d[G8::HYPER_TRAIN];

    int ivHP  = (ht & (1<<0)) ? 31 : (int)((iv32 >>  0) & 0x1F);
    int ivAtk = (ht & (1<<1)) ? 31 : (int)((iv32 >>  5) & 0x1F);
    int ivDef = (ht & (1<<2)) ? 31 : (int)((iv32 >> 10) & 0x1F);
    int ivSpA = (ht & (1<<3)) ? 31 : (int)((iv32 >> 20) & 0x1F);
    int ivSpD = (ht & (1<<4)) ? 31 : (int)((iv32 >> 25) & 0x1F);
    int ivSpe = (ht & (1<<5)) ? 31 : (int)((iv32 >> 15) & 0x1F);

    int evHP = d[G8::EV_HP], evAtk = d[G8::EV_HP+1], evDef = d[G8::EV_HP+2];
    int evSpe = d[G8::EV_HP+3], evSpA = d[G8::EV_HP+4], evSpD = d[G8::EV_HP+5];
    uint8_t level = d[G8::STAT_LEVEL];
    if (level == 0) level = 1;

    // Base stats before nature (PKHeX formula: (IV + 2*Base + EV/4 + 100) * Level / 100 + 10)
    uint16_t hp;
    if (bs.hp == 1) // Shedinja
        hp = 1;
    else
        hp = (uint16_t)(((ivHP + 2u * bs.hp + evHP / 4 + 100) * level / 100) + 10);

    uint16_t atk = (uint16_t)(((ivAtk + 2u * bs.atk  + evAtk / 4) * level / 100) + 5);
    uint16_t def = (uint16_t)(((ivDef + 2u * bs.def_  + evDef / 4) * level / 100) + 5);
    uint16_t spe = (uint16_t)(((ivSpe + 2u * bs.spe   + evSpe / 4) * level / 100) + 5);
    uint16_t spa = (uint16_t)(((ivSpA + 2u * bs.spa   + evSpA / 4) * level / 100) + 5);
    uint16_t spd = (uint16_t)(((ivSpD + 2u * bs.spd   + evSpD / 4) * level / 100) + 5);

    // Apply nature modifiers (PKHeX: ModifyStatsForNature applies *11/10 or *9/10)
    int mods[5];
    getNatureMods(d[G8::STAT_NATURE], mods);
    atk = (uint16_t)(atk * mods[0] / 10);
    def = (uint16_t)(def * mods[1] / 10);
    spe = (uint16_t)(spe * mods[2] / 10);
    spa = (uint16_t)(spa * mods[3] / 10);
    spd = (uint16_t)(spd * mods[4] / 10);

    writeU16(d, G8::STAT_HPCURRENT, hp);
    writeU16(d, G8::STAT_HPMAX, hp);
    writeU16(d, G8::STAT_ATK, atk);
    writeU16(d, G8::STAT_DEF, def);
    writeU16(d, G8::STAT_SPE, spe);
    writeU16(d, G8::STAT_SPA, spa);
    writeU16(d, G8::STAT_SPD, spd);
}

// PLA Ganbaru multiplier table (from PKHeX GanbaruExtensions)
static constexpr uint8_t GANBARU_MUL[] = {0, 2, 3, 4, 7, 8, 9, 14, 15, 16, 25};

static int getGanbaruBias(int iv) {
    if (iv >= 31) return 3;
    if (iv >= 26) return 2;
    if (iv >= 20) return 1;
    return 0;
}

// PLA HP formula (from PKHeX PA8.cs):
//   HP = GetGanbaruStat(baseHP, iv, gv, level) + GetStatHp(baseHP, level)
//   GetStatHp = (int)((level/100.0f + 1.0f) * baseHP + level)
//   GetGanbaruStat = round((sqrt(baseHP) * ganbaruMul + level) / 2.5)
static void computeAndSetStatHPPA8(uint8_t* d, uint16_t species, uint8_t form, uint8_t level) {
    auto bs = PersonalLA::getBaseStats(species, form);
    uint32_t iv32 = readU32(d, G8A::IV32);
    uint8_t ht = d[G8A::HYPER_TRAIN];
    int ivHP = (ht & (1<<0)) ? 31 : (int)((iv32 >> 0) & 0x1F);
    uint8_t gvHP = d[G8A::GV_HP];

    if (level == 0) level = 1;

    uint16_t hp;
    if (bs.hp == 1) // Shedinja
        hp = 1;
    else {
        // GetStatHp
        int statHp = (int)(((level / 100.0f) + 1.0f) * bs.hp + level);

        // GetGanbaruStat
        int idx = gvHP + getGanbaruBias(ivHP);
        if (idx > 10) idx = 10;
        int mul = GANBARU_MUL[idx];
        double step1 = std::sqrt((double)bs.hp) * mul;
        float result = (float)((float)step1 + level) / 2.5f;
        int ganbaruStat = (int)std::round(result);

        hp = (uint16_t)(statHp + ganbaruStat);
    }

    writeU16(d, G8A::STAT_HPCURRENT, hp);
}

static bool speciesPresentInTarget(uint16_t species, uint8_t form, GameType target) {
    switch (target) {
        case GameType::Sw: case GameType::Sh:
            return PersonalSWSH::isPresentInGame(species, form);
        case GameType::BD: case GameType::SP:
            return PersonalBDSP::isPresentInGame(species, form);
        case GameType::LA:
            return PersonalLA::isPresentInGame(species, form);
        case GameType::S: case GameType::V:
            return PersonalSV::isPresentInGame(species, form);
        case GameType::ZA:
            return PersonalZA::isPresentInGame(species, form);
        default:
            return false;
    }
}

// ============================================================================
// HOME Tracker generation
// ============================================================================

uint64_t PKConvert::generateTracker() {
    // Combine time-based and random components for uniqueness
    auto now = std::time(nullptr);
    uint32_t timePart = static_cast<uint32_t>(now);
    uint32_t randPart = static_cast<uint32_t>(std::rand());
    return (static_cast<uint64_t>(timePart) << 32) | randPart;
}

// ============================================================================
// Validation
// ============================================================================

bool PKConvert::canTransfer(const Pokemon& src, GameType target) {
    if (src.isEmpty()) return false;

    auto srcFamily = formatFamilyOf(src.gameType_);
    auto dstFamily = formatFamilyOf(target);

    // FRLG not supported for cross-gen
    if (srcFamily == FormatFamily::FRLG || dstFamily == FormatFamily::FRLG)
        return false;

    // Same family is always OK
    if (srcFamily == dstFamily) return true;

    // LGPE can only go forward to Gen8+
    if (srcFamily == FormatFamily::LGPE) {
        // LGPE → any Gen8+ is allowed (species check still needed)
    } else if (dstFamily == FormatFamily::LGPE) {
        // Cannot go back to LGPE from Gen8+
        return false;
    }

    // Check species exists in target game
    uint16_t species = src.species();
    uint8_t form = src.form();
    return speciesPresentInTarget(species, form, target);
}

bool PKConvert::isOneWayTransfer(GameType src, GameType target) {
    return isLGPE(src) && !isLGPE(target);
}

// ============================================================================
// PK8 <-> PK9 conversion
// ============================================================================

// Most of Block A (0x00-0x4F) shares the same offsets between PK8 and PK9.
// Key differences: gender bits, height/weight scalar positions, species format.
// Block B (moves/IVs) shares offsets 0x72-0x8C.
// Block C/D: HT/OT names and dates share offsets 0xA8-0x126.
// But Version, Language, Tracker differ.

Pokemon PKConvert::convertPK8toPK9(const Pokemon& src) {
    Pokemon dst{};
    dst.gameType_ = GameType::S; // Default to Scarlet

    const auto* s = src.data.data();
    auto* d = dst.data.data();

    uint16_t species = src.species(); // national dex
    uint8_t form = src.form();

    // Check species exists in SV
    if (!PersonalSV::isPresentInGame(species, form))
        return Pokemon{}; // empty = failure

    // Copy all data including party stats (PK8 and PK9 both use SIZE_9PARTY = 0x158)
    // Most fields share offsets; we fix up the ones that differ below.
    std::memcpy(d, s, PokeCrypto::SIZE_9PARTY);

    // Fix species: PK9 stores Gen9 internal ID
    writeU16(d, 0x08, SpeciesConverter::getInternal9(species));

    // Fix gender bits: PK8 bits 2-3 -> PK9 bits 1-2
    uint8_t genderByte = s[G8::GENDER_BYTE];
    uint8_t fateful = genderByte & 1;
    uint8_t gender = (genderByte >> 2) & 3;
    d[G9::GENDER_BYTE] = fateful | (gender << 1);

    // HeightScalar/WeightScalar: PK8 at 0x50-0x51, PK9 at 0x48-0x49
    d[G9::HEIGHT_SCALAR] = s[G8::HEIGHT_SCALAR];
    d[G9::WEIGHT_SCALAR] = s[G8::WEIGHT_SCALAR];
    d[G9::SCALE] = 128; // default scale
    // Clear PK8 positions (now repurposed in PK9)
    d[G8::HEIGHT_SCALAR] = 0;
    d[G8::WEIGHT_SCALAR] = 0;

    // Clear Gigantamax flag (bit 4 of 0x16) - not applicable in Gen9
    d[0x16] &= ~(1 << 4);

    // Clear DynamaxLevel (no longer relevant in Gen9)
    d[G8::DYNAMAX_LEVEL] = 0;

    // Set TeraType based on species' primary type
    d[G9::TERA_TYPE_ORIG] = PersonalSV::getType1(species, form);
    d[G9::TERA_TYPE_OVER] = 19; // 19 = OverrideNone

    // Version: PK8 at 0xDE -> PK9 at 0xCE
    d[G9::VERSION] = s[G8::VERSION];
    d[G8::VERSION] = 0; // clear old position

    // Language: PK8 at 0xE2 -> PK9 at 0xD5
    d[G9::LANGUAGE] = s[G8::LANGUAGE];
    d[G8::LANGUAGE] = 0; // clear old position

    // ObedienceLevel: set to MetLevel (PK8 doesn't have this)
    d[G9::OBEDIENCE_LEVEL] = s[G8::MET_LEVEL_OT_GENDER] & 0x7F;

    // Tracker: PK8 at 0x135 -> PK9 at 0x127
    uint64_t tracker = readU64(s, G8::TRACKER);
    if (tracker == 0)
        tracker = generateTracker();
    // Clear PK8 record flags + tracker area, then set PK9 tracker
    std::memset(d + G8::RECORD_FLAGS, 0, 14 + 8);
    writeU64(d, G9::TRACKER, tracker);

    // Clear PK9 record flags area (fresh start)
    std::memset(d + G9::RECORD_FLAGS_BASE, 0, 25);

    // Refresh checksum
    writeU16(d, G8::CHECKSUM, computeChecksum(d, PokeCrypto::SIZE_9STORED));

    return dst;
}

Pokemon PKConvert::convertPK9toPK8(const Pokemon& src) {
    Pokemon dst{};
    dst.gameType_ = GameType::Sw; // Default to Sword

    const auto* s = src.data.data();
    auto* d = dst.data.data();

    // PK9 stores internal Gen9 species ID - convert to national
    uint16_t speciesInternal = readU16(s, 0x08);
    uint16_t species = SpeciesConverter::getNational9(speciesInternal);
    uint8_t form = s[0x24];

    if (!PersonalSWSH::isPresentInGame(species, form))
        return Pokemon{};

    // Copy all data including party stats (PK9 and PK8 both use SIZE_9PARTY = 0x158)
    std::memcpy(d, s, PokeCrypto::SIZE_9PARTY);

    // Species: back to national dex
    writeU16(d, 0x08, species);

    // Gender bits: PK9 bits 1-2 -> PK8 bits 2-3
    uint8_t genderByte = s[G9::GENDER_BYTE];
    uint8_t fateful = genderByte & 1;
    uint8_t gender = (genderByte >> 1) & 3;
    d[G8::GENDER_BYTE] = fateful | (gender << 2);

    // HeightScalar/WeightScalar: PK9 at 0x48-0x49 -> PK8 at 0x50-0x51
    d[G8::HEIGHT_SCALAR] = s[G9::HEIGHT_SCALAR];
    d[G8::WEIGHT_SCALAR] = s[G9::WEIGHT_SCALAR];
    d[G9::HEIGHT_SCALAR] = 0;
    d[G9::WEIGHT_SCALAR] = 0;
    d[G9::SCALE] = 0;

    // DynamaxLevel defaults to 0
    d[G8::DYNAMAX_LEVEL] = 0;
    // TeraType fields become unused in PK8
    d[G9::TERA_TYPE_ORIG] = 0;
    d[G9::TERA_TYPE_OVER] = 0;

    // Version: PK9 at 0xCE -> PK8 at 0xDE
    d[G8::VERSION] = s[G9::VERSION];
    d[G9::VERSION] = 0;

    // Language: PK9 at 0xD5 -> PK8 at 0xE2
    d[G8::LANGUAGE] = s[G9::LANGUAGE];
    d[G9::LANGUAGE] = 0;

    // Tracker: PK9 at 0x127 -> PK8 at 0x135
    uint64_t tracker = readU64(s, G9::TRACKER);
    if (tracker == 0)
        tracker = generateTracker();
    std::memset(d + G9::TRACKER, 0, 25); // clear PK9 record area
    std::memset(d + G8::RECORD_FLAGS, 0, 14); // clear PK8 record flags
    writeU64(d, G8::TRACKER, tracker);

    // Refresh checksum
    writeU16(d, G8::CHECKSUM, computeChecksum(d, PokeCrypto::SIZE_9STORED));

    return dst;
}

// ============================================================================
// PK8 <-> PA8 conversion
// ============================================================================

// PA8 uses 88-byte blocks, so fields in Blocks B/C/D are at different offsets.
// Block A (0x00-0x57 for PA8) shares most fields with PK8's Block A.

Pokemon PKConvert::convertPK8toPA8(const Pokemon& src) {
    Pokemon dst{};
    dst.gameType_ = GameType::LA;

    const auto* s = src.data.data();
    auto* d = dst.data.data();

    uint16_t species = readU16(s, G8::SPECIES);
    uint8_t form = s[G8::FORM];

    if (!PersonalLA::isPresentInGame(species, form))
        return Pokemon{};

    // Block A core (0x00-0x53): copy directly (shared offsets)
    std::memcpy(d, s, 0x54);

    // Clear Gigantamax flag
    d[0x16] &= ~(1 << 4);

    // --- Moves: PK8 0x72-0x79 -> PA8 0x54-0x5B ---
    for (int i = 0; i < 4; i++)
        writeU16(d, G8A::MOVE1 + i * 2, readU16(s, G8::MOVE1 + i * 2));

    // PP: PK8 0x7A-0x7D -> PA8 0x5C-0x5F
    for (int i = 0; i < 4; i++)
        d[G8A::PP1 + i] = s[G8::PP1 + i];

    // Nickname: PK8 0x58 -> PA8 0x60
    copyName(d, G8A::NICKNAME, s, G8::NICKNAME);

    // PPUps: PK8 0x7E-0x81 -> PA8 0x86-0x89
    for (int i = 0; i < 4; i++)
        d[G8A::PPUPS1 + i] = s[G8::PPUPS1 + i];

    // Relearn: PK8 0x82-0x88 -> PA8 0x8A-0x90
    for (int i = 0; i < 4; i++)
        writeU16(d, G8A::RELEARN1 + i * 2, readU16(s, G8::RELEARN1 + i * 2));

    // IV32: PK8 0x8C -> PA8 0x94
    writeU32(d, G8A::IV32, readU32(s, G8::IV32));

    // GanbaruValues: default 0 (PA8-specific)
    std::memset(d + G8A::GV_HP, 0, 6);

    // --- Block C: HT info ---
    copyName(d, G8A::HT_NAME, s, G8::HT_NAME);
    d[G8A::HT_GENDER] = s[G8::HT_GENDER];
    d[G8A::HT_LANG] = s[G8::HT_LANG];
    d[G8A::CUR_HANDLER] = s[G8::CUR_HANDLER];
    d[G8A::HT_FRIENDSHIP] = s[G8::HT_FRIENDSHIP];

    // Version: PK8 0xDE -> PA8 0xEE
    d[G8A::VERSION] = s[G8::VERSION];
    // Language: PK8 0xE2 -> PA8 0xF2
    d[G8A::LANGUAGE] = s[G8::LANGUAGE];

    // --- Block D: OT info ---
    copyName(d, G8A::OT_NAME, s, G8::OT_NAME);
    d[G8A::OT_FRIENDSHIP] = s[G8::OT_FRIENDSHIP];

    // Dates
    d[G8A::EGG_YEAR]     = s[G8::EGG_YEAR];
    d[G8A::EGG_YEAR + 1] = s[G8::EGG_YEAR + 1];
    d[G8A::EGG_YEAR + 2] = s[G8::EGG_YEAR + 2];
    d[G8A::MET_YEAR]     = s[G8::MET_YEAR];
    d[G8A::MET_YEAR + 1] = s[G8::MET_YEAR + 1];
    d[G8A::MET_YEAR + 2] = s[G8::MET_YEAR + 2];

    // Ball, Location
    d[G8A::BALL] = s[G8::BALL];
    writeU16(d, G8A::EGG_LOC, readU16(s, G8::EGG_LOC));
    writeU16(d, G8A::MET_LOC, readU16(s, G8::MET_LOC));
    d[G8A::MET_LEVEL_OT_GENDER] = s[G8::MET_LEVEL_OT_GENDER];
    d[G8A::HYPER_TRAIN] = s[G8::HYPER_TRAIN];

    // Tracker
    uint64_t tracker = readU64(s, G8::TRACKER);
    if (tracker == 0)
        tracker = generateTracker();
    writeU64(d, G8A::TRACKER, tracker);

    // AffixedRibbon: -1 (None) — PA8 default (sbyte at 0xF8)
    d[G8A::AFFIXED_RIBBON] = 0xFF;

    // Compute Stat_HPCurrent (at 0x92 in PA8 stored format)
    computeAndSetStatHPPA8(d, species, form, src.level());

    // Checksum (PA8 uses SIZE_8ASTORED = 0x168)
    writeU16(d, G8::CHECKSUM, computeChecksum(d, PokeCrypto::SIZE_8ASTORED));

    return dst;
}

Pokemon PKConvert::convertPA8toPK8(const Pokemon& src) {
    Pokemon dst{};
    dst.gameType_ = GameType::Sw;

    const auto* s = src.data.data();
    auto* d = dst.data.data();

    uint16_t species = readU16(s, G8::SPECIES); // Block A species is same offset
    uint8_t form = s[G8::FORM];

    if (!PersonalSWSH::isPresentInGame(species, form))
        return Pokemon{};

    // Block A core (0x00-0x53)
    std::memcpy(d, s, 0x54);

    // Clear Alpha/Noble flags (PK8 doesn't have them)
    d[0x16] &= ~((1 << 5) | (1 << 6));

    // --- Moves: PA8 0x54-0x5B -> PK8 0x72-0x79 ---
    for (int i = 0; i < 4; i++)
        writeU16(d, G8::MOVE1 + i * 2, readU16(s, G8A::MOVE1 + i * 2));

    // PP
    for (int i = 0; i < 4; i++)
        d[G8::PP1 + i] = s[G8A::PP1 + i];

    // Nickname: PA8 0x60 -> PK8 0x58
    copyName(d, G8::NICKNAME, s, G8A::NICKNAME);

    // PPUps
    for (int i = 0; i < 4; i++)
        d[G8::PPUPS1 + i] = s[G8A::PPUPS1 + i];

    // Relearn
    for (int i = 0; i < 4; i++)
        writeU16(d, G8::RELEARN1 + i * 2, readU16(s, G8A::RELEARN1 + i * 2));

    // IV32
    writeU32(d, G8::IV32, readU32(s, G8A::IV32));

    // DynamaxLevel: 0 (default)
    d[G8::DYNAMAX_LEVEL] = 0;

    // --- Block C ---
    copyName(d, G8::HT_NAME, s, G8A::HT_NAME);
    d[G8::HT_GENDER] = s[G8A::HT_GENDER];
    d[G8::HT_LANG] = s[G8A::HT_LANG];
    d[G8::CUR_HANDLER] = s[G8A::CUR_HANDLER];
    d[G8::HT_FRIENDSHIP] = s[G8A::HT_FRIENDSHIP];
    d[G8::VERSION] = s[G8A::VERSION];
    d[G8::LANGUAGE] = s[G8A::LANGUAGE];

    // --- Block D ---
    copyName(d, G8::OT_NAME, s, G8A::OT_NAME);
    d[G8::OT_FRIENDSHIP] = s[G8A::OT_FRIENDSHIP];

    // Dates
    d[G8::EGG_YEAR]     = s[G8A::EGG_YEAR];
    d[G8::EGG_YEAR + 1] = s[G8A::EGG_YEAR + 1];
    d[G8::EGG_YEAR + 2] = s[G8A::EGG_YEAR + 2];
    d[G8::MET_YEAR]     = s[G8A::MET_YEAR];
    d[G8::MET_YEAR + 1] = s[G8A::MET_YEAR + 1];
    d[G8::MET_YEAR + 2] = s[G8A::MET_YEAR + 2];

    d[G8::BALL] = s[G8A::BALL];
    writeU16(d, G8::EGG_LOC, readU16(s, G8A::EGG_LOC));
    writeU16(d, G8::MET_LOC, readU16(s, G8A::MET_LOC));
    d[G8::MET_LEVEL_OT_GENDER] = s[G8A::MET_LEVEL_OT_GENDER];
    d[G8::HYPER_TRAIN] = s[G8A::HYPER_TRAIN];

    // Tracker
    uint64_t tracker = readU64(s, G8A::TRACKER);
    if (tracker == 0)
        tracker = generateTracker();
    writeU64(d, G8::TRACKER, tracker);

    // Stat level: PA8 computes from EXP, PK8 needs it at 0x148
    d[G8::STAT_LEVEL] = src.level();

    // Compute party stats (HP, ATK, DEF, SPE, SPA, SPD) from base stats + IVs + EVs
    computeAndSetPartyStatsPK8(d, species, form);

    // Checksum
    writeU16(d, G8::CHECKSUM, computeChecksum(d, PokeCrypto::SIZE_9STORED));

    return dst;
}

// ============================================================================
// PB7 -> PK8 conversion (one-way, LGPE -> SwSh)
// ============================================================================

Pokemon PKConvert::convertPB7toPK8(const Pokemon& src) {
    Pokemon dst{};
    dst.gameType_ = GameType::Sw;

    const auto* s = src.data.data();
    auto* d = dst.data.data();

    uint16_t species = readU16(s, G7::SPECIES);
    uint8_t gff = s[G7::GENDER_FORM_FATEFUL];
    uint8_t form = (gff >> 3) & 0x1F;

    if (!PersonalSWSH::isPresentInGame(species, form))
        return Pokemon{};

    // --- Block A ---
    writeU32(d, G8::EC, readU32(s, G7::EC));
    writeU16(d, G8::SPECIES, species);
    writeU16(d, G8::HELD_ITEM, readU16(s, G7::HELD_ITEM));
    writeU16(d, G8::TID16, readU16(s, G7::TID16));
    writeU16(d, G8::SID16, readU16(s, G7::SID16));
    writeU32(d, G8::EXP, readU32(s, G7::EXP));

    // Ability: recompute from SwSh personal data
    uint8_t abilityNum = s[G7::ABILITY_NUM] & 7;
    uint16_t ability;
    if (abilityNum <= 2) {
        ability = PersonalSWSH::getAbility(species, form, abilityNum > 2 ? 0 : abilityNum);
    } else {
        ability = PersonalSWSH::getAbility(species, form, 0);
    }
    writeU16(d, G8::ABILITY, ability);
    d[G8::ABILITY_NUM] = abilityNum;

    writeU32(d, G8::PID, readU32(s, G7::PID));

    // Nature + StatNature (PB7 only has Nature)
    d[G8::NATURE] = s[G7::NATURE];
    d[G8::STAT_NATURE] = s[G7::NATURE]; // Same value

    // Gender + FatefulEncounter
    uint8_t fateful = gff & 1;
    uint8_t gender = (gff >> 1) & 3;
    d[G8::GENDER_BYTE] = fateful | (gender << 2);

    d[G8::FORM] = form;

    // EVs: PB7 0x1E-0x23 -> PK8 0x26-0x2B
    std::memcpy(d + G8::EV_HP, s + G7::EV_HP, 6);

    // Pokerus
    d[0x32] = s[G7::POKERUS];

    // HeightScalar/WeightScalar
    d[G8::HEIGHT_SCALAR] = s[G7::HEIGHT_SCALAR];
    d[G8::WEIGHT_SCALAR] = s[G7::WEIGHT_SCALAR];

    // --- Block B ---
    // Nickname: PB7 0x40 -> PK8 0x58
    copyName(d, G8::NICKNAME, s, G7::NICKNAME);

    // Moves
    for (int i = 0; i < 4; i++)
        writeU16(d, G8::MOVE1 + i * 2, readU16(s, G7::MOVE1 + i * 2));

    // PP
    for (int i = 0; i < 4; i++)
        d[G8::PP1 + i] = s[G7::PP1 + i];

    // PPUps
    for (int i = 0; i < 4; i++)
        d[G8::PPUPS1 + i] = s[G7::PPUPS1 + i];

    // Relearn
    for (int i = 0; i < 4; i++)
        writeU16(d, G8::RELEARN1 + i * 2, readU16(s, G7::RELEARN1 + i * 2));

    // IV32
    writeU32(d, G8::IV32, readU32(s, G7::IV32));

    // DynamaxLevel: 0
    d[G8::DYNAMAX_LEVEL] = 0;

    // --- Block C ---
    // HT Name: PB7 0x78 -> PK8 0xA8
    copyName(d, G8::HT_NAME, s, G7::HT_NAME);
    d[G8::HT_GENDER] = s[G7::HT_GENDER];
    d[G8::CUR_HANDLER] = s[G7::CUR_HANDLER];
    d[G8::HT_FRIENDSHIP] = s[G7::HT_FRIENDSHIP];

    // Version
    d[G8::VERSION] = s[G7::VERSION];
    // Language
    d[G8::LANGUAGE] = s[G7::LANGUAGE];

    // --- Block D ---
    // OT Name: PB7 0xB0 -> PK8 0xF8
    copyName(d, G8::OT_NAME, s, G7::OT_NAME);
    d[G8::OT_FRIENDSHIP] = s[G7::OT_FRIENDSHIP];

    // Dates
    d[G8::EGG_YEAR]     = s[G7::EGG_YEAR];
    d[G8::EGG_YEAR + 1] = s[G7::EGG_YEAR + 1];
    d[G8::EGG_YEAR + 2] = s[G7::EGG_YEAR + 2];
    d[G8::MET_YEAR]     = s[G7::MET_YEAR];
    d[G8::MET_YEAR + 1] = s[G7::MET_YEAR + 1];
    d[G8::MET_YEAR + 2] = s[G7::MET_YEAR + 2];

    // Location
    writeU16(d, G8::EGG_LOC, readU16(s, G7::EGG_LOC));
    writeU16(d, G8::MET_LOC, readU16(s, G7::MET_LOC));
    d[G8::BALL] = s[G7::BALL];
    d[G8::MET_LEVEL_OT_GENDER] = s[G7::MET_LEVEL_OT_GENDER];
    d[G8::HYPER_TRAIN] = s[G7::HYPER_TRAIN];

    // Stat level: PB7 at 0xEC -> PK8 at 0x148
    d[G8::STAT_LEVEL] = s[G7::STAT_LEVEL];

    // Generate HOME tracker (cross-gen transfer always needs one)
    writeU64(d, G8::TRACKER, generateTracker());

    // Compute party stats (HP, ATK, DEF, SPE, SPA, SPD) from base stats + IVs + EVs
    computeAndSetPartyStatsPK8(d, species, form);

    // Checksum
    writeU16(d, G8::CHECKSUM, computeChecksum(d, PokeCrypto::SIZE_9STORED));

    return dst;
}

// ============================================================================
// Trivial adaptations (same base format)
// ============================================================================

// PK8 <-> PB8: Same G8PKM format, just validate species in BDSP
Pokemon PKConvert::adaptPK8toPB8(const Pokemon& src) {
    uint16_t species = readU16(src.data.data(), G8::SPECIES);
    uint8_t form = src.data[G8::FORM];

    if (!PersonalBDSP::isPresentInGame(species, form))
        return Pokemon{};

    Pokemon dst = src;
    dst.gameType_ = GameType::BD;
    // Clear Gigantamax flag (not applicable in BDSP)
    dst.data[0x16] &= ~(1 << 4);
    // DynamaxLevel = 0
    dst.data[G8::DYNAMAX_LEVEL] = 0;
    return dst;
}

Pokemon PKConvert::adaptPB8toPK8(const Pokemon& src) {
    uint16_t species = readU16(src.data.data(), G8::SPECIES);
    uint8_t form = src.data[G8::FORM];

    if (!PersonalSWSH::isPresentInGame(species, form))
        return Pokemon{};

    Pokemon dst = src;
    dst.gameType_ = GameType::Sw;
    return dst;
}

// PK9 <-> PA9: Same G9PKM format, handle Alpha byte
Pokemon PKConvert::adaptPK9toPA9(const Pokemon& src) {
    uint16_t speciesInternal = readU16(src.data.data(), 0x08);
    uint16_t species = SpeciesConverter::getNational9(speciesInternal);
    uint8_t form = src.data[0x24];

    if (!PersonalZA::isPresentInGame(species, form))
        return Pokemon{};

    Pokemon dst = src;
    dst.gameType_ = GameType::ZA;
    // TeraType fields (0x94-0x95) are not used in PA9 - clear them
    dst.data[G9::TERA_TYPE_ORIG] = 0;
    dst.data[G9::TERA_TYPE_OVER] = 0;
    // IsAlpha defaults to 0
    dst.data[G9::IS_ALPHA] = 0;
    return dst;
}

Pokemon PKConvert::adaptPA9toPK9(const Pokemon& src) {
    uint16_t speciesInternal = readU16(src.data.data(), 0x08);
    uint16_t species = SpeciesConverter::getNational9(speciesInternal);
    uint8_t form = src.data[0x24];

    if (!PersonalSV::isPresentInGame(species, form))
        return Pokemon{};

    Pokemon dst = src;
    dst.gameType_ = GameType::S;
    // Set TeraType based on species
    dst.data[G9::TERA_TYPE_ORIG] = PersonalSV::getType1(species, form);
    dst.data[G9::TERA_TYPE_OVER] = 19; // OverrideNone
    // Clear Alpha byte (not applicable in SV)
    dst.data[G9::IS_ALPHA] = 0;
    return dst;
}

// ============================================================================
// Conversion router
// ============================================================================

Pokemon PKConvert::convertTo(const Pokemon& src, GameType target) {
    if (src.isEmpty()) return Pokemon{};

    auto srcFamily = formatFamilyOf(src.gameType_);
    auto dstFamily = formatFamilyOf(target);

    // Same family - just copy with target gameType
    if (srcFamily == dstFamily) {
        Pokemon dst = src;
        dst.gameType_ = target;
        return dst;
    }

    // FRLG not supported
    if (srcFamily == FormatFamily::FRLG || dstFamily == FormatFamily::FRLG)
        return Pokemon{};

    // Cannot transfer back to LGPE
    if (dstFamily == FormatFamily::LGPE)
        return Pokemon{};

    // Step 1: Convert source to PK8 hub format
    Pokemon pk8;
    switch (srcFamily) {
        case FormatFamily::SwSh:
            pk8 = src;
            pk8.gameType_ = GameType::Sw;
            break;
        case FormatFamily::BDSP:
            pk8 = adaptPB8toPK8(src);
            break;
        case FormatFamily::PLA:
            pk8 = convertPA8toPK8(src);
            break;
        case FormatFamily::SV:
            pk8 = convertPK9toPK8(src);
            break;
        case FormatFamily::ZA: {
            // PA9 -> PK9 -> PK8
            Pokemon pk9 = adaptPA9toPK9(src);
            if (pk9.isEmpty()) return Pokemon{};
            pk8 = convertPK9toPK8(pk9);
            break;
        }
        case FormatFamily::LGPE:
            pk8 = convertPB7toPK8(src);
            break;
        default:
            return Pokemon{};
    }

    if (pk8.isEmpty()) return Pokemon{};

    // Step 2: Convert PK8 to target format
    switch (dstFamily) {
        case FormatFamily::SwSh: {
            // Validate species exists in target
            uint16_t sp = readU16(pk8.data.data(), G8::SPECIES);
            uint8_t fm = pk8.data[G8::FORM];
            if (!PersonalSWSH::isPresentInGame(sp, fm)) return Pokemon{};
            pk8.gameType_ = target;
            return pk8;
        }
        case FormatFamily::BDSP:
            return adaptPK8toPB8(pk8);
        case FormatFamily::PLA:
            return convertPK8toPA8(pk8);
        case FormatFamily::SV:
            return convertPK8toPK9(pk8);
        case FormatFamily::ZA: {
            // PK8 -> PK9 -> PA9
            Pokemon pk9 = convertPK8toPK9(pk8);
            if (pk9.isEmpty()) return Pokemon{};
            return adaptPK9toPA9(pk9);
        }
        default:
            return Pokemon{};
    }
}
