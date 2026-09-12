#include "world.h"
#include "noise.h"

namespace sb {

namespace {

Mesh buildConiferTree(float height, float canopyRadius, uint32_t seed) {
    Mesh m;
    const Vec3 barkColor(0.235f, 0.180f, 0.135f);
    const Vec3 leafDark(0.055f, 0.135f, 0.075f);
    const Vec3 leafLight(0.115f, 0.275f, 0.145f);

    const float trunkH = height * 0.42f;
    appendMesh(m, makeCylinder(canopyRadius * 0.17f, canopyRadius * 0.11f, trunkH, 7,
                               false, false, barkColor));
    Rng rng(seed);
    for (int i = 0; i < 3; ++i) {
        const float t = static_cast<float>(i) / 2.0f;
        const float y = trunkH * 0.55f + t * (height - trunkH * 0.55f) * 0.72f;
        const float r = canopyRadius * lerpf(1.0f, 0.42f, t) * rng.range(0.92f, 1.08f);
        const float h = (height - y) * lerpf(0.62f, 1.0f, t);
        // No bottom cap: an unlit downward-facing disc reads as a hard black
        // bar cutting through the trunk, which is very obvious in ASCII.
        Mesh cone = makeCylinder(r, 0.0f, h, 8, true, false,
                                 lerp(leafDark, leafLight, t * 0.8f + 0.1f));
        appendMesh(m, cone, Mat4::translation(Vec3(0.0f, y, 0.0f)) *
                            Mat4::rotationY(rng.range(0.0f, TAU)));
    }
    m.computeBounds();
    return m;
}

Mesh buildDeadTree(float height, uint32_t seed) {
    Mesh m;
    const Vec3 bark(0.24f, 0.21f, 0.17f);
    Rng rng(seed);
    appendMesh(m, makeCylinder(0.42f, 0.16f, height, 7, true, true, bark));
    for (int i = 0; i < 5; ++i) {
        const float y = height * rng.range(0.42f, 0.9f);
        const float yaw = rng.range(0.0f, TAU);
        const float pitch = rng.range(0.45f, 1.05f);
        const float len = height * rng.range(0.2f, 0.42f);
        const Vec3 base(0.0f, y, 0.0f);
        const Vec3 dir(std::cos(yaw) * std::sin(pitch), std::cos(pitch), std::sin(yaw) * std::sin(pitch));
        appendMesh(m, makeCylinder(1.0f, 0.35f, 1.0f, 5, true, true, bark),
                   segmentTransform(base, base + dir * len, 0.13f));
    }
    m.computeBounds();
    return m;
}

} // namespace

// ---------------------------------------------------------------- generation

void World::generate(const ArenaDef& arena, uint32_t seed) {
    arena_ = arena;
    const uint32_t s = seed ? seed : 1u;

    // The structural axis is decided before anything is built, because both
    // the terrain (directional canyons) and the structure layout follow it -
    // and the mission lane follows both.
    {
        Rng axisRng(s * 2749u + 31u);
        mainAxisAngle_ = axisRng.range(0.0f, TAU);
    }
    TerrainProfile tp = arena_.terrain;
    if (tp.canyonDepth > 0.01f) tp.canyonAxis = mainAxisAngle_;
    terrain_.generate(tp, s, arena_.extent, 21.0f, 3.0f);

    meshes_.clear();
    pieces_.clear();
    props_.clear();
    obstacles_.clear();

    buildMeshLibrary(s);
    placeStructures(s);
    scatterProps(s);

    grid_.build(obstacles_, arena_.extent + 20.0f, 14.0f);
}

void World::generateTestRange(float extent) {
    ArenaDef flat;
    flat.id = "test_range";
    flat.name = "TEST RANGE";
    flat.extent = extent;
    flat.terrain.hillAmp = 0.0f;
    flat.terrain.midAmp = 0.0f;
    flat.terrain.fineAmp = 0.0f;
    flat.terrain.microAmp = 0.0f;
    arena_ = flat;
    terrain_.generate(flat.terrain, 1u, extent, 21.0f, 2.1f);
    meshes_.clear();
    pieces_.clear();
    props_.clear();
    obstacles_.clear();
    grid_.build(obstacles_, extent + 20.0f, 14.0f);
}

int World::addDynamicObstacle(const Obstacle& o) {
    obstacles_.push_back(o);
    return static_cast<int>(obstacles_.size()) - 1;
}

void World::finalizeObstacles() {
    grid_.build(obstacles_, arena_.extent + 20.0f, 14.0f);
}

void World::disableObstacle(int idx) {
    if (idx >= 0 && idx < static_cast<int>(obstacles_.size()))
        obstacles_[static_cast<size_t>(idx)].active = false;
}

void World::addTestBox(const Vec3& center, const Vec3& half, bool climbable) {
    Obstacle o;
    o.center = center;
    o.half = half;
    o.kind = ObstacleKind::Building;
    o.climbable = climbable;
    obstacles_.push_back(o);
    grid_.build(obstacles_, arena_.extent + 20.0f, 14.0f);
}

void World::buildMeshLibrary(uint32_t seed) {
    Rng rng(seed * 6151u + 7u);
    auto push = [&](StructurePiece&& piece) {
        meshes_.push_back(piece.mesh);
        pieces_.push_back(std::move(piece));
    };
    auto pushPlain = [&](Mesh&& mesh, float radius, float height) {
        StructurePiece p;
        p.mesh = std::move(mesh);
        p.radius = radius;
        p.height = height;
        meshes_.push_back(p.mesh);
        pieces_.push_back(std::move(p));
    };

    // ---- ruins (several silhouettes so a district does not look stamped) ----
    ruinFirst_ = static_cast<int>(meshes_.size());
    ruinCount_ = 6;
    for (int i = 0; i < ruinCount_; ++i) {
        RuinSpec spec;
        const float t = static_cast<float>(i) / (ruinCount_ - 1);
        spec.halfWidth = arena_.ruinScale * rng.range(4.5f, 9.0f);
        spec.halfDepth = arena_.ruinScale * rng.range(4.0f, 8.0f);
        spec.floors = arena_.floorsMin +
                      static_cast<int>(t * static_cast<float>(arena_.floorsMax - arena_.floorsMin) + 0.5f);
        spec.floorHeight = rng.range(3.8f, 4.8f);
        spec.style = i % 4;
        spec.damage = clampf(arena_.ruinDamage + rng.range(-0.15f, 0.15f), 0.0f, 1.0f);
        spec.seed = seed + static_cast<uint32_t>(i) * 977u + 13u;
        spec.tint = arena_.concreteTint * rng.range(0.9f, 1.12f);
        push(buildRuin(spec));
    }

    // ---- vegetation ----
    treeFirst_ = static_cast<int>(meshes_.size());
    treeCount_ = 3;
    pushPlain(buildConiferTree(11.0f, 2.6f, seed + 11u), 1.2f, 11.0f);
    pushPlain(buildConiferTree(6.8f, 2.9f, seed + 23u), 1.3f, 6.8f);
    pushPlain(buildDeadTree(8.4f, seed + 37u), 0.9f, 8.4f);

    // ---- rocks ----
    rockFirst_ = static_cast<int>(meshes_.size());
    rockCount_ = 3;
    pushPlain(makeRock(1.6f, seed + 51u, Vec3(0.30f, 0.30f, 0.29f)), 1.5f, 2.2f);
    pushPlain(makeRock(2.8f, seed + 67u, Vec3(0.26f, 0.26f, 0.27f)), 2.6f, 3.8f);
    pushPlain(makeRock(4.4f, seed + 83u, Vec3(0.23f, 0.24f, 0.25f)), 4.0f, 5.6f);

    // ---- cover props ----
    containerFirst_ = static_cast<int>(meshes_.size());
    containerCount_ = 3;
    push(buildContainer(3.0f, 1.2f, 2.5f, seed + 101u, Vec3(0.30f, 0.34f, 0.30f)));
    push(buildContainer(3.0f, 1.2f, 2.5f, seed + 103u, Vec3(0.36f, 0.28f, 0.22f)));
    push(buildContainer(2.2f, 1.2f, 2.5f, seed + 107u, Vec3(0.26f, 0.30f, 0.36f)));

    barrierFirst_ = static_cast<int>(meshes_.size());
    barrierCount_ = 2;
    push(buildBarrier(2.6f, seed + 131u, Vec3(0.34f, 0.34f, 0.32f)));
    push(buildBarrier(1.7f, seed + 137u, Vec3(0.32f, 0.32f, 0.30f)));

    pipeFirst_ = static_cast<int>(meshes_.size());
    pipeCount_ = 2;
    push(buildPipeRack(6.0f, seed + 151u, Vec3(0.30f, 0.31f, 0.32f)));
    push(buildPipeRack(3.6f, seed + 157u, Vec3(0.31f, 0.30f, 0.28f)));

    bunkerFirst_ = static_cast<int>(meshes_.size());
    bunkerCount_ = 2;
    push(buildBunker(3.4f, seed + 173u, Vec3(0.31f, 0.31f, 0.29f)));
    push(buildBunker(2.4f, seed + 179u, Vec3(0.29f, 0.30f, 0.29f)));

    rubbleFirst_ = static_cast<int>(meshes_.size());
    rubbleCount_ = 3;
    push(buildRubblePile(3.2f, seed + 191u, arena_.concreteTint));
    push(buildRubblePile(2.1f, seed + 193u, arena_.concreteTint * 0.9f));
    push(buildRubblePile(4.6f, seed + 197u, arena_.concreteTint * 1.05f));

    lampFirst_ = static_cast<int>(meshes_.size());
    lampCount_ = 2;
    push(buildLampPost(8.5f, seed + 301u, Vec3(0.40f, 0.42f, 0.44f)));
    push(buildLampPost(9.5f, seed + 311u, Vec3(0.36f, 0.38f, 0.41f)));
    pylonFirst_ = static_cast<int>(meshes_.size());
    pylonCount_ = 1;
    push(buildPylon(26.0f, seed + 331u, Vec3(0.42f, 0.44f, 0.47f)));

    mastFirst_ = static_cast<int>(meshes_.size());
    mastCount_ = 2;
    push(buildAntennaMast(16.0f, seed + 211u, Vec3(0.32f, 0.33f, 0.34f)));
    push(buildAntennaMast(24.0f, seed + 223u, Vec3(0.30f, 0.31f, 0.33f)));

    causewayFirst_ = static_cast<int>(meshes_.size());
    causewayCount_ = 1;
    push(buildCauseway(16.0f, 5.0f, 6.5f, seed + 241u, Vec3(0.36f, 0.36f, 0.35f)));
    // causewayFirst_ + 1: the boarding ramp, placed alongside the chain.
    push(buildCausewayRamp(5.0f, 6.5f, Vec3(0.36f, 0.36f, 0.35f)));

    galleryFirst_ = static_cast<int>(meshes_.size());
    galleryCount_ = 2;
    push(buildGallery(20.0f, 5.5f, 9.0f, seed + 251u, arena_.concreteTint));
    push(buildGallery(13.0f, 4.5f, 8.0f, seed + 257u, arena_.concreteTint * 0.9f));

    // The water plane: one huge quad, drawn only on maps that declare a
    // waterline. Slightly emissive so it reads at night moods.
    if (arena_.waterLevel > -9000.0f) {
        waterMesh_ = makeBox(Vec3(arena_.extent + 60.0f, 0.05f, arena_.extent + 60.0f),
                             Vec3(0.05f, 0.13f, 0.16f));
    } else {
        waterMesh_ = Mesh();
    }
}

void World::addStructure(int meshIndex, const Vec3& pos, float yaw, float scale,
                         const Vec3& tint, bool major) {
    const StructurePiece& piece = pieces_[static_cast<size_t>(meshIndex)];
    PropInstance inst;
    inst.meshIndex = meshIndex;
    inst.xform = Mat4::translation(pos) * Mat4::rotationY(yaw) * Mat4::scaling(Vec3(scale));
    inst.tint = tint;
    inst.pos = pos;
    inst.radius = piece.radius * scale;
    inst.height = piece.height * scale;
    inst.major = major;
    props_.push_back(inst);

    // Local collision boxes into world space. Only yaw and a uniform scale, so
    // the half-extents just scale and the centre transforms.
    for (const Obstacle& o : piece.obstacles) {
        Obstacle w = o;
        w.center = transformPoint(inst.xform, o.center);
        w.half = o.half * scale;
        w.yaw = yaw;
        obstacles_.push_back(w);
    }
}

bool World::spotIsClear(const Vec3& pos, float radius) const {
    for (const PropInstance& p : props_) {
        const float dx = p.pos.x - pos.x, dz = p.pos.z - pos.z;
        const float minD = p.radius + radius;
        if (dx * dx + dz * dz < minD * minD) return false;
    }
    return true;
}

void World::placeStructures(uint32_t seed) {
    Rng rng(seed * 2749u + 31u);
    const float R = arena_.districtRadius;
    // Same rng draw as World::generate made for mainAxisAngle_, so this stays
    // equal to it and existing seeds keep their layouts.
    const float lineAngle = rng.range(0.0f, TAU);

    // The avenue stretch. Maps are much larger than the district radius, and
    // the mission lane runs the long way across them - so every layout is
    // elongated along the structural axis until the built-up ground spans the
    // route. What used to be a disc of city around the origin becomes an
    // avenue of city down the lane, which is what makes a mission read as
    // driving THROUGH somewhere rather than orbiting a knot of buildings.
    const float stretch = clampf((arena_.extent * 0.72f) / std::max(R, 1.0f),
                                 1.0f, 6.0f);
    const float axC = std::cos(lineAngle), axS = std::sin(lineAngle);
    auto elongate = [&](const Vec3& s) {
        const float along = (s.x * axC + s.z * axS) * stretch;
        const float across = -s.x * axS + s.z * axC;
        return Vec3(along * axC - across * axS, s.y,
                    along * axS + across * axC);
    };
    // More ground to build on needs more buildings to line it.
    const float countScale = 0.5f + 0.5f * stretch;

    // Where the next structure goes, given the arena's layout rule.
    auto pickSpot = [&](int i, int total) -> Vec3 {
        switch (arena_.layout) {
            case DistrictLayout::Grid: {
                // Blocks on a street grid, with the whole grid rotated.
                const int side = std::max(2, static_cast<int>(std::sqrt(static_cast<float>(total)) + 0.5f));
                const float pitch = (R * 2.0f) / side;
                const int gx = i % side, gz = (i / side) % side;
                const float x = -R + pitch * (gx + 0.5f) + rng.range(-pitch * 0.16f, pitch * 0.16f);
                const float z = -R + pitch * (gz + 0.5f) + rng.range(-pitch * 0.16f, pitch * 0.16f);
                const float c = std::cos(lineAngle), s = std::sin(lineAngle);
                return Vec3(x * c - z * s, 0.0f, x * s + z * c);
            }
            case DistrictLayout::Ring: {
                const float a = (static_cast<float>(i) / total) * TAU + rng.range(-0.12f, 0.12f);
                const float r = R * rng.range(0.72f, 1.0f);
                return Vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
            }
            case DistrictLayout::Cluster: {
                // Two gaussian-ish samples make a knot that thins at the edges.
                const float a = rng.range(0.0f, TAU);
                const float r = R * (rng.unit() * rng.unit() * 0.5f + rng.unit() * 0.5f);
                return Vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
            }
            case DistrictLayout::Line: {
                const float t = (static_cast<float>(i) / std::max(1, total - 1)) * 2.0f - 1.0f;
                const float along = t * R;
                const float across = rng.range(-R * 0.30f, R * 0.30f);
                return Vec3(std::cos(lineAngle) * along - std::sin(lineAngle) * across, 0.0f,
                            std::sin(lineAngle) * along + std::cos(lineAngle) * across);
            }
            case DistrictLayout::None:
                return Vec3(0.0f, -10000.0f, 0.0f);
            case DistrictLayout::Scattered:
            default: {
                const float a = rng.range(0.0f, TAU);
                const float r = R * std::sqrt(rng.unit());
                return Vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
            }
        }
    };

    // ---- ruins -----------------------------------------------------------
    const int ruinTotal = static_cast<int>(arena_.ruinCount * countScale + 0.5f);
    int placed = 0;
    for (int attempt = 0; attempt < ruinTotal * 8 && placed < ruinTotal; ++attempt) {
        Vec3 spot = elongate(pickSpot(placed, ruinTotal));
        if (spot.y < -1000.0f) break;
        if (lengthXZ(spot) < arena_.spawnClear + 12.0f) continue;
        if (std::fabs(spot.x) > arena_.extent - 20.0f || std::fabs(spot.z) > arena_.extent - 20.0f) continue;

        const int variant = ruinFirst_ + static_cast<int>(rng.unit() * ruinCount_) % ruinCount_;
        const StructurePiece& piece = pieces_[static_cast<size_t>(variant)];
        const float scale = rng.range(0.85f, 1.2f);
        if (!spotIsClear(spot, piece.radius * scale + 6.0f)) continue;

        // Steep ground makes a building float on one corner; skip those spots
        // and sink whatever is left so no edge hangs in the air.
        const float r = piece.radius * scale * 0.75f;
        float lo = 1e9f, hi = -1e9f;
        for (int c = 0; c < 4; ++c) {
            const float a = c * (PI * 0.5f) + PI * 0.25f;
            const float h = terrain_.height(spot.x + std::cos(a) * r, spot.z + std::sin(a) * r);
            lo = std::min(lo, h); hi = std::max(hi, h);
        }
        if (hi - lo > 4.5f) continue;
        spot.y = lo - 0.4f;

        addStructure(variant, spot, rng.range(0.0f, TAU), scale,
                     Vec3(rng.range(0.88f, 1.12f)), true);
        ++placed;
    }

    // ---- fixed-count cover props ----------------------------------------
    auto scatterFamily = [&](int first, int count, int howMany, float spread,
                             float clearance, bool major) {
        if (count <= 0) return;
        for (int i = 0; i < howMany * 6 && howMany > 0; ++i) {
            const float a = rng.range(0.0f, TAU);
            const float rr = spread * std::sqrt(rng.unit());
            Vec3 spot = elongate(Vec3(std::cos(a) * rr, 0.0f, std::sin(a) * rr));
            if (lengthXZ(spot) < arena_.spawnClear * 0.55f) continue;
            if (std::fabs(spot.x) > arena_.extent - 12.0f || std::fabs(spot.z) > arena_.extent - 12.0f) continue;
            if (terrain_.slope(spot.x, spot.z) > 0.40f) continue;
            const int variant = first + static_cast<int>(rng.unit() * count) % count;
            const StructurePiece& piece = pieces_[static_cast<size_t>(variant)];
            const float scale = rng.range(0.9f, 1.15f);
            if (!spotIsClear(spot, piece.radius * scale + clearance)) continue;
            spot.y = terrain_.height(spot.x, spot.z) - 0.15f;
            addStructure(variant, spot, rng.range(0.0f, TAU), scale,
                         Vec3(rng.range(0.85f, 1.15f)), major);
            if (--howMany <= 0) break;
        }
    };

    // ---- causeways --------------------------------------------------------
    // A chain of deck segments along the map's long line, spanning whatever
    // water or low ground lies between the islands. Their deck height rides on
    // the waterline so the road is always just above the sea.
    if (arena_.causewayCount > 0) {
        const float segLen = 32.0f;    // matches buildCauseway halfLen * 2
        const float deckTopWant =
            (arena_.waterLevel > -9000.0f ? arena_.waterLevel : 0.0f) + 3.4f;
        const int cwTotal = static_cast<int>(arena_.causewayCount * stretch + 0.5f);
        for (int i = 0; i < cwTotal; ++i) {
            const float along = (static_cast<float>(i) -
                                 cwTotal * 0.5f + 0.5f) * segLen;
            const Vec3 spot(std::cos(lineAngle) * along, 0.0f,
                            std::sin(lineAngle) * along);
            if (std::fabs(spot.x) > arena_.extent - 24.0f ||
                std::fabs(spot.z) > arena_.extent - 24.0f) continue;
            // Skip a segment only when the island is clearly TALLER than the
            // deck - a segment cut into a low shoulder is a ramp abutment,
            // and skipping those left one-segment gaps at every shoreline
            // that dropped escorts straight into the sea.
            const float ground = terrain_.height(spot.x, spot.z);
            if (ground > deckTopWant + 1.5f) continue;
            Vec3 at = spot;
            at.y = deckTopWant - 6.5f;   // piece origin puts deck top at +6.5
            addStructure(causewayFirst_, at, lineAngle + PI * 0.5f, 1.0f,
                         Vec3(1.0f), true);
            // Boarding ramps every few segments, alternating sides, wherever
            // the shore sits boardably below the deck. Without them a machine
            // with no wall grip simply cannot get ON the road - which was a
            // player standing at a three-metre deck face forever.
            if (i % 3 == 1) {
                const float side = ((i / 3) % 2) ? 1.0f : -1.0f;
                const Vec3 perp(-std::sin(lineAngle), 0.0f, std::cos(lineAngle));
                const Vec3 foot = spot + perp * side * 14.0f;
                const float footG = terrain_.height(foot.x, foot.z);
                if (footG < deckTopWant - 0.8f && footG > deckTopWant - 7.5f) {
                    addStructure(causewayFirst_ + 1, at,
                                 lineAngle + PI * 0.5f + (side < 0.0f ? PI : 0.0f),
                                 1.0f, Vec3(1.0f), true);
                }
            }
        }
    }
    // ---- galleries --------------------------------------------------------
    for (int i = 0; i < arena_.galleryCount; ++i) {
        for (int attempt = 0; attempt < 20; ++attempt) {
            const float a2 = rng.range(0.0f, TAU);
            const float rr = R * rng.range(0.3f, 1.05f);
            Vec3 spot = elongate(Vec3(std::cos(a2) * rr, 0.0f, std::sin(a2) * rr));
            if (lengthXZ(spot) < arena_.spawnClear + 16.0f) continue;
            if (std::fabs(spot.x) > arena_.extent - 30.0f ||
                std::fabs(spot.z) > arena_.extent - 30.0f) continue;
            const int variant = galleryFirst_ + static_cast<int>(rng.unit() * galleryCount_) % galleryCount_;
            const StructurePiece& piece = pieces_[static_cast<size_t>(variant)];
            if (!spotIsClear(spot, piece.radius + 5.0f)) continue;
            if (terrain_.slope(spot.x, spot.z) > 0.25f) continue;
            spot.y = terrain_.height(spot.x, spot.z) - 0.2f;
            addStructure(variant, spot, rng.range(0.0f, TAU), 1.0f, Vec3(1.0f), true);
            break;
        }
    }

    scatterFamily(containerFirst_, containerCount_, arena_.containerCount, R * 1.15f, 1.5f, false);
    scatterFamily(barrierFirst_, barrierCount_, arena_.barrierCount, R * 1.2f, 1.0f, false);
    scatterFamily(pipeFirst_, pipeCount_, arena_.pipeRackCount, R * 1.1f, 3.0f, true);
    scatterFamily(bunkerFirst_, bunkerCount_, arena_.bunkerCount, R * 1.15f, 3.0f, true);
    scatterFamily(rubbleFirst_, rubbleCount_, arena_.rubbleCount, R * 1.25f, 1.0f, false);
    scatterFamily(mastFirst_, mastCount_, arena_.mastCount, R * 1.3f, 6.0f, true);

    // ---- road furniture along the lane ------------------------------------
    // The route is a mile and a half long now, and most of it crosses open
    // ground between the built-up stretches. Street lamps every forty
    // metres, alternating sides, and a pylon line every hundred and sixty,
    // make the lane READ as a road from a long way off - which is what
    // "linear" needs to feel like: a road to follow, not a compass bearing.
    if (arena_.waterLevel <= 0.0f && arena_.layout != DistrictLayout::None) {
        const float laneLen = length(laneAt(1.0f) - laneAt(0.0f)) * 1.05f;
        const int lamps = static_cast<int>(laneLen / 40.0f);
        for (int i = 0; i < lamps; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(lamps);
            const Vec3 here = laneAt(t);
            const Vec3 ahead = laneAt(std::min(1.0f, t + 0.01f));
            Vec3 dir = flattenY(ahead - here);
            if (lengthSq(dir) < 1e-4f) continue;
            dir = normalize(dir);
            const Vec3 perp(dir.z, 0.0f, -dir.x);
            const float side = (i & 1) ? 1.0f : -1.0f;
            Vec3 spot = here + perp * (side * 13.0f);
            if (std::fabs(spot.x) > arena_.extent - 12.0f || std::fabs(spot.z) > arena_.extent - 12.0f) continue;
            if (terrain_.slope(spot.x, spot.z) > 0.42f) continue;
            if (lengthXZ(spot) < arena_.spawnClear * 0.55f) continue;
            const int variant = lampFirst_ + static_cast<int>(rng.unit() * lampCount_) % lampCount_;
            if (!spotIsClear(spot, 3.0f)) continue;
            spot.y = terrain_.height(spot.x, spot.z) - 0.1f;
            // The arm reaches over the road: face the lane.
            const float yaw = std::atan2(-perp.x * side, -perp.z * side);
            addStructure(variant, spot, yaw, 1.0f, Vec3(1.0f), false);
            // A pylon every fourth lamp, well back from the road, one side.
            if (i % 4 == 2) {
                Vec3 ps = here + perp * -42.0f;
                if (std::fabs(ps.x) > arena_.extent - 14.0f || std::fabs(ps.z) > arena_.extent - 14.0f) continue;
                if (terrain_.slope(ps.x, ps.z) > 0.40f) continue;
                if (!spotIsClear(ps, 6.0f)) continue;
                ps.y = terrain_.height(ps.x, ps.z) - 0.2f;
                addStructure(pylonFirst_, ps, std::atan2(dir.x, dir.z) + PI * 0.5f, 1.0f, Vec3(1.0f), true);
            }
        }
    }
}

Vec3 World::laneAt(float t, float sweepScale) const {
    const float ang = PI * 0.5f - mainAxisAngle_;
    const Vec3 axis(std::sin(ang), 0.0f, std::cos(ang));
    const Vec3 perp(axis.z, 0.0f, -axis.x);
    const float ext = arena_.extent * (hasWater() ? 0.68f : 0.92f);
    // One S across the axis (sin 2*pi*t): the same curve whichever end the
    // mission starts from, which is what lets the furniture be placed once.
    const float sweep = hasWater() ? 0.0f
                                   : std::sin(t * TAU) * ext * 0.12f * sweepScale;
    return axis * ((t * 2.0f - 1.0f) * ext) + perp * sweep;
}

void World::scatterProps(uint32_t seed) {
    Rng rng(seed * 7919u + 13u);
    const float extent = arena_.extent;
    const float spacing = 9.0f;
    const int side = static_cast<int>((extent * 2.0f) / spacing);

    for (int j = 0; j < side; ++j) {
        for (int i = 0; i < side; ++i) {
            const float bx = -extent + (i + 0.5f) * spacing;
            const float bz = -extent + (j + 0.5f) * spacing;
            const float x = bx + rng.range(-spacing * 0.45f, spacing * 0.45f);
            const float z = bz + rng.range(-spacing * 0.45f, spacing * 0.45f);

            const float density = fbm(x * 0.011f, z * 0.011f, 3, seed + 4242u) * 0.5f + 0.5f;
            const float slope = terrain_.slope(x, z);
            if (slope > 0.42f) continue;
            if (std::sqrt(x * x + z * z) < arena_.spawnClear) continue;

            const bool wantTree = rng.unit() < density * arena_.treeDensity * 0.30f;
            const bool wantRock = !wantTree && rng.unit() < density * arena_.rockDensity * 0.16f;
            if (!wantTree && !wantRock) continue;

            const int first = wantTree ? treeFirst_ : rockFirst_;
            const int count = wantTree ? treeCount_ : rockCount_;
            const int variant = first + static_cast<int>(rng.unit() * count) % count;
            const StructurePiece& piece = pieces_[static_cast<size_t>(variant)];
            const float scale = rng.range(0.8f, 1.3f);

            Vec3 spot(x, 0.0f, z);
            if (!spotIsClear(spot, piece.radius * scale + 1.0f)) continue;
            spot.y = terrain_.height(x, z) - (wantRock ? 0.4f * scale : 0.0f);

            PropInstance inst;
            inst.xform = Mat4::translation(spot) * Mat4::rotationY(rng.range(0.0f, TAU)) *
                         Mat4::scaling(Vec3(scale));
            inst.tint = Vec3(rng.range(0.85f, 1.15f));
            inst.pos = spot;
            inst.radius = piece.radius * scale;
            inst.height = piece.height * scale;
            inst.major = false;
            props_.push_back(inst);
            props_.back().meshIndex = variant;

            // Trees and rocks are solid but not worth climbing.
            Obstacle o;
            o.center = spot + Vec3(0.0f, inst.height * 0.45f, 0.0f);
            o.half = Vec3(piece.radius * scale * 0.55f, inst.height * 0.45f,
                          piece.radius * scale * 0.55f);
            o.kind = wantTree ? ObstacleKind::Debris : ObstacleKind::Rock;
            o.climbable = false;
            obstacles_.push_back(o);
        }
    }
}

// ------------------------------------------------------------------ drawing

void World::submit(Rasterizer& raster, const Vec3& viewPos, float viewDistance) const {
    if (!waterMesh_.verts.empty()) {
        DrawItem water;
        water.mesh = &waterMesh_;
        water.model = Mat4::translation(Vec3(0.0f, arena_.waterLevel - 0.05f, 0.0f));
        water.emissive = 0.25f;    // reads as a faint self-lit sheet at night
        water.rim = 0.0f;
        water.twoSided = true;     // visible from underneath while flooding
        raster.submit(water);
    }
    const float tileCull = viewDistance + terrain_.tileRadius();
    const std::vector<Mesh>& tiles = terrain_.tiles();
    const std::vector<Vec3>& centers = terrain_.tileCenters();
    for (size_t i = 0; i < tiles.size(); ++i) {
        const Vec3 d = centers[i] - viewPos;
        if (lengthSq(Vec3(d.x, 0.0f, d.z)) > tileCull * tileCull) continue;
        DrawItem item;
        item.mesh = &tiles[i];
        item.rim = 0.12f;          // ground reads by shading, not by silhouette
        raster.submit(item);
    }

    const float minorCull = viewDistance * 0.55f;
    for (const PropInstance& p : props_) {
        const Vec3 d = p.pos - viewPos;
        const float distSq = lengthSq(Vec3(d.x, 0.0f, d.z));
        const float cull = p.major ? viewDistance : minorCull;
        if (distSq > cull * cull) continue;
        DrawItem item;
        item.mesh = &meshes_[static_cast<size_t>(p.meshIndex)];
        item.model = p.xform;
        // Structures are deliberately held down the ramp. Concrete and a steel
        // hull under the same light land on the same glyph, and then a tank
        // standing in front of a building is invisible. Keeping the world in
        // the lower two thirds of the range and the machines above it is what
        // makes the subject of the image legible at glyph resolution.
        item.tint = p.tint * 0.62f;
        // Every structure is a shell: walls with an inside, decks with an
        // underside. Draw both faces so nothing is hollow from any angle.
        item.twoSided = true;
        item.rim = 0.30f;
        raster.submit(item);
    }
}

// ------------------------------------------------------------------ queries

SurfaceHit World::raycast(const Vec3& origin, const Vec3& dir, float maxDist) const {
    SurfaceHit best;
    best.distance = maxDist;

    Vec3 tHit;
    if (terrain_.raycast(origin, dir, maxDist, tHit)) {
        best.hit = true;
        best.point = tHit;
        best.normal = terrain_.normal(tHit.x, tHit.z);
        best.distance = length(tHit - origin);
        best.kind = ObstacleKind::Terrain;
        best.obstacle = -1;
    }

    const Vec3 d = normalize(dir);
    // Broad phase along the ray: sample the grid at intervals rather than
    // testing every obstacle in the level.
    const float step = 12.0f;
    std::vector<int> found;
    for (float t = 0.0f; t <= best.distance + step; t += step) {
        grid_.query(origin + d * std::min(t, best.distance), step, found);
        for (int idx : found) {
            const Obstacle& o = obstacles_[static_cast<size_t>(idx)];
            if (!o.active) continue;
            float tH = 0.0f;
            Vec3 n;
            if (raycastObstacle(o, origin, d, best.distance, tH, n) && tH < best.distance) {
                best.hit = true;
                best.distance = tH;
                best.point = origin + d * tH;
                best.normal = n;
                best.kind = o.kind;
                best.obstacle = idx;
            }
        }
    }
    return best;
}

bool World::lineOfSight(const Vec3& a, const Vec3& b) const {
    const Vec3 delta = b - a;
    const float dist = length(delta);
    if (dist < 1e-4f) return true;
    const SurfaceHit h = raycast(a, delta * (1.0f / dist), dist - 0.4f);
    return !h.hit;
}

SurfaceHit World::findFoothold(const Vec3& searchPoint, const Vec3& up,
                               float castUp, float castDown) const {
    const Vec3 u = normalize(up);
    const Vec3 origin = searchPoint + u * castUp;
    const Vec3 dir = -u;
    const float maxDist = castUp + castDown;

    SurfaceHit best;
    best.distance = maxDist;

    // Terrain, but only when the cast is roughly downward in world terms;
    // marching a height field along a sideways ray is meaningless.
    if (u.y > 0.15f) {
        Vec3 tHit;
        if (terrain_.raycast(origin, dir, maxDist, tHit)) {
            best.hit = true;
            best.point = tHit;
            best.normal = terrain_.normal(tHit.x, tHit.z);
            best.distance = length(tHit - origin);
            best.kind = ObstacleKind::Terrain;
            best.obstacle = -1;
        }
    } else {
        // Hanging off a wall or ceiling: the ground is still worth testing
        // directly beneath, in case the limb is near the base of the surface.
        const float groundY = terrain_.height(searchPoint.x, searchPoint.z);
        const float along = (searchPoint.y - groundY);
        if (along > -0.5f && along < castDown) {
            const Vec3 p(searchPoint.x, groundY, searchPoint.z);
            const float d = length(p - origin);
            if (d < best.distance) {
                best.hit = true;
                best.point = p;
                best.normal = terrain_.normal(p.x, p.z);
                best.distance = d;
                best.kind = ObstacleKind::Terrain;
                best.obstacle = -1;
            }
        }
    }

    grid_.query(searchPoint, maxDist + 4.0f, scratch_);
    for (int idx : scratch_) {
        const Obstacle& o = obstacles_[static_cast<size_t>(idx)];
        if (!o.active) continue;
        float tH = 0.0f;
        Vec3 n;
        if (raycastObstacle(o, origin, dir, best.distance, tH, n) && tH < best.distance) {
            best.hit = true;
            best.distance = tH;
            best.point = origin + dir * tH;
            best.normal = n;
            best.kind = o.kind;
            best.obstacle = idx;
        }
    }
    return best;
}

Vec3 World::resolveCollision(const Vec3& desired, float radius,
                             Vec3* outNormal, ObstacleKind* outKind,
                             float stepOverTop) const {
    Vec3 p = clampToWorld(desired, radius + 2.0f);
    float deepest = 0.0f;
    if (outNormal) *outNormal = Vec3(0.0f, 1.0f, 0.0f);
    if (outKind) *outKind = ObstacleKind::Terrain;

    // Two relaxation passes so the mech slides cleanly out of tight corners.
    for (int pass = 0; pass < 2; ++pass) {
        grid_.query(p, radius + 6.0f, scratch_);
        for (int idx : scratch_) {
            const Obstacle& o = obstacles_[static_cast<size_t>(idx)];
            if (!o.active) continue;
            if (o.center.y + o.half.y < stepOverTop) continue;   // step onto it
            Vec3 push, normal;
            if (!resolveSphereObstacle(o, p, radius, push, normal)) continue;
            p += push;
            const float depth = length(push);
            if (depth > deepest) {
                deepest = depth;
                if (outNormal) *outNormal = normal;
                if (outKind) *outKind = o.kind;
            }
        }
    }
    return clampToWorld(p, radius + 2.0f);
}

bool World::insideSolid(const Vec3& p, float margin) const {
    grid_.query(p, margin + 2.0f, scratch_);
    for (int idx : scratch_) {
        const Obstacle& o = obstacles_[static_cast<size_t>(idx)];
        if (o.active && o.contains(p, margin)) return true;
    }
    return false;
}

Vec3 World::clampToWorld(const Vec3& p, float margin) const {
    const float lim = arena_.extent - margin;
    return Vec3(clampf(p.x, -lim, lim), p.y, clampf(p.z, -lim, lim));
}

bool World::insideStructure(const Vec3& p, float margin) const {
    for (const PropInstance& prop : props_) {
        if (!prop.major) continue;          // only real buildings enclose anything
        const float dx = prop.pos.x - p.x, dz = prop.pos.z - p.z;
        const float r = prop.radius + margin;
        if (dx * dx + dz * dz < r * r) return true;
    }
    return false;
}

Vec3 World::findSpawnPoint(Rng& rng, const Vec3& from, float minDist, float maxDist) const {
    // Two passes. The first insists on open ground well clear of any building;
    // the second relaxes the clearance but still refuses to put a machine
    // inside a footprint, because a hollow ruin passes insideSolid() and then
    // the spawn is boxed in by its own walls with no way out.
    for (int pass = 0; pass < 2; ++pass) {
        const float clearance = (pass == 0) ? 6.0f : 1.5f;
        for (int attempt = 0; attempt < 160; ++attempt) {
            const float a = rng.range(0.0f, TAU);
            const float r = lerpf(minDist, maxDist, rng.unit());
            Vec3 p(from.x + std::cos(a) * r, 0.0f, from.z + std::sin(a) * r);
            p = clampToWorld(p, 18.0f);
            if (lengthXZ(p - from) < minDist * 0.75f) continue;
            if (terrain_.slope(p.x, p.z) > 0.35f) continue;
            p.y = terrain_.height(p.x, p.z) + 2.2f;
            if (hasWater() && p.y - 2.2f < arena_.waterLevel + 0.4f) continue;
            if (insideSolid(p, 2.5f)) continue;
            if (insideStructure(p, clearance)) continue;
            return p;
        }
    }
    // Last resort: walk outward from `from` along a few bearings looking for
    // anywhere at all that is not inside a building.
    for (int i = 0; i < 24; ++i) {
        const float a = (static_cast<float>(i) / 24.0f) * TAU;
        Vec3 p = clampToWorld(from + Vec3(std::cos(a), 0.0f, std::sin(a)) * minDist, 18.0f);
        p.y = terrain_.height(p.x, p.z) + 2.2f;
        if (!insideSolid(p, 2.0f) && !insideStructure(p, 1.0f)) return p;
    }
    Vec3 p = clampToWorld(from + Vec3(minDist, 0.0f, 0.0f), 18.0f);
    p.y = terrain_.height(p.x, p.z) + 2.2f;
    return p;
}

} // namespace sb
