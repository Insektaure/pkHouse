#include "bank_manager.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

bool BankManager::init(const std::string& basePath, GameType game) {
    basePath_ = basePath;
    game_ = game;

    // Create banks/ parent directory
    std::string banksParent = basePath + "banks/";
    mkdir(banksParent.c_str(), 0755);

    // Game-specific subdirectory (paired games share a folder)
    banksDir_ = banksParent + bankFolderNameOf(game) + "/";
    mkdir(banksDir_.c_str(), 0755);

    // Universal banks subdirectory (shared across all games)
    std::string universalDir = banksParent + "Universal/";
    mkdir(universalDir.c_str(), 0755);

    // Migrate legacy bank.bin only for ZA
    if (game == GameType::ZA)
        migrateLegacy();

    refresh();
    return true;
}

bool BankManager::migrateLegacy() {
    std::string legacyPath = basePath_ + "bank.bin";
    std::string newPath = banksDir_ + "Default.bin";

    // Check if legacy file exists
    struct stat st;
    if (stat(legacyPath.c_str(), &st) != 0)
        return false;  // no legacy file

    // Don't overwrite if Default.bin already exists
    if (stat(newPath.c_str(), &st) == 0)
        return false;

    return std::rename(legacyPath.c_str(), newPath.c_str()) == 0;
}

static BankMode detectBankMode(const std::string& filePath) {
    Bank temp;
    if (!temp.load(filePath))
        return BankMode::Game;
    return temp.mode();
}

void BankManager::refresh() {
    bankList_.clear();

    // Scan game-specific directory
    auto scanDir = [&](const std::string& dirPath) {
        DIR* dir = opendir(dirPath.c_str());
        if (!dir)
            return;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name.size() < 5)
                continue;
            if (name.substr(name.size() - 4) != ".bin")
                continue;

            std::string stem = name.substr(0, name.size() - 4);
            std::string fullPath = dirPath + name;

            BankInfo info;
            info.name = stem;
            info.fullPath = fullPath;
            info.occupiedSlots = countOccupied(fullPath);
            info.mode = detectBankMode(fullPath);
            bankList_.push_back(info);
        }
        closedir(dir);
    };

    scanDir(banksDir_);

    // Also scan Universal directory (shared across all games)
    std::string universalDir = basePath_ + "banks/Universal/";
    scanDir(universalDir);

    // Sort: universal banks first, then alphabetically (case-insensitive)
    std::sort(bankList_.begin(), bankList_.end(), [](const BankInfo& a, const BankInfo& b) {
        if (a.mode != b.mode)
            return a.mode == BankMode::Universal; // universal first
        std::string la = a.name, lb = b.name;
        std::transform(la.begin(), la.end(), la.begin(), ::tolower);
        std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
        return la < lb;
    });
}

const std::vector<BankInfo>& BankManager::list() const {
    return bankList_;
}

int BankManager::countOccupied(const std::string& filePath) {
    Bank temp;
    if (!temp.load(filePath))
        return 0;

    int count = 0;
    for (int box = 0; box < temp.boxCount(); box++) {
        for (int slot = 0; slot < temp.slotsPerBox(); slot++) {
            if (!temp.getSlot(box, slot).isEmpty())
                count++;
        }
    }
    return count;
}

int BankManager::countBanks(const std::string& basePath, GameType game) {
    int count = 0;
    auto countIn = [&](const std::string& dir) {
        DIR* d = opendir(dir.c_str());
        if (!d) return;
        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr) {
            std::string name = entry->d_name;
            if (name.size() >= 5 && name.substr(name.size() - 4) == ".bin")
                count++;
        }
        closedir(d);
    };

    countIn(basePath + "banks/" + bankFolderNameOf(game) + "/");
    countIn(basePath + "banks/Universal/");
    return count;
}

bool BankManager::createBank(const std::string& name, BankMode mode) {
    std::string safe = sanitizeName(name);
    if (safe.empty())
        return false;

    std::string dir = (mode == BankMode::Universal)
        ? basePath_ + "banks/Universal/"
        : banksDir_;
    std::string path = dir + safe + ".bin";

    // Don't overwrite existing bank
    struct stat st;
    if (stat(path.c_str(), &st) == 0)
        return false;

    // Create an empty bank file
    Bank empty;
    if (mode == BankMode::Universal)
        empty.setUniversal();
    else
        empty.setGameType(game_);
    if (!empty.save(path))
        return false;

    refresh();
    return true;
}

bool BankManager::deleteBank(const std::string& name) {
    std::string path = pathFor(name);
    if (path.empty())
        return false;

    if (std::remove(path.c_str()) != 0)
        return false;

    refresh();
    return true;
}

bool BankManager::renameBank(const std::string& oldName, const std::string& newName) {
    std::string safe = sanitizeName(newName);
    if (safe.empty())
        return false;

    std::string oldPath = pathFor(oldName);
    if (oldPath.empty())
        return false;

    // Keep the bank in its original directory
    std::string oldDir = oldPath.substr(0, oldPath.find_last_of('/') + 1);
    std::string newPath = oldDir + safe + ".bin";

    // Don't overwrite existing bank
    struct stat st;
    if (stat(newPath.c_str(), &st) == 0)
        return false;

    if (std::rename(oldPath.c_str(), newPath.c_str()) != 0)
        return false;

    refresh();
    return true;
}

std::string BankManager::loadBank(const std::string& name, Bank& bank) {
    std::string path = pathFor(name);
    if (path.empty())
        return "";

    bank.load(path);
    return path;
}

std::string BankManager::pathFor(const std::string& name) const {
    for (const auto& info : bankList_) {
        if (info.name == name)
            return info.fullPath;
    }
    return "";
}

std::string BankManager::sanitizeName(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());

    for (char c : raw) {
        // Strip invalid filesystem characters
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            continue;
        result += c;
    }

    // Trim leading/trailing whitespace
    size_t start = result.find_first_not_of(' ');
    if (start == std::string::npos)
        return "";
    size_t end = result.find_last_not_of(' ');
    result = result.substr(start, end - start + 1);

    // Limit to 32 characters
    if (result.size() > 32)
        result = result.substr(0, 32);

    return result;
}
