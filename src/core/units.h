// units.h - the small war: everything hostile that is not a spidertank.
//
// At tank scale most of what you fight is beneath you: power-suit infantry in
// squads, wheeled APCs, conventional gun tanks, fixed turret emplacements and
// rotor drones. They are deliberately NOT Mech instances - a trooper does not
// need six IK-solved limbs and a parts loadout, it needs a position, a health
// number, a gun and just enough brain to advance, shoot and die convincingly.
// Enemy spidertanks stay Mechs and stay rare; these are the texture and the
// pressure of a mission.
#pragma once

#include <cstdint>
#include <vector>

#include "math3d.h"
#include "mech.h"     // ShotRequest, Team
#include "mesh.h"
#include "raster.h"
#include "world.h"

namespace sb {

enum class UnitKind : int {
    Trooper = 0,     // power-suit rifleman: numerous, weak, dies to anything
    ATTrooper,       // the one that matters: carries a launcher that hurts
    APC,             // wheeled troop carrier with a cupola gun
    Tank,            // conventional MBT: real armour, real gun, no legs
    Turret,          // fixed emplacement; never moves, never stops watching
    Drone,           // rotor quad, hovers and harasses from above
    // ---- the second tier, from the middle of the campaign on. Fewer bodies,
    // each of which poses a question the player has to answer specifically.
    Marksman,        // anti-materiel rifle at three hundred metres: one heavy
                     // aimed shot every few seconds. Dies to a sneeze, but you
                     // have to find it first, and it does not have to move.
    Mortar,          // indirect fire onto where you ARE. Cannot depress: walk
                     // INTO its minimum range and it is a crew with sidearms.
    Jammer,          // no real gun. It blinds your fire control and your radar
                     // inside its bubble, which makes it the thing to kill
                     // first and makes ignoring it expensive.
    Warden,          // field repair walker: patches the armour around it
                     // faster than you can chew through it. A strongpoint with
                     // one of these does not fall until the Warden does.
    Count
};

const char* unitKindName(UnitKind k);

struct UnitStats {
    float maxHealth = 20.0f;
    float speed = 3.0f;
    float turnRate = 2.5f;
    float radius = 0.5f;        // hit sphere
    float height = 1.0f;        // eye/muzzle height above ground
    float preferredRange = 40.0f;
    float engageRange = 90.0f;  // will not open fire beyond this
    WeaponDef weapon;
    float burstLen = 0.8f;      // seconds of trigger per burst
    float burstPause = 1.4f;
    int bounty = 40;
    bool crushable = false;     // a mech foot or hull ends it instantly
    float hoverHeight = 0.0f;   // drones: metres above terrain
    // Below this range the gun cannot be brought to bear at all. Indirect fire
    // has a minimum as well as a maximum, and that minimum IS the counter:
    // close the distance and a mortar section is four men with pistols.
    float minRange = 0.0f;
    // Support radius: what a Warden repairs, what a Jammer blinds. Zero for
    // everything that just shoots at you.
    float supportRadius = 0.0f;
    float supportRate = 0.0f;   // structure per second returned to each ally
};

const UnitStats& unitStats(UnitKind k);

class Unit {
public:
    void init(UnitKind kind, const Vec3& pos, float yaw, uint32_t seed);

    // `targetPos` is where the thing it is fighting currently is (the player,
    // or an escort charge); `unitId` keys this unit's shots so a round can be
    // attributed. Movement is ground-clamped except for drones.
    // `targetVel` lets the gunner LEAD a moving target - and lets a fast,
    // jinking machine spoil the solution. Both halves matter: it is what
    // makes standing still lethal and what makes a dodge build work.
    void update(float dt, const World& world, const Vec3& targetPos,
                bool targetVisible, std::vector<ShotRequest>& shots, int unitId,
                const Vec3& targetVel = Vec3(0.0f, 0.0f, 0.0f));

    void applyDamage(float amount);
    // Field repair from a Warden. Never past the maximum, never a resurrection.
    void repair(float amount) {
        if (!alive_) return;
        health_ = std::min(health_ + amount, maxHealthScaled());
    }
    bool alive() const { return alive_; }
    UnitKind kind() const { return kind_; }
    const Vec3& position() const { return pos_; }
    const Vec3& velocity() const { return vel_; }
    Vec3 hitCentre() const { return pos_ + Vec3(0.0f, stats().height * 0.55f, 0.0f); }
    float hitRadius() const { return stats().radius; }
    int bounty() const { return stats().bounty; }
    float health() const { return health_; }
    float healthFraction() const { return clampf(health_ / maxHealthScaled(), 0.0f, 1.0f); }
    const UnitStats& stats() const { return unitStats(kind_); }
    float damageFlash() const { return damageFlash_; }
    // Drops a fresh corpse onto the ground and tips it: called by the mission
    // the frame the unit dies, because applyDamage has no world to ask.
    void settleWreck(const World& world);
    // Skip the garrison hold: this unit was sent at the player and knows it.
    void forceAlert() { alerted_ = true; }
    // Fire control knocked out - by an EMP surge, for now. It can still move;
    // it simply cannot shoot until this runs out.
    void suppress(float seconds) { suppressed_ = std::max(suppressed_, seconds); }
    // How hard this unit's shots group on its current target. 1 = normal;
    // above 1 for a target that is hard to hit at all (a low-profile hull).
    void setTargetProfile(float p) { targetProfile_ = clampf(p, 0.5f, 3.0f); }
    bool suppressed() const { return suppressed_ > 0.0f; }

