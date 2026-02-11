#pragma once
#include "swish_crypto.h"
#include "pokemon.h"
#include "game_type.h"
#include <vector>
#include <string>

// SaveFile - manages Pokemon save files (Gen8/Gen9).
// SCBlock-based for ZA/SV/SwSh, flat binary for BDSP.
class SaveFile {
public:
    static constexpr int SLOTS_PER_BOX = 30;
    static constexpr int COLS_PER_BOX  = 6;
    static constexpr int ROWS_PER_BOX  = 5;

    void setGameType(GameType game);  // must call before load()

    bool load(const std::string& path);
    bool save(const std::string& path);

    Pokemon getBoxSlot(int box, int slot) const;
    void setBoxSlot(int box, int slot, const Pokemon& pkm);
    void clearBoxSlot(int box, int slot);

    std::string getBoxName(int box) const;

    bool isLoaded() const { return loaded_; }

    // Dynamic box count: 40 for BDSP, 32 for others
    int boxCount() const { return boxCount_; }

private:
    std::vector<SCBlock> blocks_;

    // Cached pointers into block data (SCBlock) or raw data (BDSP)
    uint8_t* boxData_       = nullptr;
    size_t   boxDataLen_    = 0;
    uint8_t* boxLayoutData_ = nullptr;
    size_t   boxLayoutLen_  = 0;

    std::string filePath_;
    bool loaded_ = false;

    // Game-specific parameters
    GameType gameType_  = GameType::ZA;
    int boxCount_       = 32;
    int gapBoxSlot_     = 0x40;
    int sizeBoxSlot_    = PokeCrypto::SIZE_9PARTY + 0x40;

    // SCBlock keys (ZA/SV/SwSh)
    static constexpr uint32_t KBOX        = 0x0d66012c;
    static constexpr uint32_t KBOX_LAYOUT = 0x19722c89;

    // BDSP save format constants
    static constexpr int BDSP_BOX_COUNT       = 40;
    static constexpr int BDSP_BOX_OFFSET      = 0x14EF4;
    static constexpr int BDSP_LAYOUT_OFFSET   = 0x148AA;
    static constexpr int BDSP_LAYOUT_SIZE     = 0x64A;
    static constexpr int BDSP_HASH_OFFSET     = 0xE9818;
    static constexpr int BDSP_HASH_SIZE       = 16;

    // BDSP raw save data (flat binary, no SCBlocks)
    std::vector<uint8_t> rawData_;

    bool loadSCBlock(const std::string& path);
    bool saveSCBlock(const std::string& path);
    bool loadBDSP(const std::string& path);
    bool saveBDSP(const std::string& path);

    static bool isBDSPSize(size_t size);

    int getBoxOffset(int box) const {
        return sizeBoxSlot_ * box * SLOTS_PER_BOX;
    }
    int getBoxSlotOffset(int box, int slot) const {
        return getBoxOffset(box) + slot * sizeBoxSlot_;
    }
};
