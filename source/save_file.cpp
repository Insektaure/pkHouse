#include "save_file.h"
#include <fstream>
#include <cstring>

bool SaveFile::load(const std::string& path) {
    filePath_ = path;

    // Read entire file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    auto fileSize = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> fileData(fileSize);
    file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    file.close();

    // Decrypt into SCBlocks
    blocks_ = SwishCrypto::decrypt(fileData.data(), fileData.size());

    // Find box data block
    SCBlock* boxBlock = SwishCrypto::findBlock(blocks_, KBOX);
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

bool SaveFile::save(const std::string& path) {
    if (!loaded_)
        return false;

    std::vector<uint8_t> encrypted = SwishCrypto::encrypt(blocks_);

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;

    file.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
    return file.good();
}

Pokemon SaveFile::getBoxSlot(int box, int slot) const {
    Pokemon pkm;
    if (!loaded_ || !boxData_)
        return pkm;

    int offset = getBoxSlotOffset(box, slot);
    if (offset + PokeCrypto::SIZE_9PARTY > static_cast<int>(boxDataLen_))
        return pkm;

    // Box data stores Pokemon in party format, encrypted
    pkm.loadFromEncrypted(boxData_ + offset, PokeCrypto::SIZE_9PARTY);
    return pkm;
}

void SaveFile::setBoxSlot(int box, int slot, const Pokemon& pkm) {
    if (!loaded_ || !boxData_)
        return;

    int offset = getBoxSlotOffset(box, slot);
    if (offset + SIZE_BOXSLOT > static_cast<int>(boxDataLen_))
        return;

    // Encrypt and write party data
    pkm.getEncrypted(boxData_ + offset);
    // Zero the gap bytes
    std::memset(boxData_ + offset + PokeCrypto::SIZE_9PARTY, 0, GAP_BOX_SLOT);
}

void SaveFile::clearBoxSlot(int box, int slot) {
    if (!loaded_ || !boxData_)
        return;

    int offset = getBoxSlotOffset(box, slot);
    if (offset + SIZE_BOXSLOT > static_cast<int>(boxDataLen_))
        return;

    std::memset(boxData_ + offset, 0, SIZE_BOXSLOT);
}

std::string SaveFile::getBoxName(int box) const {
    if (!boxLayoutData_ || box < 0 || box >= BOX_COUNT) {
        return "Box " + std::to_string(box + 1);
    }

    // Box names are stored as UTF-16LE, 0x22 bytes per name
    // From BoxLayout9a.cs: SAV6.LongStringLength = 0x22
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
