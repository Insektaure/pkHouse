#pragma once
#include <cstdint>
#include <cstddef>

// PokeCrypto - Pokemon entity encryption/decryption for Gen9.
// Ported from PKHeX.Core/PKM/Util/PokeCrypto.cs
namespace PokeCrypto {

    constexpr int SIZE_9STORED = 0x148; // 328 bytes (box format)
    constexpr int SIZE_9PARTY  = 0x158; // 344 bytes (party format)
    constexpr int BLOCK_SIZE   = 0x50;  // 80 bytes per block
    constexpr int BLOCK_COUNT  = 4;

    // LCG-based XOR cipher on uint16 pairs.
    void cryptArray(uint8_t* data, size_t len, uint32_t seed);

    // Decrypt Gen9 Pokemon data in-place, output to outBuf.
    // ekm: encrypted data (SIZE_9STORED or SIZE_9PARTY bytes)
    // outBuf: decrypted output (same size as input)
    void decryptArray9(const uint8_t* ekm, size_t len, uint8_t* outBuf);

    // Encrypt Gen9 Pokemon data.
    void encryptArray9(const uint8_t* pk, size_t len, uint8_t* outBuf);

} // namespace PokeCrypto
