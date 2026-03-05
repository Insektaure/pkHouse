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
    uint8_t  gameVersion = 0; // SL=50, VL=51
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

    bool isEgg() const { return data[0x23D] == 1; }

    // --- Key methods ---
    bool isPokemon() const { return cardType() == 1; }
    bool getHasOT(int langIdx) const;
    bool canInjectToBank(int langIdx) const { return getHasOT(langIdx); }

    // Convert to PK9 data
    Pokemon convertToPK9(const TrainerInfo& trainer) const;

    // Load from file
    bool loadFromFile(const std::string& path);
};

// WC8 wondercard parser (0x2D0 bytes)
// Ported from PKHeX.Core/MysteryGifts/WC8.cs
struct WC8 {
    static constexpr int SIZE = 0x2D0;

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
    int32_t readI32(int ofs) const {
        int32_t v;
        std::memcpy(&v, data.data() + ofs, 4);
        return v;
    }

    // --- Field accessors ---
    uint16_t cardID()          const { return readU16(0x08); }
    uint8_t  cardType()        const { return data[0x11]; }
    uint8_t  restrictVersion() const { return data[0x0E]; }
    uint16_t checksum()        const { return readU16(0x2CC); } // for HOME gift date lookup

    uint32_t id32()            const { return readU32(0x20); }
    uint16_t tid16()           const { return readU16(0x20); }
    uint16_t sid16()           const { return readU16(0x22); }
    int32_t  originGame()      const { return readI32(0x24); }
    uint32_t encryptionConst() const { return readU32(0x28); }
    uint32_t pid()             const { return readU32(0x2C); }

    // Pokemon data
    uint16_t species()         const { return readU16(0x240); } // National dex ID
    uint8_t  form()            const { return data[0x242]; }
    uint8_t  gender()          const { return data[0x243]; }
    uint8_t  level()           const { return data[0x244]; }
    uint8_t  nature()          const { return data[0x246]; }
    uint8_t  abilityType()     const { return data[0x247]; }
    uint8_t  pidType()         const { return data[0x248]; }
    uint8_t  metLevel()        const { return data[0x249]; }
    uint8_t  dynamaxLevel()    const { return data[0x24A]; }
    uint8_t  canGigantamax()   const { return data[0x24B]; }

    // Moves
    uint16_t move1()           const { return readU16(0x230); }
    uint16_t move2()           const { return readU16(0x232); }
    uint16_t move3()           const { return readU16(0x234); }
    uint16_t move4()           const { return readU16(0x236); }
    uint16_t relearnMove1()    const { return readU16(0x238); }
    uint16_t relearnMove2()    const { return readU16(0x23A); }
    uint16_t relearnMove3()    const { return readU16(0x23C); }
    uint16_t relearnMove4()    const { return readU16(0x23E); }

    // Ball, item, locations
    uint16_t ball()            const { return readU16(0x22C); }
    uint16_t heldItem()        const { return readU16(0x22E); }
    uint16_t eggLocation()     const { return readU16(0x228); }
    uint16_t metLocation()     const { return readU16(0x22A); }

    // IVs (6 individual bytes)
    uint8_t ivHp()             const { return data[0x26C]; }
    uint8_t ivAtk()            const { return data[0x26D]; }
    uint8_t ivDef()            const { return data[0x26E]; }
    uint8_t ivSpe()            const { return data[0x26F]; }
    uint8_t ivSpA()            const { return data[0x270]; }
    uint8_t ivSpD()            const { return data[0x271]; }

    // OT Gender, EVs
    uint8_t otGender()         const { return data[0x272]; }
    uint8_t evHp()             const { return data[0x273]; }
    uint8_t evAtk()            const { return data[0x274]; }
    uint8_t evDef()            const { return data[0x275]; }
    uint8_t evSpe()            const { return data[0x276]; }
    uint8_t evSpA()            const { return data[0x277]; }
    uint8_t evSpD()            const { return data[0x278]; }

    // OT Memory fields
    uint8_t otMemoryIntensity()const { return data[0x279]; }
    uint8_t otMemory()         const { return data[0x27A]; }
    uint8_t otMemoryFeeling()  const { return data[0x27B]; }
    uint16_t otMemoryVariable()const { return readU16(0x27C); }

