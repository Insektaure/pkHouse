#include "bank.h"
#include <fstream>
#include <cstring>

Bank::Bank() {
    slots_.resize(boxCount_ * slotsPerBox_);
}

void Bank::setGameType(GameType g) {
    gameType_ = g;
    if (isLGPE(g)) {
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
    if (version == VERSION_LGPE) {
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

    return file.good();
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
    return "Bank " + std::to_string(box + 1);
}
