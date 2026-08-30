#include "buildings.h"

namespace sb {

namespace {

// Adds a box to the visual mesh. Everything here is axis-aligned in the
// structure's own space; the instance transform handles placement and yaw.
void addBox(Mesh& m, const Vec3& center, const Vec3& half, const Vec3& color, float chamfer = 0.0f) {
    if (half.x <= 1e-4f || half.y <= 1e-4f || half.z <= 1e-4f) return;
    Mesh b = (chamfer > 0.0f) ? makeChamferBox(half, chamfer, color) : makeBox(half, color);
    appendMesh(m, b, Mat4::translation(center));
}

Obstacle box(const Vec3& center, const Vec3& half, ObstacleKind kind, bool climbable = true) {
    Obstacle o;
    o.center = center;
    o.half = half;
    o.kind = kind;
    o.climbable = climbable;
    return o;
}

// One wall of a floor, split into segments around window openings. `axis` 0 is a
// wall running along X, 1 along Z.
void addWallRun(Mesh& m, Rng& rng, int axis, float side, float halfLen, float wallZ,
                float y0, float y1, float thickness, const Vec3& color,
                float openness, bool ground) {
    const int bays = std::max(2, static_cast<int>(halfLen / 1.9f));
    const float bayW = (halfLen * 2.0f) / bays;
    const float sillH = (y1 - y0) * 0.26f;
    const float headH = (y1 - y0) * 0.24f;

    for (int i = 0; i < bays; ++i) {
        const float c = -halfLen + bayW * (i + 0.5f);
        const bool missing = rng.unit() < openness;
        const bool glazed = !ground && rng.unit() > 0.25f;

        auto place = [&](float cy, float hy, float pierW) {
            const Vec3 half = (axis == 0) ? Vec3(pierW, hy, thickness) : Vec3(thickness, hy, pierW);
            const Vec3 pos = (axis == 0) ? Vec3(c, cy, wallZ * side) : Vec3(wallZ * side, cy, c);
            addBox(m, pos, half, color);
        };

        if (missing) continue;

        if (glazed) {
            // Sill, head, and a mullion each side: reads as a blown-out window.
            const float pierW = bayW * 0.16f;
            place(y0 + sillH * 0.5f, sillH * 0.5f, bayW * 0.5f);
            place(y1 - headH * 0.5f, headH * 0.5f, bayW * 0.5f);
            const float midY = (y0 + sillH + y1 - headH) * 0.5f;
            const float midH = (y1 - headH - y0 - sillH) * 0.5f;
            if (midH > 0.05f) {
                const Vec3 halfL = (axis == 0) ? Vec3(pierW, midH, thickness) : Vec3(thickness, midH, pierW);
                const Vec3 pL = (axis == 0) ? Vec3(c - bayW * 0.5f + pierW, midY, wallZ * side)
                                            : Vec3(wallZ * side, midY, c - bayW * 0.5f + pierW);
                const Vec3 pR = (axis == 0) ? Vec3(c + bayW * 0.5f - pierW, midY, wallZ * side)
                                            : Vec3(wallZ * side, midY, c + bayW * 0.5f - pierW);
                addBox(m, pL, halfL, color);
                addBox(m, pR, halfL, color);
            }
        } else {
            place((y0 + y1) * 0.5f, (y1 - y0) * 0.5f, bayW * 0.5f);
        }
    }
}

} // namespace

// ---------------------------------------------------------------------- ruins

StructurePiece buildRuin(const RuinSpec& spec) {
    StructurePiece out;
    Rng rng(spec.seed ? spec.seed : 7u);

    const float hw = spec.halfWidth;
    const float hd = spec.halfDepth;
    const float fh = spec.floorHeight;
    const int floors = std::max(1, spec.floors);
    const float wallT = 0.28f;

    const Vec3 concrete = spec.tint;
    const Vec3 concreteDark = spec.tint * 0.72f;
    const Vec3 concreteLight = spec.tint * 1.22f;
    const Vec3 rust(spec.tint.x * 1.1f, spec.tint.y * 0.72f, spec.tint.z * 0.48f);

    // ---- foundation ------------------------------------------------------
    addBox(out.mesh, Vec3(0.0f, 0.22f, 0.0f), Vec3(hw + 0.5f, 0.22f, hd + 0.5f), concreteDark, 0.12f);

    // ---- floors ----------------------------------------------------------
    // Upper floors are progressively more ruined; the topmost is usually a
    // partial shell, which is what makes a skyline read as a war zone.
    int intactFloors = floors;
    for (int f = 0; f < floors; ++f) {
        const float y0 = 0.44f + f * fh;
        const float y1 = y0 + fh;
        const float wear = spec.damage * (0.25f + 0.75f * (static_cast<float>(f) / std::max(1, floors - 1)));
        const bool collapsed = rng.unit() < wear * 0.35f && f > 0;
        if (collapsed) { intactFloors = f; break; }

        // Floor slab.
        addBox(out.mesh, Vec3(0.0f, y0 + 0.14f, 0.0f), Vec3(hw, 0.14f, hd), concreteDark);

        // Corner columns.
        for (int sx = -1; sx <= 1; sx += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                addBox(out.mesh, Vec3(sx * (hw - 0.3f), (y0 + y1) * 0.5f, sz * (hd - 0.3f)),
                       Vec3(0.34f, fh * 0.5f, 0.34f), concreteLight, 0.1f);
            }
        }

        const float openness = clampf(wear * 0.55f, 0.0f, 0.6f);
        const bool ground = (f == 0);
        addWallRun(out.mesh, rng, 0, -1.0f, hw - 0.4f, hd - wallT, y0 + 0.28f, y1, wallT, concrete, openness, ground);
        addWallRun(out.mesh, rng, 0,  1.0f, hw - 0.4f, hd - wallT, y0 + 0.28f, y1, wallT, concrete, openness, ground);
        addWallRun(out.mesh, rng, 1, -1.0f, hd - 0.4f, hw - wallT, y0 + 0.28f, y1, wallT, concrete, openness, ground);
        addWallRun(out.mesh, rng, 1,  1.0f, hd - 0.4f, hw - wallT, y0 + 0.28f, y1, wallT, concrete, openness, ground);

        // Interior partition stubs, visible through the blown-out walls.
        if (spec.style != 3 && rng.unit() < 0.7f) {
            const float px = rng.range(-hw * 0.5f, hw * 0.5f);
            addBox(out.mesh, Vec3(px, (y0 + y1) * 0.5f, rng.range(-hd * 0.4f, hd * 0.4f)),
                   Vec3(0.18f, fh * 0.42f, rng.range(1.0f, hd * 0.7f)), concreteDark);
        }
    }

