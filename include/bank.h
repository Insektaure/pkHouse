#pragma once
#include "pokemon.h"
#include "game_type.h"
#include <string>
#include <vector>

// Bank mode: per-game (native format) or universal (PA9 canonical + origin tag)
enum class BankMode { Game, Universal };

// Bank - persistent storage for extracted Pokemon.
// Stores decrypted Pokemon data in a simple binary file format.
//
// Game mode: stores Pokemon in their native format (existing behavior).
// Universal mode: stores all Pokemon as PA9 (0x158) with a 1-byte origin
// game tag per slot. Conversion happens on import/export via EntityConverter.
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

    // Universal bank: get/set the origin game for a slot
    GameType getSlotOrigin(int box, int slot) const;
    void setSlotOrigin(int box, int slot, GameType origin);

    std::string getBoxName(int box) const;
    void setBoxName(int box, const std::string& name);

    int boxCount() const { return boxCount_; }
    int slotsPerBox() const { return slotsPerBox_; }
    int totalSlots() const { return boxCount_ * slotsPerBox_; }
    BankMode mode() const { return mode_; }
    bool isUniversal() const { return mode_ == BankMode::Universal; }
    size_t fileSize() const;

    // Create as universal bank (call before first save, instead of setGameType)
    void setUniversal();

private:
    // File format:
    //   [8 bytes]  Magic: "PKHOUSE\0"
    //   [4 bytes]  Version (u32 LE): 1-5 = game banks, 6 = universal
    //   [4 bytes]  Reserved
    //   Game mode:      [N bytes] totalSlots * slotSize_ decrypted data
    //   Universal mode: [N bytes] totalSlots * (1 + SIZE_9PARTY) data
    //   Then: [boxCount * 16 bytes] box names
    static constexpr int HEADER_SIZE   = 16;
    static constexpr int SLOT_SIZE     = PokeCrypto::SIZE_9PARTY;
    static constexpr int BOX_NAME_SIZE = 16;

    static constexpr char MAGIC[8] = {'P','K','H','O','U','S','E','\0'};
    static constexpr uint32_t VERSION_32BOX    = 1;
    static constexpr uint32_t VERSION_40BOX    = 2;
    static constexpr uint32_t VERSION_LA       = 3;
    static constexpr uint32_t VERSION_LGPE     = 4;
    static constexpr uint32_t VERSION_FRLG     = 5;
    static constexpr uint32_t VERSION_UNIVERSAL = 6;

    BankMode mode_ = BankMode::Game;
    GameType gameType_ = GameType::ZA;
    int boxCount_ = 32;
    int slotsPerBox_ = 30;
    int slotSize_ = PokeCrypto::SIZE_9PARTY;
    std::vector<Pokemon> slots_;
    std::vector<GameType> slotOrigins_;  // universal mode: origin game per slot
    std::vector<std::string> boxNames_;

    uint32_t fileVersion() const {
        if (mode_ == BankMode::Universal) return VERSION_UNIVERSAL;
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
