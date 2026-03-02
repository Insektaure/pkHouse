#include "wondercard_manager.h"
#include "species_converter.h"
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

// Check if game family matches wondercard format
static bool isCompatible(GameType game, WCFormat fmt) {
    switch (fmt) {
        case WCFormat::WC8: return isSwSh(game);
        case WCFormat::WB8: return isBDSP(game);
        case WCFormat::WA8: return game == GameType::LA;
        case WCFormat::WC9: return isSV(game);
        case WCFormat::WA9: return game == GameType::ZA;
        default: return false;
    }
}

void WondercardManager::scan(const std::string& basePath, GameType game) {
    cards_.clear();

    // Build path: basePath/wondercards/<GameFolderName>/
    std::string wcParent = basePath + "wondercards/";
    mkdir(wcParent.c_str(), 0755);

    wcDir_ = wcParent + bankFolderNameOf(game) + "/";
    mkdir(wcDir_.c_str(), 0755);

    DIR* dir = opendir(wcDir_.c_str());
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() < 5) continue;

        // Check extension
        std::string ext = name.substr(name.size() - 4);
        for (auto& c : ext) c = static_cast<char>(std::tolower(c));
        if (ext != ".wc8" && ext != ".wb8" && ext != ".wa8" &&
            ext != ".wc9" && ext != ".wa9")
            continue;

        std::string fullPath = wcDir_ + name;

        // Load and validate
        Wondercard wc;
        if (!wc.loadFromFile(fullPath)) continue;
        if (!wc.isPokemonGift()) continue;
        if (!isCompatible(game, wc.format)) continue;

        WondercardInfo info;
        info.filename = name;
        info.fullPath = fullPath;
        info.format = wc.format;
        info.speciesNational = wc.speciesNational();
        info.level = wc.level();
        info.nature = wc.nature();
        info.shinyType = wc.shinyType();
        info.formatLabel = wc.formatName();
        cards_.push_back(info);
    }
    closedir(dir);

    // Sort alphabetically by filename (case-insensitive)
    std::sort(cards_.begin(), cards_.end(), [](const WondercardInfo& a, const WondercardInfo& b) {
        std::string la = a.filename, lb = b.filename;
        std::transform(la.begin(), la.end(), la.begin(), ::tolower);
        std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
        return la < lb;
    });
}

bool WondercardManager::loadCard(const std::string& path, Wondercard& out) {
    return out.loadFromFile(path);
}
