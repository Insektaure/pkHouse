#pragma once
#include <cstdint>
#include <cstddef>

// Minimal MD5 hash for BDSP save checksum.
namespace MD5 {
    // Compute MD5 hash of data, write 16-byte result to out.
    void hash(const uint8_t* data, size_t len, uint8_t out[16]);
}
