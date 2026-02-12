#pragma once
#include "save_file.h"
#include "bank.h"
#include "bank_manager.h"
#include "account.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <unordered_map>
#include <vector>

// Which panel the cursor is on
enum class Panel { Game, Bank };

// App-level screen state
enum class AppScreen { ProfileSelector, GameSelector, BankSelector, MainView };

// Purpose of text input popup
enum class TextInputPurpose { CreateBank, RenameBank };

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
    bool init();
    void shutdown();
    void showSplash();
    void showMessageAndWait(const std::string& title, const std::string& body);
    void showWorking(const std::string& msg);
    void run(const std::string& basePath, const std::string& savePath);

private:
    SDL_Window*          window_    = nullptr;
    SDL_Renderer*        renderer_  = nullptr;
    SDL_GameController*  pad_       = nullptr;
    TTF_Font*            font_      = nullptr;
    TTF_Font*            fontSmall_ = nullptr;
    TTF_Font*            fontLarge_ = nullptr;

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
    static constexpr SDL_Color COLOR_RED         = {220, 60, 60, 255};
    static constexpr SDL_Color COLOR_SELECTED    = {100, 200, 220, 255};

    // App screen state
    AppScreen screen_ = AppScreen::GameSelector;
    std::string basePath_;
    std::string savePath_;

    // Account manager
    AccountManager account_;

    // Profile selector state
    int profileSelCursor_ = 0;
    int selectedProfile_ = -1;

    // Game selector state
    GameType selectedGame_ = GameType::ZA;
    int gameSelCursor_ = 0;
    std::vector<GameType> availableGames_;
    std::unordered_map<GameType, SDL_Texture*> gameIconCache_;
    void loadGameIcons();
    void freeGameIcons();

    // Owned save + bank manager (initialized after game selection)
    SaveFile save_;
    BankManager bankManager_;
    Bank bank_;
    std::string activeBankName_;
    std::string activeBankPath_;

    // Bank selector state
    int  bankSelCursor_ = 0;
    int  bankSelScroll_ = 0;
    bool showDeleteConfirm_ = false;

    // Text input (PC only; Switch uses swkbd)
    bool showTextInput_ = false;
    TextInputPurpose textInputPurpose_;
    std::string textInputBuffer_;
    int textInputCursorPos_ = 0;
    std::string renamingBankName_;

    // Main view state
    Cursor cursor_;
    int    gameBox_ = 0;
    int    bankBox_ = 0;
    bool   showDetail_ = false;
    bool   showMenu_   = false;
    int    menuSelection_ = 0;
    bool   saveNow_    = false;
    bool   showAbout_  = false;
    bool   holding_    = false;
    Pokemon heldPkm_;
    Panel   heldSource_ = Panel::Game;
    int     heldBox_    = 0;
    int     heldSlot_   = 0;

    // Multi-select state
    std::vector<int> selectedSlots_;           // selected slot indices in selection order
    Panel         selectedPanel_ = Panel::Game;
    int           selectedBox_   = 0;
    std::vector<Pokemon> heldMulti_;           // multi-held Pokemon
    std::vector<int>     heldMultiSlots_;      // original slot indices (for cancel)
    Panel         heldMultiSource_ = Panel::Game;
    int           heldMultiBox_    = 0;

    // Sprites
    SDL_Texture* getSprite(uint16_t nationalId);
    void freeSprites();

    // Profile selector
    void drawProfileSelectorFrame();
    void handleProfileSelectorInput(bool& running);
    void selectProfile(int index);

    // Game selector
    void drawGameSelectorFrame();
    void handleGameSelectorInput(bool& running);
    void selectGame(GameType game);
    std::string buildBackupDir(GameType game) const;

    // Bank selector
    void drawBankSelectorFrame();
    void handleBankSelectorInput(bool& running);
    void openSelectedBank();
    void drawDeleteConfirmPopup();
    void handleDeleteConfirmEvent(const SDL_Event& event);
    void drawTextInputPopup();
    void handleTextInputEvent(const SDL_Event& event);
    void beginTextInput(TextInputPurpose purpose);
    void commitTextInput(const std::string& text);

    // Rendering helpers
    void drawFrame();
    void drawDetailPopup(const Pokemon& pkm);
    void drawMenuPopup();
    void drawAboutPopup();
    void drawPanel(int panelX, const std::string& boxName, int boxIdx,
                   int totalBoxes, bool isActive, SaveFile* save, Bank* bank, int box,
                   Panel panelId);
    void drawSlot(int x, int y, const Pokemon& pkm, bool isCursor, int selectOrder);
    void drawText(const std::string& text, int x, int y, SDL_Color color, TTF_Font* f);
    void drawTextCentered(const std::string& text, int cx, int cy, SDL_Color color, TTF_Font* f);
    void drawRect(int x, int y, int w, int h, SDL_Color color);
    void drawRectOutline(int x, int y, int w, int h, SDL_Color color, int thickness);
    void drawStatusBar(const std::string& msg);

    // Input handling
    void handleInput(bool& running);
    void moveCursor(int dx, int dy);
    void switchBox(int direction);
    void actionSelect();
    void actionCancel();
    void toggleSelect();
    void clearSelection();

    // Get pokemon at cursor from the appropriate source
    Pokemon getPokemonAt(int box, int slot, Panel panel) const;
    void setPokemonAt(int box, int slot, Panel panel, const Pokemon& pkm);
    void clearPokemonAt(int box, int slot, Panel panel);
};
