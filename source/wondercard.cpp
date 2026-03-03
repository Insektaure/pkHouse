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
    static bool seeded = false;
    if (!seeded) { std::srand(static_cast<unsigned>(std::time(nullptr))); seeded = true; }

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
    // AbilityType: 0/1/2=fixed slot, 3=random 0/1, 4=random 0/1/H
    int abIdx = abilityType();
    if (abIdx == 3) abIdx = std::rand() % 2;
    else if (abIdx == 4) abIdx = std::rand() % 3;
    else if (abIdx > 4) abIdx = 0;
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
        int byteIdx = 0x34 + ribbon / 8;
        int bitIdx = ribbon % 8;
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

    // OTFriendship (0x112) and CurrentFriendship
    // For eggs, use HatchCycles instead of BaseFriendship
    uint8_t friendship;
    if (isEggWC) {
        friendship = PersonalSV::getHatchCycles(specNat, formVal);
    } else {
        friendship = PersonalSV::getBaseFriendship(specNat, formVal);
    }
    pk.data[0x112] = friendship;
    if (hasOT)
        pk.data[0xC8] = friendship; // HT_Friendship (CurrentHandler=1)

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
