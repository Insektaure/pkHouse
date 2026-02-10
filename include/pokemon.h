#pragma once
#include "poke_crypto.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <array>

// Pokemon PA9 data structure for Gen9 (Pokemon ZA).
// Ported from PKHeX.Core/PKM/PA9.cs
struct Pokemon {
    std::array<uint8_t, PokeCrypto::SIZE_9PARTY> data{};

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

    // --- Block A (0x00 - 0x57) ---

    // 0x00: EncryptionConstant
    uint32_t encryptionConstant() const { return readU32(0x00); }

    // 0x08: SpeciesInternal (raw Gen9 ID)
    uint16_t speciesInternal() const { return readU16(0x08); }

    // National species ID (converted via SpeciesConverter)
    uint16_t species() const;

    // 0x0A: HeldItem
    uint16_t heldItem() const { return readU16(0x0A); }

    // 0x1C: PID
    uint32_t pid() const { return readU32(0x1C); }

    // 0x20: Nature
    uint8_t nature() const { return data[0x20]; }

    // 0x22: FatefulEncounter (bit 0), Gender (bits 1-2)
    bool fatefulEncounter() const { return (data[0x22] & 1) != 0; }
    uint8_t gender() const { return (data[0x22] >> 1) & 0x3; } // 0=M, 1=F, 2=genderless

    // 0x24: Form
    uint8_t form() const { return data[0x24]; }

    // --- Block B (0x58 - 0xA7) ---

    // 0x58: Nickname (13 UTF-16LE chars, 26 bytes)
    std::string nickname() const;

    // 0x72-0x79: Moves (4x u16)
    uint16_t move1() const { return readU16(0x72); }
    uint16_t move2() const { return readU16(0x74); }
    uint16_t move3() const { return readU16(0x76); }
    uint16_t move4() const { return readU16(0x78); }

    // 0x8C: IV32
    uint32_t iv32() const { return readU32(0x8C); }
    bool isEgg() const { return ((iv32() >> 30) & 1) == 1; }
    bool isNicknamed() const { return ((iv32() >> 31) & 1) == 1; }

    // --- Party Stats (0x148 - 0x157) ---

    // 0x148: Level (party format only)
    uint8_t level() const { return data[0x148]; }

    // --- Utility ---

    // True if the slot is empty (no pokemon)
    bool isEmpty() const {
        return encryptionConstant() == 0 && speciesInternal() == 0;
    }

    // Display name for UI: species name (or nickname if nicknamed)
    std::string displayName() const;

    // Load from encrypted raw data
    void loadFromEncrypted(const uint8_t* encrypted, size_t len);

    // Write encrypted data to buffer
    void getEncrypted(uint8_t* outBuf) const;

    // Is this a shiny Pokemon?
    bool isShiny() const {
        uint32_t p = pid();
        uint16_t tid = readU16(0x0C);
        uint16_t sid = readU16(0x0E);
        uint32_t xor_val = (p >> 16) ^ (p & 0xFFFF) ^ tid ^ sid;
        return xor_val < 16;
    }
};
