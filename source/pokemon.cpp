#include "pokemon.h"
#include "species_converter.h"

// Experience growth tables (from PKHeX.Core Experience.cs)
// 6 tables x 100 entries: minimum EXP for each level (1-100)
static const uint32_t EXP_TABLE[6][100] = {
    // Growth 0: Medium Fast
    {0, 8, 27, 64, 125, 216, 343, 512, 729, 1000,
     1331, 1728, 2197, 2744, 3375, 4096, 4913, 5832, 6859, 8000,
     9261, 10648, 12167, 13824, 15625, 17576, 19683, 21952, 24389, 27000,
     29791, 32768, 35937, 39304, 42875, 46656, 50653, 54872, 59319, 64000,
     68921, 74088, 79507, 85184, 91125, 97336, 103823, 110592, 117649, 125000,
     132651, 140608, 148877, 157464, 166375, 175616, 185193, 195112, 205379, 216000,
     226981, 238328, 250047, 262144, 274625, 287496, 300763, 314432, 328509, 343000,
     357911, 373248, 389017, 405224, 421875, 438976, 456533, 474552, 493039, 512000,
     531441, 551368, 571787, 592704, 614125, 636056, 658503, 681472, 704969, 729000,
     753571, 778688, 804357, 830584, 857375, 884736, 912673, 941192, 970299, 1000000},
    // Growth 1: Medium Slow
    {0, 15, 52, 122, 237, 406, 637, 942, 1326, 1800,
     2369, 3041, 3822, 4719, 5737, 6881, 8155, 9564, 11111, 12800,
     14632, 16610, 18737, 21012, 23437, 26012, 28737, 31610, 34632, 37800,
     41111, 44564, 48155, 51881, 55737, 59719, 63822, 68041, 72369, 76800,
     81326, 85942, 90637, 95406, 100237, 105122, 110052, 115015, 120001, 125000,
     131324, 137795, 144410, 151165, 158056, 165079, 172229, 179503, 186894, 194400,
     202013, 209728, 217540, 225443, 233431, 241496, 249633, 257834, 267406, 276458,
     286328, 296358, 305767, 316074, 326531, 336255, 346965, 357812, 367807, 378880,
     390077, 400293, 411686, 423190, 433572, 445239, 457001, 467489, 479378, 491346,
     501878, 513934, 526049, 536557, 548720, 560922, 571333, 583539, 591882, 600000},
    // Growth 2: Fast
    {0, 4, 13, 32, 65, 112, 178, 276, 393, 540,
     745, 967, 1230, 1591, 1957, 2457, 3046, 3732, 4526, 5440,
     6482, 7666, 9003, 10506, 12187, 14060, 16140, 18439, 20974, 23760,
     26811, 30146, 33780, 37731, 42017, 46656, 50653, 55969, 60505, 66560,
     71677, 78533, 84277, 91998, 98415, 107069, 114205, 123863, 131766, 142500,
     151222, 163105, 172697, 185807, 196322, 210739, 222231, 238036, 250562, 267840,
     281456, 300293, 315059, 335544, 351520, 373744, 390991, 415050, 433631, 459620,
     479600, 507617, 529063, 559209, 582187, 614566, 639146, 673863, 700115, 737280,
     765275, 804997, 834809, 877201, 908905, 954084, 987754, 1035837, 1071552, 1122660,
     1160499, 1214753, 1254796, 1312322, 1354652, 1415577, 1460276, 1524731, 1571884, 1640000},
    // Growth 3: Parabolic (Medium Slow variant)
    {0, 9, 57, 96, 135, 179, 236, 314, 419, 560,
     742, 973, 1261, 1612, 2035, 2535, 3120, 3798, 4575, 5460,
     6458, 7577, 8825, 10208, 11735, 13411, 15244, 17242, 19411, 21760,
     24294, 27021, 29949, 33084, 36435, 40007, 43808, 47846, 52127, 56660,
     61450, 66505, 71833, 77440, 83335, 89523, 96012, 102810, 109923, 117360,
     125126, 133229, 141677, 150476, 159635, 169159, 179056, 189334, 199999, 211060,
     222522, 234393, 246681, 259392, 272535, 286115, 300140, 314618, 329555, 344960,
     360838, 377197, 394045, 411388, 429235, 447591, 466464, 485862, 505791, 526260,
     547274, 568841, 590969, 613664, 636935, 660787, 685228, 710266, 735907, 762160,
     789030, 816525, 844653, 873420, 902835, 932903, 963632, 995030, 1027103, 1059860},
    // Growth 4: Slow
    {0, 6, 21, 51, 100, 172, 274, 409, 583, 800,
     1064, 1382, 1757, 2195, 2700, 3276, 3930, 4665, 5487, 6400,
     7408, 8518, 9733, 11059, 12500, 14060, 15746, 17561, 19511, 21600,
     23832, 26214, 28749, 31443, 34300, 37324, 40522, 43897, 47455, 51200,
     55136, 59270, 63605, 68147, 72900, 77868, 83058, 88473, 94119, 100000,
     106120, 112486, 119101, 125971, 133100, 140492, 148154, 156089, 164303, 172800,
     181584, 190662, 200037, 209715, 219700, 229996, 240610, 251545, 262807, 274400,
     286328, 298598, 311213, 324179, 337500, 351180, 365226, 379641, 394431, 409600,
     425152, 441094, 457429, 474163, 491300, 508844, 526802, 545177, 563975, 583200,
     602856, 622950, 643485, 664467, 685900, 707788, 730138, 752953, 776239, 800000},
    // Growth 5: Erratic
    {0, 10, 33, 80, 156, 270, 428, 640, 911, 1250,
     1663, 2160, 2746, 3430, 4218, 5120, 6141, 7290, 8573, 10000,
     11576, 13310, 15208, 17280, 19531, 21970, 24603, 27440, 30486, 33750,
     37238, 40960, 44921, 49130, 53593, 58320, 63316, 68590, 74148, 80000,
     86151, 92610, 99383, 106480, 113906, 121670, 129778, 138240, 147061, 156250,
     165813, 175760, 186096, 196830, 207968, 219520, 231491, 243890, 256723, 270000,
     283726, 297910, 312558, 327680, 343281, 359370, 375953, 393040, 410636, 428750,
     447388, 466560, 486271, 506530, 527343, 548720, 570666, 593190, 616298, 640000,
     664301, 689210, 714733, 740880, 767656, 795070, 823128, 851840, 881211, 911250,
     941963, 973360, 1005446, 1038230, 1071718, 1105920, 1140841, 1176490, 1212873, 1250000},
};

