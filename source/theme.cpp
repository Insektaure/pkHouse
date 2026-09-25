#include "theme.h"
#include <cstdio>

static const Theme themes[THEME_COUNT] = {
    // ===== 0: pkHouse (UI 2.0 dark) =====
    {
        .name            = "pkHouse",
        .bg              = {28, 29, 41, 255},
        .panelBg         = {38, 39, 54, 255},
        .statusBarBg     = {28, 29, 41, 255},
        .slotEmpty       = {32, 33, 45, 255},
        .slotFull        = {52, 54, 75, 255},
        .slotEgg         = {52, 54, 75, 255},
        .cursor          = {255, 210, 31, 255},
        .selected        = {92, 200, 217, 255},
        .selectedPos     = {99, 199, 99, 255},
        .text            = {244, 244, 248, 255},
        .textDim         = {169, 171, 192, 255},
        .textOnBadge     = {28, 29, 41, 255},
        .boxName         = {244, 244, 248, 255},
        .arrow           = {169, 171, 192, 255},
        .statusText      = {244, 244, 248, 255},
        .red             = {232, 90, 90, 255},
        .shiny           = {255, 210, 31, 255},
        .goldLabel       = {255, 210, 31, 255},
        .genderMale      = {100, 160, 255, 255},
        .genderFemale    = {255, 130, 160, 255},
        .overlay         = {0, 0, 0, 160},
        .overlayDark     = {0, 0, 0, 187},
        .menuHighlight   = {47, 48, 68, 255},
        .iconPlaceholder = {61, 64, 88, 255},
        .popupBorder     = {52, 54, 74, 255},
        .creditsText     = {140, 143, 165, 255},
        .boxPreviewBg    = {38, 39, 54, 235},
        .miniCellEmpty   = {32, 33, 45, 200},
        .miniCellFull    = {52, 54, 75, 220},
        .textFieldBg     = {28, 29, 41, 255},
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