    const float topY = 0.44f + intactFloors * fh;

    // ---- roof and damage -------------------------------------------------
    if (intactFloors >= floors) {
        addBox(out.mesh, Vec3(0.0f, topY + 0.16f, 0.0f), Vec3(hw, 0.16f, hd), concreteLight);
        // Parapet.
        for (int sz = -1; sz <= 1; sz += 2)
            addBox(out.mesh, Vec3(0.0f, topY + 0.55f, sz * (hd - 0.2f)), Vec3(hw, 0.34f, 0.18f), concrete);
        for (int sx = -1; sx <= 1; sx += 2)
            addBox(out.mesh, Vec3(sx * (hw - 0.2f), topY + 0.55f, 0.0f), Vec3(0.18f, 0.34f, hd), concrete);
        // Rooftop clutter.
        const int clutter = 2 + static_cast<int>(rng.unit() * 3.0f);
        for (int i = 0; i < clutter; ++i) {
            addBox(out.mesh,
                   Vec3(rng.range(-hw * 0.7f, hw * 0.7f), topY + 0.9f, rng.range(-hd * 0.7f, hd * 0.7f)),
                   Vec3(rng.range(0.5f, 1.3f), rng.range(0.4f, 0.9f), rng.range(0.5f, 1.2f)),
                   rng.unit() < 0.4f ? rust : concreteDark, 0.12f);
        }
    } else {
        // Sheared-off top. The collision deck below caps the shell at wall
        // height, so the visible top must be solid too: a collapsed-rubble
        // deck fills the shell to the brim, and the shards sit on it. Without
        // this a climber topped out standing on an invisible surface over an
        // open interior.
        // A collapsed top is a BROKEN ROOF, not a floating table. The deck is
        // inset from the facade, it sits on a course of surviving wall stubs,
        // and a ragged rubble rim borders it - without those it read as a
        // slab hovering over an open, wall-less storey, which from a
        // distance is the "diamond-shaped roof with a hole" that got
        // reported.
        const float deckW = hw * 0.97f, deckD = hd * 0.97f;
        // Surviving wall course under the deck edge, so the slab is carried.
        for (int sz = -1; sz <= 1; sz += 2)
            addBox(out.mesh, Vec3(0.0f, topY - 0.55f, sz * (hd - wallT)),
                   Vec3(hw, 0.55f, wallT), concrete);
        for (int sx = -1; sx <= 1; sx += 2)
            addBox(out.mesh, Vec3(sx * (hw - wallT), topY - 0.55f, 0.0f),
                   Vec3(wallT, 0.55f, hd), concrete);
        addBox(out.mesh, Vec3(0.0f, topY - 0.10f, 0.0f), Vec3(deckW, 0.24f, deckD),
               concreteDark);
        // Ragged rim: broken parapet stubs around the edge, uneven on purpose.
        for (int e = 0; e < 4; ++e) {
            const bool alongX = (e < 2);
            const float sgn = (e & 1) ? 1.0f : -1.0f;
            const int segs = 5;
            for (int k = 0; k < segs; ++k) {
                if (rng.unit() < 0.34f) continue;      // blown-away sections
                const float t2 = (k + 0.5f) / segs * 2.0f - 1.0f;
                const float hgt = rng.range(0.16f, 0.42f);
                const Vec3 c = alongX
                    ? Vec3(t2 * deckW * 0.9f, topY + 0.14f + hgt, sgn * deckD)
                    : Vec3(sgn * deckW, topY + 0.14f + hgt, t2 * deckD * 0.9f);
                const Vec3 h2 = alongX ? Vec3(deckW / segs * 0.85f, hgt, 0.16f)
                                       : Vec3(0.16f, hgt, deckD / segs * 0.85f);
                addBox(out.mesh, c, h2, concrete);
            }
        }
        // Sheared-off floor: jagged slab remnants and exposed rebar.
        const int shards = 5 + static_cast<int>(rng.unit() * 5.0f);
        for (int i = 0; i < shards; ++i) {
            const float a = rng.range(0.0f, TAU);
            addBox(out.mesh,
                   Vec3(std::cos(a) * hw * rng.range(0.3f, 0.95f), topY + rng.range(0.0f, 0.7f),
                        std::sin(a) * hd * rng.range(0.3f, 0.95f)),
                   Vec3(rng.range(0.4f, 1.1f), rng.range(0.15f, 0.6f), rng.range(0.4f, 1.1f)),
                   concreteDark);
        }
        const int rebar = 4 + static_cast<int>(rng.unit() * 6.0f);
        for (int i = 0; i < rebar; ++i) {
            const Vec3 base(rng.range(-hw * 0.9f, hw * 0.9f), topY, rng.range(-hd * 0.9f, hd * 0.9f));
            const Vec3 tip = base + Vec3(rng.range(-0.5f, 0.5f), rng.range(0.8f, 2.2f), rng.range(-0.5f, 0.5f));
            Mesh bar = makeCylinder(1.0f, 0.7f, 1.0f, 4, false, false, rust);
            appendMesh(out.mesh, bar, segmentTransform(base, tip, 0.055f));
        }
    }

