#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <cstdint>

struct Theme {
    const char* name;

    // Core UI
    SDL_Color bg;
    SDL_Color panelBg;

    // Slots
    SDL_Color slotEmpty;
    SDL_Color slotFull;
    SDL_Color slotEgg;

    // Selection
    SDL_Color selected;
    SDL_Color selectedPos;

    // Text
    SDL_Color text;
    SDL_Color textDim;
    SDL_Color textOnBadge;

    // Status & Accent
    SDL_Color statusText;
    SDL_Color red;
    SDL_Color goldLabel;       // the GTS motif's dotted routes

    // Gender
    SDL_Color genderMale;
    SDL_Color genderFemale;

    // Search Highlight
    SDL_Color searchMatch;
    SDL_Color searchDim;

    // LGPE party marker
    SDL_Color partyMark;

    // --- UI 2.0 roles ----------------------------------------------------------

    SDL_Color accent;          // top rule, logo, cursor ring, active page dot
    SDL_Color accentSave;      // the save panel's tag and page dot
    SDL_Color accentBank;      // the bank panel's tag and page dot
    SDL_Color panelBorder;     // 1px rim around panels and the info strip
    SDL_Color divider;         // vertical rules, footer rule
    SDL_Color textMuted;       // quieter than textDim: counts, version

    SDL_Color cellBorder;      // rim of a filled slot
    SDL_Color cellCursor;      // filled slot under the cursor
    SDL_Color cellEmptyBorder; // dashed rim of an empty slot
    SDL_Color dot;             // inactive page dot

    SDL_Color buttonBg;        // L/R keys, move pills
    SDL_Color buttonBorder;
    SDL_Color keyCap;          // footer button glyphs
    SDL_Color keyCapText;

    SDL_Color statusOk;        // "save loaded, no changes"
    SDL_Color statusWarn;      // "unsaved changes"
    SDL_Color alphaMark;
    SDL_Color badgeBg;         // "3 x 31" perfect-IV pill

    // Box overview mini grid, one dot per slot
    SDL_Color miniDotEmpty;
    SDL_Color miniDotFull;
    SDL_Color eggMark;
};

// The table and the selector are what let a theme be added without touching
// the screens: 0 is the pkHouse dark design, 1 the light pkHome.
inline constexpr int THEME_COUNT = 2;

const Theme& getTheme(int index);
const char*  getThemeName(int index);

int  loadThemeIndex(const std::string& basePath);
void saveThemeIndex(const std::string& basePath, int index);
