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

    // Universal bank management (cross-game, banks/Universal/)
    void refreshUniversal();
    const std::vector<BankInfo>& universalList() const;
    bool createUniversalBank(const std::string& name);
    bool deleteUniversalBank(const std::string& name);
    bool renameUniversalBank(const std::string& oldName, const std::string& newName);
    std::string loadUniversalBank(const std::string& name, Bank& bank);
    std::string universalPathFor(const std::string& name) const;
    static int countUniversalBanks(const std::string& basePath);

private:
    std::string banksDir_;   // basePath + "banks/sv/" or "banks/za/"
    std::string basePath_;
    GameType game_ = GameType::ZA;
    std::vector<BankInfo> bankList_;

    std::string universalDir_;   // basePath + "banks/Universal/"
    std::vector<BankInfo> universalBankList_;

    static std::string sanitizeName(const std::string& raw);
    bool migrateLegacy();    // move basePath/bank.bin -> banks/za/Default.bin (ZA only)
};
