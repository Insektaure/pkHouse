#pragma once
#include <string>
#include <cstddef>

inline std::string formatSize(size_t bytes) {
    if (bytes >= 1024 * 1024)
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    return std::to_string(bytes / 1024) + " KB";
}
