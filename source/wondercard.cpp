#include "wondercard.h"
#include "species_converter.h"
#include <cstdio>
#include <ctime>
#include <SDL2/SDL.h>

// --- Simple LCG RNG (avoids <random> with -fno-exceptions) ---
static uint32_t wcRandState = 0;

static void wcRandInit() {
    if (wcRandState == 0)
        wcRandState = static_cast<uint32_t>(SDL_GetTicks()) | 1;
}

static uint32_t wcRand32() {
    wcRandInit();
    wcRandState = wcRandState * 0x41C64E6D + 0x6073;
    return wcRandState;
}

// --- Format Detection ---

WCFormat Wondercard::detectFormat(const uint8_t* buf, int size) {
    switch (size) {
        case WC8_SIZE: return WCFormat::WC8;
        case WB8_SIZE: return WCFormat::WB8;
        case WC9_SIZE: // 0x2C8 — shared by WA8, WC9, WA9
            if (buf[0x0F] > 0) return WCFormat::WA8;
            if (buf[0x2C0] == 0) return WCFormat::WC9;
            return WCFormat::WA9;
        default: return WCFormat::Unknown;
    }
}

// --- File I/O ---

bool Wondercard::loadFromFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > WC_MAX_SIZE) {
        std::fclose(f);
        return false;
    }

    data.fill(0);
    dataSize = static_cast<int>(sz);
    std::fread(data.data(), 1, dataSize, f);
    std::fclose(f);

    format = detectFormat(data.data(), dataSize);
    return format != WCFormat::Unknown;
}

// --- Validation ---

bool Wondercard::isPokemonGift() const {
    int ofs = (format == WCFormat::WA8) ? 0x0F : 0x11;
    return data[ofs] == 1;
}

// --- Field Accessors ---

uint32_t Wondercard::ec() const {
    switch (format) {
        case WCFormat::WC8: case WCFormat::WB8: return readU32(0x28);
        default: return readU32(0x20); // WA8, WC9, WA9
    }
}

uint32_t Wondercard::pid() const {
    switch (format) {
        case WCFormat::WC8: case WCFormat::WB8: return readU32(0x2C);
        default: return readU32(0x24);
    }
}

uint16_t Wondercard::species() const {
    switch (format) {
        case WCFormat::WC8: return readU16(0x240);
        case WCFormat::WB8: return readU16(0x288);
        case WCFormat::WA8: return readU16(0x238);
        case WCFormat::WC9: return readU16(0x238);
        case WCFormat::WA9: return readU16(0x270);
        default: return 0;
    }
}

uint16_t Wondercard::speciesNational() const {
    uint16_t raw = species();
    // WC9/WA9 store Gen9 internal IDs
    if (format == WCFormat::WC9 || format == WCFormat::WA9)
        return SpeciesConverter::getNational9(raw);
    return raw; // WC8/WB8/WA8 store national dex directly
}

uint8_t Wondercard::form() const {
    switch (format) {
        case WCFormat::WC8: return data[0x242];
        case WCFormat::WB8: return data[0x28A];
        case WCFormat::WA8: return data[0x23A];
        case WCFormat::WC9: return data[0x23A];
        case WCFormat::WA9: return data[0x272];
        default: return 0;
    }
}

uint8_t Wondercard::gender() const {
    switch (format) {
        case WCFormat::WC8: return data[0x243];
        case WCFormat::WB8: return data[0x28B];
        case WCFormat::WA8: return data[0x23B];
        case WCFormat::WC9: return data[0x23B];
        case WCFormat::WA9: return data[0x273];
        default: return 3;
    }
}

uint8_t Wondercard::level() const {
    switch (format) {
        case WCFormat::WC8: return data[0x244];
        case WCFormat::WB8: return data[0x28C];
        case WCFormat::WA8: return data[0x23C];
        case WCFormat::WC9: return data[0x23C];
        case WCFormat::WA9: return data[0x274];
        default: return 1;
    }
}

uint8_t Wondercard::nature() const {
    switch (format) {
        case WCFormat::WC8: return data[0x246];
        case WCFormat::WB8: return data[0x28E];
        case WCFormat::WA8: return data[0x23E];
        case WCFormat::WC9: return data[0x23E];
        case WCFormat::WA9: return data[0x276];
        default: return 0xFF;
    }
}

WCAbilityType Wondercard::abilityType() const {
    uint8_t v;
    switch (format) {
        case WCFormat::WC8: v = data[0x247]; break;
        case WCFormat::WB8: v = data[0x28F]; break;
        case WCFormat::WA8: v = data[0x23F]; break;
        case WCFormat::WC9: v = data[0x23F]; break;
        case WCFormat::WA9: v = data[0x277]; break;
        default: v = 0; break;
    }
    return static_cast<WCAbilityType>(v > 4 ? 0 : v);
}

