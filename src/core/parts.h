// parts.h - the part catalog. Every spidertank, player or enemy, is assembled
// from one part per slot plus whatever weapons its chassis has hardpoints for.
//
// A part carries three things: statistics that feed the derived mech stats,
// a price, and a visual build recipe. Changing a part changes how the machine
// performs *and* how it looks, which is the whole point of the store.
#pragma once

#include <string>
#include <vector>
#include "math3d.h"
#include "mesh.h"

namespace sb {

// ---------------------------------------------------------------------- slots

enum class Slot : int {
    Chassis = 0,
    Legs,
    Engine,
    Armor,
    Sensor,
    Weapon,        // not a single slot: the chassis decides how many
    Count
};

const char* slotName(Slot s);

// Hardpoints and weapons are size-matched: a mount takes any weapon of its own
// class or smaller. This is what stops a light scout carrying siege artillery.
enum class SizeClass : int { Light = 0, Medium, Heavy, Count };
const char* sizeClassName(SizeClass s);

enum class MountKind : int { Chin = 0, Shoulder, Dorsal };

struct Hardpoint {
    Vec3 offset{0.0f, 0.0f, 0.0f};   // chassis-local, before turret rotation
    MountKind kind = MountKind::Chin;
    SizeClass size = SizeClass::Light;
    bool onTurret = true;            // rotates with the turret, or fixed to hull
    float mirror = 0.0f;             // purely cosmetic: which way pods cant
};

// ------------------------------------------------------------------- weapons

enum class AmmoKind : int {
    Unlimited = 0,   // light guns: fire forever, limited only by heat
    Cooldown,        // mediums: unlimited but gated by a per-shot recharge
    Limited          // heavies: finite rounds, bought in the store or scavenged
};

const char* ammoKindName(AmmoKind a);

struct WeaponDef {
    float damage = 10.0f;
    bool homingCapable = false;       // a Homing sensor can steer these
    float projectileSpeed = 120.0f;   // m/s - low speed means you must lead
    float fireInterval = 0.15f;       // seconds between shots
    float spread = 0.006f;            // radians
    float gravity = 0.0f;             // m/s^2, negative arcs the shot
    float blastRadius = 0.0f;         // 0 = no splash
    float blastDamage = 0.0f;
    int pellets = 1;                  // >1 for flak/shotgun bursts
    float heatPerShot = 0.05f;
    float range = 240.0f;

    AmmoKind ammo = AmmoKind::Unlimited;
    float cooldownTime = 0.0f;        // AmmoKind::Cooldown
    int magazine = 0;                 // AmmoKind::Limited - rounds per pickup
    int ammoPrice = 0;                // cost of one magazine in the store

    // ---- handling -------------------------------------------------------
    // How a gun FEELS, as opposed to what it does on paper. Two weapons with
    // the same damage per second play completely differently depending on
    // whether they have to spool up, whether they walk off target under
    // sustained fire, and whether they come out in bursts.
    //
    // spinUp: seconds of held trigger before the gun reaches its listed rate.
    // A rotary starts slow and builds; releasing lets it wind back down.
    float spinUp = 0.0f;
    // bloomPerShot / bloomRecover: the cone opens as you hold the trigger and
    // closes when you let go. This is what makes trigger discipline a skill
    // and what separates a marksman's weapon from a hose.
    float bloomPerShot = 0.0f;   // added to the spread multiplier per round
    float bloomMax = 0.0f;       // ceiling on that multiplier
    float bloomRecover = 2.5f;   // multiplier points shed per second
    // Burst weapons fire this many rounds at fireInterval, then wait burstGap.
    int burstCount = 0;          // 0 = continuous
    float burstGap = 0.0f;

