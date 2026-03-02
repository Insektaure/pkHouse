#pragma once
#include "wondercard.h"
#include <string>
#include <vector>

struct WondercardInfo {
    std::string filename;       // e.g., "Mew.wc9"
    std::string fullPath;
    WCFormat format;
    uint16_t speciesNational;
    uint8_t level;
    uint8_t nature;             // 0xFF = random
    WCShinyType shinyType;
    std::string formatLabel;    // "WC9", "WA9", etc.
};

class WondercardManager {
public:
    // Scan the game-specific wondercard subfolder
    void scan(const std::string& basePath, GameType game);

    const std::vector<WondercardInfo>& list() const { return cards_; }

    // Load a specific wondercard by full path
    static bool loadCard(const std::string& path, Wondercard& out);

private:
    std::string wcDir_;
    std::vector<WondercardInfo> cards_;
};