    // ---- ground-level damage --------------------------------------------
    const int scars = 3 + static_cast<int>(rng.unit() * 4.0f);
    for (int i = 0; i < scars; ++i) {
        const float a = rng.range(0.0f, TAU);
        addBox(out.mesh,
               Vec3(std::cos(a) * (hw + 0.8f), rng.range(0.2f, 0.8f), std::sin(a) * (hd + 0.8f)),
               Vec3(rng.range(0.5f, 1.4f), rng.range(0.2f, 0.6f), rng.range(0.5f, 1.4f)),
               concreteDark);
    }

    out.mesh.computeBounds();
    out.height = topY + 1.0f;
    out.radius = std::sqrt(hw * hw + hd * hd) + 1.0f;

    // ---- collision -------------------------------------------------------
    // Four full-height perimeter slabs so the walls are clean climbing faces,
    // with a doorway gap in one of them so the interior stays usable as cover.
    const float wallHalfH = (topY) * 0.5f;
    const float wallCenterY = topY * 0.5f;
    const int doorSide = static_cast<int>(rng.unit() * 4.0f) & 3;
    const float doorHalf = 2.0f;

    for (int side = 0; side < 4; ++side) {
        const bool alongX = (side < 2);
        const float sign = (side & 1) ? 1.0f : -1.0f;
        const float runHalf = alongX ? hw : hd;
        const float offset = alongX ? hd : hw;

        auto emit = [&](float centre, float half) {
            if (half <= 0.15f) return;
            const Vec3 c = alongX ? Vec3(centre, wallCenterY, sign * offset)
                                  : Vec3(sign * offset, wallCenterY, centre);
            const Vec3 h = alongX ? Vec3(half, wallHalfH, wallT)
                                  : Vec3(wallT, wallHalfH, half);
            out.obstacles.push_back(box(c, h, ObstacleKind::Building));
        };

        if (side == doorSide && spec.style != 3) {
            const float remain = (runHalf - doorHalf) * 0.5f;
            emit(-(doorHalf + remain), remain);
            emit( (doorHalf + remain), remain);
        } else {
            emit(0.0f, runHalf);
        }
    }
    // Roof deck, so a climber has somewhere to stand at the top.
    out.obstacles.push_back(box(Vec3(0.0f, topY + 0.1f, 0.0f), Vec3(hw, 0.3f, hd), ObstacleKind::Building));
    return out;
}