    float tracerLength = 2.6f;
    float tracerRadius = 0.075f;
    Vec3 tracerColor{0.55f, 1.0f, 0.62f};
    float recoil = 0.35f;
};

// ------------------------------------------------------------------ abilities
//
// What a part lets you *do*, as opposed to what it adds to a number. Every
// non-weapon slot can grant one. Actives are triggered by the pilot and run on
// a cooldown; passives are always on and shape how the machine handles.

enum class Ability : int {
    None = 0,
    // --- active ---------------------------------------------------------
    Dash,          // legs: a hard burst along the current heading
    Brace,         // legs: plant and lock, trading mobility for accuracy
    Overdrive,     // engine: temporary speed and cooling surge, costs heat later
    VentHeat,      // engine: dump the heat sink instantly
    Bulwark,       // armour: brief heavy damage reduction
    // --- engine actives. Six reactors used to share two abilities between
    // them, which meant the engine slot was a power budget and nothing else.
    // Each of these makes a reactor the centre of a different machine.
    Siphon,        // engine: dump reactor output into the structure - a field
                   // patch in the middle of a firefight, paid for in heat
    EmpSurge,      // engine: a discharge that kills drones outright and stops
                   // everything nearby shooting for a few seconds
    Capacitor,     // engine: instantly recharges every cooldown weapon and
                   // refunds a magazine - the alpha-strike reactor
    SilentRun,     // engine: signature drops through the floor; enemies lose
                   // the lock and reacquire slowly
    TargetLock,    // sensor: hard lock, tightening convergence and showing lead
    // --- passive --------------------------------------------------------
    ReactivePlate, // armour: the first big hit of each wave is largely absorbed
    Regenerator,   // armour: slow structure repair out of contact
    Stabiliser,    // legs: much less disturbed by impacts and rough ground
    Ghost,         // sensor: enemies acquire you more slowly
    Rangefinder,   // sensor: weapons converge better at long range
    // Specialised sensor automation. Each top-end sensor does ONE of these
    // near-perfectly; no build gets all of them.
    Homing,        // sensor: capable missiles steer themselves
    PointDefense,  // sensor: shoots down incoming rounds near the hull
    AutoDodge,     // sensor: the legs sidestep incoming fire on their own
    Count
};

const char* abilityName(Ability a);
const char* abilityBlurb(Ability a);
bool abilityIsActive(Ability a);

// ----------------------------------------------------------------- part stats

// A chassis TRAIT: the thing that makes one hull play differently from another
// rather than merely weigh more. Every frame in the catalog has exactly one,
// and no two share. This is what turns the chassis slot from "how much
// structure can I afford" into a decision about what kind of machine you are
// driving - which was the point of having nine of them.
enum class Trait : int {
    None = 0,
    Nimble,      // turns and accelerates far harder than its weight suggests
    Frontal,     // sloped glacis: hits from the front arc barely register
    LowProfile,  // a small silhouette: gunners at range group badly on you
    Platform,    // a firing deck: standing still tightens every gun you own
    Sprinter,    // shoots as well moving as standing, and moves faster
    Gunnery,     // fast traverse, little recoil - the gun-handling hull
    Scavenger,   // takes far more out of salvage than anyone else
    Bulk,        // deep magazines: heavy racks last a whole contract
    Breacher,    // its own warheads bite harder and wider
    Spotter,     // long optics: sees further and shoots further
    Count
};

const char* traitName(Trait t);
const char* traitBlurb(Trait t);

struct PartStats {
    float mass = 1.0f;            // tonnes
    float powerDraw = 0.0f;       // MW consumed
    float powerOutput = 0.0f;     // MW produced (engines only)
    float structure = 0.0f;       // hit points contributed
    float armor = 0.0f;           // flat damage reduction per hit
    Trait trait = Trait::None;    // chassis only
    float mobility = 1.0f;        // legs: base speed in m/s
    float agility = 1.0f;         // legs: turn rate and step cadence multiplier
    float jumpPower = 0.0f;       // legs: launch impulse
    float reach = 4.0f;           // legs: total limb length
    float stance = 1.9f;          // legs: ride height
    float sensorRange = 160.0f;   // sensors: how far the AI and lock work
    float heatCapacity = 1.0f;    // how much sustained fire before overheat
    float coolRate = 0.30f;

    // Climbing. 0 = the limb can only hold ground it could stand on anyway;
    // 1 = it can hold any surface at all, including a ceiling. The gripping
    // angle this maps to is 45 + grip * 135 degrees from level, so roughly 0.34
    // is the threshold for holding a sheer wall.
    float climbGrip = 0.0f;
    float climbSpeed = 0.55f;     // fraction of ground speed kept while climbing

    // What this part lets the pilot do. One per part at most.
    Ability ability = Ability::None;
    float abilityPower = 1.0f;    // meaning depends on the ability
    float abilityCooldown = 8.0f; // seconds, actives only
    float abilityDuration = 2.0f; // seconds, actives only

