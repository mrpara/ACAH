// combat.h - projectiles, damage and the debris they leave behind.
//
// Every shot is a real object that travels at its weapon's own speed and, for
// the heavy stuff, falls under gravity. Nothing hitscans. That is deliberate:
// the difference between a 420 m/s lance and a 64 m/s plasma lob is the whole
// texture of a fight, because the slow weapons only connect if you read where
// the target is going rather than where it is.
#pragma once

#include <vector>
#include "math3d.h"
#include "mech.h"
#include "parts.h"
#include "raster.h"
#include "world.h"

namespace sb {

// A live round in flight.
struct Projectile {
    Vec3 pos{0.0f, 0.0f, 0.0f};
    Vec3 prev{0.0f, 0.0f, 0.0f};     // last frame's position, for swept hit tests
    Vec3 vel{0.0f, 0.0f, 0.0f};
    float gravity = 0.0f;
    float damage = 0.0f;
    float blastRadius = 0.0f;
    float blastDamage = 0.0f;
    float life = 0.0f;               // seconds remaining before it fizzles
    float tracerLength = 2.6f;
    float tracerRadius = 0.075f;
    Vec3 color{0.55f, 1.0f, 0.62f};
    Team team = Team::Player;
    int shooter = -1;                // index into the mech list, -1 for none
    float homing = 0.0f;             // guided rounds steer toward the enemy
    bool alive = true;
};

// Short-lived visual: sparks at an impact, a muzzle bloom, an explosion ball.
struct Effect {
    Vec3 pos{0.0f, 0.0f, 0.0f};
    Vec3 vel{0.0f, 0.0f, 0.0f};
    Vec3 color{1.0f, 0.8f, 0.4f};
    float radius = 0.2f;
    float growth = 0.0f;             // radius gained per second
    float life = 0.4f;
    float maxLife = 0.4f;
    float gravity = 0.0f;
    bool spark = false;              // sparks streak; blooms are round
    int shooter = -1;                // whose gun made it (0 = player), or -1
};

// A magazine left on the field by a destroyed machine, or scattered by the
// level generator. Walking over it tops up a matching heavy weapon.
struct AmmoPickup {
    Vec3 pos{0.0f, 0.0f, 0.0f};
    std::string weaponId;            // which weapon it feeds
    int rounds = 0;
    float bob = 0.0f;                // animation phase
    float life = 90.0f;              // seconds before it despawns
    bool taken = false;
    // A repair crate instead of a magazine: `structure` points of hull,
    // welded on where you stand. weaponId is empty for these.
    float structure = 0.0f;
    bool isRepair() const { return structure > 0.0f; }
};

// One gun going off, and one round landing. The audio layer turns these into
// positioned sound: every shot in the world reports, every hit answers.
struct FireEvent {
    Vec3 pos{0.0f, 0.0f, 0.0f};
    const WeaponDef* weapon = nullptr;
    Team team = Team::Player;
    int shooter = -1;                // 0 = the player's own guns
};
struct ImpactEvent {
    Vec3 pos{0.0f, 0.0f, 0.0f};
    int surface = 0;                 // 0 dirt/concrete, 1 metal, 2 blast
    float energy = 1.0f;             // rough loudness driver
};

// Result of resolving one frame of combat, handed back to the campaign.
struct CombatEvents {
    std::vector<int> destroyed;      // indices of mechs killed this frame
    std::vector<int> unitsKilled;    // indices into the mission's unit list
    std::vector<int> propsKilled;    // indices into the mission's destructibles
    std::vector<FireEvent> fired;    // every shot spawned since the last frame
    std::vector<ImpactEvent> impacts;
    float playerDamageTaken = 0.0f;
    float playerDamageDealt = 0.0f;
    int pickupsCollected = 0;
    int repairsCollected = 0;
};

class Combat {
public:
    void reset();

    // Turns a mech's shot requests into projectiles.
    void spawnShots(const std::vector<ShotRequest>& shots, Rng& rng);