// ---------------------------------------------------------------- cover props

StructurePiece buildContainer(float len, float wid, float hgt, uint32_t seed, const Vec3& tint) {
    StructurePiece out;
    Rng rng(seed ? seed : 3u);
    const Vec3 body = tint;
    const Vec3 trim = tint * 1.3f;
    const Vec3 dark = tint * 0.65f;

    addBox(out.mesh, Vec3(0.0f, hgt * 0.5f, 0.0f), Vec3(len, hgt * 0.5f, wid), body, 0.12f);
    // Corrugation.
    const int ribs = std::max(3, static_cast<int>(len * 1.6f));
    for (int i = 0; i < ribs; ++i) {
        const float x = -len + (len * 2.0f) * (i + 0.5f) / ribs;
        addBox(out.mesh, Vec3(x, hgt * 0.5f, 0.0f), Vec3(len / ribs * 0.28f, hgt * 0.46f, wid + 0.05f), dark);
    }
    // Corner castings and rails.
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            addBox(out.mesh, Vec3(sx * len, hgt * 0.5f, sz * wid), Vec3(0.16f, hgt * 0.5f, 0.16f), trim);
    addBox(out.mesh, Vec3(0.0f, hgt, 0.0f), Vec3(len, 0.09f, wid), trim);
    addBox(out.mesh, Vec3(0.0f, 0.06f, 0.0f), Vec3(len, 0.06f, wid), trim);
    // Door end.
    addBox(out.mesh, Vec3(len * 0.99f, hgt * 0.5f, 0.0f), Vec3(0.06f, hgt * 0.42f, wid * 0.9f), dark);
    if (rng.unit() < 0.5f)
        addBox(out.mesh, Vec3(-len * 0.99f, hgt * 0.62f, wid * 0.3f), Vec3(0.05f, 0.22f, 0.5f), trim);

    out.mesh.computeBounds();
    out.height = hgt;
    out.radius = std::sqrt(len * len + wid * wid);
    out.obstacles.push_back(box(Vec3(0.0f, hgt * 0.5f, 0.0f), Vec3(len, hgt * 0.5f, wid),
                                ObstacleKind::Container));
    return out;
}

StructurePiece buildBarrier(float len, uint32_t seed, const Vec3& tint) {
    StructurePiece out;
    Rng rng(seed ? seed : 11u);
    const float h = 1.5f;
    // Jersey barrier profile: wide foot tapering to a narrow top.
    Mesh body = makeSlopedBox(Vec3(len, 0.0f, 0.62f), Vec3(len, 0.0f, 0.26f), h, tint);
    appendMesh(out.mesh, body);
    addBox(out.mesh, Vec3(0.0f, h * 0.94f, 0.0f), Vec3(len, 0.09f, 0.30f), tint * 1.25f);
    // Hazard chevrons.
    const int marks = std::max(2, static_cast<int>(len));
    for (int i = 0; i < marks; ++i) {
        const float x = -len + (len * 2.0f) * (i + 0.5f) / marks;
        addBox(out.mesh, Vec3(x, h * 0.55f, 0.30f), Vec3(len / marks * 0.3f, 0.26f, 0.03f),
               (i & 1) ? Vec3(0.85f, 0.62f, 0.10f) : tint * 0.55f);
    }
    if (rng.unit() < 0.4f)
        addBox(out.mesh, Vec3(rng.range(-len, len), 0.2f, rng.range(-1.2f, 1.2f)),
               Vec3(0.4f, 0.2f, 0.4f), tint * 0.6f);

    out.mesh.computeBounds();
    out.height = h;
    out.radius = len + 0.7f;
    out.obstacles.push_back(box(Vec3(0.0f, h * 0.5f, 0.0f), Vec3(len, h * 0.5f, 0.45f),
                                ObstacleKind::Barrier));
    return out;
}

