#pragma once
#include "poke_crypto.h"
#include "game_type.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <array>
#include <vector>
#include <utility>

// EXP table: 6 growth rates x 100 levels (defined in pokemon.cpp)
extern const uint32_t EXP_TABLE[6][100];

// Per-format offset table for Pokemon data fields.
// -1 means the field needs special handling (see accessor comments).
struct PokemonOffsets {
    int speciesInternal;  // readU16
    int heldItem;         // readU16
    int pid;              // readU32
    int nature;           // byte read, -1 = pid % 25 (PK3)
    int fateful;          // byte offset, -1 = u32 bit (PK3)
    int fatefulBit;       // bit position within byte
    int genderByte;       // byte offset, -1 = PID-based (PK3)
    int genderShift;      // right-shift before & 3
    int form;             // byte offset, -1 = always 0 (PK3)
    int formShift;        // right-shift (3 for PB7, 0 otherwise)
    int ball;             // byte offset, -1 = u16 bits (PK3)
    int ability;          // offset, -1 = always 0 (PK3)
    bool abilityIsU8;     // true for PB7
    int evBase;           // evHp = base+0 .. evSpD = base+5
    int tid;              // readU16
    int sid;              // readU16
    int moveBase;         // move1 = base, +2, +4, +6
    int iv32;             // readU32
    int nickname;         // UTF16 offset, -1 = Gen3 encoding
    int otName;           // UTF16 offset, -1 = Gen3 encoding
    int levelByte;        // byte offset, -1 = computed from EXP
    int expOfs;           // u32 offset for EXP, -1 = N/A
    int alphaByte;        // byte for alpha, -1 = always false
    bool alphaIsNonZero;  // true = !=0 (PA9), false = bit5 (PA8)
    int languageByte;     // byte offset for language field
    int formArgument;     // u32 offset (Alcremie decoration), -1 = N/A
    int canGmaxByte;      // byte with Gmax flag (bit 4), -1 = N/A
    int heightScalar;     // u8 offset (LGPE size), -1 = N/A
    int weightScalar;     // u8 offset (LGPE size), -1 = N/A
};

// Returns the offset table for a given game format.
inline const PokemonOffsets& pokemonOffsetsFor(GameType g) {
    //                              spec  held  pid   nat  fate fBit gend gShf form fShf ball  abi  aU8  ev    tid   sid   move  iv32  nick  ot    lvl   exp   alph aNZ  lang  fArg  gmax hSca wSca
    static constexpr PokemonOffsets PK3 = {0x20, 0x22, 0x00, -1,  -1,  31,  -1,  0,   -1,  0,   -1,  -1,  false, 0x38, 0x04, 0x06, 0x2C, 0x48, -1,   -1,   -1,   0x24, -1,  false, 0x12, -1,   -1,   -1,  -1};
    static constexpr PokemonOffsets PB7 = {0x08, 0x0A, 0x18, 0x1C, 0x1D, 0,  0x1D, 1,  0x1D, 3,  0xDC, 0x14, true, 0x1E, 0x0C, 0x0E, 0x5A, 0x74, 0x40, 0xB0, 0xEC, -1,   -1,  false, 0xE3, 0x3C, -1,   0x3A,0x3B};
    static constexpr PokemonOffsets PK8 = {0x08, 0x0A, 0x1C, 0x20, 0x22, 0,  0x22, 2,  0x24, 0,  0x124, 0x14, false, 0x26, 0x0C, 0x0E, 0x72, 0x8C, 0x58, 0xF8, 0x148, -1,  -1,  false, 0xE2, 0xE4, 0x16, -1,  -1};
    static constexpr PokemonOffsets PA8 = {0x08, 0x0A, 0x1C, 0x20, 0x22, 0,  0x22, 2,  0x24, 0,  0x137, 0x14, false, 0x26, 0x0C, 0x0E, 0x54, 0x94, 0x60, 0x110, -1,   0x10, 0x16, false, 0xF2, 0xE4, -1,   -1,  -1};
    static constexpr PokemonOffsets PA9 = {0x08, 0x0A, 0x1C, 0x20, 0x22, 0,  0x22, 1,  0x24, 0,  0x124, 0x14, false, 0x26, 0x0C, 0x0E, 0x72, 0x8C, 0x58, 0xF8, 0x148, -1,  0x23, true,  0xD5, 0xD0, -1,   -1,  -1};
    if (isFRLG(g)) return PK3;
    if (isLGPE(g)) return PB7;
    if (g == GameType::LA) return PA8;
    if (isSV(g) || g == GameType::ZA) return PA9;
    return PK8; // SwSh, BDSP
}

