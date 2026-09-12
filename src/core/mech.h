// mech.h - a spidertank: six legs, a turret, and whatever parts are bolted on.
//
// The machine is oriented by its feet, not by the world. Body "up" is the
// average of the surface normals its limbs are gripping, so walking up a
// hillside, onto a wall and across a ceiling are the same code path - the only
// thing that changes is which surfaces the legs are allowed to hold, which is
// what the legs' climb rating decides.
#pragma once

#include <array>
#include <string>
#include <vector>
#include "math3d.h"
#include "mesh.h"
#include "parts.h"
#include "raster.h"
#include "world.h"

namespace sb {

enum class MechState : int {
    Grounded = 0,
    Crouching,     // storing energy for a jump
    Airborne,
    Landing,       // absorbing an impact
    Destroyed
};

const char* mechStateName(MechState s);

enum class Team : int { Player = 0, Hostile };

// What the player or the AI wants the machine to do this frame.
struct MechInput {
    Vec3 moveWorld{0.0f, 0.0f, 0.0f};   // desired travel direction, world space
    float throttle = 0.0f;              // 0..1
    Vec3 aimPoint{0.0f, 0.0f, 0.0f};
    // Sensor-computed firing solution: where to put the rounds so they MEET
    // the target (the red marker on the HUD). Strength is how hard the fire
    // control pulls shots onto it - 0 disables, better sensors give more.
    Vec3 assistPoint{0.0f, 0.0f, 0.0f};
    float assistStrength = 0.0f;
    bool jumpHeld = false;
    bool fireHeld = false;              // fire group 1 (left mouse)
    bool fire2Held = false;             // fire group 2 (right mouse)
    bool sprint = false;                // overdrive the legs for a short burst
    // One ability per part slot: legs / engine / armour / sensor, on their own
    // keys. Nothing overrides anything; the loadout is the ability bar.
    bool ability[4] = {false, false, false, false};
    int toggleMask = 0;                 // bits 0..3: flip that mount on/off this frame
    bool releaseGrip = false;           // deliberately let go of a surface
};

struct Leg {
    Vec3 hipLocal{0.0f, 0.0f, 0.0f};
    Vec3 restLocal{0.0f, 0.0f, 0.0f};
    int group = 0;                      // tripod group, 0 or 1
    float phaseOffset = 0.0f;

    Vec3 foot{0.0f, 0.0f, 0.0f};        // world-space contact target
    Vec3 footNormal{0.0f, 1.0f, 0.0f};
    bool planted = false;

    Vec3 stepFrom{0.0f, 0.0f, 0.0f};
    Vec3 stepTo{0.0f, 0.0f, 0.0f};
    Vec3 stepToNormal{0.0f, 1.0f, 0.0f};
    float stepT = 1.0f;
    bool stepping = false;
    float stepClear = 0.0f;             // arc height this step needs to clear an obstacle

    // Solved chain, world space.
    Vec3 hipWorld{0.0f, 0.0f, 0.0f};
    Vec3 coxaEnd{0.0f, 0.0f, 0.0f};
    Vec3 knee{0.0f, 0.0f, 0.0f};
    Vec3 footSolved{0.0f, 0.0f, 0.0f};
    float extension = 0.0f;             // 0..1 fraction of reach in use
};

// A weapon actually fitted to a hardpoint, with its live state.
struct MountedWeapon {
    const PartDef* part = nullptr;
    Hardpoint mount;
    float cooldown = 0.0f;      // seconds until it can fire again
    int rounds = 0;             // AmmoKind::Limited only
    int reserve = 0;            // spare magazines bought in the store
    float spin = 0.0f;          // cosmetic barrel spin / recoil recovery
    // Live handling state, per mount.
    float spool = 0.0f;         // 0..1 rotary spin-up
    float bloom = 0.0f;         // extra spread accumulated by sustained fire
    int burstLeft = 0;          // rounds remaining in the current burst
    int group = 0;              // 0 = left mouse, 1 = right mouse
    bool enabled = true;        // number keys flip this in the field
};

// Emitted by the mech when it fires; the combat layer turns these into rounds.
struct ShotRequest {
    Vec3 origin;
    Vec3 direction;
    const WeaponDef* weapon = nullptr;
    Team team = Team::Player;
    int shooter = -1;
    // Multiplies the weapon's cone. Bracing, a target lock and a rangefinder
    // all narrow it; this is where those abilities actually land.
    float spreadScale = 1.0f;
    // Steering rate for guided rounds, radians-ish per second. Zero = dumb.
    float homing = 0.0f;
    // Multiplies this round's blast radius and splash damage. A breaching
    // hull sets it above one; nothing else touches it.
    float blastScale = 1.0f;
    // Multiplies how far this round flies before it expires. A fire platform
    // reaches past what is shooting back at it; nothing else touches this.
    float rangeScale = 1.0f;
};

class Mech {
public:
    void init(const World& world, const Loadout& loadout, const Vec3& spawnPos,
              float headingYaw, Team team, uint32_t seed);

