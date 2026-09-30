#pragma once
#include "bank.h"
#include "game_type.h"

// Sorting a bank: one box or the whole bank, in memory. Nothing is written to
// the SD card here - the bank is saved, or the change dropped, by the usual
// save / leave-without-saving flow, which is what makes a sort undoable.
//
// Every order only moves Pokemon between slots: none is created, lost or
// changed. Eggs go after the others. Pokemon that sort equal keep their order.
namespace BankSort {

enum class Order {
    Dex,         // national Pokedex number, then form
    Name,        // species name in the app's language
    ShinyFirst,  // shinies, then the others; each by Pokedex number
    Level,       // highest first
    Ivs,         // best IV total first
    Compact,     // the current order, gaps closed
    LivingDex,   // one slot per species of the game's Pokedex (whole bank)
};
constexpr int ORDER_COUNT = 7;

struct Result {
    bool done = false;   // the bank was rearranged
    int  pokemon = 0;    // Pokemon in what was sorted
    // Living Dex
    int  species = 0;    // species in the game's Pokedex
    int  placed = 0;     // of those, how many have a Pokemon in their slot
    // Living Dex that does not fit (done == false, needed > capacity): the
    // slots it needs packed - the Pokemon, plus one gap per missing species.
    int  needed = 0;
    int  capacity = 0;
};

// Sorts box `box`, or the whole bank when `box` < 0. A Living Dex is always
// the whole bank. `game` picks the Pokedex (the bank's own game).
Result sort(Bank& bank, GameType game, Order order, int box);

} // namespace BankSort
