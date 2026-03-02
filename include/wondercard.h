#pragma once
#include "game_type.h"
#include "pokemon.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <array>

// Wondercard format types
enum class WCFormat { WC8, WB8, WA8, WC9, WA9, Unknown };

// File size constants
constexpr int WC8_SIZE = 0x2D0;
constexpr int WB8_SIZE = 0x2DC;
constexpr int WC9_SIZE = 0x2C8; // Also WA8/WA9 (disambiguated by content)

// Maximum wondercard data size
constexpr int WC_MAX_SIZE = WB8_SIZE;

// ShinyType from wondercard
enum class WCShinyType : uint8_t {
    Never = 0, Random = 1, AlwaysStar = 2, AlwaysSquare = 3, FixedValue = 4
};

// AbilityType from wondercard
enum class WCAbilityType : uint8_t {
    First = 0, Second = 1, Hidden = 2, RandomDual = 3, RandomAny = 4
};

struct Wondercard {
    std::array<uint8_t, WC_MAX_SIZE> data{};
    int dataSize = 0;
    WCFormat format = WCFormat::Unknown;
    std::string filename; // original filename

    // --- Helpers ---
    uint8_t readU8(int ofs) const { return data[ofs]; }
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

    // --- Load & Detect ---
    bool loadFromFile(const std::string& path);
    static WCFormat detectFormat(const uint8_t* buf, int size);

    // --- Validation ---
    bool isPokemonGift() const;

    // --- Field accessors (format-aware) ---
    uint32_t ec() const;
    uint32_t pid() const;
    uint16_t species() const;           // raw internal ID from card
    uint16_t speciesNational() const;   // converted to national dex
    uint8_t  form() const;
    uint8_t  gender() const;            // 0=M, 1=F, 2=Genderless, 3=Random
    uint8_t  level() const;
    uint8_t  nature() const;            // 0xFF = random
    WCAbilityType abilityType() const;
    WCShinyType shinyType() const;
    uint16_t ball() const;
    uint16_t heldItem() const;
    uint16_t move(int idx) const;       // idx 0-3
    uint16_t relearnMove(int idx) const;// idx 0-3
    uint8_t  iv(int idx) const;         // idx 0-5 (HP,Atk,Def,Spe,SpA,SpD)
    uint8_t  ev(int idx) const;         // idx 0-5
    uint16_t tid() const;
    uint16_t sid() const;
    uint8_t  otGender() const;          // 0=M, 1=F, >=2 = use default
    uint8_t  metLevel() const;
    uint16_t metLocation() const;
    uint16_t eggLocation() const;
    uint8_t  originGame() const;

    // Format-specific
    uint8_t  teraType() const;          // WC9 only
    bool     isAlpha() const;           // WA9 only
    uint8_t  dynamaxLevel() const;      // WC8 only
    bool     canGigantamax() const;     // WC8 only

    // --- Target game type ---
    GameType targetGameType() const;

    // --- Convert to Pokemon ---
    Pokemon toPokemon() const;

    // --- Display helpers ---
    std::string formatName() const;
};
