#include "bank.h"
#include <fstream>
#include <cstring>

Bank::Bank() {
    slots_.resize(boxCount_ * SLOTS_PER_BOX);
}

void Bank::setGameType(GameType g) {
    gameType_ = g;
    int newBoxCount = (g == GameType::BDSP) ? 40 : 32;
    slotSize_ = (g == GameType::LA) ? PokeCrypto::SIZE_8APARTY : PokeCrypto::SIZE_9PARTY;
    if (newBoxCount != boxCount_) {
        boxCount_ = newBoxCount;
        slots_.resize(boxCount_ * SLOTS_PER_BOX);
    }
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
    if (version == VERSION_LA) {
        fileBoxCount = 32;
        fileSlotSize = PokeCrypto::SIZE_8APARTY;
    } else if (version == VERSION_40BOX) {
        fileBoxCount = 40;
        fileSlotSize = PokeCrypto::SIZE_9PARTY;
    } else if (version == VERSION_32BOX) {
        fileBoxCount = 32;
        fileSlotSize = PokeCrypto::SIZE_9PARTY;
    } else {
        return false; // Unsupported version
    }

    // Use the file's parameters
    boxCount_ = fileBoxCount;
    slotSize_ = fileSlotSize;
    slots_.resize(boxCount_ * SLOTS_PER_BOX);

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