// LA growth rates per species (from personal_la, offset 0x15 per entry)
// 1276 entries covering all species + forms
static const uint8_t LA_GROWTH_RATES[1276] = {
    0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3,
    3, 3, 3, 4, 4, 0, 0, 4, 4, 0, 0, 3, 3, 3, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 0, 0, 0, 5, 5, 3, 3, 3, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 3, 3, 3, 0,
    0, 0, 0, 0, 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 5,
    5, 4, 0, 0, 0, 0, 0, 0, 5, 5, 0, 0, 0, 0, 0, 5,
    5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 3, 0, 0, 0, 3, 3, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 0, 4, 4, 4,
    4, 0, 0, 0, 0, 0, 3, 4, 4, 0, 3, 0, 0, 0, 4, 0,
    0, 0, 0, 0, 0, 0, 3, 0, 4, 0, 0, 0, 0, 0, 0, 3,
    0, 0, 0, 0, 0, 3, 5, 3, 0, 0, 0, 0, 5, 5, 4, 0,
    0, 4, 5, 5, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0,
    0, 5, 4, 5, 5, 5, 5, 5, 5, 5, 5, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3,
    3, 3, 3, 3, 0, 0, 0, 0, 5, 5, 5, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 3, 3, 3, 0, 0, 4, 0, 0, 0, 3, 4,
    5, 5, 5, 0, 0, 5, 5, 0, 0, 0, 0, 3, 0, 0, 5, 5,
    2, 2, 0, 0, 0, 0, 0, 0, 3, 3, 3, 0, 0, 1, 1, 0,
    0, 4, 4, 0, 0, 2, 2, 0, 0, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 4, 4, 0, 4, 3, 0, 0, 0, 3, 3, 3, 0, 0,
    0, 5, 0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0,
    0, 3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 0, 0, 0, 3,
    3, 0, 0, 0, 0, 0, 0, 0, 4, 2, 2, 0, 0, 4, 3, 4,
    4, 4, 0, 0, 0, 0, 0, 0, 4, 3, 0, 5, 5, 5, 5, 3,
    3, 5, 5, 5, 5, 0, 0, 5, 1, 1, 5, 5, 5, 3, 0, 0,
    5, 0, 0, 0, 4, 0, 0, 0, 3, 5, 0, 5, 0, 4, 0, 0,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 5, 5, 0,
    0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 3, 3, 3, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 4, 3, 3, 3, 0, 0, 3, 3, 3, 0,
    0, 0, 0, 4, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 3,
    3, 3, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 4, 4, 3, 3,
    3, 3, 3, 3, 0, 0, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0, 3,
    3, 3, 5, 5, 5, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0,
    0, 0, 0, 5, 5, 5, 5, 0, 0, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 0, 5, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5,
    5, 5, 5, 4, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 5, 5,
    0, 5, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 4, 5, 5, 0,
    0, 0, 0, 4, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 0, 0, 3, 3, 3, 0, 0, 0, 4, 4, 0, 0, 0,
    0, 0, 0, 4, 4, 3, 3, 3, 1, 1, 1, 0, 0, 0, 5, 5,
    3, 3, 0, 0, 3, 3, 0, 0, 5, 5, 5, 0, 0, 0, 0, 0,
    4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 4, 0, 0, 0,
    5, 5, 5, 5, 0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 0, 0, 0, 3, 0, 5, 3, 3, 3, 3, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 5, 5, 5, 3, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 3, 0, 0, 0, 5, 0, 0, 0, 0, 5, 5, 0,
    5, 5, 5, 5, 5, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 5, 3, 4, 0, 5, 3, 3, 3, 0,
    0, 5, 3, 4, 5, 0, 5, 5, 0, 1, 0, 0, 0, 0, 3, 0,
    5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 5, 3, 5, 5, 0, 0, 0, 0, 0, 5, 5, 5,
    3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 3, 4, 0, 0, 0, 0, 3, 3, 3, 3, 0, 3, 3,
    0, 0, 0, 0, 0, 0, 0, 5, 5, 5, 5, 5, 5, 5, 0, 5,
    5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0,
    5, 5, 5, 5, 5, 5, 0, 3, 0, 0, 0, 0, 0, 0, 4, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5,
    5, 5, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5,
    4, 0, 5, 5, 5, 5, 5, 5, 5, 0, 0, 5,
};

