#include "swish_crypto.h"
#include <cstring>
#include <algorithm>

// Static XOR pad (127 usable bytes + 1 trailing zero for alignment to 128)
// From SwishCrypto.cs lines 39-49
static const uint8_t STATIC_XORPAD[128] = {
    0xA0, 0x92, 0xD1, 0x06, 0x07, 0xDB, 0x32, 0xA1, 0xAE, 0x01, 0xF5, 0xC5, 0x1E, 0x84, 0x4F, 0xE3,
    0x53, 0xCA, 0x37, 0xF4, 0xA7, 0xB0, 0x4D, 0xA0, 0x18, 0xB7, 0xC2, 0x97, 0xDA, 0x5F, 0x53, 0x2B,
    0x75, 0xFA, 0x48, 0x16, 0xF8, 0xD4, 0x8A, 0x6F, 0x61, 0x05, 0xF4, 0xE2, 0xFD, 0x04, 0xB5, 0xA3,
    0x0F, 0xFC, 0x44, 0x92, 0xCB, 0x32, 0xE6, 0x1B, 0xB9, 0xB1, 0x2E, 0x01, 0xB0, 0x56, 0x53, 0x36,
    0xD2, 0xD1, 0x50, 0x3D, 0xDE, 0x5B, 0x2E, 0x0E, 0x52, 0xFD, 0xDF, 0x2F, 0x7B, 0xCA, 0x63, 0x50,
    0xA4, 0x67, 0x5D, 0x23, 0x17, 0xC0, 0x52, 0xE1, 0xA6, 0x30, 0x7C, 0x2B, 0xB6, 0x70, 0x36, 0x5B,
    0x2A, 0x27, 0x69, 0x33, 0xF5, 0x63, 0x7B, 0x36, 0x3F, 0x26, 0x9B, 0xA3, 0xED, 0x7A, 0x53, 0x00,
    0xA4, 0x48, 0xB3, 0x50, 0x9E, 0x14, 0xA0, 0x52, 0xDE, 0x7E, 0x10, 0x2B, 0x1B, 0x77, 0x6E, 0x00,
};

static constexpr size_t XORPAD_SIZE = 0x7F; // 127 usable bytes

// SHA256 intro/outro salts for hash computation
// From SwishCrypto.cs lines 23-37
static const uint8_t INTRO_HASH[64] = {
    0x9E, 0xC9, 0x9C, 0xD7, 0x0E, 0xD3, 0x3C, 0x44, 0xFB, 0x93, 0x03, 0xDC, 0xEB, 0x39, 0xB4, 0x2A,
    0x19, 0x47, 0xE9, 0x63, 0x4B, 0xA2, 0x33, 0x44, 0x16, 0xBF, 0x82, 0xA2, 0xBA, 0x63, 0x55, 0xB6,
    0x3D, 0x9D, 0xF2, 0x4B, 0x5F, 0x7B, 0x6A, 0xB2, 0x62, 0x1D, 0xC2, 0x1B, 0x68, 0xE5, 0xC8, 0xB5,
    0x3A, 0x05, 0x90, 0x00, 0xE8, 0xA8, 0x10, 0x3D, 0xE2, 0xEC, 0xF0, 0x0C, 0xB2, 0xED, 0x4F, 0x6D,
};

static const uint8_t OUTRO_HASH[64] = {
    0xD6, 0xC0, 0x1C, 0x59, 0x8B, 0xC8, 0xB8, 0xCB, 0x46, 0xE1, 0x53, 0xFC, 0x82, 0x8C, 0x75, 0x75,
    0x13, 0xE0, 0x45, 0xDF, 0x32, 0x69, 0x3C, 0x75, 0xF0, 0x59, 0xF8, 0xD9, 0xA2, 0x5F, 0xB2, 0x17,
    0xE0, 0x80, 0x52, 0xDB, 0xEA, 0x89, 0x73, 0x99, 0x75, 0x79, 0xAF, 0xCB, 0x2E, 0x80, 0x07, 0xE6,
    0xF1, 0x26, 0xE0, 0x03, 0x0A, 0xE6, 0x6F, 0xF6, 0x41, 0xBF, 0x7E, 0x59, 0xC2, 0xAE, 0x55, 0xFD,
};

static constexpr size_t SIZE_HASH = 32; // SHA256

// Minimal SHA256 implementation for portability
namespace {

struct SHA256_CTX {
    uint32_t state[8];
    uint64_t bitcount;
    uint8_t  buffer[64];
};

static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

static inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32_t sig0(uint32_t x) { return rotr(x,2) ^ rotr(x,13) ^ rotr(x,22); }
static inline uint32_t sig1(uint32_t x) { return rotr(x,6) ^ rotr(x,11) ^ rotr(x,25); }
static inline uint32_t gam0(uint32_t x) { return rotr(x,7) ^ rotr(x,18) ^ (x >> 3); }
static inline uint32_t gam1(uint32_t x) { return rotr(x,17) ^ rotr(x,19) ^ (x >> 10); }

static void sha256_transform(SHA256_CTX* ctx, const uint8_t* data) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t(data[i*4]) << 24) | (uint32_t(data[i*4+1]) << 16) |
               (uint32_t(data[i*4+2]) << 8) | uint32_t(data[i*4+3]);
    for (int i = 16; i < 64; i++)
        w[i] = gam1(w[i-2]) + w[i-7] + gam0(w[i-15]) + w[i-16];

    uint32_t a=ctx->state[0], b=ctx->state[1], c=ctx->state[2], d=ctx->state[3];
    uint32_t e=ctx->state[4], f=ctx->state[5], g=ctx->state[6], h=ctx->state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + sig1(e) + ch(e,f,g) + K256[i] + w[i];
        uint32_t t2 = sig0(a) + maj(a,b,c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }

    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

