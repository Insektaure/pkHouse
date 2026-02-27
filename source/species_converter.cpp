#include "species_converter.h"
#include <fstream>

// Delta tables from SpeciesConverter.cs lines 160-173
// Index: raw - FIRST_UNALIGNED (917)
// Result: raw + delta[index]
static constexpr int FIRST_UNALIGNED_9 = 917;

static const int8_t TABLE_9_INTERNAL_TO_NATIONAL[] = {
                                       65, -1, -1,
    -1, -1, 31, 31, 47, 47, 29, 29, 53, 31,
    31, 46, 44, 30, 30, -7, -7, -7, 13, 13,
    -2, -2, 23, 23, 24, -21, -21, 27, 27, 47,
    47, 47, 26, 14, -33, -33, -33, -17, -17, 3,
    -29, 12, -12, -31, -31, -31, 3, 3, -24, -24,
    -44, -44, -30, -30, -28, -28, 23, 23, 6, 7,
    29, 8, 3, 4, 4, 20, 4, 23, 6, 3,
    3, 4, -1, 13, 9, 7, 5, 7, 9, 9,
    -43, -43, -43, -68, -68, -68, -58, -58, -25, -29,
    -31, 6, -1, 6, 0, 0, 0, 3, 3, 4,
    2, 3, 3, -5, -12, -12,
};

static const int8_t TABLE_9_NATIONAL_TO_INTERNAL[] = {
                                       1, 1, 1,
    1, 33, 33, 33, 21, 21, 44, 44, 7, 7,
    7, 29, 31, 31, 31, 68, 68, 68, 2, 2,
    17, 17, 30, 30, 24, 24, 28, 28, 58, 58,
    12, -13, -13, -31, -31, -29, -29, 43, 43, 43,
    -31, -31, -3, -30, -30, -23, -23, -14, -24, -3,
    -3, -47, -47, -12, -27, -27, -44, -46, -26, 31,
    29, -53, -65, 25, -6, -3, -7, -4, -4, -8,
    -4, 1, -3, -3, -6, -4, -47, -47, -47, -23,
    -23, -5, -7, -9, -7, -20, -13, -9, -9, -29,
    -23, 1, 12, 12, 0, 0, 0, -6, 5, -6,
    -3, -3, -2, -4, -3, -3,
};

static constexpr size_t TABLE_INT_TO_NAT_SIZE = sizeof(TABLE_9_INTERNAL_TO_NATIONAL);
static constexpr size_t TABLE_NAT_TO_INT_SIZE = sizeof(TABLE_9_NATIONAL_TO_INTERNAL);

uint16_t SpeciesConverter::getNational9(uint16_t raw) {
    int shift = raw - FIRST_UNALIGNED_9;
    if (static_cast<unsigned>(shift) >= TABLE_INT_TO_NAT_SIZE)
        return raw;
    return static_cast<uint16_t>(raw + TABLE_9_INTERNAL_TO_NATIONAL[shift]);
}

uint16_t SpeciesConverter::getInternal9(uint16_t species) {
    int shift = species - FIRST_UNALIGNED_9;
    if (static_cast<unsigned>(shift) >= TABLE_NAT_TO_INT_SIZE)
        return species;
    return static_cast<uint16_t>(species + TABLE_9_NATIONAL_TO_INTERNAL[shift]);
}

// --- Gen3 species conversion (from PKHeX SpeciesConverter.cs lines 87-136) ---
// Species 1-251 map 1:1 between national and internal. Divergence starts at 252/277.
static constexpr int FIRST_UNALIGNED_NATIONAL_3 = 252;
static constexpr int FIRST_UNALIGNED_INTERNAL_3 = 277;

// Delta: internal ID (index + 277) + delta = national ID
static const int8_t TABLE_3_INTERNAL_TO_NATIONAL[] = {
                                       -25, -25, -25,
    -25, -25, -25, -25, -25, -25, -25, -25, -25, -25,
    -25, -25, -25, -25, -25, -25, -25, -25, -25, -25,
    -25, -11, -11, -11, -28, -28, -21, -21,  19, -31,
    -31, -28, -28,   7,   7, -15, -15,  35,  25,  25,
    -21,   3, -20,  16,  16,  45,  15,  15,  21,  21,
    -12, -12,  -4,  -4,  -4, -39, -39, -28, -28, -17,
    -17,  22,  22,  22, -13, -13,  15,  15, -11, -11,
    -52, -26, -26, -42, -42, -52, -49, -49, -25, -25,
      0,  -6,  -6, -48, -77, -77, -77, -51, -51, -12,
    -77, -77, -77,  -7,  -7,  -7, -17, -24, -24, -43,
    -45, -12, -78, -78, -78, -34, -73, -73, -43, -43,
    -43, -43,-112,-112,-112, -24, -24, -24, -24, -24,
    -24, -24, -24, -24, -22, -22, -22, -27, -27, -24,
    -24, -53,
};

