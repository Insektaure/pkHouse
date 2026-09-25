#include "bank.h"
#include <vector>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <cstring>

Bank::Bank() {
    slots_.resize(boxCount_ * slotsPerBox_);
    boxNames_.resize(boxCount_);
}

void Bank::setGameType(GameType g) {
    gameType_ = g;
    auto& info   = gameInfo(g);
    boxCount_    = info.boxCount;
    slotsPerBox_ = info.slotsPerBox;
    slotSize_    = info.bankSlotSize;
    slots_.resize(boxCount_ * slotsPerBox_);
    boxNames_.resize(boxCount_);
}

bool Bank::isValidFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;

    // Verify magic
    char magic[8];
    if (!file.read(magic, 8))
        return false;
    if (std::memcmp(magic, MAGIC, 8) != 0)
        return false;

    // Verify version is one we understand
    uint32_t version = 0;
    if (!file.read(reinterpret_cast<char*>(&version), 4))
        return false;
    switch (version) {
        case VERSION_FRLG:
        case VERSION_LGPE:
        case VERSION_LA:
        case VERSION_40BOX:
        case VERSION_32BOX:
            return true;
        default:
            return false;
    }
}

bool Bank::layoutFor(uint32_t version, int& boxes, int& slotSize, int& perBox) {
    switch (version) {
        case VERSION_FRLG:  boxes = 14; slotSize = PokeCrypto::SIZE_3STORED; perBox = 30; return true;
        case VERSION_LGPE:  boxes = 40; slotSize = PokeCrypto::SIZE_6PARTY;  perBox = 25; return true;
        case VERSION_LA:    boxes = 32; slotSize = PokeCrypto::SIZE_8APARTY; perBox = 30; return true;
        case VERSION_40BOX: boxes = 40; slotSize = PokeCrypto::SIZE_9PARTY;  perBox = 30; return true;
        case VERSION_32BOX: boxes = 32; slotSize = PokeCrypto::SIZE_9PARTY;  perBox = 30; return true;
    }
    return false;
}

int Bank::countOccupiedInFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return 0;

    char magic[8];
    uint32_t version = 0;
    int boxes = 0, slotSize = 0, perBox = 0;
    if (std::fread(magic, 1, 8, f) != 8 || std::memcmp(magic, MAGIC, 8) != 0 ||
        std::fread(&version, 1, 4, f) != 4 || !layoutFor(version, boxes, slotSize, perBox) ||
        std::fseek(f, HEADER_SIZE, SEEK_SET) != 0) {
        std::fclose(f);
        return 0;
    }

    // All the slots in one read. A short file counts its missing slots as
    // empty, as load() leaves them.
    const size_t total = static_cast<size_t>(boxes) * perBox;
    std::vector<uint8_t> buf(total * slotSize, 0);
    const size_t got = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);

    // The same test as before: each slot in a Pokemon with the Bank's default
    // game type (the list never set one), asked isEmpty().
    int count = 0;
    Pokemon pkm;
    pkm.gameType_ = GameType::ZA;
    for (size_t i = 0; i < total; i++) {
        const size_t off = i * slotSize;
        if (off >= got) break;
        pkm.data.fill(0);
        std::memcpy(pkm.data.data(), buf.data() + off, std::min<size_t>(slotSize, got - off));
        if (!pkm.isEmpty()) count++;
    }
    return count;
}

bool Bank::isSlotEmpty(int box, int slot) const {
    const int idx = slotIndex(box, slot);
    if (idx < 0 || idx >= totalSlots()) return true;
    // getSlot() sets the game type on its copy; do the same test in place.
    const Pokemon& p = slots_[idx];
    if (p.gameType_ == gameType_) return p.isEmpty();
    Pokemon copy = p;
    copy.gameType_ = gameType_;
    return copy.isEmpty();
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
    if (!layoutFor(version, fileBoxCount, fileSlotSize, fileSlotsPerBox))
        return false; // Unsupported version

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
