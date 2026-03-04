#include "wondercard.h"
#include "personal_sv.h"
#include "personal_swsh.h"
#include "personal_za.h"
#include "personal_bdsp.h"
#include "xoroshiro128plus.h"
#include "plus_za.h"
#include "encounter_server_date.h"
#include "species_converter.h"
#include "poke_crypto.h"
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
#include <algorithm>
#include <unordered_map>

// --- Gen9 Move PP table (from PKHeX MoveInfo9.PP, indexed by move ID) ---
static constexpr uint8_t MOVE_PP[920] = {
    0, 35, 25, 10, 15, 20, 20, 15, 15, 15, 35, 30,  5, 10, 20, 30, 35, 35, 20, 15,
   20, 20, 25, 20, 30,  5, 10, 15, 15, 15, 25, 20,  5, 35, 15, 20, 20, 10, 15, 30,
   35, 20, 20, 30, 25, 40, 20, 15, 20, 20, 20, 30, 25, 15, 30, 25,  5, 15, 10,  5,
   20, 20, 20,  5, 35, 20, 20, 20, 20, 20, 15, 25, 15, 10, 20, 25, 10, 35, 30, 15,
   10, 40, 10, 15, 30, 15, 20, 10, 15, 10,  5, 10, 10, 25, 10, 20, 40, 30, 30, 20,
   20, 15, 10, 40, 15,  5, 30, 10, 20, 10, 40, 40, 20, 30, 30, 20, 30, 10, 10, 20,
    5, 10, 30, 20, 20, 20,  5, 15, 15, 20, 10, 15, 35, 20, 15,  5, 10, 30, 15, 40,
   20, 10, 10,  5, 10, 30, 10, 15, 20, 15, 40, 20, 10,  5, 15, 10,  5, 10, 15, 30,
   30, 10, 10, 20, 10,  1,  1, 10, 25, 10,  5, 15, 25, 15, 10, 15, 30,  5, 40, 15,
   10, 25, 10, 30, 10, 20, 10, 10, 10, 10, 10, 20,  5, 40,  5,  5, 15,  5, 10,  5,
   10, 10, 10, 10, 20, 20, 40, 15,  5, 20, 20, 25,  5, 15, 10,  5, 20, 15, 20, 25,
   20,  5, 30,  5, 10, 20, 40,  5, 20, 40, 20, 15, 35, 10,  5,  5,  5, 15,  5, 20,
    5,  5, 15, 20, 10,  5,  5, 15, 10, 15, 15, 10, 10, 10, 20, 10, 10, 10, 10, 15,
   15, 15, 10, 20, 20, 10, 20, 20, 20, 20, 20, 10, 10, 10, 20, 20,  5, 15, 10, 10,
   15, 10, 20,  5,  5, 10, 10, 20,  5, 10, 20, 10, 20, 20, 20,  5,  5, 15, 20, 10,
   15, 20, 15,  5, 10, 15, 10,  5,  5, 10, 15, 10,  5, 20, 25,  5, 40, 15,  5, 40,
   15, 20, 20,  5, 15, 20, 20, 15, 15,  5, 10, 30, 20, 30, 15,  5, 40, 15,  5, 20,
    5, 15, 25, 25, 15, 20, 15, 20, 15, 20, 10, 20, 20,  5,  5,  5,  5, 40, 10, 10,
    5, 10, 10, 15, 10, 20, 15, 30, 10, 20,  5, 10, 10, 15, 10, 10,  5, 15,  5, 10,
   10, 30, 20, 20, 10, 10,  5,  5, 10,  5, 20, 10, 20, 10, 15, 10, 20, 20, 20, 15,
   15, 10, 15, 15, 15, 10, 10, 10, 20, 10, 30,  5, 10, 15, 10, 10,  5, 20, 30, 10,
   30, 15, 15, 15, 15, 30, 10, 20, 15, 10, 10, 20, 15,  5,  5, 15, 15,  5, 10,  5,
   20,  5, 15, 20,  5, 20, 20, 20, 20, 10, 20, 10, 15, 20, 15, 10, 10,  5, 10,  5,
    5, 10,  5,  5, 10,  5,  5,  5, 15, 10, 10, 10, 10, 10, 10, 15, 20, 15, 10, 15,
   10, 15, 10, 20, 10, 10, 10, 20, 20, 20, 20, 20, 15, 15, 15, 15, 15, 15, 20, 15,
   10, 15, 15, 15, 15, 10, 10, 10, 10, 10, 15, 15, 15, 15,  5,  5, 15,  5, 10, 10,
   10, 20, 20, 20, 10, 10, 30, 15, 15, 10, 15, 25, 10, 15, 10, 10, 10, 20, 10, 10,
   10, 10, 10, 15, 15,  5,  5, 10, 10, 10,  5,  5, 10,  5,  5, 15, 10,  5,  5,  5,
   10, 10, 10, 10, 20, 25, 10, 20, 30, 25, 20, 20, 15, 20, 15, 20, 20, 10, 10, 10,
   10, 10, 20, 10, 30, 15, 10, 10, 10, 20, 20,  5,  5,  5, 20, 10, 10, 20, 15, 20,
   20, 10, 20, 30, 10, 10, 40, 40, 30, 20, 40, 20, 20, 10, 10, 10, 10,  5, 10, 10,
    5,  5,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  5,
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 40, 15, 20, 30, 20, 15, 15, 20, 10, 15,
   15, 10,  5, 10, 10, 20, 15, 10, 15, 15, 15,  5, 15, 20, 20,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  5,  5, 10, 10, 10, 20, 10, 10, 10,  5,  5, 20, 10, 10, 10,  1,
    5, 15,  5,  1,  1,  1,  1,  1,  1, 10, 15, 15, 20, 20, 20, 20, 15, 15, 10, 10,
    5, 20,  5, 10,  5, 15, 10, 10,  5, 15, 20, 10, 10, 15, 10, 10, 10, 10, 10, 10,
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,  5, 10, 15, 10, 15,
    5,  5,  5, 10, 15, 40, 10, 10, 10, 15, 10, 10, 10, 10,  5,  5,  5, 10,  5, 20,
   10, 10,  5, 20, 20, 10, 10,  5,  5,  5, 40, 10, 20, 10, 10, 10, 10,  5,  5, 15,
    5, 10, 10, 10,  5,  5,  5, 15, 10, 10, 15,  5, 10, 10, 10,  5, 10, 10,  5, 10,
   10, 10, 10, 10, 15, 15, 10, 10, 10,  5, 15, 10, 10, 10, 10, 10, 10, 15, 15,  5,
   10, 15,  5,  1, 15, 10, 15, 10, 10, 10, 10, 10, 10, 10,  5, 15, 15, 10,  5,  5,
   10, 10, 10, 10, 20, 20, 20,  5, 10, 10,  5, 10,  5,  5, 10, 20, 10, 10, 10, 10,
   10,  5, 15, 10, 10, 10,  5,  5, 10,  5,  5, 10, 10, 15, 10, 10, 15, 10, 15,  5,
};

static inline uint8_t getMovePP(uint16_t moveId) {
    return moveId < 920 ? MOVE_PP[moveId] : 0;
}

// --- UTF-8 to UTF-16LE conversion (BMP only, covers all species names) ---
static std::u16string utf8to16(const std::string& s) {
    std::u16string out;
    size_t i = 0;
    while (i < s.size()) {
        uint32_t cp;
        uint8_t c = static_cast<uint8_t>(s[i]);
        if (c < 0x80) { cp = c; i += 1; }
        else if ((c >> 5) == 0x6)  { cp = (c & 0x1F) << 6;  cp |= (static_cast<uint8_t>(s[i+1]) & 0x3F); i += 2; }
        else if ((c >> 4) == 0xE)  { cp = (c & 0x0F) << 12; cp |= (static_cast<uint8_t>(s[i+1]) & 0x3F) << 6;
                                     cp |= (static_cast<uint8_t>(s[i+2]) & 0x3F); i += 3; }
        else { i += 4; continue; } // skip non-BMP
        out += static_cast<char16_t>(cp);
    }
    return out;
}

// --- Localized species name lookup (lazy-loaded per language) ---
static std::unordered_map<int, std::vector<std::string>> langSpeciesCache;

static const std::string& getLocalizedSpeciesName(uint16_t natdex, int langId) {
    static const std::string empty;
    auto it = langSpeciesCache.find(langId);
    if (it == langSpeciesCache.end()) {
        // Load from romfs: species_{langId}.txt (English uses existing species_en.txt)
        std::string path = (langId == 2)
            ? "romfs:/data/species_en.txt"
            : "romfs:/data/species_" + std::to_string(langId) + ".txt";
        std::ifstream f(path);
        std::vector<std::string> names;
        if (f.is_open()) {
            std::string line;
            while (std::getline(f, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                names.push_back(line);
            }
        }
        it = langSpeciesCache.emplace(langId, std::move(names)).first;
    }
    if (natdex < it->second.size())
        return it->second[natdex];
    return empty;
}

// --- WC9 string helpers ---

std::u16string WC9::getNickname(int langIdx) const {
    int ofs = 0x28 + langIdx * 0x1C;
    std::u16string result;
    for (int i = 0; i < 13; i++) {
        uint16_t ch = readU16(ofs + i * 2);
        if (ch == 0) break;
        result += static_cast<char16_t>(ch);
    }
    return result;
}

bool WC9::hasNickname(int langIdx) const {
    return readU16(0x28 + langIdx * 0x1C) != 0;
}

std::u16string WC9::getOTName(int langIdx) const {
    int ofs = 0x124 + langIdx * 0x1C;
    std::u16string result;
    for (int i = 0; i < 13; i++) {
        uint16_t ch = readU16(ofs + i * 2);
        if (ch == 0) break;
        result += static_cast<char16_t>(ch);
    }
    return result;
}

bool WC9::getHasOT(int langIdx) const {
    return readU16(0x124 + langIdx * 0x1C) != 0;
}

bool WC9::loadFromFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;
    auto sz = f.tellg();
    if (sz != SIZE) return false;
    f.seekg(0);
    f.read(reinterpret_cast<char*>(data.data()), SIZE);
    return f.good();
}

// --- WC8 string helpers ---

std::u16string WC8::getNickname(int langIdx) const {
    int ofs = 0x30 + langIdx * 0x1C;
    std::u16string result;
    for (int i = 0; i < 13; i++) {
        uint16_t ch = readU16(ofs + i * 2);
        if (ch == 0) break;
        result += static_cast<char16_t>(ch);
    }
    return result;
}

bool WC8::hasNickname(int langIdx) const {
    return readU16(0x30 + langIdx * 0x1C) != 0;
}

std::u16string WC8::getOTName(int langIdx) const {
    int ofs = 0x12C + langIdx * 0x1C;
    std::u16string result;
    for (int i = 0; i < 13; i++) {
        uint16_t ch = readU16(ofs + i * 2);
        if (ch == 0) break;
        result += static_cast<char16_t>(ch);
    }
    return result;
}

bool WC8::getHasOT(int langIdx) const {
    return readU16(0x12C + langIdx * 0x1C) != 0;
}

bool WC8::loadFromFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;
    auto sz = f.tellg();
    if (sz != SIZE) return false;
    f.seekg(0);
    f.read(reinterpret_cast<char*>(data.data()), SIZE);
    return f.good();
}

// --- WA9 string helpers ---

std::u16string WA9::getNickname(int langIdx) const {
    int ofs = 0x28 + langIdx * 0x1C; // same base as WC9
    std::u16string result;
    for (int i = 0; i < 13; i++) {
        uint16_t ch = readU16(ofs + i * 2);
        if (ch == 0) break;
        result += static_cast<char16_t>(ch);
    }
    return result;
}

bool WA9::hasNickname(int langIdx) const {
    return readU16(0x28 + langIdx * 0x1C) != 0;
}

std::u16string WA9::getOTName(int langIdx) const {
    int ofs = 0x140 + langIdx * 0x1C; // different from WC9's 0x124
    std::u16string result;
    for (int i = 0; i < 13; i++) {
        uint16_t ch = readU16(ofs + i * 2);
        if (ch == 0) break;
        result += static_cast<char16_t>(ch);
    }
    return result;
}

bool WA9::getHasOT(int langIdx) const {
    return readU16(0x140 + langIdx * 0x1C) != 0;
}