StructurePiece buildPipeRack(float len, uint32_t seed, const Vec3& tint) {
    StructurePiece out;
    Rng rng(seed ? seed : 17u);
    const float h = 3.6f;
    const Vec3 steel = tint;
    const Vec3 pipe = tint * Vec3(1.25f, 1.05f, 0.85f);

    // Uprights and cross-braces.
    const int posts = std::max(2, static_cast<int>(len / 2.4f) + 1);
    for (int i = 0; i < posts; ++i) {
        const float x = -len + (len * 2.0f) * i / std::max(1, posts - 1);
        for (int sz = -1; sz <= 1; sz += 2)
            addBox(out.mesh, Vec3(x, h * 0.5f, sz * 0.9f), Vec3(0.16f, h * 0.5f, 0.16f), steel);
        addBox(out.mesh, Vec3(x, h - 0.15f, 0.0f), Vec3(0.14f, 0.14f, 0.9f), steel);
    }
    // Pipes running the length.
    for (int i = 0; i < 4; ++i) {
        const float y = h * (0.42f + 0.16f * i);
        const float z = rng.range(-0.65f, 0.65f);
        Mesh p = makeCylinder(1.0f, 1.0f, 1.0f, 7, true, true, pipe);
        appendMesh(out.mesh, p, segmentTransform(Vec3(-len, y, z), Vec3(len, y, z), rng.range(0.16f, 0.3f)));
    }
    addBox(out.mesh, Vec3(0.0f, h + 0.08f, 0.0f), Vec3(len, 0.08f, 1.0f), steel * 1.15f);

    out.mesh.computeBounds();
    out.height = h + 0.2f;
    out.radius = len + 1.2f;
    // Only the deck collides, so a mech can walk under the rack or climb on top.
    out.obstacles.push_back(box(Vec3(0.0f, h - 0.1f, 0.0f), Vec3(len, 0.4f, 1.0f), ObstacleKind::Pillar));
    for (int sz = -1; sz <= 1; sz += 2)
        out.obstacles.push_back(box(Vec3(-len, h * 0.5f, sz * 0.9f), Vec3(0.2f, h * 0.5f, 0.2f),
                                    ObstacleKind::Pillar));
    return out;
}

StructurePiece buildBunker(float radius, uint32_t seed, const Vec3& tint) {
    StructurePiece out;
    Rng rng(seed ? seed : 23u);
    const float h = 2.6f;
    Mesh shell = makeSlopedBox(Vec3(radius, 0.0f, radius * 0.8f),
                               Vec3(radius * 0.78f, 0.0f, radius * 0.6f), h, tint);
    appendMesh(out.mesh, shell);
    // Firing slit and roof cap.
    addBox(out.mesh, Vec3(0.0f, h * 0.62f, radius * 0.72f), Vec3(radius * 0.7f, 0.18f, 0.12f), tint * 0.4f);
    addBox(out.mesh, Vec3(0.0f, h + 0.14f, 0.0f), Vec3(radius * 0.85f, 0.14f, radius * 0.68f), tint * 1.2f);
    for (int i = 0; i < 4; ++i) {
        const float a = rng.range(0.0f, TAU);
        addBox(out.mesh, Vec3(std::cos(a) * radius * 1.1f, 0.28f, std::sin(a) * radius * 0.95f),
               Vec3(rng.range(0.3f, 0.7f), 0.28f, rng.range(0.3f, 0.7f)), tint * 0.7f);
    }
    out.mesh.computeBounds();
    out.height = h + 0.3f;
    out.radius = radius * 1.3f;
    out.obstacles.push_back(box(Vec3(0.0f, h * 0.5f, 0.0f), Vec3(radius * 0.92f, h * 0.5f, radius * 0.72f),
                                ObstacleKind::Building));
    return out;
}

