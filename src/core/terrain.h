// terrain.h - the analytic height field and the tiled mesh built from it.
//
// The height function is the source of truth: the gait solver, collision and
// projectiles all sample height()/normal() directly, and the drawable mesh is
// just a tessellation of the same function. Tiles exist so the renderer can
// frustum-cull the ground instead of pushing 50k triangles every frame.
//
// A TerrainProfile turns the same generator into visually distinct landscapes -
// rolling hills, terraced mesas, ridged highlands, carved canyons or a flat
// urban pad - which is what gives each arena its own character.
#pragma once

#include <vector>
#include "math3d.h"
#include "mesh.h"

namespace sb {

struct TerrainProfile {
    // Broad landform.
    float hillAmp = 24.0f;
    float hillFreq = 0.0062f;
    float plainsBias = 1.4f;       // higher pushes more of the map toward flat
    float plainsFloor = 0.18f;     // how flat the flattest areas get

    // Mid and fine relief. These are single-octave on purpose: max slope is
    // very close to 3 * amplitude * frequency, so they can be tuned directly.
    float midAmp = 5.0f,   midFreq = 0.021f;
    float fineAmp = 1.40f, fineFreq = 0.055f;
    float microAmp = 0.30f, microFreq = 0.130f;

    // Character modifiers.
    float ridged = 0.0f;           // 0 smooth, 1 sharp alpine ridges
    float terrace = 0.0f;          // >0 quantises height into steps (mesas)
    float terraceBlend = 0.55f;
    float canyonDepth = 0.0f;      // carves steep channels
    float canyonFreq = 0.0042f;
    float canyonWidth = 0.16f;
    // When set (> -900), the main canyon is a single winding channel running
    // along this angle - the arena's structural axis - instead of an
    // undirected band field. The mission lane follows the same axis, so the
    // route runs down the canyon floor rather than across its walls. The old
    // band field still carves shallower side branches for variety.
    float canyonAxis = -999.0f;

    // A flattened disc at the origin, for arenas built on a prepared pad.
    float flatRadius = 0.0f;
    float flatFalloff = 45.0f;
    float flatHeight = 0.0f;

    // Surface look.
    Vec3 groundColor{0.095f, 0.165f, 0.105f};
    Vec3 rockColor{0.205f, 0.205f, 0.198f};
    float slopeRockiness = 3.4f;
    float mottle = 0.12f;
};

class Terrain {
public:
    void generate(const TerrainProfile& profile, uint32_t seed, float extent,
                  float tileSize, float vertexStep);

    // Ground height and surface normal at a world XZ position.
    float height(float x, float z) const;
    float height(const Vec3& p) const { return height(p.x, p.z); }
    Vec3 normal(float x, float z) const;

    // Height with the finest octave omitted. Foot placement uses this so limbs
    // do not twitch over noise far below the resolution of the character grid.
    float heightSmooth(float x, float z) const;

    float slope(float x, float z) const { return 1.0f - clampf(normal(x, z).y, 0.0f, 1.0f); }

    bool raycast(const Vec3& origin, const Vec3& dir, float maxDist, Vec3& hit) const;

    float extent() const { return extent_; }
    Vec3 clampToWorld(const Vec3& p, float margin) const;

    const std::vector<Mesh>& tiles() const { return tiles_; }
    const std::vector<Vec3>& tileCenters() const { return tileCenters_; }
    float tileRadius() const { return tileRadius_; }
    const TerrainProfile& profile() const { return profile_; }

private:
    float baseHeight(float x, float z, bool includeMicro) const;

    TerrainProfile profile_;
    uint32_t seed_ = 1;
    float extent_ = 200.0f;
    float tileSize_ = 20.0f;
    float vertexStep_ = 2.0f;
    float tileRadius_ = 0.0f;
    std::vector<Mesh> tiles_;
    std::vector<Vec3> tileCenters_;
};

} // namespace sb
