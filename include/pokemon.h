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

    // PID: 0x18 (PB7), 0x1C (PK8/PA8/PA9)
    uint32_t pid() const { return readU32(isLGPE(gameType_) ? 0x18 : 0x1C); }

    // Nature: 0x1C (PB7), 0x20 (PK8/PA8/PA9)
    uint8_t nature() const { return isLGPE(gameType_) ? data[0x1C] : data[0x20]; }

    // FatefulEncounter: 0x1D bit 0 (PB7), 0x22 bit 0 (PK8/PA8/PA9)
    bool fatefulEncounter() const {
        int ofs = isLGPE(gameType_) ? 0x1D : 0x22;
        return (data[ofs] & 1) != 0;
    }

    // Gender: 0x1D bits 1-2 (PB7), 0x22 bits 2-3 (PK8/PB8/PA8), 0x22 bits 1-2 (PA9)
    uint8_t gender() const {
        if (isLGPE(gameType_))
            return (data[0x1D] >> 1) & 0x3;
        if (isSwSh(gameType_) || isBDSP(gameType_) || gameType_ == GameType::LA)
            return (data[0x22] >> 2) & 0x3;
        return (data[0x22] >> 1) & 0x3; // PA9
    }

    // Form: 0x1D bits 3-7 (PB7), 0x24 (PK8/PA8/PA9)
    uint8_t form() const { return isLGPE(gameType_) ? (data[0x1D] >> 3) : data[0x24]; }

    // Ability: 0x14 u8 (PB7), 0x14 u16 (PK8/PA8/PA9)
    uint16_t ability() const {
        return isLGPE(gameType_) ? (uint16_t)data[0x14] : readU16(0x14);
    }

    // EVs: 0x1E-0x23 (PB7), 0x26-0x2B (PK8/PA8/PA9)
    uint8_t evHp()  const { return data[isLGPE(gameType_) ? 0x1E : 0x26]; }
    uint8_t evAtk() const { return data[isLGPE(gameType_) ? 0x1F : 0x27]; }
    uint8_t evDef() const { return data[isLGPE(gameType_) ? 0x20 : 0x28]; }
    uint8_t evSpe() const { return data[isLGPE(gameType_) ? 0x21 : 0x29]; }
    uint8_t evSpA() const { return data[isLGPE(gameType_) ? 0x22 : 0x2A]; }
    uint8_t evSpD() const { return data[isLGPE(gameType_) ? 0x23 : 0x2B]; }

    // TID/SID (offsets 0x0C, 0x0E — same for all formats)
    uint16_t tid() const { return readU16(0x0C); }
    uint16_t sid() const { return readU16(0x0E); }

    // --- Fields with game-aware offsets ---

    // Nickname: 0x58 (PK8/PA9), 0x60 (PA8)
    std::string nickname() const;

    // OT Name: 0xF8 (PK8/PA9), 0x110 (PA8)
    std::string otName() const;

    // Moves: 0x5A-0x60 (PB7), 0x54-0x5A (PA8), 0x72-0x78 (PK8/PA9)
    uint16_t move1() const { return readU16(isLGPE(gameType_) ? 0x5A : gameType_ == GameType::LA ? 0x54 : 0x72); }
    uint16_t move2() const { return readU16(isLGPE(gameType_) ? 0x5C : gameType_ == GameType::LA ? 0x56 : 0x74); }
    uint16_t move3() const { return readU16(isLGPE(gameType_) ? 0x5E : gameType_ == GameType::LA ? 0x58 : 0x76); }
    uint16_t move4() const { return readU16(isLGPE(gameType_) ? 0x60 : gameType_ == GameType::LA ? 0x5A : 0x78); }

    // IV32: 0x74 (PB7), 0x94 (PA8), 0x8C (PK8/PA9)
    uint32_t iv32() const {
        if (isLGPE(gameType_)) return readU32(0x74);
        return readU32(gameType_ == GameType::LA ? 0x94 : 0x8C);
    }
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

    // IsAlpha: PA9 → 0x23 != 0, PA8 → 0x16 bit 5, PK8/PB8/PB7 → false
    bool isAlpha() const {
        if (gameType_ == GameType::LA)
            return (data[0x16] >> 5) & 1;
        if (isSwSh(gameType_) || isBDSP(gameType_) || isLGPE(gameType_))
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
