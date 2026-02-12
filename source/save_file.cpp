#include "save_file.h"
#include "md5.h"
#include <fstream>
#include <cstdio>
#include <cstring>

void SaveFile::setGameType(GameType game) {
    gameType_ = game;
    kbox_ = 0x0d66012c; // Reset to default; LA overrides below
    slotsPerBox_ = 30;   // Default; LGPE overrides below
    if (game == GameType::ZA) {
        gapBoxSlot_  = 0x40;
        sizeBoxSlot_ = PokeCrypto::SIZE_9PARTY + 0x40;
        boxCount_    = 32;
    } else if (isBDSP(game)) {
        gapBoxSlot_  = 0;
        sizeBoxSlot_ = PokeCrypto::SIZE_9PARTY;
        boxCount_    = BDSP_BOX_COUNT;
    } else if (game == GameType::LA) {
        gapBoxSlot_  = 0;
        sizeBoxSlot_ = PokeCrypto::SIZE_8ASTORED;
        boxCount_    = 32;
        kbox_        = 0x47E1CEAB;
    } else if (isLGPE(game)) {
        gapBoxSlot_  = 0;
        sizeBoxSlot_ = PokeCrypto::SIZE_6PARTY;
        boxCount_    = LGPE_BOX_COUNT;
        slotsPerBox_ = LGPE_SLOTS_PER_BOX;
    } else {
        // SV and SwSh: no gap, 32 boxes
        gapBoxSlot_  = 0;
        sizeBoxSlot_ = PokeCrypto::SIZE_9PARTY;
        boxCount_    = 32;
    }
}

bool SaveFile::isBDSPSize(size_t size) {
    return size == 0xE9828  // v1.0
        || size == 0xEDC20  // v1.1
        || size == 0xEED8C  // v1.2
        || size == 0xEF0A4; // v1.3
}

bool SaveFile::load(const std::string& path) {
    filePath_ = path;
    loaded_ = false;
    boxData_ = nullptr;
    boxLayoutData_ = nullptr;

    if (isBDSP(gameType_))
        return loadBDSP(path);
    if (isLGPE(gameType_))
        return loadLGPE(path);
    return loadSCBlock(path);
}

bool SaveFile::save(const std::string& path) {
    if (!loaded_)
        return false;

    if (isBDSP(gameType_))
        return saveBDSP(path);
    if (isLGPE(gameType_))
        return saveLGPE(path);
    return saveSCBlock(path);
}

bool SaveFile::loadSCBlock(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    auto fileSize = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> fileData(fileSize);
    file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    file.close();

    // Save original file data for round-trip verification (decrypt modifies in-place)
    originalFileData_ = fileData;

    // Decrypt into SCBlocks
    blocks_ = SwishCrypto::decrypt(fileData.data(), fileData.size());

    // Find box data block
    SCBlock* boxBlock = SwishCrypto::findBlock(blocks_, kbox_);
    if (!boxBlock)
        return false;
    boxData_ = boxBlock->data.data();
    boxDataLen_ = boxBlock->data.size();

    // Find box layout block (box names)
    SCBlock* layoutBlock = SwishCrypto::findBlock(blocks_, KBOX_LAYOUT);
    if (layoutBlock) {
        boxLayoutData_ = layoutBlock->data.data();
        boxLayoutLen_ = layoutBlock->data.size();
    }

    loaded_ = true;
    return true;
}

bool SaveFile::saveSCBlock(const std::string& path) {
    std::vector<uint8_t> encrypted = SwishCrypto::encrypt(blocks_);

    // Open for in-place writing (r+b) to avoid truncating the file.
    // The Switch save filesystem journal can break if we truncate + rewrite.
    // Our encrypted output is always the exact same size as the original.
    FILE* f = std::fopen(path.c_str(), "r+b");
    if (!f) {
        // File doesn't exist yet — create it
        f = std::fopen(path.c_str(), "wb");
    }
    if (!f)
        return false;

    size_t written = std::fwrite(encrypted.data(), 1, encrypted.size(), f);
    std::fclose(f);
    return written == encrypted.size();
}

bool SaveFile::loadBDSP(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    auto fileSize = static_cast<size_t>(file.tellg());
    if (!isBDSPSize(fileSize))
        return false;

    file.seekg(0);
    rawData_.resize(fileSize);
    file.read(reinterpret_cast<char*>(rawData_.data()), fileSize);
    file.close();

    // Box data at fixed offset (party format, 0x158 per slot, 40 boxes * 30 slots)
    size_t boxDataEnd = BDSP_BOX_OFFSET +
        static_cast<size_t>(BDSP_BOX_COUNT) * slotsPerBox_ * PokeCrypto::SIZE_9PARTY;
    if (boxDataEnd > rawData_.size())
        return false;

    boxData_ = rawData_.data() + BDSP_BOX_OFFSET;
    boxDataLen_ = boxDataEnd - BDSP_BOX_OFFSET;

    // Box layout at fixed offset
    if (BDSP_LAYOUT_OFFSET + BDSP_LAYOUT_SIZE <= static_cast<int>(rawData_.size())) {
        boxLayoutData_ = rawData_.data() + BDSP_LAYOUT_OFFSET;
        boxLayoutLen_ = BDSP_LAYOUT_SIZE;
    }

    loaded_ = true;
    return true;
}