// Calculate level from EXP using growth rate table
static uint8_t levelFromExp(uint32_t exp, uint8_t growth) {
    if (growth > 5) growth = 0;
    const uint32_t* table = EXP_TABLE[growth];
    if (exp >= table[99]) return 100;
    uint8_t lv = 1;
    while (lv < 100 && exp >= table[lv])
        ++lv;
    return lv;
}

// Gen3 FRLG growth rates per national species (0-386)
// From PKHeX PersonalTable FR (offset 0x13 per 0x1C-byte entry)
static const uint8_t FRLG_GROWTH_RATES[387] = {
    0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0,
    3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3,
    3, 3, 3, 4, 4, 0, 0, 4, 4, 0, 0, 3, 3, 3, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 5, 5, 3, 3, 3, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 3, 3, 3, 0,
    0, 0, 0, 0, 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 5,
    5, 4, 0, 0, 0, 0, 0, 0, 5, 5, 0, 0, 0, 0, 0, 5,
    5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 0, 0, 0, 0, 4, 4, 4, 4, 0, 5, 5, 0, 4, 4, 4,
    4, 0, 0, 3, 3, 3, 3, 4, 4, 0, 3, 3, 3, 3, 4, 3,
    3, 0, 0, 0, 0, 0, 3, 0, 4, 0, 0, 0, 0, 0, 0, 3,
    0, 4, 4, 0, 0, 3, 5, 3, 0, 0, 0, 0, 5, 5, 4, 0,
    0, 4, 5, 5, 5, 5, 0, 0, 0, 0, 5, 4, 0, 0, 0, 0,
    0, 5, 4, 5, 5, 5, 5, 5, 5, 5, 5, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3,
    3, 3, 3, 3, 3, 3, 0, 0, 5, 5, 5, 0, 0, 2, 2, 5,
    5, 5, 1, 1, 1, 3, 3, 3, 2, 2, 4, 0, 4, 4, 3, 4,
    5, 5, 5, 0, 0, 5, 5, 0, 0, 1, 2, 3, 2, 2, 5, 5,
    2, 2, 0, 0, 0, 4, 4, 4, 3, 3, 3, 3, 3, 1, 1, 1,
    2, 4, 4, 0, 0, 2, 2, 0, 0, 1, 1, 1, 1, 1, 1, 0,
    3, 4, 4, 4, 4, 5, 4, 3, 0, 0, 0, 3, 3, 3, 1, 1,
    1, 5, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5,
};

