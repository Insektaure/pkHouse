#include "bank.h"
#include <fstream>
#include <cstring>

Bank::Bank() {
    slots_.resize(boxCount_ * slotsPerBox_);
    boxNames_.resize(boxCount_);
}

void Bank::setGameType(GameType g) {
    gameType_ = g;
    if (isFRLG(g)) {
        boxCount_ = 14;
        slotsPerBox_ = 30;
        slotSize_ = PokeCrypto::SIZE_3STORED;
    } else if (isLGPE(g)) {
        boxCount_ = 40;
        slotsPerBox_ = 25;
        slotSize_ = PokeCrypto::SIZE_6PARTY;
    } else if (isBDSP(g)) {
        boxCount_ = 40;
        slotsPerBox_ = 30;
        slotSize_ = PokeCrypto::SIZE_9PARTY;
    } else if (g == GameType::LA) {
        boxCount_ = 32;
        slotsPerBox_ = 30;
        slotSize_ = PokeCrypto::SIZE_8APARTY;
    } else {
        boxCount_ = 32;
        slotsPerBox_ = 30;
        slotSize_ = PokeCrypto::SIZE_9PARTY;
    }
    slots_.resize(boxCount_ * slotsPerBox_);
    boxNames_.resize(boxCount_);
}

bool Bank::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        // File doesn't exist - start with empty bank
        return true;
    }

    // Read and verify header
    char magic[8];
    file.read(magic, 8);
    if (std::memcmp(magic, MAGIC, 8) != 0) {
        return false; // Invalid file
    }

    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), 4);

    int fileBoxCount;
    int fileSlotSize;
    int fileSlotsPerBox;
    if (version == VERSION_FRLG) {
        fileBoxCount = 14;
        fileSlotSize = PokeCrypto::SIZE_3STORED;
        fileSlotsPerBox = 30;
    } else if (version == VERSION_LGPE) {
        fileBoxCount = 40;
        fileSlotSize = PokeCrypto::SIZE_6PARTY;
        fileSlotsPerBox = 25;
    } else if (version == VERSION_LA) {
        fileBoxCount = 32;
        fileSlotSize = PokeCrypto::SIZE_8APARTY;
        fileSlotsPerBox = 30;
    } else if (version == VERSION_40BOX) {
        fileBoxCount = 40;
        fileSlotSize = PokeCrypto::SIZE_9PARTY;
        fileSlotsPerBox = 30;
    } else if (version == VERSION_32BOX) {
        fileBoxCount = 32;
        fileSlotSize = PokeCrypto::SIZE_9PARTY;
        fileSlotsPerBox = 30;
    } else {
        return false; // Unsupported version
    }

    // Use the file's parameters
    boxCount_ = fileBoxCount;
    slotSize_ = fileSlotSize;
    slotsPerBox_ = fileSlotsPerBox;
    slots_.resize(boxCount_ * slotsPerBox_);

    // Skip reserved
    file.seekg(HEADER_SIZE);

    // Read all slots (decrypted data)
    int total = totalSlots();
    for (int i = 0; i < total; i++) {
        file.read(reinterpret_cast<char*>(slots_[i].data.data()), slotSize_);
    }

    // Read box names if present (appended after slot data)
    boxNames_.resize(boxCount_);
    for (int i = 0; i < boxCount_; i++) {
        char nameBuf[BOX_NAME_SIZE] = {};
        if (!file.read(nameBuf, BOX_NAME_SIZE))
            break; // Old file without names — leave remaining as empty
        // Find null terminator or use full buffer
        int len = 0;
        while (len < BOX_NAME_SIZE && nameBuf[len] != '\0') len++;
        boxNames_[i] = std::string(nameBuf, len);
    }

    return true;
}

bool Bank::save(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;

    // Write header
    file.write(MAGIC, 8);
    uint32_t ver = fileVersion();
    file.write(reinterpret_cast<const char*>(&ver), 4);
    uint32_t reserved = 0;
    file.write(reinterpret_cast<const char*>(&reserved), 4);

    // Write all slots
    int total = totalSlots();
    for (int i = 0; i < total; i++) {
        file.write(reinterpret_cast<const char*>(slots_[i].data.data()), slotSize_);
    }

    // Write box names (16 bytes each, null-padded)
    for (int i = 0; i < boxCount_; i++) {
        char nameBuf[BOX_NAME_SIZE] = {};
        if (i < (int)boxNames_.size()) {
            std::memcpy(nameBuf, boxNames_[i].c_str(),
                        std::min((int)boxNames_[i].size(), BOX_NAME_SIZE));
        }
        file.write(nameBuf, BOX_NAME_SIZE);
    }

    return file.good();
}

Pokemon Bank::getSlot(int box, int slot) const {
    int idx = slotIndex(box, slot);
    if (idx < 0 || idx >= totalSlots())
        return Pokemon{};
    Pokemon pkm = slots_[idx];
    pkm.gameType_ = gameType_;
    return pkm;
}

void Bank::setSlot(int box, int slot, const Pokemon& pkm) {
    int idx = slotIndex(box, slot);
    if (idx < 0 || idx >= totalSlots())
        return;
    slots_[idx] = pkm;
}

void Bank::clearSlot(int box, int slot) {
    int idx = slotIndex(box, slot);
    if (idx < 0 || idx >= totalSlots())
        return;
    slots_[idx] = Pokemon{};
}

std::string Bank::getBoxName(int box) const {
    if (box >= 0 && box < (int)boxNames_.size() && !boxNames_[box].empty())
        return boxNames_[box];
    return "Bank " + std::to_string(box + 1);
}

void Bank::setBoxName(int box, const std::string& name) {
    if (box < 0 || box >= (int)boxNames_.size())
        return;
    if ((int)name.size() > BOX_NAME_SIZE)
        boxNames_[box] = name.substr(0, BOX_NAME_SIZE);
    else
        boxNames_[box] = name;
}
