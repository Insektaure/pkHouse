#pragma once
#include "pokemon.h"
#include "game_type.h"
#include <string>
#include <vector>

// Bank - persistent storage for extracted Pokemon.
// Stores decrypted Pokemon data in a simple binary file format.
// Supports two modes:
//   Game bank: all slots share one GameType (format-specific slotSize)
//   Universal bank: each slot stores 1-byte origin tag + MAX_PARTY_SIZE data
class Bank {
public:

    Bank();

    void setGameType(GameType g);
    void setUniversal();

    // Load bank from file. Returns true on success; creates empty bank if file missing.
    bool load(const std::string& path);

    // Save bank to file.
    bool save(const std::string& path);

    Pokemon getSlot(int box, int slot) const;
    void setSlot(int box, int slot, const Pokemon& pkm);
    void clearSlot(int box, int slot);

    // Universal bank: get the origin GameType for a slot
    GameType getSlotOrigin(int box, int slot) const;

    std::string getBoxName(int box) const;
    void setBoxName(int box, const std::string& name);

    bool isUniversal() const { return isUniversal_; }
    int boxCount() const { return boxCount_; }
    int slotsPerBox() const { return slotsPerBox_; }
    int totalSlots() const { return boxCount_ * slotsPerBox_; }
    size_t fileSize() const { return HEADER_SIZE + (size_t)totalSlots() * slotSize_ + (size_t)boxCount_ * BOX_NAME_SIZE; }

private:
    // File format:
    //   [8 bytes]  Magic: "PKHOUSE\0"
    //   [4 bytes]  Version (u32 LE): 1-5 = game banks, 6 = universal
    //   [4 bytes]  Reserved
    //   [N bytes]  totalSlots * slotSize decrypted data
    //   Universal slot = [1 byte GameType tag][MAX_PARTY_SIZE bytes data]
    static constexpr int HEADER_SIZE   = 16;
    static constexpr int SLOT_SIZE     = PokeCrypto::SIZE_9PARTY;
    static constexpr int BOX_NAME_SIZE = 16;

    // Universal slot: 1 byte tag + MAX_PARTY_SIZE
    static constexpr int UNIVERSAL_SLOT_SIZE = 1 + PokeCrypto::MAX_PARTY_SIZE;

    static constexpr char MAGIC[8] = {'P','K','H','O','U','S','E','\0'};
    static constexpr uint32_t VERSION_32BOX    = 1;
    static constexpr uint32_t VERSION_40BOX    = 2;
    static constexpr uint32_t VERSION_LA       = 3;
    static constexpr uint32_t VERSION_LGPE     = 4;
    static constexpr uint32_t VERSION_FRLG     = 5;
    static constexpr uint32_t VERSION_UNIVERSAL = 6;

    bool isUniversal_ = false;
    GameType gameType_ = GameType::ZA;
    int boxCount_ = 32;
    int slotsPerBox_ = 30;
    int slotSize_ = PokeCrypto::SIZE_9PARTY;
    std::vector<Pokemon> slots_;
    std::vector<GameType> slotOrigins_; // universal mode only: per-slot origin tag
    std::vector<std::string> boxNames_;

    uint32_t fileVersion() const {
        if (isUniversal_) return VERSION_UNIVERSAL;
        if (isFRLG(gameType_)) return VERSION_FRLG;
        if (isLGPE(gameType_)) return VERSION_LGPE;
        if (gameType_ == GameType::LA) return VERSION_LA;
        return boxCount_ == 40 ? VERSION_40BOX : VERSION_32BOX;
    }

    int slotIndex(int box, int slot) const {
        return box * slotsPerBox_ + slot;
    }

    // Encode/decode GameType as a single byte for file storage
    static uint8_t encodeGameType(GameType g);
    static GameType decodeGameType(uint8_t v);
};