bool WA9::loadFromFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;
    auto sz = f.tellg();
    if (sz != SIZE) return false;
    f.seekg(0);
    f.read(reinterpret_cast<char*>(data.data()), SIZE);
    return f.good();
}

// --- WB8 string helpers (stride 0x20, not 0x1C) ---

std::u16string WB8::getNickname(int langIdx) const {
    int ofs = 0x30 + langIdx * 0x20;
    std::u16string result;
    for (int i = 0; i < 13; i++) {
        uint16_t ch = readU16(ofs + i * 2);
        if (ch == 0) break;
        result += static_cast<char16_t>(ch);
    }
    return result;
}

bool WB8::hasNickname(int langIdx) const {
    return readU16(0x30 + langIdx * 0x20) != 0;
}

std::u16string WB8::getOTName(int langIdx) const {
    int ofs = 0x150 + langIdx * 0x20;
    std::u16string result;
    for (int i = 0; i < 13; i++) {
        uint16_t ch = readU16(ofs + i * 2);
        if (ch == 0) break;
        result += static_cast<char16_t>(ch);
    }
    return result;
}

bool WB8::getHasOT(int langIdx) const {
    return readU16(0x150 + langIdx * 0x20) != 0;
}

bool WB8::loadFromFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;
    auto sz = f.tellg();
    if (sz != SIZE) return false;
    f.seekg(0);
    f.read(reinterpret_cast<char*>(data.data()), SIZE);
    return f.good();
}

// --- Language index conversion (PKHeX LanguageID mapping) ---
// PKHeX: JPN=1, ENG=2, FRE=3, ITA=4, GER=5, (6=unused), SPA=7, KOR=8, CHS=9, CHT=10
// WC9 stores 9 language slots (index 0-8): JPN, ENG, FRE, ITA, GER, SPA, KOR, CHS, CHT
// Mapping: lang < 6 -> lang - 1, lang >= 7 -> lang - 2
static int getLanguageIndex(int language) {
    if (language < 1 || language == 6 || language > 10)
        return 1; // fallback to English (index 1)
    return language < 6 ? language - 1 : language - 2;
}

// --- Distribution date lookup ---

struct MetDate { uint8_t year; uint8_t month; uint8_t day; };

static MetDate getDistributionDate(uint16_t cardId, uint16_t checksum) {
    // Try WC9Gifts by CardID first
    for (size_t i = 0; i < WC9Gifts_Count; i++) {
        if (WC9Gifts[i].cardID == cardId) {
            int year = WC9Gifts[i].startYear;
            int month = WC9Gifts[i].startMonth;
            int day = WC9Gifts[i].startDay + WC9Gifts[i].daysAfterStart;
            // Handle day overflow
            // Simple approach: for daysAfterStart <= 7 this is fine for all months
            return { static_cast<uint8_t>(year - 2000), static_cast<uint8_t>(month), static_cast<uint8_t>(day) };
        }
    }
    // Try WC9GiftsChk by checksum
    for (size_t i = 0; i < WC9GiftsChk_Count; i++) {
        if (WC9GiftsChk[i].cardID == checksum) {
            int year = WC9GiftsChk[i].startYear;
            int month = WC9GiftsChk[i].startMonth;
            int day = WC9GiftsChk[i].startDay + WC9GiftsChk[i].daysAfterStart;
            return { static_cast<uint8_t>(year - 2000), static_cast<uint8_t>(month), static_cast<uint8_t>(day) };
        }
    }
    // Fallback: SV release date 2022-11-18
    return { 22, 11, 18 };
}

static MetDate getDistributionDateWC8(uint16_t cardId, uint16_t checksum) {
    // Try WC8Gifts by CardID first
    for (size_t i = 0; i < WC8Gifts_Count; i++) {
        if (WC8Gifts[i].cardID == cardId) {
            int year = WC8Gifts[i].startYear;
            int month = WC8Gifts[i].startMonth;
            int day = WC8Gifts[i].startDay + WC8Gifts[i].daysAfterStart;
            return { static_cast<uint8_t>(year - 2000), static_cast<uint8_t>(month), static_cast<uint8_t>(day) };
        }
    }
    // Try WC8GiftsChk by checksum
    for (size_t i = 0; i < WC8GiftsChk_Count; i++) {
        if (WC8GiftsChk[i].cardID == checksum) {
            int year = WC8GiftsChk[i].startYear;
            int month = WC8GiftsChk[i].startMonth;
            int day = WC8GiftsChk[i].startDay + WC8GiftsChk[i].daysAfterStart;
            return { static_cast<uint8_t>(year - 2000), static_cast<uint8_t>(month), static_cast<uint8_t>(day) };
        }
    }
    // Fallback: SwSh release date 2019-11-15
    return { 19, 11, 15 };
}

static MetDate getDistributionDateWA9(uint16_t cardId) {
    // WA9 only uses CardID lookup (no checksum fallback)
    for (size_t i = 0; i < WA9Gifts_Count; i++) {
        if (WA9Gifts[i].cardID == cardId) {
            int year = WA9Gifts[i].startYear;
            int month = WA9Gifts[i].startMonth;
            int day = WA9Gifts[i].startDay + WA9Gifts[i].daysAfterStart;
            return { static_cast<uint8_t>(year - 2000), static_cast<uint8_t>(month), static_cast<uint8_t>(day) };
        }
    }
    // Fallback: ZA release date 2025-10-14
    return { 25, 10, 14 };
}

static MetDate getDistributionDateWB8(uint16_t cardId) {
    // WB8 only uses CardID lookup (all HOME gifts)
    for (size_t i = 0; i < WB8Gifts_Count; i++) {
        if (WB8Gifts[i].cardID == cardId) {
            int year = WB8Gifts[i].startYear;
            int month = WB8Gifts[i].startMonth;
            int day = WB8Gifts[i].startDay + WB8Gifts[i].daysAfterStart;
            return { static_cast<uint8_t>(year - 2000), static_cast<uint8_t>(month), static_cast<uint8_t>(day) };
        }
    }
    // Fallback: BDSP release date 2021-11-19
    return { 21, 11, 19 };
}

// --- RNG helpers ---
static uint32_t randU32() {
    return (static_cast<uint32_t>(std::rand()) << 16) | (static_cast<uint32_t>(std::rand()) & 0xFFFF);
}

// --- WC9 → PK9 conversion ---