// Gen3 FRLG gender ratios per national species (0-386)
// From PKHeX PersonalTable FR (offset 0x10 per 0x1C-byte entry)
// 0x00=all male, 0x1F=87.5%M, 0x3F=75%M, 0x7F=50/50, 0xBF=25%M, 0xFE=all female, 0xFF=genderless
static const uint8_t FRLG_GENDER_RATIOS[387] = {
    0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFE, 0xFE, 0xFE,
    0x00, 0x00, 0x00, 0xBF, 0xBF, 0xBF, 0xBF, 0xBF, 0xBF, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x3F, 0x3F, 0x7F, 0x7F, 0x7F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0xFF, 0xFF, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0xFF, 0xFF, 0x7F, 0x7F, 0x7F, 0x7F, 0x00, 0x00, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF, 0xFF, 0x7F, 0x7F, 0xFE, 0x3F, 0x3F, 0x7F,
    0x00, 0x7F, 0x7F, 0x7F, 0xFF, 0x1F, 0x1F, 0x1F, 0x1F, 0xFF, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
    0xFF, 0xFF, 0xFF, 0x7F, 0x7F, 0x7F, 0xFF, 0xFF, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
    0x1F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xBF, 0xBF, 0x1F,
    0x1F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x1F, 0x1F, 0x7F, 0x7F, 0x7F, 0xFF, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0xBF, 0xBF, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xBF, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF, 0x7F, 0x7F, 0x00, 0x00, 0xFE, 0x3F,
    0x3F, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F, 0x7F, 0xFF, 0xFF, 0xFF, 0x1F, 0x1F, 0x1F, 0x1F,
    0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0xFF, 0x7F, 0x7F, 0x7F, 0x3F, 0x3F, 0xBF, 0x7F, 0xBF, 0xBF, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x00, 0xFE, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0xFF, 0xFF, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF, 0xFF, 0x1F, 0x1F, 0x1F, 0x1F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x1F, 0xBF, 0x7F, 0x7F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF,
};

uint8_t Pokemon::level() const {
    if (isFRLG(gameType_)) {
        // PK3: calculate from EXP at offset 0x24 using species growth rate
        uint32_t exp = readU32(0x24);
        uint16_t sp = species();
        uint8_t growth = (sp < 387) ? FRLG_GROWTH_RATES[sp] : 0;
        return levelFromExp(exp, growth);
    }
    if (isLGPE(gameType_))
        return data[0xEC]; // PB7: Stat_Level in party stats
    if (gameType_ != GameType::LA)
        return data[0x148];

    // LA box data doesn't store level — calculate from EXP (offset 0x10)
    uint32_t exp = readU32(0x10);
    uint16_t sp = speciesInternal();
    uint8_t growth = (sp < 1276) ? LA_GROWTH_RATES[sp] : 0;
    return levelFromExp(exp, growth);
}

uint16_t Pokemon::species() const {
    if (isFRLG(gameType_))
        return SpeciesConverter::getNational3(speciesInternal());
    if (isSwSh(gameType_) || isBDSP(gameType_) || gameType_ == GameType::LA || isLGPE(gameType_))
        return speciesInternal(); // PK8/PB8/PA8/PB7 stores national dex ID directly
    return SpeciesConverter::getNational9(speciesInternal());
}

