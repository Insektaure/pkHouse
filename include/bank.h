#pragma once
#include "pokemon.h"
#include "game_type.h"
#include <string>
#include <vector>

// Bank - persistent storage for extracted Pokemon.
// Stores decrypted Pokemon data in a simple binary file format.
class Bank {
public:
    static constexpr int SLOTS_PER_BOX = 30;

    Bank();

    void setGameType(GameType g);

    // Load bank from file. Returns true on success; creates empty bank if file missing.
    bool load(const std::string& path);

    // Save bank to file.
    bool save(const std::string& path);

    Pokemon getSlot(int box, int slot) const;
    void setSlot(int box, int slot, const Pokemon& pkm);
    void clearSlot(int box, int slot);

    std::string getBoxName(int box) const;

    int boxCount() const { return boxCount_; }
    int totalSlots() const { return boxCount_ * SLOTS_PER_BOX; }

private:
    // File format:
    //   [8 bytes]  Magic: "PKHOUSE\0"
    //   [4 bytes]  Version (u32 LE): 1 = 32 boxes, 2 = 40 boxes
    //   [4 bytes]  Reserved
    //   [N bytes]  totalSlots * SIZE_9PARTY decrypted data
    static constexpr int HEADER_SIZE = 16;
    static constexpr int SLOT_SIZE   = PokeCrypto::SIZE_9PARTY;

    static constexpr char MAGIC[8] = {'P','K','H','O','U','S','E','\0'};
    static constexpr uint32_t VERSION_32BOX = 1;
    static constexpr uint32_t VERSION_40BOX = 2;
    static constexpr uint32_t VERSION_LA    = 3;

    GameType gameType_ = GameType::ZA;
    int boxCount_ = 32;
    int slotSize_ = PokeCrypto::SIZE_9PARTY;
    std::vector<Pokemon> slots_;

    uint32_t fileVersion() const {
        if (gameType_ == GameType::LA) return VERSION_LA;
        return boxCount_ == 40 ? VERSION_40BOX : VERSION_32BOX;
    }

    int slotIndex(int box, int slot) const {
        return box * SLOTS_PER_BOX + slot;
    }
};