// Pokemon data structure for Gen3/Gen6/Gen8/Gen8a/Gen9 (PK3/PB7/PK8/PA8/PA9).
// Ported from PKHeX.Core/PKM/PA9.cs, G8PKM.cs, PA8.cs, PK3.cs
struct Pokemon {
    std::array<uint8_t, PokeCrypto::MAX_PARTY_SIZE> data{};
    GameType gameType_ = GameType::ZA;

    // --- Helpers ---
    const PokemonOffsets& ofs() const { return pokemonOffsetsFor(gameType_); }

    uint16_t readU16(int o) const {
        uint16_t v;
        std::memcpy(&v, data.data() + o, 2);
        return v;
    }
    uint32_t readU32(int o) const {
        uint32_t v;
        std::memcpy(&v, data.data() + o, 4);
        return v;
    }
    void writeU16(int o, uint16_t v) {
        std::memcpy(data.data() + o, &v, 2);
    }
    void writeU32(int o, uint32_t v) {
        std::memcpy(data.data() + o, &v, 4);
    }

    // --- Block A (offset-table driven) ---

    uint32_t encryptionConstant() const { return readU32(0x00); }

    uint16_t speciesInternal() const { return readU16(ofs().speciesInternal); }

    // National species ID (converted via SpeciesConverter)
    uint16_t species() const;

    uint16_t heldItem() const { return readU16(ofs().heldItem); }

    uint32_t pid() const { return readU32(ofs().pid); }

    // Nature: byte read for modern, pid % 25 for PK3
    uint8_t nature() const {
        int o = ofs().nature;
        return o >= 0 ? data[o] : static_cast<uint8_t>(pid() % 25);
    }

    // FatefulEncounter: bit in byte for modern, bit 31 of u32@0x4C for PK3
    bool fatefulEncounter() const {
        auto& o = ofs();
        if (o.fateful < 0) return (readU32(0x4C) >> 31) & 1;
        return (data[o.fateful] >> o.fatefulBit) & 1;
    }

    // Gender: byte bits for modern, PID-based for PK3 (impl in pokemon.cpp)
    uint8_t gender() const;

    // Form: byte for modern, always 0 for PK3
    uint8_t form() const {
        auto& o = ofs();
        return o.form >= 0 ? static_cast<uint8_t>(data[o.form] >> o.formShift) : 0;
    }

    // Ball: byte for modern, bits 11-14 of u16@0x46 for PK3
    uint8_t ball() const {
        int o = ofs().ball;
        return o >= 0 ? data[o] : static_cast<uint8_t>((readU16(0x46) >> 11) & 0xF);
    }

    // Ability: u16 for modern, u8 for PB7, 0 for PK3
    uint16_t ability() const {
        auto& o = ofs();
        if (o.ability < 0) return 0;
        return o.abilityIsU8 ? static_cast<uint16_t>(data[o.ability]) : readU16(o.ability);
    }

    // EVs
    uint8_t evHp()  const { return data[ofs().evBase + 0]; }
    uint8_t evAtk() const { return data[ofs().evBase + 1]; }
    uint8_t evDef() const { return data[ofs().evBase + 2]; }
    uint8_t evSpe() const { return data[ofs().evBase + 3]; }
    uint8_t evSpA() const { return data[ofs().evBase + 4]; }
    uint8_t evSpD() const { return data[ofs().evBase + 5]; }

    // TID/SID
    uint16_t tid() const { return readU16(ofs().tid); }
    uint16_t sid() const { return readU16(ofs().sid); }

