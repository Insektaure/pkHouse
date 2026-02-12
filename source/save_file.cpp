#include "save_file.h"
#include "md5.h"
#include <fstream>
#include <cstdio>
#include <cstring>

void SaveFile::setGameType(GameType game) {
    gameType_ = game;
    kbox_ = 0x0d66012c; // Reset to default; LA overrides below
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
    return loadSCBlock(path);
}

bool SaveFile::save(const std::string& path) {
    if (!loaded_)
        return false;

    if (isBDSP(gameType_))
        return saveBDSP(path);
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
        static_cast<size_t>(BDSP_BOX_COUNT) * SLOTS_PER_BOX * PokeCrypto::SIZE_9PARTY;
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