    // How hard hostile fire lands, as a fraction of the listed weapon rating.
    // The campaign sets this per mission. It exists because making the enemies
    // genuinely aggressive and making the guns hit twice as hard were asked
    // for in the same breath, and together they made the early campaign a
    // wall: the player faces four of them and they face one of the player.
    // Blunting the damage keeps the fights long and readable without making
    // the AI stupid, which was the thing actually worth keeping.
    void setHostileDamageScale(float s) { hostileDamage_ = s; }
    float hostileDamageScale() const { return hostileDamage_; }

    // Advances every round, resolves hits against `mechs`, the small units,
    // the destructibles and the world, applies damage, and reports what died.
    // `mechs` is indexed by ShotRequest::shooter; units carry ids of
    // -100-unitIndex. `units` and `props` may be null (the tools use that).
    void update(float dt, const World& world, std::vector<Mech*>& mechs,
                CombatEvents& events,
                std::vector<class Unit>* units = nullptr,
                std::vector<struct Destructible>* props = nullptr);

    // Drops the heavy magazines a destroyed machine was carrying.
    void dropSalvage(const Mech& victim, Rng& rng);

    void addPickup(const Vec3& pos, const std::string& weaponId, int rounds);
    // A REPAIR crate: structure salvaged off a wreck. With no checkpoint
    // respawn any more, this is the only way to recover during a long
    // contract - which makes clearing a strongpoint worth doing rather than
    // something to sneak past, and keeps a fifteen-minute mission from
    // being one slow unrecoverable bleed.
    void addRepair(const Vec3& pos, float structure);

    // `eyeRadius` > 0 means the view is FROM INSIDE the player's machine:
    // the player's own rounds and muzzle blooms are drawn small and dim
    // inside that radius of the eye and not at all right at it, so a rotary
    // gun's stream does not wallpaper the windscreen. Everything else -
    // hostile fire, impacts, rounds already downrange - draws as normal.
    void submit(Rasterizer& raster, const Vec3& viewPos, float eyeRadius = 0.0f) const;

    // Spawns a bloom plus a spray of sparks. Public because the mech's own
    // destruction sequence uses it.
    void explosion(const Vec3& at, float radius, const Vec3& color, Rng& rng);

    // Point defense: destroy the closest live hostile round within `radius`
    // of `center`. Returns true if one died. The mission drives this from the
    // AEGIS sensor.
    bool interceptOne(const Vec3& center, float radius, Team defender);

    const std::vector<Projectile>& projectiles() const { return shots_; }
    const std::vector<AmmoPickup>& pickups() const { return pickups_; }
    int liveProjectiles() const;

private:
    void impact(Projectile& p, const Vec3& at, const Vec3& normal,
                const World& world, std::vector<Mech*>& mechs, CombatEvents& ev,
                std::vector<class Unit>* units = nullptr,
                std::vector<struct Destructible>* props = nullptr,
                int surface = 0);
    void applySplash(const Projectile& p, const Vec3& at,
                     std::vector<Mech*>& mechs, CombatEvents& ev,
                     std::vector<class Unit>* units = nullptr,
                     std::vector<struct Destructible>* props = nullptr);
    void updatePickups(float dt, std::vector<Mech*>& mechs, CombatEvents& ev);

    std::vector<Projectile> shots_;
    std::vector<Effect> effects_;
    std::vector<AmmoPickup> pickups_;
    std::vector<FireEvent> pendingFire_;   // spawned between updates
    Rng rng_{20260828u};
    float hostileDamage_ = 1.0f;

    // Unit meshes reused for every tracer and spark.
    mutable bool meshesBuilt_ = false;
    mutable Mesh tracerMesh_;        // a thin box along +Z, length 1
    mutable Mesh blastMesh_;         // a low-poly sphere
    void ensureMeshes() const;
};

// Swept sphere-vs-segment test, exposed because the AI uses it to decide
// whether an incoming round is worth dodging.
bool segmentHitsSphere(const Vec3& a, const Vec3& b, const Vec3& centre,
                       float radius, float* tOut);

// Where to aim to hit a target moving at constant velocity with a round of the
// given speed. Returns false if the round can never catch it.
bool leadTarget(const Vec3& shooter, const Vec3& target, const Vec3& targetVel,
                float projectileSpeed, Vec3* aimPointOut);

} // namespace sb