WCShinyType Wondercard::shinyType() const {
    uint8_t v;
    switch (format) {
        case WCFormat::WC8: v = data[0x248]; break;
        case WCFormat::WB8: v = data[0x290]; break;
        case WCFormat::WA8: v = data[0x240]; break;
        case WCFormat::WC9: v = data[0x240]; break;
        case WCFormat::WA9: v = data[0x278]; break;
        default: v = 0; break;
    }
    return static_cast<WCShinyType>(v > 4 ? 0 : v);
}

uint16_t Wondercard::ball() const {
    switch (format) {
        case WCFormat::WC8: return readU16(0x22C);
        case WCFormat::WB8: return readU16(0x274);
        case WCFormat::WA8: return readU16(0x224);
        case WCFormat::WC9: return readU16(0x224);
        case WCFormat::WA9: return readU16(0x25C);
        default: return 4;
    }
}

uint16_t Wondercard::heldItem() const {
    switch (format) {
        case WCFormat::WC8: return readU16(0x22E);
        case WCFormat::WB8: return readU16(0x276);
        case WCFormat::WA8: return readU16(0x226);
        case WCFormat::WC9: return readU16(0x226);
        case WCFormat::WA9: return readU16(0x25E);
        default: return 0;
    }
}

uint16_t Wondercard::move(int idx) const {
    if (idx < 0 || idx > 3) return 0;
    int base;
    switch (format) {
        case WCFormat::WC8: base = 0x230; break;
        case WCFormat::WB8: base = 0x278; break;
        case WCFormat::WA8: base = 0x228; break;
        case WCFormat::WC9: base = 0x228; break;
        case WCFormat::WA9: base = 0x260; break;
        default: return 0;
    }
    return readU16(base + idx * 2);
}

uint16_t Wondercard::relearnMove(int idx) const {
    if (idx < 0 || idx > 3) return 0;
    int base;
    switch (format) {
        case WCFormat::WC8: base = 0x238; break;
        case WCFormat::WB8: base = 0x280; break;
        case WCFormat::WA8: base = 0x230; break;
        case WCFormat::WC9: base = 0x230; break;
        case WCFormat::WA9: base = 0x268; break;
        default: return 0;
    }
    return readU16(base + idx * 2);
}

uint8_t Wondercard::iv(int idx) const {
    if (idx < 0 || idx > 5) return 0;
    int base;
    switch (format) {
        case WCFormat::WC8: base = 0x26C; break;
        case WCFormat::WB8: base = 0x2B2; break;
        case WCFormat::WA8: base = 0x264; break;
        case WCFormat::WC9: base = 0x268; break;
        case WCFormat::WA9: base = 0x29A; break;
        default: return 0;
    }
    return data[base + idx];
}

uint8_t Wondercard::ev(int idx) const {
    if (idx < 0 || idx > 5) return 0;
    int base;
    switch (format) {
        case WCFormat::WC8: base = 0x273; break;
        case WCFormat::WB8: base = 0x2B9; break;
        case WCFormat::WA8: base = 0x26B; break;
        case WCFormat::WC9: base = 0x26F; break;
        case WCFormat::WA9: base = 0x2A1; break;
        default: return 0;
    }
    return data[base + idx];
}

uint16_t Wondercard::tid() const {
    switch (format) {
        case WCFormat::WC8: case WCFormat::WB8: return readU16(0x20);
        default: return readU16(0x18);
    }
}

uint16_t Wondercard::sid() const {
    switch (format) {
        case WCFormat::WC8: case WCFormat::WB8: return readU16(0x22);
        default: return readU16(0x1A);
    }
}

uint8_t Wondercard::otGender() const {
    switch (format) {
        case WCFormat::WC8: return data[0x272];
        case WCFormat::WB8: return data[0x2B8];
        case WCFormat::WA8: return data[0x26A];
        case WCFormat::WC9: return data[0x26E];
        case WCFormat::WA9: return data[0x2A0];
        default: return 2;
    }
}

uint8_t Wondercard::metLevel() const {
    switch (format) {
        case WCFormat::WC8: return data[0x249];
        case WCFormat::WB8: return data[0x291];
        case WCFormat::WA8: return data[0x241];
        case WCFormat::WC9: return data[0x241];
        case WCFormat::WA9: return data[0x279];
        default: return 0;
    }
}

uint16_t Wondercard::metLocation() const {
    switch (format) {
        case WCFormat::WC8: return readU16(0x22A);
        case WCFormat::WB8: return readU16(0x272);
        case WCFormat::WA8: return readU16(0x222);
        case WCFormat::WC9: return readU16(0x222);
        case WCFormat::WA9: return readU16(0x25A);
        default: return 0;
    }
}

uint16_t Wondercard::eggLocation() const {
    switch (format) {
        case WCFormat::WC8: return readU16(0x228);
        case WCFormat::WB8: return readU16(0x270);
        case WCFormat::WA8: return readU16(0x220);
        case WCFormat::WC9: return readU16(0x220);
        case WCFormat::WA9: return readU16(0x258);
        default: return 0;
    }
}

uint16_t Wondercard::cardId() const {
    return readU16(0x08); // Same offset for all formats
}

uint16_t Wondercard::checksum() const {
    switch (format) {
        case WCFormat::WC8: return readU16(0x2CC);
        case WCFormat::WC9: return readU16(0x2C4);
        default: return 0;
    }
}

