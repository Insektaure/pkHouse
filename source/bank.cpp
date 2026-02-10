#include "bank.h"
#include <fstream>
#include <cstring>

Bank::Bank() {
    // All slots initialized to empty (zeroed data)
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
    if (version != VERSION) {
        return false; // Unsupported version
    }

    // Skip reserved
    file.seekg(HEADER_SIZE);

    // Read all slots (decrypted PA9 party data)
    for (int i = 0; i < TOTAL_SLOTS; i++) {
        file.read(reinterpret_cast<char*>(slots_[i].data.data()), SLOT_SIZE);
    }

    return file.good();
}

bool Bank::save(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;

    // Write header
    file.write(MAGIC, 8);
    file.write(reinterpret_cast<const char*>(&VERSION), 4);
    uint32_t reserved = 0;
    file.write(reinterpret_cast<const char*>(&reserved), 4);

    // Write all slots
    for (int i = 0; i < TOTAL_SLOTS; i++) {
        file.write(reinterpret_cast<const char*>(slots_[i].data.data()), SLOT_SIZE);
    }

    return file.good();
}

Pokemon Bank::getSlot(int box, int slot) const {
    int idx = slotIndex(box, slot);
    if (idx < 0 || idx >= TOTAL_SLOTS)
        return Pokemon{};
    return slots_[idx];
}

void Bank::setSlot(int box, int slot, const Pokemon& pkm) {
    int idx = slotIndex(box, slot);
    if (idx < 0 || idx >= TOTAL_SLOTS)
        return;
    slots_[idx] = pkm;
}

void Bank::clearSlot(int box, int slot) {
    int idx = slotIndex(box, slot);
    if (idx < 0 || idx >= TOTAL_SLOTS)
        return;
    slots_[idx] = Pokemon{};
}

std::string Bank::getBoxName(int box) const {
    return "Bank " + std::to_string(box + 1);
}
