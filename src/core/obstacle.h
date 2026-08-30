// obstacle.h - the solid geometry a mech can collide with, shoot at, take cover
// behind and climb on.
//
// Everything solid in the world is an oriented box. That is a deliberate
// simplification: a box has six flat faces, so "what surface is under this foot"
// and "can I grip this wall" both reduce to cheap closed-form maths, and a ruined
// building assembled from a dozen boxes still gives the climbing system real
// walls, ledges and rooftops to work with.
#pragma once

#include <vector>
#include "math3d.h"

namespace sb {

enum class ObstacleKind : uint8_t {
    Terrain = 0,     // never stored; returned by surface queries
    Building,
    Wall,
    Container,
    Rock,
    Barrier,
    Debris,
    Pillar
};

struct Obstacle {
    Vec3 center{0.0f, 0.0f, 0.0f};
    Vec3 half{1.0f, 1.0f, 1.0f};
    float yaw = 0.0f;            // rotation about +Y
    bool climbable = true;
    bool blocksSight = true;
    // Destructible props register obstacles too; when the prop dies, its
    // obstacle is switched off rather than erased, so every index into the
    // list stays valid for the rest of the mission.
    bool active = true;
    ObstacleKind kind = ObstacleKind::Building;

    // World-space bounding sphere, for broad-phase rejection.
    float boundRadius() const { return length(half); }

    Vec3 toLocal(const Vec3& p) const {
        const float c = std::cos(-yaw), s = std::sin(-yaw);
        const Vec3 d = p - center;
        return Vec3(d.x * c + d.z * s, d.y, -d.x * s + d.z * c);
    }
    Vec3 toWorldDir(const Vec3& d) const {
        const float c = std::cos(yaw), s = std::sin(yaw);
        return Vec3(d.x * c + d.z * s, d.y, -d.x * s + d.z * c);
    }
    Vec3 toWorld(const Vec3& p) const { return center + toWorldDir(p); }

    bool contains(const Vec3& p, float margin = 0.0f) const {
        const Vec3 l = toLocal(p);
        return std::fabs(l.x) <= half.x + margin &&
               std::fabs(l.y) <= half.y + margin &&
               std::fabs(l.z) <= half.z + margin;
    }
};

// The closest point on a box's surface to `p`, with the outward normal of the
// face that point lies on. Works whether `p` is inside or outside.
void closestOnObstacle(const Obstacle& o, const Vec3& p, Vec3& outPoint, Vec3& outNormal);

// Slab-method ray/box intersection. `tHit` is the distance along a normalised
// `dir`. Returns false when the ray misses or only hits behind the origin.
bool raycastObstacle(const Obstacle& o, const Vec3& origin, const Vec3& dir,
                     float maxDist, float& tHit, Vec3& outNormal);

// Pushes a sphere of `radius` out of the box along the shallowest axis.
// Returns true and fills `push` when they overlap.
bool resolveSphereObstacle(const Obstacle& o, const Vec3& p, float radius, Vec3& push, Vec3& normal);

// A uniform grid over the obstacle list, so foot placement and collision do not
// have to walk every building in the level.
class ObstacleGrid {
public:
    void build(const std::vector<Obstacle>& obstacles, float extent, float cellSize);

    // Appends the indices of every obstacle whose cell overlaps the query.
    void query(const Vec3& center, float radius, std::vector<int>& out) const;

private:
    int index(float x, float z) const;
    std::vector<std::vector<int>> cells_;
    int dim_ = 1;
    float cell_ = 8.0f;
    float extent_ = 100.0f;
};

} // namespace sb
