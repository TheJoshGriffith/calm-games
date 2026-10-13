#pragma once

#include <cstdint>
#include <iterator>
#include <utility>

namespace calm {

// Small, fast, deterministic PRNG (xoshiro256**, seeded via SplitMix64).
// The same seed always yields the same sequence on every platform, so any
// generated game can be recreated from its seed alone.
class Rng {
public:
    explicit Rng(std::uint64_t seed) noexcept
    {
        std::uint64_t x = seed;
        for (auto& word : m_state)
            word = splitMix(x);
    }

    // SplitMix64 step; also handy for deriving independent sub-seeds.
    static std::uint64_t splitMix(std::uint64_t& x) noexcept
    {
        std::uint64_t z = (x += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    static std::uint64_t mix(std::uint64_t a, std::uint64_t b) noexcept
    {
        std::uint64_t x = a ^ (b * 0x9E3779B97F4A7C15ull);
        return splitMix(x);
    }

    std::uint64_t next() noexcept
    {
        const std::uint64_t result = rotl(m_state[1] * 5, 7) * 9;
        const std::uint64_t t = m_state[1] << 17;
        m_state[2] ^= m_state[0];
        m_state[3] ^= m_state[1];
        m_state[1] ^= m_state[2];
        m_state[0] ^= m_state[3];
        m_state[2] ^= t;
        m_state[3] = rotl(m_state[3], 45);
        return result;
    }

    // Uniform integer in [0, bound), without modulo bias.
    std::uint32_t below(std::uint32_t bound) noexcept
    {
        if (bound <= 1)
            return 0;
        const std::uint64_t limit = UINT64_MAX - (UINT64_MAX % bound);
        std::uint64_t value;
        do {
            value = next();
        } while (value >= limit);
        return static_cast<std::uint32_t>(value % bound);
    }

    template <typename RandomIt>
    void shuffle(RandomIt first, RandomIt last) noexcept
    {
        auto n = static_cast<std::uint32_t>(std::distance(first, last));
        for (std::uint32_t i = n; i > 1; --i)
            std::swap(first[i - 1], first[below(i)]);
    }

private:
    static constexpr std::uint64_t rotl(std::uint64_t x, int k) noexcept
    {
        return (x << k) | (x >> (64 - k));
    }

    std::uint64_t m_state[4] {};
};

} // namespace calm
