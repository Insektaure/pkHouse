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

    // Game-specific subdirectory
    if (game == GameType::SV)
        banksDir_ = banksParent + "sv/";
    else if (game == GameType::SwSh)
        banksDir_ = banksParent + "swsh/";
    else if (game == GameType::BDSP)
        banksDir_ = banksParent + "bdsp/";
    else if (game == GameType::LA)
        banksDir_ = banksParent + "la/";
    else
        banksDir_ = banksParent + "za/";

    mkdir(banksDir_.c_str(), 0755);

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

void BankManager::refresh() {
    bankList_.clear();

    DIR* dir = opendir(banksDir_.c_str());
    if (!dir)
        return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        // Only .bin files
        if (name.size() < 5)
            continue;
        if (name.substr(name.size() - 4) != ".bin")
            continue;

        std::string stem = name.substr(0, name.size() - 4);
        std::string fullPath = banksDir_ + name;

        BankInfo info;
        info.name = stem;
        info.fullPath = fullPath;
        info.occupiedSlots = countOccupied(fullPath);
        bankList_.push_back(info);
    }
    closedir(dir);

    // Sort alphabetically (case-insensitive)
    std::sort(bankList_.begin(), bankList_.end(), [](const BankInfo& a, const BankInfo& b) {
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
        for (int slot = 0; slot < Bank::SLOTS_PER_BOX; slot++) {
            if (!temp.getSlot(box, slot).isEmpty())
                count++;
        }
    }
    return count;
}

bool BankManager::createBank(const std::string& name) {
    std::string safe = sanitizeName(name);
    if (safe.empty())
        return false;

    std::string path = banksDir_ + safe + ".bin";

    // Don't overwrite existing bank
    struct stat st;
    if (stat(path.c_str(), &st) == 0)
        return false;

    // Create an empty bank file
    Bank empty;
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

    std::string newPath = banksDir_ + safe + ".bin";

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
