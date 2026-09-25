#pragma once
#include "save_file.h"
#include "bank.h"
#include "bank_manager.h"
#include "account.h"
#include "theme.h"
#include "wondercard.h"
#include "card_import.h"
#include "gts.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <functional>
#include <ctime>

// Draws `tex` into a disc of radius `rad` centred on (cx, cy), masking the
// corners with `bg` - the colour behind it - with an anti-aliased rim. Used for
// the type icons in the box view info strip and the summary. The card export
// keeps its own copy (ui_card.cpp) so its output never depends on this one.
void blitDisc(SDL_Renderer* r, SDL_Texture* tex, int cx, int cy, int rad, SDL_Color bg);

// What a message is about, for its icon (UI 2.0 dialogs, source/ui_dialog.cpp).
enum class DialogKind { Info, Success, Error };

// How a confirmation looks and asks (8a / 8b). The defaults are a plain
// "B Cancel / A Confirm" question.
struct ConfirmStyle {
    const char* confirmKey = nullptr;  // label of the A button; "Confirm" when null
    bool danger  = false;              // red: something is lost
    bool hold    = false;              // A must be held for UI::HOLD_MS (Pokemon would be lost)
    bool warning = false;              // amber warning icon rather than the info one
    std::string note;                  // red line above the buttons
    // The thing being acted on, drawn in a row under the text (a bank, a
    // Pokemon). `objectH` is its height; nothing is drawn when it is 0.
    int objectH = 0;
    std::function<void(int x, int y, int w)> drawObject;
};

// Which panel the cursor is on. Preview is never under the cursor: it is the
// bank picker's look at a bank that has not been opened (UI::previewBank_).
enum class Panel { Game, Bank, Preview };

// App-level screen state
enum class AppScreen {
    ProfileSelector, GameSelector, BankSelector, MainView,
    GtsHub, GtsBrowse
};

// Purpose of text input popup
enum class TextInputPurpose {
    CreateBank, RenameBank, RenameBoxName,
    SearchSpecies, SearchOT, SearchLevelMin, SearchLevelMax
};

// Search filter enums
enum class GenderFilter { Any, Male, Female, Genderless };
enum class PerfectIVFilter { Off, AtLeastOne, All6 };
enum class RibbonFilter { Off, HasRibbon, HasMark, HasAny };
enum class SearchMode { List, Highlight };

// Search filter criteria
struct SearchFilter {
    uint16_t speciesId = 0;       // national dex ID (0 = any)
    std::string speciesName;
    std::string otName;
    bool filterShiny  = false;
    bool filterEgg    = false;
    bool filterAlpha  = false;
    GenderFilter gender = GenderFilter::Any;
    PerfectIVFilter perfectIVs = PerfectIVFilter::Off;
    RibbonFilter ribbonFilter = RibbonFilter::Off;
    int levelMin = 0;
    int levelMax = 0;
    SearchMode mode = SearchMode::Highlight;
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
    // Modal, over the current screen dimmed. A (or B) closes a message; a
    // confirmation returns true on A - once held long enough when style.hold.
    void showMessageAndWait(const std::string& title, const std::string& body,
                            DialogKind kind = DialogKind::Error);
    bool showConfirmDialog(const std::string& title, const std::string& body,
                           const ConfirmStyle& style = ConfirmStyle());
    // A blocking operation is running: a dialog over the current screen,
    // dimmed (8b "blocking · progress"). `writing` adds the "don't close"
    // warning, for anything that writes to the SD card; `progress` in 0..1
    // adds a bar, for work that knows how far along it is.
    void showWorking(const std::string& msg, bool writing = false, float progress = -1.0f);
    bool screenDrawn_ = false;   // a screen has been shown: something to dim
    void setAppletMode(bool mode) { appletMode_ = mode; }
    bool isDualBankMode() const { return appletMode_ || allBanksMode_; }
    void run(const std::string& basePath, const std::string& savePath);

private:
    SDL_Window*          window_    = nullptr;
    SDL_Renderer*        renderer_  = nullptr;
    SDL_GameController*  pad_       = nullptr;
    TTF_Font*            font_      = nullptr;
    TTF_Font*            fontSmall_ = nullptr;
    TTF_Font*            fontLarge_ = nullptr;

    // Shared system font bytes, kept so the card renderer can open its own
    // sizes on demand without re-querying the pl service.
    void*                fontData_     = nullptr;
    size_t               fontDataSize_ = 0;

    // Sprite cache: (national dex ID | form << 16) -> texture
    std::unordered_map<uint32_t, SDL_Texture*> spriteCache_;
    std::unordered_map<uint32_t, SDL_Texture*> shinySpriteCache_;

    // Ribbon sprite cache: filename -> texture
    std::unordered_map<std::string, SDL_Texture*> ribbonSpriteCache_;
    SDL_Texture* getRibbonSprite(const std::string& filename);

