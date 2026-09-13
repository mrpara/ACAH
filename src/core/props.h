// props.h - things that exist to be destroyed.
//
// The core loop of an ACAH mission is progressing through a defended level
// wrecking it, so the levels are furnished with destructibles: fuel tanks that
// go up in a fireball and take their neighbours with them, crate stacks that
// burst into salvage, antenna masts, guard sheds, barrel clusters. Each one is
// worth money on the destruction ledger, many drop ammunition, and the
// explosive ones are weapons in themselves if the player reads the ground.
#pragma once

#include <cstdint>
#include <vector>

#include "math3d.h"
#include "mesh.h"
#include "raster.h"
#include "world.h"

namespace sb {

enum class PropStyle : int {
    FuelTank = 0,    // big horizontal cylinder: the fireball
    BarrelCluster,   // three drums: small fireball, cheap
    CrateStack,      // supply crates: no blast, best ammo drop
    AntennaMast,     // tall lattice: mission-objective material
    GuardShed,       // small hut: crunches satisfyingly
    CoolingStack,    // industrial chimney: tall, dramatic collapse
    Count
};

struct Destructible {
    PropStyle style = PropStyle::CrateStack;
    Vec3 pos{0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float health = 30.0f;
    float maxHealth = 30.0f;
    bool alive = true;
    bool explosive = false;
    float blastRadius = 0.0f;
    float blastDamage = 0.0f;
    int cashValue = 30;
    int obstacle = -1;          // index into World's obstacle list, or -1
    float ammoChance = 0.25f;   // chance of dropping a magazine on death
    float hitRadius = 1.4f;
    float hitHeight = 1.6f;     // hit sphere centre sits at half this
    float damageFlash = 0.0f;
    bool shielded = false;      // under a live shield pylon this frame
    // Seconds since it died. Drives the collapse (a stack topples, a tank
    // splits and slumps, a mast folds, a chimney comes down in the direction
    // of the hit) and the scorched remnant that stays on the field after.
    float deadAge = 0.0f;
    float fallYaw = 0.0f;       // which way it went over

    Vec3 hitCentre() const { return pos + Vec3(0.0f, hitHeight * 0.5f, 0.0f); }

    void applyDamage(float amount) {
        if (shielded && amount < 900.0f) { damageFlash = 0.5f; return; }
        if (!alive) return;
        health -= amount;
        damageFlash = 1.0f;
        if (health <= 0.0f) { health = 0.0f; alive = false; }
    }
};

// Furnishes a generated world with destructibles: clustered near structures,
// scattered along open ground, never inside a building footprint. Registers
// blocking obstacles for the solid ones and returns the set.
//
// `laneAxis` is the mission's direction of travel. Props concentrate in a
// corridor along it so the billable scenery lines the route the objectives
// actually take, instead of tempting the player off into empty map corners.
// Pass a zero vector for the old anywhere-goes scatter.
std::vector<Destructible> furnishArena(World& world, uint32_t seed, int budget,
                                       const Vec3& laneAxis = Vec3(0.0f, 0.0f, 0.0f));

void submitDestructible(Rasterizer& raster, const Destructible& d, const Vec3& viewPos);

} // namespace sb
