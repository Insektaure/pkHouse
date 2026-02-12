#pragma once
#include <cstdint>
#include <cstddef>

// PokeCrypto - Pokemon entity encryption/decryption for Gen8/Gen9.
// Ported from PKHeX.Core/PKM/Util/PokeCrypto.cs
namespace PokeCrypto {

    // Gen8/Gen9 shared (PK8, PB8, PA9)
    constexpr int SIZE_9STORED = 0x148; // 328 bytes (box format)
    constexpr int SIZE_9PARTY  = 0x158; // 344 bytes (party format)
    constexpr int BLOCK_SIZE   = 0x50;  // 80 bytes per block
    constexpr int BLOCK_COUNT  = 4;

    // Gen8a (PA8 — Legends: Arceus)
    constexpr int SIZE_8ASTORED = 0x168; // 360 bytes (box format)
    constexpr int SIZE_8APARTY  = 0x178; // 376 bytes (party format)
    constexpr int SIZE_8ABLOCK  = 0x58;  // 88 bytes per block

    // Gen6/Gen7 (PB7 — Let's Go Pikachu/Eevee)
    constexpr int SIZE_6STORED = 0xE8;   // 232 bytes
    constexpr int SIZE_6PARTY  = 0x104;  // 260 bytes (LGPE boxes store party format)
    constexpr int SIZE_6BLOCK  = 56;     // 56 bytes per block

    // Largest party size across all formats (for Pokemon data array sizing)
    constexpr int MAX_PARTY_SIZE = SIZE_8APARTY; // 0x178

    // LCG-based XOR cipher on uint16 pairs.
    void cryptArray(uint8_t* data, size_t len, uint32_t seed);

    // Decrypt/encrypt Gen8/Gen9 Pokemon data (PK8, PB8, PA9).
    void decryptArray9(const uint8_t* ekm, size_t len, uint8_t* outBuf);
    void encryptArray9(const uint8_t* pk, size_t len, uint8_t* outBuf);

    // Decrypt/encrypt Gen8a Pokemon data (PA8 — Legends: Arceus).
    void decryptArray8A(const uint8_t* ekm, size_t len, uint8_t* outBuf);
    void encryptArray8A(const uint8_t* pk, size_t len, uint8_t* outBuf);

    // Decrypt/encrypt Gen6/Gen7 Pokemon data (PB7 — Let's Go Pikachu/Eevee).
    void decryptArray6(const uint8_t* ekm, size_t len, uint8_t* outBuf);
    void encryptArray6(const uint8_t* pk, size_t len, uint8_t* outBuf);

} // namespace PokeCrypto