    // Ribbon bytes: 0x24C to 0x26B (32 bytes)
    const uint8_t* ribbonData() const { return data.data() + 0x24C; }
    static constexpr int RIBBON_SIZE = 0x20;

    // Nickname: 0x30 + langIdx * 0x1C (UTF-16LE, up to 12 chars)
    std::u16string getNickname(int langIdx) const;
    bool hasNickname(int langIdx) const;

    // OT Name: 0x12C + langIdx * 0x1C (UTF-16LE, up to 12 chars)
    std::u16string getOTName(int langIdx) const;

    bool isEgg() const { return data[0x245] == 1; }
    bool isHOMEGift() const { return cardID() >= 9000; }

    // --- Key methods ---
    bool isPokemon() const { return cardType() == 1; }
    bool getHasOT(int langIdx) const;
    bool canInjectToBank(int langIdx) const { return getHasOT(langIdx); }

    // Convert to PK8 data
    Pokemon convertToPK8(const TrainerInfo& trainer) const;

    // Load from file
    bool loadFromFile(const std::string& path);
};

// WA9 wondercard parser (0x2C8 bytes) — Pokemon Legends: Z-A
// Ported from PKHeX.Core/MysteryGifts/WA9.cs
struct WA9 {
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
    int32_t readI32(int ofs) const {
        int32_t v;
        std::memcpy(&v, data.data() + ofs, 4);
        return v;
    }

    // --- Field accessors (offsets from PKHeX WA9.cs) ---
    uint16_t cardID()          const { return readU16(0x08); }
    uint8_t  cardType()        const { return data[0x11]; }
    uint8_t  restrictVersion() const { return data[0x0E]; }
    uint16_t checksum()        const { return readU16(0x2C0); }

    uint32_t id32()            const { return readU32(0x18); }
    int32_t  originGame()      const { return readI32(0x1C); }
    uint32_t encryptionConst() const { return readU32(0x20); }
    uint32_t pid()             const { return readU32(0x24); }

    // Pokemon data
    uint16_t speciesInternal() const { return readU16(0x270); }
    uint8_t  form()            const { return data[0x272]; }
    uint8_t  gender()          const { return data[0x273]; }
    uint8_t  level()           const { return data[0x274]; }
    uint8_t  nature()          const { return data[0x276]; }
    uint8_t  abilityType()     const { return data[0x277]; }
    uint8_t  pidType()         const { return data[0x278]; }
    uint8_t  metLevel()        const { return data[0x279]; }

    // Scale and Alpha
    uint16_t scale()           const { return readU16(0x2AC); }
    uint8_t  isAlpha()         const { return data[0x2AE]; }

    // Moves
    uint16_t move1()           const { return readU16(0x260); }
    uint16_t move2()           const { return readU16(0x262); }
    uint16_t move3()           const { return readU16(0x264); }
    uint16_t move4()           const { return readU16(0x266); }
    uint16_t relearnMove1()    const { return readU16(0x268); }
    uint16_t relearnMove2()    const { return readU16(0x26A); }
    uint16_t relearnMove3()    const { return readU16(0x26C); }
    uint16_t relearnMove4()    const { return readU16(0x26E); }

    // Ball, item, locations
    uint16_t ball()            const { return readU16(0x25C); }
    uint16_t heldItem()        const { return readU16(0x25E); }
    uint16_t eggLocation()     const { return readU16(0x258); }
    uint16_t metLocation()     const { return readU16(0x25A); }

    // IVs (6 individual bytes at 0x29A-0x29F)
    uint8_t ivHp()             const { return data[0x29A]; }
    uint8_t ivAtk()            const { return data[0x29B]; }
    uint8_t ivDef()            const { return data[0x29C]; }
    uint8_t ivSpe()            const { return data[0x29D]; }
    uint8_t ivSpA()            const { return data[0x29E]; }
    uint8_t ivSpD()            const { return data[0x29F]; }

    // OT Gender, EVs
    uint8_t otGender()         const { return data[0x2A0]; }
    uint8_t evHp()             const { return data[0x2A1]; }
    uint8_t evAtk()            const { return data[0x2A2]; }
    uint8_t evDef()            const { return data[0x2A3]; }
    uint8_t evSpe()            const { return data[0x2A4]; }
    uint8_t evSpA()            const { return data[0x2A5]; }
    uint8_t evSpD()            const { return data[0x2A6]; }