StructurePiece buildRubblePile(float radius, uint32_t seed, const Vec3& tint) {
    StructurePiece out;
    Rng rng(seed ? seed : 29u);
    const int chunks = 10 + static_cast<int>(rng.unit() * 10.0f);
    float top = 0.0f;
    for (int i = 0; i < chunks; ++i) {
        const float a = rng.range(0.0f, TAU);
        const float r = rng.range(0.0f, radius);
        const float fall = 1.0f - (r / std::max(radius, 0.01f));
        const Vec3 h(rng.range(0.25f, 0.8f), rng.range(0.2f, 0.7f), rng.range(0.25f, 0.8f));
        const Vec3 c(std::cos(a) * r, h.y * 0.9f + fall * radius * 0.35f, std::sin(a) * r);
        addBox(out.mesh, c, h, tint * rng.range(0.7f, 1.2f), 0.08f);
        top = std::max(top, c.y + h.y);
    }
    out.mesh.computeBounds();
    out.height = top;
    out.radius = radius;
    out.obstacles.push_back(box(Vec3(0.0f, top * 0.42f, 0.0f), Vec3(radius * 0.8f, top * 0.42f, radius * 0.8f),
                                ObstacleKind::Debris));
    return out;
}

StructurePiece buildAntennaMast(float height, uint32_t seed, const Vec3& tint) {
    StructurePiece out;
    Rng rng(seed ? seed : 31u);
    const Vec3 steel = tint;
    addBox(out.mesh, Vec3(0.0f, 0.3f, 0.0f), Vec3(1.4f, 0.3f, 1.4f), tint * 0.8f, 0.2f);
    // Lattice tower: four legs plus alternating braces.
    const int segs = std::max(3, static_cast<int>(height / 3.0f));
    const float segH = height / segs;
    for (int s = 0; s < segs; ++s) {
        const float y0 = 0.6f + s * segH, y1 = y0 + segH;
        const float r0 = lerpf(0.85f, 0.28f, static_cast<float>(s) / segs);
        const float r1 = lerpf(0.85f, 0.28f, static_cast<float>(s + 1) / segs);
        for (int i = 0; i < 4; ++i) {
            const float a = i * (PI * 0.5f) + PI * 0.25f;
            const Vec3 a0(std::cos(a) * r0, y0, std::sin(a) * r0);
            const Vec3 a1(std::cos(a) * r1, y1, std::sin(a) * r1);
            Mesh leg = makeCylinder(1.0f, 1.0f, 1.0f, 4, false, false, steel);
            appendMesh(out.mesh, leg, segmentTransform(a0, a1, 0.075f));
            const float b = a + PI * 0.5f;
            const Vec3 b1(std::cos(b) * r1, y1, std::sin(b) * r1);
            Mesh brace = makeCylinder(1.0f, 1.0f, 1.0f, 3, false, false, steel * 0.85f);
            appendMesh(out.mesh, brace, segmentTransform(a0, b1, 0.045f));
        }
    }
    // Dish and warning light.
    appendMesh(out.mesh, makeCylinder(0.9f, 0.2f, 0.35f, 8, true, false, steel * 1.2f),
               Mat4::translation(Vec3(0.0f, height * 0.72f, 0.55f)) * Mat4::rotationX(deg2rad(-70.0f)));
    appendMesh(out.mesh, makeSphere(0.16f, 4, 6, Vec3(0.9f, 0.3f, 0.25f)),
               Mat4::translation(Vec3(0.0f, height + 0.7f, 0.0f)));
    (void)rng;

    out.mesh.computeBounds();
    out.height = height + 0.9f;
    out.radius = 1.6f;
    out.obstacles.push_back(box(Vec3(0.0f, height * 0.5f, 0.0f), Vec3(0.55f, height * 0.5f, 0.55f),
                                ObstacleKind::Pillar));
    return out;
}