bool SaveFile::saveBDSP(const std::string& path) {
    if (rawData_.empty())
        return false;

    // Recalculate MD5 checksum
    // Clear existing hash, compute MD5 of entire save, write hash back
    if (rawData_.size() >= static_cast<size_t>(BDSP_HASH_OFFSET + BDSP_HASH_SIZE)) {
        std::memset(rawData_.data() + BDSP_HASH_OFFSET, 0, BDSP_HASH_SIZE);
        MD5::hash(rawData_.data(), rawData_.size(), rawData_.data() + BDSP_HASH_OFFSET);
    }

    // Open for in-place writing to avoid truncation issues on Switch save filesystem
    FILE* f = std::fopen(path.c_str(), "r+b");
    if (!f) {
        f = std::fopen(path.c_str(), "wb");
    }
    if (!f)
        return false;

    size_t written = std::fwrite(rawData_.data(), 1, rawData_.size(), f);
    std::fclose(f);
    return written == rawData_.size();
}

Pokemon SaveFile::getBoxSlot(int box, int slot) const {
    Pokemon pkm;
    if (!loaded_ || !boxData_)
        return pkm;

    int offset = getBoxSlotOffset(box, slot);
    if (offset + sizeBoxSlot_ > static_cast<int>(boxDataLen_))
        return pkm;

    // gameType_ must be set before decrypt (selects correct algorithm)
    pkm.gameType_ = gameType_;
    int dataSize = sizeBoxSlot_ - gapBoxSlot_;
    pkm.loadFromEncrypted(boxData_ + offset, dataSize);
    return pkm;
}

void SaveFile::setBoxSlot(int box, int slot, Pokemon pkm) {
    if (!loaded_ || !boxData_)
        return;

    int offset = getBoxSlotOffset(box, slot);
    if (offset + sizeBoxSlot_ > static_cast<int>(boxDataLen_))
        return;

    // Ensure correct game type, refresh checksum, encrypt and write
    pkm.gameType_ = gameType_;
    pkm.getEncrypted(boxData_ + offset);
    // Zero the gap bytes (if any)
    if (gapBoxSlot_ > 0)
        std::memset(boxData_ + offset + (sizeBoxSlot_ - gapBoxSlot_), 0, gapBoxSlot_);
}

void SaveFile::clearBoxSlot(int box, int slot) {
    if (!loaded_ || !boxData_)
        return;

    int offset = getBoxSlotOffset(box, slot);
    if (offset + sizeBoxSlot_ > static_cast<int>(boxDataLen_))
        return;

    // Write encrypted blank PKM (matching PKHeX behavior) instead of raw zeros.
    // A blank PKM has all-zero decrypted data; we encrypt it so the slot
    // contains valid PokeCrypto-encrypted "empty" data.
    Pokemon blank;
    blank.gameType_ = gameType_;
    blank.getEncrypted(boxData_ + offset);
    // Zero the gap bytes (if any)
    if (gapBoxSlot_ > 0)
        std::memset(boxData_ + offset + (sizeBoxSlot_ - gapBoxSlot_), 0, gapBoxSlot_);
}

std::string SaveFile::getBoxName(int box) const {
    if (!boxLayoutData_ || box < 0 || box >= boxCount_) {
        return "Box " + std::to_string(box + 1);
    }

    // Box names are stored as UTF-16LE, 0x22 bytes per name
    constexpr int NAME_SIZE = 0x22;
    int nameOfs = box * NAME_SIZE;
    if (nameOfs + NAME_SIZE > static_cast<int>(boxLayoutLen_)) {
        return "Box " + std::to_string(box + 1);
    }

    // Read UTF-16LE string
    std::string result;
    const uint8_t* p = boxLayoutData_ + nameOfs;
    for (int i = 0; i < NAME_SIZE / 2; i++) {
        uint16_t ch;
        std::memcpy(&ch, p + i * 2, 2);
        if (ch == 0)
            break;
        if (ch < 0x80) {
            result += static_cast<char>(ch);
        } else if (ch < 0x800) {
            result += static_cast<char>(0xC0 | (ch >> 6));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (ch >> 12));
            result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }

    if (result.empty())
        return "Box " + std::to_string(box + 1);

    return result;
}

