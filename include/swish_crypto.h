#pragma once
#include "sc_block.h"
#include <vector>
#include <cstdint>

// SwishCrypto - save file encryption/decryption for Gen8+ Pokemon games.
// Ported from PKHeX.Core/Saves/Encryption/SwishCrypto/SwishCrypto.cs
namespace SwishCrypto {

    // XOR the data in-place with the repeating 127-byte static xorpad.
    void cryptStaticXorpadBytes(uint8_t* data, size_t len);

    // Decrypt a save file into SCBlocks.
    // Modifies fileData in-place (XOR step), then parses blocks.
    std::vector<SCBlock> decrypt(uint8_t* fileData, size_t fileSize);

    // Encrypt SCBlocks back into raw save file data.
    std::vector<uint8_t> encrypt(const std::vector<SCBlock>& blocks);

    // Find a block by key (linear search).
    SCBlock* findBlock(std::vector<SCBlock>& blocks, uint32_t key);
    const SCBlock* findBlock(const std::vector<SCBlock>& blocks, uint32_t key);

} // namespace SwishCrypto