    // Rebuilds derived stats and the visual assembly. Cheap enough to call from
    // the store every time the player changes a part.
    void setLoadout(const Loadout& loadout);

    void update(float dt, const World& world, const MechInput& input,
                std::vector<ShotRequest>& shotsOut);

    // Poses the machine on flat imaginary ground at the origin, with no world
    // and no simulation. The store's turntable uses this: a showroom model
    // should stand still and stand correctly, which is exactly what you get by
    // planting every foot at its rest position and solving once.
    void poseStatic(const Loadout& loadout, float yaw);

    // Re-poses an already-loaded machine without rebuilding its assembly, which
    // is what makes a turntable cheap: the 190-piece build happens once, the
    // spin happens every frame.
    void poseAt(const Vec3& position, float yaw);

    void submit(Rasterizer& raster, const Vec3& viewPos) const;

    // Draws only the pieces contributed by one slot. `mount` selects a single
    // weapon hardpoint when slot is Slot::Weapon, or -1 for all of them. The
    // store's part inspector uses this: what you inspect is literally the
    // geometry that gets bolted on, not a stand-in.
    void submitSlotOnly(Rasterizer& raster, Slot slot, int mount) const;

    // ------------------------------------------------------------ combat ----
    bool alive() const { return health_ > 0.0f; }
    void applyDamage(float amount, const Vec3& fromDirection);

    // Multiplies structure, for the campaign's difficulty curve. A tier-0 hull
    // is already a substantial machine, so without this the tutorial enemies
    // are as tough as the player - which is not what "derelict patrol unit"
    // should mean. Call after init; it rescales current health too.
    void scaleHealth(float factor);
    void applyImpulse(const Vec3& v) { vel_ += v; }
    // Checkpoint respawns: bring structure back up to at least this fraction.
    // Field repair from salvage: adds structure, never past the maximum.
    void repairStructure(float amount) {
        if (health_ <= 0.0f) return;
        health_ = std::min(health_ + amount, stats_.maxHealth);
    }
    void restoreHealth(float fraction) {
        health_ = std::max(health_, stats_.maxHealth * clampf(fraction, 0.0f, 1.0f));
        if (health_ > 0.0f && state_ == MechState::Destroyed) state_ = MechState::Grounded;
    }
    float health() const { return health_; }
    float healthFraction() const { return clampf(health_ / std::max(stats_.maxHealth, 1.0f), 0.0f, 1.0f); }
    float heat() const { return heat_; }
    bool overheated() const { return overheated_; }

    // A capsule-ish bound used for hit tests: centre and radius.
    Vec3 hitCentre() const { return pos_ + up_ * 0.35f; }
    float hitRadius() const { return hitRadius_; }

    // ------------------------------------------------------- introspection --
    const Vec3& position() const { return pos_; }
    const Vec3& velocity() const { return vel_; }
    const Vec3& up() const { return up_; }
    const Vec3& forward() const { return forward_; }
    float speed() const { return length(vel_); }
    MechState state() const { return state_; }
    const MechStats& stats() const { return stats_; }
    const Loadout& loadout() const { return loadout_; }
    const std::array<Leg, 6>& legs() const { return legs_; }
    const std::vector<MountedWeapon>& weapons() const { return weapons_; }
    std::vector<MountedWeapon>& weapons() { return weapons_; }
    Team team() const { return team_; }
    int legsInAir() const;
    float climbFraction() const;        // 0 level, 1 fully inverted
    bool onWall() const { return climbing_ || climbFraction() > 0.18f; }
    // Nonzero for ~half a second while the machine pours over a roof lip.
    float crestEase() const { return crestEase_; }
    float cornerStress() const { return cornerStress_; }
    float jumpCharge() const { return jumpCharge_; }
    float mantleFraction() const { return mantle_; }
    bool lastHitFromRear() const { return lastHitFromRear_; }
    // World-space travel direction of the last round that connected, for the
    // HUD's hit-direction indicator. Valid while lastHitAge() is small.
    const Vec3& lastHitDirection() const { return lastHitDir_; }
    float lastHitAge() const { return lastHitAge_; }
    float turretYaw() const { return turretYaw_; }