std::string SaveFile::verifyRoundTrip() {
    if (originalFileData_.empty())
        return "No original data";

    // Re-encrypt blocks (no modifications have been made yet)
    std::vector<uint8_t> encrypted = SwishCrypto::encrypt(blocks_);

    std::string result;

    if (encrypted.size() != originalFileData_.size()) {
        result = "SIZE MISMATCH: encrypted=" + std::to_string(encrypted.size())
               + " original=" + std::to_string(originalFileData_.size());
    } else {
        // Compare byte-by-byte
        size_t diffCount = 0;
        size_t firstDiff = 0;
        for (size_t i = 0; i < encrypted.size(); i++) {
            if (encrypted[i] != originalFileData_[i]) {
                if (diffCount == 0)
                    firstDiff = i;
                diffCount++;
            }
        }

        if (diffCount == 0) {
            result = "OK";
        } else {
            // Check if the difference is only in the hash (last 32 bytes)
            size_t hashStart = originalFileData_.size() - 32;
            bool onlyHashDiffers = true;
            for (size_t i = 0; i < hashStart; i++) {
                if (encrypted[i] != originalFileData_[i]) {
                    onlyHashDiffers = false;
                    break;
                }
            }

            char buf[256];
            if (onlyHashDiffers) {
                std::snprintf(buf, sizeof(buf),
                    "HASH ONLY: payload matches but hash differs");
            } else {
                std::snprintf(buf, sizeof(buf),
                    "DIFF: %zu bytes differ, first at 0x%zX (enc=0x%02X orig=0x%02X)",
                    diffCount, firstDiff,
                    encrypted[firstDiff], originalFileData_[firstDiff]);
            }
            result = buf;
        }
    }

    // Free the original data
    originalFileData_.clear();
    originalFileData_.shrink_to_fit();

    return result;
}

// --- CRC16NoInvert (for LGPE BEEF block checksums) ---

static const uint16_t CRC16_TABLE[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040,
};

uint16_t SaveFile::crc16NoInvert(const uint8_t* data, size_t len) {
    uint16_t chk = 0;
    for (size_t i = 0; i < len; i++)
        chk = CRC16_TABLE[(uint8_t)(data[i] ^ chk)] ^ (chk >> 8);
    return chk;
}

// --- LGPE save format (flat binary with BEEF block checksums) ---

bool SaveFile::loadLGPE(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    auto fileSize = static_cast<size_t>(file.tellg());
    if (fileSize != LGPE_SAVE_SIZE)
        return false;

    file.seekg(0);
    rawData_.resize(fileSize);
    file.read(reinterpret_cast<char*>(rawData_.data()), fileSize);
    file.close();

    // Box data at fixed offset (party format, 0x104 per slot, 40 boxes * 25 slots)
    size_t boxDataEnd = LGPE_BOX_OFFSET + LGPE_BOX_SIZE;
    if (boxDataEnd > rawData_.size())
        return false;

    boxData_ = rawData_.data() + LGPE_BOX_OFFSET;
    boxDataLen_ = LGPE_BOX_SIZE;

    // LGPE has no custom box names
    boxLayoutData_ = nullptr;
    boxLayoutLen_ = 0;

    loaded_ = true;
    return true;
}

bool SaveFile::saveLGPE(const std::string& path) {
    if (rawData_.empty())
        return false;

    // Update PokeListHeader Count: scan all 1000 slots for highest occupied index
    int highestOccupied = -1;
    int totalSlots = LGPE_BOX_COUNT * LGPE_SLOTS_PER_BOX;
    for (int i = 0; i < totalSlots; i++) {
        int offset = i * PokeCrypto::SIZE_6PARTY;
        if (offset + PokeCrypto::SIZE_6PARTY > static_cast<int>(boxDataLen_))
            break;
        // Check if slot is occupied (EC != 0 or species != 0)
        uint32_t ec;
        std::memcpy(&ec, boxData_ + offset, 4);
        // Decrypt first 8 bytes to check — but the data is encrypted.
        // Use a simpler heuristic: decrypt and check species.
        Pokemon tmp;
        tmp.gameType_ = gameType_;
        tmp.loadFromEncrypted(boxData_ + offset, PokeCrypto::SIZE_6PARTY);
        if (!tmp.isEmpty())
            highestOccupied = i;
    }

    // Count = next empty slot index (highest + 1, or 0 if all empty)
    uint16_t count = static_cast<uint16_t>(highestOccupied + 1);
    // PokeListHeader: 6 party pointers (u16 each) + 1 starter pointer (u16) + 1 count (u16)
    // Count is at offset 7*2 = 14 within the header block
    size_t countOffset = LGPE_HEADER_OFFSET + 7 * 2;
    if (countOffset + 2 <= rawData_.size())
        std::memcpy(rawData_.data() + countOffset, &count, 2);

    // Recalculate CRC16NoInvert checksums for all 21 blocks
    for (int i = 0; i < LGPE_NUM_BLOCKS; i++) {
        uint16_t chk = crc16NoInvert(
            rawData_.data() + LGPE_BLOCKS[i].offset,
            LGPE_BLOCKS[i].length);
        // Checksum stored at: BLOCK_INFO_OFS + 0x14 + i*8 + 6
        size_t chkOffset = LGPE_BLOCK_INFO_OFS + 0x14 + i * 8 + 6;
        if (chkOffset + 2 <= rawData_.size())
            std::memcpy(rawData_.data() + chkOffset, &chk, 2);
    }

    // Write in-place (like BDSP)
    FILE* f = std::fopen(path.c_str(), "r+b");
    if (!f) {
        f = std::fopen(path.c_str(), "wb");
    }
    if (!f)
        return false;

    size_t written = std::fwrite(rawData_.data(), 1, rawData_.size(), f);
    std::fclose(f);
    return written == rawData_.size();
}