    // OT Memory fields
    uint8_t otMemoryIntensity()const { return data[0x2A7]; }
    uint8_t otMemory()         const { return data[0x2A8]; }
    uint8_t otMemoryFeeling()  const { return data[0x2A9]; }
    uint16_t otMemoryVariable()const { return readU16(0x2AA); }

    // Ribbon bytes: 0x27A to 0x299 (32 bytes) — NOT USED in conversion (commented out in PKHeX)
    const uint8_t* ribbonData() const { return data.data() + 0x27A; }
    static constexpr int RIBBON_SIZE = 0x20;

    // Nickname: 0x28 + langIdx * 0x1C (UTF-16LE, up to 13 chars) — same base as WC9
    std::u16string getNickname(int langIdx) const;
    bool hasNickname(int langIdx) const;

    // OT Name: 0x140 + langIdx * 0x1C (different from WC9's 0x124)
    std::u16string getOTName(int langIdx) const;

    bool isEgg() const { return data[0x275] != 0; }

    // --- Key methods ---
    bool isPokemon() const { return cardType() == 1; }
    bool getHasOT(int langIdx) const;
    bool canInjectToBank(int langIdx) const { return getHasOT(langIdx); }

    // Convert to PA9 data (ZA format)
    Pokemon convertToPA9(const TrainerInfo& trainer) const;

    // Load from file
    bool loadFromFile(const std::string& path);
};

// WB8 wondercard parser (0x2DC bytes) — Brilliant Diamond / Shining Pearl
// Ported from PKHeX.Core/MysteryGifts/WB8.cs
struct WB8 {
    static constexpr int SIZE = 0x2DC;

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
    int32_t readI32(int ofs) const {
        int32_t v;
        std::memcpy(&v, data.data() + ofs, 4);
        return v;
    }

    // --- Field accessors ---
    uint16_t cardID()          const { return readU16(0x08); }
    uint8_t  cardType()        const { return data[0x11]; }
    uint8_t  restrictVersion() const { return data[0x0E]; }

    uint32_t id32()            const { return readU32(0x20); }
    uint16_t tid16()           const { return readU16(0x20); }
    uint16_t sid16()           const { return readU16(0x22); }
    int32_t  originGame()      const { return readI32(0x24); }
    uint32_t encryptionConst() const { return readU32(0x28); }
    uint32_t pid()             const { return readU32(0x2C); }

    // Pokemon data (national dex species)
    uint16_t species()         const { return readU16(0x288); }
    uint8_t  form()            const { return data[0x28A]; }
    uint8_t  gender()          const { return data[0x28B]; }
    uint8_t  level()           const { return data[0x28C]; }
    uint8_t  nature()          const { return data[0x28E]; }
    uint8_t  abilityType()     const { return data[0x28F]; }
    uint8_t  pidType()         const { return data[0x290]; }
    uint8_t  metLevel()        const { return data[0x291]; }

    // Moves
    uint16_t move1()           const { return readU16(0x278); }
    uint16_t move2()           const { return readU16(0x27A); }
    uint16_t move3()           const { return readU16(0x27C); }
    uint16_t move4()           const { return readU16(0x27E); }
    uint16_t relearnMove1()    const { return readU16(0x280); }
    uint16_t relearnMove2()    const { return readU16(0x282); }
    uint16_t relearnMove3()    const { return readU16(0x284); }
    uint16_t relearnMove4()    const { return readU16(0x286); }

    // Ball, item, locations
    uint16_t ball()            const { return readU16(0x274); }
    uint16_t heldItem()        const { return readU16(0x276); }
    uint16_t eggLocation()     const { return readU16(0x270); }
    uint16_t metLocation()     const { return readU16(0x272); }

    // IVs (6 individual bytes at 0x2B2-0x2B7)
    uint8_t ivHp()             const { return data[0x2B2]; }
    uint8_t ivAtk()            const { return data[0x2B3]; }
    uint8_t ivDef()            const { return data[0x2B4]; }
    uint8_t ivSpe()            const { return data[0x2B5]; }
    uint8_t ivSpA()            const { return data[0x2B6]; }
    uint8_t ivSpD()            const { return data[0x2B7]; }