uint8_t Wondercard::originGame() const {
    switch (format) {
        case WCFormat::WC8: case WCFormat::WB8: return static_cast<uint8_t>(readU32(0x24));
        default: return static_cast<uint8_t>(readU32(0x1C));
    }
}

uint8_t Wondercard::teraType() const {
    if (format == WCFormat::WC9) return data[0x242];
    return 0;
}

bool Wondercard::isAlpha() const {
    if (format == WCFormat::WA9) return data[0x2AE] != 0;
    return false;
}

uint8_t Wondercard::dynamaxLevel() const {
    if (format == WCFormat::WC8) return data[0x24A];
    return 0;
}

bool Wondercard::canGigantamax() const {
    if (format == WCFormat::WC8) return data[0x24B] != 0;
    return false;
}

// --- Target Game Type ---

GameType Wondercard::targetGameType() const {
    switch (format) {
        case WCFormat::WC8: return GameType::Sw;
        case WCFormat::WB8: return GameType::BD;
        case WCFormat::WA8: return GameType::LA;
        case WCFormat::WC9: return GameType::S;
        case WCFormat::WA9: return GameType::ZA;
        default: return GameType::ZA;
    }
}

// --- Display Helpers ---

std::string Wondercard::formatName() const {
    switch (format) {
        case WCFormat::WC8: return "WC8";
        case WCFormat::WB8: return "WB8";
        case WCFormat::WA8: return "WA8";
        case WCFormat::WC9: return "WC9";
        case WCFormat::WA9: return "WA9";
        default: return "???";
    }
}

// --- PID Generation ---

static uint32_t generatePID(uint16_t tidVal, uint16_t sidVal,
                              WCShinyType sType, uint32_t cardPid) {
    switch (sType) {
        case WCShinyType::FixedValue:
            return cardPid;

        case WCShinyType::AlwaysStar: {
            uint32_t r = wcRand32();
            uint16_t low = cardPid & 0xFFFF;
            uint16_t high = (1 ^ low ^ tidVal ^ sidVal); // XOR = 1 → star shiny
            return (static_cast<uint32_t>(high) << 16) | low;
        }

        case WCShinyType::AlwaysSquare: {
            uint32_t r = wcRand32();
            uint16_t low = cardPid & 0xFFFF;
            uint16_t high = (0 ^ low ^ tidVal ^ sidVal); // XOR = 0 → square shiny
            return (static_cast<uint32_t>(high) << 16) | low;
        }

        case WCShinyType::Never: {
            uint32_t p = wcRand32();
            uint32_t xorVal = (p >> 16) ^ (p & 0xFFFF) ^ tidVal ^ sidVal;
            if (xorVal < 16)
                p ^= 0x10000000; // flip bit to break shiny
            return p;
        }

        case WCShinyType::Random:
        default:
            return wcRand32();
    }
}

// --- IV32 Generation ---

static uint32_t generateIV32(const Wondercard& wc) {
    uint8_t ivs[6]; // HP, Atk, Def, Spe, SpA, SpD
    int guaranteedPerfect = 0;

    // First pass: identify fixed values and sentinel count
    for (int i = 0; i < 6; i++) {
        uint8_t v = wc.iv(i);
        if (v <= 31) {
            ivs[i] = v;
        } else if (v >= 0xFC && v <= 0xFE) {
            int count = v - 0xFB; // 0xFC=1, 0xFD=2, 0xFE=3
            if (count > guaranteedPerfect)
                guaranteedPerfect = count;
            ivs[i] = 0xFF; // mark unresolved
        } else {
            ivs[i] = 0xFF; // any >31 that isn't sentinel → random
        }
    }

    // Assign guaranteed perfect IVs to random unresolved slots
    if (guaranteedPerfect > 0) {
        int unresolved[6];
        int unresolvedCount = 0;
        for (int i = 0; i < 6; i++) {
            if (ivs[i] == 0xFF)
                unresolved[unresolvedCount++] = i;
        }

        // Fisher-Yates shuffle
        for (int i = unresolvedCount - 1; i > 0; i--) {
            int j = wcRand32() % (i + 1);
            int tmp = unresolved[i];
            unresolved[i] = unresolved[j];
            unresolved[j] = tmp;
        }

        // Set perfect IVs
        for (int i = 0; i < guaranteedPerfect && i < unresolvedCount; i++)
            ivs[unresolved[i]] = 31;

        // Randomize remaining
        for (int i = guaranteedPerfect; i < unresolvedCount; i++)
            ivs[unresolved[i]] = wcRand32() % 32;
    } else {
        // No sentinel — just randomize any unresolved
        for (int i = 0; i < 6; i++) {
            if (ivs[i] == 0xFF)
                ivs[i] = wcRand32() % 32;
        }
    }

    // Pack: HP | (Atk<<5) | (Def<<10) | (Spe<<15) | (SpA<<20) | (SpD<<25)
    uint32_t packed = 0;
    packed |= (ivs[0] & 0x1F);
    packed |= (ivs[1] & 0x1F) << 5;
    packed |= (ivs[2] & 0x1F) << 10;
    packed |= (ivs[3] & 0x1F) << 15;
    packed |= (ivs[4] & 0x1F) << 20;
    packed |= (ivs[5] & 0x1F) << 25;
    return packed;
}