// Delta: national ID (index + 252) + delta = internal ID
static const int8_t TABLE_3_NATIONAL_TO_INTERNAL[] = {
              25,  25,  25,  25,  25,  25,  25,  25,
     25,  25,  25,  25,  25,  25,  25,  25,  25,  25,
     25,  25,  25,  25,  25,  25,  28,  28,  31,  31,
    112, 112, 112,  28,  28,  21,  21,  77,  77,  77,
     11,  11,  11,  77,  77,  77,  39,  39,  52,  21,
     15,  15,  20,  52,  78,  78,  78,  49,  49,  28,
     28,  42,  42,  73,  73,  48,  51,  51,  12,  12,
     -7,  -7,  17,  17,  -3,  26,  26, -19,   4,   4,
      4,  13,  13,  25,  25,  45,  43,  11,  11, -16,
    -16, -15, -15, -25, -25,  43,  43,  43,  43, -21,
    -21,  34, -35,  24,  24,   6,   6,  12,  53,  17,
      0, -15, -15, -22, -22, -22,   7,   7,   7,  12,
    -45,  24,  24,  24,  24,  24,  24,  24,  24,  24,
     27,  27,  22,  22,  22,  24,  24,
};

static constexpr size_t TABLE_3_INT_TO_NAT_SIZE = sizeof(TABLE_3_INTERNAL_TO_NATIONAL);
static constexpr size_t TABLE_3_NAT_TO_INT_SIZE = sizeof(TABLE_3_NATIONAL_TO_INTERNAL);

uint16_t SpeciesConverter::getNational3(uint16_t raw) {
    if (raw < FIRST_UNALIGNED_INTERNAL_3)
        return raw;
    int shift = raw - FIRST_UNALIGNED_INTERNAL_3;
    if (static_cast<unsigned>(shift) >= TABLE_3_INT_TO_NAT_SIZE)
        return raw;
    return static_cast<uint16_t>(raw + TABLE_3_INTERNAL_TO_NATIONAL[shift]);
}

uint16_t SpeciesConverter::getInternal3(uint16_t species) {
    if (species < FIRST_UNALIGNED_NATIONAL_3)
        return species;
    int shift = species - FIRST_UNALIGNED_NATIONAL_3;
    if (static_cast<unsigned>(shift) >= TABLE_3_NAT_TO_INT_SIZE)
        return species;
    return static_cast<uint16_t>(species + TABLE_3_NATIONAL_TO_INTERNAL[shift]);
}

// Species name data
static std::vector<std::string> s_names;
static const std::string s_unknown = "???";

void SpeciesName::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        return;
    std::string line;
    while (std::getline(file, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        s_names.push_back(std::move(line));
    }
}

const std::string& SpeciesName::get(uint16_t nationalId) {
    if (nationalId < s_names.size())
        return s_names[nationalId];
    return s_unknown;
}

// Move name data
static std::vector<std::string> s_moveNames;

void MoveName::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        return;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        s_moveNames.push_back(std::move(line));
    }
}

const std::string& MoveName::get(uint16_t moveId) {
    if (moveId < s_moveNames.size())
        return s_moveNames[moveId];
    return s_unknown;
}

// Nature name data
static std::vector<std::string> s_natureNames;

void NatureName::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        return;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        s_natureNames.push_back(std::move(line));
    }
}

const std::string& NatureName::get(uint8_t natureId) {
    if (natureId < s_natureNames.size())
        return s_natureNames[natureId];
    return s_unknown;
}

// Ability name data
static std::vector<std::string> s_abilityNames;

void AbilityName::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        return;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        s_abilityNames.push_back(std::move(line));
    }
}

const std::string& AbilityName::get(uint16_t abilityId) {
    if (abilityId < s_abilityNames.size())
        return s_abilityNames[abilityId];
    return s_unknown;
}