    // ------------------------------------------------------------ abilities --
    // One per part slot (legs / engine / armour / sensor), each on its own key.
    Ability slotAbility(int slot) const { return stats_.actives[slot].kind; }
    bool abilityReady(int slot) const {
        return abilityCooldown_[slot] <= 0.0f && stats_.actives[slot].kind != Ability::None;
    }
    float abilityCooldownFraction(int slot) const {
        return (stats_.actives[slot].cooldown > 0.0f)
            ? clampf(abilityCooldown_[slot] / stats_.actives[slot].cooldown, 0.0f, 1.0f) : 0.0f;
    }
    bool abilityEngaged(int slot) const { return abilityTimer_[slot] > 0.0f; }
    bool anyAbilityEngaged() const {
        for (int i = 0; i < 4; ++i) if (abilityTimer_[i] > 0.0f) return true;
        return false;
    }
    bool engagedKind(Ability a) const {
        for (int i = 0; i < 4; ++i)
            if (abilityTimer_[i] > 0.0f && stats_.actives[i].kind == a) return true;
        return false;
    }
    float engagedPower(Ability a) const {
        for (int i = 0; i < 4; ++i)
            if (abilityTimer_[i] > 0.0f && stats_.actives[i].kind == a)
                return stats_.actives[i].power;
        return 0.0f;
    }
    bool braced() const { return engagedKind(Ability::Brace); }
    // Running dark: hostiles acquire far more slowly and lose the lock.
    bool silent() const { return engagedKind(Ability::SilentRun); }
    // How loud this machine is to somebody looking for it, 1 = a normal
    // spidertank. Signature masking shaves it; silent running cuts it to a
    // quarter, which is the difference between "they notice you late" and
    // "they lose you entirely and have to search". Ghost was in the catalog
    // as a described ability that nothing anywhere read - this is what makes
    // both of them real.
    float signature() const {
        float sig = 1.0f;
        const int g = static_cast<int>(Ability::Ghost);
        if (stats_.hasPassive[g])
            sig *= clampf(1.0f - 0.26f * stats_.passivePower[g], 0.45f, 1.0f);
        if (silent()) sig = std::min(sig, 0.24f);
        return sig;
    }
    // One-shot flags the mission drains: an EMP that has gone off, and a
    // siphon pulse for the effect layer.
    bool takeEmpPulse() { const bool p = empPulse_ > 0.0f; empPulse_ = 0.0f; return p; }
    float siphonFlash() const { return siphonFlash_; }
    bool locked() const { return engagedKind(Ability::TargetLock); }
    Mat4 bodyMatrix() const { return bodyXform_; }
    Mat4 turretMatrix() const { return turretXform_; }
    Vec3 muzzlePosition(int weaponIndex) const;
    Vec3 aimDirection() const;
    float maxLegReach() const { return stats_.legReach; }

    // Index into the game's mech list. Shots carry it so a round can never
    // hit the machine that fired it.
    void setIndex(int i) { index_ = i; }
    int index() const { return index_; }

    // Bounty value, used by the campaign when this mech is destroyed.
    int bounty() const { return bounty_; }
    void setBounty(int v) { bounty_ = v; }

    // Ammunition helpers for the store.
    void refillAmmo();

    // Called between waves: the reactive plate is good for one heavy hit each.
    void resetWaveState() { reactiveSpent_ = false; }

private:
    void layoutLegs();
    void buildVisual();
    void updateOrientation(float dt, const World& world, const MechInput& in);
    void updateLocomotion(float dt, const World& world, const MechInput& in);
    void updateGait(float dt, const World& world);
    void updateJump(float dt, const World& world, const MechInput& in);
    void solveIK();
    void updateTurret(float dt, const Vec3& aimPoint);
    void updateWeapons(float dt, const MechInput& in, std::vector<ShotRequest>& shotsOut);
    void updateAbility(float dt, const MechInput& in);

    Vec3 localToWorld(const Vec3& local) const;
    bool gripAllowed(const Vec3& normal) const;

    // -------------------------------------------------------------- visual --
    struct VisualPart {
        const Mesh* mesh = nullptr;
        Mat4 local = Mat4::identity();   // relative to the body or turret frame
        Vec3 tint{1.0f, 1.0f, 1.0f};
        float emissive = 0.0f;
        uint8_t lod = 0;                 // 0 always, 1 medium range, 2 close only
        uint8_t frame = 0;               // 0 body, 1 turret, 2+ weapon mount index
        // Which fitted part put this piece on the machine. The store uses it to
        // draw one part in isolation, which is the only honest way to preview
        // something you are about to buy: what you inspect is literally what
        // gets bolted on.
        Slot origin = Slot::Chassis;
    };
    std::vector<VisualPart> visual_;

