#pragma once
#include <cstdint>

namespace terrain {
// Seeded gradient noise with quintic interpolation, without external libraries.
// All hashing uses defined uint64_t wraparound, including negative coordinates.
class GradientNoise {
    uint64_t seed;
    uint64_t hash(int64_t x, int64_t y, int64_t z) const;
public:
    explicit GradientNoise(uint64_t p_seed) : seed(p_seed) {}
    double noise2(double x, double z) const;
    double noise3(double x, double y, double z) const;
    double fbm2(double x, double z, int octaves = 4) const;
    double fbm3(double x, double y, double z, int octaves = 3) const;
};
} // namespace terrain
