#pragma once
#include <cstdint>

// Xoroshiro128Plus PRNG — ported from PKHeX.Core/Legality/RNG/Algorithms/Xoroshiro128Plus.cs
// Used by Lumiose RNG for Pokemon Legends: Z-A seed-correlated generation.
struct Xoroshiro128Plus {
    static constexpr uint64_t XOROSHIRO_CONST = 0x82A2B175229D6A5B;

    uint64_t s0, s1;

    explicit Xoroshiro128Plus(uint64_t seed, uint64_t s1val = XOROSHIRO_CONST)
        : s0(seed), s1(s1val) {}

    static inline uint64_t rotl(uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }

    uint64_t next() {
        uint64_t _s0 = s0;
        uint64_t _s1 = s1;
        uint64_t result = _s0 + _s1;

        _s1 ^= _s0;
        s0 = rotl(_s0, 24) ^ _s1 ^ (_s1 << 16);
        s1 = rotl(_s1, 37);

        return result;
    }

    // Bounded random: returns value in [0, max)
    // Uses rejection sampling with a power-of-2 bitmask.
    uint64_t nextInt(uint64_t max = 0xFFFFFFFF) {
        uint64_t mask = getBitmask(max);
        for (;;) {
            uint64_t result = next() & mask;
            if (result < max)
                return result;
        }
    }

private:
    static uint64_t getBitmask(uint64_t exclusiveMax) {
        --exclusiveMax;
        int lz = __builtin_clzll(exclusiveMax | 1); // avoid UB on 0
        return (1ULL << (64 - lz)) - 1;
    }
};