    // Limb colours, cached from the loadout so the per-frame leg assembly does
    // not have to look them up again.
    Vec3 limbTint_{0.4f, 0.42f, 0.44f};
    Vec3 platePal_{0.55f, 0.55f, 0.52f};   // leg armour matches the hull deck
    float turretMountY_ = -1.0f;   // per-body-plan turret ring height
    Vec3 jointTint_{0.55f, 0.57f, 0.58f};
    Vec3 darkTint_{0.2f, 0.21f, 0.22f};
    Vec3 accentTint_{0.85f, 0.45f, 0.10f};
    Vec3 glowTint_{0.4f, 1.0f, 0.6f};
    float bulk_ = 1.0f;
    // Cached from the fitted legs so the per-frame limb assembly can vary its
    // build without looking the part up six times a frame.
    int legStyle_ = 0;
    float legGirth_ = 1.0f;
    // Per-chassis skeleton: how limbs anchor and articulate. Set by
    // layoutLegs so silhouettes differ at the bone, not just the skin.
    int hullStyle_ = 0;            // chassis style, cached for the leg pass
    float femurFrac_ = 0.42f;      // femur share of leg reach
    float tibiaFrac_ = 0.45f;      // tibia share of leg reach
    float kneeOut_ = 0.0f;         // knee bend biased outward (crab) vs up
    // Fitted armour spills onto the limbs: extra lapped plates in the armour
    // part's own tint, scaled by its mass.
    float armorLegT_ = 0.0f;
    Vec3 armorTint_{0.5f, 0.5f, 0.5f};
    float climbPose_ = 0.0f;       // eases toward 1 on a wall; drives the heave
    // The crest follow-through: set to 1 the instant the wall is released at
    // the top, counting down over ~half a second. It drives the nose-over
    // pose, a faster up-vector settle and a gentle pull onto the roof, so
    // topping out reads as the body POURING over the lip, not popping.
    float crestEase_ = 0.0f;
    // Airborne posture slider: 0 tucked (rising), 1 reaching (falling). A
    // continuous value, because a boolean here snapped the legs open mid-jump.
    float airPose_ = 0.0f;

    Loadout loadout_;
    MechStats stats_;
    std::array<Leg, 6> legs_{};
    std::vector<MountedWeapon> weapons_;

    Team team_ = Team::Player;
    MechState state_ = MechState::Grounded;
    uint32_t seed_ = 1;

    Vec3 pos_{0.0f, 0.0f, 0.0f};
    Vec3 vel_{0.0f, 0.0f, 0.0f};
    Vec3 up_{0.0f, 1.0f, 0.0f};
    Vec3 forward_{0.0f, 0.0f, 1.0f};
    Vec3 supportPoint_{0.0f, 0.0f, 0.0f};
    Vec3 supportNormal_{0.0f, 1.0f, 0.0f};