// --- Write UTF-16LE string to Pokemon data ---

static void writeOTName(Pokemon& pkm, int offset, const char* ascii) {
    for (int i = 0; ascii[i] != 0 && i < 12; i++)
        pkm.writeU16(offset + i * 2, static_cast<uint16_t>(ascii[i]));
}

// --- Get OT name offset in wondercard for English (language index 1) ---
static int wcOTOffset(WCFormat fmt) {
    // Base offsets + (English language index 1) * stride
    switch (fmt) {
        case WCFormat::WC8: return 0x12C + 1 * 0x1C; // 0x148
        case WCFormat::WB8: return 0x150 + 1 * 0x20; // 0x170
        case WCFormat::WA8: return 0x124 + 1 * 0x1C; // 0x140
        case WCFormat::WC9: return 0x124 + 1 * 0x1C; // 0x140
        case WCFormat::WA9: return 0x140 + 1 * 0x1C; // 0x15C
        default: return -1;
    }
}

// --- Check if wondercard has a fixed OT name ---
static bool wcHasOT(const Wondercard& wc) {
    int ofs = wcOTOffset(wc.format);
    if (ofs < 0 || ofs + 2 > wc.dataSize) return false;
    return wc.readU16(ofs) != 0;
}

// --- Copy OT name from wondercard to Pokemon (UTF-16LE, up to 0x1A bytes) ---
static void copyWCOTName(const Wondercard& wc, Pokemon& pkm, int pkmOTOffset) {
    int wcOfs = wcOTOffset(wc.format);
    if (wcOfs < 0) return;
    constexpr int OT_NAME_BYTES = 0x1A; // 13 UTF-16LE chars
    for (int i = 0; i < OT_NAME_BYTES; i += 2) {
        uint16_t ch = wc.readU16(wcOfs + i);
        pkm.writeU16(pkmOTOffset + i, ch);
        if (ch == 0) break;
    }
}

// --- Distribution Window Database (from PKHeX EncounterServerDate.cs) ---
// Maps CardID → distribution start date + optional offset days.
// Met date = start + addDays (GetGenerateDate in PKHeX).

struct WCDistWindow {
    uint16_t cardId;
    uint8_t year;    // years since 2000
    uint8_t month;
    uint8_t day;
    uint8_t addDays; // GenerateDaysAfterStart
};

static const WCDistWindow s_wc8Dates[] = {
    {9008, 20, 6, 2, 0},   // Hidden Ability Grookey
    {9009, 20, 6, 2, 0},   // Hidden Ability Scorbunny
    {9010, 20, 6, 2, 0},   // Hidden Ability Sobble
    {9011, 20, 6, 30, 0},  // Shiny Zeraora
    {9012, 20, 11, 10, 0}, // Gigantamax Melmetal
    {9013, 21, 6, 17, 0},  // Gigantamax Bulbasaur
    {9014, 21, 6, 17, 0},  // Gigantamax Squirtle
    {9029, 25, 2, 11, 0},  // Shiny Keldeo
};

static const WCDistWindow s_wa8Dates[] = {
    { 138, 22, 1, 27, 0},  // Poke Center Happiny
    { 301, 22, 2, 4, 0},   // Piplup
    { 801, 22, 2, 25, 0},  // Hisuian Growlithe
    {1201, 22, 5, 31, 0},  // Regigigas
    {1202, 22, 5, 31, 0},  // Piplup
    {1203, 22, 8, 18, 0},  // Hisuian Growlithe
    { 151, 22, 9, 3, 0},   // Otsukimi Clefairy
    {9018, 22, 5, 18, 0},  // Hidden Ability Rowlet
    {9019, 22, 5, 18, 0},  // Hidden Ability Cyndaquil
    {9020, 22, 5, 18, 0},  // Hidden Ability Oshawott
    {9027, 25, 1, 27, 0},  // Shiny Enamorus
};

static const WCDistWindow s_wb8Dates[] = {
    {9015, 22, 5, 18, 0},  // Hidden Ability Turtwig
    {9016, 22, 5, 18, 0},  // Hidden Ability Chimchar
    {9017, 22, 5, 18, 0},  // Hidden Ability Piplup
    {9026, 25, 1, 27, 0},  // Shiny Manaphy
};

