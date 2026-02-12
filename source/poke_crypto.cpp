#include "poke_crypto.h"
#include <cstring>

// Block shuffle position table (24 patterns * 4 + 8*4 duplicates)
// From PokeCrypto.cs lines 66-101
static const uint8_t BLOCK_POSITION[128] = {
    0, 1, 2, 3,  0, 1, 3, 2,  0, 2, 1, 3,  0, 3, 1, 2,
    0, 2, 3, 1,  0, 3, 2, 1,  1, 0, 2, 3,  1, 0, 3, 2,
    2, 0, 1, 3,  3, 0, 1, 2,  2, 0, 3, 1,  3, 0, 2, 1,
    1, 2, 0, 3,  1, 3, 0, 2,  2, 1, 0, 3,  3, 1, 0, 2,
    2, 3, 0, 1,  3, 2, 0, 1,  1, 2, 3, 0,  1, 3, 2, 0,
    2, 1, 3, 0,  3, 1, 2, 0,  2, 3, 1, 0,  3, 2, 1, 0,
    // duplicates of 0-7
    0, 1, 2, 3,  0, 1, 3, 2,  0, 2, 1, 3,  0, 3, 1, 2,
    0, 2, 3, 1,  0, 3, 2, 1,  1, 0, 2, 3,  1, 0, 3, 2,
};

// Inverse shuffle table
// From PokeCrypto.cs lines 106-110
static const uint8_t BLOCK_POSITION_INVERT[32] = {
    0, 1, 2, 4, 3, 5, 6, 7, 12, 18, 13, 19, 8, 10, 14, 20, 16, 22, 9, 11, 15, 21, 17, 23,
    0, 1, 2, 4, 3, 5, 6, 7,
};

static inline uint16_t readU16LE(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, 2);
    return v;
}

static inline void writeU16LE(uint8_t* p, uint16_t v) {
    std::memcpy(p, &v, 2);
}

