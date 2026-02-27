#pragma once
#include "pokemon.h"
#include "game_type.h"
#include <string>
#include <vector>

// Bank - persistent storage for extracted Pokemon.
// Stores decrypted Pokemon data in a simple binary file format.
class Bank {
public:

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
    void setBoxName(int box, const std::string& name);

    int boxCount() const { return boxCount_; }
    int slotsPerBox() const { return slotsPerBox_; }
    int totalSlots() const { return boxCount_ * slotsPerBox_; }
    size_t fileSize() const { return HEADER_SIZE + (size_t)totalSlots() * slotSize_ + (size_t)boxCount_ * BOX_NAME_SIZE; }

private:
    // File format:
    //   [8 bytes]  Magic: "PKHOUSE\0"
    //   [4 bytes]  Version (u32 LE): 1 = 32 boxes, 2 = 40 boxes
    //   [4 bytes]  Reserved
    //   [N bytes]  totalSlots * SIZE_9PARTY decrypted data
    static constexpr int HEADER_SIZE   = 16;
    static constexpr int SLOT_SIZE     = PokeCrypto::SIZE_9PARTY;
    static constexpr int BOX_NAME_SIZE = 16;

    static constexpr char MAGIC[8] = {'P','K','H','O','U','S','E','\0'};
    static constexpr uint32_t VERSION_32BOX = 1;
    static constexpr uint32_t VERSION_40BOX = 2;
    static constexpr uint32_t VERSION_LA    = 3;
    static constexpr uint32_t VERSION_LGPE  = 4;
    static constexpr uint32_t VERSION_FRLG  = 5;

    GameType gameType_ = GameType::ZA;
    int boxCount_ = 32;
    int slotsPerBox_ = 30;
    int slotSize_ = PokeCrypto::SIZE_9PARTY;
    std::vector<Pokemon> slots_;
    std::vector<std::string> boxNames_;

    uint32_t fileVersion() const {
        if (isFRLG(gameType_)) return VERSION_FRLG;
        if (isLGPE(gameType_)) return VERSION_LGPE;
        if (gameType_ == GameType::LA) return VERSION_LA;
        return boxCount_ == 40 ? VERSION_40BOX : VERSION_32BOX;
    }

    int slotIndex(int box, int slot) const {
        return box * slotsPerBox_ + slot;
    }
};
