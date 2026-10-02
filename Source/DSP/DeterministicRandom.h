#pragma once

#include <cstdint>

// Deterministic xorshift64* generator (architecture 5.8): same seed yields the
// same sequence, so tests and baseline renders are reproducible. Callers that
// want true randomness pass a changing seed. RT-safe: no allocation, noexcept.
class DeterministicRandom
{
public:
    explicit DeterministicRandom (std::uint64_t seed = kDefaultSeed) noexcept
    {
        reseed (seed);
    }

    void reseed (std::uint64_t seed) noexcept
    {
        state = seed != 0 ? seed : kDefaultSeed;
    }

    std::uint64_t nextUInt() noexcept
    {
        auto x = state;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state = x;
        return x * 0x2545F4914F6CDD1Dull;
    }

    // [0, 1)
    float nextFloat() noexcept
    {
        return (float) ((nextUInt() >> 40) * (1.0 / 16777216.0));
    }

    // [-1, 1)
    float nextBipolar() noexcept
    {
        return nextFloat() * 2.0f - 1.0f;
    }

private:
    static constexpr std::uint64_t kDefaultSeed = 0x9E3779B97F4A7C15ull;

    std::uint64_t state = kDefaultSeed;
};