    // Display TID/SID: Gen7+ uses 6-digit/4-digit format, Gen3 uses raw 16-bit
    uint32_t displayTid() const {
        if (isFRLG(gameType_)) return tid();
        uint32_t combined = (static_cast<uint32_t>(sid()) << 16) | tid();
        return combined % 1000000;
    }
    uint32_t displaySid() const {
        if (isFRLG(gameType_)) return sid();
        uint32_t combined = (static_cast<uint32_t>(sid()) << 16) | tid();
        return combined / 1000000;
    }

    // --- String fields (impl in pokemon.cpp) ---

    std::string nickname() const;
    std::string otName() const;

    // Moves
    uint16_t move1() const { return readU16(ofs().moveBase + 0); }
    uint16_t move2() const { return readU16(ofs().moveBase + 2); }
    uint16_t move3() const { return readU16(ofs().moveBase + 4); }
    uint16_t move4() const { return readU16(ofs().moveBase + 6); }

    // IV32: bit-packed IV word
    uint32_t iv32() const { return readU32(ofs().iv32); }
    bool isEgg() const { return ((iv32() >> 30) & 1) == 1; }
    bool isNicknamed() const {
        if (isFRLG(gameType_)) return true;
        return ((iv32() >> 31) & 1) == 1;
    }

    // IVs (from iv32 bit-packed — same layout for all formats)
    int ivHp()  const { return (iv32() >>  0) & 0x1F; }
    int ivAtk() const { return (iv32() >>  5) & 0x1F; }
    int ivDef() const { return (iv32() >> 10) & 0x1F; }
    int ivSpe() const { return (iv32() >> 15) & 0x1F; }
    int ivSpA() const { return (iv32() >> 20) & 0x1F; }
    int ivSpD() const { return (iv32() >> 25) & 0x1F; }

    // Level (impl in pokemon.cpp — byte read or computed from EXP)
    uint8_t level() const;

    // --- Utility ---

    bool isEmpty() const {
        return encryptionConstant() == 0 && speciesInternal() == 0;
    }

    std::string displayName() const;

    void refreshChecksum();
    void loadFromEncrypted(const uint8_t* encrypted, size_t len);
    void getEncrypted(uint8_t* outBuf);

    // Language: byte at format-specific offset
    uint8_t language() const {
        int o = ofs().languageByte;
        return (o >= 0) ? data[o] : 0;
    }

    // FormArgument: u32 (Alcremie decoration in low byte)
    uint32_t formArgument() const {
        int o = ofs().formArgument;
        return (o >= 0) ? readU32(o) : 0;
    }

    // CanGigantamax: PK8 byte 0x16 bit 4
    bool canGigantamax() const {
        int o = ofs().canGmaxByte;
        return (o >= 0) && (data[o] & 0x10) != 0;
    }

    // Height/Weight scalars (LGPE size tracking, u8 0-255)
    uint8_t heightScalar() const {
        int o = ofs().heightScalar;
        return (o >= 0) ? data[o] : 0;
    }
    uint8_t weightScalar() const {
        int o = ofs().weightScalar;
        return (o >= 0) ? data[o] : 0;
    }

    // IsAlpha: PA9 → 0x23 != 0, PA8 → 0x16 bit 5, others → false
    bool isAlpha() const {
        auto& o = ofs();
        if (o.alphaByte < 0) return false;
        return o.alphaIsNonZero ? (data[o.alphaByte] != 0)
                                : ((data[o.alphaByte] >> 5) & 1);
    }

    // Ribbon/mark info
    struct RibbonInfo {
        const char* name;     // Display name
        const char* filename; // romfs filename (without path/extension)
        bool isMark;
    };

    // Returns list of all set ribbons/marks
    std::vector<RibbonInfo> getRibbonsAndMarks() const;

    // Shiny: XOR == 0 for Gen3, XOR < 16 for modern
    bool isShiny() const {
        uint32_t p = pid();
        uint16_t t = tid();
        uint16_t s = sid();
        uint32_t xor_val = (p >> 16) ^ (p & 0xFFFF) ^ t ^ s;
        if (isFRLG(gameType_)) return xor_val == 0;
        return xor_val < 16;
    }
};
