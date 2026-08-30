// world.h - the battlefield: terrain, structures, cover props, and the surface
// queries everything else is built on.
//
// The important idea here is that "what am I standing on" is a single query that
// answers with a point and a normal, whether the answer is a hillside, a wall or
// the underside of a walkway. That is what lets one gait solver handle walking,
// climbing and hanging without special cases.
#pragma once

#include <vector>
#include "arena.h"
#include "buildings.h"
#include "math3d.h"
#include "mesh.h"
#include "obstacle.h"
#include "raster.h"
#include "terrain.h"

namespace sb {

struct SurfaceHit {
    bool hit = false;
    Vec3 point{0.0f, 0.0f, 0.0f};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
    ObstacleKind kind = ObstacleKind::Terrain;
    int obstacle = -1;             // index into obstacles(), -1 for terrain
};

struct PropInstance {
    int meshIndex = 0;
    Mat4 xform = Mat4::identity();
    Vec3 tint{1.0f, 1.0f, 1.0f};
    Vec3 pos{0.0f, 0.0f, 0.0f};
    float radius = 1.0f;
    float height = 1.0f;
    bool major = false;            // drawn even at long range
};

class World {
public:
    void generate(const ArenaDef& arena, uint32_t seed);

    // Test scaffolding: a flat empty world plus hand-placed boxes. The climb
    // and unit harnesses need geometry whose exact shape they control -
    // measuring a top-out against whatever the arena generator scattered
    // nearby is how this suite once tested the wrong building entirely.
    void generateTestRange(float extent);
    void addTestBox(const Vec3& center, const Vec3& half, bool climbable = true);

    // Destructible support. Props register their blocking volume here at
    // mission start (finalizeObstacles() once after the batch), and switch it
    // off when destroyed - indices stay stable all mission.
    int addDynamicObstacle(const Obstacle& o);
    void finalizeObstacles();
    void disableObstacle(int idx);

    const Terrain& terrain() const { return terrain_; }
    float waterLevel() const { return arena_.waterLevel; }
    // The arena's structural grain: the angle its line layouts and causeway
    // chains run along. Missions on water maps lay their lane along this so
    // the objective path actually follows the bridges.
    float mainAxisAngle() const { return mainAxisAngle_; }
    bool hasWater() const { return arena_.waterLevel > -9000.0f; }
    const ArenaDef& arena() const { return arena_; }
    const std::vector<Obstacle>& obstacles() const { return obstacles_; }
    float extent() const { return arena_.extent; }

    void submit(Rasterizer& raster, const Vec3& viewPos, float viewDistance) const;

    // ------------------------------------------------------------- queries --

    // Nearest solid along a ray, testing terrain and every obstacle.
    SurfaceHit raycast(const Vec3& origin, const Vec3& dir, float maxDist) const;

    // True when nothing solid sits between the two points.
    bool lineOfSight(const Vec3& a, const Vec3& b) const;

    // Looks for something to stand on near `searchPoint`, casting along -up from
    // `castUp` above it down to `castDown` below. This is the heart of the
    // climbing system: on the ground it finds terrain, on a wall it finds the
    // wall face, and the returned normal is what orients the whole machine.
    SurfaceHit findFoothold(const Vec3& searchPoint, const Vec3& up,
                            float castUp, float castDown) const;

    // Pushes a sphere out of solid geometry. `outNormal` receives the surface
    // normal of the deepest contact, which the mech uses to decide whether the
    // thing it just walked into is climbable.
    Vec3 resolveCollision(const Vec3& desired, float radius,
                          Vec3* outNormal = nullptr, ObstacleKind* outKind = nullptr) const;

    // True if the point is inside any solid.
    bool insideSolid(const Vec3& p, float margin = 0.0f) const;

    Vec3 clampToWorld(const Vec3& p, float margin) const;

    // A clear, roughly level spot to put a mech, at least `minDist` from `from`.
    Vec3 findSpawnPoint(Rng& rng, const Vec3& from, float minDist, float maxDist) const;

    // True if this spot is inside a structure's footprint. Ruins are hollow -
    // perimeter walls around an empty middle - so a point in the centre of a
    // building is not "inside solid" at all, and a machine placed there is
    // simply walled in.
    bool insideStructure(const Vec3& p, float margin = 0.0f) const;

private:
    void buildMeshLibrary(uint32_t seed);
    void placeStructures(uint32_t seed);
    void scatterProps(uint32_t seed);
    void addStructure(int meshIndex, const Vec3& pos, float yaw, float scale,
                      const Vec3& tint, bool major);
    bool spotIsClear(const Vec3& pos, float radius) const;

    ArenaDef arena_;
    Terrain terrain_;

    std::vector<Mesh> meshes_;
    std::vector<PropInstance> props_;
    std::vector<Obstacle> obstacles_;
    ObstacleGrid grid_;

    // Index ranges into meshes_ for each structure family.
    int ruinFirst_ = 0, ruinCount_ = 0;
    int treeFirst_ = 0, treeCount_ = 0;
    int rockFirst_ = 0, rockCount_ = 0;
    int containerFirst_ = 0, containerCount_ = 0;
    int barrierFirst_ = 0, barrierCount_ = 0;
    int pipeFirst_ = 0, pipeCount_ = 0;
    int bunkerFirst_ = 0, bunkerCount_ = 0;
    int rubbleFirst_ = 0, rubbleCount_ = 0;
    int mastFirst_ = 0, mastCount_ = 0;
    int causewayFirst_ = 0, causewayCount_ = 0;
    int galleryFirst_ = 0, galleryCount_ = 0;
    Mesh waterMesh_;               // one big quad at the waterline
    float mainAxisAngle_ = 0.0f;

    std::vector<StructurePiece> pieces_;   // parallel to meshes_, for obstacles
    mutable std::vector<int> scratch_;
};

} // namespace sb
