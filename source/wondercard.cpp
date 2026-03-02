#include "wondercard.h"
#include "species_converter.h"
#include <cstdio>
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
        case WCFormat::WB8: return readU16(0x284);
        case WCFormat::WA8: return readU16(0x224);
        case WCFormat::WC9: return readU16(0x222);
        case WCFormat::WA9: return readU16(0x25C);
        default: return 4;
    }
}

uint16_t Wondercard::heldItem() const {
    switch (format) {
        case WCFormat::WC8: return readU16(0x22E);
        case WCFormat::WB8: return readU16(0x286);
        case WCFormat::WA8: return readU16(0x226);
        case WCFormat::WC9: return readU16(0x224);
        case WCFormat::WA9: return readU16(0x25E);
        default: return 0;
    }
}

uint16_t Wondercard::move(int idx) const {
    if (idx < 0 || idx > 3) return 0;
    int base;
    switch (format) {
        case WCFormat::WC8: base = 0x230; break;
        case WCFormat::WB8: base = 0x270; break;
        case WCFormat::WA8: base = 0x228; break;
        case WCFormat::WC9: base = 0x226; break;
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
        case WCFormat::WB8: base = 0x278; break;
        case WCFormat::WA8: base = 0x230; break;
        case WCFormat::WC9: base = 0x22E; break;
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
        case WCFormat::WA8: base = 0x268; break;
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
        case WCFormat::WA8: base = 0x26F; break;
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
        case WCFormat::WB8: return data[0x2B1];
        case WCFormat::WA8: return data[0x267];
        case WCFormat::WC9: return data[0x267];
        case WCFormat::WA9: return data[0x299];
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
        case WCFormat::WB8: return readU16(0x282);
        case WCFormat::WA8: return readU16(0x222);
        case WCFormat::WC9: return readU16(0x220);
        case WCFormat::WA9: return readU16(0x25A);
        default: return 0;
    }
}

uint16_t Wondercard::eggLocation() const {
    switch (format) {
        case WCFormat::WC8: return readU16(0x228);
        case WCFormat::WB8: return readU16(0x280);
        case WCFormat::WA8: return readU16(0x220);
        case WCFormat::WC9: return readU16(0x21E);
        case WCFormat::WA9: return readU16(0x258);
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
    uint16_t tidVal = tid();
    uint16_t sidVal = sid();
    if (otGender() >= 2) {
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
    writeOTName(pkm, otOfs, "Event");

    // --- OT Friendship ---
    int friendOfs = isLA ? 0x12A : 0x112;
    pkm.data[friendOfs] = 70; // default base friendship

    // --- Met Date (current date) ---
    int metDateOfs = isLA ? 0x134 : 0x11C;
    // Use a reasonable default date
    pkm.data[metDateOfs]     = 24; // year (2024)
    pkm.data[metDateOfs + 1] = 1;  // month
    pkm.data[metDateOfs + 2] = 1;  // day

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
