#include "terrain.h"
#include "noise.h"

namespace sb {

void Terrain::generate(const TerrainProfile& profile, uint32_t seed, float extent,
                       float tileSize, float vertexStep) {
    profile_ = profile;
    seed_ = seed ? seed : 1u;
    extent_ = extent;
    tileSize_ = tileSize;
    vertexStep_ = vertexStep;
    tiles_.clear();
    tileCenters_.clear();
    tileRadius_ = 0.0f;

    const int tilesPerSide = std::max(1, static_cast<int>(std::ceil((extent_ * 2.0f) / tileSize_)));
    const int quads = std::max(1, static_cast<int>(std::round(tileSize_ / vertexStep_)));
    const float step = tileSize_ / quads;

    tiles_.reserve(static_cast<size_t>(tilesPerSide) * tilesPerSide);
    tileCenters_.reserve(tiles_.capacity());

    for (int tz = 0; tz < tilesPerSide; ++tz) {
        for (int tx = 0; tx < tilesPerSide; ++tx) {
            const float ox = -extent_ + tx * tileSize_;
            const float oz = -extent_ + tz * tileSize_;

            Mesh tile;
            tile.verts.reserve(static_cast<size_t>(quads + 1) * (quads + 1));
            for (int j = 0; j <= quads; ++j) {
                for (int i = 0; i <= quads; ++i) {
                    const float x = ox + i * step;
                    const float z = oz + j * step;
                    const float y = height(x, z);
                    const Vec3 n = normal(x, z);

                    // Grassy on flats, pale rock on steep faces, with gentle
                    // low-frequency mottling. Anything stronger competes with the
                    // shading for the few ramp steps the ASCII stage has.
                    const float steep = smoothstep01((1.0f - n.y) * profile_.slopeRockiness);
                    const float mottle = fbm(x * 0.055f, z * 0.055f, 2, seed_ + 991u) * 0.5f + 0.5f;
                    Vec3 col = lerp(profile_.groundColor, profile_.rockColor, steep);
                    col = col * (1.0f - profile_.mottle * 0.5f + profile_.mottle * mottle);

                    tile.verts.push_back({Vec3(x, y, z), n, col});
                }
            }
            const int stride = quads + 1;
            tile.idx.reserve(static_cast<size_t>(quads) * quads * 6);
            for (int j = 0; j < quads; ++j) {
                for (int i = 0; i < quads; ++i) {
                    const uint32_t a = static_cast<uint32_t>(j * stride + i);
                    const uint32_t b = a + 1;
                    const uint32_t c = a + stride;
                    const uint32_t d = c + 1;
                    // Counter-clockwise when seen from above (+Y).
                    tile.idx.insert(tile.idx.end(), {a, c, b, b, c, d});
                }
            }
            tile.computeBounds();
            tileCenters_.push_back(tile.boundsCenter);
            tileRadius_ = std::max(tileRadius_, tile.boundsRadius);
            tiles_.push_back(std::move(tile));
        }
    }
}

float Terrain::baseHeight(float x, float z, bool includeMicro) const {
    const TerrainProfile& p = profile_;

    // Broad landform. Ridged noise folds the field about zero, which turns
    // rounded hills into sharp alpine crests.
    float broad = fbm(x * p.hillFreq, z * p.hillFreq, 4, seed_);
    if (p.ridged > 0.0f) {
        const float r = 1.0f - std::fabs(broad) * 2.0f;
        broad = lerpf(broad, r, clampf(p.ridged, 0.0f, 1.0f));
    }
    float h = broad * p.hillAmp;

    // A second, slower field decides where the plains are; multiplying the
    // landform by it carves flat basins between the high ground.
    const float plains = smoothstep01(fbm(x * 0.0031f, z * 0.0031f, 2, seed_ + 5501u) * p.plainsBias + 0.5f);
    h *= lerpf(p.plainsFloor, 1.0f, plains);

    // Mid-scale relief matters more than it looks: broad hills alone leave the
    // ground almost flat across any one screenful, every ground normal comes out
    // the same, and the character ramp collapses to a single glyph. Single
    // octave, so max slope is very close to 3 * amplitude * frequency.
    h += valueNoise(x * p.midFreq, z * p.midFreq, seed_ + 77u) * p.midAmp;
    h += valueNoise(x * p.fineFreq, z * p.fineFreq, seed_ + 191u) * p.fineAmp;
    if (includeMicro)
        h += valueNoise(x * p.microFreq, z * p.microFreq, seed_ + 313u) * p.microAmp;

    // Terracing: quantise into steps and blend back, giving mesa country.
    if (p.terrace > 0.01f) {
        const float stepped = std::floor(h / p.terrace) * p.terrace + p.terrace * 0.5f;
        h = lerpf(h, stepped, clampf(p.terraceBlend, 0.0f, 1.0f));
    }

    // Canyons.
    if (p.canyonDepth > 0.01f) {
        if (p.canyonAxis > -900.0f) {
            // Directional: one winding master channel along the arena's
            // structural axis, so the mission lane runs down the canyon floor
            // instead of across its walls. The channel centreline meanders
            // with distance travelled; the old band field survives at reduced
            // depth as side branches feeding into it.
            const float ca = std::cos(p.canyonAxis), sa = std::sin(p.canyonAxis);
            const float along = x * ca + z * sa;
            const float cross = -x * sa + z * ca;
            const float meander =
                std::sin(along * 0.011f) * 26.0f +
                valueNoise(along * 0.004f, 13.7f, seed_ + 4177u) * 34.0f;
            const float halfW = std::max(p.canyonWidth, 0.02f) * 210.0f;
            const float d = std::fabs(cross - meander);
            const float inChannel = 1.0f - smoothstep01(d / halfW);
            h -= inChannel * p.canyonDepth;
            const float c = fbm(x * p.canyonFreq, z * p.canyonFreq, 3, seed_ + 8821u);
            const float band = std::fabs(c);
            const float side = 1.0f - smoothstep01(band / std::max(p.canyonWidth * 0.7f, 0.01f));
            h -= side * p.canyonDepth * 0.40f;
        } else {
            // Undirected: a ridged band field carves channels through everything.
            const float c = fbm(x * p.canyonFreq, z * p.canyonFreq, 3, seed_ + 8821u);
            const float band = std::fabs(c);
            const float inChannel = 1.0f - smoothstep01(band / std::max(p.canyonWidth, 0.01f));
            h -= inChannel * p.canyonDepth;
        }
    }

    // Prepared pad at the origin, for arenas built on flat ground.
    if (p.flatRadius > 0.5f) {
        const float d = std::sqrt(x * x + z * z);
        const float t = smoothstep01((d - p.flatRadius) / std::max(p.flatFalloff, 1.0f));
        h = lerpf(p.flatHeight, h, t);
    }
    return h;
}

float Terrain::height(float x, float z) const { return baseHeight(x, z, true); }
float Terrain::heightSmooth(float x, float z) const { return baseHeight(x, z, false); }

Vec3 Terrain::normal(float x, float z) const {
    // Central differences. The epsilon is large enough to skip the very finest
    // noise octave, which keeps foot placement from jittering.
    const float e = 0.45f;
    const float hL = height(x - e, z), hR = height(x + e, z);
    const float hD = height(x, z - e), hU = height(x, z + e);
    return normalize(Vec3(hL - hR, 2.0f * e, hD - hU));
}

bool Terrain::raycast(const Vec3& origin, const Vec3& dir, float maxDist, Vec3& hit) const {
    const Vec3 d = normalize(dir);
    float t = 0.0f;
    float prevGap = origin.y - height(origin.x, origin.z);
    // Coarse march, then a few bisection steps once we cross the surface.
    const float step = 0.6f;
    while (t < maxDist) {
        const float next = std::min(t + step, maxDist);
        const Vec3 p = origin + d * next;
        const float gap = p.y - height(p.x, p.z);
        if (gap <= 0.0f && prevGap > 0.0f) {
            float lo = t, hi = next;
            for (int i = 0; i < 12; ++i) {
                const float mid = (lo + hi) * 0.5f;
                const Vec3 pm = origin + d * mid;
                if (pm.y - height(pm.x, pm.z) > 0.0f) lo = mid; else hi = mid;
            }
            hit = origin + d * ((lo + hi) * 0.5f);
            return true;
        }
        prevGap = gap;
        t = next;
        if (next >= maxDist) break;
    }
    return false;
}

Vec3 Terrain::clampToWorld(const Vec3& p, float margin) const {
    const float lim = extent_ - margin;
    return Vec3(clampf(p.x, -lim, lim), p.y, clampf(p.z, -lim, lim));
}

} // namespace sb
