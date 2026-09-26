#pragma once
#include "pokemon.h"
#include "game_type.h"
#include <cstdint>
#include <string>
#include <vector>

// Client for the pkHouse GTS: a public board where a card's payload can be
// left for anyone else to take.
//
// Every call here BLOCKS until it finishes or times out. That is deliberate:
// each one is a single request the user asked for by pressing a button, the
// app's run loop is single-threaded, and showWorking() already exists to cover
// the wait. A worker thread would buy a responsive frame during the one or two
// seconds a request takes, at the price of a loading/error state machine in
// every screen that touches the board.
//
// The wire format is the same PKHC payload the card QR codes carry, so a
// deposit is a card and a download can be saved back out as one.
namespace Gts {

// The board this build talks to.
constexpr const char* DEFAULT_URL = "https://gts.nxplaza.net";

// Where this console's board identity is kept.
constexpr const char* CONFIG_DIR = "sdmc:/config/pkHouse";

// One listing row, exactly as the board returns it.
//
// Every field below is read out of the payload BY THE SERVER, never declared
// by whoever deposited it - otherwise a listing could claim to be a shiny
// six-IV Mewtwo and only turn out to be a Magikarp once somebody downloaded it.
struct Entry {
    std::string id;          // 32 hex characters
    GameType    game  = GameType::ZA;
    bool        gameKnown = false;   // false when this build predates the game
    std::string family;              // za, sv, swsh, bdsp, la, lgpe, frlg

    uint16_t species  = 0;
    uint8_t  form     = 0;
    uint8_t  level    = 0;           // 0 = the server could not extract one
    bool     shiny    = false;
    bool     alpha    = false;   // Legends: Arceus and Z-A only
    uint8_t  gender   = 3;           // 0 male, 1 female, 2 none, 3 not extracted
    uint8_t  nature   = 0;
    uint8_t  ball     = 0;
    uint16_t ability  = 0;
    uint16_t heldItem = 0;
    uint8_t  language = 0;
    bool     egg      = false;
    int      ivTotal  = 0;
    int      evTotal  = 0;

    std::string nickname;
    std::string otName;
    std::string note;
    std::string legality;            // pending, legal, illegal, error

    // Why it was rejected. Sent only for the verdicts
    // where there is something to say - a legal Pokemon's report says "Legal!"
    // and sixty of those would be most of a page of JSON saying nothing.
    std::string legalityReport;
    int         downloads = 0;
    uint32_t    created   = 0;
};

// What to ask the board for. Defaults are "everything the scanner has judged, newest first".
struct Filter {
    std::string family;              // "" = every game
    uint16_t    species = 0;         // 0 = any
    std::string speciesName;         // for showing the filter back to the user
    int  shiny  = -1;                // -1 any, 0 no, 1 yes
    int  egg    = -1;
    int  alpha  = -1;                // -1 any, 0 no, 1 yes; Legends: Arceus and Z-A only
    int  ball   = -1;                // -1 any
    int  minIvs = -1;                // -1 any, else 0..186
    // Which verdicts to list. Checked is everything the scanner has been
    // through - legal, illegal, unreadable - so a rejected deposit shows with
    // its reason; All adds the ones still waiting to be judged. A board that
    // predates "checked" answers it with legal only.
    enum class Legality { Checked, LegalOnly, All };
    Legality legality = Legality::Checked;
    bool byPopularity = false;       // false = newest first

    bool isDefault() const {
        return family.empty() && species == 0 && shiny < 0 && egg < 0
            && alpha < 0 && ball < 0 && minIvs < 0 && legality == Legality::Checked && !byPopularity;
    }
};

// Whether the alpha filter applies: any game, or one that has alphas.
inline bool alphaApplies(const Filter& f) {
    return f.family.empty() || f.family == "la" || f.family == "za";
}

struct Page {
    std::vector<Entry> entries;
    int  offset = 0;
    bool more   = false;             // another page exists after this one
};

// Brings up sockets and curl, loads the board address and this console's
// identity. Safe to call when there is no network: everything then fails with
// a message rather than crashing.
bool init(const std::string& basePath);
void shutdown();

// False when no board address is configured. Every call below fails in that
// case, so screens check this first and say what to do about it.
bool configured();
const std::string& baseUrl();

// Why the last call failed. Empty after a call that succeeded.
const std::string& lastError();

// Claims this console's id on the board. Called once before anything else;
// the id and token are generated on first run and kept on the SD card.
bool hello();

// One page of the listing. `offset` must be a multiple of PAGE_SIZE.
constexpr int PAGE_SIZE = 60;   // exactly one browse grid
bool search(const Filter& filter, int offset, Page& out);

// The whole Pokemon behind a listing, so the detail view can show moves, IVs
// and everything else the listing does not carry.
//
// Looking is not taking, so this counts nothing. markDownloaded() does.
bool fetch(const std::string& id, Pokemon& out, GameType& game);

// Counts one download, after a listing has actually been kept.
bool markDownloaded(const std::string& id);

// Puts a payload on the board. `outDuplicate` comes back true when those exact
// bytes were already listed, in which case `outId` is the listing that already
// exists and nothing was added.
bool deposit(const std::vector<uint8_t>& payload, const std::string& note,
             std::string& outId, bool& outDuplicate);

} // namespace Gts