uint8_t Pokemon::gender() const {
    if (isFRLG(gameType_)) {
        uint16_t sp = species();
        if (sp >= 387) return 2; // genderless
        uint8_t ratio = FRLG_GENDER_RATIOS[sp];
        if (ratio == 0xFF) return 2; // genderless
        if (ratio == 0xFE) return 1; // always female
        if (ratio == 0x00) return 0; // always male
        return (pid() & 0xFF) >= ratio ? 0 : 1; // 0=male, 1=female
    }
    if (isLGPE(gameType_))
        return (data[0x1D] >> 1) & 3;
    return (data[0x22] >> 2) & 3;
}

// Gen3 English character encoding table (byte 0x00-0xFF → Unicode codepoint)
// From PKHeX StringConverter3.cs G3_EN — terminator = 0xFF → 0
static const uint16_t G3_EN[256] = {
    // 0x00-0x0F
    0x0020, 0x00C0, 0x00C1, 0x00C2, 0x00C7, 0x00C8, 0x00C9, 0x00CA,
    0x00CB, 0x00CC, 0x3053, 0x00CE, 0x00CF, 0x00D2, 0x00D3, 0x00D4,
    // 0x10-0x1F
    0x0152, 0x00D9, 0x00DA, 0x00DB, 0x00D1, 0x00DF, 0x00E0, 0x00E1,
    0x306D, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x307E,
    // 0x20-0x2F
    0x00EE, 0x00EF, 0x00F2, 0x00F3, 0x00F4, 0x0153, 0x00F9, 0x00FA,
    0x00FB, 0x00F1, 0x00BA, 0x00AA, 0x2469, 0x0026, 0x002B, 0x3042,
    // 0x30-0x3F
    0x3043, 0x3045, 0x3047, 0x3049, 0x3083, 0x003D, 0x003B, 0x304C,
    0x304E, 0x3050, 0x3052, 0x3054, 0x3056, 0x3058, 0x305A, 0x305C,
    // 0x40-0x4F
    0x305E, 0x3060, 0x3062, 0x3065, 0x3067, 0x3069, 0x3070, 0x3073,
    0x3076, 0x3079, 0x307C, 0x3071, 0x3074, 0x3077, 0x307A, 0x307D,
    // 0x50-0x5F
    0x3063, 0x00BF, 0x00A1, 0x2483, 0x2484, 0x30AA, 0x30AB, 0x30AD,
    0x30AF, 0x30B1, 0x00CD, 0x0025, 0x0028, 0x0029, 0x30BB, 0x30BD,
    // 0x60-0x6F
    0x30BF, 0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC,
    0x00E2, 0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x00ED,
    // 0x70-0x7F
    0x30DF, 0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9,
    0x30EA, 0x2191, 0x2193, 0x2190, 0xFF0B, 0x30F2, 0x30F3, 0x30A1,
    // 0x80-0x8F
    0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x2482, 0x003C, 0x003E, 0x30AC,
    0x30AE, 0x30B0, 0x30B2, 0x30B4, 0x30B6, 0x30B8, 0x30BA, 0x30BC,
    // 0x90-0x9F
    0x30BE, 0x30C0, 0x30C2, 0x30C5, 0x30C7, 0x30C9, 0x30D0, 0x30D3,
    0x30D6, 0x30D9, 0x30DC, 0x30D1, 0x30D4, 0x30D7, 0x30DA, 0x30DD,
    // 0xA0-0xAF
    0x30C3, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036,
    0x0037, 0x0038, 0x0039, 0x0021, 0x003F, 0x002E, 0x002D, 0xFF65,
    // 0xB0-0xBF
    0x246C, 0x201C, 0x201D, 0x2018, 0x0027, 0x2642, 0x2640, 0x0024,
    0x002C, 0x2467, 0x002F, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045,
    // 0xC0-0xCF
    0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D,
    0x004E, 0x004F, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055,
    // 0xD0-0xDF
    0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x0061, 0x0062, 0x0063,
    0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B,
    // 0xE0-0xEF
    0x006C, 0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073,
    0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x25BA,
    // 0xF0-0xFF
    0x003A, 0x00C4, 0x00D6, 0x00DC, 0x00E4, 0x00F6, 0x00FC, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// Decode a Gen3 encoded string to UTF-8
static std::string readGen3String(const uint8_t* base, int offset, int maxBytes) {
    std::string result;
    for (int i = 0; i < maxBytes; i++) {
        uint8_t b = base[offset + i];
        if (b == 0xFF) // Gen3 terminator
            break;
        uint16_t ch = G3_EN[b];
        if (ch == 0)
            break;
        // Encode Unicode codepoint as UTF-8
        if (ch < 0x80) {
            result += static_cast<char>(ch);
        } else if (ch < 0x800) {
            result += static_cast<char>(0xC0 | (ch >> 6));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (ch >> 12));
            result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }
    return result;
}

// Helper: read UTF-16LE string from given offset
static std::string readUtf16String(const uint8_t* base, int offset, int maxChars) {
    std::string result;
    for (int i = 0; i < maxChars; i++) {
        uint16_t ch;
        std::memcpy(&ch, base + offset + i * 2, 2);
        if (ch == 0)
            break;
        if (ch < 0x80) {
            result += static_cast<char>(ch);
        } else if (ch < 0x800) {
            result += static_cast<char>(0xC0 | (ch >> 6));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (ch >> 12));
            result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }
    return result;
}

std::string Pokemon::nickname() const {
    if (isFRLG(gameType_))
        return readGen3String(data.data(), 0x08, 10);
    int ofs;
    if (isLGPE(gameType_))       ofs = 0x40;
    else if (gameType_ == GameType::LA) ofs = 0x60;
    else                                ofs = 0x58;
    return readUtf16String(data.data(), ofs, 13);
}

std::string Pokemon::otName() const {
    if (isFRLG(gameType_))
        return readGen3String(data.data(), 0x14, 7);
    int ofs;
    if (isLGPE(gameType_))       ofs = 0xB0;
    else if (gameType_ == GameType::LA) ofs = 0x110;
    else                                ofs = 0xF8;
    return readUtf16String(data.data(), ofs, 13);
}

std::string Pokemon::displayName() const {
    if (isEmpty())
        return "";
    if (isEgg())
        return SpeciesName::get(0); // "Egg"
    if (isNicknamed())
        return nickname();
    return SpeciesName::get(species());
}

void Pokemon::loadFromEncrypted(const uint8_t* encrypted, size_t len) {
    if (isFRLG(gameType_))
        PokeCrypto::decryptArray3(encrypted, len, data.data());
    else if (isLGPE(gameType_))
        PokeCrypto::decryptArray6(encrypted, len, data.data());
    else if (gameType_ == GameType::LA)
        PokeCrypto::decryptArray8A(encrypted, len, data.data());
    else
        PokeCrypto::decryptArray9(encrypted, len, data.data());
}

void Pokemon::refreshChecksum() {
    if (isFRLG(gameType_)) {
        // PK3: sum u16 words from 0x20 to 0x4F (48 bytes = 24 words), store at 0x1C
        uint16_t chk = 0;
        for (int i = 0x20; i < 0x50; i += 2)
            chk += readU16(i);
        writeU16(0x1C, chk);
        return;
    }
    // Checksum = sum of all 16-bit words from byte 8 to SIZE_STORED
    int end;
    if (isLGPE(gameType_))       end = PokeCrypto::SIZE_6STORED;
    else if (gameType_ == GameType::LA) end = PokeCrypto::SIZE_8ASTORED;
    else                                end = PokeCrypto::SIZE_9STORED;
    uint16_t chk = 0;
    for (int i = 8; i < end; i += 2)
        chk += readU16(i);
    writeU16(0x06, chk);
}

void Pokemon::getEncrypted(uint8_t* outBuf) {
    refreshChecksum();
    if (isFRLG(gameType_))
        PokeCrypto::encryptArray3(data.data(), PokeCrypto::SIZE_3STORED, outBuf);
    else if (isLGPE(gameType_))
        PokeCrypto::encryptArray6(data.data(), PokeCrypto::SIZE_6PARTY, outBuf);
    else if (gameType_ == GameType::LA)
        PokeCrypto::encryptArray8A(data.data(), PokeCrypto::SIZE_8ASTORED, outBuf);
    else
        PokeCrypto::encryptArray9(data.data(), PokeCrypto::SIZE_9PARTY, outBuf);
}
