#pragma once
#include "pokemon.h"
#include "game_type.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <array>

// Trainer info for wondercard injection (from save file or preset)
struct TrainerInfo {
    uint32_t id32 = 0;
    uint8_t  gender = 0;
    uint8_t  language = 0;
    std::u16string otName;
    bool valid = false;
};

// Summary info for a scanned wondercard file
struct WCInfo {
    std::string filename;
    std::string path;
    uint16_t cardID;
    uint16_t species;    // national dex ID
    uint8_t  level;
    bool     isShiny;
    bool     hasOT;      // wondercard has preset OT (can inject to bank)
    bool     valid;      // file loaded and is a Pokemon wondercard
};

// WC9 wondercard parser (0x2C8 bytes)
// Ported from PKHeX.Core/Mystry/WC9.cs
struct WC9 {
    static constexpr int SIZE = 0x2C8;

    std::array<uint8_t, SIZE> data{};

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

    // --- Field accessors ---
    uint16_t cardID()          const { return readU16(0x08); }
    uint8_t  cardType()        const { return data[0x11]; }
    uint16_t restrictVersion() const { return readU16(0x0E); }
    uint16_t scale()           const { return readU16(0x0C); }

    uint32_t id32()            const { return readU32(0x18); }
    uint8_t  originGame()      const { return data[0x1C]; }
    uint32_t encryptionConst() const { return readU32(0x20); }
    uint32_t pid()             const { return readU32(0x24); }

    // Pokemon data
    uint16_t speciesInternal() const { return readU16(0x238); }
    uint8_t  form()            const { return data[0x23A]; }
    uint8_t  gender()          const { return data[0x23B]; }
    uint8_t  level()           const { return data[0x23C]; }
    uint8_t  nature()          const { return data[0x23E]; }
    uint8_t  abilityType()     const { return data[0x23F]; }
    uint8_t  pidType()         const { return data[0x240]; }
    uint8_t  metLevel()        const { return data[0x241]; }
    uint8_t  teraTypeOrig()    const { return data[0x242]; }
    uint8_t  heightScalar()    const { return data[0x244]; }
    uint8_t  weightScalar()    const { return data[0x246]; }

    // Moves
    uint16_t move1()           const { return readU16(0x228); }
    uint16_t move2()           const { return readU16(0x22A); }
    uint16_t move3()           const { return readU16(0x22C); }
    uint16_t move4()           const { return readU16(0x22E); }
    uint16_t relearnMove1()    const { return readU16(0x230); }
    uint16_t relearnMove2()    const { return readU16(0x232); }
    uint16_t relearnMove3()    const { return readU16(0x234); }
    uint16_t relearnMove4()    const { return readU16(0x236); }

    // Ball, item, locations
    uint16_t ball()            const { return readU16(0x224); }
    uint16_t heldItem()        const { return readU16(0x226); }
    uint16_t eggLocation()     const { return readU16(0x220); }
    uint16_t metLocation()     const { return readU16(0x222); }

    // IVs (6 individual bytes)
    uint8_t ivHp()             const { return data[0x268]; }
    uint8_t ivAtk()            const { return data[0x269]; }
    uint8_t ivDef()            const { return data[0x26A]; }
    uint8_t ivSpe()            const { return data[0x26B]; }
    uint8_t ivSpA()            const { return data[0x26C]; }
    uint8_t ivSpD()            const { return data[0x26D]; }

    // EVs (6 individual bytes)
    uint8_t otGender()         const { return data[0x26E]; }
    uint8_t evHp()             const { return data[0x26F]; }
    uint8_t evAtk()            const { return data[0x270]; }
    uint8_t evDef()            const { return data[0x271]; }
    uint8_t evSpe()            const { return data[0x272]; }
    uint8_t evSpA()            const { return data[0x273]; }
    uint8_t evSpD()            const { return data[0x274]; }

    // OT Memory fields
    uint8_t otMemoryIntensity()const { return data[0x275]; }
    uint8_t otMemory()         const { return data[0x276]; }
    uint8_t otMemoryFeeling()  const { return data[0x277]; }
    uint16_t otMemoryVariable()const { return readU16(0x278); }

    // Ribbon bytes: 0x248 to 0x267 (32 bytes)
    const uint8_t* ribbonData() const { return data.data() + 0x248; }
    static constexpr int RIBBON_SIZE = 0x20;

    // Nickname: 0x28 + langIdx * 0x1C (UTF-16LE, up to 13 chars)
    std::u16string getNickname(int langIdx) const;
    bool hasNickname(int langIdx) const;

    // OT Name: 0x124 + langIdx * 0x1C (UTF-16LE, up to 13 chars)
    std::u16string getOTName(int langIdx) const;

    // --- Key methods ---
    bool isPokemon() const { return cardType() == 1; }
    bool getHasOT(int langIdx) const;
    bool canInjectToBank(int langIdx) const { return getHasOT(langIdx); }

    // Convert to PK9 data
    Pokemon convertToPK9(const TrainerInfo& trainer) const;

    // Load from file
    bool loadFromFile(const std::string& path);
};

// Scan wondercard directory for .wc9 files
std::vector<WCInfo> scanWondercards(const std::string& basePath, GameType game);