    // OT Gender, EVs
    uint8_t otGender()         const { return data[0x2B8]; }
    uint8_t evHp()             const { return data[0x2B9]; }
    uint8_t evAtk()            const { return data[0x2BA]; }
    uint8_t evDef()            const { return data[0x2BB]; }
    uint8_t evSpe()            const { return data[0x2BC]; }
    uint8_t evSpA()            const { return data[0x2BD]; }
    uint8_t evSpD()            const { return data[0x2BE]; }

    // Contest stats (0x2BF-0x2C4)
    uint8_t contestCool()      const { return data[0x2BF]; }
    uint8_t contestBeauty()    const { return data[0x2C0]; }
    uint8_t contestCute()      const { return data[0x2C1]; }
    uint8_t contestSmart()     const { return data[0x2C2]; }
    uint8_t contestTough()     const { return data[0x2C3]; }
    uint8_t contestSheen()     const { return data[0x2C4]; }

    // Ribbon bytes: 0x292 to 0x2B1 (32 bytes, 0xFF = end)
    const uint8_t* ribbonData() const { return data.data() + 0x292; }
    static constexpr int RIBBON_SIZE = 0x20;

    // Nickname: 0x30 + langIdx * 0x20 (UTF-16LE, stride 0x20)
    std::u16string getNickname(int langIdx) const;
    bool hasNickname(int langIdx) const;

    // OT Name: 0x150 + langIdx * 0x20 (UTF-16LE, stride 0x20)
    std::u16string getOTName(int langIdx) const;

    bool isEgg() const { return data[0x28D] == 1; }
    bool isHOMEGift() const { return cardID() >= 9000; }

    // --- Key methods ---
    bool isPokemon() const { return cardType() == 1; }
    bool getHasOT(int langIdx) const;
    bool canInjectToBank(int langIdx) const { return getHasOT(langIdx); }

    // Convert to PB8 data
    Pokemon convertToPB8(const TrainerInfo& trainer) const;

    // Load from file
    bool loadFromFile(const std::string& path);
};

// WA8 wondercard parser (0x2C8 bytes) — Legends: Arceus
// Ported from PKHeX.Core/MysteryGifts/WA8.cs
struct WA8 {
    static constexpr int SIZE = 0x2C8;
    std::array<uint8_t, SIZE> data{};

    uint16_t readU16(int ofs) const { return data[ofs] | (data[ofs+1] << 8); }
    uint32_t readU32(int ofs) const { return data[ofs] | (data[ofs+1]<<8) | (data[ofs+2]<<16) | (data[ofs+3]<<24); }
    int32_t readI32(int ofs) const { uint32_t v = readU32(ofs); return static_cast<int32_t>(v); }

    // Card metadata
    uint16_t cardID() const { return readU16(0x08); }
    uint8_t cardType() const { return data[0x0F]; }

    // Trainer / ID
    uint32_t id32() const { return readU32(0x18); }
    uint16_t tid16() const { return readU16(0x18); }
    uint16_t sid16() const { return readU16(0x1A); }
    int32_t originGame() const { return readI32(0x1C); }
    uint32_t encryptionConst() const { return readU32(0x20); }
    uint32_t pid() const { return readU32(0x24); }

    // Locations
    uint16_t eggLocation() const { return readU16(0x220); }
    uint16_t metLocation() const { return readU16(0x222); }
    uint16_t ball() const { return readU16(0x224); }
    uint16_t heldItem() const { return readU16(0x226); }

    // Moves
    uint16_t move1() const { return readU16(0x228); }
    uint16_t move2() const { return readU16(0x22A); }
    uint16_t move3() const { return readU16(0x22C); }
    uint16_t move4() const { return readU16(0x22E); }
    uint16_t relearnMove1() const { return readU16(0x230); }
    uint16_t relearnMove2() const { return readU16(0x232); }
    uint16_t relearnMove3() const { return readU16(0x234); }
    uint16_t relearnMove4() const { return readU16(0x236); }

    // Pokemon attributes
    uint16_t species() const { return readU16(0x238); } // National dex
    uint8_t form() const { return data[0x23A]; }
    uint8_t gender() const { return data[0x23B]; }
    uint8_t level() const { return data[0x23C]; }
    uint8_t nature() const { return data[0x23E]; }
    uint8_t abilityType() const { return data[0x23F]; }
    uint8_t pidType() const { return data[0x240]; }
    uint8_t metLevel() const { return data[0x241]; }