Pokemon WC9::convertToPK9(const TrainerInfo& trainer) const {
    static bool seeded = false;
    if (!seeded) { std::srand(static_cast<unsigned>(std::time(nullptr))); seeded = true; }

    Pokemon pk;
    pk.gameType_ = GameType::S; // PK9 format
    pk.data.fill(0);

    uint16_t specInt = speciesInternal();
    uint16_t specNat = SpeciesConverter::getNational9(specInt);
    uint8_t formVal = form();
    uint8_t curLevel = level() > 0 ? level() : static_cast<uint8_t>(1 + std::rand() % 100);
    uint8_t mLevel = metLevel() > 0 ? metLevel() : curLevel;
    int langIdx = getLanguageIndex(trainer.language);
    bool hasOT = getHasOT(langIdx);

    // EncryptionConstant (0x00)
    uint32_t ec = encryptionConst() != 0 ? encryptionConst() : randU32();
    pk.writeU32(0x00, ec);

    // Species (0x08) - internal Gen9 ID
    pk.writeU16(0x08, specInt);

    // HeldItem (0x0A)
    pk.writeU16(0x0A, heldItem());

    // ID32 (0x0C) - handled after date determination
    // Start with WC9 ID32, may be overridden
    uint32_t pkId32 = id32();

    // Ability (0x14, 0x16)
    // AbilityType: 0/1/2=fixed slot, 3=random 0/1, 4=random 0/1/H
    // PKHeX default (no criteria): types 3 and 4 both pick random 0 or 1
    int abIdx = abilityType();
    if (abIdx >= 3) abIdx = std::rand() % 2;
    uint16_t abilityId = PersonalSV::getAbility(specNat, formVal, abIdx);
    pk.writeU16(0x14, abilityId);
    pk.data[0x16] = static_cast<uint8_t>(1 << abIdx); // AbilityNumber: 1=slot0, 2=slot1, 4=hidden

    // PID (0x1C) — set after ID32 is finalized
    // Will be set below

    // Nature (0x20, 0x21)
    uint8_t nat = nature();
    if (nat == 255) nat = static_cast<uint8_t>(std::rand() % 25);
    pk.data[0x20] = nat;
    pk.data[0x21] = nat; // StatNature = Nature

    // Gender + FatefulEncounter (0x22)
    uint8_t genderVal = gender();
    if (genderVal == 3) {
        // Derive from species ratio
        uint8_t ratio = PersonalSV::getGenderRatio(specNat, formVal);
        if (ratio == 0xFF) genderVal = 2;      // genderless
        else if (ratio == 0xFE) genderVal = 1;  // always female
        else if (ratio == 0x00) genderVal = 0;  // always male
        else genderVal = (std::rand() % 256) >= ratio ? 0 : 1;
    }
    pk.data[0x22] = static_cast<uint8_t>((genderVal << 1) | 1); // bit 0 = fateful

    // Form (0x24)
    pk.data[0x24] = form();

    // EVs (0x26-0x2B)
    pk.data[0x26] = evHp();
    pk.data[0x27] = evAtk();
    pk.data[0x28] = evDef();
    pk.data[0x29] = evSpe();
    pk.data[0x2A] = evSpA();
    pk.data[0x2B] = evSpD();

    // Ribbons (0x34+): each WC9 ribbon byte (non-0xFF) sets a bit in PK9
    for (int i = 0; i < RIBBON_SIZE; i++) {
        uint8_t ribbon = ribbonData()[i];
        if (ribbon == 0xFF) continue;
        int byteIdx = ribbon < 64 ? 0x34 + (ribbon >> 3) : 0x40 + ((ribbon - 64) >> 3);
        int bitIdx = ribbon & 7;
        if (byteIdx < static_cast<int>(pk.data.size()))
            pk.data[byteIdx] |= (1 << bitIdx);
        pk.data[0xD4] = ribbon; // AffixedRibbon
    }

    // Height/Weight scalars (0x48-0x49)
    pk.data[0x48] = heightScalar();
    pk.data[0x49] = weightScalar();

    // Scale (0x4A) — patch-aware: pre-1.2.0 cards use weighted random
    // IsBeforePatch120: cardID is 1/6/501/1501 AND date before 2023-03-01
    // (date not yet computed here, but cardID check is sufficient for these 4 cards
    //  since their distribution dates are all before 2023-03-01)
    uint16_t cid = cardID();
    bool patch120Card = (cid == 1 || cid == 6 || cid == 501 || cid == 1501);
    uint16_t scaleVal = scale();
    if (patch120Card) {
        // PokeSizeUtil.GetRandomScalar: triangular distribution (0x81 + 0x80)
        pk.data[0x4A] = static_cast<uint8_t>((std::rand() % 0x81) + (std::rand() % 0x80));
    } else if (scaleVal == 256) {
        pk.data[0x4A] = static_cast<uint8_t>(std::rand() % 256);
    } else {
        pk.data[0x4A] = static_cast<uint8_t>(scaleVal);
    }

    // Nickname (0x58) - UTF-16LE, up to 13 chars
    // pkLang is the PK9 language (set later, but we compute it here)
    uint8_t pkLang;
    bool isEggWC = isEgg();
    {
        int lo = 0x28 + langIdx * 0x1C + 0x1A;
        uint8_t wcLang = data[lo];
        pkLang = wcLang != 0 ? wcLang : trainer.language;
    }
    {
        std::u16string nick;
        bool isNicknamed = hasNickname(langIdx);
        if (isEggWC) {
            // Egg wondercards use localized "Egg" name (species index 0)
            const std::string& eggName = getLocalizedSpeciesName(0, pkLang);
            if (!eggName.empty())
                nick = utf8to16(eggName);
            else
                nick = utf8to16("Egg");
            isNicknamed = true;
        } else if (isNicknamed) {
            nick = getNickname(langIdx);
        } else {
            // Use localized species name matching the PK9 language
            const std::string& name = getLocalizedSpeciesName(specNat, pkLang);
            if (!name.empty())
                nick = utf8to16(name);
            else
                nick = utf8to16(SpeciesName::get(specNat)); // fallback to English
        }
        for (size_t i = 0; i < nick.size() && i < 13; i++) {
            uint16_t ch = static_cast<uint16_t>(nick[i]);
            std::memcpy(pk.data.data() + 0x58 + i * 2, &ch, 2);
        }

        // Moves (0x72-0x78)
        pk.writeU16(0x72, move1());
        pk.writeU16(0x74, move2());
        pk.writeU16(0x76, move3());
        pk.writeU16(0x78, move4());

        // Move PP (0x7A-0x7D)
        pk.data[0x7A] = getMovePP(move1());
        pk.data[0x7B] = getMovePP(move2());
        pk.data[0x7C] = getMovePP(move3());
        pk.data[0x7D] = getMovePP(move4());

        // Relearn Moves (0x82-0x88)
        pk.writeU16(0x82, relearnMove1());
        pk.writeU16(0x84, relearnMove2());
        pk.writeU16(0x86, relearnMove3());
        pk.writeU16(0x88, relearnMove4());

        // IVs (0x8C) - packed u32
        uint8_t ivs[6] = { ivHp(), ivAtk(), ivDef(), ivSpe(), ivSpA(), ivSpD() };
        // Check for perfect IV flags
        int perfectCount = 0;
        for (int i = 0; i < 6; i++) {
            if (ivs[i] >= 0xFC) {
                perfectCount = ivs[i] - 0xFB;
                break;
            }
        }
        if (perfectCount > 0) {
            // Set random IVs, then force N perfect ones
            for (int i = 0; i < 6; i++)
                ivs[i] = static_cast<uint8_t>(std::rand() % 32);
            int placed = 0;
            while (placed < perfectCount) {
                int idx = std::rand() % 6;
                if (ivs[idx] != 31) {
                    ivs[idx] = 31;
                    placed++;
                }
            }
        } else {
            for (int i = 0; i < 6; i++) {
                if (ivs[i] > 31)
                    ivs[i] = static_cast<uint8_t>(std::rand() % 32);
            }
        }
        uint32_t iv32 = (ivs[0]) | (ivs[1] << 5) | (ivs[2] << 10)
                       | (ivs[3] << 15) | (ivs[4] << 20) | (ivs[5] << 25);
        if (isEggWC)
            iv32 |= (1u << 30); // IsEgg bit
        if (isNicknamed)
            iv32 |= (1u << 31); // IsNicknamed bit
        pk.writeU32(0x8C, iv32);
    }

    // TeraType (0x94 = original, 0x95 = override)
    pk.data[0x94] = teraTypeOrig();
    pk.data[0x95] = 19; // TeraTypeUtil.OverrideNone — no override, use original

    // HT Name (0xA8) - if hasOT: player's OT; else: empty
    if (hasOT) {
        for (size_t i = 0; i < trainer.otName.size() && i < 13; i++) {
            uint16_t ch = static_cast<uint16_t>(trainer.otName[i]);
            std::memcpy(pk.data.data() + 0xA8 + i * 2, &ch, 2);
        }
        // HT Gender
        pk.data[0xC2] = trainer.gender;
        // HT Language
        pk.data[0xC3] = trainer.language;
    }

    // CurrentHandler (0xC4)
    pk.data[0xC4] = hasOT ? 1 : 0;

    // Version (0xCE)
    uint8_t originGame = this->originGame();
    if (originGame != 0) {
        pk.data[0xCE] = originGame;
    } else if (trainer.gameVersion == 50 || trainer.gameVersion == 51) {
        pk.data[0xCE] = trainer.gameVersion;
    } else {
        pk.data[0xCE] = 50; // fallback to Scarlet
    }

    // Language (0xD5) — already computed as pkLang above
    pk.data[0xD5] = pkLang;

    // OT Name (0xF8) - if hasOT: WC9 OT; else: player's OT
    {
        const std::u16string& otStr = hasOT ? getOTName(langIdx) : trainer.otName;
        for (size_t i = 0; i < otStr.size() && i < 13; i++) {
            uint16_t ch = static_cast<uint16_t>(otStr[i]);
            std::memcpy(pk.data.data() + 0xF8 + i * 2, &ch, 2);
        }
    }

    // EXP (0x10) - from growth rate table
    {
        uint8_t growth = PersonalSV::getGrowthRate(specNat, formVal);
        if (growth > 5) growth = 0;
        uint32_t exp = (curLevel >= 1 && curLevel <= 100) ? EXP_TABLE[growth][curLevel - 1] : 0;
        pk.writeU32(0x10, exp);
    }

    // OTFriendship (0x112) = always BaseFriendship
    // CurrentFriendship = egg ? HatchCycles : BaseFriendship
    // CurrentHandler=1 → HT_Friendship(0xC8), CurrentHandler=0 → OT_Friendship(0x112)
    {
        uint8_t baseFriend = PersonalSV::getBaseFriendship(specNat, formVal);
        uint8_t curFriend = isEggWC ? PersonalSV::getHatchCycles(specNat, formVal) : baseFriend;
        pk.data[0x112] = baseFriend;
        if (hasOT)
            pk.data[0xC8] = curFriend;
        else
            pk.data[0x112] = curFriend; // overrides baseFriend for !hasOT eggs
    }

    // OT Memory fields (0x113-0x118)
    pk.data[0x113] = otMemoryIntensity();
    pk.data[0x114] = otMemory();
    // 0x115 is unused alignment byte
    pk.writeU16(0x116, otMemoryVariable());
    pk.data[0x118] = otMemoryFeeling();

    // MetDate (0x11C-0x11E) - year-2000, month, day
    uint16_t checksum = readU16(0x2C4);
    MetDate md = getDistributionDate(cardID(), checksum);
    pk.data[0x11C] = md.year;
    pk.data[0x11D] = md.month;
    pk.data[0x11E] = md.day;

    // EggMetDate (0x119-0x11B) - for egg wondercards, use same date
    if (isEggWC) {
        pk.data[0x119] = md.year;
        pk.data[0x11A] = md.month;
        pk.data[0x11B] = md.day;
    }

    // Now finalize ID32 based on date
    // If OTGender >= 2: use player's ID
    if (otGender() >= 2) {
        pkId32 = trainer.id32;
    } else {
        // If date <= 2023-09-13 (patch 2.0.1): use ID32Old
        int fullYear = md.year + 2000;
        bool beforePatch200 = (fullYear < 2023) ||
                              (fullYear == 2023 && md.month < 9) ||
                              (fullYear == 2023 && md.month == 9 && md.day <= 13);
        if (beforePatch200) {
            pkId32 = id32() - 1000000u * static_cast<uint32_t>(cardID());
        }
    }
    pk.writeU32(0x0C, pkId32);

    // ObedienceLevel (0x11F)
    pk.data[0x11F] = curLevel;

    // EggLocation (0x120)
    pk.writeU16(0x120, eggLocation());

    // MetLocation (0x122)
    pk.writeU16(0x122, metLocation());

    // Ball (0x124 lower 7 bits)
    uint8_t ballVal = ball() != 0 ? static_cast<uint8_t>(ball()) : 4; // default Poké Ball

    // MetLevel (0x125 bits 0-6) + OTGender (bit 7)
    uint8_t otGenderBit = otGender() < 2 ? otGender() : trainer.gender;
    pk.data[0x124] = ballVal;
    pk.data[0x125] = static_cast<uint8_t>((mLevel & 0x7F) | ((otGenderBit & 1) << 7));

    // PID (0x1C) — now that we have ID32 finalized
    {
        uint16_t tid16 = static_cast<uint16_t>(pkId32 & 0xFFFF);
        uint16_t sid16 = static_cast<uint16_t>(pkId32 >> 16);
        uint8_t pType = pidType();
        uint32_t pidVal;
        switch (pType) {
            case 0: { // Never shiny (antishiny)
                pidVal = randU32();
                uint32_t xorVal = (pidVal >> 16) ^ (pidVal & 0xFFFF) ^ tid16 ^ sid16;
                if (xorVal < 16)
                    pidVal ^= 0x10000000;
                break;
            }
            case 1: // Random
                pidVal = randU32();
                break;
            case 2: { // Always Star shiny
                uint16_t low = static_cast<uint16_t>(pid() & 0xFFFF);
                uint16_t high = 1u ^ low ^ tid16 ^ sid16;
                pidVal = (static_cast<uint32_t>(high) << 16) | low;
                break;
            }
            case 3: { // Always Square shiny
                uint16_t low = static_cast<uint16_t>(pid() & 0xFFFF);
                uint16_t high = 0u ^ low ^ tid16 ^ sid16;
                pidVal = (static_cast<uint32_t>(high) << 16) | low;
                break;
            }
            case 4: // Fixed value
            default:
                pidVal = pid();
                break;
        }
        pk.writeU32(0x1C, pidVal);
    }

    // Level in party stats (0x148)
    pk.data[0x148] = curLevel;

    // Party stats calculation
    {
        auto bs = PersonalSV::getBaseStats(specNat, formVal);
        uint32_t storedIV32 = pk.readU32(0x8C);
        int iv_hp  = storedIV32 & 0x1F;
        int iv_atk = (storedIV32 >> 5) & 0x1F;
        int iv_def = (storedIV32 >> 10) & 0x1F;
        int iv_spe = (storedIV32 >> 15) & 0x1F;
        int iv_spa = (storedIV32 >> 20) & 0x1F;
        int iv_spd = (storedIV32 >> 25) & 0x1F;
        int ev_hp = pk.data[0x26], ev_atk = pk.data[0x27], ev_def = pk.data[0x28];
        int ev_spe = pk.data[0x29], ev_spa = pk.data[0x2A], ev_spd = pk.data[0x2B];

        // HP stat (Shedinja always 1)
        int hp;
        if (specNat == 292) {
            hp = 1;
        } else {
            hp = ((2 * bs.hp + iv_hp + ev_hp / 4) * curLevel / 100) + curLevel + 10;
        }
        pk.writeU16(0x14A, static_cast<uint16_t>(hp));
        pk.writeU16(0x8A, static_cast<uint16_t>(hp));

        // Other stats with nature modifier
        int boosted = nat / 5;
        int reduced = nat % 5;
        auto calcStat = [&](int base, int iv, int ev, int statIdx) -> uint16_t {
            int val = ((2 * base + iv + ev / 4) * curLevel / 100) + 5;
            if (boosted != reduced) {
                if (statIdx == boosted) val = val * 11 / 10;
                if (statIdx == reduced) val = val * 9 / 10;
            }
            return static_cast<uint16_t>(val);
        };
        pk.writeU16(0x14C, calcStat(bs.atk, iv_atk, ev_atk, 0));
        pk.writeU16(0x14E, calcStat(bs.def_, iv_def, ev_def, 1));
        pk.writeU16(0x150, calcStat(bs.spe, iv_spe, ev_spe, 2));
        pk.writeU16(0x152, calcStat(bs.spa, iv_spa, ev_spa, 3));
        pk.writeU16(0x154, calcStat(bs.spd, iv_spd, ev_spd, 4));
    }

    // Refresh checksum
    pk.refreshChecksum();

    return pk;
}

// --- WC8 → PK8 conversion ---

