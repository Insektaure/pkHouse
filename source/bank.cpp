#include "bank.h"
#include <fstream>
#include <cstring>

Bank::Bank() {
    slots_.resize(boxCount_ * slotsPerBox_);
    boxNames_.resize(boxCount_);
}

void Bank::setGameType(GameType g) {
    gameType_ = g;
    isUniversal_ = false;
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
    slotOrigins_.clear();
    boxNames_.resize(boxCount_);
}

void Bank::setUniversal() {
    isUniversal_ = true;
    boxCount_ = 32;
    slotsPerBox_ = 30;
    slotSize_ = UNIVERSAL_SLOT_SIZE;
    slots_.resize(boxCount_ * slotsPerBox_);
    slotOrigins_.resize(boxCount_ * slotsPerBox_, GameType::ZA);
    boxNames_.resize(boxCount_);
}

uint8_t Bank::encodeGameType(GameType g) {
    switch (g) {
        case GameType::ZA: return 0;
        case GameType::S:  return 1;
        case GameType::V:  return 2;
        case GameType::Sw: return 3;
        case GameType::Sh: return 4;
        case GameType::BD: return 5;
        case GameType::SP: return 6;
        case GameType::LA: return 7;
        case GameType::GP: return 8;
        case GameType::GE: return 9;
        case GameType::FR: return 10;
        case GameType::LG: return 11;
    }
    return 0;
}

GameType Bank::decodeGameType(uint8_t v) {
    switch (v) {
        case 0:  return GameType::ZA;
        case 1:  return GameType::S;
        case 2:  return GameType::V;
        case 3:  return GameType::Sw;
        case 4:  return GameType::Sh;
        case 5:  return GameType::BD;
        case 6:  return GameType::SP;
        case 7:  return GameType::LA;
        case 8:  return GameType::GP;
        case 9:  return GameType::GE;
        case 10: return GameType::FR;
        case 11: return GameType::LG;
    }
    return GameType::ZA;
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

    if (version == VERSION_UNIVERSAL) {
        // Universal bank
        isUniversal_ = true;
        boxCount_ = 32;
        slotsPerBox_ = 30;
        slotSize_ = UNIVERSAL_SLOT_SIZE;
        slots_.resize(boxCount_ * slotsPerBox_);
        slotOrigins_.resize(boxCount_ * slotsPerBox_, GameType::ZA);

        file.seekg(HEADER_SIZE);

        int total = totalSlots();
        for (int i = 0; i < total; i++) {
            uint8_t tag = 0;
            file.read(reinterpret_cast<char*>(&tag), 1);
            slotOrigins_[i] = decodeGameType(tag);

            file.read(reinterpret_cast<char*>(slots_[i].data.data()),
                       PokeCrypto::MAX_PARTY_SIZE);
            slots_[i].gameType_ = slotOrigins_[i];
        }

        // Read box names
        boxNames_.resize(boxCount_);
        for (int i = 0; i < boxCount_; i++) {
            char nameBuf[BOX_NAME_SIZE] = {};
            if (!file.read(nameBuf, BOX_NAME_SIZE))
                break;
            int len = 0;
            while (len < BOX_NAME_SIZE && nameBuf[len] != '\0') len++;
            boxNames_[i] = std::string(nameBuf, len);
        }
        return true;
    }

    int fileBoxCount;
    int fileSlotSize;
    int fileSlotsPerBox;
    isUniversal_ = false;
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
    slotOrigins_.clear();

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

    int total = totalSlots();

    if (isUniversal_) {
        // Write universal slots: [1 byte tag][MAX_PARTY_SIZE data]
        for (int i = 0; i < total; i++) {
            uint8_t tag = (i < (int)slotOrigins_.size())
                ? encodeGameType(slotOrigins_[i]) : 0;
            file.write(reinterpret_cast<const char*>(&tag), 1);
            file.write(reinterpret_cast<const char*>(slots_[i].data.data()),
                       PokeCrypto::MAX_PARTY_SIZE);
        }
    } else {
        // Write game-specific slots
        for (int i = 0; i < total; i++) {
            file.write(reinterpret_cast<const char*>(slots_[i].data.data()), slotSize_);
        }
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
    if (isUniversal_) {
        // In universal mode, each slot has its own GameType from the origin tag
        if (idx < (int)slotOrigins_.size())
            pkm.gameType_ = slotOrigins_[idx];
        else
            pkm.gameType_ = GameType::ZA;
    } else {
        pkm.gameType_ = gameType_;
    }
    return pkm;
}

void Bank::setSlot(int box, int slot, const Pokemon& pkm) {
    int idx = slotIndex(box, slot);
    if (idx < 0 || idx >= totalSlots())
        return;
    slots_[idx] = pkm;
    if (isUniversal_ && idx < (int)slotOrigins_.size()) {
        slotOrigins_[idx] = pkm.gameType_;
    }
}

void Bank::clearSlot(int box, int slot) {
    int idx = slotIndex(box, slot);
    if (idx < 0 || idx >= totalSlots())
        return;
    slots_[idx] = Pokemon{};
    if (isUniversal_ && idx < (int)slotOrigins_.size()) {
        slotOrigins_[idx] = GameType::ZA;
    }
}

GameType Bank::getSlotOrigin(int box, int slot) const {
    if (!isUniversal_) return gameType_;
    int idx = slotIndex(box, slot);
    if (idx < 0 || idx >= (int)slotOrigins_.size())
        return GameType::ZA;
    return slotOrigins_[idx];
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