    // Ribbons (byte-indexed array at 0x244, 0x20 bytes, 0xFF = end)
    static constexpr int RIBBON_OFS = 0x244;
    static constexpr int RIBBON_SIZE = 0x20;

    // IVs (each i8, 0xFC/0xFD/0xFE = flawless count)
    uint8_t ivHp() const { return data[0x264]; }
    uint8_t ivAtk() const { return data[0x265]; }
    uint8_t ivDef() const { return data[0x266]; }
    uint8_t ivSpe() const { return data[0x267]; }
    uint8_t ivSpA() const { return data[0x268]; }
    uint8_t ivSpD() const { return data[0x269]; }

    // OT Gender / EVs
    uint8_t otGender() const { return data[0x26A]; }
    uint8_t evHp() const { return data[0x26B]; }
    uint8_t evAtk() const { return data[0x26C]; }
    uint8_t evDef() const { return data[0x26D]; }
    uint8_t evSpe() const { return data[0x26E]; }
    uint8_t evSpA() const { return data[0x26F]; }
    uint8_t evSpD() const { return data[0x270]; }

    // OT Memory
    uint8_t otMemoryIntensity() const { return data[0x271]; }
    uint8_t otMemory() const { return data[0x272]; }
    uint8_t otMemoryFeeling() const { return data[0x273]; }
    uint16_t otMemoryVariable() const { return readU16(0x274); }

    // Ganbaru Values (unique to WA8/PA8)
    uint8_t gvHp() const { return data[0x276]; }
    uint8_t gvAtk() const { return data[0x277]; }
    uint8_t gvDef() const { return data[0x278]; }
    uint8_t gvSpe() const { return data[0x279]; }
    uint8_t gvSpA() const { return data[0x27A]; }
    uint8_t gvSpD() const { return data[0x27B]; }

    // Nickname: 0x28 + langIdx * 0x1C (UTF-16LE, stride 0x1C)
    std::u16string getNickname(int langIdx) const;
    bool hasNickname(int langIdx) const;

    // OT Name: 0x124 + langIdx * 0x1C (UTF-16LE, stride 0x1C)
    std::u16string getOTName(int langIdx) const;

    bool isEgg() const { return data[0x23D] == 1; }
    bool isHOMEGift() const { return cardID() >= 9000; }

    // --- Key methods ---
    bool isPokemon() const { return cardType() == 1; }
    bool getHasOT(int langIdx) const;
    bool canInjectToBank(int langIdx) const { return getHasOT(langIdx); }

    // Convert to PA8 data
    Pokemon convertToPA8(const TrainerInfo& trainer) const;

    // Load from file
    bool loadFromFile(const std::string& path);
};

// WB7 wondercard parser (0x310 bytes) — Let's Go Pikachu/Eevee
// Ported from PKHeX.Core/MysteryGifts/WB7.cs
struct WB7 {
    static constexpr int SIZE = 0x310;
    static constexpr int CARD_START = 0x208;

    std::array<uint8_t, SIZE> data{};

    // --- Helpers ---
    uint16_t readU16(int ofs) const { return data[ofs] | (data[ofs+1] << 8); }
    uint32_t readU32(int ofs) const { return data[ofs] | (data[ofs+1]<<8) | (data[ofs+2]<<16) | (data[ofs+3]<<24); }
    int32_t readI32(int ofs) const { uint32_t v = readU32(ofs); return static_cast<int32_t>(v); }

    // Card-relative accessor
    uint8_t card(int ofs) const { return data[CARD_START + ofs]; }
    uint16_t cardU16(int ofs) const { return readU16(CARD_START + ofs); }
    uint32_t cardU32(int ofs) const { return readU32(CARD_START + ofs); }
    int32_t cardI32(int ofs) const { return readI32(CARD_START + ofs); }

    // --- Field accessors ---
    uint16_t cardID() const { return cardU16(0x00); }
    uint8_t cardType() const { return card(0x51); }
    uint8_t restrictVersion() const { return data[0x00]; }

