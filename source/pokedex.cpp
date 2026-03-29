// Pokedex registration for all supported save formats.
// Ported from PKHeX.Core: Zukan9a, Zukan9Paldea, Zukan9Kitakami,
//   Zukan8 (SwSh), Zukan8b (BDSP), Zukan7b (LGPE), SAV3 (FRLG).

#include "pokedex.h"
#include "save_file.h"
#include "pokemon.h"
#include "species_converter.h"
#include "binary_io.h"
#include "personal_za.h"
#include "personal_sv.h"
#include "personal_swsh.h"
#include "personal_bdsp.h"
#include <cstring>
#include <algorithm>

namespace Pokedex {

// ============================================================
//  Common helpers
// ============================================================

// Language ID → dex bit index.  Skips lang 0 and lang 6.
// Maps: 1→0, 2→1, 3→2, 4→3, 5→4, 7→5, 8→6, 9→7, 10→8
static int getDexLangFlag(int lang) {
    if (lang > 10 || lang == 6 || lang <= 0) return 0;
    return (lang >= 7) ? lang - 2 : lang - 1;
}

static uint16_t langBitMask(int lang) {
    return static_cast<uint16_t>(1u << getDexLangFlag(lang));
}

// Check if species+form exists in a game's PersonalTable.
// Uses IS_PRESENT[] flag (byte 0x1C in PersonalInfo), matching PKHeX IsPresentInGame.
static bool isPresentZA(uint16_t species, uint8_t form) {
    if (species == 0 || species >= PERSONAL_ZA_COUNT) return false;
    return PersonalZA::IS_PRESENT[PersonalZA::getEntryIndex(species, form)] != 0;
}

static bool isPresentSV(uint16_t species, uint8_t form) {
    if (species == 0 || species >= PERSONAL_SV_COUNT) return false;
    return PersonalSV::IS_PRESENT[PersonalSV::getEntryIndex(species, form)] != 0;
}

// ============================================================
//  ZA — Zukan9a  (PokeDexEntry9a, 0x84 bytes per species)
//  SCBlock key: 0x2D87BE5C
//  Index: SpeciesConverter::getInternal9(nationalId)
// ============================================================

namespace {

// PokeDexEntry9a field offsets
constexpr int ZA_ENTRY_SIZE          = 0x84;
constexpr int ZA_FORMS_CAUGHT        = 0x00; // u32 bitflags
constexpr int ZA_FORMS_SEEN          = 0x04; // u32 bitflags
constexpr int ZA_LANGUAGE            = 0x08; // u16 bitflags
constexpr int ZA_IS_NEW              = 0x0A; // u8
constexpr int ZA_GENDER_SEEN         = 0x0B; // u8 (bit0=M, bit1=F, bit2=genderless)
constexpr int ZA_SHINY_SEEN          = 0x0C; // u32 bitflags (per form)
constexpr int ZA_ALPHA_FLAG          = 0x11; // u8
constexpr int ZA_DISPLAY_FORM        = 0x5A; // u8
constexpr int ZA_DISPLAY_GENDER      = 0x5B; // u8 (DisplayGender9a enum)
constexpr int ZA_DISPLAY_SHINY       = 0x5C; // u8

// DisplayGender9a enum values
constexpr uint8_t DG9A_GENDERLESS          = 0;
constexpr uint8_t DG9A_MALE                = 1;
constexpr uint8_t DG9A_FEMALE              = 2;
constexpr uint8_t DG9A_GENDERED_NO_DIFF    = 3;

// Species with visible gender differences in ZA dex display.
// Ported from PokeDexEntry9a.cs BiGender + BiGenderDLC.
static bool isBiGenderZA(uint16_t species) {
    switch (species) {
        // Base game BiGender
        case 3: case 25: case 26: case 64: case 65: case 123: case 129:
        case 130: case 133: case 154: case 208: case 212: case 214: case 229:
        case 252: case 253: case 254: case 255: case 256: case 257: case 258:
        case 259: case 260: case 307: case 308: case 315: case 322: case 323:
        case 407: case 443: case 444: case 445: case 449: case 450: case 459:
        case 460: case 485: case 668:
        // BiGenderDLC
        case 41: case 42:             // Zubat, Golbat
        case 396: case 397: case 398: // Starly, Staravia, Staraptor
            return true;
        default:
            return false;
    }
}

// GetFormExtraFlags: auto-set additional form seen/caught flags on first registration.
// Ported from Zukan9a.GetFormExtraFlags.
static uint32_t getFormExtraFlags(uint16_t species) {
    switch (species) {
        case 676: return 1;     // Furfrou: base form
        case 681: return 3;     // Aegislash: both forms
        case 479: return 0x3F;  // Rotom: all 6 forms
        case 648: return 3;     // Meloetta
        case 649: return 0x1F;  // Genesect: 5 forms
        case 778: return 3;     // Mimikyu
        case 877: return 3;     // Morpeko
        case 720: return 3;     // Hoopa
        case 718: return 0x10;  // Zygarde: Complete form
        default:  return 0;
    }
}

// GetFormExtraFlagsShinySeen: shiny-specific multi-form flags.
// Ported from Zukan9a.GetFormExtraFlagsShinySeen.
static uint32_t getFormExtraFlagsShinySeen(uint16_t species, uint8_t form) {
    switch (species) {
        case 676: return 1;        // Furfrou
        case 681: return 3;        // Aegislash
        case 664: case 665: return 0xFFFFF; // Scatterbug/Spewpa: all 20 Vivillon forms
        case 479: return 0x3F;     // Rotom
        case 648: return 3;        // Meloetta
        case 649: return 0x1F;     // Genesect
        case 778: return 3;        // Mimikyu
        case 877: return 3;        // Morpeko
        case 720: return 3;        // Hoopa
        case 978:                  // Tatsugiri
            if (form == 0) return 8;
            if (form == 1) return 16;
            if (form == 2) return 32;
            return 0;
        case 801:                  // Magearna
            if (form == 0) return 4;
            if (form == 1) return 8;
            return 0;
        case 718: return 0x3F;     // Zygarde: all forms
        case 658: return (form != 2) ? 0xB : 0u; // Greninja
        case 670: return (form == 5) ? 0x20 : 0u; // Floette
        default: return 0;
    }
}

static uint8_t calcDisplayGenderZA(uint8_t gender, uint16_t species, uint8_t form) {
    if (gender == 2) return DG9A_GENDERLESS;

    uint8_t gr = PersonalZA::getGenderRatio(species, form);
    if (gr == 255)  return DG9A_GENDERLESS;   // genderless species
    if (gr == 0)    return DG9A_MALE;          // male-only
    if (gr == 254)  return DG9A_FEMALE;        // female-only

    // Dual-gender: check if species has visible gender differences
    if (isBiGenderZA(species))
        return (gender == 1) ? DG9A_FEMALE : DG9A_MALE;

    return DG9A_GENDERED_NO_DIFF;
}

} // anon

static void registerZA(SaveFile& save, const Pokemon& pkm) {
    SCBlock* block = save.findBlock(0x2D87BE5C);
    if (!block || block->data.empty()) return;

    uint16_t species = pkm.species();
    uint8_t  form    = pkm.form();
    uint8_t  gender  = pkm.gender();

    if (!isPresentZA(species, form))
        return;

    uint16_t internal = SpeciesConverter::getInternal9(species);
    size_t offset = static_cast<size_t>(internal) * ZA_ENTRY_SIZE;
    if (offset + ZA_ENTRY_SIZE > block->data.size()) return;

    uint8_t* e = block->data.data() + offset;

    bool wasCaught = readU32LE(e + ZA_FORMS_CAUGHT) != 0;

    // Set form caught + seen
    writeU32LE(e + ZA_FORMS_CAUGHT, readU32LE(e + ZA_FORMS_CAUGHT) | (1u << form));
    writeU32LE(e + ZA_FORMS_SEEN,   readU32LE(e + ZA_FORMS_SEEN)   | (1u << form));

    // Set gender seen (clamp to 0-2)
    uint8_t gc = std::min(gender, static_cast<uint8_t>(2));
    e[ZA_GENDER_SEEN] |= static_cast<uint8_t>(1u << gc);


    // Set language
    writeU16LE(e + ZA_LANGUAGE, readU16LE(e + ZA_LANGUAGE) | langBitMask(pkm.language()));

    // Set shiny
    bool isShiny = pkm.isShiny();
    if (isShiny) {
        writeU32LE(e + ZA_SHINY_SEEN, readU32LE(e + ZA_SHINY_SEEN) | (1u << form));
        // Shiny-specific multi-form flags
        uint32_t shinyExtra = getFormExtraFlagsShinySeen(species, form);
        if (shinyExtra)
            writeU32LE(e + ZA_SHINY_SEEN, readU32LE(e + ZA_SHINY_SEEN) | shinyExtra);
    }

    // Set alpha
    if (pkm.isAlpha())
        e[ZA_ALPHA_FLAG] = 1;

    // First registration: set display data + IsNew + form extra flags
    if (!wasCaught) {
        e[ZA_DISPLAY_FORM]   = form;
        e[ZA_DISPLAY_GENDER] = calcDisplayGenderZA(gender, species, form);
        e[ZA_DISPLAY_SHINY]  = isShiny ? 1 : 0;
        e[ZA_IS_NEW]         = 1;

        // Auto-set additional form flags for multi-form species
        uint32_t formExtra = getFormExtraFlags(species);
        if (formExtra) {
            writeU32LE(e + ZA_FORMS_SEEN,   readU32LE(e + ZA_FORMS_SEEN)   | formExtra);
            writeU32LE(e + ZA_FORMS_CAUGHT, readU32LE(e + ZA_FORMS_CAUGHT) | formExtra);
        }
    }
}

// ============================================================
//  SV — Zukan9Kitakami  (PokeDexEntry9Kitakami, 0x20 per entry)
//  SCBlock key: 0xF5D7C0E2
//  Since pkHouse only supports DLC saves, we use Kitakami exclusively.
//  (Starting in SV 2.0.1, the Paldea block is dummied out.)
// ============================================================

namespace {

constexpr int SV_KITA_ENTRY_SIZE     = 0x20;
constexpr int SVK_FORMS_OBTAINED     = 0x00; // u32
constexpr int SVK_FORMS_SEEN         = 0x04; // u32
constexpr int SVK_FORMS_HEARD        = 0x08; // u32
constexpr int SVK_FORMS_CHECKED      = 0x0C; // u32
constexpr int SVK_LANGUAGE           = 0x10; // u16
constexpr int SVK_GENDER_SEEN        = 0x12; // u8
constexpr int SVK_SHINY_SEEN         = 0x13; // u8 (bit0=regular, bit1=shiny)
// Display regions: Paldea (0x14), Kitakami (0x18), Blueberry (0x1C)
// Each: form(u8), gender(u8), shiny(u8), pad(u8)
constexpr int SVK_DISP_PALDEA       = 0x14;
constexpr int SVK_DISP_KITAKAMI     = 0x18;
constexpr int SVK_DISP_BLUEBERRY    = 0x1C;

// DexPaldea/DexKitakami/DexBlueberry per species (from PersonalInfo9SV).
// Used for display slot eligibility and UpdateAdjacent.
// Extracted from PKHeX PersonalTable9SV binary (0x50 bytes/entry).
static constexpr uint16_t SV_DEX_PALDEA[1424] = {
#include "sv_dex_paldea.inc"
};
static constexpr uint8_t SV_DEX_KITAKAMI[1424] = {
#include "sv_dex_kitakami.inc"
};
static constexpr uint8_t SV_DEX_BLUEBERRY[1424] = {
#include "sv_dex_blueberry.inc"
};

// UpdateAdjacent: mark neighbor dex entries as "heard".
// Ported from Zukan9Kitakami.UpdateAdjacent + MarkAsKnown1.
static void updateAdjacent(SCBlock* block, uint16_t species) {
    // Get dex index for this species (check all 3 groups)
    if (species >= 1424) return;

    uint8_t group = 0;
    uint16_t index = 0;
    if (SV_DEX_PALDEA[species] != 0) { group = 1; index = SV_DEX_PALDEA[species]; }
    else if (SV_DEX_KITAKAMI[species] != 0) { group = 2; index = SV_DEX_KITAKAMI[species]; }
    else if (SV_DEX_BLUEBERRY[species] != 0) { group = 3; index = SV_DEX_BLUEBERRY[species]; }
    if (index == 0) return;

    // Mark index-1 and index+1 as "heard" (skip starters: group==1 && index<=9)
    for (int delta : {-1, 1}) {
        uint16_t neighborIdx = index + delta;
        if (neighborIdx == 0) continue;
        if (group == 1 && neighborIdx <= 9) continue; // hide starter evolutions

        // Find species with this dex index in same group
        uint16_t neighborSpecies = 0;
        for (uint16_t s = 1; s < 1424; s++) {
            bool match = false;
            if (group == 1 && SV_DEX_PALDEA[s] == neighborIdx) match = true;
            else if (group == 2 && SV_DEX_KITAKAMI[s] == neighborIdx) match = true;
            else if (group == 3 && SV_DEX_BLUEBERRY[s] == neighborIdx) match = true;
            if (match) { neighborSpecies = s; break; }
        }
        if (neighborSpecies == 0) continue;

        // Get this neighbor's entry and set heard flag for form 0
        uint16_t nInternal = SpeciesConverter::getInternal9(neighborSpecies);
        size_t nOfs = static_cast<size_t>(nInternal) * SV_KITA_ENTRY_SIZE;
        if (nOfs + SV_KITA_ENTRY_SIZE > block->data.size()) continue;

        uint8_t* ne = block->data.data() + nOfs;
        // Only set if not already known (forms_heard == 0)
        if (readU32LE(ne + SVK_FORMS_HEARD) == 0)
            writeU32LE(ne + SVK_FORMS_HEARD, readU32LE(ne + SVK_FORMS_HEARD) | 1u); // form 0
    }
}

} // anon

static void registerSVKitakami(SaveFile& save, const Pokemon& pkm) {
    SCBlock* block = save.findBlock(0xF5D7C0E2);
    if (!block || block->data.empty()) return;

    uint16_t species = pkm.species();
    uint8_t  form    = pkm.form();
    uint8_t  gender  = pkm.gender();

    if (!isPresentSV(species, form))
        return;

    uint16_t internal = SpeciesConverter::getInternal9(species);
    size_t offset = static_cast<size_t>(internal) * SV_KITA_ENTRY_SIZE;
    if (offset + SV_KITA_ENTRY_SIZE > block->data.size()) return;

    uint8_t* e = block->data.data() + offset;

    bool wasObtained = readU32LE(e + SVK_FORMS_OBTAINED) != 0;

    // Set form obtained + seen + heard
    writeU32LE(e + SVK_FORMS_OBTAINED, readU32LE(e + SVK_FORMS_OBTAINED) | (1u << form));
    writeU32LE(e + SVK_FORMS_SEEN,     readU32LE(e + SVK_FORMS_SEEN)     | (1u << form));
    writeU32LE(e + SVK_FORMS_HEARD,    readU32LE(e + SVK_FORMS_HEARD)    | (1u << form));

    // Set gender seen
    uint8_t gc = std::min(gender, static_cast<uint8_t>(2));
    e[SVK_GENDER_SEEN] |= static_cast<uint8_t>(1u << gc);

    // Set model seen (bit 0 = regular always, bit 1 = shiny)
    e[SVK_SHINY_SEEN] |= 0x01; // regular model seen
    if (pkm.isShiny())
        e[SVK_SHINY_SEEN] |= 0x02;

    // Set language — both Pokemon language and save file language (PKHeX parity)
    writeU16LE(e + SVK_LANGUAGE, readU16LE(e + SVK_LANGUAGE) | langBitMask(pkm.language()));
    uint8_t saveLang = save.saveLanguage();
    if (saveLang != 0 && saveLang != pkm.language())
        writeU16LE(e + SVK_LANGUAGE, readU16LE(e + SVK_LANGUAGE) | langBitMask(saveLang));

    // First registration: set display with dex eligibility check
    if (!wasObtained) {
        uint8_t dg = gender; // 0=M, 1=F, 2=genderless
        uint8_t ds = pkm.isShiny() ? 1 : 0;

        // Set display for each region only if slot is empty AND species is in that dex
        if (e[SVK_DISP_PALDEA] == 0 && e[SVK_DISP_PALDEA + 1] == 0 &&
            species < 1424 && SV_DEX_PALDEA[species] != 0) {
            e[SVK_DISP_PALDEA + 0] = form;
            e[SVK_DISP_PALDEA + 1] = dg;
            e[SVK_DISP_PALDEA + 2] = ds;
        }
        if (e[SVK_DISP_KITAKAMI] == 0 && e[SVK_DISP_KITAKAMI + 1] == 0 &&
            species < 1424 && SV_DEX_KITAKAMI[species] != 0) {
            e[SVK_DISP_KITAKAMI + 0] = form;
            e[SVK_DISP_KITAKAMI + 1] = dg;
            e[SVK_DISP_KITAKAMI + 2] = ds;
        }
        if (e[SVK_DISP_BLUEBERRY] == 0 && e[SVK_DISP_BLUEBERRY + 1] == 0 &&
            species < 1424 && SV_DEX_BLUEBERRY[species] != 0) {
            e[SVK_DISP_BLUEBERRY + 0] = form;
            e[SVK_DISP_BLUEBERRY + 1] = dg;
            e[SVK_DISP_BLUEBERRY + 2] = ds;
        }
    }

    // Update adjacent entries as "heard" (neighbor discovery)
    updateAdjacent(block, species);
}

// ============================================================
//  SwSh — Zukan8  (0x30-byte entries, dex-index based)
//  SCBlock key: 0x4716c404 (Galar)
//  Also: 0x3F936BA9 (Armor), 0x3C9366F0 (Crown)
// ============================================================

namespace {

// Dex index tables extracted from PKHeX PersonalTable8SWSH binary.
// Maps entry index (species + form variants) → dex number (1-based, 0 = not in dex).
// PersonalInfo8SWSH: PokeDexIndex at 0x5C, ArmorDexIndex at 0xAC, CrownDexIndex at 0xAE.

// Galar Pokedex index per species (1192 entries)
static constexpr uint16_t SWSH_DEX_GALAR[1192] = {
    0,0,0,0,378,379,380,0,0,0,13,14,15,0,0,0,0,0,0,0,
    0,0,0,0,0,194,195,0,0,0,0,0,0,0,0,255,256,68,69,0,
    0,0,0,55,56,57,0,0,0,0,164,165,182,184,0,0,0,0,70,71,
    0,0,0,0,0,0,138,139,140,0,0,0,0,0,0,0,0,333,334,0,
    0,0,0,218,0,0,0,0,0,0,150,151,141,142,143,178,0,0,98,99,
    0,0,0,0,0,0,108,109,0,250,251,264,265,0,0,0,0,0,146,147,
    0,0,365,0,0,0,0,0,0,144,145,361,373,196,197,198,199,0,0,0,
    0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,19,20,0,0,0,0,0,220,221,193,254,0,257,258,92,93,0,
    0,0,58,0,0,253,0,0,0,0,0,0,0,0,100,101,200,201,0,0,
    0,0,217,0,0,0,0,0,179,0,0,304,0,227,0,292,0,0,0,0,
    75,76,236,148,149,78,355,0,0,0,0,0,0,0,0,0,107,110,0,0,
    0,0,0,0,0,0,383,384,385,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,31,32,0,0,0,0,0,36,37,38,39,40,41,0,0,62,63,
    120,121,122,0,0,0,0,0,0,0,104,105,106,0,0,0,0,0,0,0,
    0,0,294,295,0,0,0,0,0,66,67,0,0,0,0,60,0,0,0,0,
    356,357,0,0,300,0,0,0,321,322,323,0,0,0,0,0,0,362,363,228,
    229,102,103,82,83,0,0,0,0,152,153,0,0,0,0,135,136,0,0,0,
    216,79,80,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,59,61,0,0,0,0,0,0,0,116,117,0,0,0,
    128,129,230,231,0,124,125,0,0,0,0,0,0,0,130,131,118,119,252,364,
    0,0,0,0,0,0,260,298,299,314,315,285,286,222,223,0,0,0,354,96,
    97,293,0,0,266,0,0,0,259,0,202,203,0,77,0,123,0,137,81,372,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,44,45,0,0,0,0,0,0,90,91,26,
    27,28,0,0,168,169,170,174,175,166,167,0,171,172,173,132,133,134,248,249,
    0,0,0,0,0,0,262,263,0,0,154,0,0,0,367,368,296,86,87,224,
    225,297,327,329,0,0,0,0,157,158,0,0,50,51,267,268,269,270,271,272,
    0,0,72,73,74,0,0,0,273,274,0,0,305,306,0,64,65,189,190,113,
    114,115,0,0,0,277,278,287,288,289,324,325,326,279,280,0,275,276,226,0,
    0,0,88,89,246,247,0,281,282,283,284,317,316,386,387,388,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,48,
    49,0,0,0,0,0,0,0,0,0,0,0,0,0,111,112,0,208,209,330,
    331,332,212,213,210,211,290,291,234,235,0,0,0,0,318,319,0,0,0,0,
    204,320,0,0,389,390,391,0,338,339,191,192,358,359,176,177,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,17,18,0,
    0,0,187,188,0,0,155,307,308,84,85,214,215,0,0,340,341,244,245,94,
    95,52,53,54,0,342,343,232,233,0,0,156,381,382,0,0,347,348,301,0,
    346,360,392,393,394,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,24,
    25,21,22,23,10,11,12,29,30,126,127,34,35,42,43,46,47,161,162,163,
    205,206,207,312,313,309,180,181,310,311,159,160,351,352,335,336,241,242,243,238,
    239,240,33,183,237,219,366,328,185,186,345,353,349,350,369,370,337,344,302,303,
    374,375,376,377,371,395,396,397,398,399,400,0,0,0,0,0,0,0,0,0,
    0,194,194,194,194,194,194,194,0,194,195,0,0,68,69,164,165,182,182,184,
    0,0,0,333,334,0,0,0,218,0,0,0,0,251,365,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,236,31,32,0,0,0,0,0,0,0,0,0,0,0,
    0,0,129,230,231,372,372,372,372,372,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,154,367,368,368,368,327,0,0,0,0,0,
    0,226,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,209,332,191,191,191,192,
    192,192,0,0,0,0,0,0,0,0,0,0,0,0,155,382,382,382,382,382,
    382,382,382,382,382,382,382,382,382,382,382,382,0,0,0,0,0,0,0,0,
    0,0,0,0,0,301,0,0,0,309,309,311,335,336,186,186,186,186,186,186,
    186,186,370,337,344,398,399,400,0,0,0,0,
};

constexpr int SWSH_ENTRY_SIZE = 0x30;

// Zukan8 entry layout:
//   0x00-0x07: u64 SeenRegion[0] — non-shiny male (bit N = form N, bit 63 = Gmax)
//   0x08-0x0F: u64 SeenRegion[1] — non-shiny female
//   0x10-0x17: u64 SeenRegion[2] — shiny male
//   0x18-0x1F: u64 SeenRegion[3] — shiny female
//   0x20-0x23: u32 CaughtInfo (packed bitfield)
//   0x24-0x27: u32 BattledCount
//   0x28-0x2B: u32 Unk1
//   0x2C-0x2F: u32 Unk2

// CaughtInfo bit layout:
//   bit 0:      Owned
//   bit 1:      OwnedGigantamax
//   bits 2-14:  LanguagesObtained (13 bits)
//   bits 15-27: DisplayFormID (13 bits)
//   bit 28:     DisplayGigantamaxInstead
//   bits 29-30: DisplayGender (0=M, 1=F)
//   bit 31:     DisplayShiny

static uint64_t readU64LE(const uint8_t* p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    return v;
}
static void writeU64LE(uint8_t* p, uint64_t v) {
    std::memcpy(p, &v, 8);
}

// Armor DLC dex index (Isle of Armor, SCBlock 0x3F936BA9)
static constexpr uint16_t SWSH_DEX_ARMOR[1192] = {
    0,68,69,70,0,0,0,71,72,73,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,85,86,168,169,0,0,0,0,0,0,0,0,0,0,12,
    13,0,0,0,0,0,0,0,0,0,0,0,0,0,146,147,0,0,0,0,
    142,143,144,31,32,33,0,0,0,0,0,0,40,41,0,0,0,0,0,1,
    2,105,106,0,0,0,0,0,0,0,131,132,0,0,0,0,0,0,38,39,
    0,0,205,206,170,171,0,0,54,0,0,183,184,7,80,172,198,199,94,95,
    98,99,0,118,0,0,0,120,116,42,43,0,207,0,0,0,0,208,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,188,189,84,0,11,0,0,0,0,0,
    0,0,0,140,141,0,145,0,0,0,0,0,0,0,58,59,0,0,0,3,
    0,0,0,0,0,0,52,0,0,0,0,0,119,0,121,0,0,0,0,0,
    0,0,0,44,45,0,47,153,0,0,200,0,0,209,0,0,0,0,0,0,
    0,117,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,48,49,
    34,35,36,0,0,0,0,0,0,0,0,0,0,148,149,150,0,0,139,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,111,112,
    190,191,0,0,173,0,0,0,0,0,0,0,0,0,0,0,0,0,0,137,
    138,91,92,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,25,26,27,0,0,0,0,0,0,0,0,0,203,204,0,0,0,
    0,0,0,0,0,135,136,4,5,0,0,0,0,0,0,0,0,0,0,0,
    6,0,0,0,0,0,0,0,0,0,0,50,51,82,83,0,0,0,46,0,
    0,0,107,55,185,81,0,0,0,0,0,0,0,0,210,37,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,113,114,115,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,154,155,156,151,152,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,74,75,76,0,0,201,202,0,176,177,178,0,0,0,122,123,161,
    162,0,0,0,0,0,0,0,0,0,87,88,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,102,66,67,77,78,192,193,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,65,0,163,
    164,63,0,0,29,30,53,179,180,181,182,0,0,0,0,0,186,187,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,22,23,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,108,109,0,0,194,195,196,197,0,0,0,0,0,0,
    0,0,103,0,60,61,62,28,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,157,158,110,127,128,0,0,0,0,17,18,0,0,159,160,0,
    0,0,0,0,79,89,90,124,125,133,134,0,0,0,0,0,0,0,0,0,
    0,0,165,166,167,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,
    10,0,0,0,14,15,16,0,0,0,0,0,0,56,57,0,0,0,0,0,
    19,20,21,174,175,93,96,97,0,0,0,0,129,130,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,126,0,0,0,0,0,104,0,0,
    0,0,0,0,0,0,0,0,0,0,0,100,101,211,0,0,0,0,0,0,
    0,85,85,85,85,85,85,85,0,85,86,168,169,0,0,0,0,0,0,0,
    0,0,0,0,0,1,0,2,0,0,0,206,171,0,0,0,0,0,3,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,157,158,158,110,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,93,93,0,0,0,0,0,0,0,0,0,
    0,0,0,0,104,0,0,0,101,211,0,0,
};

// Crown Tundra DLC dex index (SCBlock 0x3C9366F0)
static constexpr uint16_t SWSH_DEX_CROWN[1192] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,65,66,67,68,69,70,44,45,0,0,0,
    0,144,145,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,105,106,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,11,0,14,16,19,0,0,62,63,190,0,74,75,76,77,0,123,124,
    125,126,127,173,202,203,204,194,195,196,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,146,0,0,0,43,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,79,78,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,170,0,28,0,0,0,0,
    7,8,0,0,0,120,0,0,0,0,0,0,0,0,0,0,0,0,13,15,
    18,0,0,0,0,0,139,140,141,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,71,72,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,174,175,191,192,193,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,35,36,0,0,0,0,60,
    61,0,0,151,152,183,184,185,186,188,189,0,0,0,0,0,0,0,0,107,
    0,25,26,159,160,161,0,0,0,187,0,113,114,115,129,130,131,197,198,199,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,87,88,0,10,
    0,0,47,116,117,118,172,134,135,0,0,0,0,0,0,0,0,0,0,31,
    32,29,0,0,0,0,17,20,0,0,81,80,0,9,0,0,0,0,27,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,21,57,58,59,0,0,0,0,0,
    0,0,0,0,0,0,168,169,0,0,64,0,0,0,103,104,0,0,0,0,
    0,0,0,0,147,148,149,150,0,0,0,0,0,0,51,52,53,54,55,56,
    0,0,22,23,24,0,0,0,95,96,0,0,0,0,0,93,94,179,180,0,
    0,0,0,0,0,0,0,48,49,50,0,0,0,121,122,30,97,98,0,0,
    0,119,153,154,0,0,0,0,0,0,0,102,101,136,137,138,0,0,205,206,
    207,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,83,84,85,86,
    82,0,0,128,0,0,0,0,33,34,0,0,142,143,181,182,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,91,92,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,0,
    0,162,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,
    6,163,164,165,0,0,0,0,0,166,167,3,4,0,0,155,156,176,177,178,
    0,0,0,0,0,0,0,0,0,0,99,100,0,0,132,133,40,41,42,37,
    38,39,73,0,0,0,12,0,0,0,0,158,1,2,89,90,171,157,108,109,
    0,0,0,0,0,110,111,112,0,0,0,0,0,0,200,201,208,209,210,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,105,106,0,0,0,0,0,0,0,0,0,11,202,203,204,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,71,72,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,64,103,104,104,104,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,46,0,0,0,0,0,0,132,133,0,0,0,0,0,0,
    0,0,90,171,157,0,0,0,0,0,210,210,
};

// SCBlock keys for each SwSh dex
constexpr uint32_t KZUKAN_GALAR = 0x4716c404;
constexpr uint32_t KZUKAN_ARMOR = 0x3F936BA9;
constexpr uint32_t KZUKAN_CROWN = 0x3C9366F0;

// Dex lookup: returns (dexIndex, blockKey) for the first matching dex.
// Priority: Galar > Armor > Crown (same as PKHeX Zukan8.DexLookup).
struct SwShDexResult { uint16_t idx; uint32_t blockKey; };

static SwShDexResult getSwShDex(uint16_t species) {
    if (species >= 1192) return {0, 0};
    if (SWSH_DEX_GALAR[species] != 0) return {SWSH_DEX_GALAR[species], KZUKAN_GALAR};
    if (SWSH_DEX_ARMOR[species] != 0) return {SWSH_DEX_ARMOR[species], KZUKAN_ARMOR};
    if (SWSH_DEX_CROWN[species] != 0) return {SWSH_DEX_CROWN[species], KZUKAN_CROWN};
    return {0, 0};
}

} // anon