Pokemon WC8::convertToPK8(const TrainerInfo& trainer) const {
    static bool seeded = false;
    if (!seeded) { std::srand(static_cast<unsigned>(std::time(nullptr))); seeded = true; }

    Pokemon pk;
    pk.gameType_ = GameType::Sw; // PK8 format
    pk.data.fill(0);

    uint16_t specNat = species(); // WC8 species is already national dex
    uint8_t formVal = form();
    uint8_t curLevel = level() > 0 ? level() : static_cast<uint8_t>(1 + std::rand() % 100);
    uint8_t mLevel = metLevel() > 0 ? metLevel() : curLevel;
    int langIdx = getLanguageIndex(trainer.language);
    bool hasOT = getHasOT(langIdx);

    // EncryptionConstant (0x00) — set initial, may be overridden for old HOME
    uint32_t ec = encryptionConst() != 0 ? encryptionConst() : randU32();
    pk.writeU32(0x00, ec);

    // Species (0x08) — national dex ID directly
    pk.writeU16(0x08, specNat);

    // HeldItem (0x0A)
    pk.writeU16(0x0A, heldItem());

    // TID16/SID16 (0x0C-0x0F) — start with WC8 values
    uint16_t pkTid16 = tid16();
    uint16_t pkSid16 = sid16();

    // Ability (0x14, 0x16)
    int abIdx = abilityType();
    if (abIdx >= 3) abIdx = std::rand() % 2;
    uint16_t abilityId = PersonalSWSH::getAbility(specNat, formVal, abIdx);
    pk.writeU16(0x14, abilityId);
    // AbilityNumber: (1 << abIdx) | CanGigantamax flag at bit 4
    pk.data[0x16] = static_cast<uint8_t>((1 << abIdx) | (canGigantamax() ? 0x10 : 0));

    // Nature (0x20, 0x21)
    uint8_t nat = nature();
    if (nat == 255) nat = static_cast<uint8_t>(std::rand() % 25);
    pk.data[0x20] = nat;
    pk.data[0x21] = nat;

    // Gender + FatefulEncounter (0x22) — PK8: bit0=fateful, bits 2-3=gender
    uint8_t genderVal = gender();
    if (genderVal == 3) {
        uint8_t ratio = PersonalSWSH::getGenderRatio(specNat, formVal);
        if (ratio == 0xFF) genderVal = 2;
        else if (ratio == 0xFE) genderVal = 1;
        else if (ratio == 0x00) genderVal = 0;
        else genderVal = (std::rand() % 256) >= ratio ? 0 : 1;
    }
    pk.data[0x22] = static_cast<uint8_t>((genderVal << 2) | 1); // PK8: gender at bits 2-3

    // Form (0x24) — Meowstic special case
    if (specNat == 678)
        pk.data[0x24] = static_cast<uint8_t>(genderVal & 1);
    else
        pk.data[0x24] = formVal;

    // EVs (0x26-0x2B)
    pk.data[0x26] = evHp();
    pk.data[0x27] = evAtk();
    pk.data[0x28] = evDef();
    pk.data[0x29] = evSpe();
    pk.data[0x2A] = evSpA();
    pk.data[0x2B] = evSpD();

    // Ribbons (0x34+): same logic as WC9
    for (int i = 0; i < RIBBON_SIZE; i++) {
        uint8_t ribbon = ribbonData()[i];
        if (ribbon == 0xFF) continue;
        int byteIdx = ribbon < 64 ? 0x34 + (ribbon >> 3) : 0x40 + ((ribbon - 64) >> 3);
        int bitIdx = ribbon & 7;
        if (byteIdx < static_cast<int>(pk.data.size()))
            pk.data[byteIdx] |= (1 << bitIdx);
    }

    // Distribution date — needed for HOME old/new determination
    uint16_t chk = checksum();
    MetDate md = getDistributionDateWC8(cardID(), chk);
    pk.data[0x11C] = md.year;
    pk.data[0x11D] = md.month;
    pk.data[0x11E] = md.day;

    // Determine if old HOME gift (before 2023-05-30)
    // DayNumber for 2023-05-30 = 738669 (from PKHeX)
    bool homeGift = isHOMEGift();
    bool homeOld = false;
    if (homeGift) {
        int fullYear = md.year + 2000;
        // Simple date comparison against 2023-05-30
        homeOld = (fullYear < 2023) ||
                  (fullYear == 2023 && md.month < 5) ||
                  (fullYear == 2023 && md.month == 5 && md.day < 30);
    }

    // Override EC for old HOME gifts (keep WC8 value even if 0)
    if (homeOld)
        pk.writeU32(0x00, encryptionConst());

    // HeightScalar (0x50), WeightScalar (0x51) — PK8 offsets
    if (homeOld) {
        // Old HOME: scalars stay 0
        pk.data[0x50] = 0;
        pk.data[0x51] = 0;
    } else if (cardID() == 9029) {
        // Shiny Keldeo: fixed scalar 128
        pk.data[0x50] = 128;
        pk.data[0x51] = 128;
    } else {
        pk.data[0x50] = static_cast<uint8_t>((std::rand() % 0x81) + (std::rand() % 0x80));
        pk.data[0x51] = static_cast<uint8_t>((std::rand() % 0x81) + (std::rand() % 0x80));
    }

    // Nickname (0x58) — same offset as PK9
    uint8_t pkLang;
    bool isEggWC = isEgg();
    {
        int lo = 0x30 + langIdx * 0x1C + 0x1A;
        uint8_t wcLang = data[lo];
        pkLang = wcLang != 0 ? wcLang : trainer.language;
    }
    {
        std::u16string nick;
        bool isNicknamed = hasNickname(langIdx);
        if (isEggWC) {
            const std::string& eggName = getLocalizedSpeciesName(0, pkLang);
            if (!eggName.empty())
                nick = utf8to16(eggName);
            else
                nick = utf8to16("Egg");
            isNicknamed = true;
        } else if (isNicknamed) {
            nick = getNickname(langIdx);
        } else {
            const std::string& name = getLocalizedSpeciesName(specNat, pkLang);
            if (!name.empty())
                nick = utf8to16(name);
            else
                nick = utf8to16(SpeciesName::get(specNat));
        }
        for (size_t i = 0; i < nick.size() && i < 13; i++) {
            uint16_t ch = static_cast<uint16_t>(nick[i]);
            std::memcpy(pk.data.data() + 0x58 + i * 2, &ch, 2);
        }

        // Moves (0x72-0x78)
        pk.writeU16(0x72, move1());
        pk.writeU16(0x74, move2());
        pk.writeU16(0x76, move3());
        pk.writeU16(0x78, move4());

        // Move PP (0x7A-0x7D) — HealPP uses max PP
        pk.data[0x7A] = getMovePP(move1());
        pk.data[0x7B] = getMovePP(move2());
        pk.data[0x7C] = getMovePP(move3());
        pk.data[0x7D] = getMovePP(move4());

        // Relearn Moves (0x82-0x88)
        pk.writeU16(0x82, relearnMove1());
        pk.writeU16(0x84, relearnMove2());
        pk.writeU16(0x86, relearnMove3());
        pk.writeU16(0x88, relearnMove4());

        // IVs (0x8C) — packed u32, same logic as WC9
        uint8_t ivs[6] = { ivHp(), ivAtk(), ivDef(), ivSpe(), ivSpA(), ivSpD() };
        int perfectCount = 0;
        for (int i = 0; i < 6; i++) {
            if (ivs[i] >= 0xFC) {
                perfectCount = ivs[i] - 0xFB;
                break;
            }
        }
        if (perfectCount > 0) {
            for (int i = 0; i < 6; i++)
                ivs[i] = static_cast<uint8_t>(std::rand() % 32);
            int placed = 0;
            while (placed < perfectCount) {
                int idx = std::rand() % 6;
                if (ivs[idx] != 31) {
                    ivs[idx] = 31;
                    placed++;
                }
            }
        } else {
            for (int i = 0; i < 6; i++) {
                if (ivs[i] > 31)
                    ivs[i] = static_cast<uint8_t>(std::rand() % 32);
            }
        }
        uint32_t iv32 = (ivs[0]) | (ivs[1] << 5) | (ivs[2] << 10)
                       | (ivs[3] << 15) | (ivs[4] << 20) | (ivs[5] << 25);
        if (isEggWC)
            iv32 |= (1u << 30);
        if (isNicknamed)
            iv32 |= (1u << 31);
        pk.writeU32(0x8C, iv32);
    }

    // DynamaxLevel (0x90) — PK8 only
    pk.data[0x90] = dynamaxLevel();

    // HT Name (0xA8) — same offset as PK9
    if (hasOT) {
        for (size_t i = 0; i < trainer.otName.size() && i < 13; i++) {
            uint16_t ch = static_cast<uint16_t>(trainer.otName[i]);
            std::memcpy(pk.data.data() + 0xA8 + i * 2, &ch, 2);
        }
        pk.data[0xC2] = trainer.gender;
        pk.data[0xC3] = trainer.language;
    }

    // CurrentHandler (0xC4)
    pk.data[0xC4] = hasOT ? 1 : 0;

    // Version (0xDE) — PK8 offset, NOT 0xCE
    // RestrictVersion: 0=any(non-entity), 1=SW, 2=SH, 3=both
    {
        uint8_t ver;
        int32_t origGame = originGame();
        if (origGame != 0) {
            ver = static_cast<uint8_t>(origGame);
        } else if (trainer.gameVersion == 44 || trainer.gameVersion == 45) {
            ver = trainer.gameVersion;
        } else {
            ver = 44; // fallback to Sword
        }
        // Validate against RestrictVersion (PKHeX CanBeReceivedByVersion)
        uint8_t rv = restrictVersion();
        if (rv == 1 && ver != 44) ver = 44;       // SW only
        else if (rv == 2 && ver != 45) ver = 45;   // SH only
        pk.data[0xDE] = ver;
    }

    // Language (0xE2) — PK8 offset, NOT 0xD5
    pk.data[0xE2] = pkLang;

    // AffixedRibbon (0xE8) — PK8 offset, NOT 0xD4 — set to -1 (none)
    pk.data[0xE8] = 0xFF;

    // OT Name (0xF8) — same offset as PK9
    {
        const std::u16string& otStr = hasOT ? getOTName(langIdx) : trainer.otName;
        for (size_t i = 0; i < otStr.size() && i < 13; i++) {
            uint16_t ch = static_cast<uint16_t>(otStr[i]);
            std::memcpy(pk.data.data() + 0xF8 + i * 2, &ch, 2);
        }
    }

    // EXP (0x10)
    {
        uint8_t growth = PersonalSWSH::getGrowthRate(specNat, formVal);
        if (growth > 5) growth = 0;
        uint32_t exp = (curLevel >= 1 && curLevel <= 100) ? EXP_TABLE[growth][curLevel - 1] : 0;
        pk.writeU32(0x10, exp);
    }

    // ID32 handling — WC8 is simpler than WC9 (no date-based subtraction)
    if (otGender() >= 2) {
        uint32_t value = trainer.id32;
        if (homeGift)
            value %= 1000000u;
        pk.writeU32(0x0C, value);
        pkTid16 = static_cast<uint16_t>(value & 0xFFFF);
        pkSid16 = static_cast<uint16_t>(value >> 16);
    } else {
        pk.writeU16(0x0C, pkTid16);
        pk.writeU16(0x0E, pkSid16);
    }

    // OTFriendship (0x112) = always BaseFriendship
    // CurrentFriendship = egg ? HatchCycles : BaseFriendship
    // CurrentHandler=1 → HT_Friendship(0xC8), CurrentHandler=0 → OT_Friendship(0x112)
    {
        uint8_t baseFriend = PersonalSWSH::getBaseFriendship(specNat, formVal);
        uint8_t curFriend = isEggWC ? PersonalSWSH::getHatchCycles(specNat, formVal) : baseFriend;
        pk.data[0x112] = baseFriend;
        if (hasOT)
            pk.data[0xC8] = curFriend;
        else
            pk.data[0x112] = curFriend; // overrides baseFriend for !hasOT eggs
    }

    // OT Memory fields (0x113-0x118)
    pk.data[0x113] = otMemoryIntensity();
    pk.data[0x114] = otMemory();
    pk.writeU16(0x116, otMemoryVariable());
    pk.data[0x118] = otMemoryFeeling();

    // EggMetDate (0x119-0x11B) — for egg wondercards
    if (isEggWC) {
        pk.data[0x119] = md.year;
        pk.data[0x11A] = md.month;
        pk.data[0x11B] = md.day;
    }

    // ObedienceLevel — PK8 doesn't have this field (PK9 only at 0x11F)

    // EggLocation (0x120)
    pk.writeU16(0x120, eggLocation());

    // MetLocation (0x122)
    pk.writeU16(0x122, metLocation());

    // Ball (0x124)
    uint8_t ballVal = ball() != 0 ? static_cast<uint8_t>(ball()) : 4;

    // MetLevel (0x125 bits 0-6) + OTGender (bit 7)
    uint8_t otGenderBit = otGender() < 2 ? otGender() : trainer.gender;
    pk.data[0x124] = ballVal;
    pk.data[0x125] = static_cast<uint8_t>((mLevel & 0x7F) | ((otGenderBit & 1) << 7));

    // PID (0x1C) — ShinyType8: 0=Never, 1=Random, 2=Star, 3=Square, 4=Fixed
    {
        uint8_t pType = pidType();
        uint32_t pidVal;
        switch (pType) {
            case 0: { // Never shiny (antishiny)
                pidVal = randU32();
                uint32_t xorVal = (pidVal >> 16) ^ (pidVal & 0xFFFF) ^ pkTid16 ^ pkSid16;
                if (xorVal < 16)
                    pidVal ^= 0x10000000;
                break;
            }
            case 1: // Random
                pidVal = randU32();
                break;
            case 2: { // Always Star shiny
                uint16_t low = static_cast<uint16_t>(pid() & 0xFFFF);
                uint16_t high = 1u ^ low ^ pkTid16 ^ pkSid16;
                pidVal = (static_cast<uint32_t>(high) << 16) | low;
                break;
            }
            case 3: { // Always Square shiny
                uint16_t low = static_cast<uint16_t>(pid() & 0xFFFF);
                uint16_t high = 0u ^ low ^ pkTid16 ^ pkSid16;
                pidVal = (static_cast<uint32_t>(high) << 16) | low;
                break;
            }
            case 4: { // Fixed value — PKHeX GetFixedPID logic
                pidVal = pid();
                if (pidVal != 0 && id32() != 0) {
                    break; // both non-zero: use PID as-is
                }
                // Check if PID would be shiny against the PK8's TID/SID
                uint32_t xorVal = (pidVal >> 16) ^ (pidVal & 0xFFFF) ^ pkTid16 ^ pkSid16;
                if (xorVal >= 16) {
                    break; // not shiny: use PID as-is
                }
                // PID IS shiny — anti-shiny for HOME gifts that aren't shiny-possible
                // IsHOMEShinyPossible = ID32==0 && PID!=0
                if (homeGift && !(id32() == 0 && pidVal != 0))
                    pidVal ^= 0x10000000;
                break;
            }
            default:
                pidVal = pid();
                break;
        }
        pk.writeU32(0x1C, pidVal);
    }

    // Level in party stats (0x148)
    pk.data[0x148] = curLevel;

    // Party stats calculation
    {
        auto bs = PersonalSWSH::getBaseStats(specNat, formVal);
        uint32_t storedIV32 = pk.readU32(0x8C);
        int iv_hp  = storedIV32 & 0x1F;
        int iv_atk = (storedIV32 >> 5) & 0x1F;
        int iv_def = (storedIV32 >> 10) & 0x1F;
        int iv_spe = (storedIV32 >> 15) & 0x1F;
        int iv_spa = (storedIV32 >> 20) & 0x1F;
        int iv_spd = (storedIV32 >> 25) & 0x1F;
        int ev_hp = pk.data[0x26], ev_atk = pk.data[0x27], ev_def = pk.data[0x28];
        int ev_spe = pk.data[0x29], ev_spa = pk.data[0x2A], ev_spd = pk.data[0x2B];

        int hp;
        if (specNat == 292) {
            hp = 1;
        } else {
            hp = ((2 * bs.hp + iv_hp + ev_hp / 4) * curLevel / 100) + curLevel + 10;
        }
        pk.writeU16(0x14A, static_cast<uint16_t>(hp));
        pk.writeU16(0x8A, static_cast<uint16_t>(hp));

        int boosted = nat / 5;
        int reduced = nat % 5;
        auto calcStat = [&](int base, int iv, int ev, int statIdx) -> uint16_t {
            int val = ((2 * base + iv + ev / 4) * curLevel / 100) + 5;
            if (boosted != reduced) {
                if (statIdx == boosted) val = val * 11 / 10;
                if (statIdx == reduced) val = val * 9 / 10;
            }
            return static_cast<uint16_t>(val);
        };
        pk.writeU16(0x14C, calcStat(bs.atk, iv_atk, ev_atk, 0));
        pk.writeU16(0x14E, calcStat(bs.def_, iv_def, ev_def, 1));
        pk.writeU16(0x150, calcStat(bs.spe, iv_spe, ev_spe, 2));
        pk.writeU16(0x152, calcStat(bs.spa, iv_spa, ev_spa, 3));
        pk.writeU16(0x154, calcStat(bs.spd, iv_spd, ev_spd, 4));
    }

    pk.refreshChecksum();

    return pk;
}

