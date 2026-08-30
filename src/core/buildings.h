// buildings.h - procedural ruined structures and cover props.
//
// Each builder returns one merged mesh (drawn as a single instanced item) plus
// the handful of boxes that actually collide. The visual mesh is far more
// detailed than the collision: a ruin is fifty-odd boxes to look at and about
// eight to walk into, and the walls are deliberately full height so the climbing
// system has clean vertical faces to grip.
#pragma once

#include <vector>
#include "math3d.h"
#include "mesh.h"
#include "obstacle.h"

namespace sb {

struct StructurePiece {
    Mesh mesh;
    std::vector<Obstacle> obstacles;   // local space, base at y = 0
    float height = 0.0f;
    float radius = 0.0f;               // footprint radius for scattering
};

struct RuinSpec {
    float halfWidth = 8.0f;
    float halfDepth = 6.0f;
    int floors = 4;
    float floorHeight = 4.2f;
    int style = 0;              // 0 tower block, 1 warehouse, 2 office, 3 stump
    float damage = 0.5f;        // 0 intact, 1 mostly rubble
    uint32_t seed = 1;
    Vec3 tint{0.5f, 0.5f, 0.5f};
};

StructurePiece buildRuin(const RuinSpec& spec);

// Cover props. All small, all solid, all climbable except where noted.
StructurePiece buildContainer(float len, float wid, float hgt, uint32_t seed, const Vec3& tint);
StructurePiece buildBarrier(float len, uint32_t seed, const Vec3& tint);
StructurePiece buildPipeRack(float len, uint32_t seed, const Vec3& tint);
StructurePiece buildBunker(float radius, uint32_t seed, const Vec3& tint);
StructurePiece buildRubblePile(float radius, uint32_t seed, const Vec3& tint);
StructurePiece buildAntennaMast(float height, uint32_t seed, const Vec3& tint);

// A causeway segment: a raised road deck on pillars with low side rails, for
// island maps where the deck is the only dry path. Local origin at deck centre,
// deck top at y = height; the deck runs along +-X.
StructurePiece buildCauseway(float halfLen, float halfWid, float height,
                             uint32_t seed, const Vec3& tint);

// A covered gallery: two heavy walls and a roof slab forming an enclosed run
// along +-X. Fighting through one is a tunnel fight: no jumping, no climbing
// out, cover at the mouths.
StructurePiece buildCausewayRamp(float halfWid, float height, const Vec3& tint);
StructurePiece buildGallery(float halfLen, float halfWid, float clearance,
                            uint32_t seed, const Vec3& tint);

} // namespace sb