static const WCDistWindow s_wc9Dates[] = {
    {   1, 22, 11, 17, 0}, // PokeCenter Birthday Flabebe
    {   6, 22, 12, 16, 0}, // Jump Festa Gyarados
    { 501, 23, 2, 16, 0},  // Jiseok's Garganacl
    {1513, 23, 2, 27, 0},  // Hisuian Zoroark DLC Gift
    { 502, 23, 3, 31, 0},  // TCG Flying Lechonk
    { 503, 23, 4, 13, 0},  // Gavin's Palafin
    {  25, 23, 4, 21, 0},  // PokeCenter Pikachu
    {1003, 23, 5, 29, 0},  // Bronzong
    {1002, 23, 5, 31, 0},  // Pichu
    {  28, 23, 6, 9, 0},   // Bronzong
    {1005, 23, 6, 16, 0},  // Gastrodon
    { 504, 23, 6, 30, 0},  // Paul's Shiny Arcanine
    {1522, 23, 7, 21, 0},  // Dark Tera Charizard
    {  24, 23, 7, 26, 0},  // Nontaro's Shiny Grimmsnarl
    { 505, 23, 8, 7, 0},   // WCS 2023 Tatsugiri
    {1521, 23, 8, 8, 0},   // My Very Own Mew
    { 506, 23, 8, 10, 0},  // Eduardo Gastrodon
    {1524, 23, 9, 6, 0},   // Glaseado Cetitan
    { 507, 23, 10, 13, 0}, // Trixie Mimikyu
    {  31, 23, 11, 1, 0},  // PokeCenter Birthday Charcadet/Pawmi
    {1006, 23, 11, 2, 0},  // Korea Bundle Fidough
    { 508, 23, 11, 17, 0}, // Alex's Dragapult
    {1526, 23, 11, 22, 0}, // Team Star Revavroom
    {1529, 23, 12, 7, 0},  // New Moon Darkrai
    {1530, 23, 12, 7, 0},  // Shiny Buddy Lucario
    {1527, 23, 12, 13, 0}, // Paldea Gimmighoul
    {  36, 23, 12, 14, 0}, // Roaring Moon / Iron Valiant
    {1007, 23, 12, 29, 0}, // Baxcalibur
    {  38, 24, 1, 14, 0},  // Scream Tail etc
    {  48, 24, 2, 22, 0},  // Project Snorlax Gift
    {1534, 24, 3, 12, 0},  // YOASOBI Pawmot
    {1535, 24, 3, 14, 0},  // Liko's Sprigatito
    { 509, 24, 4, 4, 0},   // Marco's Iron Hands
    {1008, 24, 5, 4, 0},   // Flutter Mane
    {  52, 24, 5, 11, 0},  // Sophia's Gyarados
    {1536, 24, 5, 18, 0},  // Dot's Quaxly
    {  49, 24, 5, 31, 0},  // Talonflame
    { 510, 24, 6, 7, 0},   // Nils's Porygon2
    {  50, 24, 7, 13, 0},  // Summer Festival Eevee
    {1537, 24, 7, 24, 0},  // Roy's Fuecoco
    { 511, 24, 8, 15, 0},  // WCS 2024 Steenee
    { 512, 24, 8, 16, 0},  // Tomoya's Sylveon
    {  62, 24, 10, 31, 0}, // PokeCenter Birthday Tandemaus
    { 513, 24, 11, 15, 0}, // Patrick's Pelipper
    {  54, 24, 11, 21, 0}, // Operation Mythical Keldeo
    {  55, 24, 11, 21, 0}, // Operation Mythical Zarude
    {  56, 24, 11, 21, 0}, // Operation Mythical Deoxys
    {1011, 24, 11, 21, 0}, // KOR Keldeo
    {1012, 24, 11, 21, 0}, // KOR Zarude
    {1013, 24, 11, 21, 0}, // KOR Deoxys
    {1010, 25, 1, 21, 0},  // KOR Lucario
    { 514, 25, 2, 5, 2},   // Pokemon Day 2025 Flying Eevee (+2 days)
    { 519, 25, 2, 20, 0},  // Marco's Jumpluff
    {  66, 25, 4, 18, 0},  // Wei Chyr's Rillaboom
    {1019, 25, 4, 24, 0},  // KOR Ditto Project
    {1020, 25, 6, 6, 0},   // PTC 2025 Porygon2
    { 523, 25, 6, 13, 0},  // NAIC 2025 Incineroar
    {  67, 25, 6, 20, 0},  // PJCS 2025 Flutter Mane
    {  68, 25, 6, 20, 0},  // PJCS 2025 Amoonguss
    {1542, 25, 8, 7, 0},   // Shiny Wo-Chien
    {1544, 25, 8, 21, 0},  // Shiny Chien-Pao
    {1546, 25, 9, 4, 0},   // Shiny Ting-Lu
    {1548, 25, 9, 18, 0},  // Shiny Chi-Yu
    { 524, 25, 8, 14, 0},  // WCS 2025 Toedscool
    { 525, 25, 8, 15, 0},  // WCS 2025 Farigiraf
    {1540, 25, 9, 25, 0},  // Shiny Miraidon/Koraidon
    {  70, 25, 10, 31, 0}, // PokeCenter Fidough Birthday
    { 526, 25, 11, 21, 0}, // LAIC 2026 Whimsicott
    {9021, 23, 5, 30, 0},  // Hidden Ability Sprigatito
    {9022, 23, 5, 30, 0},  // Hidden Ability Fuecoco
    {9023, 23, 5, 30, 0},  // Hidden Ability Quaxly
    {9024, 24, 10, 16, 0}, // Shiny Meloetta
    {9025, 24, 11, 1, 0},  // PokeCenter Birthday Tandemaus
    {9030, 25, 10, 31, 0}, // PokeCenter Fidough Birthday
};