    // Card date: encoded as (year-2000)*10000 + month*100 + day
    uint32_t rawDate() const { return cardU32(0x4C); }

    // Trainer / ID (Card-relative)
    uint32_t id32() const { return cardU32(0x68); }
    uint16_t tid16() const { return cardU16(0x68); }
    uint16_t sid16() const { return cardU16(0x6A); }
    int32_t originGame() const { return cardI32(0x6C); }
    uint32_t encryptionConst() const { return cardU32(0x70); }
    uint8_t ball() const { return card(0x76); } // u8, NOT u16!
    uint16_t heldItem() const { return cardU16(0x78); }

    // Moves
    uint16_t move1() const { return cardU16(0x7A); }
    uint16_t move2() const { return cardU16(0x7C); }
    uint16_t move3() const { return cardU16(0x7E); }
    uint16_t move4() const { return cardU16(0x80); }

    // Pokemon attributes
    uint16_t species() const { return cardU16(0x82); } // National dex
    uint8_t form() const { return card(0x84); }
    uint8_t nature() const { return card(0xA0); }
    uint8_t gender() const { return card(0xA1); }
    uint8_t abilityType() const { return isHOMEGift() ? card(0xA2) : 3; }
    uint8_t pidType() const { return card(0xA3); } // ShinyType6: 0=Fixed,1=Rand,2=Always,3=Never
    uint16_t eggLocation() const { return cardU16(0xA4); }
    uint16_t metLocation() const { return cardU16(0xA6); }
    uint8_t metLevel() const { return card(0xA8); }

    // IVs
    uint8_t ivHp() const { return card(0xAF); }
    uint8_t ivAtk() const { return card(0xB0); }
    uint8_t ivDef() const { return card(0xB1); }
    uint8_t ivSpe() const { return card(0xB2); }
    uint8_t ivSpA() const { return card(0xB3); }
    uint8_t ivSpD() const { return card(0xB4); }

    uint8_t otGender() const { return card(0xB5); }
    uint8_t level() const { return card(0xD0); }
    bool isEgg() const { return card(0xD1) == 1; }
    uint32_t pid() const { return cardU32(0xD4); }

    // Relearn moves
    uint16_t relearnMove1() const { return cardU16(0xD8); }
    uint16_t relearnMove2() const { return cardU16(0xDA); }
    uint16_t relearnMove3() const { return cardU16(0xDC); }
    uint16_t relearnMove4() const { return cardU16(0xDE); }

    // Awakening Values
    uint8_t avHp() const { return card(0xE5); }
    uint8_t avAtk() const { return card(0xE6); }
    uint8_t avDef() const { return card(0xE7); }
    uint8_t avSpe() const { return card(0xE8); }
    uint8_t avSpA() const { return card(0xE9); }
    uint8_t avSpD() const { return card(0xEA); }

    // Ribbons (raw data offsets, NOT Card-relative)
    uint8_t rib0() const { return data[0x74]; }
    uint8_t rib1() const { return data[0x75]; }

    // Language data at raw offset 0x1D8 + langIdx
    uint8_t languageData(int langIdx) const { return data[0x1D8 + langIdx]; }

    bool isHOMEGift() const { return cardID() >= 9000; }
    bool isMainlandChinaGift() const { return cardID() > 1500 && cardID() <= 1503; }

    // Nickname: 0x04 + langIdx * 0x1A (UTF-16LE, up to 12 chars)
    std::u16string getNickname(int langIdx) const;
    bool hasNickname(int langIdx) const;

    // OT Name: 0xEE + langIdx * 0x1A (UTF-16LE, up to 12 chars)
    std::u16string getOTName(int langIdx) const;

    // --- Key methods ---
    bool isPokemon() const { return cardType() == 0; } // NOTE: 0 for Pokemon, 1 for Item!
    bool getHasOT(int langIdx) const;
    bool canInjectToBank(int langIdx) const { return getHasOT(langIdx); }

    // Convert to PB7 data
    Pokemon convertToPB7(const TrainerInfo& trainer) const;

    // Load from file
    bool loadFromFile(const std::string& path);
};

// Scan wondercard directory for .wc9/.wc8/.wa9/.wb8/.wa8/.wb7/.wb7full files
std::vector<WCInfo> scanWondercards(const std::string& basePath, GameType game);
