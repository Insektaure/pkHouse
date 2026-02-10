#pragma once
#include "pokemon.h"
#include <string>
#include <array>

// Bank - persistent storage for extracted Pokemon.
// Stores decrypted PA9 data in a simple binary file format.
class Bank {
public:
    static constexpr int BOX_COUNT     = 32;
    static constexpr int SLOTS_PER_BOX = 30;
    static constexpr int TOTAL_SLOTS   = BOX_COUNT * SLOTS_PER_BOX;

    Bank();

    // Load bank from file. Returns true on success; creates empty bank if file missing.
    bool load(const std::string& path);

    // Save bank to file.
    bool save(const std::string& path);

    Pokemon getSlot(int box, int slot) const;
    void setSlot(int box, int slot, const Pokemon& pkm);
    void clearSlot(int box, int slot);

    std::string getBoxName(int box) const;

private:
    // File format:
    //   [8 bytes]  Magic: "PKHOUSE\0"
    //   [4 bytes]  Version (u32 LE)
    //   [4 bytes]  Reserved
    //   [N bytes]  TOTAL_SLOTS * SIZE_9PARTY decrypted PA9 data
    static constexpr int HEADER_SIZE = 16;
    static constexpr int SLOT_SIZE   = PokeCrypto::SIZE_9PARTY;

    static constexpr char MAGIC[8] = {'P','K','H','O','U','S','E','\0'};
    static constexpr uint32_t VERSION = 1;

    std::array<Pokemon, TOTAL_SLOTS> slots_;

    int slotIndex(int box, int slot) const {
        return box * SLOTS_PER_BOX + slot;
    }
};
