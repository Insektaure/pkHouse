#pragma once
#include "bank.h"
#include "save_file.h"  // for GameType
#include <string>
#include <vector>
#include <ctime>

struct BankInfo {
    std::string name;       // filename without .bin
    std::string fullPath;
    int occupiedSlots;      // 0..960
    GameType game = GameType::ZA;  // which game this bank belongs to
    bool valid = true;      // false = stray .bin that isn't a real bank file
    time_t modified = 0;    // file's last write, for "Edited 3 days ago"
};

class BankManager {
public:
    bool init(const std::string& basePath, GameType game);
    void refresh();
    const std::vector<BankInfo>& list() const;

    bool createBank(const std::string& name);
    // True if a bank file with this (sanitized) name already exists in the
    // current game folder. Used to give feedback when a create is rejected.
    bool bankExists(const std::string& name) const;
    bool deleteBank(const std::string& name);
    bool renameBank(const std::string& oldName, const std::string& newName);
    std::string loadBank(const std::string& name, Bank& bank);
    std::string pathFor(const std::string& name) const;
    static int countOccupied(const std::string& filePath);
    static int countBanks(const std::string& basePath, GameType game);
    // Names (without .bin) of the valid banks countBanks counts, sorted.
    static std::vector<std::string> bankNames(const std::string& basePath, GameType game);

    // One representative game per bank folder, in game card order. Paired
    // games (Sword/Shield...) share a folder, so this is one per family.
    static constexpr GameType FAMILY_GAMES[] = {
        GameType::GP, GameType::Sw, GameType::BD,
        GameType::LA, GameType::S, GameType::ZA, GameType::FR
    };

    // Scan all game folders and build a combined bank list
    bool initAll(const std::string& basePath);
    bool isAllMode() const { return allMode_; }

    // Visual row helpers for grouped display (headers + bank entries)
    // Returns the visual row index for a given bank index (accounting for group headers)
    int bankToVisualRow(int bankIdx) const;
    // Returns total visual rows (banks + group headers)
    int totalVisualRows() const;

private:
    // Subfolder under banks/ that deleted banks are moved into (soft delete).
    // The per-game folder structure is mirrored inside it, e.g.
    // banks/trash/za/Name.bin. No leading dot: some Switch file managers hide
    // dot-folders, which would make trashed banks unrecoverable for users.
    static constexpr const char* TRASH_DIR = "trash";

    bool allMode_ = false;
    std::string banksDir_;   // basePath + "banks/sv/" or "banks/za/"
    std::string basePath_;
    GameType game_ = GameType::ZA;
    std::vector<BankInfo> bankList_;
    static std::string sanitizeName(const std::string& raw);
    bool migrateLegacy();    // move basePath/bank.bin -> banks/za/Default.bin (ZA only)
};
