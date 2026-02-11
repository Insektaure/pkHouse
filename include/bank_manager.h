#pragma once
#include "bank.h"
#include <string>
#include <vector>

struct BankInfo {
    std::string name;       // filename without .bin
    std::string fullPath;
    int occupiedSlots;      // 0..960
};

class BankManager {
public:
    bool init(const std::string& basePath);   // creates banks/ dir, migrates legacy bank.bin
    void refresh();                            // rescan banks/ dir, rebuild list with slot counts
    const std::vector<BankInfo>& list() const;

    bool createBank(const std::string& name);
    bool deleteBank(const std::string& name);
    bool renameBank(const std::string& oldName, const std::string& newName);
    std::string loadBank(const std::string& name, Bank& bank);   // returns full path
    std::string pathFor(const std::string& name) const;
    static int countOccupied(const std::string& filePath);

private:
    std::string banksDir_;   // basePath + "banks/"
    std::string basePath_;
    std::vector<BankInfo> bankList_;
    static std::string sanitizeName(const std::string& raw);
    bool migrateLegacy();    // move basePath/bank.bin -> banks/Default.bin if exists
};
