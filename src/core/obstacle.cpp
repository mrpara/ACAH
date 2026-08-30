#include "obstacle.h"

namespace sb {

void closestOnObstacle(const Obstacle& o, const Vec3& p, Vec3& outPoint, Vec3& outNormal) {
    const Vec3 l = o.toLocal(p);
    const Vec3& h = o.half;

    const bool inside = std::fabs(l.x) <= h.x && std::fabs(l.y) <= h.y && std::fabs(l.z) <= h.z;
    Vec3 localPoint, localNormal;

    if (inside) {
        // Push out through whichever face is nearest.
        const float dx = h.x - std::fabs(l.x);
        const float dy = h.y - std::fabs(l.y);
        const float dz = h.z - std::fabs(l.z);
        localPoint = l;
        if (dx <= dy && dx <= dz) {
            localPoint.x = (l.x < 0.0f) ? -h.x : h.x;
            localNormal = Vec3((l.x < 0.0f) ? -1.0f : 1.0f, 0.0f, 0.0f);
        } else if (dy <= dz) {
            localPoint.y = (l.y < 0.0f) ? -h.y : h.y;
            localNormal = Vec3(0.0f, (l.y < 0.0f) ? -1.0f : 1.0f, 0.0f);
        } else {
            localPoint.z = (l.z < 0.0f) ? -h.z : h.z;
            localNormal = Vec3(0.0f, 0.0f, (l.z < 0.0f) ? -1.0f : 1.0f);
        }
    } else {
        localPoint = Vec3(clampf(l.x, -h.x, h.x), clampf(l.y, -h.y, h.y), clampf(l.z, -h.z, h.z));
        Vec3 d = l - localPoint;
        if (lengthSq(d) < 1e-10f) {
            localNormal = Vec3(0.0f, 1.0f, 0.0f);
        } else {
            // On an edge or corner the normal is the direction out, which reads
            // better for foot placement than picking one arbitrary face.
            localNormal = normalize(d);
        }
    }

    outPoint = o.toWorld(localPoint);
    outNormal = normalize(o.toWorldDir(localNormal));
}

bool raycastObstacle(const Obstacle& o, const Vec3& origin, const Vec3& dir,
                     float maxDist, float& tHit, Vec3& outNormal) {
    const Vec3 lo = o.toLocal(origin);
    const float c = std::cos(-o.yaw), s = std::sin(-o.yaw);
    const Vec3 ld(dir.x * c + dir.z * s, dir.y, -dir.x * s + dir.z * c);

    float tMin = 0.0f, tMax = maxDist;
    int axisMin = -1;
    float signMin = 1.0f;

    const float lop[3] = {lo.x, lo.y, lo.z};
    const float ldp[3] = {ld.x, ld.y, ld.z};
    const float hp[3] = {o.half.x, o.half.y, o.half.z};

    for (int a = 0; a < 3; ++a) {
        if (std::fabs(ldp[a]) < 1e-7f) {
            if (lop[a] < -hp[a] || lop[a] > hp[a]) return false;
            continue;
        }
        const float inv = 1.0f / ldp[a];
        float t1 = (-hp[a] - lop[a]) * inv;
        float t2 = ( hp[a] - lop[a]) * inv;
        float sgn = -1.0f;
        if (t1 > t2) { const float tmp = t1; t1 = t2; t2 = tmp; sgn = 1.0f; }
        if (t1 > tMin) { tMin = t1; axisMin = a; signMin = sgn; }
        if (t2 < tMax) tMax = t2;
        if (tMin > tMax) return false;
    }
    if (axisMin < 0) return false;     // origin already inside
    if (tMin < 0.0f || tMin > maxDist) return false;

    tHit = tMin;
    Vec3 localN(0.0f, 0.0f, 0.0f);
    if (axisMin == 0) localN.x = signMin;
    else if (axisMin == 1) localN.y = signMin;
    else localN.z = signMin;
    outNormal = normalize(o.toWorldDir(localN));
    return true;
}

bool resolveSphereObstacle(const Obstacle& o, const Vec3& p, float radius, Vec3& push, Vec3& normal) {
    const Vec3 l = o.toLocal(p);
    const Vec3& h = o.half;
    const Vec3 clamped(clampf(l.x, -h.x, h.x), clampf(l.y, -h.y, h.y), clampf(l.z, -h.z, h.z));
    Vec3 d = l - clamped;
    const float distSq = lengthSq(d);

    if (distSq > radius * radius) return false;

    Vec3 localPush, localNormal;
    if (distSq > 1e-10f) {
        const float dist = std::sqrt(distSq);
        localNormal = d * (1.0f / dist);
        localPush = localNormal * (radius - dist);
    } else {
        // Centre is inside the box: leave through the nearest face.
        const float dx = h.x - std::fabs(l.x);
        const float dy = h.y - std::fabs(l.y);
        const float dz = h.z - std::fabs(l.z);
        if (dx <= dy && dx <= dz) {
            localNormal = Vec3((l.x < 0.0f) ? -1.0f : 1.0f, 0.0f, 0.0f);
            localPush = localNormal * (dx + radius);
        } else if (dy <= dz) {
            localNormal = Vec3(0.0f, (l.y < 0.0f) ? -1.0f : 1.0f, 0.0f);
            localPush = localNormal * (dy + radius);
        } else {
            localNormal = Vec3(0.0f, 0.0f, (l.z < 0.0f) ? -1.0f : 1.0f);
            localPush = localNormal * (dz + radius);
        }
    }
    push = o.toWorldDir(localPush);
    normal = normalize(o.toWorldDir(localNormal));
    return true;
}

// ------------------------------------------------------------------- the grid

void ObstacleGrid::build(const std::vector<Obstacle>& obstacles, float extent, float cellSize) {
    extent_ = extent;
    cell_ = std::max(cellSize, 1.0f);
    dim_ = std::max(1, static_cast<int>(std::ceil((extent_ * 2.0f) / cell_)));
    cells_.assign(static_cast<size_t>(dim_) * dim_, {});

    for (size_t i = 0; i < obstacles.size(); ++i) {
        const Obstacle& o = obstacles[i];
        // Register in every cell the box's footprint touches, so a long wall is
        // found from anywhere along its length.
        const float r = std::sqrt(o.half.x * o.half.x + o.half.z * o.half.z);
        const int x0 = static_cast<int>((o.center.x - r + extent_) / cell_);
        const int x1 = static_cast<int>((o.center.x + r + extent_) / cell_);
        const int z0 = static_cast<int>((o.center.z - r + extent_) / cell_);
        const int z1 = static_cast<int>((o.center.z + r + extent_) / cell_);
        for (int z = std::max(0, z0); z <= std::min(dim_ - 1, z1); ++z)
            for (int x = std::max(0, x0); x <= std::min(dim_ - 1, x1); ++x)
                cells_[static_cast<size_t>(z) * dim_ + x].push_back(static_cast<int>(i));
    }
}

int ObstacleGrid::index(float x, float z) const {
    const int gx = static_cast<int>((x + extent_) / cell_);
    const int gz = static_cast<int>((z + extent_) / cell_);
    if (gx < 0 || gz < 0 || gx >= dim_ || gz >= dim_) return -1;
    return gz * dim_ + gx;
}

void ObstacleGrid::query(const Vec3& center, float radius, std::vector<int>& out) const {
    out.clear();
    if (cells_.empty()) return;
    const int reach = std::max(1, static_cast<int>(std::ceil(radius / cell_)));
    const int gx = static_cast<int>((center.x + extent_) / cell_);
    const int gz = static_cast<int>((center.z + extent_) / cell_);
    for (int dz = -reach; dz <= reach; ++dz) {
        for (int dx = -reach; dx <= reach; ++dx) {
            const int cx = gx + dx, cz = gz + dz;
            if (cx < 0 || cz < 0 || cx >= dim_ || cz >= dim_) continue;
            for (int i : cells_[static_cast<size_t>(cz) * dim_ + cx]) {
                // Cells overlap, so the same box can appear more than once.
                bool seen = false;
                for (int j : out) if (j == i) { seen = true; break; }
                if (!seen) out.push_back(i);
            }
        }
    }
    (void)index(0.0f, 0.0f);
}

} // namespace sb
