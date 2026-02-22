#pragma once
#include "save_file.h"
#include "bank.h"
#include "bank_manager.h"
#include "account.h"
#include "theme.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <unordered_set>

// Which panel the cursor is on
enum class Panel { Game, Bank };

// App-level screen state
enum class AppScreen { ProfileSelector, GameSelector, BankSelector, MainView };

// Purpose of text input popup
enum class TextInputPurpose {
    CreateBank, RenameBank,
    SearchSpecies, SearchOT, SearchLevelMin, SearchLevelMax
};

// Search filter enums
enum class GenderFilter { Any, Male, Female, Genderless };
enum class PerfectIVFilter { Off, AtLeastOne, All6 };
enum class SearchMode { List, Highlight };

// Search filter criteria
struct SearchFilter {
    std::string speciesName;
    std::string otName;
    bool filterShiny  = false;
    bool filterEgg    = false;
    bool filterAlpha  = false;
    GenderFilter gender = GenderFilter::Any;
    PerfectIVFilter perfectIVs = PerfectIVFilter::Off;
    int levelMin = 0;
    int levelMax = 0;
    SearchMode mode = SearchMode::List;
};

// Search result entry
struct SearchResult {
    Panel panel;
    int box;
    int slot;
    std::string speciesName;
    uint8_t level;
    bool isShiny;
    bool isEgg;
    bool isAlpha;
    uint8_t gender;
    std::string otName;
};

// Cursor position within the two-panel display
struct Cursor {
    Panel panel = Panel::Game;
    int box     = 0;
    int col     = 0; // 0-5 (or 0-4 for LGPE)
    int row     = 0; // 0-4

    int slot(int cols = 6) const { return row * cols + col; }
};

// Main UI class - manages rendering and input for the two-panel box viewer.
class UI {
public:
    bool init();
    void shutdown();
    void showSplash();
    void showMessageAndWait(const std::string& title, const std::string& body);
    bool showConfirmDialog(const std::string& title, const std::string& body);
    void showWorking(const std::string& msg);
    void setAppletMode(bool mode) { appletMode_ = mode; }
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

    // Box view overlay layout
    static constexpr int BV_COLS         = 8;
    static constexpr int BV_CELL_W       = 140;
    static constexpr int BV_CELL_H       = 32;
    static constexpr int BV_CELL_PAD     = 4;
    static constexpr int BV_MINI_SPRITE  = 32;
    static constexpr int BV_MINI_CELL    = 36;
    static constexpr int BV_MINI_PAD     = 2;
    static constexpr int BV_PREVIEW_PAD  = 8;
    static constexpr int BV_PREVIEW_HDR  = 22;

    // Theme
    int themeIndex_ = 0;
    const Theme* theme_ = nullptr;
    const Theme& T() const { return *theme_; }

    // Theme selector state
    bool showThemeSelector_ = false;
    int  themeSelCursor_    = 0;
    int  themeSelOriginal_  = 0;

    // Search/Filter state
    bool showSearchFilter_  = false;
    bool showSearchResults_ = false;
    SearchFilter searchFilter_;
    std::vector<SearchResult> searchResults_;
    int  searchFilterCursor_ = 0;
    int  searchLevelFocus_   = 0;   // 0=min, 1=max
    int  searchResultCursor_ = 0;
    int  searchResultScroll_ = 0;
    bool searchHighlightActive_ = false;
    std::unordered_set<uint64_t> searchMatchSet_;

    // Joystick navigation
    static constexpr int16_t STICK_DEADZONE = 16000;
    static constexpr uint32_t STICK_INITIAL_DELAY = 400; // ms before first repeat
    static constexpr uint32_t STICK_REPEAT_DELAY  = 200; // ms between repeats
    int stickDirX_ = 0;  // -1, 0, +1
    int stickDirY_ = 0;
    uint32_t stickMoveTime_ = 0; // last move timestamp
    bool stickMoved_ = false;    // has initial move fired?
    void updateStick(int16_t axisX, int16_t axisY);

    // App screen state
    AppScreen screen_ = AppScreen::GameSelector;
    bool appletMode_ = false;
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

    // Dual-bank state (applet mode: left panel shows bankLeft_ instead of save_)
    Bank bankLeft_;
    std::string leftBankName_;
    std::string leftBankPath_;
    Panel bankSelTarget_ = Panel::Bank;

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
    bool   showBoxView_   = false;
    Panel  boxViewPanel_  = Panel::Game;
    int    boxViewCursor_ = 0;
    bool   zlPressed_     = false;
    bool   zrPressed_     = false;
    bool   holding_    = false;
    Pokemon heldPkm_;

    // Swap history for full undo on cancel
    struct SwapRecord {
        Pokemon pkm;
        Panel   panel;
        int     box;
        int     slot;
    };
    std::vector<SwapRecord> swapHistory_;

    // Multi-select state
    std::vector<int> selectedSlots_;           // selected slot indices in selection order
    Panel         selectedPanel_ = Panel::Game;
    int           selectedBox_   = 0;
    std::vector<Pokemon> heldMulti_;           // multi-held Pokemon
    std::vector<int>     heldMultiSlots_;      // original slot indices (for cancel)
    Panel         heldMultiSource_ = Panel::Game;
    int           heldMultiBox_    = 0;
    bool          positionPreserve_ = false; // place at original slot positions

    // Rectangle drag-select state
    bool     yHeld_         = false;
    bool     yDragActive_   = false;
    int      dragAnchorCol_  = 0;
    int      dragAnchorRow_  = 0;
    Panel    dragPanel_      = Panel::Game;
    int      dragBox_         = 0;
    uint32_t lastYTapTime_   = 0;  // for double-tap Y detection
    static constexpr uint32_t DOUBLE_TAP_MS = 300;

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
    bool saveBankFiles();

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
    void drawThemeSelectorPopup();
    void drawSearchFilterPopup();
    void drawSearchResultsPopup();
    void drawHeldOverlay();
    void drawBoxViewOverlay();
    void drawBoxPreview(int boxIdx, int anchorX, int anchorY);
    void drawPanel(int panelX, const std::string& boxName, int boxIdx,
                   int totalBoxes, bool isActive, SaveFile* save, Bank* bank, int box,
                   Panel panelId);
    void drawSlot(int x, int y, const Pokemon& pkm, bool isCursor, int selectOrder,
                  int highlightState = 0);
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
    void beginYPress();
    void endYPress();
    void updateDragSelection();
    void selectAll();
    void handleSearchFilterInput(const SDL_Event& event);
    void handleSearchResultsInput(const SDL_Event& event);
    void executeSearch();
    bool isSearchMatch(Panel panel, int box, int slot) const;
    void clearSearchHighlight();
    void handleBoxViewInput(const SDL_Event& event);
    void moveBoxViewCursor(int dx, int dy);
    void openBoxView(Panel panel);
    void closeBoxView(bool navigate);

    // Dynamic grid: LGPE has 5 columns (5x5), others have 6 (6x5)
    int gridCols() const { return isLGPE(selectedGame_) ? 5 : 6; }
    int maxSlots() const { return gridCols() * 5; }

    // Get pokemon at cursor from the appropriate source
    Pokemon getPokemonAt(int box, int slot, Panel panel) const;
    void setPokemonAt(int box, int slot, Panel panel, const Pokemon& pkm);
    void clearPokemonAt(int box, int slot, Panel panel);
};
