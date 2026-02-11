#pragma once
#include "save_file.h"
#include "bank.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <unordered_map>

// Which panel the cursor is on
enum class Panel { Game, Bank };

// Cursor position within the two-panel display
struct Cursor {
    Panel panel = Panel::Game;
    int box     = 0;
    int col     = 0; // 0-5
    int row     = 0; // 0-4

    int slot() const { return row * 6 + col; }
};

// Main UI class - manages rendering and input for the two-panel box viewer.
class UI {
public:
    // Returns true if user chose "save & exit", false for "quit without saving".
    bool init();
    void shutdown();
    bool run(SaveFile& save, Bank& bank);

private:
    SDL_Window*          window_    = nullptr;
    SDL_Renderer*        renderer_  = nullptr;
    SDL_GameController*  pad_       = nullptr;
    TTF_Font*            font_      = nullptr;
    TTF_Font*            fontSmall_ = nullptr;

    // Sprite cache: national dex ID -> texture
    std::unordered_map<uint16_t, SDL_Texture*> spriteCache_;
    SDL_Texture* eggSprite_ = nullptr;

    // Status icons
    SDL_Texture* iconShiny_      = nullptr;
    SDL_Texture* iconAlpha_      = nullptr;
    SDL_Texture* iconShinyAlpha_ = nullptr;

    // Screen dimensions (Switch: 1280x720)
    static constexpr int SCREEN_W = 1280;
    static constexpr int SCREEN_H = 720;

    // Layout
    static constexpr int PANEL_W   = 610;
    static constexpr int PANEL_X_L = 15;
    static constexpr int PANEL_X_R = 655;
    static constexpr int BOX_HDR_Y = 10;
    static constexpr int BOX_HDR_H = 40;
    static constexpr int GRID_Y    = 55;

    // Grid cells: 6 cols x 5 rows
    static constexpr int CELL_W   = 96;
    static constexpr int CELL_H   = 120;
    static constexpr int CELL_PAD = 5;

    // Sprite size within a cell
    static constexpr int SPRITE_SIZE = 68;

    // Status bar
    static constexpr int STATUS_BAR_H = 40;

    // Colors
    static constexpr SDL_Color COLOR_BG         = {30, 30, 40, 255};
    static constexpr SDL_Color COLOR_PANEL_BG   = {45, 45, 60, 255};
    static constexpr SDL_Color COLOR_SLOT_EMPTY  = {60, 60, 80, 255};
    static constexpr SDL_Color COLOR_SLOT_FULL   = {70, 90, 120, 255};
    static constexpr SDL_Color COLOR_SLOT_EGG    = {90, 80, 70, 255};
    static constexpr SDL_Color COLOR_CURSOR      = {255, 220, 50, 255};
    static constexpr SDL_Color COLOR_TEXT        = {240, 240, 240, 255};
    static constexpr SDL_Color COLOR_TEXT_DIM    = {160, 160, 170, 255};
    static constexpr SDL_Color COLOR_SHINY       = {255, 215, 0, 255};
    static constexpr SDL_Color COLOR_BOX_NAME    = {200, 200, 220, 255};
    static constexpr SDL_Color COLOR_ARROW       = {180, 180, 200, 255};
    static constexpr SDL_Color COLOR_STATUS      = {140, 200, 140, 255};

    // State
    Cursor cursor_;
    int    gameBox_ = 0;
    int    bankBox_ = 0;
    bool   holding_    = false;
    Pokemon heldPkm_;
    Panel   heldSource_ = Panel::Game;
    int     heldBox_    = 0;
    int     heldSlot_   = 0;

    // Sprites
    SDL_Texture* getSprite(uint16_t nationalId);
    void freeSprites();

    // Rendering helpers
    void drawFrame(SaveFile& save, Bank& bank);
    void drawPanel(int panelX, const std::string& boxName, int boxIdx,
                   int totalBoxes, bool isActive, SaveFile* save, Bank* bank, int box);
    void drawSlot(int x, int y, const Pokemon& pkm, bool isCursor);
    void drawText(const std::string& text, int x, int y, SDL_Color color, TTF_Font* f);
    void drawTextCentered(const std::string& text, int cx, int cy, SDL_Color color, TTF_Font* f);
    void drawRect(int x, int y, int w, int h, SDL_Color color);
    void drawRectOutline(int x, int y, int w, int h, SDL_Color color, int thickness);
    void drawStatusBar(const std::string& msg);

    // Input handling
    void handleInput(SaveFile& save, Bank& bank, bool& running, bool& shouldSave);
    void moveCursor(int dx, int dy);
    void switchBox(int direction);
    void actionSelect(SaveFile& save, Bank& bank);
    void actionCancel(SaveFile& save, Bank& bank);

    // Get pokemon at cursor from the appropriate source
    Pokemon getPokemonAt(int box, int slot, Panel panel, SaveFile& save, Bank& bank) const;
    void setPokemonAt(int box, int slot, Panel panel, const Pokemon& pkm, SaveFile& save, Bank& bank);
    void clearPokemonAt(int box, int slot, Panel panel, SaveFile& save, Bank& bank);
};