    float rideHeight_ = 1.9f;
    float crouch_ = 0.0f;          // 0 standing, 1 fully folded
    float gaitPhase_ = 0.0f;
    float bobPhase_ = 0.0f;
    // Free-running clock for spinning barrels, advanced with the spool so a
    // rotary accelerates and coasts rather than snapping between still and
    // blurred.
    float spinPhase_ = 0.0f;
    float recoil_ = 0.0f;
    float airTime_ = 0.0f;
    float peakFallSpeed_ = 0.0f;
    float landTimer_ = 0.0f;
    float jumpCharge_ = 0.0f;
    bool jumpLatched_ = false;
    // What jumping earned you: fall damage is only owed on height beyond what
    // the jump itself gained. A machine that leaps 15 m and lands where it
    // started took a fall it chose; one that walks off a 30 m roof did not.
    float jumpLaunchY_ = 0.0f;
    float apexY_ = -1e9f;
    float gripLost_ = 0.0f;
    float abilityCooldown_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float abilityTimer_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float overdriveDebt_ = 0.0f;   // heat owed after an engine surge
    bool reactiveSpent_ = false;   // the plate absorbs one heavy hit per wave
    bool lastHitFromRear_ = false; // HUD hit-direction cue
    float repairPause_ = 0.0f;     // time since last hit, for the regenerator
    // Wall commitment. Averaging the foot normals is right for ground that
    // varies smoothly, but during a wall transition half the legs are on the
    // ground and half on the face, and the average is a 45-degree diagonal
    // that neither surface can hold. Once the machine is driving into a face
    // its limbs can grip, it commits to that face until it lets go or runs
    // out of wall.
    bool climbing_ = false;
    float climbGrace_ = 0.0f;
    float empPulse_ = 0.0f;      // set the frame an EMP surge fires
    float siphonFlash_ = 0.0f;   // decays; drives the repair flash
    // Getting unstuck. wedged_ is "cannot move"; confineTimer_ is the subtler
    // "moves fine, gets nowhere" - walking laps of a courtyard it cannot leave.
    float confineTimer_ = 0.0f;
    Vec3 confineRef_ = Vec3(0.0f);
    float escapeScan_ = 0.0f;      // countdown to the next bearing sweep
    float escapeOpen_ = 0.0f;      // how far the most open bearing runs
    bool enclosed_ = false;        // scan verdict: boxed in, not just idle
    bool pinched_ = false;         // scan verdict: half the compass blocked close
    float stepOver_ = 0.0f;        // 0..1: how hard the hull is being lifted over a lump
    float footPitch_ = 0.0f;       // hull pitch that follows the feet (cosmetic)
    float stepRaise_ = 0.0f;       // metres of extra ride height asked for by a step-over
    float stepRaiseNow_ = 0.0f;    // smoothed copy the physics uses
    Vec3 escapeDir_ = Vec3(1.0f, 0.0f, 0.0f);
    // Ground height where the current climb began: the floor under every
    // "is this a roof" test, so the pavement at the base of a wall can never
    // be mistaken for the top of it.
    float climbBaseY_ = -1e9f;
    Vec3 climbNormal_{0.0f, 1.0f, 0.0f};
    // Rounding a corner is the hardest thing a climbing machine does: for a
    // moment the limbs are spanning two planes and carrying the load on half
    // the feet. `cornerStress_` decays after a transition and scales the grip
    // down while it lasts, so a specialist steps round and a heavy or
    // ill-suited machine peels off the wall.
    float cornerStress_ = 0.0f;
    float slipTimer_ = 0.0f;
    // How far into a top-out the machine is: fraction of planted feet that
    // are gripping the roof rather than the face. Drives the pitch-over and
    // a climb assist so the crest is a pull, not a stall.
    float mantle_ = 0.0f;

public:
    // How well the limbs are currently holding, 0..1. Below about 0.35 the
    // machine is scrabbling and will start shedding feet.
    float gripQuality() const;
private:
    float wading_ = 0.0f;      // 0 dry .. 1+ hull depth
    float floodTimer_ = 0.0f;
public:
    float wading() const { return wading_; }
private:
    float wedged_ = 0.0f;
    Vec3 lastPos_{0.0f, 0.0f, 0.0f};

    float turretYaw_ = 0.0f;
    Vec3 lastHitDir_{0.0f, 0.0f, 1.0f};
    float lastHitAge_ = 999.0f;
    float turretPitch_ = 0.0f;
    Vec3 gaitVel_{0.0f, 0.0f, 0.0f};    // smoothed velocity used for step planning
    // Body lean: smoothed acceleration drives a small pitch/roll on the hull
    // frame, so the machine noses up when it surges and banks into turns.
    Vec3 leanAccel_{0.0f, 0.0f, 0.0f};
    Vec3 prevFrameVel_{0.0f, 0.0f, 0.0f};

    float health_ = 100.0f;
    float heat_ = 0.0f;
    bool overheated_ = false;
    float hitRadius_ = 2.2f;
    float damageFlash_ = 0.0f;
    int bounty_ = 0;
    int index_ = -1;
    int nextWeapon_ = 0;

    Mat4 bodyXform_ = Mat4::identity();
    Mat4 turretXform_ = Mat4::identity();
};

// The shared library of unit primitives every mech is assembled from. One copy
// for the whole game: a mech is ~190 transforms, not ~190 meshes.
struct MechMeshLibrary {
    Mesh box;          // half-extent 1 cube
    Mesh chamfer;      // chamfered cube
    Mesh cyl6, cyl8, cyl12, cyl4;   // r=1, h=1, along +Y, base at origin
    Mesh cone6;
    Mesh sphere;
    Mesh wedge;        // sloped box, 1x1x1
    Mesh taper;        // frustum: base half 1 at y=0 tapering to 0.55 at y=1
    Mesh pod;          // tapered faceted pod: r 1 at y=0 down to 0.5 at y=1
    static const MechMeshLibrary& instance();
private:
    MechMeshLibrary();
};

} // namespace sb
