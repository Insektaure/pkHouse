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

private:
    std::string banksDir_;   // basePath + "banks/sv/" or "banks/za/"
    std::string basePath_;
    GameType game_ = GameType::ZA;
    std::vector<BankInfo> bankList_;
    static std::string sanitizeName(const std::string& raw);
    bool migrateLegacy();    // move basePath/bank.bin -> banks/za/Default.bin (ZA only)
};