static void registerSwSh(SaveFile& save, const Pokemon& pkm) {
    uint16_t species = pkm.species();
    uint8_t  form    = pkm.form();
    uint8_t  gender  = pkm.gender();

    // Find which dex this species belongs to (matches PKHeX GetEntry guard)
    auto [dexIdx, blockKey] = getSwShDex(species);
    if (dexIdx == 0) return;

    SCBlock* block = save.findBlock(blockKey);
    if (!block || block->data.empty()) return;

    size_t entryOfs = static_cast<size_t>(dexIdx - 1) * SWSH_ENTRY_SIZE;
    if (entryOfs + SWSH_ENTRY_SIZE > block->data.size()) return;

    uint8_t* e = block->data.data() + entryOfs;

    // Determine which seen region to set:
    //   region 0 = non-shiny male, 1 = non-shiny female
    //   region 2 = shiny male,     3 = shiny female
    int region = 0;
    if (gender == 1) region += 1;
    if (pkm.isShiny()) region += 2;

    // Alcremie: combined form = baseForm * 7 + decoration (FormArgument low byte)
    if (species == 869) { // Alcremie
        form = static_cast<uint8_t>(form * 7 + (pkm.formArgument() & 0xFF));
    }
    // Eternatus form 1 → form 0 + Gigantamax bit 63
    else if (species == 890 && form == 1) { // Eternatus Eternamax
        form = 0;
        // Set bit 63 (Gmax flag) in the seen region
        uint64_t seenGmax = readU64LE(e + region * 8);
        seenGmax |= (1ULL << 63);
        writeU64LE(e + region * 8, seenGmax);
        if (pkm.isShiny()) {
            uint64_t seenGmaxNS = readU64LE(e + (region - 2) * 8);
            seenGmaxNS |= (1ULL << 63);
            writeU64LE(e + (region - 2) * 8, seenGmaxNS);
        }
    }

    // Set form bit in the appropriate seen QWORD
    uint8_t formBit = (form < 63) ? form : 0;
    uint64_t seen = readU64LE(e + region * 8);
    seen |= (1ULL << formBit);
    writeU64LE(e + region * 8, seen);

    // Also set the non-shiny region for the same gender (always mark regular as seen)
    if (pkm.isShiny()) {
        uint64_t seenNonShiny = readU64LE(e + (region - 2) * 8);
        seenNonShiny |= (1ULL << formBit);
        writeU64LE(e + (region - 2) * 8, seenNonShiny);
    }

    // Read/modify caught info
    uint32_t caught = readU32LE(e + 0x20);
    bool wasOwned = (caught & 1) != 0;

    // Set owned bit
    caught |= 1;

    // Set Gigantamax owned bit (bit 1) if Pokemon can Gigantamax
    if (pkm.canGigantamax())
        caught |= (1u << 1);

    // Set language (bits 2-14)
    int langBit = getDexLangFlag(pkm.language());
    caught |= (1u << (langBit + 2));

    // Set battled count to 1 if first time seeing
    if (!wasOwned) {
        uint32_t battled = readU32LE(e + 0x24);
        if (battled == 0) writeU32LE(e + 0x24, 1);
    }

    // First registration: set display info
    if (!wasOwned) {
        // Clear display form (bits 15-27) and set to current form
        caught &= ~(0x1FFFu << 15);
        caught |= (static_cast<uint32_t>(form & 0x1FFF) << 15);

        // Set display gender (bits 29-30)
        caught &= ~(3u << 29);
        if (gender == 1) caught |= (1u << 29);

        // Set display shiny (bit 31)
        if (pkm.isShiny()) caught |= (1u << 31);
    }

    writeU32LE(e + 0x20, caught);
}