    // Escorted / friendly variants reuse the same machinery.
    void setTeam(Team t) { team_ = t; }
    Team team() const { return team_; }

    // 0 = fight normally; 1 = march to `goal` (convoys, escorts), shooting on
    // the move but never breaking off the route.
    void setMode(int mode, const Vec3& goal) { mode_ = mode; goal_ = goal; }
    int mode() const { return mode_; }
    // The road a marching unit should trust when the direct line is water: a
    // point on the causeway chain and its direction. Escorts and convoys on
    // island maps steer back to this rather than driving into the sea.
    void setRoad(const Vec3& point, const Vec3& dir) { roadPoint_ = point; roadDir_ = normalize(dir); hasRoad_ = true; }
    // Escorts wait for their protection: the mission raises this when the
    // player has fallen behind, and the charge creeps instead of charging.
    void setHold(bool h) { hold_ = h; }
    void setMarchPace(float p) { marchPace_ = p; }
    // An explicit route: waypoints marched in order before the final goal.
    // Built by the mission from the actual causeway decks, because a straight
    // line to the goal on an island map is a straight line into the sea.
    void setRoute(const std::vector<Vec3>& wps) { route_ = wps; routeAt_ = 0; }
    // Amphibious hulls ford deep water at half pace instead of flooding. The
    // recovery crawler is one; the player's spidertank is pointedly not.
    void setAmphibious(bool a) { amphibious_ = a; }
    // Escort charges are structural, not stock: they survive being the target.
    void scaleHealth(float f) { health_ *= f; healthScale_ = f; }
    // Field repair: a dead escort is disabled, not lost - bring it back at a
    // fraction and let the fight continue. Each revive is paid for elsewhere.
    void revive(float frac) {
        alive_ = true;
        health_ = maxHealthScaled() * clampf(frac, 0.05f, 1.0f);
    }
    void teleport(const Vec3& p) { pos_ = p; vel_ = Vec3(0.0f); }
    float maxHealthScaled() const { return stats().maxHealth * healthScale_; }

    void submit(Rasterizer& raster, const Vec3& viewPos) const;

private:
    UnitKind kind_ = UnitKind::Trooper;
    Team team_ = Team::Hostile;
    bool alive_ = true;
    Vec3 pos_{0.0f, 0.0f, 0.0f};
    Vec3 vel_{0.0f, 0.0f, 0.0f};
    float yaw_ = 0.0f;          // hull facing
    float gunYaw_ = 0.0f;       // turret/torso facing, world space
    float gunPitch_ = 0.0f;
    float health_ = 20.0f;
    float damageFlash_ = 0.0f;
    // Wreck pose, set once when the unit dies: vehicles stay on the field as
    // scorched hulls rather than blinking out of existence.
    float wreckRoll_ = 0.0f, wreckPitch_ = 0.0f;

    float burstTimer_ = 0.0f;   // >0: firing
    float pauseTimer_ = 1.0f;   // >0: waiting between bursts
    float fireCooldown_ = 0.0f;
    float animPhase_ = 0.0f;    // legs / wheels / rotors
    float aiTimer_ = 0.0f;
    Vec3 wander_{0.0f, 0.0f, 1.0f};
    int mode_ = 0;
    Vec3 goal_{0.0f, 0.0f, 0.0f};
    Vec3 roadPoint_{0.0f, 0.0f, 0.0f};
    Vec3 roadDir_{0.0f, 0.0f, 1.0f};
    bool hasRoad_ = false;
    bool hold_ = false;
    float marchPace_ = 0.62f;      // mode-1 cruise as a fraction of speed
    bool amphibious_ = false;
    // Garrison discipline: units hold their strongpoint (anchor_) until the
    // player closes in or shoots them; even then they fight from position
    // rather than pursuing across the map.
    bool alerted_ = false;
    float suppressed_ = 0.0f;   // seconds of EMP silence remaining
    float targetProfile_ = 1.0f;
    Vec3 anchor_{0.0f, 0.0f, 0.0f};
    std::vector<Vec3> route_;
    size_t routeAt_ = 0;
    float healthScale_ = 1.0f;
    uint32_t rng_ = 1u;

    float frand();              // 0..1 from rng_
};

// The one shared set of unit meshes, mirroring MechMeshLibrary: a unit is a
// handful of transforms over these, never its own geometry.
struct UnitMeshLibrary {
    Mesh trooperBody, trooperHead, trooperLimb, launcher;
    Mesh apcHull, wheel, cupola;
    Mesh tankHull, tankTurret, tankBarrel;
    Mesh turretBase, turretHead, turretBarrel;
    Mesh droneBody, rotor;
    // Detail furniture: what turns silhouettes into vehicles.
    Mesh skirt, exhaust, brake, antenna, drum, bullbar, gunpod;
    // The second tier's furniture. Each of these exists so its unit reads at a
    // glance from a hundred metres: a long barrel on a bipod, a stubby tube
    // angled at the sky, a dish, a gantry of repair arms.
    Mesh longBarrel, bipod, mortarTube, baseplate, dish, dishMast, repairArm, walkLeg;
    static const UnitMeshLibrary& instance();
};

} // namespace sb
