#include "theme.h"
#include <cstdio>

static const Theme themes[THEME_COUNT] = {
    // ===== 0: pkHouse (UI 2.0 dark) =====
    {
        .name            = "pkHouse",
        .bg              = {28, 29, 41, 255},
        .panelBg         = {38, 39, 54, 255},
        .slotEmpty       = {32, 33, 45, 255},
        .slotFull        = {52, 54, 75, 255},
        .slotEgg         = {52, 54, 75, 255},
        .selected        = {92, 200, 217, 255},
        .selectedPos     = {99, 199, 99, 255},
        .text            = {244, 244, 248, 255},
        .textDim         = {169, 171, 192, 255},
        .textOnBadge     = {28, 29, 41, 255},
        .statusText      = {244, 244, 248, 255},
        .red             = {232, 90, 90, 255},
        .goldLabel       = {255, 210, 31, 255},
        .genderMale      = {100, 160, 255, 255},
        .genderFemale    = {255, 130, 160, 255},
        .searchMatch     = {255, 140, 50, 255},
        .searchDim       = {20, 20, 30, 150},
        .partyMark       = {92, 200, 217, 255},

        .accent          = {255, 210, 31, 255},
        .accentSave      = {255, 210, 31, 255},
        .accentBank      = {92, 200, 217, 255},
        .panelBorder     = {52, 54, 74, 255},
        .divider         = {58, 60, 82, 255},
        .textMuted       = {140, 143, 165, 255},
        .cellBorder      = {63, 66, 96, 255},
        .cellCursor      = {61, 64, 88, 255},
        .cellEmptyBorder = {58, 60, 82, 255},
        .dot             = {63, 66, 96, 255},
        .buttonBg        = {47, 48, 68, 255},
        .buttonBorder    = {58, 60, 82, 255},
        .keyCap          = {244, 244, 248, 255},
        .keyCapText      = {28, 29, 41, 255},
        .statusOk        = {99, 199, 99, 255},
        .statusWarn      = {240, 170, 50, 255},
        .alphaMark       = {232, 90, 90, 255},
        .badgeBg         = {74, 68, 36, 255},
        .miniDotEmpty    = {46, 48, 66, 255},
        .miniDotFull     = {111, 115, 144, 255},
        .eggMark         = {239, 230, 207, 255},
    },

    // ===== 1: pkHome (light, after Pokemon HOME) =====
    // Mint ground, white panels framed in teal, pale mint slots, teal for the
    // bank side and the labels, and HOME's orange for the accent and the
    // cursor. Dark text throughout, so the key caps flip: dark caps with
    // white glyphs, and white ink on orange.
    {
        .name            = "pkHome",
        .bg              = {196, 236, 208, 255},
        .panelBg         = {250, 253, 251, 255},
        .slotEmpty       = {234, 247, 239, 255},
        .slotFull        = {226, 244, 234, 255},
        .slotEgg         = {244, 238, 222, 255},
        .selected        = {38, 166, 154, 255},
        .selectedPos     = {46, 160, 90, 255},
        .text            = {48, 52, 56, 255},
        .textDim         = {92, 108, 104, 255},
        .textOnBadge     = {255, 255, 255, 255},
        .statusText      = {48, 52, 56, 255},
        .red             = {214, 64, 64, 255},
        .goldLabel       = {244, 140, 30, 255},
        .genderMale      = {40, 110, 230, 255},
        .genderFemale    = {228, 70, 118, 255},
        .searchMatch     = {226, 96, 30, 255},
        .searchDim       = {246, 251, 248, 170},
        .partyMark       = {38, 166, 154, 255},

        .accent          = {244, 140, 30, 255},
        .accentSave      = {232, 122, 20, 255},
        .accentBank      = {38, 166, 154, 255},
        .panelBorder     = {64, 182, 166, 255},
        .divider         = {168, 216, 202, 255},
        .textMuted       = {136, 152, 148, 255},
        .cellBorder      = {196, 230, 214, 255},
        .cellCursor      = {255, 236, 212, 255},
        .cellEmptyBorder = {200, 230, 215, 255},
        .dot             = {178, 220, 206, 255},
        .buttonBg        = {255, 255, 255, 255},
        .buttonBorder    = {120, 200, 186, 255},
        .keyCap          = {56, 60, 64, 255},
        .keyCapText      = {255, 255, 255, 255},
        .statusOk        = {40, 150, 84, 255},
        .statusWarn      = {222, 116, 18, 255},
        .alphaMark       = {214, 64, 64, 255},
        .badgeBg         = {255, 229, 196, 255},
        .miniDotEmpty    = {206, 230, 218, 255},
        .miniDotFull     = {104, 134, 126, 255},
        .eggMark         = {196, 168, 110, 255},
    },
};

const Theme& getTheme(int index) {
    if (index < 0 || index >= THEME_COUNT)
        return themes[0];
    return themes[index];
}

const char* getThemeName(int index) {
    if (index < 0 || index >= THEME_COUNT)
        return themes[0].name;
    return themes[index].name;
}

int loadThemeIndex(const std::string& basePath) {
    std::string path = basePath + "theme.cfg";
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return 0;
    uint8_t idx = 0;
    std::fread(&idx, 1, 1, f);
    std::fclose(f);
    if (idx >= THEME_COUNT) idx = 0;
    return idx;
}

void saveThemeIndex(const std::string& basePath, int index) {
    std::string path = basePath + "theme.cfg";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    uint8_t idx = static_cast<uint8_t>(index);
    std::fwrite(&idx, 1, 1, f);
    std::fclose(f);
}