// ============================================================
//  BDSP — Zukan8b  (flat binary at save offset 0x7A328, size 0x30B8)
//  Per-species state + gender/shiny flags + language
// ============================================================

namespace {

// Zukan8b layout: 493 species max (national dex 1-493)
// Offsets within the 0x30B8-byte block:
//   0x0000: State[493]        — u32 per species (0=none, 1=heard, 2=seen, 3=caught)
//   0x07B4: MaleShiny[493]    — u32 per species (bool)
//   0x0F68: FemaleShiny[493]  — u32 per species (bool)
//   0x171C: Male[493]         — u32 per species (bool)
//   0x1ED0: Female[493]       — u32 per species (bool)
//   0x2684: Language[493]     — u32 per species (bitflags)
//   0x2E38: FormFlags...      — per-species form tracking (variable)
constexpr size_t BDSP_ZUKAN_OFFSET    = 0x7A328;
constexpr size_t BDSP_ZUKAN_SIZE      = 0x30B8;
constexpr int    BDSP_MAX_SPECIES     = 493;
constexpr size_t BDSP_STATE_BASE      = 0x0000;
constexpr size_t BDSP_MALE_SHINY_BASE = 0x07B4;
constexpr size_t BDSP_FEM_SHINY_BASE  = 0x0F68;
constexpr size_t BDSP_MALE_BASE       = 0x171C;
constexpr size_t BDSP_FEMALE_BASE     = 0x1ED0;
constexpr size_t BDSP_LANG_BASE       = 0x2684;

} // anon

