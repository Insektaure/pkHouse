#pragma once
#include <cstdint>
#include <cstring>

inline uint16_t readU16LE(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, 2);
    return v;
}

inline void writeU16LE(uint8_t* p, uint16_t v) {
    std::memcpy(p, &v, 2);
}

inline uint32_t readU32LE(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

inline void writeU32LE(uint8_t* p, uint32_t v) {
    std::memcpy(p, &v, 4);
}
