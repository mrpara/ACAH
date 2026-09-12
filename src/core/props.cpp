#include "props.h"

namespace sb {

namespace {

// Shared prop meshes, one copy for the whole game.
struct PropMeshes {
    Mesh fuelTank, tankLeg;
    Mesh barrel;
    Mesh crate;
    Mesh mastSection, mastDish;
    Mesh shedBody, shedRoof;
    Mesh stack;
    static const PropMeshes& instance() {
        static PropMeshes m = [] {
            PropMeshes p;
            const Vec3 steel(0.55f, 0.56f, 0.58f);
            const Vec3 rust(0.55f, 0.38f, 0.24f);
            const Vec3 hazard(0.85f, 0.55f, 0.16f);
            const Vec3 crateCol(0.42f, 0.48f, 0.38f);
            const Vec3 concrete(0.5f, 0.5f, 0.48f);
            p.fuelTank = makeCylinder(1.5f, 1.5f, 4.6f, 10, true, true, steel);
            p.tankLeg = makeBox(Vec3(0.14f, 0.55f, 0.14f), Vec3(0.3f, 0.3f, 0.32f));
            p.barrel = makeCylinder(0.45f, 0.45f, 1.1f, 8, true, true, hazard);
            p.crate = makeChamferBox(Vec3(0.75f, 0.55f, 0.75f), 0.08f, crateCol);
            p.mastSection = makeCylinder(0.28f, 0.2f, 5.0f, 6, true, true, steel);
            p.mastDish = makeCylinder(1.1f, 0.15f, 0.5f, 8, true, true, Vec3(0.7f, 0.72f, 0.75f));
            p.shedBody = makeBox(Vec3(2.0f, 1.3f, 1.6f), concrete);
            p.shedRoof = makeSlopedBox(Vec3(2.2f, 0.12f, 1.8f), Vec3(1.6f, 0.1f, 1.3f),
                                       0.5f, rust);
            p.stack = makeCylinder(1.3f, 0.95f, 9.0f, 10, true, true, concrete);
            return p;
        }();
        return m;
    }
};

Destructible makeProp(PropStyle style, const Vec3& pos, float yaw, Rng& rng) {
    Destructible d;
    d.style = style;
    d.pos = pos;
    d.yaw = yaw;
    switch (style) {
        case PropStyle::FuelTank:
            d.maxHealth = 26.0f; d.explosive = true;
            d.blastRadius = 8.5f; d.blastDamage = 55.0f;
            d.cashValue = 68; d.ammoChance = 0.25f;
            d.hitRadius = 2.4f; d.hitHeight = 2.6f;
            break;
        case PropStyle::BarrelCluster:
            d.maxHealth = 8.0f; d.explosive = true;
            d.blastRadius = 4.0f; d.blastDamage = 26.0f;
            d.cashValue = 24; d.ammoChance = 0.3f;
            d.hitRadius = 1.1f; d.hitHeight = 1.2f;
            break;
        case PropStyle::CrateStack:
            d.maxHealth = 12.0f;
            d.cashValue = 30; d.ammoChance = 0.75f;
            d.hitRadius = 1.5f; d.hitHeight = 2.0f;
            break;
        case PropStyle::AntennaMast:
            d.maxHealth = 40.0f;
            d.cashValue = 88; d.ammoChance = 0.15f;
            d.hitRadius = 1.0f; d.hitHeight = 8.0f;
            break;
        case PropStyle::GuardShed:
            d.maxHealth = 45.0f;
            d.cashValue = 56; d.ammoChance = 0.45f;
            d.hitRadius = 2.4f; d.hitHeight = 2.4f;
            break;
        case PropStyle::CoolingStack:
            d.maxHealth = 70.0f;
            d.cashValue = 108; d.ammoChance = 0.2f;
            d.hitRadius = 1.6f; d.hitHeight = 8.5f;
            break;
        default: break;
    }
    d.maxHealth *= rng.range(0.9f, 1.15f);
    d.health = d.maxHealth;
    return d;
}

} // namespace

std::vector<Destructible> furnishArena(World& world, uint32_t seed, int budget,
                                       const Vec3& laneAxis) {
    std::vector<Destructible> out;
    Rng rng(seed * 2654435761u + 17u);
    const float extent = world.arena().extent * 0.86f;

    // The mission corridor. Wide enough that the world does not read as a
    // hallway, narrow enough that nearly everything worth shooting is on the
    // way to the next objective.
    const bool laned = length(laneAxis) > 0.5f;
    const Vec3 axis = laned ? normalize(flattenY(laneAxis)) : Vec3(0.0f, 0.0f, 1.0f);
    const Vec3 perp{axis.z, 0.0f, -axis.x};
    const float halfWidth = std::max(38.0f, world.arena().extent * 0.34f);

    // Weighted style mix: mostly the cheap satisfying stuff, a few set pieces.
    auto rollStyle = [&]() -> PropStyle {
        const float r = rng.unit();
        if (r < 0.22f) return PropStyle::BarrelCluster;
        if (r < 0.46f) return PropStyle::CrateStack;
        if (r < 0.62f) return PropStyle::FuelTank;
        if (r < 0.76f) return PropStyle::GuardShed;
        if (r < 0.90f) return PropStyle::AntennaMast;
        return PropStyle::CoolingStack;
    };

    int placed = 0, attempts = 0;
    while (placed < budget && attempts < budget * 14) {
        ++attempts;
        // Half the props cluster near existing structures - supply dumps grow
        // next to buildings - and half scatter across the open ground.
        Vec3 p;
        if ((attempts & 1) && !world.obstacles().empty()) {
            const Obstacle& host =
                world.obstacles()[rng.next() % world.obstacles().size()];
            const float ang = rng.range(0.0f, TAU);
            const float dist = host.boundRadius() + rng.range(3.0f, 12.0f);
            p = host.center + Vec3(std::sin(ang) * dist, 0.0f, std::cos(ang) * dist);
        } else if (laned) {
            p = axis * rng.range(-extent, extent) +
                perp * rng.range(-halfWidth, halfWidth);
        } else {
            p = Vec3(rng.range(-extent, extent), 0.0f, rng.range(-extent, extent));
        }
        // Keep the supply dumps on the route. A prop far off the corridor is
        // scenery nobody is paid to visit; let a few through so the flanks
        // are not sterile, cull the rest.
        if (laned && std::fabs(dot(p, perp)) > halfWidth && rng.unit() < 0.85f)
            continue;
        p.y = world.terrain().height(p.x, p.z);
        // Reject slopes and occupied ground.
        if (world.terrain().normal(p.x, p.z).y < 0.92f) continue;
        if (world.insideSolid(p + Vec3(0.0f, 1.0f, 0.0f), 1.6f)) continue;

        Destructible d = makeProp(rollStyle(), p, rng.range(0.0f, TAU), rng);

        // Solid enough to block movement? Register a volume.
        if (d.style == PropStyle::FuelTank || d.style == PropStyle::GuardShed ||
            d.style == PropStyle::CoolingStack || d.style == PropStyle::CrateStack) {
            Obstacle o;
            o.center = d.pos + Vec3(0.0f, d.hitHeight * 0.5f, 0.0f);
            o.half = Vec3(d.hitRadius, d.hitHeight * 0.5f, d.hitRadius);
            o.yaw = d.yaw;
            o.kind = ObstacleKind::Debris;
            o.climbable = true;
            d.obstacle = world.addDynamicObstacle(o);
        }
        out.push_back(d);
        ++placed;
    }
    world.finalizeObstacles();
    return out;
}

void submitDestructible(Rasterizer& raster, const Destructible& d, const Vec3& viewPos) {
    if (lengthSq(d.pos - viewPos) > 240.0f * 240.0f) return;
    const PropMeshes& pm = PropMeshes::instance();

    // Dead: the collapse plays over the first second, then the remnant sits
    // there scorched. Nothing blinks out of existence any more - a field you
    // have fought across looks fought across.
    if (!d.alive) {
        const float t = smoothstep01(clampf(d.deadAge / 0.9f, 0.0f, 1.0f));
        const Vec3 scorch = lerp(Vec3(0.55f, 0.45f, 0.38f), Vec3(0.17f, 0.16f, 0.15f),
                                 clampf(d.deadAge / 1.4f, 0.0f, 1.0f));
        auto drawD = [&](const Mesh& m, const Mat4& model) {
            DrawItem it;
            it.mesh = &m;
            it.model = model;
            it.tint = scorch;
            it.twoSided = true;
            raster.submit(it);
        };
        // Topple about a ground pivot on the far side from the fall direction.
        const Mat4 base = Mat4::translation(d.pos) * Mat4::rotationY(d.yaw);
        const float fallDir = d.fallYaw - d.yaw;
        auto topple = [&](float pivotR, float angle) {
            return base * Mat4::rotationY(fallDir) * Mat4::translation(Vec3(0.0f, 0.0f, pivotR)) *
                   Mat4::rotationX(angle) * Mat4::translation(Vec3(0.0f, 0.0f, -pivotR)) *
                   Mat4::rotationY(-fallDir);
        };
        switch (d.style) {
            case PropStyle::FuelTank:
                // The shell splits: two halves slump apart and the cradle stays.
                drawD(pm.fuelTank, base * Mat4::translation(Vec3(-0.5f * t, 1.75f - 0.9f * t, -2.3f)) *
                                       Mat4::rotationZ(-0.55f * t) * Mat4::rotationX(PI * 0.5f) *
                                       Mat4::scaling(Vec3(1.0f, 0.55f, 1.0f)));
                drawD(pm.fuelTank, base * Mat4::translation(Vec3(0.6f * t, 1.75f - 1.1f * t, -2.3f)) *
                                       Mat4::rotationZ(0.75f * t) * Mat4::rotationX(PI * 0.5f) *
                                       Mat4::scaling(Vec3(1.0f, 0.55f, 0.85f)));
                for (int s2 = -1; s2 <= 1; s2 += 2)
                    drawD(pm.tankLeg, base * Mat4::translation(Vec3(0.0f, 0.55f, s2 * 1.5f)));
                break;
            case PropStyle::BarrelCluster:
                // Drums blown over and outward.
                drawD(pm.barrel, base * Mat4::translation(Vec3(-0.5f - 0.6f * t, 0.0f, 0.2f)) *
                                     Mat4::rotationZ(1.4f * t));
                drawD(pm.barrel, base * Mat4::translation(Vec3(0.45f + 0.7f * t, 0.0f, -0.3f)) *
                                     Mat4::rotationZ(-1.3f * t) * Mat4::rotationY(0.5f));
                if (t < 0.95f)
                    drawD(pm.barrel, base * Mat4::translation(Vec3(0.25f, 1.6f * t * (1.0f - t), 0.5f + 0.9f * t)) *
                                         Mat4::rotationX(-1.5f * t));
                break;
            case PropStyle::CrateStack:
                // The top crate slides off; the others crush.
                drawD(pm.crate, base * Mat4::translation(Vec3(-0.4f, 0.55f - 0.25f * t, 0.0f)) *
                                    Mat4::scaling(Vec3(1.0f, 1.0f - 0.45f * t, 1.0f)));
                drawD(pm.crate, base * Mat4::translation(Vec3(1.0f, 0.55f - 0.2f * t, 0.3f)) *
                                    Mat4::rotationY(0.4f) * Mat4::rotationZ(0.3f * t) *
                                    Mat4::scaling(Vec3(1.0f, 1.0f - 0.35f * t, 1.0f)));
                drawD(pm.crate, base * Mat4::translation(Vec3(0.2f - 1.4f * t, 1.65f - 1.1f * t, 0.1f + 0.4f * t)) *
                                    Mat4::rotationY(0.2f) * Mat4::rotationZ(1.1f * t));
                break;
            case PropStyle::AntennaMast:
                // Folds at the joint and comes down whole.
                drawD(pm.mastSection, topple(1.0f, 1.45f * t));
                drawD(pm.mastSection, topple(1.0f, 1.45f * t) * Mat4::translation(Vec3(0.0f, 4.6f, 0.0f)) *
                                          Mat4::rotationX(0.6f * t) *
                                          Mat4::scaling(Vec3(0.75f, 0.75f, 0.75f)));
                break;
            case PropStyle::GuardShed:
                // The roof drops onto flattened walls.
                drawD(pm.shedBody, base * Mat4::translation(Vec3(0.0f, 1.3f - 0.75f * t, 0.0f)) *
                                       Mat4::scaling(Vec3(1.0f + 0.15f * t, 1.0f - 0.6f * t, 1.0f + 0.15f * t)));
                drawD(pm.shedRoof, base * Mat4::translation(Vec3(0.3f * t, 2.65f - 1.5f * t, 0.0f)) *
                                       Mat4::rotationZ(0.35f * t));
                break;
            case PropStyle::CoolingStack:
                // The chimney comes down in the direction it was hit from.
                drawD(pm.stack, topple(2.2f, 1.5f * t));
                break;
            default: break;
        }
        return;
    }

    const Vec3 flash = lerp(Vec3(1.0f, 1.0f, 1.0f), Vec3(1.0f, 0.5f, 0.35f),
                            clampf(d.damageFlash, 0.0f, 1.0f));
    auto draw = [&](const Mesh& m, const Mat4& model, float emissive = 0.0f) {
        DrawItem it;
        it.mesh = &m;
        it.model = model;
        it.tint = flash;
        it.emissive = emissive;
        it.twoSided = true;   // sheds and tanks are shells; stay solid inside
        raster.submit(it);
    };
    const Mat4 base = Mat4::translation(d.pos) * Mat4::rotationY(d.yaw);

    switch (d.style) {
        case PropStyle::FuelTank:
            // Horizontal cylinder on cradle legs.
            draw(pm.fuelTank, base * Mat4::translation(Vec3(0.0f, 1.75f, -2.3f)) *
                                  Mat4::rotationX(PI * 0.5f));
            for (int s = -1; s <= 1; s += 2)
                draw(pm.tankLeg, base * Mat4::translation(Vec3(0.0f, 0.55f, s * 1.5f)));
            break;
        case PropStyle::BarrelCluster:
            draw(pm.barrel, base * Mat4::translation(Vec3(-0.5f, 0.0f, 0.2f)));
            draw(pm.barrel, base * Mat4::translation(Vec3(0.45f, 0.0f, -0.3f)));
            draw(pm.barrel, base * Mat4::translation(Vec3(0.25f, 0.0f, 0.5f)));
            break;
        case PropStyle::CrateStack:
            draw(pm.crate, base * Mat4::translation(Vec3(-0.4f, 0.55f, 0.0f)));
            draw(pm.crate, base * Mat4::translation(Vec3(1.0f, 0.55f, 0.3f)) *
                               Mat4::rotationY(0.4f));
            draw(pm.crate, base * Mat4::translation(Vec3(0.2f, 1.65f, 0.1f)) *
                               Mat4::rotationY(0.2f));
            break;
        case PropStyle::AntennaMast:
            draw(pm.mastSection, base);
            draw(pm.mastSection, base * Mat4::translation(Vec3(0.0f, 4.6f, 0.0f)) *
                                     Mat4::scaling(Vec3(0.75f, 0.75f, 0.75f)));
            draw(pm.mastDish, base * Mat4::translation(Vec3(0.35f, 6.9f, 0.0f)) *
                                  Mat4::rotationZ(-0.5f));
            break;
        case PropStyle::GuardShed:
            draw(pm.shedBody, base * Mat4::translation(Vec3(0.0f, 1.3f, 0.0f)));
            draw(pm.shedRoof, base * Mat4::translation(Vec3(0.0f, 2.65f, 0.0f)));
            break;
        case PropStyle::CoolingStack:
            draw(pm.stack, base);
            break;
        default: break;
    }
}

} // namespace sb