static void registerBDSP(SaveFile& save, const Pokemon& pkm) {
    uint8_t* raw = save.rawData();
    size_t rawSize = save.rawDataSize();

    if (rawSize < BDSP_ZUKAN_OFFSET + BDSP_ZUKAN_SIZE) return;

    uint8_t* z = raw + BDSP_ZUKAN_OFFSET;

    uint16_t species = pkm.species();
    if (species == 0 || species > BDSP_MAX_SPECIES) return;

    // Check if present in BDSP
    if (species < PERSONAL_BDSP_COUNT &&
        PersonalBDSP::FORM_COUNT[species] == 0) return;

    uint8_t gender = pkm.gender();
    size_t idx = static_cast<size_t>(species - 1) * 4;

    // State → Caught (3) — never downgrade
    uint32_t state = readU32LE(z + BDSP_STATE_BASE + idx);
    if (state < 3)
        writeU32LE(z + BDSP_STATE_BASE + idx, 3);

    // Set gender flags
    if (gender == 0 || gender == 2) { // Male or genderless
        writeU32LE(z + BDSP_MALE_BASE + idx, 1);
        if (pkm.isShiny())
            writeU32LE(z + BDSP_MALE_SHINY_BASE + idx, 1);
    }
    if (gender == 1 || gender == 2) { // Female or genderless
        writeU32LE(z + BDSP_FEMALE_BASE + idx, 1);
        if (pkm.isShiny())
            writeU32LE(z + BDSP_FEM_SHINY_BASE + idx, 1);
    }

    // Set language (bitfield in u32)
    uint32_t langFlags = readU32LE(z + BDSP_LANG_BASE + idx);
    int langBit = getDexLangFlag(pkm.language());
    langFlags |= (1u << langBit);
    writeU32LE(z + BDSP_LANG_BASE + idx, langFlags);

    // Set form flags for species with alternate forms.
    // Layout: normal flags at formOfs, shiny flags at formOfs + formSize.
    // Each form flag = u32 (4-byte aligned bool).
    uint8_t form = pkm.form();
    struct BDSPFormInfo { uint16_t species; size_t offset; uint8_t count; };
    static constexpr BDSPFormInfo BDSP_FORMS[] = {
        {201, 0x2E38, 28}, // Unown
        {351, 0x2F18,  4}, // Castform
        {386, 0x2F38,  4}, // Deoxys
        {412, 0x2F58,  3}, // Burmy
        {413, 0x2F70,  3}, // Wormadam
        {414, 0x2F88,  3}, // Mothim
        {421, 0x2FA0,  2}, // Cherrim
        {422, 0x2FB0,  2}, // Shellos
        {423, 0x2FC0,  2}, // Gastrodon
        {479, 0x2FD0,  6}, // Rotom
        {487, 0x3000,  2}, // Giratina
        {492, 0x3010,  2}, // Shaymin
        {493, 0x3020, 18}, // Arceus
    };
    for (const auto& fi : BDSP_FORMS) {
        if (species != fi.species) continue;
        if (form >= fi.count) break;
        size_t formSize = static_cast<size_t>(fi.count) * 4;
        size_t normalOfs = fi.offset + static_cast<size_t>(form) * 4;
        size_t shinyOfs  = fi.offset + formSize + static_cast<size_t>(form) * 4;
        if (normalOfs + 4 <= BDSP_ZUKAN_SIZE)
            writeU32LE(z + normalOfs, 1);
        if (pkm.isShiny() && shinyOfs + 4 <= BDSP_ZUKAN_SIZE)
            writeU32LE(z + shinyOfs, 1);
        break;
    }
}