    // Chassis only: the directional armour profile and the hull family.
    float frontDamageMult = 0.70f;
    float rearDamageMult = 1.40f;
};

// The shape of the hull, and the rules that come with it. This is what makes
// two chassis different machines rather than different stat lines.
enum class HullFamily : int {
    Turreted = 0,   // hull + rotating turret: the baseline
    Casemate,       // guns fixed in the bow; aim with the legs, best front plate
    LowProfile,     // flat and wide; hugs cover, small target, thin skin
    Artillery       // open cradle; devastating guns that only fire planted
};

struct PartDef {
    std::string id;
    std::string name;
    std::string maker;
    std::string blurb;
    Slot slot = Slot::Chassis;
    SizeClass size = SizeClass::Light;
    int price = 0;
    int tier = 0;                 // gates what shows up in the store
    PartStats stats;
    HullFamily family = HullFamily::Turreted;   // chassis only
    WeaponDef weapon;             // only meaningful for Slot::Weapon
    Vec3 tint{1.0f, 1.0f, 1.0f};
    int style = 0;                // which visual recipe the builder uses
    std::vector<Hardpoint> hardpoints;   // chassis only
};

// ---------------------------------------------------------------- the catalog

class PartCatalog {
public:
    static const PartCatalog& instance();

    const PartDef* find(const std::string& id) const;
    const std::vector<PartDef>& all() const { return parts_; }

    // Every part in a slot, in catalog order. `maxTier` filters the store.
    std::vector<const PartDef*> bySlot(Slot slot, int maxTier = 99) const;

    // Default free starter loadout.
    const PartDef* starter(Slot slot) const;

private:
    PartCatalog();
    std::vector<PartDef> parts_;
};

// ------------------------------------------------------------------- loadout

struct Loadout {
    std::string chassis, legs, engine, armor, sensor;
    std::vector<std::string> weapons;     // one entry per chassis hardpoint, may be empty
    std::vector<int> weaponGroups;        // 0 = left mouse, 1 = right mouse, per hardpoint

    const PartDef* part(Slot s) const;
    void ensureWeaponSlots();             // resizes `weapons` to the chassis hardpoint count
};

// Everything the simulation actually reads, derived once when a loadout changes.
struct MechStats {
    Trait trait = Trait::None;    // whatever the fitted chassis brings
    float maxSpeed = 6.0f;
    // How much of the top speed came from reactor headroom rather than legs.
    // The workshop shows it so "buy a bigger reactor" reads as a real
    // mobility choice and not just a power budget number.
    float driveFactor = 1.0f;
    float acceleration = 20.0f;
    float turnRate = 3.0f;
    float stepCadence = 1.0f;      // multiplies gait frequency
    float jumpImpulse = 0.0f;
    float legReach = 4.0f;
    float standHeight = 1.9f;
    float maxHealth = 100.0f;
    float armor = 0.0f;
    float sensorRange = 160.0f;
    float heatCapacity = 1.0f;
    float coolRate = 0.3f;

    // Maximum surface tilt from level, in radians, that the limbs can hold.
    // Heavier machines grip worse: the same claws have more mass to hold up.
    float climbAngle = deg2rad(45.0f);
    float climbSpeedFactor = 0.55f;
    bool canClimbWalls = false;

    // One active per slot - legs, engine, armour, sensor, in that order, on
    // their own keys (Q/E/R/F) - plus every passive. Nothing overrides.
    struct ActiveAbility {
        Ability kind = Ability::None;
        float power = 1.0f;
        float cooldown = 8.0f;
        float duration = 2.0f;
    };
    ActiveAbility actives[4];
    bool hasPassive[static_cast<int>(Ability::Count)] = {};
    float passivePower[static_cast<int>(Ability::Count)] = {};

    // Directional armour: incoming damage is multiplied by the arc it arrives
    // through. The chassis family sets these - a casemate has the best frontal
    // plate in the game and the thinnest back.
    float frontDamageMult = 0.70f;
    float rearDamageMult = 1.40f;

    // Hull family rules.
    HullFamily hullFamily = HullFamily::Turreted;
    float turretYawLimit = 100.0f;   // radians; effectively free unless casemate
    bool plantToFire = false;        // artillery: heavy guns only fire standing
    float profileScale = 1.0f;       // low-profile: smaller target, lower stance

    float mass = 1.0f;
    float powerOutput = 0.0f;
    float powerDraw = 0.0f;
    float powerMargin = 1.0f;      // <1 means overdrawn: everything gets worse
    bool overdrawn = false;
};

MechStats deriveStats(const Loadout& loadout);

// A short human-readable stat line for the store, e.g. "MASS 12.4t  PWR 3.1/4.0".
std::string loadoutSummary(const Loadout& loadout, const MechStats& stats);

} // namespace sb
