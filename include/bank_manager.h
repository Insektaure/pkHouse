#pragma once
#include "bank.h"
#include "save_file.h"  // for GameType
#include <string>
#include <vector>

struct BankInfo {
    std::string name;       // filename without .bin
    std::string fullPath;
    int occupiedSlots;      // 0..960
};

class BankManager {
public:
    // Initialize for game-specific banks
    bool init(const std::string& basePath, GameType game);
    // Initialize for universal banks (game-agnostic)
    bool initUniversal(const std::string& basePath);

    void refresh();
    const std::vector<BankInfo>& list() const;

    bool createBank(const std::string& name);
    bool deleteBank(const std::string& name);
    bool renameBank(const std::string& oldName, const std::string& newName);
    std::string loadBank(const std::string& name, Bank& bank);
    std::string pathFor(const std::string& name) const;
    static int countOccupied(const std::string& filePath);
    static int countBanks(const std::string& basePath, GameType game);
    static int countUniversalBanks(const std::string& basePath);

    bool isUniversal() const { return isUniversal_; }

private:
    std::string banksDir_;   // basePath + "banks/sv/" or "banks/Universal/"
    std::string basePath_;
    GameType game_ = GameType::ZA;
    bool isUniversal_ = false;
    std::vector<BankInfo> bankList_;
    static std::string sanitizeName(const std::string& raw);
    bool migrateLegacy();    // move basePath/bank.bin -> banks/za/Default.bin (ZA only)
};
