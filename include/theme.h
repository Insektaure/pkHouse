#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <cstdint>

struct Theme {
    const char* name;

    // Core UI
    SDL_Color bg;
    SDL_Color panelBg;
    SDL_Color statusBarBg;

    // Slots
    SDL_Color slotEmpty;
    SDL_Color slotFull;
    SDL_Color slotEgg;

    // Cursor & Selection
    SDL_Color cursor;
    SDL_Color selected;
    SDL_Color selectedPos;

    // Text
    SDL_Color text;
    SDL_Color textDim;
    SDL_Color textOnBadge;

    // Box & Navigation
    SDL_Color boxName;
    SDL_Color arrow;

    // Status & Accent
    SDL_Color statusText;
    SDL_Color red;
    SDL_Color shiny;
    SDL_Color goldLabel;

    // Gender
    SDL_Color genderMale;
    SDL_Color genderFemale;

    // Overlays
    SDL_Color overlay;
    SDL_Color overlayDark;

    // Menu / Selectors
    SDL_Color menuHighlight;
    SDL_Color iconPlaceholder;

    // Popups
    SDL_Color popupBorder;
    SDL_Color creditsText;

    // Box View
    SDL_Color boxPreviewBg;
    SDL_Color miniCellEmpty;
    SDL_Color miniCellFull;

    // Text Input
    SDL_Color textFieldBg;
};

inline constexpr int THEME_COUNT = 7;

const Theme& getTheme(int index);
const char*  getThemeName(int index);

int  loadThemeIndex(const std::string& basePath);
void saveThemeIndex(const std::string& basePath, int index);
