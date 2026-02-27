#pragma once
#include "poke_crypto.h"
#include "game_type.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <array>

// Pokemon data structure for Gen3/Gen6/Gen8/Gen8a/Gen9 (PK3/PB7/PK8/PA8/PA9).
// Ported from PKHeX.Core/PKM/PA9.cs, G8PKM.cs, PA8.cs, PK3.cs
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

    // --- Block A (game-aware offsets) ---

    // EncryptionConstant: 0x00 for all formats (PK3: this is PID)
    uint32_t encryptionConstant() const { return readU32(0x00); }

    // Species internal ID: 0x20 (PK3), 0x08 (all others)
    uint16_t speciesInternal() const {
        return readU16(isFRLG(gameType_) ? 0x20 : 0x08);
    }

    // National species ID (converted via SpeciesConverter)
    uint16_t species() const;

    // HeldItem: 0x22 (PK3), 0x0A (all others)
    uint16_t heldItem() const {
        return readU16(isFRLG(gameType_) ? 0x22 : 0x0A);
    }

    // PID: 0x00 (PK3), 0x18 (PB7), 0x1C (PK8/PA8/PA9)
    uint32_t pid() const {
        if (isFRLG(gameType_)) return readU32(0x00);
        return readU32(isLGPE(gameType_) ? 0x18 : 0x1C);
    }

    // Nature: PID % 25 (PK3), 0x1C (PB7), 0x20 (PK8/PA8/PA9)
    uint8_t nature() const {
        if (isFRLG(gameType_)) return pid() % 25;
        return isLGPE(gameType_) ? data[0x1C] : data[0x20];
    }

    // FatefulEncounter: bit 31 of RIB0 at 0x4C (PK3), 0x1D bit 0 (PB7), 0x22 bit 0 (PK8/PA8/PA9)
    bool fatefulEncounter() const {
        if (isFRLG(gameType_)) return (readU32(0x4C) >> 31) & 1;
        int ofs = isLGPE(gameType_) ? 0x1D : 0x22;
        return (data[ofs] & 1) != 0;
    }

    // Gender: derived from PID (PK3), 0x1D bits 1-2 (PB7), 0x22 bits (PK8/PA8/PA9)
    uint8_t gender() const;

    // Form: always 0 for PK3, 0x1D bits 3-7 (PB7), 0x24 (PK8/PA8/PA9)
    uint8_t form() const {
        if (isFRLG(gameType_)) return 0;
        return isLGPE(gameType_) ? (data[0x1D] >> 3) : data[0x24];
    }

    // Ability: 0 for PK3 (not stored), 0x14 u8 (PB7), 0x14 u16 (PK8/PA8/PA9)
    uint16_t ability() const {
        if (isFRLG(gameType_)) return 0;
        return isLGPE(gameType_) ? (uint16_t)data[0x14] : readU16(0x14);
    }

    // EVs: 0x38-0x3D (PK3), 0x1E-0x23 (PB7), 0x26-0x2B (PK8/PA8/PA9)
    uint8_t evHp()  const { return data[isFRLG(gameType_) ? 0x38 : isLGPE(gameType_) ? 0x1E : 0x26]; }
    uint8_t evAtk() const { return data[isFRLG(gameType_) ? 0x39 : isLGPE(gameType_) ? 0x1F : 0x27]; }
    uint8_t evDef() const { return data[isFRLG(gameType_) ? 0x3A : isLGPE(gameType_) ? 0x20 : 0x28]; }
    uint8_t evSpe() const { return data[isFRLG(gameType_) ? 0x3B : isLGPE(gameType_) ? 0x21 : 0x29]; }
    uint8_t evSpA() const { return data[isFRLG(gameType_) ? 0x3C : isLGPE(gameType_) ? 0x22 : 0x2A]; }
    uint8_t evSpD() const { return data[isFRLG(gameType_) ? 0x3D : isLGPE(gameType_) ? 0x23 : 0x2B]; }

    // TID/SID: 0x04/0x06 (PK3), 0x0C/0x0E (all others)
    uint16_t tid() const { return readU16(isFRLG(gameType_) ? 0x04 : 0x0C); }
    uint16_t sid() const { return readU16(isFRLG(gameType_) ? 0x06 : 0x0E); }

    // --- Fields with game-aware offsets ---

    // Nickname: 0x08 Gen3 encoded (PK3), 0x40 (PB7), 0x58 (PK8/PA9), 0x60 (PA8)
    std::string nickname() const;

    // OT Name: 0x14 Gen3 encoded (PK3), 0xB0 (PB7), 0xF8 (PK8/PA9), 0x110 (PA8)
    std::string otName() const;

    // Moves: 0x2C-0x32 (PK3), 0x5A-0x60 (PB7), 0x54-0x5A (PA8), 0x72-0x78 (PK8/PA9)
    uint16_t move1() const {
        if (isFRLG(gameType_)) return readU16(0x2C);
        return readU16(isLGPE(gameType_) ? 0x5A : gameType_ == GameType::LA ? 0x54 : 0x72);
    }
    uint16_t move2() const {
        if (isFRLG(gameType_)) return readU16(0x2E);
        return readU16(isLGPE(gameType_) ? 0x5C : gameType_ == GameType::LA ? 0x56 : 0x74);
    }
    uint16_t move3() const {
        if (isFRLG(gameType_)) return readU16(0x30);
        return readU16(isLGPE(gameType_) ? 0x5E : gameType_ == GameType::LA ? 0x58 : 0x76);
    }
    uint16_t move4() const {
        if (isFRLG(gameType_)) return readU16(0x32);
        return readU16(isLGPE(gameType_) ? 0x60 : gameType_ == GameType::LA ? 0x5A : 0x78);
    }

    // IV32: 0x48 (PK3), 0x74 (PB7), 0x94 (PA8), 0x8C (PK8/PA9)
    uint32_t iv32() const {
        if (isFRLG(gameType_)) return readU32(0x48);
        if (isLGPE(gameType_)) return readU32(0x74);
        return readU32(gameType_ == GameType::LA ? 0x94 : 0x8C);
    }
    bool isEgg() const { return ((iv32() >> 30) & 1) == 1; }
    bool isNicknamed() const {
        if (isFRLG(gameType_)) return true; // PK3 always stores name in nickname field
        return ((iv32() >> 31) & 1) == 1;
    }

    // IVs (from iv32 bit-packed — same layout for all formats)
    int ivHp()  const { return (iv32() >>  0) & 0x1F; }
    int ivAtk() const { return (iv32() >>  5) & 0x1F; }
    int ivDef() const { return (iv32() >> 10) & 0x1F; }
    int ivSpe() const { return (iv32() >> 15) & 0x1F; }
    int ivSpA() const { return (iv32() >> 20) & 0x1F; }
    int ivSpD() const { return (iv32() >> 25) & 0x1F; }

    // Level: party 0x54 or from EXP (PK3), 0xEC (PB7), 0x148 (PK8/PA9), from EXP (PA8)
    uint8_t level() const;

    // --- Utility ---

    bool isEmpty() const {
        return encryptionConstant() == 0 && speciesInternal() == 0;
    }

    std::string displayName() const;

    void refreshChecksum();
    void loadFromEncrypted(const uint8_t* encrypted, size_t len);
    void getEncrypted(uint8_t* outBuf);

    // IsAlpha: PA9 → 0x23 != 0, PA8 → 0x16 bit 5, others → false
    bool isAlpha() const {
        if (isFRLG(gameType_)) return false;
        if (gameType_ == GameType::LA)
            return (data[0x16] >> 5) & 1;
        if (isSwSh(gameType_) || isBDSP(gameType_) || isLGPE(gameType_))
            return false;
        return data[0x23] != 0; // PA9 (ZA/SV)
    }

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