// ============================================================
//  LGPE — Zukan7b  (flat binary, block at save offset 0x02A00)
//  Inherits Zukan7 layout: caught/seen bitfields + language bitfields
// ============================================================

namespace {

// LGPE Pokedex block is at save offset 0x02A00, size 0x020E8.
// Within that block, the pokedex base is at offset 0x0550.
// Format from Zukan7.cs:
//   Layout: Magic(4) + Flags(4) + Misc(0x80) + Caught(0x68) + Seen(4×0x8C) + Display(4×0x8C) + ...
//   OFS_CAUGHT  = 0x88 (bitfield, 0x68 bytes = 832 bits)
//   OFS_SEEN    = 0xF0 (4 gender/shiny regions × 0x8C bytes each)
//   OFS_DISPLAY = 0x320 (4 display regions × 0x8C bytes each)
// Max species for LGPE = 809 (only 153 in-game: 1-151, 808, 809)
constexpr size_t LGPE_ZUKAN_BLOCK_OFFSET = 0x02A00;
constexpr size_t LGPE_POKEDEX_BASE       = 0x0550;  // within the block
constexpr size_t LGPE_CAUGHT_OFS         = 0x88;    // from PokeDex base
constexpr size_t LGPE_SEEN_OFS           = 0xF0;    // from PokeDex base
constexpr size_t LGPE_BIT_SEEN_SIZE      = 0x8C;    // bytes per seen region
constexpr size_t LGPE_DISPLAY_OFS        = 0xF0 + 4 * 0x8C; // = 0x320
constexpr size_t LGPE_LANG_OFS           = 0x550; // from PokeDex base (after caught+seen+display)
constexpr int    LGPE_LANG_COUNT         = 9;     // 9 language flags per species
constexpr size_t LGPE_LANG_BYTE_COUNT    = 920;   // total bytes for language bitfield
constexpr size_t LGPE_SIZE_OFS           = 0xF78; // from BLOCK start (not PokeDex base)
constexpr int    LGPE_SIZE_ENTRY_SIZE    = 6;     // bytes per size entry
constexpr int    LGPE_SIZE_ENTRY_COUNT   = 186;   // total species+form entries
constexpr int    LGPE_MAX_SPECIES        = 809;

} // anon

