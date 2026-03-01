#include "save_file.h"
#include "md5.h"
#include <fstream>
#include <cstdio>
#include <cstring>

void SaveFile::setGameType(GameType game) {
    gameType_ = game;
    kbox_ = 0x0d66012c; // Reset to default; LA overrides below
    kparty_ = 0;
    partySlotSize_ = 0;
    slotsPerBox_ = 30;   // Default; LGPE overrides below
    if (game == GameType::ZA) {
        gapBoxSlot_  = 0x40;
        sizeBoxSlot_ = PokeCrypto::SIZE_9PARTY + 0x40;
        boxCount_    = 32;
        kparty_      = 0x3AA1A9AD;
        partySlotSize_ = 0x1E0;  // 0x158 + 0x88 gap
    } else if (isSV(game)) {
        gapBoxSlot_  = 0;
        sizeBoxSlot_ = PokeCrypto::SIZE_9PARTY;
        boxCount_    = 32;
        kparty_      = 0x3AA1A9AD;
        partySlotSize_ = PokeCrypto::SIZE_9PARTY;
    } else if (isSwSh(game)) {
        gapBoxSlot_  = 0;
        sizeBoxSlot_ = PokeCrypto::SIZE_9PARTY;
        boxCount_    = 32;
        kparty_      = 0x2985fe5d;
        partySlotSize_ = PokeCrypto::SIZE_9PARTY;
    } else if (isBDSP(game)) {
        gapBoxSlot_  = 0;
        sizeBoxSlot_ = PokeCrypto::SIZE_9PARTY;
        boxCount_    = BDSP_BOX_COUNT;
        partySlotSize_ = PokeCrypto::SIZE_9PARTY;
    } else if (game == GameType::LA) {
        gapBoxSlot_  = 0;
        sizeBoxSlot_ = PokeCrypto::SIZE_8ASTORED;
        boxCount_    = 32;
        kbox_        = 0x47E1CEAB;
        kparty_      = 0x2985fe5d;
        partySlotSize_ = PokeCrypto::SIZE_8APARTY;
    } else if (isLGPE(game)) {
        gapBoxSlot_  = 0;
        sizeBoxSlot_ = PokeCrypto::SIZE_6PARTY;
        boxCount_    = LGPE_BOX_COUNT;
        slotsPerBox_ = LGPE_SLOTS_PER_BOX;
        // LGPE: no separate party (party members are pointer-indexed box slots)
    } else if (isFRLG(game)) {
        gapBoxSlot_  = 0;
        sizeBoxSlot_ = PokeCrypto::SIZE_3STORED;  // 80 bytes per slot
        boxCount_    = GBA_BOX_COUNT;              // 14 boxes
        slotsPerBox_ = GBA_SLOTS_PER_BOX;         // 30 slots
        partySlotSize_ = PokeCrypto::SIZE_3PARTY;  // 100 bytes per party slot
    } else {
        // Fallback (should not be reached)
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
    partyData_ = nullptr;
    partyDataLen_ = 0;

    if (isFRLG(gameType_))
        return loadGBA(path);
    if (isBDSP(gameType_))
        return loadBDSP(path);
    if (isLGPE(gameType_))
        return loadLGPE(path);
    return loadSCBlock(path);
}

bool SaveFile::save(const std::string& path) {
    if (!loaded_)
        return false;

    if (isFRLG(gameType_))
        return saveGBA(path);
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

    // Find party block
    if (kparty_) {
        SCBlock* partyBlock = SwishCrypto::findBlock(blocks_, kparty_);
        if (partyBlock) {
            partyData_ = partyBlock->data.data();
            partyDataLen_ = partyBlock->data.size();
        }
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

    // Party data at fixed offset
    size_t partyEnd = BDSP_PARTY_OFFSET + PARTY_SLOTS * PokeCrypto::SIZE_9PARTY + 1;
    if (partyEnd <= rawData_.size()) {
        partyData_ = rawData_.data() + BDSP_PARTY_OFFSET;
        partyDataLen_ = partyEnd - BDSP_PARTY_OFFSET;
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

    if (isFRLG(gameType_) || isLGPE(gameType_)) {
        // FRLG/LGPE: empty slots are all-zero bytes (not encrypted blank).
        std::memset(boxData_ + offset, 0, sizeBoxSlot_);
    } else {
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
}

// Gen3 English character table (same as in pokemon.cpp — G3_EN)
// We only need the common characters for box names here.
static const uint16_t G3_EN_SAVE[256] = {
    0x0020, 0x00C0, 0x00C1, 0x00C2, 0x00C7, 0x00C8, 0x00C9, 0x00CA,
    0x00CB, 0x00CC, 0x3053, 0x00CE, 0x00CF, 0x00D2, 0x00D3, 0x00D4,
    0x0152, 0x00D9, 0x00DA, 0x00DB, 0x00D1, 0x00DF, 0x00E0, 0x00E1,
    0x306D, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x307E,
    0x00EE, 0x00EF, 0x00F2, 0x00F3, 0x00F4, 0x0153, 0x00F9, 0x00FA,
    0x00FB, 0x00F1, 0x00BA, 0x00AA, 0x2469, 0x0026, 0x002B, 0x3042,
    0x3043, 0x3045, 0x3047, 0x3049, 0x3083, 0x003D, 0x003B, 0x304C,
    0x304E, 0x3050, 0x3052, 0x3054, 0x3056, 0x3058, 0x305A, 0x305C,
    0x305E, 0x3060, 0x3062, 0x3065, 0x3067, 0x3069, 0x3070, 0x3073,
    0x3076, 0x3079, 0x307C, 0x3071, 0x3074, 0x3077, 0x307A, 0x307D,
    0x3063, 0x00BF, 0x00A1, 0x2483, 0x2484, 0x30AA, 0x30AB, 0x30AD,
    0x30AF, 0x30B1, 0x00CD, 0x0025, 0x0028, 0x0029, 0x30BB, 0x30BD,
    0x30BF, 0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC,
    0x00E2, 0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x00ED,
    0x30DF, 0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9,
    0x30EA, 0x2191, 0x2193, 0x2190, 0xFF0B, 0x30F2, 0x30F3, 0x30A1,
    0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x2482, 0x003C, 0x003E, 0x30AC,
    0x30AE, 0x30B0, 0x30B2, 0x30B4, 0x30B6, 0x30B8, 0x30BA, 0x30BC,
    0x30BE, 0x30C0, 0x30C2, 0x30C5, 0x30C7, 0x30C9, 0x30D0, 0x30D3,
    0x30D6, 0x30D9, 0x30DC, 0x30D1, 0x30D4, 0x30D7, 0x30DA, 0x30DD,
    0x30C3, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036,
    0x0037, 0x0038, 0x0039, 0x0021, 0x003F, 0x002E, 0x002D, 0xFF65,
    0x246C, 0x201C, 0x201D, 0x2018, 0x0027, 0x2642, 0x2640, 0x0024,
    0x002C, 0x2467, 0x002F, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045,
    0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D,
    0x004E, 0x004F, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055,
    0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x0061, 0x0062, 0x0063,
    0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B,
    0x006C, 0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073,
    0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x25BA,
    0x003A, 0x00C4, 0x00D6, 0x00DC, 0x00E4, 0x00F6, 0x00FC, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

static std::string decodeGen3String(const uint8_t* p, int maxBytes) {
    std::string result;
    for (int i = 0; i < maxBytes; i++) {
        uint8_t b = p[i];
        if (b == 0xFF) break;
        uint16_t ch = G3_EN_SAVE[b];
        if (ch == 0) break;
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
    return result;
}

std::string SaveFile::getBoxName(int box) const {
    if (box < 0 || box >= boxCount_)
        return "Box " + std::to_string(box + 1);

    if (isFRLG(gameType_)) {
        // FRLG: box names stored after all pokemon data in Gen3 encoding
        if (!boxLayoutData_)
            return "Box " + std::to_string(box + 1);
        int nameOfs = box * GBA_BOXNAME_LEN;
        if (nameOfs + GBA_BOXNAME_LEN > static_cast<int>(boxLayoutLen_))
            return "Box " + std::to_string(box + 1);
        std::string name = decodeGen3String(boxLayoutData_ + nameOfs, GBA_BOXNAME_LEN);
        if (name.empty())
            return "Box " + std::to_string(box + 1);
        return name;
    }

    if (!boxLayoutData_)
        return "Box " + std::to_string(box + 1);

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

    // --- CompressStorage ---
    // LGPE stores Pokemon as a sequential flat list; gaps are not legal.
    // Pack all occupied slots to the beginning (matching PKHeX CompressStorage).
    int totalSlots = LGPE_BOX_COUNT * LGPE_SLOTS_PER_BOX;
    int slotSize = PokeCrypto::SIZE_6PARTY;

    // Build old→new index mapping for pointer updates
    std::vector<int> oldToNew(totalSlots, -1);
    int writeIdx = 0;

    for (int i = 0; i < totalSlots; i++) {
        int offset = i * slotSize;
        if (offset + slotSize > static_cast<int>(boxDataLen_))
            break;

        // EC (first 4 bytes) is unencrypted; EC==0 means empty slot
        uint32_t ec;
        std::memcpy(&ec, boxData_ + offset, 4);
        if (ec == 0)
            continue;

        oldToNew[i] = writeIdx;
        if (writeIdx != i) {
            int dstOffset = writeIdx * slotSize;
            std::memmove(boxData_ + dstOffset, boxData_ + offset, slotSize);
        }
        writeIdx++;
    }

    // Zero remaining slots after the last occupied one
    for (int i = writeIdx; i < totalSlots; i++) {
        int offset = i * slotSize;
        if (offset + slotSize <= static_cast<int>(boxDataLen_))
            std::memset(boxData_ + offset, 0, slotSize);
    }

    // --- Update PokeListHeader ---
    // Layout: 6 party pointers (u16) + 1 starter pointer (u16) + count (u16)
    // Update party and starter pointers using the index mapping
    for (int i = 0; i < 7; i++) {
        size_t ptrOfs = LGPE_HEADER_OFFSET + i * 2;
        if (ptrOfs + 2 > rawData_.size())
            break;

        uint16_t oldIdx;
        std::memcpy(&oldIdx, rawData_.data() + ptrOfs, 2);

        if (oldIdx < static_cast<uint16_t>(totalSlots) && oldToNew[oldIdx] >= 0) {
            uint16_t newIdx = static_cast<uint16_t>(oldToNew[oldIdx]);
            std::memcpy(rawData_.data() + ptrOfs, &newIdx, 2);
        }
    }

    // Update count
    uint16_t count = static_cast<uint16_t>(writeIdx);
    size_t countOfs = LGPE_HEADER_OFFSET + 7 * 2;
    if (countOfs + 2 <= rawData_.size())
        std::memcpy(rawData_.data() + countOfs, &count, 2);

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

// --- GBA save format (sector-based, for FRLG) ---

static inline uint16_t readU16LE(const uint8_t* p) {
    uint16_t v; std::memcpy(&v, p, 2); return v;
}
static inline uint32_t readU32LE(const uint8_t* p) {
    uint32_t v; std::memcpy(&v, p, 4); return v;
}
static inline void writeU16LE(uint8_t* p, uint16_t v) {
    std::memcpy(p, &v, 2);
}

uint16_t SaveFile::checkSum32GBA(const uint8_t* data, size_t len) {
    uint32_t chk = 0;
    for (size_t i = 0; i + 3 < len; i += 4) {
        uint32_t v;
        std::memcpy(&v, data + i, 4);
        chk += v;
    }
    return static_cast<uint16_t>(chk + (chk >> 16));
}

bool SaveFile::loadGBA(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    auto fileSize = static_cast<size_t>(file.tellg());
    if (fileSize != GBA_SAVE_SIZE)
        return false;

    file.seekg(0);
    rawData_.resize(GBA_SAVE_SIZE);
    file.read(reinterpret_cast<char*>(rawData_.data()), GBA_SAVE_SIZE);
    file.close();

    // Determine active save slot by comparing save counters at sector 0
    // Each slot = 14 sectors of 0x1000 bytes
    uint32_t counter[2] = {0, 0};
    bool valid[2] = {false, false};

    for (int slot = 0; slot < 2; slot++) {
        int slotBase = slot * GBA_SECTOR_COUNT * GBA_SECTOR_SIZE;
        int bitTrack = 0;
        for (int i = 0; i < GBA_SECTOR_COUNT; i++) {
            int sectorOfs = slotBase + i * GBA_SECTOR_SIZE;
            uint16_t id = readU16LE(rawData_.data() + sectorOfs + GBA_OFS_SECTOR_ID);
            if (id < GBA_SECTOR_COUNT)
                bitTrack |= (1 << id);
            if (id == 0)
                counter[slot] = readU32LE(rawData_.data() + sectorOfs + GBA_OFS_SAVE_INDEX);
        }
        valid[slot] = (bitTrack == 0x3FFF); // all 14 sectors present
    }

    if (!valid[0] && !valid[1])
        return false;
    if (!valid[0]) gbaActiveSlot_ = 1;
    else if (!valid[1]) gbaActiveSlot_ = 0;
    else gbaActiveSlot_ = (counter[1] > counter[0]) ? 1 : 0;

    // Build contiguous storage buffer from sectors 5-13 of active slot
    gbaStorage_.resize(GBA_STORAGE_SECTORS * GBA_SECTOR_USED);
    std::memset(gbaStorage_.data(), 0, gbaStorage_.size());

    int slotBase = gbaActiveSlot_ * GBA_SECTOR_COUNT * GBA_SECTOR_SIZE;
    for (int i = 0; i < GBA_SECTOR_COUNT; i++) {
        int sectorOfs = slotBase + i * GBA_SECTOR_SIZE;
        uint16_t id = readU16LE(rawData_.data() + sectorOfs + GBA_OFS_SECTOR_ID);
        if (id >= GBA_STORAGE_FIRST && id <= GBA_STORAGE_LAST) {
            int storageIdx = id - GBA_STORAGE_FIRST;
            std::memcpy(gbaStorage_.data() + storageIdx * GBA_SECTOR_USED,
                        rawData_.data() + sectorOfs,
                        GBA_SECTOR_USED);
        }
    }

    // Box data starts at offset 4 in storage (first 4 bytes = current box index)
    boxData_ = gbaStorage_.data() + 4;
    boxDataLen_ = gbaStorage_.size() - 4;

    // Box names start after all pokemon data:
    // 14 boxes * 30 slots * 80 bytes = 33600 bytes from boxData_
    int boxNameOfs = GBA_BOX_COUNT * GBA_SLOTS_PER_BOX * PokeCrypto::SIZE_3STORED;
    if (boxNameOfs + GBA_BOX_COUNT * GBA_BOXNAME_LEN <= static_cast<int>(boxDataLen_)) {
        boxLayoutData_ = boxData_ + boxNameOfs;
        boxLayoutLen_ = GBA_BOX_COUNT * GBA_BOXNAME_LEN;
    } else {
        boxLayoutData_ = nullptr;
        boxLayoutLen_ = 0;
    }

    // Assemble Large buffer from sectors 1-4 (for party data)
    gbaLarge_.resize(4 * GBA_SECTOR_USED);
    std::memset(gbaLarge_.data(), 0, gbaLarge_.size());
    for (int i = 0; i < GBA_SECTOR_COUNT; i++) {
        int sectorOfs = slotBase + i * GBA_SECTOR_SIZE;
        uint16_t id = readU16LE(rawData_.data() + sectorOfs + GBA_OFS_SECTOR_ID);
        if (id >= 1 && id <= 4) {
            std::memcpy(gbaLarge_.data() + (id - 1) * GBA_SECTOR_USED,
                        rawData_.data() + sectorOfs,
                        GBA_SECTOR_USED);
        }
    }
    // Party data at offset 0x038 in Large buffer
    partyData_ = gbaLarge_.data() + 0x038;
    partyDataLen_ = PARTY_SLOTS * PokeCrypto::SIZE_3PARTY;

    loaded_ = true;
    return true;
}

bool SaveFile::saveGBA(const std::string& path) {
    if (rawData_.empty() || gbaStorage_.empty())
        return false;

    // Write modified storage back to sectors in BOTH save slots
    for (int slot = 0; slot < 2; slot++) {
        int slotBase = slot * GBA_SECTOR_COUNT * GBA_SECTOR_SIZE;

        // Copy sector structure from active slot if writing to the other slot
        if (slot != gbaActiveSlot_) {
            int activeBase = gbaActiveSlot_ * GBA_SECTOR_COUNT * GBA_SECTOR_SIZE;
            std::memcpy(rawData_.data() + slotBase,
                        rawData_.data() + activeBase,
                        GBA_SECTOR_COUNT * GBA_SECTOR_SIZE);
        }

        // Write storage data back to sectors 5-13 and Large data back to sectors 1-4
        for (int i = 0; i < GBA_SECTOR_COUNT; i++) {
            int sectorOfs = slotBase + i * GBA_SECTOR_SIZE;
            uint16_t id = readU16LE(rawData_.data() + sectorOfs + GBA_OFS_SECTOR_ID);
            if (id >= GBA_STORAGE_FIRST && id <= GBA_STORAGE_LAST) {
                int storageIdx = id - GBA_STORAGE_FIRST;
                std::memcpy(rawData_.data() + sectorOfs,
                            gbaStorage_.data() + storageIdx * GBA_SECTOR_USED,
                            GBA_SECTOR_USED);
            } else if (id >= 1 && id <= 4) {
                std::memcpy(rawData_.data() + sectorOfs,
                            gbaLarge_.data() + (id - 1) * GBA_SECTOR_USED,
                            GBA_SECTOR_USED);
            }
        }

        // Recalculate checksums for all 14 sectors
        for (int i = 0; i < GBA_SECTOR_COUNT; i++) {
            int sectorOfs = slotBase + i * GBA_SECTOR_SIZE;
            uint16_t chk = checkSum32GBA(rawData_.data() + sectorOfs, GBA_SECTOR_USED);
            writeU16LE(rawData_.data() + sectorOfs + GBA_OFS_CHECKSUM, chk);
        }
    }

    // Write in-place
    FILE* f = std::fopen(path.c_str(), "r+b");
    if (!f)
        f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;

    size_t written = std::fwrite(rawData_.data(), 1, rawData_.size(), f);
    std::fclose(f);
    return written == rawData_.size();
}

// --- Party access ---

bool SaveFile::hasParty() const {
    return partyData_ != nullptr && partySlotSize_ > 0;
}

int SaveFile::partyCount() const {
    if (!hasParty())
        return 0;

    if (gameType_ == GameType::ZA) {
        // ZA: auto-detect by scanning for non-empty slots
        int count = 0;
        for (int i = 0; i < PARTY_SLOTS; i++) {
            int ofs = i * partySlotSize_;
            if (ofs + 4 > static_cast<int>(partyDataLen_))
                break;
            uint32_t ec;
            std::memcpy(&ec, partyData_ + ofs, 4);
            if (ec != 0)
                count++;
        }
        return count;
    }

    if (isFRLG(gameType_)) {
        // FRLG: party count at Large[0x034]
        if (gbaLarge_.size() > 0x034)
            return gbaLarge_[0x034];
        return 0;
    }

    // SV, SwSh, LA, BDSP: count byte after the 6 party slots
    size_t countOfs = static_cast<size_t>(PARTY_SLOTS) * partySlotSize_;
    if (countOfs < partyDataLen_)
        return partyData_[countOfs];
    return 0;
}

void SaveFile::setPartyCount(int count) {
    if (!hasParty() || count < 0 || count > PARTY_SLOTS)
        return;

    if (gameType_ == GameType::ZA) {
        // ZA: count is auto-detected, nothing to write
        return;
    }

    if (isFRLG(gameType_)) {
        if (gbaLarge_.size() > 0x034)
            gbaLarge_[0x034] = static_cast<uint8_t>(count);
        return;
    }

    size_t countOfs = static_cast<size_t>(PARTY_SLOTS) * partySlotSize_;
    if (countOfs < partyDataLen_)
        partyData_[countOfs] = static_cast<uint8_t>(count);
}

Pokemon SaveFile::getPartySlot(int slot) const {
    Pokemon pkm;
    if (!hasParty() || slot < 0 || slot >= PARTY_SLOTS)
        return pkm;

    int offset = slot * partySlotSize_;
    if (offset + partySlotSize_ > static_cast<int>(partyDataLen_))
        return pkm;

    pkm.gameType_ = gameType_;
    // Party data uses party-size format (larger than stored)
    int dataSize = isFRLG(gameType_) ? PokeCrypto::SIZE_3PARTY
                 : (gameType_ == GameType::LA) ? PokeCrypto::SIZE_8APARTY
                 : PokeCrypto::SIZE_9PARTY;
    pkm.loadFromEncrypted(partyData_ + offset, dataSize);
    return pkm;
}

void SaveFile::setPartySlot(int slot, const Pokemon& pkm) {
    if (!hasParty() || slot < 0 || slot >= PARTY_SLOTS)
        return;

    int offset = slot * partySlotSize_;
    if (offset + partySlotSize_ > static_cast<int>(partyDataLen_))
        return;

    // Zero the full slot first (clears stale party stats extension and gap bytes)
    std::memset(partyData_ + offset, 0, partySlotSize_);

    Pokemon copy = pkm;
    copy.gameType_ = gameType_;

    // Calculate party stats (FRLG: fills bytes 80-99, LA: fills 0x168-0x175 + HPCurrent)
    copy.calcPartyStats();

    if (isFRLG(gameType_)) {
        // FRLG: getEncrypted writes SIZE_3STORED (80 bytes) but party slots are
        // SIZE_3PARTY (100 bytes). The extra 20 bytes are plaintext party stats
        // (level, HP, all stats). Copy them from the Pokemon data after encrypting.
        copy.getEncrypted(partyData_ + offset);
        std::memcpy(partyData_ + offset + PokeCrypto::SIZE_3STORED,
                    copy.data.data() + PokeCrypto::SIZE_3STORED,
                    PokeCrypto::SIZE_3PARTY - PokeCrypto::SIZE_3STORED);
    } else if (gameType_ == GameType::LA) {
        // LA: getEncrypted writes SIZE_8ASTORED (0x168) but party slots are
        // SIZE_8APARTY (0x178). The extra 0x10 bytes are LCG-encrypted party stats.
        // Encrypt with full party size to include them.
        copy.refreshChecksum();
        PokeCrypto::encryptArray8A(copy.data.data(), PokeCrypto::SIZE_8APARTY,
                                   partyData_ + offset);
    } else {
        // Gen9 (ZA/SV/SwSh), BDSP: getEncrypted writes SIZE_9PARTY which
        // matches the encrypted party format. No extra handling needed.
        copy.getEncrypted(partyData_ + offset);
    }

    // Compact and update count
    compactParty();
}

void SaveFile::clearPartySlot(int slot) {
    if (!hasParty() || slot < 0 || slot >= PARTY_SLOTS)
        return;

    int offset = slot * partySlotSize_;
    if (offset + partySlotSize_ > static_cast<int>(partyDataLen_))
        return;

    std::memset(partyData_ + offset, 0, partySlotSize_);

    // Compact and update count
    compactParty();
}

void SaveFile::compactParty() {
    if (!hasParty())
        return;

    // Shift non-empty slots to the front, zero trailing slots
    uint8_t temp[PARTY_SLOTS][PokeCrypto::MAX_PARTY_SIZE + 0x88] = {};
    int writeIdx = 0;

    for (int i = 0; i < PARTY_SLOTS; i++) {
        int ofs = i * partySlotSize_;
        if (ofs + partySlotSize_ > static_cast<int>(partyDataLen_))
            break;
        // Check if slot is non-empty (EC != 0 for encrypted data)
        uint32_t ec;
        std::memcpy(&ec, partyData_ + ofs, 4);
        if (ec != 0) {
            std::memcpy(temp[writeIdx], partyData_ + ofs, partySlotSize_);
            writeIdx++;
        }
    }

    // Write back compacted slots
    for (int i = 0; i < PARTY_SLOTS; i++) {
        int ofs = i * partySlotSize_;
        if (ofs + partySlotSize_ > static_cast<int>(partyDataLen_))
            break;
        if (i < writeIdx)
            std::memcpy(partyData_ + ofs, temp[i], partySlotSize_);
        else
            std::memset(partyData_ + ofs, 0, partySlotSize_);
    }

    setPartyCount(writeIdx);
}