static const WCDistWindow s_wa9Dates[] = {
    {1601, 25, 10, 14, 2}, // Ralts holding Gardevoirite (+2 days)
    { 102, 25, 10, 23, 2}, // Slowpoke PokeCenter Gift (+2 days)
    { 101, 25, 10, 31, 0}, // PokeCenter Audino Birthday
    {1607, 25, 12, 9, 0},  // Alpha Charizard
};

// Checksum-based tables (HOME gift revisions and alternate cards)
static const WCDistWindow s_wc8ChkDates[] = {
    // HOME 1.0.0 to 2.0.2 (rev 1)
    {0xFBBE, 20, 2, 12, 0}, // Bulbasaur
    {0x48F5, 20, 2, 12, 0}, // Charmander
    {0x47DB, 20, 2, 12, 0}, // Squirtle
    {0x671A, 20, 2, 12, 0}, // Pikachu
    {0x81A2, 20, 2, 12, 0}, // Original Color Magearna
    {0x4CC7, 20, 2, 12, 0}, // Eevee
    {0x1A0B, 20, 2, 12, 0}, // Rotom
    {0x1C26, 20, 2, 12, 0}, // Pichu
    // HOME 3.0.0 onward (rev 2)
    {0x7124, 23, 5, 30, 0}, // Bulbasaur
    {0xC26F, 23, 5, 30, 0}, // Charmander
    {0xCD41, 23, 5, 30, 0}, // Squirtle
    {0xED80, 23, 5, 30, 0}, // Pikachu
    {0x0B38, 23, 5, 30, 0}, // Original Color Magearna
    {0xC65D, 23, 5, 30, 0}, // Eevee
    {0x9091, 23, 5, 30, 0}, // Rotom
    {0x96BC, 23, 5, 30, 0}, // Pichu
};

static const WCDistWindow s_wc9ChkDates[] = {
    {0xE5EB, 22, 11, 17, 0}, // Fly Pikachu - rev 1
    {0x908B, 23, 2, 2, 0},   // Fly Pikachu - rev 2
};

// Look up in a table by key
static bool lookupInTable(const WCDistWindow* table, int count, uint16_t key,
                          uint8_t& outYear, uint8_t& outMonth, uint8_t& outDay) {
    for (int i = 0; i < count; i++) {
        if (table[i].cardId == key) {
            if (table[i].addDays == 0) {
                outYear  = table[i].year;
                outMonth = table[i].month;
                outDay   = table[i].day;
            } else {
                struct tm t = {};
                t.tm_year = table[i].year + 100;
                t.tm_mon  = table[i].month - 1;
                t.tm_mday = table[i].day + table[i].addDays;
                mktime(&t);
                outYear  = static_cast<uint8_t>(t.tm_year - 100);
                outMonth = static_cast<uint8_t>(t.tm_mon + 1);
                outDay   = static_cast<uint8_t>(t.tm_mday);
            }
            return true;
        }
    }
    return false;
}

// Look up distribution date for a card. Tries CardID first, then checksum fallback.
static bool lookupDistDate(WCFormat fmt, uint16_t cid, uint16_t chk,
                           uint8_t& outYear, uint8_t& outMonth, uint8_t& outDay) {
    // CardID-based lookup
    switch (fmt) {
        case WCFormat::WC8:
            if (lookupInTable(s_wc8Dates, sizeof(s_wc8Dates)/sizeof(s_wc8Dates[0]), cid, outYear, outMonth, outDay))
                return true;
            break;
        case WCFormat::WA8:
            if (lookupInTable(s_wa8Dates, sizeof(s_wa8Dates)/sizeof(s_wa8Dates[0]), cid, outYear, outMonth, outDay))
                return true;
            break;
        case WCFormat::WB8:
            if (lookupInTable(s_wb8Dates, sizeof(s_wb8Dates)/sizeof(s_wb8Dates[0]), cid, outYear, outMonth, outDay))
                return true;
            break;
        case WCFormat::WC9:
            if (lookupInTable(s_wc9Dates, sizeof(s_wc9Dates)/sizeof(s_wc9Dates[0]), cid, outYear, outMonth, outDay))
                return true;
            break;
        case WCFormat::WA9:
            if (lookupInTable(s_wa9Dates, sizeof(s_wa9Dates)/sizeof(s_wa9Dates[0]), cid, outYear, outMonth, outDay))
                return true;
            break;
        default:
            return false;
    }

    // Checksum-based fallback (WC8 and WC9 only)
    if (fmt == WCFormat::WC8)
        return lookupInTable(s_wc8ChkDates, sizeof(s_wc8ChkDates)/sizeof(s_wc8ChkDates[0]), chk, outYear, outMonth, outDay);
    if (fmt == WCFormat::WC9)
        return lookupInTable(s_wc9ChkDates, sizeof(s_wc9ChkDates)/sizeof(s_wc9ChkDates[0]), chk, outYear, outMonth, outDay);

    return false;
}