static void registerLGPE(SaveFile& save, const Pokemon& pkm) {
    uint8_t* raw = save.rawData();
    size_t rawSize = save.rawDataSize();

    size_t dexBase = LGPE_ZUKAN_BLOCK_OFFSET + LGPE_POKEDEX_BASE;
    // Ensure enough room for the display region end
    size_t requiredEnd = dexBase + LGPE_DISPLAY_OFS + 4 * LGPE_BIT_SEEN_SIZE;
    if (rawSize < requiredEnd) return;

    uint16_t species = pkm.species();
    if (species == 0 || species > LGPE_MAX_SPECIES) return;

    // Check if species is actually in LGPE (1-151, 808, 809)
    if (species > 151 && species != 808 && species != 809) return;

    uint8_t* pdx = raw + dexBase;

    int bit     = species - 1;
    int byteIdx = bit >> 3;
    int bitIdx  = bit & 7;
    uint8_t mask = static_cast<uint8_t>(1 << bitIdx);

    // Set caught
    pdx[LGPE_CAUGHT_OFS + byteIdx] |= mask;

    // Set seen — region = gender | (shiny << 1)
    // 0=Male/Normal, 1=Female/Normal, 2=Male/Shiny, 3=Female/Shiny
    uint8_t gender = pkm.gender();
    int shift = (gender & 1) | (pkm.isShiny() ? 2 : 0);
    pdx[LGPE_SEEN_OFS + shift * LGPE_BIT_SEEN_SIZE + byteIdx] |= mask;

    // Also mark non-shiny variant as seen (clear shiny bit to get same-gender non-shiny)
    if (pkm.isShiny())
        pdx[LGPE_SEEN_OFS + (shift & ~2) * LGPE_BIT_SEEN_SIZE + byteIdx] |= mask;

    // Set display flag for the same region (makes entry visible in dex)
    pdx[LGPE_DISPLAY_OFS + shift * LGPE_BIT_SEEN_SIZE + byteIdx] |= mask;
    if (pkm.isShiny())
        pdx[LGPE_DISPLAY_OFS + (shift & ~2) * LGPE_BIT_SEEN_SIZE + byteIdx] |= mask;

    // Set language flag (9 languages per species, compressed bitfield)
    int langFlag = getDexLangFlag(pkm.language());
    if (langFlag >= 0 && langFlag < LGPE_LANG_COUNT) {
        int lbit = (species - 1) * LGPE_LANG_COUNT + langFlag;
        if (lbit >= 0 && static_cast<size_t>(lbit >> 3) < LGPE_LANG_BYTE_COUNT)
            pdx[LGPE_LANG_OFS + (lbit >> 3)] |= static_cast<uint8_t>(1 << (lbit & 7));
    }

    // Size tracking: update min/max height and weight.
    // Size data is at LGPE_SIZE_OFS from the BLOCK start (not PokeDex base).
    // 4 groups: MinHeight, MaxHeight, MinWeight, MaxWeight (each 186 entries × 6 bytes).
    uint8_t* sizeBase = raw + LGPE_ZUKAN_BLOCK_OFFSET + LGPE_SIZE_OFS;
    size_t sizeEnd = LGPE_ZUKAN_BLOCK_OFFSET + LGPE_SIZE_OFS +
                     LGPE_SIZE_ENTRY_SIZE * LGPE_SIZE_ENTRY_COUNT * 4;
    int sizeIdx = (species <= 151) ? (species - 1) :
                  (species == 808) ? 151 : (species == 809) ? 152 : -1;
    if (sizeIdx >= 0 && sizeIdx < LGPE_SIZE_ENTRY_COUNT && sizeEnd <= rawSize) {
        uint8_t ht = pkm.heightScalar();
        uint8_t wt = pkm.weightScalar();
        // Each group offset: group * EntryCount * EntrySize
        auto sizeOfs = [&](int group) -> uint8_t* {
            return sizeBase + LGPE_SIZE_ENTRY_SIZE * (sizeIdx + group * LGPE_SIZE_ENTRY_COUNT);
        };
        // Entry layout: [0]=height, [1]=flag, [2]=weight, [3..5]=padding
        // Update MinHeight (group 0) — overwrite if new is smaller or unset (0)
        uint8_t* minH = sizeOfs(0);
        if (minH[0] == 0 || ht < minH[0]) { minH[0] = ht; minH[2] = wt; }
        // Update MaxHeight (group 1) — overwrite if new is larger
        uint8_t* maxH = sizeOfs(1);
        if (ht > maxH[0]) { maxH[0] = ht; maxH[2] = wt; }
        // Update MinWeight (group 2) — overwrite if new is smaller or unset (0)
        uint8_t* minW = sizeOfs(2);
        if (minW[2] == 0 || wt < minW[2]) { minW[0] = ht; minW[2] = wt; minW[1] = 1; }
        // Update MaxWeight (group 3) — overwrite if new is larger
        uint8_t* maxW = sizeOfs(3);
        if (wt > maxW[2]) { maxW[0] = ht; maxW[2] = wt; maxW[1] = 1; }
    }
}