// --- WA9 → PA9 conversion (Legends: Z-A) ---
// Ported from PKHeX.Core/MysteryGifts/WA9.cs ConvertToPKM()
// Uses Lumiose RNG (Xoroshiro128Plus seed-correlated generation)

// 64-bit random helper
static uint64_t randU64() {
    uint64_t v = 0;
    for (int i = 0; i < 4; i++)
        v = (v << 16) | (static_cast<uint64_t>(std::rand()) & 0xFFFF);
    return v;
}

// Shiny XOR helpers
static uint32_t getShinyXor(uint32_t pid, uint32_t id32) {
    uint32_t comp = pid ^ id32;
    return (comp ^ (comp >> 16)) & 0xFFFF;
}

static bool isShiny(uint32_t pid, uint32_t id32) {
    return getShinyXor(pid, id32) < 16;
}

Pokemon WA9::convertToPA9(const TrainerInfo& trainer) const {
    static bool seeded = false;
    if (!seeded) { std::srand(static_cast<unsigned>(std::time(nullptr))); seeded = true; }

    Pokemon pk;
    pk.gameType_ = GameType::ZA; // PA9 format
    pk.data.fill(0);

    uint16_t specInt = speciesInternal();
    uint16_t specNat = SpeciesConverter::getNational9(specInt);
    uint8_t formVal = form();
    uint8_t curLevel = level() > 0 ? level() : static_cast<uint8_t>(1 + std::rand() % 100);
    uint8_t mLevel = metLevel() > 0 ? metLevel() : curLevel;
    int langIdx = getLanguageIndex(trainer.language);
    bool hasOT = getHasOT(langIdx);
    bool isEggWC = isEgg();

    // --- ID32 (always old format for WA9: ID32 - 1000000 * CardID) ---
    uint32_t pkId32;
    if (otGender() >= 2) {
        pkId32 = trainer.id32;
    } else {
        pkId32 = id32() - 1000000u * static_cast<uint32_t>(cardID());
    }
    pk.writeU32(0x0C, pkId32);

    // Species (0x08) - internal Gen9 ID
    pk.writeU16(0x08, specInt);

    // HeldItem (0x0A)
    pk.writeU16(0x0A, heldItem());

    // --- Lumiose RNG PINGA generation ---
    // Ref: PKHeX LumioseRNG.cs, WA9.SetPINGA()
    // WA9 uses SkipTrainer correlation (no fake TID), RollCount = 1
    {
        // Flawless IV count from IV_HP
        int flawlessIVs = 0;
        uint8_t hpIV = ivHp();
        if (hpIV >= 0xFC && hpIV <= 0xFE)
            flawlessIVs = hpIV - 0xFB; // 0xFC=1, 0xFD=2, 0xFE=3

        // WA9 IVs (if no flawless flags, these are the fixed values)
        uint8_t wcIVs[6] = { ivHp(), ivAtk(), ivDef(), ivSpe(), ivSpA(), ivSpD() };
        bool hasFixedIVs = (flawlessIVs == 0);

        // Map PIDType to Shiny enum for RNG
        // ShinyType8: 0=Never, 1=Random, 2=AlwaysStar, 3=AlwaysSquare, 4=FixedValue
        uint8_t pType = pidType();
        // In PKHeX GetAdaptedPID: AlwaysStar/AlwaysSquare fall into the else (Never) branch
        // because they are not Shiny.Random or Shiny.Always. PID is overridden post-RNG anyway.
        // FixedValue(4): uses ShinyRandom in RNG, PID overridden afterwards.
        enum { ShinyNever = 0, ShinyRandom = 1 };
        int shinyMode = ShinyRandom;
        if (pType == 0 || pType == 2 || pType == 3) shinyMode = ShinyNever;

        // AbilityType → AbilityPermission
        // 0=OnlyFirst, 1=OnlySecond, 2=OnlyHidden, 3=Any12, 4+=Any12H
        uint8_t abType = abilityType();

        // Gender param
        uint8_t wcGender = gender(); // 0=M, 1=F, 2=Genderless, 3=Random
        uint8_t genderRatio;
        if (wcGender == 0) genderRatio = 0;         // always male
        else if (wcGender == 1) genderRatio = 0xFE;  // always female
        else if (wcGender == 2) genderRatio = 0xFF;   // genderless
        else genderRatio = PersonalZA::getGenderRatio(specNat, formVal); // random from species

        // Nature param
        uint8_t wcNature = nature();
        bool fixedNature = (wcNature < 25);

        // Scale param
        uint16_t scaleVal = scale();
        bool fixedScale = (scaleVal != 256); // 256 = random

        // Generate seed (mimics TryApply64 outer loop — first seed always works with unrestricted)
        uint64_t init = randU64();
        Xoroshiro128Plus outerRand(init);
        uint64_t seed = outerRand.next();
        Xoroshiro128Plus rand(seed);

        // 1. EncryptionConstant
        uint32_t ec = static_cast<uint32_t>(rand.nextInt(0xFFFFFFFF));
        pk.writeU32(0x00, ec);

        // 2. PID — SkipTrainer: no fakeTID call, use pk.ID32 directly
        uint32_t pidVal = static_cast<uint32_t>(rand.nextInt(0xFFFFFFFF));
        // Single shiny roll (RollCount=1)
        if (shinyMode == ShinyRandom) {
            // ForceShinyState: adapt PID to match shiny determination against real ID32
            // Since SkipTrainer means fakeTID == pkId32, ForceShinyState is a no-op
            // (the PID is already evaluated against the same ID32)
        } else { // ShinyNever (also used for Star/Square, PID overridden post-RNG)
            // PKHeX does two anti-shiny checks (against fakeTID and pk.ID32)
            // With SkipTrainer both are the same ID, but we match PKHeX exactly
            if (isShiny(pidVal, pkId32))
                pidVal ^= 0x10000000;
            if (isShiny(pidVal, pkId32))
                pidVal ^= 0x10000000;
        }
        // Post-RNG PID override for Star/Square/Fixed
        if (pType == 2) { // AlwaysStar
            uint16_t tid = static_cast<uint16_t>(pkId32 & 0xFFFF);
            uint16_t sid = static_cast<uint16_t>(pkId32 >> 16);
            uint16_t low = static_cast<uint16_t>(pid() & 0xFFFF);
            pidVal = (static_cast<uint32_t>(1u ^ low ^ tid ^ sid) << 16) | low;
        } else if (pType == 3) { // AlwaysSquare
            uint16_t tid = static_cast<uint16_t>(pkId32 & 0xFFFF);
            uint16_t sid = static_cast<uint16_t>(pkId32 >> 16);
            uint16_t low = static_cast<uint16_t>(pid() & 0xFFFF);
            pidVal = (static_cast<uint32_t>(0u ^ low ^ tid ^ sid) << 16) | low;
        } else if (pType == 4) { // FixedValue
            pidVal = pid();
        }
        pk.writeU32(0x1C, pidVal);

        // 3. IVs — from seed
        int ivs[6] = {-1, -1, -1, -1, -1, -1}; // HP, ATK, DEF, SPA, SPD, SPE

        if (hasFixedIVs) {
            // Copy exact IVs (clamped to 0-31)
            ivs[0] = wcIVs[0] > 31 ? -1 : wcIVs[0]; // HP
            ivs[1] = wcIVs[1] > 31 ? -1 : wcIVs[1]; // ATK
            ivs[2] = wcIVs[2] > 31 ? -1 : wcIVs[2]; // DEF
            ivs[3] = wcIVs[4] > 31 ? -1 : wcIVs[4]; // SPA (note: WA9 order is HP,ATK,DEF,SPE,SPA,SPD)
            ivs[4] = wcIVs[5] > 31 ? -1 : wcIVs[5]; // SPD
            ivs[5] = wcIVs[3] > 31 ? -1 : wcIVs[3]; // SPE
        }

        // Place flawless IVs from RNG
        for (int i = 0; i < flawlessIVs; ) {
            int idx = static_cast<int>(rand.nextInt(6));
            if (ivs[idx] != -1) continue; // slot already filled, re-roll
            ivs[idx] = 31;
            i++;
        }
        // Fill remaining from RNG
        for (int i = 0; i < 6; i++) {
            if (ivs[i] == -1)
                ivs[i] = static_cast<int>(rand.nextInt(32));
        }

        // Pack IV32: HP(0-4), ATK(5-9), DEF(10-14), SPE(15-19), SPA(20-24), SPD(25-29)
        uint32_t iv32 = static_cast<uint32_t>(ivs[0])
                      | (static_cast<uint32_t>(ivs[1]) << 5)
                      | (static_cast<uint32_t>(ivs[2]) << 10)
                      | (static_cast<uint32_t>(ivs[5]) << 15)  // SPE at index 5 → bits 15-19
                      | (static_cast<uint32_t>(ivs[3]) << 20)  // SPA at index 3 → bits 20-24
                      | (static_cast<uint32_t>(ivs[4]) << 25); // SPD at index 4 → bits 25-29

        // 4. Ability from RNG
        int abIdx;
        if (abType >= 4) { // Any12H
            abIdx = static_cast<int>(rand.nextInt(3));
        } else if (abType == 3) { // Any12
            abIdx = static_cast<int>(rand.nextInt(2));
        } else {
            abIdx = abType; // fixed: 0, 1, or 2
        }

        // 5. Gender from RNG
        uint8_t genderVal;
        if (genderRatio == 0xFF) genderVal = 2;       // genderless
        else if (genderRatio == 0xFE) genderVal = 1;  // always female
        else if (genderRatio == 0x00) genderVal = 0;   // always male
        else {
            uint64_t gRand = rand.nextInt(100);
            // PKHeX threshold: ratio 31→12%, 63→25%, 127→50%, 191→75%, 225→89%
            int threshold;
            if (genderRatio <= 31) threshold = 12;
            else if (genderRatio <= 63) threshold = 25;
            else if (genderRatio <= 127) threshold = 50;
            else if (genderRatio <= 191) threshold = 75;
            else threshold = 89;
            genderVal = (gRand < static_cast<uint64_t>(threshold)) ? 1 : 0;
        }

        // 6. Nature from RNG
        uint8_t nat;
        if (fixedNature) {
            nat = wcNature;
        } else {
            nat = static_cast<uint8_t>(rand.nextInt(25));
        }

        // 7. Scale from RNG
        uint8_t scaleResult;
        if (fixedScale) {
            scaleResult = static_cast<uint8_t>(scaleVal);
        } else {
            // Triangular distribution: nextInt(0x81) + nextInt(0x80)
            scaleResult = static_cast<uint8_t>(rand.nextInt(0x81) + rand.nextInt(0x80));
        }

        // Apply Lumiose RNG results to PA9
        uint16_t abilityId = PersonalZA::getAbility(specNat, formVal, abIdx);
        pk.writeU16(0x14, abilityId);
        pk.data[0x16] = static_cast<uint8_t>(1 << abIdx); // AbilityNumber

        pk.data[0x20] = nat;       // Nature
        pk.data[0x21] = nat;       // StatNature

        // Gender + FatefulEncounter (0x22): bit0=fateful, bits1-2=gender (same as PK9)
        pk.data[0x22] = static_cast<uint8_t>((genderVal << 1) | 1);

        // IsAlpha (0x23) — PA9-specific, PK9 has this as unused padding
        pk.data[0x23] = isAlpha() != 0 ? 1 : 0;

        // IsEgg + IsNicknamed bits in IV32
        bool isNicknamed = false; // set below during nickname handling
        if (isEggWC)
            iv32 |= (1u << 30);
        // IsNicknamed bit set later after nickname resolution

        // Scale (0x4A)
        pk.data[0x4A] = scaleResult;

        // --- Now set non-RNG fields ---

        // Form (0x24)
        pk.data[0x24] = formVal;

        // EVs (0x26-0x2B)
        pk.data[0x26] = evHp();
        pk.data[0x27] = evAtk();
        pk.data[0x28] = evDef();
        pk.data[0x29] = evSpe();
        pk.data[0x2A] = evSpA();
        pk.data[0x2B] = evSpD();

        // NO ribbons for WA9 (commented out in PKHeX WA9.cs)

        // Nickname (0x58)
        uint8_t pkLang;
        {
            // WA9 language byte is in OT area: 0x140 + langIdx*0x1C + 0x1A
            int lo = 0x140 + langIdx * 0x1C + 0x1A;
            uint8_t wcLang = data[lo];
            pkLang = wcLang != 0 ? wcLang : trainer.language;
        }
        {
            std::u16string nick;
            isNicknamed = hasNickname(langIdx);
            if (isEggWC) {
                const std::string& eggName = getLocalizedSpeciesName(0, pkLang);
                nick = !eggName.empty() ? utf8to16(eggName) : utf8to16("Egg");
                isNicknamed = true;
            } else if (isNicknamed) {
                nick = getNickname(langIdx);
            } else {
                const std::string& name = getLocalizedSpeciesName(specNat, pkLang);
                nick = !name.empty() ? utf8to16(name) : utf8to16(SpeciesName::get(specNat));
            }
            for (size_t i = 0; i < nick.size() && i < 13; i++) {
                uint16_t ch = static_cast<uint16_t>(nick[i]);
                std::memcpy(pk.data.data() + 0x58 + i * 2, &ch, 2);
            }

            // Set IsNicknamed bit in IV32
            if (isNicknamed)
                iv32 |= (1u << 31);
            pk.writeU32(0x8C, iv32);
        }

        // Moves (0x72-0x78)
        pk.writeU16(0x72, move1());
        pk.writeU16(0x74, move2());
        pk.writeU16(0x76, move3());
        pk.writeU16(0x78, move4());

        // Relearn Moves (0x82-0x88)
        pk.writeU16(0x82, relearnMove1());
        pk.writeU16(0x84, relearnMove2());
        pk.writeU16(0x86, relearnMove3());
        pk.writeU16(0x88, relearnMove4());

        // Plus flags (PA9-specific: 0xD6 = PlusFlags0 (264 bits), 0x94 = PlusFlags1 (96 bits))
        // Ref: PKHeX PlusRecordApplicator.SetPlusFlagsEncounter
        {
            int entryIdx = PersonalZA::getEntryIndex(specNat, formVal);
            auto plusLearnset = PlusZA::parseLearnset(PlusZA::PLUS_ZA_PKL, PlusZA::PLUS_ZA_PKL_SIZE, entryIdx);
            for (const auto& ml : plusLearnset) {
                if (curLevel < ml.level) break; // learnset is sorted by level
                int pmIdx = PlusZA::findPlusMoveIndex(ml.move);
                if (pmIdx < 0) continue;
                // Set bit: indices 0-263 → byte at 0xD6, indices 264-359 → byte at 0x94
                if (pmIdx < 264) {
                    int byteOfs = 0xD6 + (pmIdx >> 3);
                    pk.data[byteOfs] |= (1 << (pmIdx & 7));
                } else {
                    int adj = pmIdx - 264;
                    int byteOfs = 0x94 + (adj >> 3);
                    pk.data[byteOfs] |= (1 << (adj & 7));
                }
            }
        }

        // SetMoves fallback: if WA9 doesn't specify moves, use learnset encounter moves
        if (move1() == 0) {
            int entryIdx = PersonalZA::getEntryIndex(specNat, formVal);
            auto lvlLearnset = PlusZA::parseLearnset(PlusZA::LVLMOVE_ZA_PKL, PlusZA::LVLMOVE_ZA_PKL_SIZE, entryIdx);
            // Circular buffer of last 4 moves at or below curLevel (skip level 0)
            uint16_t moves[4] = {0, 0, 0, 0};
            int ctr = 0;
            for (const auto& ml : lvlLearnset) {
                if (ml.level < 1) continue;
                if (ml.level > curLevel) break;
                moves[ctr & 3] = ml.move;
                ctr++;
            }
            // Rectify order (rotate so oldest is first)
            if (ctr > 4) {
                uint16_t tmp[4];
                int start = ctr & 3;
                for (int i = 0; i < 4; i++)
                    tmp[i] = moves[(start + i) & 3];
                std::memcpy(moves, tmp, sizeof(moves));
            }
            pk.writeU16(0x72, moves[0]);
            pk.writeU16(0x74, moves[1]);
            pk.writeU16(0x76, moves[2]);
            pk.writeU16(0x78, moves[3]);
        }

        // Move PP (0x7A-0x7D) — always after move finalization
        pk.data[0x7A] = getMovePP(pk.readU16(0x72));
        pk.data[0x7B] = getMovePP(pk.readU16(0x74));
        pk.data[0x7C] = getMovePP(pk.readU16(0x76));
        pk.data[0x7D] = getMovePP(pk.readU16(0x78));
    }

    // HT Name (0xA8) - if hasOT: player's OT; else: empty
    if (hasOT) {
        for (size_t i = 0; i < trainer.otName.size() && i < 13; i++) {
            uint16_t ch = static_cast<uint16_t>(trainer.otName[i]);
            std::memcpy(pk.data.data() + 0xA8 + i * 2, &ch, 2);
        }
        pk.data[0xC2] = trainer.gender;    // HT Gender
        pk.data[0xC3] = trainer.language;  // HT Language
    }

    // CurrentHandler (0xC4)
    pk.data[0xC4] = hasOT ? 1 : 0;

    // Version (0xCE)
    int32_t origGame = originGame();
    if (origGame != 0) {
        pk.data[0xCE] = static_cast<uint8_t>(origGame);
    } else if (trainer.gameVersion == 52) {
        pk.data[0xCE] = 52;
    } else {
        pk.data[0xCE] = 52; // ZA only
    }

    // Language (0xD5)
    uint8_t pkLangFinal;
    {
        int lo = 0x140 + langIdx * 0x1C + 0x1A;
        uint8_t wcLang = data[lo];
        pkLangFinal = wcLang != 0 ? wcLang : trainer.language;
    }
    pk.data[0xD5] = pkLangFinal;

    // AffixedRibbon (0xD4) = -1 (0xFF) — no ribbons set
    pk.data[0xD4] = 0xFF;

    // OT Name (0xF8) - if hasOT: WA9 OT; else: player's OT
    {
        const std::u16string& otStr = hasOT ? getOTName(langIdx) : trainer.otName;
        for (size_t i = 0; i < otStr.size() && i < 13; i++) {
            uint16_t ch = static_cast<uint16_t>(otStr[i]);
            std::memcpy(pk.data.data() + 0xF8 + i * 2, &ch, 2);
        }
    }

    // EXP (0x10)
    {
        uint8_t growth = PersonalZA::getGrowthRate(specNat, formVal);
        if (growth > 5) growth = 0;
        uint32_t exp = (curLevel >= 1 && curLevel <= 100) ? EXP_TABLE[growth][curLevel - 1] : 0;
        pk.writeU32(0x10, exp);
    }

    // OT_Friendship (0x112) / HT_Friendship (0xC8) / CurrentFriendship routing
    {
        uint8_t baseFriend = PersonalZA::getBaseFriendship(specNat, formVal);
        uint8_t curFriend = isEggWC ? PersonalZA::getHatchCycles(specNat, formVal) : baseFriend;
        pk.data[0x112] = baseFriend;
        if (hasOT)
            pk.data[0xC8] = curFriend;
        else
            pk.data[0x112] = curFriend;
    }

    // OT Memory fields (0x113-0x118)
    pk.data[0x113] = otMemoryIntensity();
    pk.data[0x114] = otMemory();
    pk.writeU16(0x116, otMemoryVariable());
    pk.data[0x118] = otMemoryFeeling();

    // MetDate (0x11C-0x11E)
    MetDate md = getDistributionDateWA9(cardID());
    pk.data[0x11C] = md.year;
    pk.data[0x11D] = md.month;
    pk.data[0x11E] = md.day;

    // EggMetDate (0x119-0x11B) — for egg wondercards
    if (isEggWC) {
        pk.data[0x119] = md.year;
        pk.data[0x11A] = md.month;
        pk.data[0x11B] = md.day;
    }

    // ObedienceLevel (0x11F)
    pk.data[0x11F] = curLevel;

    // EggLocation (0x120)
    pk.writeU16(0x120, eggLocation());

    // MetLocation (0x122)
    pk.writeU16(0x122, metLocation());

    // Ball (0x124)
    uint8_t ballVal = ball() != 0 ? static_cast<uint8_t>(ball()) : 4;
    pk.data[0x124] = ballVal;

    // MetLevel (0x125 bits 0-6) + OTGender (bit 7)
    uint8_t otGenderBit = otGender() < 2 ? otGender() : trainer.gender;
    pk.data[0x125] = static_cast<uint8_t>((mLevel & 0x7F) | ((otGenderBit & 1) << 7));

    // ID32 already set above (0x0C)

    // Level in party stats (0x148)
    pk.data[0x148] = curLevel;

    // Party stats calculation
    {
        auto bs = PersonalZA::getBaseStats(specNat, formVal);
        uint32_t storedIV32 = pk.readU32(0x8C);
        int iv_hp  = storedIV32 & 0x1F;
        int iv_atk = (storedIV32 >> 5) & 0x1F;
        int iv_def = (storedIV32 >> 10) & 0x1F;
        int iv_spe = (storedIV32 >> 15) & 0x1F;
        int iv_spa = (storedIV32 >> 20) & 0x1F;
        int iv_spd = (storedIV32 >> 25) & 0x1F;
        int ev_hp = pk.data[0x26], ev_atk = pk.data[0x27], ev_def = pk.data[0x28];
        int ev_spe = pk.data[0x29], ev_spa = pk.data[0x2A], ev_spd = pk.data[0x2B];

        uint8_t nat = pk.data[0x20];
        int boosted = nat / 5;
        int reduced = nat % 5;

        // HP stat (Shedinja always 1)
        int hp;
        if (specNat == 292) {
            hp = 1;
        } else {
            hp = ((2 * bs.hp + iv_hp + ev_hp / 4) * curLevel / 100) + curLevel + 10;
        }
        pk.writeU16(0x14A, static_cast<uint16_t>(hp));
        pk.writeU16(0x8A, static_cast<uint16_t>(hp)); // Stat_HPCurrent

        auto calcStat = [&](int base, int iv, int ev, int statIdx) -> uint16_t {
            int val = ((2 * base + iv + ev / 4) * curLevel / 100) + 5;
            if (boosted != reduced) {
                if (statIdx == boosted) val = val * 11 / 10;
                if (statIdx == reduced) val = val * 9 / 10;
            }
            return static_cast<uint16_t>(val);
        };
        pk.writeU16(0x14C, calcStat(bs.atk, iv_atk, ev_atk, 0));
        pk.writeU16(0x14E, calcStat(bs.def_, iv_def, ev_def, 1));
        pk.writeU16(0x150, calcStat(bs.spe, iv_spe, ev_spe, 2));
        pk.writeU16(0x152, calcStat(bs.spa, iv_spa, ev_spa, 3));
        pk.writeU16(0x154, calcStat(bs.spd, iv_spd, ev_spd, 4));
    }

    pk.refreshChecksum();

    return pk;
}