// --- Convert Wondercard to Pokemon ---

Pokemon Wondercard::toPokemon() const {
    Pokemon pkm;
    GameType gt = targetGameType();
    pkm.gameType_ = gt;
    pkm.data.fill(0);

    bool isLA = (gt == GameType::LA);
    bool isGen9 = (isSV(gt) || gt == GameType::ZA);
    bool isGen8 = (isSwSh(gt) || isBDSP(gt));

    // --- Encryption Constant ---
    uint32_t ecVal = ec();
    if (ecVal == 0) ecVal = wcRand32();
    pkm.writeU32(0x00, ecVal);

    // --- Species (internal ID) ---
    // WC8/WB8/WA8 store national dex (= internal for Gen8)
    // WC9/WA9 already store Gen9 internal IDs
    pkm.writeU16(0x08, species());

    // --- HeldItem ---
    pkm.writeU16(0x0A, heldItem());

    // --- TID/SID ---
    bool hasFixedOT = wcHasOT(*this);
    uint16_t tidVal = tid();
    uint16_t sidVal = sid();
    if (!hasFixedOT) {
        // Card uses player's OT → use generic values
        tidVal = 12345;
        sidVal = 54321;
    }
    pkm.writeU16(0x0C, tidVal);
    pkm.writeU16(0x0E, sidVal);

    // --- Ability ---
    WCAbilityType aType = abilityType();
    uint8_t abilNum;
    switch (aType) {
        case WCAbilityType::First:     abilNum = 1; break;
        case WCAbilityType::Second:    abilNum = 2; break;
        case WCAbilityType::Hidden:    abilNum = 4; break;
        case WCAbilityType::RandomDual:abilNum = (wcRand32() % 2 == 0) ? 1 : 2; break;
        case WCAbilityType::RandomAny: {
            uint32_t r = wcRand32() % 3;
            abilNum = (r == 2) ? 4 : (r + 1);
            break;
        }
        default: abilNum = 1; break;
    }
    // AbilityNumber at 0x16 bits 0-2
    pkm.data[0x16] = (pkm.data[0x16] & ~0x07) | (abilNum & 0x07);

    // --- WC8: Gigantamax flag at 0x16 bit 4 ---
    if (format == WCFormat::WC8 && canGigantamax())
        pkm.data[0x16] |= 0x10;

    // --- WA8: IsAlpha at 0x16 bit 5 ---
    if (format == WCFormat::WA8 && isAlpha())
        pkm.data[0x16] |= 0x20;

    // --- PID ---
    uint32_t pidVal = generatePID(tidVal, sidVal, shinyType(), pid());
    pkm.writeU32(0x1C, pidVal);

    // --- Nature ---
    uint8_t nat = nature();
    if (nat >= 25) nat = wcRand32() % 25;
    pkm.data[0x20] = nat;
    pkm.data[0x21] = nat; // StatNature = Nature for wondercards

    // --- FatefulEncounter + Gender ---
    uint8_t g = gender();
    if (g == 3) g = wcRand32() % 2; // Random → pick M or F
    if (isGen9) {
        // PA9/PK9: Gender at bits 1-2, FatefulEncounter at bit 0
        pkm.data[0x22] = 1 | (g << 1); // fateful = 1
    } else {
        // PK8/PA8/PB8: Gender at bits 2-3, FatefulEncounter at bit 0
        pkm.data[0x22] = 1 | (g << 2); // fateful = 1
    }

    // --- IsAlpha (PA9 at 0x23, full byte) ---
    if (format == WCFormat::WA9 && isAlpha())
        pkm.data[0x23] = 1;

    // --- Form ---
    pkm.data[0x24] = form();

    // --- EVs ---
    for (int i = 0; i < 6; i++)
        pkm.data[0x26 + i] = ev(i);

    // --- Height/Weight Scalars ---
    if (isLA) {
        pkm.data[0x50] = static_cast<uint8_t>(wcRand32() % 256);
        pkm.data[0x51] = static_cast<uint8_t>(wcRand32() % 256);
        pkm.data[0x52] = static_cast<uint8_t>(wcRand32() % 256); // Scale
    } else if (isGen9) {
        pkm.data[0x48] = static_cast<uint8_t>(wcRand32() % 256);
        pkm.data[0x49] = static_cast<uint8_t>(wcRand32() % 256);
        pkm.data[0x4A] = static_cast<uint8_t>(wcRand32() % 256); // Scale
    } else {
        // Gen8 (PK8/PB8)
        pkm.data[0x50] = static_cast<uint8_t>(wcRand32() % 256);
        pkm.data[0x51] = static_cast<uint8_t>(wcRand32() % 256);
    }

    // --- Moves ---
    int moveOfs = isLA ? 0x54 : 0x72;
    for (int i = 0; i < 4; i++)
        pkm.writeU16(moveOfs + i * 2, move(i));

    // --- Relearn Moves ---
    int relearnOfs = isLA ? 0x8A : 0x82;
    for (int i = 0; i < 4; i++)
        pkm.writeU16(relearnOfs + i * 2, relearnMove(i));

    // --- IV32 ---
    uint32_t iv32 = generateIV32(*this);
    int iv32Ofs = isLA ? 0x94 : 0x8C;
    pkm.writeU32(iv32Ofs, iv32);

    // --- DynamaxLevel (PK8 only) ---
    if (format == WCFormat::WC8)
        pkm.data[isLA ? 0x98 : 0x90] = dynamaxLevel();

    // --- Version (OriginGame) ---
    uint8_t ver = originGame();
    if (ver == 0) {
        // Use a default version for the target game
        switch (format) {
            case WCFormat::WC8: ver = 44; break; // Sword
            case WCFormat::WB8: ver = 48; break; // BD
            case WCFormat::WA8: ver = 47; break; // LA
            case WCFormat::WC9: ver = 50; break; // Scarlet
            case WCFormat::WA9: ver = 53; break; // ZA
            default: ver = 53; break;
        }
    }
    int verOfs = isLA ? 0xEE : isGen9 ? 0xCE : 0xDE;
    pkm.data[verOfs] = ver;

    // --- Language ---
    int langOfs = isLA ? 0xF2 : isGen9 ? 0xD5 : 0xE2;
    pkm.data[langOfs] = 2; // English

    // --- CurrentHandler = 0 (OT) ---
    int handlerOfs = isLA ? 0xD4 : 0xC4;
    pkm.data[handlerOfs] = 0;

    // --- OT Name ---
    int otOfs = isLA ? 0x110 : 0xF8;
    if (hasFixedOT) {
        copyWCOTName(*this, pkm, otOfs);
    } else {
        writeOTName(pkm, otOfs, "Event");
    }

    // --- OT Friendship ---
    int friendOfs = isLA ? 0x12A : 0x112;
    pkm.data[friendOfs] = 70; // default base friendship

    // --- Met Date (use distribution window date if available, else current date) ---
    int metDateOfs = isLA ? 0x134 : 0x11C;
    {
        uint8_t metY, metM, metD;
        if (lookupDistDate(format, cardId(), checksum(), metY, metM, metD)) {
            pkm.data[metDateOfs]     = metY;
            pkm.data[metDateOfs + 1] = metM;
            pkm.data[metDateOfs + 2] = metD;
        } else {
            time_t now = time(nullptr);
            struct tm* t = localtime(&now);
            pkm.data[metDateOfs]     = static_cast<uint8_t>(t->tm_year - 100);
            pkm.data[metDateOfs + 1] = static_cast<uint8_t>(t->tm_mon + 1);
            pkm.data[metDateOfs + 2] = static_cast<uint8_t>(t->tm_mday);
        }
    }

    // --- ObedienceLevel (Gen9 only) ---
    if (isGen9) {
        uint8_t lvl = level();
        if (lvl == 0) lvl = 1;
        pkm.data[0x11F] = lvl;
    }

    // --- Ball ---
    uint16_t ballVal = ball();
    if (ballVal == 0) ballVal = 4; // default Poke Ball
    int ballOfs = isLA ? 0x137 : 0x124;
    pkm.data[ballOfs] = static_cast<uint8_t>(ballVal);

    // --- MetLevel + OTGender ---
    uint8_t mLvl = metLevel();
    uint8_t lvl = level();
    if (lvl == 0) lvl = 1 + wcRand32() % 100;
    if (mLvl == 0) mLvl = lvl;
    uint8_t otG = otGender();
    if (otG >= 2) otG = 0; // default male OT
    int metLvlOfs = isLA ? 0x13D : 0x125;
    pkm.data[metLvlOfs] = (mLvl & 0x7F) | (otG << 7);

    // --- MetLocation ---
    if (isLA) {
        pkm.writeU16(0x13A, metLocation());
        pkm.writeU16(0x138, eggLocation());
    } else if (isGen9) {
        pkm.writeU16(0x122, metLocation());
        pkm.writeU16(0x120, eggLocation());
    } else {
        // PK8/PB8: MetLocation/EggLocation offsets
        pkm.writeU16(0x120, metLocation());
        pkm.writeU16(0x11E, eggLocation());
    }

    // --- TeraType (WC9 → PK9 only) ---
    if (format == WCFormat::WC9 && isSV(gt)) {
        pkm.data[0x94] = teraType(); // TeraTypeOriginal
        pkm.data[0x95] = teraType(); // TeraTypeOverride
    }

    // --- Level (party byte) ---
    int lvlOfs = isLA ? 0x168 : 0x148;
    pkm.data[lvlOfs] = lvl;

    // --- EXP (placeholder from level — approximate) ---
    // Use a simple cubic approximation: EXP ≈ level^3
    uint32_t exp = static_cast<uint32_t>(lvl) * lvl * lvl;
    pkm.writeU32(0x10, exp);

    // --- Finalize ---
    pkm.refreshChecksum();
    return pkm;
}
