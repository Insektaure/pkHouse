#pragma once
#include "poke_crypto.h"
#include "game_type.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <array>

// Pokemon data structure for Gen8/Gen8a/Gen9 (PK8/PA8/PA9).
// Ported from PKHeX.Core/PKM/PA9.cs, G8PKM.cs, PA8.cs
struct Pokemon {
    std::array<uint8_t, PokeCrypto::MAX_PARTY_SIZE> data{};
    GameType gameType_ = GameType::ZA;

    // --- Helpers ---
    uint16_t readU16(int ofs) const {
        uint16_t v;
        std::memcpy(&v, data.data() + ofs, 2);
        return v;
    }
    uint32_t readU32(int ofs) const {
        uint32_t v;
        std::memcpy(&v, data.data() + ofs, 4);
        return v;
    }
    void writeU16(int ofs, uint16_t v) {
        std::memcpy(data.data() + ofs, &v, 2);
    }
    void writeU32(int ofs, uint32_t v) {
        std::memcpy(data.data() + ofs, &v, 4);
    }

    // --- Block A (shared offsets across all formats) ---

    // 0x00: EncryptionConstant
    uint32_t encryptionConstant() const { return readU32(0x00); }

    // 0x08: Species (national ID for Gen8/8a, internal for Gen9)
    uint16_t speciesInternal() const { return readU16(0x08); }

    // National species ID (converted via SpeciesConverter for Gen9)
    uint16_t species() const;

    // 0x0A: HeldItem
    uint16_t heldItem() const { return readU16(0x0A); }

    // 0x1C: PID
    uint32_t pid() const { return readU32(0x1C); }

    // 0x20: Nature
    uint8_t nature() const { return data[0x20]; }

    // 0x22: Gender (PK8/PB8/PA8: bits 2-3, PA9: bits 1-2)
    bool fatefulEncounter() const { return (data[0x22] & 1) != 0; }
    uint8_t gender() const {
        if (isSwSh(gameType_) || isBDSP(gameType_) || gameType_ == GameType::LA)
            return (data[0x22] >> 2) & 0x3; // PK8/PB8/PA8: bits 2-3
        return (data[0x22] >> 1) & 0x3;     // PA9: bits 1-2
    }

    // 0x24: Form
    uint8_t form() const { return data[0x24]; }

    // 0x14: Ability
    uint16_t ability() const { return readU16(0x14); }

    // EVs (offsets 0x26-0x2B — same for all formats)
    uint8_t evHp()  const { return data[0x26]; }
    uint8_t evAtk() const { return data[0x27]; }
    uint8_t evDef() const { return data[0x28]; }
    uint8_t evSpe() const { return data[0x29]; }
    uint8_t evSpA() const { return data[0x2A]; }
    uint8_t evSpD() const { return data[0x2B]; }

    // TID/SID (offsets 0x0C, 0x0E — same for all formats)
    uint16_t tid() const { return readU16(0x0C); }
    uint16_t sid() const { return readU16(0x0E); }

    // --- Fields with game-aware offsets ---

    // Nickname: 0x58 (PK8/PA9), 0x60 (PA8)
    std::string nickname() const;

    // OT Name: 0xF8 (PK8/PA9), 0x110 (PA8)
    std::string otName() const;

    // Moves: 0x72-0x78 (PK8/PA9), 0x54-0x5A (PA8)
    uint16_t move1() const { return readU16(gameType_ == GameType::LA ? 0x54 : 0x72); }
    uint16_t move2() const { return readU16(gameType_ == GameType::LA ? 0x56 : 0x74); }
    uint16_t move3() const { return readU16(gameType_ == GameType::LA ? 0x58 : 0x76); }
    uint16_t move4() const { return readU16(gameType_ == GameType::LA ? 0x5A : 0x78); }

    // IV32: 0x8C (PK8/PA9), 0x94 (PA8)
    uint32_t iv32() const { return readU32(gameType_ == GameType::LA ? 0x94 : 0x8C); }
    bool isEgg() const { return ((iv32() >> 30) & 1) == 1; }
    bool isNicknamed() const { return ((iv32() >> 31) & 1) == 1; }

    // IVs (from iv32 bit-packed)
    int ivHp()  const { return (iv32() >>  0) & 0x1F; }
    int ivAtk() const { return (iv32() >>  5) & 0x1F; }
    int ivDef() const { return (iv32() >> 10) & 0x1F; }
    int ivSpe() const { return (iv32() >> 15) & 0x1F; }
    int ivSpA() const { return (iv32() >> 20) & 0x1F; }
    int ivSpD() const { return (iv32() >> 25) & 0x1F; }

    // Level: 0x148 (PK8/PA9), calculated from EXP for PA8 (LA box data has no level)
    uint8_t level() const;

    // --- Utility ---

    bool isEmpty() const {
        return encryptionConstant() == 0 && speciesInternal() == 0;
    }

    std::string displayName() const;

    void refreshChecksum();
    void loadFromEncrypted(const uint8_t* encrypted, size_t len);
    void getEncrypted(uint8_t* outBuf);

    // IsAlpha: PA9 → 0x23 != 0, PA8 → 0x16 bit 5, PK8/PB8 → false
    bool isAlpha() const {
        if (gameType_ == GameType::LA)
            return (data[0x16] >> 5) & 1;
        if (isSwSh(gameType_) || isBDSP(gameType_))
            return false;
        return data[0x23] != 0; // PA9 (ZA/SV)
    }

    bool isShiny() const {
        uint32_t p = pid();
        uint16_t t = readU16(0x0C);
        uint16_t s = readU16(0x0E);
        uint32_t xor_val = (p >> 16) ^ (p & 0xFFFF) ^ t ^ s;
        return xor_val < 16;
    }
};
