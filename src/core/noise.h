// noise.h - deterministic value noise and fBm. Used for the terrain height
// field and for scattering props, so the same seed always gives the same world.
#pragma once

#include <cstdint>
#include "math3d.h"

namespace sb {

inline float hashToUnit(int32_t x, int32_t y, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u
               + static_cast<uint32_t>(y) * 668265263u
               + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);   // [0,1)
}

inline float valueNoise(float x, float y, uint32_t seed) {
    const float fx = std::floor(x), fy = std::floor(y);
    const int32_t ix = static_cast<int32_t>(fx), iy = static_cast<int32_t>(fy);
    const float tx = x - fx, ty = y - fy;
    // Smoothstep interpolation keeps the field C1 continuous, which matters
    // because the terrain normal is a finite difference of it.
    const float ux = tx * tx * (3.0f - 2.0f * tx);
    const float uy = ty * ty * (3.0f - 2.0f * ty);

    const float a = hashToUnit(ix,     iy,     seed);
    const float b = hashToUnit(ix + 1, iy,     seed);
    const float c = hashToUnit(ix,     iy + 1, seed);
    const float d = hashToUnit(ix + 1, iy + 1, seed);
    return lerpf(lerpf(a, b, ux), lerpf(c, d, ux), uy) * 2.0f - 1.0f;   // [-1,1]
}

inline float fbm(float x, float y, int octaves, uint32_t seed,
                 float lacunarity = 2.02f, float gain = 0.5f) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f, fx = x, fy = y;
    for (int i = 0; i < octaves; ++i) {
        sum += valueNoise(fx, fy, seed + static_cast<uint32_t>(i) * 131u) * amp;
        norm += amp;
        amp *= gain;
        fx *= lacunarity;
        fy *= lacunarity;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

} // namespace sb
