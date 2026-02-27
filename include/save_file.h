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
    static constexpr int COLS_PER_BOX  = 6;
    static constexpr int ROWS_PER_BOX  = 5;

    void setGameType(GameType game);  // must call before load()

    bool load(const std::string& path);
    bool save(const std::string& path);

    Pokemon getBoxSlot(int box, int slot) const;
    void setBoxSlot(int box, int slot, Pokemon pkm);
    void clearBoxSlot(int box, int slot);

    std::string getBoxName(int box) const;

    bool isLoaded() const { return loaded_; }

    // Debug: verify encrypt(decrypt(file)) == file. Call right after load().
    // Returns "OK" if round-trip matches, or a description of the mismatch.
    std::string verifyRoundTrip();

    // Dynamic box count and slots per box
    int boxCount() const { return boxCount_; }
    int slotsPerBox() const { return slotsPerBox_; }

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
    int slotsPerBox_    = 30;
    int gapBoxSlot_     = 0x40;
    int sizeBoxSlot_    = PokeCrypto::SIZE_9PARTY + 0x40;

    // SCBlock keys
    uint32_t kbox_ = 0x0d66012c;                        // LA uses 0x47E1CEAB
    static constexpr uint32_t KBOX_LAYOUT = 0x19722c89; // same for all games

    // BDSP save format constants
    static constexpr int BDSP_BOX_COUNT       = 40;
    static constexpr int BDSP_BOX_OFFSET      = 0x14EF4;
    static constexpr int BDSP_LAYOUT_OFFSET   = 0x148AA;
    static constexpr int BDSP_LAYOUT_SIZE     = 0x64A;
    static constexpr int BDSP_HASH_OFFSET     = 0xE9818;
    static constexpr int BDSP_HASH_SIZE       = 16;

    // LGPE save format constants
    static constexpr int LGPE_SAVE_SIZE      = 0x100000;  // 1 MB
    static constexpr int LGPE_BOX_OFFSET     = 0x05C00;   // PokeListPokemon block
    static constexpr int LGPE_BOX_SIZE       = 0x3F7A0;   // 1000 * 260
    static constexpr int LGPE_HEADER_OFFSET  = 0x05A00;   // PokeListHeader block
    static constexpr int LGPE_BOX_COUNT      = 40;
    static constexpr int LGPE_SLOTS_PER_BOX  = 25;
    static constexpr int LGPE_BLOCK_INFO_OFS = 0xB8600;   // boGG from PKHeX (block info base)
    static constexpr int LGPE_NUM_BLOCKS     = 21;

    struct LGPEBlock { int offset; int length; };
    static constexpr LGPEBlock LGPE_BLOCKS[LGPE_NUM_BLOCKS] = {
        {0x00000, 0x00D90}, {0x00E00, 0x00200}, {0x01000, 0x00168},
        {0x01200, 0x01800}, {0x02A00, 0x020E8}, {0x04C00, 0x00930},
        {0x05600, 0x00004}, {0x05800, 0x00130}, {0x05A00, 0x00012},
        {0x05C00, 0x3F7A0}, {0x45400, 0x00008}, {0x45600, 0x00E90},
        {0x46600, 0x010A4}, {0x47800, 0x000F0}, {0x47A00, 0x06010},
        {0x4DC00, 0x00200}, {0x4DE00, 0x00098}, {0x4E000, 0x00068},
        {0x4E200, 0x69780}, {0xB7A00, 0x000B0}, {0xB7C00, 0x00940},
    };

    static uint16_t crc16NoInvert(const uint8_t* data, size_t len);

    // GBA save format constants (sector-based, for FRLG)
    static constexpr int GBA_SAVE_SIZE       = 0x20000;  // 128KB
    static constexpr int GBA_SECTOR_SIZE     = 0x1000;   // 4KB per sector
    static constexpr int GBA_SECTOR_USED     = 0xF80;    // usable data per sector
    static constexpr int GBA_SECTOR_COUNT    = 14;       // sectors per save slot
    static constexpr int GBA_STORAGE_FIRST   = 5;        // first sector ID for box storage
    static constexpr int GBA_STORAGE_LAST    = 13;       // last sector ID for box storage
    static constexpr int GBA_STORAGE_SECTORS = 9;        // sectors for box storage
    static constexpr int GBA_BOX_COUNT       = 14;
    static constexpr int GBA_SLOTS_PER_BOX   = 30;
    static constexpr int GBA_BOXNAME_LEN     = 9;        // 8 chars + terminator (Gen3 encoded)

    // GBA sector footer offsets (within each 0x1000 sector)
    static constexpr int GBA_OFS_SECTOR_ID   = 0xFF4;    // u16 sector ID
    static constexpr int GBA_OFS_CHECKSUM    = 0xFF6;    // u16 checksum
    static constexpr int GBA_OFS_SAVE_INDEX  = 0xFFC;    // u32 save counter

    // GBA CheckSum32: sum u32s in data, fold to u16
    static uint16_t checkSum32GBA(const uint8_t* data, size_t len);

    // GBA assembled storage buffer (sectors 5-13 concatenated)
    std::vector<uint8_t> gbaStorage_;
    int gbaActiveSlot_ = 0;

    // BDSP and LGPE raw save data (flat binary, no SCBlocks)
    std::vector<uint8_t> rawData_;

    // Original file data for round-trip verification (cleared after verify)
    std::vector<uint8_t> originalFileData_;

    bool loadSCBlock(const std::string& path);
    bool saveSCBlock(const std::string& path);
    bool loadBDSP(const std::string& path);
    bool saveBDSP(const std::string& path);
    bool loadLGPE(const std::string& path);
    bool saveLGPE(const std::string& path);
    bool loadGBA(const std::string& path);
    bool saveGBA(const std::string& path);

    static bool isBDSPSize(size_t size);

    int getBoxOffset(int box) const {
        return sizeBoxSlot_ * box * slotsPerBox_;
    }
    int getBoxSlotOffset(int box, int slot) const {
        return getBoxOffset(box) + slot * sizeBoxSlot_;
    }
};
