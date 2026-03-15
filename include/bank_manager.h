#pragma once
#include "bank.h"
#include "save_file.h"  // for GameType
#include <string>
#include <vector>

struct BankInfo {
    std::string name;       // filename without .bin
    std::string fullPath;
    int occupiedSlots;      // 0..960
    GameType game = GameType::ZA;  // which game this bank belongs to
};

class BankManager {
public:
    bool init(const std::string& basePath, GameType game);
    void refresh();
    const std::vector<BankInfo>& list() const;

    bool createBank(const std::string& name);
    bool deleteBank(const std::string& name);
    bool renameBank(const std::string& oldName, const std::string& newName);
    std::string loadBank(const std::string& name, Bank& bank);
    std::string pathFor(const std::string& name) const;
    static int countOccupied(const std::string& filePath);
    static int countBanks(const std::string& basePath, GameType game);

    // Scan all game folders and build a combined bank list
    bool initAll(const std::string& basePath);
    bool isAllMode() const { return allMode_; }

private:
    bool allMode_ = false;
    std::string banksDir_;   // basePath + "banks/sv/" or "banks/za/"
    std::string basePath_;
    GameType game_ = GameType::ZA;
    std::vector<BankInfo> bankList_;
    static std::string sanitizeName(const std::string& raw);
    bool migrateLegacy();    // move basePath/bank.bin -> banks/za/Default.bin (ZA only)
};
