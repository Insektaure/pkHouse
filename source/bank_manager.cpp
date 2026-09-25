#include "bank_manager.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

bool BankManager::init(const std::string& basePath, GameType game, const Progress& progress) {
    basePath_ = basePath;
    game_ = game;
    allMode_ = false;

    // Create banks/ parent directory
    std::string banksParent = basePath + "banks/";
    mkdir(banksParent.c_str(), 0755);

    // Game-specific subdirectory (paired games share a folder)
    banksDir_ = banksParent + bankFolderNameOf(game) + "/";

    mkdir(banksDir_.c_str(), 0755);

    // Migrate legacy bank.bin only for ZA
    if (game == GameType::ZA)
        migrateLegacy();

    refresh(progress);
    return true;
}

// Everything the list shows about one bank file.
static BankInfo describeBank(const std::string& fullPath, const std::string& stem, GameType game) {
    BankInfo info;
    info.name = stem;
    info.fullPath = fullPath;
    info.valid = Bank::isValidFile(fullPath);
    info.occupiedSlots = info.valid ? BankManager::countOccupied(fullPath) : 0;
    info.game = game;
    struct stat st;
    if (stat(fullPath.c_str(), &st) == 0) info.modified = st.st_mtime;
    return info;
}

bool BankManager::initAll(const std::string& basePath, const Progress& progress) {
    basePath_ = basePath;
    allMode_ = true;
    bankList_.clear();

    std::string banksParent = basePath + "banks/";

    // List every family's files first, so progress can say "n of total".
    struct Found { std::string path, stem; GameType game; };
    std::vector<Found> found;
    for (GameType g : FAMILY_GAMES) {
        std::string dir = banksParent + bankFolderNameOf(g) + "/";
        DIR* d = opendir(dir.c_str());
        if (!d) continue;

        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr) {
            std::string name = entry->d_name;
            if (name.size() < 5 || name.substr(name.size() - 4) != ".bin")
                continue;
            found.push_back({dir + name, name.substr(0, name.size() - 4), g});
        }
        closedir(d);
    }

    const int total = static_cast<int>(found.size());
    for (int i = 0; i < total; i++) {
        bankList_.push_back(describeBank(found[i].path, found[i].stem, found[i].game));
        if (progress) progress(i + 1, total);
    }

    // Sort by game card order then alphabetically by name
    auto gameOrder = [](GameType g) -> int {
        // Paired games share the same order index
        if (isLGPE(g)) return 0;
        if (isSwSh(g)) return 1;
        if (isBDSP(g)) return 2;
        if (g == GameType::LA) return 3;
        if (isSV(g))   return 4;
        if (g == GameType::ZA) return 5;
        if (isFRLG(g)) return 6;
        return 7;
    };
    std::sort(bankList_.begin(), bankList_.end(), [&](const BankInfo& a, const BankInfo& b) {
        int oa = gameOrder(a.game), ob = gameOrder(b.game);
        if (oa != ob) return oa < ob;
        std::string la = a.name, lb = b.name;
        std::transform(la.begin(), la.end(), la.begin(), ::tolower);
        std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
        return la < lb;
    });

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

void BankManager::refresh(const Progress& progress) {
    bankList_.clear();

    DIR* dir = opendir(banksDir_.c_str());
    if (!dir)
        return;

    // Names first, then the files, so progress can say "n of total".
    std::vector<std::string> names;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        // Only .bin files
        if (name.size() < 5 || name.substr(name.size() - 4) != ".bin")
            continue;
        names.push_back(name);
    }
    closedir(dir);

    const int total = static_cast<int>(names.size());
    for (int i = 0; i < total; i++) {
        const std::string& name = names[i];
        bankList_.push_back(describeBank(banksDir_ + name, name.substr(0, name.size() - 4), game_));
        if (progress) progress(i + 1, total);
    }

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
    // Read straight from the file: same count, without building a Bank.
    return Bank::countOccupiedInFile(filePath);
}

int BankManager::countBanks(const std::string& basePath, GameType game) {
    std::string dir = basePath + "banks/" + bankFolderNameOf(game) + "/";
    DIR* d = opendir(dir.c_str());
    if (!d) return 0;

    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() >= 5 && name.substr(name.size() - 4) == ".bin" &&
            Bank::isValidFile(dir + name))
            count++;
    }
    closedir(d);
    return count;
}

std::vector<std::string> BankManager::bankNames(const std::string& basePath, GameType game) {
    std::vector<std::string> names;
    std::string dir = basePath + "banks/" + bankFolderNameOf(game) + "/";
    DIR* d = opendir(dir.c_str());
    if (!d) return names;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() >= 5 && name.substr(name.size() - 4) == ".bin" &&
            Bank::isValidFile(dir + name))
            names.push_back(name.substr(0, name.size() - 4));
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    return names;
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
    // Find the bank entry so we know both its path and which game it belongs to
    const BankInfo* info = nullptr;
    for (const auto& b : bankList_) {
        if (b.name == name) { info = &b; break; }
    }
    if (!info)
        return false;

    // Soft delete: move the file into a trash area instead of removing it,
    // preserving the per-game folder layout so it stays recoverable:
    //   banks/<game>/Name.bin  ->  banks/trash/<game>/Name.bin
    std::string trashParent = basePath_ + "banks/" + TRASH_DIR + "/";
    mkdir(trashParent.c_str(), 0755);
    std::string trashDir = trashParent + bankFolderNameOf(info->game) + "/";
    mkdir(trashDir.c_str(), 0755);

    // Avoid clobbering a previously deleted bank of the same name
    std::string destPath = trashDir + name + ".bin";
    struct stat st;
    for (int i = 2; stat(destPath.c_str(), &st) == 0; i++)
        destPath = trashDir + name + " (" + std::to_string(i) + ").bin";

    if (std::rename(info->fullPath.c_str(), destPath.c_str()) != 0)
        return false;

    refresh();
    return true;
}

bool BankManager::bankExists(const std::string& name) const {
    std::string safe = sanitizeName(name);
    if (safe.empty())
        return false;

    std::string path = banksDir_ + safe + ".bin";
    struct stat st;
    return stat(path.c_str(), &st) == 0;
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

int BankManager::bankToVisualRow(int bankIdx) const {
    if (!allMode_ || bankList_.empty()) return bankIdx;
    int headers = 0;
    for (int i = 0; i <= bankIdx && i < (int)bankList_.size(); i++) {
        if (i == 0 || bankList_[i].game != bankList_[i - 1].game)
            headers++;
    }
    return bankIdx + headers;
}

int BankManager::totalVisualRows() const {
    if (!allMode_ || bankList_.empty()) return (int)bankList_.size();
    int headers = 0;
    for (int i = 0; i < (int)bankList_.size(); i++) {
        if (i == 0 || bankList_[i].game != bankList_[i - 1].game)
            headers++;
    }
    return (int)bankList_.size() + headers;
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