// ============================================================
//  FRLG — SAV3 sector-based (caught/seen bitfields in section 0)
//  Section 0 contains PokeDex at offset 0x18.
//  Caught: PokeDex + 0x10 (52 bytes), Seen: PokeDex + 0x44 (52 bytes)
//  Also updates seen copies in sections 1 and 4.
// ============================================================

namespace {

constexpr int FRLG_POKEDEX_OFS       = 0x18;  // within section 0 data
constexpr int FRLG_CAUGHT_OFS        = 0x10;  // from PokeDex base
constexpr int FRLG_SEEN_OFS          = 0x44;  // from PokeDex base
constexpr int FRLG_PID_UNOWN_OFS     = 0x04;  // from PokeDex base (u32)
constexpr int FRLG_PID_SPINDA_OFS    = 0x08;  // from PokeDex base (u32)
constexpr int FRLG_MAX_SPECIES       = 386;
constexpr int FRLG_BITFIELD_SIZE     = 52;    // bytes (416 bits, only 386 used)

// Offsets for seen copies in the assembled Large data.
// Large starts at assembled byte 0xF2C (FRLG SmallLength).
// SeenOffset2 (FRLG) = 0x5F8 in Large → section 1, internal offset 0x5A4
// SeenOffset3 (FRLG) = 0x3A18 in Large → section 4, internal offset 0xB44
constexpr int FRLG_SEEN2_SECTION     = 1;
constexpr int FRLG_SEEN2_OFFSET      = 0x5A4; // within section 1 data
constexpr int FRLG_SEEN3_SECTION     = 4;
constexpr int FRLG_SEEN3_OFFSET      = 0xB44; // within section 4 data

} // anon