static inline uint32_t readU32LE(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

void PokeCrypto::cryptArray(uint8_t* data, size_t len, uint32_t seed) {
    size_t pairs = len / 2;
    for (size_t i = 0; i < pairs; i++) {
        seed = 0x41C64E6D * seed + 0x00006073;
        uint16_t xorVal = static_cast<uint16_t>(seed >> 16);
        uint16_t val = readU16LE(data + i * 2);
        val ^= xorVal;
        writeU16LE(data + i * 2, val);
    }
}

static void shuffleArray(const uint8_t* data, size_t len, uint32_t sv, int blockSize, uint8_t* out) {
    constexpr int start = 8;
    int blockCount = PokeCrypto::BLOCK_COUNT;
    int index = static_cast<int>(sv) * blockCount;
    int end = start + blockSize * blockCount;

    // Copy header (first 8 bytes)
    std::memcpy(out, data, start);
    // Copy tail (after all blocks)
    if (static_cast<int>(len) > end)
        std::memcpy(out + end, data + end, len - end);
    // Shuffle blocks
    for (int block = 0; block < blockCount; block++) {
        int destOfs = start + blockSize * block;
        int srcBlock = BLOCK_POSITION[index + block];
        int srcOfs = start + blockSize * srcBlock;
        std::memcpy(out + destOfs, data + srcOfs, blockSize);
    }
}

void PokeCrypto::decryptArray9(const uint8_t* ekm, size_t len, uint8_t* outBuf) {
    // Make a working copy to decrypt in-place
    uint8_t tmp[SIZE_9PARTY];
    size_t sz = len > SIZE_9PARTY ? SIZE_9PARTY : len;
    std::memcpy(tmp, ekm, sz);

    uint32_t pv = readU32LE(tmp);
    uint32_t sv = (pv >> 13) & 31;

    // Decrypt blocks (offset 8 to 8 + 4*80 = 328)
    constexpr int start = 8;
    int end = BLOCK_COUNT * BLOCK_SIZE + start;
    cryptArray(tmp + start, end - start, pv);

    // Decrypt party stats (if present)
    if (static_cast<int>(sz) > end)
        cryptArray(tmp + end, sz - end, pv);

    // Unshuffle blocks
    shuffleArray(tmp, sz, sv, BLOCK_SIZE, outBuf);
}

void PokeCrypto::encryptArray9(const uint8_t* pk, size_t len, uint8_t* outBuf) {
    uint32_t pv = readU32LE(pk);
    uint32_t sv = (pv >> 13) & 31;

    // Inverse shuffle
    shuffleArray(pk, len, BLOCK_POSITION_INVERT[sv], BLOCK_SIZE, outBuf);

    // Encrypt blocks
    constexpr int start = 8;
    int end = BLOCK_COUNT * BLOCK_SIZE + start;
    cryptArray(outBuf + start, end - start, pv);

    // Encrypt party stats
    if (static_cast<int>(len) > end)
        cryptArray(outBuf + end, len - end, pv);
}

// --- PA8 (Legends: Arceus) — same algorithm, larger block size (0x58) ---

void PokeCrypto::decryptArray8A(const uint8_t* ekm, size_t len, uint8_t* outBuf) {
    uint8_t tmp[SIZE_8APARTY];
    size_t sz = len > SIZE_8APARTY ? SIZE_8APARTY : len;
    std::memcpy(tmp, ekm, sz);
    if (sz < SIZE_8APARTY)
        std::memset(tmp + sz, 0, SIZE_8APARTY - sz);

    uint32_t pv = readU32LE(tmp);
    uint32_t sv = (pv >> 13) & 31;

    // Decrypt blocks (offset 8 to 8 + 4*88 = 360)
    constexpr int start = 8;
    int end = BLOCK_COUNT * SIZE_8ABLOCK + start;
    cryptArray(tmp + start, end - start, pv);

    // Decrypt party stats (if present)
    if (static_cast<int>(sz) > end)
        cryptArray(tmp + end, sz - end, pv);

    // Unshuffle blocks
    shuffleArray(tmp, sz, sv, SIZE_8ABLOCK, outBuf);
}

void PokeCrypto::encryptArray8A(const uint8_t* pk, size_t len, uint8_t* outBuf) {
    uint32_t pv = readU32LE(pk);
    uint32_t sv = (pv >> 13) & 31;

    // Inverse shuffle
    shuffleArray(pk, len, BLOCK_POSITION_INVERT[sv], SIZE_8ABLOCK, outBuf);

    // Encrypt blocks
    constexpr int start = 8;
    int end = BLOCK_COUNT * SIZE_8ABLOCK + start;
    cryptArray(outBuf + start, end - start, pv);

    // Encrypt party stats
    if (static_cast<int>(len) > end)
        cryptArray(outBuf + end, len - end, pv);
}

// --- PB7 (Let's Go Pikachu/Eevee) — same algorithm, smaller block size (56) ---

void PokeCrypto::decryptArray6(const uint8_t* ekm, size_t len, uint8_t* outBuf) {
    uint8_t tmp[SIZE_6PARTY];
    size_t sz = len > SIZE_6PARTY ? SIZE_6PARTY : len;
    std::memcpy(tmp, ekm, sz);
    if (sz < SIZE_6PARTY)
        std::memset(tmp + sz, 0, SIZE_6PARTY - sz);

    uint32_t pv = readU32LE(tmp);
    uint32_t sv = (pv >> 13) & 31;

    // Decrypt blocks (offset 8 to 8 + 4*56 = 232)
    constexpr int start = 8;
    int end = BLOCK_COUNT * SIZE_6BLOCK + start;
    cryptArray(tmp + start, end - start, pv);

    // Decrypt party stats (if present)
    if (static_cast<int>(sz) > end)
        cryptArray(tmp + end, sz - end, pv);

    // Unshuffle blocks
    shuffleArray(tmp, sz, sv, SIZE_6BLOCK, outBuf);
}

void PokeCrypto::encryptArray6(const uint8_t* pk, size_t len, uint8_t* outBuf) {
    uint32_t pv = readU32LE(pk);
    uint32_t sv = (pv >> 13) & 31;

    // Inverse shuffle
    shuffleArray(pk, len, BLOCK_POSITION_INVERT[sv], SIZE_6BLOCK, outBuf);

    // Encrypt blocks
    constexpr int start = 8;
    int end = BLOCK_COUNT * SIZE_6BLOCK + start;
    cryptArray(outBuf + start, end - start, pv);

    // Encrypt party stats
    if (static_cast<int>(len) > end)
        cryptArray(outBuf + end, len - end, pv);
}