// --- WB8 → PB8 conversion (Brilliant Diamond / Shining Pearl) ---
// Ported from PKHeX.Core/MysteryGifts/WB8.cs ConvertToPKM()
// PB8 shares G8PKM layout with PK8 (same encryption, same offsets)

Pokemon WB8::convertToPB8(const TrainerInfo& trainer) const {
    static bool seeded = false;
    if (!seeded) { std::srand(static_cast<unsigned>(std::time(nullptr))); seeded = true; }

    Pokemon pk;
    pk.gameType_ = GameType::BD; // PB8 format (G8PKM)
    pk.data.fill(0);

    uint16_t specNat = species(); // WB8 species is already national dex
    uint8_t formVal = form();
    uint8_t curLevel = level() > 0 ? level() : static_cast<uint8_t>(1 + std::rand() % 100);
    uint8_t mLevel = metLevel() > 0 ? metLevel() : curLevel;
    int langIdx = getLanguageIndex(trainer.language);
    bool hasOT = getHasOT(langIdx);
    bool homeGift = isHOMEGift();

    // EncryptionConstant (0x00)
    uint32_t ec = encryptionConst() != 0 ? encryptionConst() : randU32();
    pk.writeU32(0x00, ec);

    // Species (0x08)
    pk.writeU16(0x08, specNat);

    // HeldItem (0x0A)
    pk.writeU16(0x0A, heldItem());

    // TID16/SID16 (0x0C-0x0F) — start with WB8 values
    uint16_t pkTid16 = tid16();
    uint16_t pkSid16 = sid16();

    // Ability (0x14, 0x16)
    int abIdx = abilityType();
    if (abIdx == 3) abIdx = std::rand() % 2;        // Any12: random 0/1
    else if (abIdx == 4) abIdx = std::rand() % 3;   // Any12H: random 0/1/2
    else if (abIdx > 4) abIdx = 0;                    // safety fallback
    uint16_t abilityId = PersonalBDSP::getAbility(specNat, formVal, abIdx);
    pk.writeU16(0x14, abilityId);
    pk.data[0x16] = static_cast<uint8_t>(1 << abIdx);

    // Nature (0x20, 0x21)
    uint8_t nat = nature();
    if (nat == 255) nat = static_cast<uint8_t>(std::rand() % 25);
    pk.data[0x20] = nat;
    pk.data[0x21] = nat;

    // Gender + FatefulEncounter (0x22) — PB8/G8PKM: bit0=fateful, bits 2-3=gender
    uint8_t genderVal = gender();
    if (genderVal == 3) {
        uint8_t ratio = PersonalBDSP::getGenderRatio(specNat, formVal);
        if (ratio == 0xFF) genderVal = 2;
        else if (ratio == 0xFE) genderVal = 1;
        else if (ratio == 0x00) genderVal = 0;
        else genderVal = (std::rand() % 256) >= ratio ? 0 : 1;
    }
    pk.data[0x22] = static_cast<uint8_t>((genderVal << 2) | 1);

    // Form (0x24)
    if (specNat == 678) // Meowstic
        pk.data[0x24] = static_cast<uint8_t>(genderVal & 1);
    else
        pk.data[0x24] = formVal;

    // EVs (0x26-0x2B)
    pk.data[0x26] = evHp();
    pk.data[0x27] = evAtk();
    pk.data[0x28] = evDef();
    pk.data[0x29] = evSpe();
    pk.data[0x2A] = evSpA();
    pk.data[0x2B] = evSpD();

    // Contest stats (0x2C-0x31) — WB8 has these, WC8 doesn't
    pk.data[0x2C] = contestCool();
    pk.data[0x2D] = contestBeauty();
    pk.data[0x2E] = contestCute();
    pk.data[0x2F] = contestSmart();
    pk.data[0x30] = contestTough();
    pk.data[0x31] = contestSheen();

    // Ribbons (0x34+): same logic as WC8
    for (int i = 0; i < RIBBON_SIZE; i++) {
        uint8_t ribbon = ribbonData()[i];
        if (ribbon == 0xFF) continue;
        int byteIdx = ribbon < 64 ? 0x34 + (ribbon >> 3) : 0x40 + ((ribbon - 64) >> 3);
        int bitIdx = ribbon & 7;
        if (byteIdx < static_cast<int>(pk.data.size()))
            pk.data[byteIdx] |= (1 << bitIdx);
    }

    // HeightScalar (0x50), WeightScalar (0x51)
    if (cardID() == 9026) {
        // Manaphy: fixed 128
        pk.data[0x50] = 128;
        pk.data[0x51] = 128;
    } else {
        pk.data[0x50] = static_cast<uint8_t>((std::rand() % 0x81) + (std::rand() % 0x80));
        pk.data[0x51] = static_cast<uint8_t>((std::rand() % 0x81) + (std::rand() % 0x80));
    }

    // Distribution date
    MetDate md = getDistributionDateWB8(cardID());

    // Nickname (0x58)
    uint8_t pkLang;
    bool isEggWC = isEgg();
    {
        int lo = 0x30 + langIdx * 0x20 + 0x1A;
        uint8_t wcLang = data[lo];
        pkLang = wcLang != 0 ? wcLang : trainer.language;
    }

    // IsDateLockJapanese: cards 9015/9016/9017 (HOME Sinnoh starters)
    // were JPN-only before 2022-05-20; clamp date for non-Japanese recipients
    {
        uint16_t cid = cardID();
        if ((cid == 9015 || cid == 9016 || cid == 9017) &&
            pkLang != 1 /* Japanese */ &&
            (md.year < 22 || (md.year == 22 && (md.month < 5 || (md.month == 5 && md.day < 20))))) {
            md = { 22, 5, 20 };
        }
    }

    // MetDate (0x11C-0x11E) — write after date lock adjustment
    pk.data[0x11C] = md.year;
    pk.data[0x11D] = md.month;
    pk.data[0x11E] = md.day;

    {
        std::u16string nick;
        bool isNicknamed = hasNickname(langIdx);
        if (isEggWC) {
            const std::string& eggName = getLocalizedSpeciesName(0, pkLang);
            if (!eggName.empty())
                nick = utf8to16(eggName);
            else
                nick = utf8to16("Egg");
            isNicknamed = false; // WB8 eggs: IsNicknamed = false (unlike WC8)
        } else if (isNicknamed) {
            nick = getNickname(langIdx);
        } else {
            const std::string& name = getLocalizedSpeciesName(specNat, pkLang);
            if (!name.empty())
                nick = utf8to16(name);
            else
                nick = utf8to16(SpeciesName::get(specNat));
        }
        for (size_t i = 0; i < nick.size() && i < 13; i++) {
            uint16_t ch = static_cast<uint16_t>(nick[i]);
            std::memcpy(pk.data.data() + 0x58 + i * 2, &ch, 2);
        }

        // Moves (0x72-0x78)
        pk.writeU16(0x72, move1());
        pk.writeU16(0x74, move2());
        pk.writeU16(0x76, move3());
        pk.writeU16(0x78, move4());

        // Move PP (0x7A-0x7D)
        pk.data[0x7A] = getMovePP(move1());
        pk.data[0x7B] = getMovePP(move2());
        pk.data[0x7C] = getMovePP(move3());
        pk.data[0x7D] = getMovePP(move4());

        // Relearn Moves (0x82-0x88)
        pk.writeU16(0x82, relearnMove1());
        pk.writeU16(0x84, relearnMove2());
        pk.writeU16(0x86, relearnMove3());
        pk.writeU16(0x88, relearnMove4());

        // IVs (0x8C) — packed u32
        uint8_t ivs[6] = { ivHp(), ivAtk(), ivDef(), ivSpe(), ivSpA(), ivSpD() };
        int perfectCount = 0;
        for (int i = 0; i < 6; i++) {
            if (ivs[i] >= 0xFC) {
                perfectCount = ivs[i] - 0xFB;
                break;
            }
        }
        if (perfectCount > 0) {
            for (int i = 0; i < 6; i++)
                ivs[i] = static_cast<uint8_t>(std::rand() % 32);
            int placed = 0;
            while (placed < perfectCount) {
                int idx = std::rand() % 6;
                if (ivs[idx] != 31) {
                    ivs[idx] = 31;
                    placed++;
                }
            }
        } else {
            for (int i = 0; i < 6; i++) {
                if (ivs[i] > 31)
                    ivs[i] = static_cast<uint8_t>(std::rand() % 32);
            }
        }
        uint32_t iv32 = (ivs[0]) | (ivs[1] << 5) | (ivs[2] << 10)
                       | (ivs[3] << 15) | (ivs[4] << 20) | (ivs[5] << 25);
        if (isEggWC)
            iv32 |= (1u << 30);
        if (isNicknamed)
            iv32 |= (1u << 31);
        pk.writeU32(0x8C, iv32);
    }

    // HT Name (0xA8) — same offset as PK8
    if (hasOT) {
        for (size_t i = 0; i < trainer.otName.size() && i < 13; i++) {
            uint16_t ch = static_cast<uint16_t>(trainer.otName[i]);
            std::memcpy(pk.data.data() + 0xA8 + i * 2, &ch, 2);
        }
        pk.data[0xC2] = trainer.gender;
        pk.data[0xC3] = trainer.language;
    }

    // CurrentHandler (0xC4)
    pk.data[0xC4] = hasOT ? 1 : 0;

    // Version (0xDE) — PB8/G8PKM offset
    {
        uint8_t ver;
        int32_t origGame = originGame();
        if (origGame != 0) {
            ver = static_cast<uint8_t>(origGame);
        } else if (trainer.gameVersion == 48 || trainer.gameVersion == 49) {
            ver = trainer.gameVersion;
        } else {
            ver = 48; // fallback to BD
        }
        pk.data[0xDE] = ver;
    }

    // Language (0xE2) — PB8/G8PKM offset
    pk.data[0xE2] = pkLang;

    // AffixedRibbon (0xE8) — PB8/G8PKM offset, set to -1 (none)
    pk.data[0xE8] = 0xFF;

    // OT Name (0xF8) — same offset as PK8
    {
        const std::u16string& otStr = hasOT ? getOTName(langIdx) : trainer.otName;
        for (size_t i = 0; i < otStr.size() && i < 13; i++) {
            uint16_t ch = static_cast<uint16_t>(otStr[i]);
            std::memcpy(pk.data.data() + 0xF8 + i * 2, &ch, 2);
        }
    }

    // EXP (0x10)
    {
        uint8_t growth = PersonalBDSP::getGrowthRate(specNat, formVal);
        if (growth > 5) growth = 0;
        uint32_t exp = (curLevel >= 1 && curLevel <= 100) ? EXP_TABLE[growth][curLevel - 1] : 0;
        pk.writeU32(0x10, exp);
    }

    // ID32 handling — WB8: only override when both TID16/SID16 == 0
    if (pkTid16 == 0 && pkSid16 == 0) {
        pkTid16 = static_cast<uint16_t>(trainer.id32 & 0xFFFF);
        pkSid16 = static_cast<uint16_t>(trainer.id32 >> 16);
    }
    pk.writeU16(0x0C, pkTid16);
    pk.writeU16(0x0E, pkSid16);

    // OTFriendship (0x112) / CurrentFriendship
    {
        uint8_t baseFriend = PersonalBDSP::getBaseFriendship(specNat, formVal);
        uint8_t curFriend = isEggWC ? PersonalBDSP::getHatchCycles(specNat, formVal) : baseFriend;
        pk.data[0x112] = baseFriend;
        if (hasOT)
            pk.data[0xC8] = curFriend;
        else
            pk.data[0x112] = curFriend;
    }

    // EggLocation (0x120) — default 0xFFFF for BDSP
    {
        uint16_t eggLoc = eggLocation();
        if (eggLoc == 0) eggLoc = 0xFFFF;
        pk.writeU16(0x120, eggLoc);
    }

    // MetLocation (0x122)
    pk.writeU16(0x122, metLocation());

    // Ball (0x124)
    uint8_t ballVal = ball() != 0 ? static_cast<uint8_t>(ball()) : 4;

    // MetLevel (0x125 bits 0-6) + OTGender (bit 7)
    uint8_t otGenderBit = otGender() < 2 ? otGender() : trainer.gender;
    pk.data[0x124] = ballVal;
    pk.data[0x125] = static_cast<uint8_t>((mLevel & 0x7F) | ((otGenderBit & 1) << 7));

    // EggMetDate (0x119-0x11B) and egg handling
    if (isEggWC) {
        // SetEggMetData: eggLoc = metLoc, metLoc = 0xFFFE
        // IsEgg bit is already set via IV32 bit 30 in the IV section above
        pk.writeU16(0x120, metLocation()); // eggLocation = original metLocation
        pk.writeU16(0x122, 0xFFFF);        // metLocation = Default8bNone (0xFFFF)
        pk.data[0x119] = md.year;
        pk.data[0x11A] = md.month;
        pk.data[0x11B] = md.day;
    }

    // PID (0x1C) — ShinyType8: same as WC8
    {
        uint8_t pType = pidType();
        uint32_t pidVal;
        switch (pType) {
            case 0: { // Never shiny (antishiny) — single XOR check
                pidVal = randU32();
                uint32_t xorVal = (pidVal >> 16) ^ (pidVal & 0xFFFF) ^ pkTid16 ^ pkSid16;
                if (xorVal < 16)
                    pidVal ^= 0x10000000;
                break;
            }
            case 1: // Random
                pidVal = randU32();
                break;
            case 2: { // Always Star shiny
                uint16_t low = static_cast<uint16_t>(pid() & 0xFFFF);
                uint16_t high = 1u ^ low ^ pkTid16 ^ pkSid16;
                pidVal = (static_cast<uint32_t>(high) << 16) | low;
                break;
            }
            case 3: { // Always Square shiny
                uint16_t low = static_cast<uint16_t>(pid() & 0xFFFF);
                uint16_t high = 0u ^ low ^ pkTid16 ^ pkSid16;
                pidVal = (static_cast<uint32_t>(high) << 16) | low;
                break;
            }
            case 4: { // Fixed value — PKHeX GetFixedPID logic
                pidVal = pid();
                if (pidVal != 0 && id32() != 0) {
                    break; // both non-zero: use PID as-is
                }
                // Check if PID would be shiny against PB8's TID/SID
                uint32_t xorVal = (pidVal >> 16) ^ (pidVal & 0xFFFF) ^ pkTid16 ^ pkSid16;
                if (xorVal >= 16) {
                    break; // not shiny: use PID as-is
                }
                // PID IS shiny — HOME anti-shiny: tr.ID32 + 0x10
                if (homeGift) {
                    uint32_t trId32 = (static_cast<uint32_t>(pkSid16) << 16) | pkTid16;
                    pidVal = trId32 + 0x10u;
                }
                break;
            }
            default:
                pidVal = pid();
                break;
        }
        pk.writeU32(0x1C, pidVal);
    }

    // Level in party stats (0x148)
    pk.data[0x148] = curLevel;

    // Party stats calculation
    {
        auto bs = PersonalBDSP::getBaseStats(specNat, formVal);
        uint32_t storedIV32 = pk.readU32(0x8C);
        int iv_hp  = storedIV32 & 0x1F;
        int iv_atk = (storedIV32 >> 5) & 0x1F;
        int iv_def = (storedIV32 >> 10) & 0x1F;
        int iv_spe = (storedIV32 >> 15) & 0x1F;
        int iv_spa = (storedIV32 >> 20) & 0x1F;
        int iv_spd = (storedIV32 >> 25) & 0x1F;
        int ev_hp = pk.data[0x26], ev_atk = pk.data[0x27], ev_def = pk.data[0x28];
        int ev_spe = pk.data[0x29], ev_spa = pk.data[0x2A], ev_spd = pk.data[0x2B];

        int hp;
        if (specNat == 292) {
            hp = 1;
        } else {
            hp = ((2 * bs.hp + iv_hp + ev_hp / 4) * curLevel / 100) + curLevel + 10;
        }
        pk.writeU16(0x14A, static_cast<uint16_t>(hp));
        pk.writeU16(0x8A, static_cast<uint16_t>(hp));

        int boosted = nat / 5;
        int reduced = nat % 5;
        auto calcStat = [&](int base, int iv, int ev, int statIdx) -> uint16_t {
            int val = ((2 * base + iv + ev / 4) * curLevel / 100) + 5;
            if (boosted != reduced) {
                if (statIdx == boosted) val = val * 11 / 10;
                if (statIdx == reduced) val = val * 9 / 10;
            }
            return static_cast<uint16_t>(val);
        };
        pk.writeU16(0x14C, calcStat(bs.atk, iv_atk, ev_atk, 0));
        pk.writeU16(0x14E, calcStat(bs.def_, iv_def, ev_def, 1));
        pk.writeU16(0x150, calcStat(bs.spe, iv_spe, ev_spe, 2));
        pk.writeU16(0x152, calcStat(bs.spa, iv_spa, ev_spa, 3));
        pk.writeU16(0x154, calcStat(bs.spd, iv_spd, ev_spd, 4));
    }

    pk.refreshChecksum();

    return pk;
}