StructurePiece buildCauseway(float halfLen, float halfWid, float height,
                             uint32_t seed, const Vec3& tint) {
    Rng rng(seed * 373u + 5u);
    StructurePiece out;
    const Vec3 deckCol = tint;
    const Vec3 railCol = tint * 0.8f;
    const Vec3 pillarCol = tint * 0.65f;

    // Deck.
    addBox(out.mesh, Vec3(0.0f, height - 0.35f, 0.0f),
           Vec3(halfLen, 0.35f, halfWid), deckCol, 0.1f);
    out.obstacles.push_back(box(Vec3(0.0f, height - 0.35f, 0.0f),
                                Vec3(halfLen, 0.35f, halfWid),
                                ObstacleKind::Building));
    // Side rails - low enough to shoot over, high enough to read as a road.
    for (int sz = -1; sz <= 1; sz += 2) {
        addBox(out.mesh, Vec3(0.0f, height + 0.35f, sz * (halfWid - 0.25f)),
               Vec3(halfLen, 0.35f, 0.2f), railCol);
        out.obstacles.push_back(box(Vec3(0.0f, height + 0.35f, sz * (halfWid - 0.25f)),
                                    Vec3(halfLen, 0.35f, 0.2f),
                                    ObstacleKind::Barrier));
    }
    // Pillars down to well below the waterline.
    const int pillars = std::max(2, static_cast<int>(halfLen / 9.0f) + 1);
    for (int i = 0; i < pillars; ++i) {
        const float x = -halfLen + (2.0f * halfLen) * (i + 0.5f) / pillars;
        addBox(out.mesh, Vec3(x, height * 0.5f - 6.0f, 0.0f),
               Vec3(1.0f, height * 0.5f + 6.0f, 1.0f), pillarCol);
        out.obstacles.push_back(box(Vec3(x, height * 0.5f - 6.0f, 0.0f),
                                    Vec3(1.0f, height * 0.5f + 6.0f, 1.0f),
                                    ObstacleKind::Pillar));
    }
    out.radius = std::sqrt(halfLen * halfLen + halfWid * halfWid);
    out.height = height + 0.8f;
    (void)rng;
    return out;
}


StructurePiece buildCausewayRamp(float halfWid, float height, const Vec3& tint) {
    // The on-ramp: a stair of wide slabs descending from the deck edge to the
    // shore, so any machine - climber or not - can board the chain. Placed at
    // intervals along the causeway like highway ramps.
    StructurePiece out;
    const Vec3 col = tint * 0.9f;
    const int steps = 6;
    for (int k = 0; k < steps; ++k) {
        const float top = height - 0.95f * (k + 1);
        if (top < 0.6f) break;
        const float zc = halfWid + 1.6f + 3.1f * k;
        addBox(out.mesh, Vec3(0.0f, top * 0.5f, zc),
               Vec3(4.2f, top * 0.5f, 1.7f), col * (1.0f - 0.03f * k), 0.08f);
        out.obstacles.push_back(box(Vec3(0.0f, top * 0.5f, zc),
                                    Vec3(4.2f, top * 0.5f, 1.7f),
                                    ObstacleKind::Building));
    }
    out.radius = halfWid + 1.6f + 3.1f * steps;
    out.height = height;
    return out;
}

StructurePiece buildGallery(float halfLen, float halfWid, float clearance,
                            uint32_t seed, const Vec3& tint) {
    Rng rng(seed * 509u + 3u);
    StructurePiece out;
    const Vec3 wallCol = tint * 0.9f;
    const Vec3 roofCol = tint * 0.7f;
    const float wallT = 0.9f;
    const float wallH = clearance * 0.5f;

    for (int sz = -1; sz <= 1; sz += 2) {
        addBox(out.mesh, Vec3(0.0f, wallH, sz * (halfWid + wallT)),
               Vec3(halfLen, wallH, wallT), wallCol);
        out.obstacles.push_back(box(Vec3(0.0f, wallH, sz * (halfWid + wallT)),
                                    Vec3(halfLen, wallH, wallT),
                                    ObstacleKind::Building));
    }
    // Roof slab; its top is standable, its underside is what makes the run a
    // tunnel. Solid on every face - no hollow look from above.
    addBox(out.mesh, Vec3(0.0f, clearance + 0.55f, 0.0f),
           Vec3(halfLen, 0.55f, halfWid + wallT * 2.0f), roofCol);
    out.obstacles.push_back(box(Vec3(0.0f, clearance + 0.55f, 0.0f),
                                Vec3(halfLen, 0.55f, halfWid + wallT * 2.0f),
                                ObstacleKind::Building));
    // Portal frames at the mouths, for silhouette.
    for (int sx = -1; sx <= 1; sx += 2)
        addBox(out.mesh, Vec3(sx * halfLen, clearance * 0.5f, 0.0f),
               Vec3(0.4f, clearance * 0.5f, halfWid + wallT * 2.0f),
               wallCol * 1.1f, 0.1f);
    out.radius = std::sqrt(halfLen * halfLen + (halfWid + wallT) * (halfWid + wallT));
    out.height = clearance + 1.1f;
    (void)rng;
    return out;
}

} // namespace sb
