#include "i18n.h"

#define JSON_NOEXCEPTION
#include "json.hpp"

#include <fstream>
#include <sstream>
#include <cstdio>
#include <unordered_map>
#include <dirent.h>

namespace i18n {

static std::unordered_map<std::string, std::string> strings_;
static std::unordered_map<std::string, std::string> fallback_;
static std::string lang_ = "en";
static std::string empty_;

static bool loadJson(const std::string& path, std::unordered_map<std::string, std::string>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    std::string buf(sz, '\0');
    std::fread(&buf[0], 1, sz, f);
    std::fclose(f);

    auto j = nlohmann::json::parse(buf, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;

    out.clear();
    for (auto& [key, val] : j.items()) {
        if (val.is_string())
            out[key] = val.get<std::string>();
    }
    return true;
}

void init(const std::string& lang) {
    lang_ = lang;

    // Always load English as fallback
    loadJson("romfs:/data/strings/en.json", fallback_);

    if (lang == "en") {
        strings_ = fallback_;
    } else {
        std::string path = "romfs:/data/strings/" + lang + ".json";
        if (!loadJson(path, strings_)) {
            // Fallback to English if translation file not found
            strings_ = fallback_;
            lang_ = "en";
        }
    }
}

const std::string& get(const char* key) {
    auto it = strings_.find(key);
    if (it != strings_.end()) return it->second;

    auto fb = fallback_.find(key);
    if (fb != fallback_.end()) return fb->second;

    // Return the key itself as last resort
    static std::unordered_map<std::string, std::string> keyCache;
    auto& cached = keyCache[key];
    if (cached.empty()) cached = key;
    return cached;
}

std::string fmt(const char* key, const std::vector<std::string>& args) {
    const std::string& tmpl = get(key);
    std::string result = tmpl;

    for (size_t i = 0; i < args.size(); i++) {
        std::string placeholder = "{" + std::to_string(i) + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.size(), args[i]);
            pos += args[i].size();
        }
    }
    return result;
}

const std::string& currentLang() {
    return lang_;
}

std::vector<std::string> availableLangs() {
    std::vector<std::string> langs;
    DIR* dir = opendir("romfs:/data/strings");
    if (!dir) {
        langs.push_back("en");
        return langs;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() > 5 && name.substr(name.size() - 5) == ".json") {
            std::string code = name.substr(0, name.size() - 5);
            // Only include languages listed in KNOWN_LANGS (font support filter)
            bool known = false;
            for (int i = 0; i < KNOWN_LANG_COUNT; i++) {
                if (code == KNOWN_LANGS[i].code) { known = true; break; }
            }
            if (known) langs.push_back(code);
        }
    }
    closedir(dir);

    // Sort: en first, then alphabetical
    std::sort(langs.begin(), langs.end(), [](const std::string& a, const std::string& b) {
        if (a == "en") return true;
        if (b == "en") return false;
        return a < b;
    });

    if (langs.empty()) langs.push_back("en");
    return langs;
}

void clearCache() {
    // No-op for now; text render cache is managed by UI
}

} // namespace i18n