static void setFlagBit(uint8_t* base, int species) {
    int bit = species - 1;
    base[bit >> 3] |= static_cast<uint8_t>(1 << (bit & 7));
}

static void registerFRLG(SaveFile& save, const Pokemon& pkm) {
    uint16_t species = pkm.species();
    if (species == 0 || species > FRLG_MAX_SPECIES) return;

    // Section 0: caught + seen (primary)
    uint8_t* sect0 = save.findGbaSectorData(0);
    if (!sect0) return;

    uint8_t* pdx = sect0 + FRLG_POKEDEX_OFS;

    // Unown/Spinda: store PID on first encounter (before marking seen)
    auto isSeen = [&](int sp) {
        int b = sp - 1;
        return (pdx[FRLG_SEEN_OFS + (b >> 3)] >> (b & 7)) & 1;
    };
    if (species == 201 && !isSeen(201)) // Unown
        writeU32LE(pdx + FRLG_PID_UNOWN_OFS, pkm.pid());
    else if (species == 327 && !isSeen(327)) // Spinda
        writeU32LE(pdx + FRLG_PID_SPINDA_OFS, pkm.pid());

    // Set caught
    setFlagBit(pdx + FRLG_CAUGHT_OFS, species);

    // Set seen (primary)
    setFlagBit(pdx + FRLG_SEEN_OFS, species);

    // Sync seen copy in section 1
    uint8_t* sect1 = save.findGbaSectorData(FRLG_SEEN2_SECTION);
    if (sect1)
        setFlagBit(sect1 + FRLG_SEEN2_OFFSET, species);

    // Sync seen copy in section 4
    uint8_t* sect4 = save.findGbaSectorData(FRLG_SEEN3_SECTION);
    if (sect4)
        setFlagBit(sect4 + FRLG_SEEN3_OFFSET, species);
}

// ============================================================
//  Dispatcher
// ============================================================

void registerPokemon(SaveFile& save, const Pokemon& pkm) {
    if (pkm.isEmpty() || pkm.isEgg()) return;

    GameType game = save.gameType();

    if (game == GameType::ZA) {
        registerZA(save, pkm);
    } else if (isSV(game)) {
        registerSVKitakami(save, pkm);
    } else if (isSwSh(game)) {
        registerSwSh(save, pkm);
    } else if (isBDSP(game)) {
        registerBDSP(save, pkm);
    } else if (isLGPE(game)) {
        registerLGPE(save, pkm);
    } else if (isFRLG(game)) {
        registerFRLG(save, pkm);
    }
    // LA: intentionally skipped — research-task based dex, not applicable
}

} // namespace Pokedex
