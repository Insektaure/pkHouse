#pragma once
#include "swish_crypto.h"
#include "pokemon.h"
#include <vector>
#include <string>

// SaveFile - manages a Pokemon ZA save file.
// Decrypts/encrypts via SwishCrypto, provides box access.
// Ported from PKHeX.Core/Saves/SAV9ZA.cs
class SaveFile {
public:
    static constexpr int BOX_COUNT     = 32;
    static constexpr int SLOTS_PER_BOX = 30;
    static constexpr int COLS_PER_BOX  = 6;
    static constexpr int ROWS_PER_BOX  = 5;

    // SIZE_BOXSLOT = SIZE_9PARTY (0x158) + GapBoxSlot (0x40) = 0x198
    // From SAV9ZA.cs line 132,172
    static constexpr int GAP_BOX_SLOT  = 0x40;
    static constexpr int SIZE_BOXSLOT  = PokeCrypto::SIZE_9PARTY + GAP_BOX_SLOT;

    bool load(const std::string& path);
    bool save(const std::string& path);

    Pokemon getBoxSlot(int box, int slot) const;
    void setBoxSlot(int box, int slot, const Pokemon& pkm);
    void clearBoxSlot(int box, int slot);

    std::string getBoxName(int box) const;

    bool isLoaded() const { return loaded_; }

private:
    std::vector<SCBlock> blocks_;

    // Cached pointers into block data
    uint8_t* boxData_       = nullptr;
    size_t   boxDataLen_    = 0;
    uint8_t* boxLayoutData_ = nullptr;
    size_t   boxLayoutLen_  = 0;

    std::string filePath_;
    bool loaded_ = false;

    // Block keys from SaveBlockAccessor9ZA.cs
    static constexpr uint32_t KBOX        = 0x0d66012c;
    static constexpr uint32_t KBOX_LAYOUT = 0x19722c89;

    int getBoxOffset(int box) const {
        return SIZE_BOXSLOT * box * SLOTS_PER_BOX;
    }
    int getBoxSlotOffset(int box, int slot) const {
        return getBoxOffset(box) + slot * SIZE_BOXSLOT;
    }
};