// --- File scanner ---

std::vector<WCInfo> scanWondercards(const std::string& basePath, GameType game) {
    std::vector<WCInfo> results;

    // Build path: basePath + "wondercards/" + bankFolderName + "/"
    std::string wcDir = basePath + "wondercards/" + bankFolderNameOf(game) + "/";

    bool swsh = isSwSh(game);
    bool za   = (game == GameType::ZA);
    bool bdsp = isBDSP(game);
    const char* targetExt = swsh ? ".wc8" : (za ? ".wa9" : (bdsp ? ".wb8" : ".wc9"));

    DIR* dir = opendir(wcDir.c_str());
    if (!dir) return results;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() < 5) continue;
        std::string ext = name.substr(name.size() - 4);
        for (auto& c : ext) c = static_cast<char>(std::tolower(c));
        if (ext != targetExt) continue;

        std::string fullPath = wcDir + name;

        if (swsh) {
            WC8 wc;
            if (!wc.loadFromFile(fullPath) || !wc.isPokemon()) {
                results.push_back({ name, fullPath, 0, 0, 0, false, false, false });
                continue;
            }

            uint16_t specNat = wc.species(); // already national dex

            // Shiny detection — same ShinyType8 enum as WC9
            bool isShiny = false;
            uint8_t pType = wc.pidType();
            if (pType == 2 || pType == 3) {
                isShiny = true;
            } else if (pType == 4) {
                uint32_t wid = wc.id32();
                uint32_t wpid = wc.pid();
                if (wid != 0) {
                    uint16_t t = static_cast<uint16_t>(wid & 0xFFFF);
                    uint16_t s = static_cast<uint16_t>(wid >> 16);
                    uint32_t xor_val = (wpid >> 16) ^ (wpid & 0xFFFF) ^ t ^ s;
                    isShiny = xor_val < 16;
                }
            }

            bool hasOT = wc.getHasOT(getLanguageIndex(2));

            results.push_back({
                name, fullPath, wc.cardID(), specNat, wc.level(),
                isShiny, hasOT, true
            });
        } else if (za) {
            WA9 wc;
            if (!wc.loadFromFile(fullPath) || !wc.isPokemon()) {
                results.push_back({ name, fullPath, 0, 0, 0, false, false, false });
                continue;
            }

            uint16_t specNat = SpeciesConverter::getNational9(wc.speciesInternal());

            bool isShiny = false;
            uint8_t pType = wc.pidType();
            if (pType == 2 || pType == 3) {
                isShiny = true;
            } else if (pType == 4) {
                uint32_t wid = wc.id32();
                uint32_t wpid = wc.pid();
                if (wid != 0) {
                    uint16_t t = static_cast<uint16_t>(wid & 0xFFFF);
                    uint16_t s = static_cast<uint16_t>(wid >> 16);
                    uint32_t xor_val = (wpid >> 16) ^ (wpid & 0xFFFF) ^ t ^ s;
                    isShiny = xor_val < 16;
                }
            }

            bool hasOT = wc.getHasOT(getLanguageIndex(2));

            results.push_back({
                name, fullPath, wc.cardID(), specNat, wc.level(),
                isShiny, hasOT, true
            });
        } else if (bdsp) {
            WB8 wc;
            if (!wc.loadFromFile(fullPath) || !wc.isPokemon()) {
                results.push_back({ name, fullPath, 0, 0, 0, false, false, false });
                continue;
            }

            uint16_t specNat = wc.species(); // already national dex

            bool isShiny = false;
            uint8_t pType = wc.pidType();
            if (pType == 2 || pType == 3) {
                isShiny = true;
            } else if (pType == 4) {
                uint32_t wid = wc.id32();
                uint32_t wpid = wc.pid();
                if (wid != 0) {
                    uint16_t t = static_cast<uint16_t>(wid & 0xFFFF);
                    uint16_t s = static_cast<uint16_t>(wid >> 16);
                    uint32_t xor_val = (wpid >> 16) ^ (wpid & 0xFFFF) ^ t ^ s;
                    isShiny = xor_val < 16;
                }
            }

            bool hasOT = wc.getHasOT(getLanguageIndex(2));

            results.push_back({
                name, fullPath, wc.cardID(), specNat, wc.level(),
                isShiny, hasOT, true
            });
        } else {
            WC9 wc;
            if (!wc.loadFromFile(fullPath) || !wc.isPokemon()) {
                results.push_back({ name, fullPath, 0, 0, 0, false, false, false });
                continue;
            }

            uint16_t specNat = SpeciesConverter::getNational9(wc.speciesInternal());

            bool isShiny = false;
            uint8_t pType = wc.pidType();
            if (pType == 2 || pType == 3) {
                isShiny = true;
            } else if (pType == 4) {
                uint32_t wid = wc.id32();
                uint32_t wpid = wc.pid();
                if (wid != 0) {
                    uint16_t t = static_cast<uint16_t>(wid & 0xFFFF);
                    uint16_t s = static_cast<uint16_t>(wid >> 16);
                    uint32_t xor_val = (wpid >> 16) ^ (wpid & 0xFFFF) ^ t ^ s;
                    isShiny = xor_val < 16;
                }
            }

            bool hasOT = wc.getHasOT(getLanguageIndex(2));

            results.push_back({
                name, fullPath, wc.cardID(), specNat, wc.level(),
                isShiny, hasOT, true
            });
        }
    }
    closedir(dir);

    // Sort: valid entries first by cardID, then invalid by filename
    std::sort(results.begin(), results.end(),
        [](const WCInfo& a, const WCInfo& b) {
            if (a.valid != b.valid) return a.valid > b.valid;
            if (a.valid) return a.cardID < b.cardID;
            return a.filename < b.filename;
        });

    return results;
}
