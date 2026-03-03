#include "wondercard.h"
#include "personal_sv.h"
#include "encounter_server_date.h"
#include "species_converter.h"
#include "poke_crypto.h"
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
#include <algorithm>

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

// --- RNG helpers ---
static uint32_t randU32() {
    return (static_cast<uint32_t>(std::rand()) << 16) | (static_cast<uint32_t>(std::rand()) & 0xFFFF);
}

// --- WC9 → PK9 conversion ---

Pokemon WC9::convertToPK9(const TrainerInfo& trainer) const {
    Pokemon pk;
    pk.gameType_ = GameType::S; // PK9 format
    pk.data.fill(0);

    uint16_t specInt = speciesInternal();
    uint16_t specNat = SpeciesConverter::getNational9(specInt);
    uint8_t formVal = form();
    uint8_t curLevel = level() > 0 ? level() : 1;
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
    int abType = abilityType();
    uint16_t abilityId = PersonalSV::getAbility(specNat, formVal, abType);
    pk.writeU16(0x14, abilityId);
    pk.data[0x16] = static_cast<uint8_t>(abType <= 2 ? abType : 0);

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
        int byteIdx = 0x34 + ribbon / 8;
        int bitIdx = ribbon % 8;
        if (byteIdx < static_cast<int>(pk.data.size()))
            pk.data[byteIdx] |= (1 << bitIdx);
        // Set AffixedRibbon (0x3C is the AffixedRibbon offset in PK9 at 0x38...
        // Actually in PK9, AffixedRibbon is at offset 0x51 (sbyte)
        pk.data[0x51] = ribbon;
    }

    // Height/Weight scalars (0x48-0x49)
    pk.data[0x48] = heightScalar();
    pk.data[0x49] = weightScalar();

    // Scale (0x4A)
    uint16_t scaleVal = scale();
    if (scaleVal == 256)
        pk.data[0x4A] = static_cast<uint8_t>(std::rand() % 256);
    else
        pk.data[0x4A] = static_cast<uint8_t>(scaleVal);

    // Nickname (0x58) - UTF-16LE, up to 13 chars
    {
        std::u16string nick;
        bool isNicknamed = hasNickname(langIdx);
        if (isNicknamed) {
            nick = getNickname(langIdx);
        } else {
            // Use species name — get national ID and lookup name
            const std::string& name = SpeciesName::get(specNat);
            // Convert UTF-8 to UTF-16LE (basic ASCII conversion)
            for (char c : name)
                nick += static_cast<char16_t>(static_cast<uint8_t>(c));
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

        // Move PP (0x7A-0x7D) - set to 0 (game recalculates)
        // Already 0 from fill

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
        if (isNicknamed)
            iv32 |= (1u << 31); // IsNicknamed bit
        pk.writeU32(0x8C, iv32);
    }

    // TeraType (0x94)
    pk.data[0x94] = teraTypeOrig();

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
    } else {
        // Pick SL (50) or VL (51) randomly
        pk.data[0xCE] = 50 + (std::rand() % 2);
    }

    // Language (0xD5)
    {
        int langOfs = 0x28 + langIdx * 0x1C + 0x1A;
        uint8_t wcLang = data[langOfs];
        pk.data[0xD5] = wcLang != 0 ? wcLang : trainer.language;
    }

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

    // OTFriendship (0x112)
    pk.data[0x112] = PersonalSV::getBaseFriendship(specNat, formVal);

    // OT Memory fields (0x113-0x118)
    pk.data[0x113] = otMemoryIntensity();
    pk.data[0x114] = otMemory();
    pk.data[0x115] = otMemoryFeeling();
    pk.writeU16(0x116, otMemoryVariable());

    // MetDate (0x11C-0x11E) - year-2000, month, day
    uint16_t checksum = readU16(0x2C4);
    MetDate md = getDistributionDate(cardID(), checksum);
    pk.data[0x11C] = md.year;
    pk.data[0x11D] = md.month;
    pk.data[0x11E] = md.day;

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
    uint8_t otGenderBit = hasOT ? otGender() : trainer.gender;
    pk.data[0x124] = ballVal;
    pk.data[0x125] = static_cast<uint8_t>((mLevel & 0x7F) | ((otGenderBit & 1) << 7));

    // PID (0x1C) — now that we have ID32 finalized
    {
        uint16_t tid16 = static_cast<uint16_t>(pkId32 & 0xFFFF);
        uint16_t sid16 = static_cast<uint16_t>(pkId32 >> 16);
        uint8_t pType = pidType();
        uint32_t pidVal;
        switch (pType) {
            case 0: // Random
                pidVal = randU32();
                break;
            case 1: { // Never shiny (antishiny)
                pidVal = randU32();
                uint32_t xorVal = (pidVal >> 16) ^ (pidVal & 0xFFFF) ^ tid16 ^ sid16;
                if (xorVal < 16)
                    pidVal ^= 0x10000000;
                break;
            }
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

    // Refresh checksum
    pk.refreshChecksum();

    return pk;
}

// --- File scanner ---

std::vector<WCInfo> scanWondercards(const std::string& basePath, GameType game) {
    std::vector<WCInfo> results;

    // Build path: basePath + "wondercards/" + bankFolderName + "/"
    std::string wcDir = basePath + "wondercards/" + bankFolderNameOf(game) + "/";

    DIR* dir = opendir(wcDir.c_str());
    if (!dir) return results;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        // Filter .wc9 files
        if (name.size() < 5) continue;
        std::string ext = name.substr(name.size() - 4);
        // Case-insensitive compare
        for (auto& c : ext) c = static_cast<char>(std::tolower(c));
        if (ext != ".wc9") continue;

        std::string fullPath = wcDir + name;

        // Quick-parse the file for summary info
        WC9 wc;
        if (!wc.loadFromFile(fullPath) || !wc.isPokemon()) {
            results.push_back({ name, fullPath, 0, 0, 0, false, false, false });
            continue;
        }

        uint16_t specNat = SpeciesConverter::getNational9(wc.speciesInternal());

        // Determine shiny from PIDType
        bool isShiny = false;
        uint8_t pType = wc.pidType();
        if (pType == 2 || pType == 3) {
            isShiny = true;
        } else if (pType == 4) {
            // Fixed PID — check if shiny against its own ID32
            uint32_t wid = wc.id32();
            uint32_t wpid = wc.pid();
            if (wid != 0) {
                uint16_t t = static_cast<uint16_t>(wid & 0xFFFF);
                uint16_t s = static_cast<uint16_t>(wid >> 16);
                uint32_t xor_val = (wpid >> 16) ^ (wpid & 0xFFFF) ^ t ^ s;
                isShiny = xor_val < 16;
            }
        }

        // Check hasOT for English (language 2) as default check
        bool hasOT = wc.getHasOT(getLanguageIndex(2));

        results.push_back({
            name,
            fullPath,
            wc.cardID(),
            specNat,
            wc.level(),
            isShiny,
            hasOT,
            true
        });
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