static void sha256_init(SHA256_CTX* ctx) {
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
    ctx->bitcount = 0;
    std::memset(ctx->buffer, 0, 64);
}

static void sha256_update(SHA256_CTX* ctx, const uint8_t* data, size_t len) {
    size_t bufIdx = static_cast<size_t>((ctx->bitcount >> 3) & 63);
    ctx->bitcount += static_cast<uint64_t>(len) << 3;
    size_t i = 0;
    if (bufIdx > 0) {
        size_t space = 64 - bufIdx;
        size_t toCopy = std::min(len, space);
        std::memcpy(ctx->buffer + bufIdx, data, toCopy);
        i = toCopy;
        if (bufIdx + toCopy == 64) {
            sha256_transform(ctx, ctx->buffer);
        }
    }
    for (; i + 64 <= len; i += 64)
        sha256_transform(ctx, data + i);
    if (i < len)
        std::memcpy(ctx->buffer, data + i, len - i);
}

static void sha256_final(SHA256_CTX* ctx, uint8_t hash[32]) {
    size_t bufIdx = static_cast<size_t>((ctx->bitcount >> 3) & 63);
    ctx->buffer[bufIdx++] = 0x80;
    if (bufIdx > 56) {
        std::memset(ctx->buffer + bufIdx, 0, 64 - bufIdx);
        sha256_transform(ctx, ctx->buffer);
        bufIdx = 0;
    }
    std::memset(ctx->buffer + bufIdx, 0, 56 - bufIdx);
    uint64_t bits = ctx->bitcount;
    for (int i = 7; i >= 0; i--)
        ctx->buffer[56 + (7 - i)] = static_cast<uint8_t>(bits >> (i * 8));
    sha256_transform(ctx, ctx->buffer);
    for (int i = 0; i < 8; i++) {
        hash[i*4+0] = static_cast<uint8_t>(ctx->state[i] >> 24);
        hash[i*4+1] = static_cast<uint8_t>(ctx->state[i] >> 16);
        hash[i*4+2] = static_cast<uint8_t>(ctx->state[i] >> 8);
        hash[i*4+3] = static_cast<uint8_t>(ctx->state[i]);
    }
}

} // anonymous namespace

static void computeHash(const uint8_t* payload, size_t payloadLen, uint8_t out[32]) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, INTRO_HASH, 64);
    sha256_update(&ctx, payload, payloadLen);
    sha256_update(&ctx, OUTRO_HASH, 64);
    sha256_final(&ctx, out);
}

void SwishCrypto::cryptStaticXorpadBytes(uint8_t* data, size_t len) {
    size_t i = 0;
    // Process in chunks of XORPAD_SIZE (127 bytes)
    while (i + XORPAD_SIZE <= len) {
        for (size_t j = 0; j < XORPAD_SIZE; j++)
            data[i + j] ^= STATIC_XORPAD[j];
        i += XORPAD_SIZE;
    }
    // Remainder
    for (size_t j = 0; i + j < len; j++)
        data[i + j] ^= STATIC_XORPAD[j];
}

std::vector<SCBlock> SwishCrypto::decrypt(uint8_t* fileData, size_t fileSize) {
    // Ignore last 32 bytes (SHA256 hash)
    size_t payloadLen = fileSize - SIZE_HASH;

    // XOR decrypt the payload
    cryptStaticXorpadBytes(fileData, payloadLen);

    // Parse blocks sequentially
    std::vector<SCBlock> blocks;
    blocks.reserve(payloadLen / 500); // rough estimate
    size_t offset = 0;
    while (offset < payloadLen) {
        blocks.push_back(SCBlock::readFromOffset(fileData, payloadLen, offset));
    }

    return blocks;
}

std::vector<uint8_t> SwishCrypto::encrypt(const std::vector<SCBlock>& blocks) {
    // Calculate total size
    size_t totalSize = 0;
    for (auto& b : blocks)
        totalSize += b.encodedSize();
    totalSize += SIZE_HASH; // append hash

    std::vector<uint8_t> result(totalSize, 0);

    // Write all blocks
    size_t pos = 0;
    for (auto& b : blocks)
        pos += b.writeBlock(result.data() + pos);

    size_t payloadLen = pos;

    // XOR encrypt the payload
    cryptStaticXorpadBytes(result.data(), payloadLen);

    // Compute and store SHA256 hash
    computeHash(result.data(), payloadLen, result.data() + payloadLen);

    result.resize(payloadLen + SIZE_HASH);
    return result;
}

SCBlock* SwishCrypto::findBlock(std::vector<SCBlock>& blocks, uint32_t key) {
    for (auto& b : blocks) {
        if (b.key == key)
            return &b;
    }
    return nullptr;
}

const SCBlock* SwishCrypto::findBlock(const std::vector<SCBlock>& blocks, uint32_t key) {
    for (auto& b : blocks) {
        if (b.key == key)
            return &b;
    }
    return nullptr;
}