    // Ball sprite cache: ball ID -> texture
    std::unordered_map<uint8_t, SDL_Texture*> ballSpriteCache_;
    SDL_Texture* getBallSprite(uint8_t ballId);

    // Type icon cache: type ID -> texture
    std::unordered_map<uint8_t, SDL_Texture*> typeSpriteCache_;
    SDL_Texture* getTypeSprite(uint8_t typeId);

    // Text texture cache: avoids re-rasterising identical strings every frame
    struct TextCacheKey {
        std::string text;
        TTF_Font*   font;
        uint32_t    colorVal; // packed RGBA
        bool operator==(const TextCacheKey& o) const {
            return text == o.text && font == o.font && colorVal == o.colorVal;
        }
    };
    struct TextCacheKeyHash {
        size_t operator()(const TextCacheKey& k) const {
            size_t h = std::hash<std::string>{}(k.text);
            h ^= std::hash<uintptr_t>{}(reinterpret_cast<uintptr_t>(k.font)) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<uint32_t>{}(k.colorVal) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
    struct TextCacheEntry {
        SDL_Texture* tex;
        int w, h;
    };
    std::unordered_map<TextCacheKey, TextCacheEntry, TextCacheKeyHash> textCache_;
    const TextCacheEntry& getTextEntry(const std::string& text, TTF_Font* f, SDL_Color color);
    void clearTextCache();

    // Extra sizes of the system font for the 2.0 screens, opened on first use.
    // Bold is TTF's synthetic bold on the same face: the shared font has no
    // bold cut, and a bundled one would lose the Japanese glyphs.
    std::unordered_map<int, TTF_Font*> uiFonts_;
    TTF_Font* uiFont(int size, bool bold = false);
    void closeUiFonts();

    int  textWidth(const std::string& text, TTF_Font* f);
    // `text` cut at a code point boundary and ended with "..." so it fits in
    // maxW; returned unchanged when it already fits.
    std::string fitText(const std::string& text, TTF_Font* f, int maxW);

    // Text with extra space between letters, for the small caps labels. Drawn
    // one code point at a time, so it is for short strings only.
    int  drawTextTracked(const std::string& text, int x, int y, SDL_Color color,
                         TTF_Font* f, int tracking);
    int  measureTextTracked(const std::string& text, TTF_Font* f, int tracking);

    // --- Shapes (source/ui_shapes.cpp) ---------------------------------------

    // Anti-aliased quarter discs and quarter rings, white, one per radius and
    // stroke width, tinted with a colour mod when drawn.
    std::unordered_map<uint32_t, SDL_Texture*> cornerCache_;
    static constexpr int CORNER_MASK = 0xFFFF;   // stroke value: outside of the arc
    SDL_Texture* cornerTexture(int radius, int stroke);
    void freeShapeCache();

    void fillRounded(int x, int y, int w, int h, int radius, SDL_Color c);
    void strokeRounded(int x, int y, int w, int h, int radius, int stroke, SDL_Color c);
    void dashRounded(int x, int y, int w, int h, int radius, SDL_Color c,
                       int dash = 4, int gap = 3);
    void fillDisc(int cx, int cy, int radius, SDL_Color c);
    // `tex` scaled into the rect with rounded corners, masked in `bg` - the
    // colour behind it - the way blitDisc masks a disc.
    void blitRounded(SDL_Texture* tex, int x, int y, int w, int h, int radius, SDL_Color bg);
    enum class ArrowDir { Left, Right, Up, Down };
    // A small solid triangle centred on (cx, cy), `size` long and wide.
    void fillArrow(float cx, float cy, float size, ArrowDir dir, SDL_Color c);

    // Status icons
    SDL_Texture* iconShiny_      = nullptr;
    SDL_Texture* iconAlpha_      = nullptr;
    SDL_Texture* iconShinyAlpha_ = nullptr;
    SDL_Texture* iconDynamax_    = nullptr;
    SDL_Texture* iconHouse_      = nullptr;
    SDL_Texture* iconGlobe_      = nullptr;
    SDL_Texture* iconBank_       = nullptr;
    SDL_Texture* iconCheck_      = nullptr;


    // Screen dimensions (Switch: 1280x720)
    static constexpr int SCREEN_W = 1280;
    static constexpr int SCREEN_H = 720;

    // --- Box view layout (UI 2.0) ---------------------------------------------
    //
    // Measured off the mockup (external/UI_2.0), 1280x720:
    //   0..4     accent rule
    //   4..72    top bar: logo, game, trainer, save status
    //   72..572  the two box panels
    //   578..661 info strip for the Pokemon under the cursor
    //   668..720 footer: button hints and version
    // The info strip sits in the middle of the space between the panels and
    // the footer, so it has the same gap above and below.
    static constexpr int ACCENT_RULE_H = 4;
    static constexpr int TOPBAR_H      = 72;

    static constexpr int PANEL_Y   = 72;
    static constexpr int PANEL_H   = 500;
    static constexpr int FOOTER_Y  = 668;
    static constexpr int INFO_H    = 83;
    static constexpr int INFO_Y    = PANEL_Y + PANEL_H
                                   + (FOOTER_Y - (PANEL_Y + PANEL_H) - INFO_H) / 2;
    static constexpr int PANEL_W   = 596;
    static constexpr int PANEL_X_L = 32;
    static constexpr int PANEL_X_R = 652;
    static constexpr int PANEL_RADIUS = 14;

    // Grid cells: 6 cols x 5 rows (5 x 5 for LGPE), centred in the panel.
    static constexpr int GRID_Y     = PANEL_Y + 75;
    static constexpr int CELL_W     = 87;
    static constexpr int CELL_H     = 72;
    static constexpr int CELL_GAP   = 8;
    static constexpr int CELL_RADIUS = 10;

    // Sprite size within a cell
    static constexpr int SPRITE_SIZE = 56;

    // Page dots under the grid, one per box.
    static constexpr int DOTS_Y = PANEL_Y + 481;

    // Info strip
    static constexpr int INFO_X = 32;
    static constexpr int INFO_W = 1216;

    // Footer
    static constexpr int FOOTER_H = SCREEN_H - FOOTER_Y;

    // Legacy status bar height, still used by the screens not yet redone.
    static constexpr int STATUS_BAR_H = 40;

    // Box overview (ZL/ZR), UI 2.0. Eight cards a row; with more than four
    // rows (40-box games) the cards get shorter so every box stays on screen.
    static constexpr int BV_COLS       = 8;
    static constexpr int BV_TOP        = 76;
    static constexpr int BV_BOTTOM     = 640;
    static constexpr int BV_CARD_W     = 141;
    static constexpr int BV_CARD_H     = 131;
    static constexpr int BV_ROW_GAP    = 13;
    static constexpr int BV_DOT_PITCH  = 14;   // 10px dot + 4px gap, at full height

    // Preview of the highlighted box, with sprites
    static constexpr int BV_MINI_SPRITE  = 32;
    static constexpr int BV_MINI_CELL    = 36;
    static constexpr int BV_MINI_PAD     = 3;
    static constexpr int BV_PREVIEW_PAD  = 10;
    static constexpr int BV_PREVIEW_HDR  = 30;

    // Theme
    int themeIndex_ = 0;
    const Theme* theme_ = nullptr;
    const Theme* lastTheme_ = nullptr; // tracks theme changes for text cache invalidation
    const Theme& T() const { return *theme_; }

    // Theme selector state
    bool showThemeSelector_ = false;
    int  themeSelCursor_    = 0;
    int  themeSelOriginal_  = 0;

    // Language selector state
    bool showLanguageSelector_ = false;
    int  langSelCursor_        = 0;
    std::vector<std::string> langList_;

    // Wondercard list state
    bool showWondercardList_ = false;
    int  wcListCursor_  = 0;
    int  wcListScroll_  = 0;
    std::vector<WCInfo> wcList_;

    // Card import state
    bool showCardList_ = false;
    int  cardListCursor_ = 0;
    int  cardListScroll_ = 0;
    std::vector<CardFile> cardList_;

    // Decoded summary of the highlighted card. Reading a QR code costs enough
    // that it only happens once the cursor has stopped moving, and only over
    // the region a card keeps its code in.
    CardPayload::Parsed cardPreview_;
    int          cardPreviewIdx_ = -1;   // which row cardPreview_ belongs to
    uint32_t     cardPreviewSince_ = 0;  // when the cursor last landed
    static constexpr uint32_t CARD_PREVIEW_DELAY_MS = 200;

    // --- Online GTS ----------------------------------------------------------

    // Cursor is on the "Online GTS" tile above the game grid.
    bool gameSelOnGts_ = false;

    // Hub: 0 = browse, 1 = search, 2 = deposit.
    int  gtsHubCursor_ = 0;

    // The 60-slot browse grid. One page is exactly one request, which is why
    // PAGE_SIZE and the grid size are the same number.
    static constexpr int GTS_COLS = 10;
    static constexpr int GTS_ROWS = 6;
    static constexpr int GTS_PER_PAGE = GTS_COLS * GTS_ROWS;
    static_assert(GTS_PER_PAGE == Gts::PAGE_SIZE,
                  "the browse grid and a server page must hold the same number");

    // Sized so six rows plus one line of detail clear the status bar. The two
    // pixels a row gives up buy the line: the grid cannot tell you whether a
    // Pokemon is legal, and that is the thing most worth knowing before
    // spending a download on it.
    static constexpr int GTS_CELL_W = 118;
    static constexpr int GTS_CELL_H = 94;
    static constexpr int GTS_CELL_PAD = 4;
    static constexpr int GTS_SPRITE = 54;
    static constexpr int GTS_GRID_TOP = 58;

    Gts::Filter gtsFilter_;
    Gts::Page   gtsPage_;
    int  gtsCursor_ = 0;          // 0..59 within the page on screen
    int  gtsOffset_ = 0;          // that page's offset on the board

    // Detail popup over the grid. The Pokemon is fetched on open, because the
    // listing carries only what the grid draws.
    bool     gtsDetail_ = false;
    Pokemon  gtsDetailPkm_;
    GameType gtsDetailGame_ = GameType::ZA;
    std::string gtsDetailId_;        // the listing gtsDetailPkm_ came from

    // Everything fetched while this page has been on screen, by listing id.
    //
    // Opening the same listing twice should not ask the board twice: it is the
    // same bytes, the wait is the only thing that changes, and a second request
    // would be a second entry in the board's logs for one person looking once.
    // Dropped when the page changes, so it never grows past 60.
    std::unordered_map<std::string, Pokemon> gtsFetched_;
    std::unordered_map<std::string, GameType> gtsFetchedGame_;

    // Listings already counted as a download this session.
    std::unordered_set<std::string> gtsCounted_;

    // Deposit picker: cards from every family folder at once, since no game
    // has been chosen at this point in the flow.
    struct GtsCard {
        CardFile file;
        GameType folderGame;   // which family folder it was found in
    };
    std::vector<GtsCard> gtsCards_;
    bool showGtsDeposit_   = false;
    int  gtsCardCursor_    = 0;
    int  gtsCardScroll_    = 0;
    CardPayload::Parsed gtsCardPreview_;
    int      gtsCardPreviewIdx_   = -1;
    uint32_t gtsCardPreviewSince_ = 0;

    // Search filter popup, shown over the hub. The row count is here rather
    // than beside the popup's own enum because handleStickRepeat() scrolls the
    // same list from another translation unit.
    static constexpr int GTS_FILTER_ROWS = 9;
    bool showGtsFilter_   = false;
    int  gtsFilterCursor_ = 0;

    // The species picker is shared with the local box search; this says which
    // filter a confirmed pick belongs to.
    bool speciesPickerForGts_ = false;

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

    // Species picker state (letter → species list)
    bool showSpeciesLetterPicker_ = false;
    bool showSpeciesListPicker_   = false;
    int  speciesLetterCursor_ = 0;   // 0="-", 1-26=A-Z
    int  speciesLetterScroll_ = 0;
    int  speciesListCursor_   = 0;
    int  speciesListScroll_   = 0;
    std::vector<uint16_t> availableSpecies_;    // all species for current game
    std::vector<uint16_t> speciesPickerList_;   // species filtered by letter

    // Joystick navigation
    static constexpr int16_t STICK_DEADZONE   = 16000;
    static constexpr int16_t TRIGGER_DEADZONE = 8000;
    static constexpr uint32_t STICK_INITIAL_DELAY = 400; // ms before first repeat
    static constexpr uint32_t STICK_REPEAT_DELAY  = 200; // ms between repeats
    int stickDirX_ = 0;  // -1, 0, +1
    int stickDirY_ = 0;
    uint32_t stickMoveTime_ = 0; // last move timestamp
    bool stickMoved_ = false;    // has initial move fired?
    void updateStick(int16_t axisX, int16_t axisY);

    // L/R shoulder button repeat
    static constexpr uint32_t BUMPER_INITIAL_DELAY = 400;
    static constexpr uint32_t BUMPER_REPEAT_DELAY  = 200;
    bool lHeld_ = false;
    bool rHeld_ = false;
    uint32_t bumperRepeatTime_ = 0;
    bool bumperMoved_ = false;

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

    // How many games each profile has a save for, shown on its card. Read
    // once, not per frame: counting opens every game's save data.
    std::vector<int> profileSaveCounts_;
    void loadProfileSaveCounts();   // shows its own progress dialog

    // Backups of one game's save for a profile: how many there are, and when
    // the newest was made (0 when there is none). Backups are per game and
    // only made when a save is opened, which is why the game selector shows
    // them and the profile cards do not.
    int backupStats(int profile, GameType game, time_t& newest) const;
    std::string relativeDay(time_t t) const;

    // Game selector state
    GameType selectedGame_ = GameType::ZA;
    int gameSelCursor_ = 0;           // index into gameSelList_
    bool gameSelOnAllBanks_ = false;  // cursor is on the "All banks" tile
    bool allBanksMode_ = false;       // entered bank selector via "View All Banks"
    std::vector<GameType> availableGames_;
    std::unordered_map<GameType, SDL_Texture*> gameIconCache_;
    std::unordered_map<GameType, int> gameBankCounts_;
    void refreshBankCounts();
    // Shows its own progress dialog.
    void loadGameIcons();
    void freeGameIcons();
    void enterAllBanksMode();

    // --- Game selector (UI 2.0) ---
    //
    // L/R step through the filters; the grid scrolls one row at a time to keep
    // the cursor in view. Filters with no game in them are not offered.
    enum GameFilter { GF_ALL, GF_RECENT, GF_GEN9, GF_GEN8, GF_GEN7, GF_GEN3, GF_COUNT };
    int gameSelFilter_ = GF_ALL;
    int gameSelScroll_ = 0;           // first grid row on screen
    std::vector<int> gameSelList_;    // indices into availableGames_, filtered
    static int gameGeneration(GameType g);
    bool gameMatchesFilter(GameType g, int filter) const;
    std::vector<int> visibleGameFilters() const;
    void rebuildGameSelList();
    void stepGameFilter(int dir);

    // What the cards and the detail panel show, read when the list is built
    // rather than per frame: it means listing directories.
    struct GameSelInfo {
        int    backups = 0;
        time_t lastBackup = 0;
        std::vector<std::string> banks;
    };
    std::unordered_map<GameType, GameSelInfo> gameSelInfo_;
    int allBanksTotal_ = 0;           // for the "All banks" tile
    int allBanksFamilies_ = 0;
    std::vector<std::pair<GameType, int>> allBanksByFamily_;  // families with banks

    void drawGameSelTopBar();
    // Logo, a page title and the step row shared by the game selector, the
    // backup screen and the bank picker. Steps before `current` are ticked.
    void drawFlowTopBar(const std::string& title, const std::vector<std::string>& steps,
                        int current);
    void drawGameSelTopRight();
    // The step row itself, centred on (centerX, cy).
    void drawSteps(const std::vector<std::string>& steps, int current, int centerX, int cy);

    void drawCheckDisc(int cx, int cy, int radius, SDL_Color disc, SDL_Color tick);
    static SDL_Color gameTint(GameType g);
    // Like wrapText, but breaks anywhere: for paths, which have no spaces.
    std::vector<std::string> wrapChars(const std::string& text, TTF_Font* f, int maxW,
                                       int maxLines);
    void drawGameSelTiles();
    void drawGameSelFilters();
    void drawGameCard(int listIdx, const SDL_Rect& r, bool isCursor);
    void drawGameDetailPanel();
    void drawGameIcon(GameType g, int x, int y, int size, int radius, SDL_Color bg);
    // Up to two lines of `text` in `maxW`, split at spaces; returns the lines.
    std::vector<std::string> wrapText(const std::string& text, TTF_Font* f, int maxW,
                                      int maxLines = 2);

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

    // A bank the picker is showing but has not opened: loaded for display only,
    // once the cursor has settled on it, into its own object so nothing that
    // is open is touched.
    Bank        previewBank_;
    std::string previewBankPath_;
    std::string previewBankName_;
    time_t      previewBankModified_ = 0;

    // When the open banks were last written, for the picker's "Last edited"
    // tile. Taken from the bank list when a bank is opened and set again when
    // pkHouse saves it, so the tile never has to ask the SD card.
    time_t      activeBankModified_ = 0;
    time_t      leftBankModified_ = 0;
    int         previewBox_ = 0;
    uint32_t    bankPreviewSince_ = 0;
    static constexpr uint32_t BANK_PREVIEW_DELAY_MS = 200;
    bool updateBankPreview();          // true when it loaded something new
    void clearBankPreview();

    // What happened to the backup when the save was opened, for the bank
    // picker's banner.
    enum class BackupOutcome { None, Saved, Skipped, Failed };
    BackupOutcome lastBackup_ = BackupOutcome::None;
    std::string   lastBackupDir_;
    time_t        lastBackupWhen_ = 0;

    // The backup screen (1b), redrawn as the steps go by. `step` is the one in
    // progress (0 open, 1 space, 2 write, 3 load); `fraction` is the write's.
    void drawBackupProgress(GameType game, const std::string& dir, int step, float fraction);

    // Bank selector state
    int  bankSelCursor_ = 0;
    int  bankSelScroll_ = 0;
    bool showDeleteConfirm_ = false;

    TextInputPurpose textInputPurpose_;
    std::string textInputBuffer_;
    int textInputCursorPos_ = 0;
    std::string renamingBankName_;
    int renamingBoxIdx_ = 0;
    Bank* renamingBoxBank_ = nullptr;

    // Pre-computed display attributes for a single slot (avoids repeated
    // species lookups, string formatting, and accessor calls during rendering).
    struct SlotDisplay {
        bool     empty    = true;
        bool     egg      = false;
        bool     shiny    = false;
        bool     alpha    = false;
        uint8_t  gender   = 2;  // 0=male, 1=female, 2=none
        uint16_t species  = 0;  // national dex ID (for sprite lookup)
        uint8_t  form     = 0;  // form index (for form-aware sprites)
        uint8_t  level    = 0;
        std::string name;       // truncated display name (≤10 chars)
    };

    // Cache of SlotDisplay per (panel, box). Invalidated on mutations.
    struct BoxDisplayKey {
        Panel panel;
        int   box;
        bool operator==(const BoxDisplayKey& o) const { return panel == o.panel && box == o.box; }
    };
    struct BoxDisplayKeyHash {
        size_t operator()(const BoxDisplayKey& k) const {
            return std::hash<int>{}(static_cast<int>(k.panel) * 10000 + k.box);
        }
    };
    std::unordered_map<BoxDisplayKey, std::vector<SlotDisplay>, BoxDisplayKeyHash> slotDisplayCache_;
    const std::vector<SlotDisplay>& getSlotDisplays(Panel panel, int box);
    void invalidateSlotDisplay(Panel panel, int box);
    void invalidateAllSlotDisplays() { slotDisplayCache_.clear(); }

    // Dirty flag: when true the next frame will be redrawn, then reset.
    // Call markDirty() from any code that changes visible state.
    bool dirty_ = true;
    void markDirty() { dirty_ = true; }

    // Something in the save or a bank differs from what is on disk. Set by
    // every write through setPokemonAt/clearPokemonAt and box renames, cleared
    // when the files are written or freshly loaded. Shown in the top bar.
    bool unsavedChanges_ = false;

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
    bool   heldFromLGPEParty_ = false;       // block save→bank moves for LGPE party
    int    lgpeHeldPartyIdx_ = -1;          // which party pointer (0-5) held Pokemon belongs to
    std::array<uint16_t, 6> lgpePartyBackup_{};  // backup for cancel/undo

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
    SDL_Texture* getSprite(uint16_t nationalId, uint8_t form = 0);
    SDL_Texture* getShinySprite(uint16_t nationalId, uint8_t form = 0);
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
    uint32_t deleteHoldStart_ = 0;     // A held on the delete dialog since (0 = not)
    void tickDeleteHold();
    void deleteSelectedBank();

    // --- Dialogs (source/ui_dialog.cpp) ---
    // Long enough to let go when A was pressed by mistake.
    static constexpr uint32_t HOLD_MS = 2000;
    enum class DialogIcon { Info, Success, Warning, Trash };
    enum class ButtonStyle { Neutral, Primary, Danger, Hold };
    struct DialogButton {
        const char* key;               // "A", "B"
        std::string label;
        ButtonStyle style = ButtonStyle::Neutral;
        float       held = 0.0f;       // Hold: how far along, 0..1
    };
    void drawDialogBackdrop();
    void drawDialog(DialogIcon icon, const std::string& title, const std::string& body,
                    int objectH, const std::function<void(int, int, int)>& drawObject,
                    const std::string& note, const std::vector<DialogButton>& buttons,
                    const std::string& footnote);
    void drawBankObjectRow(const BankInfo& bank, int x, int y, int w);
    ConfirmStyle releaseStyle(Panel from, const Pokemon* pkm, const std::string& where);
    void drawPokemonObjectRow(const Pokemon& pkm, const std::string& where, int x, int y, int w);
    SDL_Texture* iconTrash_ = nullptr;
    SDL_Texture* iconWarn_  = nullptr;

    // --- Bank picker (UI 2.0) ---
    //
    // A list of banks beside a box panel. The list is on the side the chosen
    // bank will open on; the panel previews the highlighted bank, or shows the
    // save when picking the bank a save opens beside.
    bool  bankSelListOnLeft() const;
    Panel bankSelPreviewSource() const;
    bool  bankSelCanCreate() const;    // the "New bank" row exists
    int   bankSelRowCount() const;     // banks, plus "New bank" when it exists
    void  drawBankPickerTopBar();
    void  drawBankList(int x, int w);
    void  drawBankStats(Panel src, int x);
    void  moveBankCursor(int dir);
    void  jumpBankGroup(int dir);
    void handleDeleteConfirmEvent(const SDL_Event& event);
    void beginTextInput(TextInputPurpose purpose);
    void commitTextInput(const std::string& text);

    // --- Online GTS (source/ui_gts.cpp) ---

    // The motif the board is drawn in, shared by the access band and the hub.
    // A slice of a wireframe sphere, a dotted route between two points, and a
    // scatter of stars - all clipped to a rectangle, all in theme colours.
    void drawGtsGlobe(const SDL_Rect& clip, int cx, int cy, int radius, uint8_t wire);
    void drawGtsLink(int ax, int ay, int bx, int by, int lift, uint8_t trail,
                     int dotA, int dotB);
    void drawGtsStars(const SDL_Rect& area, int count, uint32_t seed);
    void drawGtsHubFrame();
    void handleGtsHubInput(bool& running);
    void drawGtsBrowseFrame();
    void handleGtsBrowseInput(bool& running);
    void drawGtsSlot(int x, int y, const Gts::Entry& e, bool isCursor);
    void moveGtsCursor(int dx, int dy);
    void drawGtsFilterPopup();
    void handleGtsFilterInput(const SDL_Event& event);
    void drawGtsDepositPopup();
    void handleGtsDepositInput(const SDL_Event& event);
    void updateGtsCardPreview();
    void enterGts();
    bool gtsLoadPage(int offset);             // blocks; reports failure itself
    void gtsOpenDetail();
    void gtsSaveCurrentAsCard();
    void gtsScanAllCards();
    void gtsUploadSelectedCard();
    std::string gtsEntryLabel(const Gts::Entry& e) const;

    // Rendering helpers

    // Draws whichever screen is current. One place, because four popups draw
    // the screen underneath themselves before drawing on top of it, and four
    // copies of the same if-chain is four chances for a new screen to be added
    // to three of them.
    void drawCurrentScreen();
    void drawFrame();
    void drawMenuPopup();
    void drawAboutPopup();
    void drawThemeSelectorPopup();
    void drawLanguageSelectorPopup();
    void drawSearchFilterPopup();
    void drawSearchResultsPopup();
    void drawSpeciesLetterPicker();
    void drawSpeciesListPicker();
    void drawWondercardListPopup();
    void drawCardListPopup();
    void drawHeldOverlay();
    void drawBoxViewOverlay();
    void drawBoxViewTabs();
    void drawBoxViewStats();
    int  drawBoxViewLegend(bool measureOnly);
    SDL_Rect boxCardRect(int idx, int totalBoxes) const;
    void drawBoxCard(int idx, const SDL_Rect& r, bool isCursor);
    void drawBoxPreview(int boxIdx, const SDL_Rect& card);
    int  panelBoxCount(Panel panel) const;
    std::string panelBoxName(Panel panel, int box) const;
    // --- Box view (UI 2.0) ---
    // House icon and "pkHouse", vertically centred on cy. Returns the x just
    // past the wordmark.
    int  drawLogo(int x, int cy);
    void drawTopBar();
    // One box panel. Everything it shows - which box, its name, the tag above
    // it - comes from the current state, so the bank selector can draw the
    // same panel beside its list.
    // `x` and `box` override where the panel goes and which box it shows (the
    // bank picker draws it beside its list); `tag` replaces the small caps
    // line above the box name.
    void drawBoxPanel(Panel panelId, bool isActive, int x = -1, int box = -1,
                      const std::string& tag = std::string());
    void drawInfoStrip();
    SDL_Rect slotRect(Panel panelId, int col, int row) const;
    SDL_Rect slotRectAt(int panelX, int col, int row) const;
    SDL_Texture* spriteFor(uint16_t species, uint8_t form, bool shiny, bool egg);
    void drawSpriteFit(SDL_Texture* tex, int cx, int cy, int size, Uint8 alpha = 255);

    void drawSlot(int x, int y, const SlotDisplay& sd, bool isCursor, int selectOrder,
                  int highlightState = 0, bool isParty = false);
    void drawText(const std::string& text, int x, int y, SDL_Color color, TTF_Font* f);
    void drawTextCentered(const std::string& text, int cx, int cy, SDL_Color color, TTF_Font* f);
    void drawRect(int x, int y, int w, int h, SDL_Color color);
    void drawRectOutline(int x, int y, int w, int h, SDL_Color color, int thickness);
    void drawStatusBar(const std::string& msg);

    // --- hint bar ------------------------------------------------------------

    // One thing a button does. `button` is the hardware label and is never
    // translated -- A, B, X and Y are the same everywhere -- so only `labelKey`
    // goes through i18n. HINT_DPAD draws the d-pad cross instead of text, which
    // is what stops "D-Pad" and "Steuerkreuz" having to fit inside a circle.
    struct ButtonHint {
        const char* button;
        const char* labelKey;
    };
    static constexpr const char* HINT_DPAD = "\x01";

    // Draws one hint at (x, y) and returns how wide it was, so a row can be
    // laid out without measuring twice.
    int  drawButtonHint(int x, int y, const char* button, const std::string& label);
    int  measureButtonHint(const char* button, const std::string& label);

    // A row of them along the status bar.
    //
    // `message` is the contextual text some screens lead with - what is being
    // held, how many are selected - drawn before the keys. `rightReserve` is
    // how much of the bar the caller intends to use on the right, so a language
    // with long labels drops hints from the end instead of running underneath
    // the profile and game name.
    void drawHintBar(const ButtonHint* hints, int count,
                     const std::string& message = std::string(), int rightReserve = 0);

    // The 2.0 footer: a rule, hints as key caps, version on the right. The old
    // drawHintBar stays for the screens that have not been redone.
    // `rightReserve` < 0 draws the version on the right; otherwise that much
    // room is left free there for the caller to fill.
    void drawFooterBar(const ButtonHint* hints, int count,
                       const std::string& message = std::string(), int rightReserve = -1);
    int  drawFooterKey(int x, int cy, const char* button, bool measureOnly);

    // --- Pokemon summary (source/ui_summary.cpp) ----------------------------
    //
    // Full screen, shaped like the exported card without its QR code; the
    // QR's place holds the complete ribbon and mark list. Shared by the box
    // view (X), the GTS listing and the card import confirmation, which is why
    // the caller supplies the footer keys and `where`: the chip beside the
    // name saying where this Pokemon is - a box and slot, or on the GTS the
    // game family it belongs to, which decides which saves can ever take it.
    void drawDetailPopup(const Pokemon& pkm, const ButtonHint* hints, int hintCount,
                         const std::string& where);
    void drawSummaryHeader(const Pokemon& pkm, const std::string& where);
    void drawSummaryPortrait(const Pokemon& pkm);
    void drawSummaryRibbons(const Pokemon& pkm, int x, int y, int w, int h);
    void drawSummaryAttributes(const Pokemon& pkm);
    void drawSummaryMoves(const Pokemon& pkm);
    void drawSummaryIVs(const Pokemon& pkm);
    void drawSummaryEVs(const Pokemon& pkm);
    void drawSummaryProvenance(const Pokemon& pkm);

    // The ribbon list scrolls with the D-pad when it is longer than its tile.
    // Reset whenever a different Pokemon is shown.
    int  detailRibbonScroll_ = 0;
    static constexpr int SUM_RIBBON_ROWS = 14;
    void scrollDetailRibbons(int dir, const Pokemon& pkm);
    // Box view context for the chip: "Save · Box 1 · 3 / 26".
    std::string detailWhere();

    // Input handling
    void handleInput(bool& running);
    void handleMenuInput(const SDL_Event& event, bool& running);
    void handleDetailInput(const SDL_Event& event);
    void handleNormalInput(const SDL_Event& event);
    void handleStickRepeat();
    void handleBumperRepeat();
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
    void handleSpeciesLetterPickerInput(const SDL_Event& event);
    void handleSpeciesListPickerInput(const SDL_Event& event);
    void buildAvailableSpeciesList();
    void buildSpeciesListForLetter(int letterIndex);
    bool letterHasSpecies(int letterIndex) const;
    void handleWondercardListInput(const SDL_Event& event);
    void handleCardListInput(const SDL_Event& event);
    void updateCardPreview();
    // Draws whichever decoded card it is handed, so the importer and the GTS
    // deposit picker can each keep their own preview without sharing state.
    // `pending` means the cursor has moved and nothing has been decoded yet.
    void drawCardPreviewPane(const CardPayload::Parsed& parsed, bool pending,
                             int paneX, int paneY, int paneW, int paneH);
    void freeCardPreview();
    bool showCardImportConfirm(const Pokemon& pkm);
    void importCard(const CardFile& card);
    bool relocateCard(const CardFile& card, GameType correctGame,
                      std::string& outFolder);
    void injectWondercard(const WCInfo& info);
    std::string exportPokemon(const Pokemon& pkm);

    // --- Card export (source/ui_card.cpp) ---

    // Font sizes used only by the card; opened per export and closed again.
    struct CardFonts {
        TTF_Font* title     = nullptr;
        TTF_Font* level     = nullptr;
        TTF_Font* dex       = nullptr;
        TTF_Font* value     = nullptr;
        TTF_Font* move      = nullptr;
        TTF_Font* body      = nullptr;
        TTF_Font* small     = nullptr;
        TTF_Font* micro     = nullptr;
        TTF_Font* watermark = nullptr;
    };

    // Renders a 1280x720 shareable card and writes it to <basePath>/cards/.
    // Returns the filename written, or "" on failure. exportPokemonCard owns
    // the fonts; renderPokemonCard lets a batch open them once.
    std::string exportPokemonCard(const Pokemon& pkm);
    std::string renderPokemonCard(const Pokemon& pkm, const CardFonts& fonts);

    bool openCardFonts(CardFonts& f);
    void closeCardFonts(CardFonts& f);
    void drawCardHeader(const Pokemon& pkm, const CardFonts& f);
    void drawCardPortrait(const Pokemon& pkm, const CardFonts& f);
    void drawCardAttributes(const Pokemon& pkm, const CardFonts& f);
    void drawCardMoves(const Pokemon& pkm, const CardFonts& f);
    void drawCardIVs(const Pokemon& pkm, const CardFonts& f);
    void drawCardEVs(const Pokemon& pkm, const CardFonts& f);
    void drawCardFooter(const Pokemon& pkm, const CardFonts& f);
    void executeSearch();
    bool matchesSearchFilter(const Pokemon& pkm, const std::string& filterSpecies,
                             const std::string& filterOT) const;
    bool isSearchMatch(Panel panel, int box, int slot) const;
    void clearSearchHighlight();
    void refreshHighlightSet();
    void handleBoxViewInput(const SDL_Event& event);
    void moveBoxViewCursor(int dx, int dy);
    void openBoxView(Panel panel);
    void closeBoxView(bool navigate);
    void switchBoxViewPanel(Panel panel);

    // Dynamic grid: LGPE has 5 columns (5x5), others have 6 (6x5)
    int gridCols() const { return isLGPE(selectedGame_) ? 5 : 6; }
    int maxSlots() const { return gridCols() * 5; }

    // Get pokemon at cursor from the appropriate source
    Pokemon getPokemonAt(int box, int slot, Panel panel) const;
    void setPokemonAt(int box, int slot, Panel panel, const Pokemon& pkm);
    void clearPokemonAt(int box, int slot, Panel panel);
};
