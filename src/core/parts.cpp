#include "parts.h"

#include <cstdio>

namespace sb {

const char* slotName(Slot s) {
    switch (s) {
        case Slot::Chassis: return "CHASSIS";
        case Slot::Legs:    return "LEGS";
        case Slot::Engine:  return "ENGINE";
        case Slot::Armor:   return "ARMOUR";
        case Slot::Sensor:  return "SENSOR";
        case Slot::Weapon:  return "WEAPON";
        default:            return "?";
    }
}

const char* sizeClassName(SizeClass s) {
    switch (s) {
        case SizeClass::Light:  return "LIGHT";
        case SizeClass::Medium: return "MEDIUM";
        case SizeClass::Heavy:  return "HEAVY";
        default:                return "?";
    }
}

const char* ammoKindName(AmmoKind a) {
    switch (a) {
        case AmmoKind::Unlimited: return "UNLIMITED";
        case AmmoKind::Cooldown:  return "COOLDOWN";
        case AmmoKind::Limited:   return "LIMITED";
        default:                  return "?";
    }
}

namespace {

// Reference mass for the mobility curve. A mech at exactly this weight moves at
// its legs' rated speed; lighter is quicker, heavier is slower.
constexpr float kReferenceMass = 12.0f;

Hardpoint hp(float x, float y, float z, MountKind kind, SizeClass size, bool turret) {
    Hardpoint h;
    h.offset = Vec3(x, y, z);
    h.kind = kind;
    h.size = size;
    h.onTurret = turret;
    h.mirror = (x < 0.0f) ? -1.0f : 1.0f;
    return h;
}

} // namespace

const char* traitName(Trait t) {
    switch (t) {
        case Trait::Nimble:     return "NIMBLE FRAME";
        case Trait::Frontal:    return "SLOPED GLACIS";
        case Trait::LowProfile: return "LOW PROFILE";
        case Trait::Platform:   return "FIRING PLATFORM";
        case Trait::Sprinter:   return "RUNNING GUNS";
        case Trait::Gunnery:    return "GUN HANDLING";
        case Trait::Scavenger:  return "SALVAGE RIG";
        case Trait::Bulk:       return "DEEP MAGAZINES";
        case Trait::Breacher:   return "BREACHING CHARGE";
        case Trait::Spotter:    return "LONG OPTICS";
        default:                return "";
    }
}

const char* traitBlurb(Trait t) {
    switch (t) {
        case Trait::Nimble:
            return "Turns and accelerates far above its weight.";
        case Trait::Frontal:
            return "Sloped front: hits from ahead do a third less.";
        case Trait::LowProfile:
            return "Small silhouette. Gunners at range group badly on you.";
        case Trait::Platform:
            return "Stand still and every gun on the hull tightens right up.";
        case Trait::Sprinter:
            return "Shoots as well moving as parked, and moves quicker.";
        case Trait::Gunnery:
            return "Fast traverse, almost no recoil climb.";
        case Trait::Scavenger:
            return "Salvage and ammunition crates give far more.";
        case Trait::Bulk:
            return "Deep racks: limited weapons carry extra magazines.";
        case Trait::Breacher:
            return "Your own warheads bite harder and wider.";
        case Trait::Spotter:
            return "Sees much further, and the fire control reaches with it.";
        default: return "";
    }
}

const char* abilityName(Ability a) {
    switch (a) {
        case Ability::Dash:          return "DASH";
        case Ability::Brace:         return "BRACE";
        case Ability::Overdrive:     return "OVERDRIVE";
        case Ability::VentHeat:      return "HEAT PURGE";
        case Ability::Bulwark:       return "BULWARK";
        case Ability::TargetLock:    return "TARGET LOCK";
        case Ability::Siphon:        return "STRUCTURE SIPHON";
        case Ability::EmpSurge:      return "EMP SURGE";
        case Ability::Capacitor:     return "CAPACITOR DUMP";
        case Ability::SilentRun:     return "SILENT RUNNING";
        case Ability::ReactivePlate: return "REACTIVE PLATE";
        case Ability::Regenerator:   return "FIELD REPAIR";
        case Ability::Stabiliser:    return "GYRO STABILISER";
        case Ability::Ghost:         return "SIGNATURE MASKING";
        case Ability::Rangefinder:   return "RANGEFINDER";
        case Ability::Homing:        return "SEEKER GUIDANCE";
        case Ability::PointDefense:  return "POINT DEFENSE";
        case Ability::AutoDodge:     return "REFLEX DRIVE";
        default:                     return "NONE";
    }
}

const char* abilityBlurb(Ability a) {
    switch (a) {
        case Ability::Dash:          return "Burst along your heading. Breaks a lock, crosses open ground.";
        case Ability::Brace:         return "Plant and lock the hull. No movement, far steadier guns.";
        case Ability::Overdrive:     return "Surge the reactor: faster, cooler, then a heat debt.";
        case Ability::VentHeat:      return "Dump the heat sink at once. Clears an overheat.";
        case Ability::Bulwark:       return "Angle the plating. Heavy damage reduction, briefly.";
        case Ability::TargetLock:    return "Hard lock: tighter convergence and a lead marker.";
        case Ability::Siphon:
            return "Pour the reactor into the frame: structure back, heat owed.";
        case Ability::EmpSurge:
            return "Discharge. Drones fall out of the sky; guns nearby go quiet.";
        case Ability::Capacitor:
            return "Dump the bank: every cooldown gun recharges, racks refill.";
        case Ability::SilentRun:
            return "Run cold and dark. Hostiles lose you and reacquire slowly.";
        case Ability::ReactivePlate: return "The first heavy hit of each wave is mostly absorbed.";
        case Ability::Regenerator:   return "Repairs structure slowly while out of contact.";
        case Ability::Stabiliser:    return "Shrugs off impacts and rough ground.";
        case Ability::Ghost:         return "Hostiles take longer to acquire you.";
        case Ability::Rangefinder:   return "Weapons converge far better at long range.";
        case Ability::Homing:
            return "Capable missiles steer themselves onto the target.";
        case Ability::PointDefense:
            return "Automatically shoots down incoming rounds near the hull.";
        case Ability::AutoDodge:
            return "The legs sidestep incoming fire without being asked.";
        default:                     return "";
    }
}

bool abilityIsActive(Ability a) {
    switch (a) {
        case Ability::Siphon: case Ability::EmpSurge:
        case Ability::Capacitor: case Ability::SilentRun:
        case Ability::Dash: case Ability::Brace: case Ability::Overdrive:
        case Ability::VentHeat: case Ability::Bulwark: case Ability::TargetLock:
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------- the catalog

PartCatalog::PartCatalog() {
    auto add = [&](const PartDef& p) { parts_.push_back(p); };

    // =================================================================== CHASSIS
    {
        PartDef p;
        p.slot = Slot::Chassis;
        p.id = "ch_wasp"; p.name = "WASP-C LIGHT FRAME"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "Minimal hull. Two light mounts, almost no armour, cheap to run.";
        p.price = 0; p.tier = 0; p.size = SizeClass::Light; p.style = 0;
        p.tint = Vec3(0.62f, 0.63f, 0.58f);
        p.stats.mass = 3.2f; p.stats.powerDraw = 0.35f;
        p.stats.structure = 95.0f; p.stats.armor = 0.5f;
        p.stats.trait = Trait::Nimble;
        p.hardpoints = {
            hp(-0.62f, 0.10f, 0.95f, MountKind::Chin, SizeClass::Light, true),
            hp( 0.62f, 0.10f, 0.95f, MountKind::Chin, SizeClass::Light, true),
        };
        p.family = HullFamily::Turreted;
        p.stats.frontDamageMult = 0.80f; p.stats.rearDamageMult = 1.35f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Chassis;
        p.id = "ch_drone"; p.name = "DRONE-3 UTILITY FRAME"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "A third light mount welded onto a wider frame. Heavier and "
                  "thirstier than the Wasp, and no better protected.";
        p.price = 700; p.tier = 0; p.size = SizeClass::Light; p.style = 0;
        p.tint = Vec3(0.58f, 0.60f, 0.56f);
        // A sidegrade, not a downgrade: an extra gun for weight and power, with
        // the same hull behind it. Priced above the free frame, so a part that
        // left you strictly weaker would be a trap on anyone who bought it.
        p.stats.mass = 4.1f; p.stats.powerDraw = 0.62f;
        p.stats.structure = 108.0f; p.stats.armor = 0.5f;
        p.stats.trait = Trait::Scavenger;
        p.hardpoints = {
            hp(-0.66f, 0.08f, 0.98f, MountKind::Chin, SizeClass::Light, true),
            hp( 0.66f, 0.08f, 0.98f, MountKind::Chin, SizeClass::Light, true),
            hp( 0.00f, 0.92f, 0.02f, MountKind::Dorsal, SizeClass::Light, true),
        };
        p.family = HullFamily::Turreted;
        p.stats.frontDamageMult = 0.80f; p.stats.rearDamageMult = 1.35f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Chassis;
        p.id = "ch_mule"; p.name = "MULE-7 LINE HULL"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "The workhorse. Two light chins plus a medium dorsal mount.";
        p.price = 1400; p.tier = 1; p.size = SizeClass::Medium; p.style = 1;
        p.tint = Vec3(0.60f, 0.60f, 0.52f);
        p.stats.mass = 5.4f; p.stats.powerDraw = 0.55f;
        p.stats.structure = 165.0f; p.stats.armor = 1.5f;
        p.stats.trait = Trait::Bulk;
        p.hardpoints = {
            hp(-0.72f, 0.08f, 1.05f, MountKind::Chin, SizeClass::Light, true),
            hp( 0.72f, 0.08f, 1.05f, MountKind::Chin, SizeClass::Light, true),
            hp( 0.00f, 0.92f, 0.02f, MountKind::Dorsal, SizeClass::Medium, true),
        };
        p.family = HullFamily::Turreted;
        p.stats.frontDamageMult = 0.70f; p.stats.rearDamageMult = 1.40f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Chassis;
        p.id = "ch_shrike"; p.name = "SHRIKE R-4 RAIDER"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Light but well armed. Three mounts on a frame built for speed.";
        p.price = 2600; p.tier = 2; p.size = SizeClass::Medium; p.style = 2;
        p.tint = Vec3(0.52f, 0.56f, 0.58f);
        p.stats.mass = 4.6f; p.stats.powerDraw = 0.70f;
        p.stats.structure = 145.0f; p.stats.armor = 1.0f;
        p.stats.trait = Trait::Sprinter;
        p.hardpoints = {
            hp(-0.78f, 0.42f, 0.42f, MountKind::Shoulder, SizeClass::Medium, true),
            hp( 0.78f, 0.42f, 0.42f, MountKind::Shoulder, SizeClass::Medium, true),
            hp( 0.00f, 0.02f, 1.15f, MountKind::Chin, SizeClass::Light, true),
        };
        p.family = HullFamily::Turreted;
        p.stats.frontDamageMult = 0.74f; p.stats.rearDamageMult = 1.45f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Chassis;
        p.id = "ch_revenant"; p.name = "REVENANT AH-2"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Assault hull. Four mounts, thick shoulders, drinks power.";
        p.price = 5200; p.tier = 2; p.size = SizeClass::Heavy; p.style = 3;
        p.tint = Vec3(0.55f, 0.55f, 0.48f);
        p.stats.mass = 8.6f; p.stats.powerDraw = 0.95f;
        p.stats.structure = 250.0f; p.stats.armor = 2.5f;
        p.stats.trait = Trait::Gunnery;
        p.hardpoints = {
            hp(-0.92f, 0.46f, 0.30f, MountKind::Shoulder, SizeClass::Medium, true),
            hp( 0.92f, 0.46f, 0.30f, MountKind::Shoulder, SizeClass::Medium, true),
            hp(-0.62f, 0.02f, 1.10f, MountKind::Chin, SizeClass::Light, true),
            hp( 0.62f, 0.02f, 1.10f, MountKind::Chin, SizeClass::Light, true),
        };
        p.family = HullFamily::Turreted;
        p.stats.frontDamageMult = 0.60f; p.stats.rearDamageMult = 1.50f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Chassis;
        p.id = "ch_bastion"; p.name = "BASTION S-9 SIEGE DECK"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "A mobile gun platform. Five mounts including a heavy dorsal.";
        p.price = 11000; p.tier = 3; p.size = SizeClass::Heavy; p.style = 4;
        p.tint = Vec3(0.50f, 0.50f, 0.45f);
        p.stats.mass = 13.5f; p.stats.powerDraw = 1.35f;
        p.stats.structure = 365.0f; p.stats.armor = 4.0f;
        p.stats.trait = Trait::Platform;
        p.hardpoints = {
            hp( 0.00f, 1.06f, -0.18f, MountKind::Dorsal, SizeClass::Heavy, true),
            hp(-1.02f, 0.40f, 0.34f, MountKind::Shoulder, SizeClass::Medium, true),
            hp( 1.02f, 0.40f, 0.34f, MountKind::Shoulder, SizeClass::Medium, true),
            hp(-0.66f, 0.00f, 1.18f, MountKind::Chin, SizeClass::Light, true),
            hp( 0.66f, 0.00f, 1.18f, MountKind::Chin, SizeClass::Light, true),
        };
        // The siege deck is the first artillery family hull: its guns only speak
        // from a planted stance, and in exchange it carries the heaviest rack.
        p.family = HullFamily::Artillery;
        p.stats.frontDamageMult = 0.68f; p.stats.rearDamageMult = 1.55f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Chassis;
        p.id = "ch_ferrum"; p.name = "FERRUM CASEMATE"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "No turret at all: the guns live in an armoured bow and you "
                  "aim with your legs. The best frontal plate money buys at "
                  "this weight - and a back you must never show.";
        p.price = 3400; p.tier = 1; p.size = SizeClass::Medium; p.style = 5;
        p.family = HullFamily::Casemate;
        p.tint = Vec3(0.52f, 0.54f, 0.52f);
        p.stats.mass = 7.4f; p.stats.powerDraw = 0.70f;
        p.stats.structure = 240.0f; p.stats.armor = 3.0f;
        p.stats.trait = Trait::Frontal;
        p.stats.frontDamageMult = 0.45f; p.stats.rearDamageMult = 1.75f;
        p.hardpoints = {
            hp( 0.00f, 0.30f, 1.05f, MountKind::Chin, SizeClass::Medium, false),
            hp(-0.70f, 0.10f, 1.00f, MountKind::Chin, SizeClass::Light, false),
            hp( 0.70f, 0.10f, 1.00f, MountKind::Chin, SizeClass::Light, false),
        };
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Chassis;
        p.id = "ch_judicator"; p.name = "JUDICATOR ASSAULT GUN"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "The casemate idea taken as far as it goes: a heavy gun in a "
                  "fixed mantlet behind glacis nothing light gets through. "
                  "Point it at the problem.";
        p.price = 9200; p.tier = 3; p.size = SizeClass::Heavy; p.style = 5;
        p.family = HullFamily::Casemate;
        p.tint = Vec3(0.48f, 0.50f, 0.48f);
        p.stats.mass = 12.0f; p.stats.powerDraw = 1.10f;
        p.stats.structure = 380.0f; p.stats.armor = 5.0f;
        p.stats.trait = Trait::Breacher;
        p.stats.frontDamageMult = 0.40f; p.stats.rearDamageMult = 1.80f;
        p.hardpoints = {
            hp( 0.00f, 0.34f, 1.10f, MountKind::Chin, SizeClass::Heavy, false),
            hp(-0.80f, 0.12f, 0.95f, MountKind::Chin, SizeClass::Light, false),
            hp( 0.80f, 0.12f, 0.95f, MountKind::Chin, SizeClass::Light, false),
        };
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Chassis;
        p.id = "ch_viper"; p.name = "VIPER LOW-PROFILE"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "A hull you can hide. Rides low, hugs cover a line tank "
                  "stands over, and gives gunners a third less to hit - out of "
                  "plating that stops a third less.";
        p.price = 2900; p.tier = 1; p.size = SizeClass::Light; p.style = 6;
        p.family = HullFamily::LowProfile;
        p.tint = Vec3(0.50f, 0.55f, 0.50f);
        p.stats.mass = 3.8f; p.stats.powerDraw = 0.50f;
        p.stats.structure = 130.0f; p.stats.armor = 1.0f;
        p.stats.trait = Trait::LowProfile;
        p.stats.frontDamageMult = 0.75f; p.stats.rearDamageMult = 1.30f;
        p.hardpoints = {
            hp(-0.70f, 0.06f, 0.90f, MountKind::Chin, SizeClass::Light, true),
            hp( 0.70f, 0.06f, 0.90f, MountKind::Chin, SizeClass::Light, true),
            hp( 0.00f, 0.55f, -0.10f, MountKind::Dorsal, SizeClass::Medium, true),
        };
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Chassis;
        p.id = "ch_longbow"; p.name = "LONGBOW FIRE PLATFORM"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "An open cradle built around one enormous mount. Devastating "
                  "planted, helpless on the move: the guns will not speak "
                  "until the legs are still.";
        p.price = 7600; p.tier = 2; p.size = SizeClass::Heavy; p.style = 7;
        p.family = HullFamily::Artillery;
        p.tint = Vec3(0.52f, 0.52f, 0.47f);
        p.stats.mass = 10.0f; p.stats.powerDraw = 1.00f;
        p.stats.structure = 250.0f; p.stats.armor = 2.5f;
        p.stats.trait = Trait::Spotter;
        p.stats.frontDamageMult = 0.75f; p.stats.rearDamageMult = 1.50f;
        p.hardpoints = {
            hp( 0.00f, 1.10f, -0.15f, MountKind::Dorsal, SizeClass::Heavy, true),
            hp( 0.00f, 0.85f,  0.45f, MountKind::Dorsal, SizeClass::Medium, true),
            hp(-0.60f, 0.05f, 1.05f, MountKind::Chin, SizeClass::Light, true),
        };
        add(p);
    }


    // ====================================================================== LEGS
    {
        PartDef p;
        p.slot = Slot::Legs;
        p.id = "lg_strider"; p.name = "STRIDER MK1 LIMBS"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "Standard six-limb walking gear. Unremarkable and reliable.";
        p.price = 0; p.tier = 0; p.style = 0;
        p.tint = Vec3(0.58f, 0.59f, 0.55f);
        p.stats.mass = 3.8f; p.stats.powerDraw = 0.85f;
        p.stats.structure = 70.0f; p.stats.armor = 0.5f;
        p.stats.mobility = 6.85f; p.stats.agility = 1.00f; p.stats.jumpPower = 22.20f;
        p.stats.reach = 4.95f; p.stats.stance = 2.35f;
        p.stats.climbGrip = 0.42f; p.stats.climbSpeed = 0.34f;
                p.stats.ability = Ability::Stabiliser; p.stats.abilityPower = 0.35f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Legs;
        p.id = "lg_scout"; p.name = "SCOUT PATTERN LIMBS"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "Lighter and quicker than the Mk1, and grips a little better. "
                  "Will not carry a heavy hull.";
        p.price = 650; p.tier = 0; p.style = 1;
        p.tint = Vec3(0.58f, 0.60f, 0.60f);
        p.stats.mass = 2.4f; p.stats.powerDraw = 0.34f; p.stats.structure = 40.0f;
        p.stats.mobility = 7.85f; p.stats.agility = 1.16f; p.stats.jumpPower = 24.60f;
        p.stats.reach = 5.05f; p.stats.stance = 2.42f;
        p.stats.climbGrip = 0.46f; p.stats.climbSpeed = 0.42f;
                p.stats.ability = Ability::Dash; p.stats.abilityPower = 20.0f;
        p.stats.abilityCooldown = 6.0f; p.stats.abilityDuration = 0.35f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Legs;
        p.id = "lg_harrier"; p.name = "HARRIER SPRINT LIMBS"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Long, light and fast. Barely armoured - do not get hit.";
        p.price = 1900; p.tier = 1; p.style = 1;
        p.tint = Vec3(0.54f, 0.58f, 0.60f);
        p.stats.mass = 3.1f; p.stats.powerDraw = 1.25f;
        p.stats.structure = 55.0f; p.stats.armor = 0.0f;
        p.stats.mobility = 9.10f; p.stats.agility = 1.30f; p.stats.jumpPower = 27.75f;
        p.stats.reach = 5.45f; p.stats.stance = 2.62f;
        p.stats.climbGrip = 0.58f; p.stats.climbSpeed = 0.58f;
                p.stats.ability = Ability::Dash; p.stats.abilityPower = 30.0f;
        p.stats.abilityCooldown = 4.5f; p.stats.abilityDuration = 0.40f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Legs;
        p.id = "lg_anvil"; p.name = "ANVIL LOAD LIMBS"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Squat, heavily shielded actuators. Slow, and very hard to break.";
        p.price = 2400; p.tier = 1; p.style = 2;
        p.tint = Vec3(0.52f, 0.51f, 0.46f);
        p.stats.mass = 6.9f; p.stats.powerDraw = 1.05f;
        p.stats.structure = 170.0f; p.stats.armor = 2.0f;
        p.stats.mobility = 6.10f; p.stats.agility = 0.80f; p.stats.jumpPower = 16.66f;
        p.stats.reach = 5.25f; p.stats.stance = 2.50f;
        p.stats.climbGrip = 0.08f; p.stats.climbSpeed = 0.22f;
                p.stats.ability = Ability::Brace; p.stats.abilityPower = 0.65f;
        p.stats.abilityCooldown = 5.0f; p.stats.abilityDuration = 4.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Legs;
        p.id = "lg_grasshopper"; p.name = "GRASSHOPPER BALLISTIC LIMBS"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Spring-loaded knees storing enormous launch energy.";
        p.price = 4300; p.tier = 2; p.style = 3;
        p.tint = Vec3(0.56f, 0.60f, 0.55f);
        p.stats.mass = 4.6f; p.stats.powerDraw = 1.45f;
        p.stats.structure = 95.0f; p.stats.armor = 1.0f;
        p.stats.mobility = 7.60f; p.stats.agility = 1.15f; p.stats.jumpPower = 42.83f;
        p.stats.reach = 4.55f; p.stats.stance = 2.05f;
        p.stats.climbGrip = 0.50f; p.stats.climbSpeed = 0.50f;
                p.stats.ability = Ability::Dash; p.stats.abilityPower = 26.0f;
        p.stats.abilityCooldown = 5.5f; p.stats.abilityDuration = 0.38f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Legs;
        p.id = "lg_titan"; p.name = "TITAN ASSAULT LIMBS"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Everything at once, if you can supply the power.";
        p.price = 8800; p.tier = 3; p.style = 4;
        p.tint = Vec3(0.50f, 0.52f, 0.50f);
        p.stats.mass = 7.4f; p.stats.powerDraw = 1.95f;
        p.stats.structure = 190.0f; p.stats.armor = 2.5f;
        p.stats.mobility = 7.60f; p.stats.agility = 1.05f; p.stats.jumpPower = 24.60f;
        p.stats.reach = 5.30f; p.stats.stance = 2.55f;
        p.stats.climbGrip = 0.14f; p.stats.climbSpeed = 0.26f;
                p.stats.ability = Ability::Brace; p.stats.abilityPower = 0.80f;
        p.stats.abilityCooldown = 4.0f; p.stats.abilityDuration = 5.0f;
        add(p);
    }

    // ==================================================================== ENGINE
    {
        PartDef p;
        p.slot = Slot::Engine;
        p.id = "en_civic"; p.name = "CIVIC 2.0 CELL"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "A converted utility cell. It will do, for now.";
        p.price = 0; p.tier = 0; p.style = 0;
        p.tint = Vec3(0.42f, 0.46f, 0.48f);
        p.stats.mass = 1.3f; p.stats.powerOutput = 2.6f;
        p.stats.structure = 20.0f; p.stats.coolRate = 0.55f; p.stats.heatCapacity = 1.15f;
                p.stats.ability = Ability::VentHeat; p.stats.abilityPower = 1.0f;
        p.stats.abilityCooldown = 14.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Engine;
        p.id = "en_surplus"; p.name = "SURPLUS 2.6 CELL"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "A reconditioned civic core. A little more power, a lot more "
                  "cooling, and it was cheap.";
        p.price = 550; p.tier = 0; p.style = 0;
        p.tint = Vec3(0.50f, 0.52f, 0.48f);
        p.stats.mass = 1.5f; p.stats.powerOutput = 3.4f;
        p.stats.structure = 26.0f; p.stats.coolRate = 0.78f; p.stats.heatCapacity = 1.30f;
        // The cheap surge, so the first upgrade still teaches the button.
        p.stats.ability = Ability::Overdrive; p.stats.abilityPower = 1.30f;
        p.stats.abilityCooldown = 10.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Engine;
        p.id = "en_milspec"; p.name = "MILSPEC 4.0 REACTOR"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Military standard. Comfortable margin, good heat handling.";
        p.price = 2200; p.tier = 1; p.style = 1;
        p.tint = Vec3(0.40f, 0.50f, 0.52f);
        p.stats.mass = 2.4f; p.stats.powerOutput = 4.8f;
        p.stats.structure = 35.0f; p.stats.coolRate = 0.42f; p.stats.heatCapacity = 1.25f;
        // The alpha-strike reactor: everything on cooldown comes back at once.
        p.stats.ability = Ability::Capacitor; p.stats.abilityPower = 1.0f;
        p.stats.abilityCooldown = 22.0f; p.stats.abilityDuration = 0.4f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Engine;
        p.id = "en_halcyon"; p.name = "HALCYON FUSION CORE"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Runs cold and quiet. Expensive, and worth it.";
        p.price = 6400; p.tier = 2; p.style = 2;
        p.tint = Vec3(0.36f, 0.56f, 0.60f);
        p.stats.mass = 3.6f; p.stats.powerOutput = 7.4f;
        p.stats.structure = 45.0f; p.stats.coolRate = 0.62f; p.stats.heatCapacity = 1.6f;
        // Fusion output poured back into the frame: the survivability reactor.
        p.stats.ability = Ability::Siphon; p.stats.abilityPower = 0.16f;
        p.stats.abilityCooldown = 26.0f; p.stats.abilityDuration = 0.4f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Engine;
        p.id = "en_pyre"; p.name = "PYRE OVERDRIVE CORE"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Enormous output. Runs hot enough that sustained fire is a risk.";
        p.price = 13500; p.tier = 3; p.style = 3;
        p.tint = Vec3(0.62f, 0.44f, 0.30f);
        p.stats.mass = 5.2f; p.stats.powerOutput = 11.0f;
        p.stats.structure = 55.0f; p.stats.coolRate = 0.34f; p.stats.heatCapacity = 1.15f;
        // Eleven megawatts with nowhere to go: dump it into the air instead.
        p.stats.ability = Ability::EmpSurge; p.stats.abilityPower = 1.0f;
        p.stats.abilityCooldown = 20.0f; p.stats.abilityDuration = 0.4f;
        p.stats.abilityCooldown = 8.0f; p.stats.abilityDuration = 6.0f;
        add(p);
    }

    // ==================================================================== ARMOUR
    {
        PartDef p;
        p.slot = Slot::Armor;
        p.id = "ar_none"; p.name = "NO PLATING"; p.maker = "-";
        p.blurb = "Bare frame. Every kilogram saved is a kilogram of speed.";
        p.price = 0; p.tier = 0; p.style = 0;
        p.tint = Vec3(0.55f, 0.55f, 0.55f);
        p.stats.mass = 0.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Armor;
        p.id = "ar_spall"; p.name = "SPALL LINER"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "A thin anti-fragmentation skin. Barely armour, but it costs "
                  "almost nothing and weighs almost nothing.";
        p.price = 380; p.tier = 0; p.style = 1;
        p.tint = Vec3(0.48f, 0.48f, 0.46f);
        p.stats.mass = 0.9f; p.stats.structure = 34.0f; p.stats.armor = 1.1f;
                p.stats.ability = Ability::Regenerator; p.stats.abilityPower = 1.8f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Armor;
        p.id = "ar_weave"; p.name = "COMPOSITE WEAVE"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "Thin bonded panels. Cheap insurance against light autocannon.";
        p.price = 900; p.tier = 0; p.style = 1;
        p.tint = Vec3(0.58f, 0.57f, 0.50f);
        p.stats.mass = 1.7f; p.stats.structure = 70.0f; p.stats.armor = 2.0f;
                p.stats.ability = Ability::Regenerator; p.stats.abilityPower = 3.2f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Armor;
        p.id = "ar_ceramic"; p.name = "LAYERED CERAMIC"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Proper armour. Noticeably heavier, noticeably harder to kill.";
        p.price = 3100; p.tier = 1; p.style = 2;
        p.tint = Vec3(0.60f, 0.58f, 0.48f);
        p.stats.mass = 3.6f; p.stats.structure = 155.0f; p.stats.armor = 5.0f;
                p.stats.ability = Ability::ReactivePlate; p.stats.abilityPower = 0.55f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Armor;
        p.id = "ar_reactive"; p.name = "REACTIVE SLAB ARRAY"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Bolt-on reactive blocks. Shrugs off splash almost entirely.";
        p.price = 7200; p.tier = 2; p.style = 3;
        p.tint = Vec3(0.54f, 0.53f, 0.46f);
        p.stats.mass = 6.4f; p.stats.structure = 265.0f; p.stats.armor = 9.0f;
                p.stats.ability = Ability::ReactivePlate; p.stats.abilityPower = 0.75f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Armor;
        p.id = "ar_ablative"; p.name = "ABLATIVE HEX PLATING"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "The heaviest plating that will still let a mech walk.";
        p.price = 14000; p.tier = 3; p.style = 4;
        p.tint = Vec3(0.52f, 0.54f, 0.52f);
        p.stats.mass = 8.8f; p.stats.structure = 360.0f; p.stats.armor = 13.0f;
                p.stats.ability = Ability::Bulwark; p.stats.abilityPower = 0.72f;
        p.stats.abilityCooldown = 16.0f; p.stats.abilityDuration = 4.0f;
        add(p);
    }

    // ==================================================================== SENSOR
    {
        PartDef p;
        p.slot = Slot::Sensor;
        p.id = "se_basic"; p.name = "MK1 OPTICAL POD"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "Cameras and a rangefinder. Short ranged but weighs nothing.";
        p.price = 0; p.tier = 0; p.style = 0;
        p.tint = Vec3(0.45f, 0.62f, 0.55f);
        p.stats.mass = 0.3f; p.stats.powerDraw = 0.10f; p.stats.sensorRange = 140.0f;
                p.stats.ability = Ability::Ghost; p.stats.abilityPower = 0.15f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Sensor;
        p.id = "se_wide"; p.name = "WIDE-ANGLE POD"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "More glass than the Mk1 and a longer baseline. Sees further, "
                  "still cheap.";
        p.price = 500; p.tier = 0; p.style = 0;
        p.tint = Vec3(0.44f, 0.65f, 0.57f);
        p.stats.mass = 0.4f; p.stats.powerDraw = 0.15f; p.stats.sensorRange = 185.0f;
                p.stats.ability = Ability::Rangefinder; p.stats.abilityPower = 0.35f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Sensor;
        p.id = "se_track"; p.name = "TRACKER ARRAY"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Marks hostiles on the HUD and estimates their heading.";
        p.price = 1600; p.tier = 1; p.style = 1;
        p.tint = Vec3(0.40f, 0.70f, 0.60f);
        p.stats.mass = 0.6f; p.stats.powerDraw = 0.25f; p.stats.sensorRange = 230.0f;
                p.stats.ability = Ability::TargetLock; p.stats.abilityPower = 0.55f;
        p.stats.abilityCooldown = 9.0f; p.stats.abilityDuration = 5.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Sensor;
        p.id = "se_lidar"; p.name = "LIDAR MAST"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Long-range scanning with a computed lead marker on your target.";
        p.price = 4800; p.tier = 2; p.style = 2;
        p.tint = Vec3(0.38f, 0.78f, 0.70f);
        p.stats.mass = 1.1f; p.stats.powerDraw = 0.45f; p.stats.sensorRange = 330.0f;
                p.stats.ability = Ability::TargetLock; p.stats.abilityPower = 0.85f;
        p.stats.abilityCooldown = 7.0f; p.stats.abilityDuration = 7.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Sensor;
        p.id = "se_seeker"; p.name = "SEEKER UPLINK"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Terminal guidance for capable missiles: fire in the general "
                  "direction and the uplink walks them home. Does nothing for "
                  "guns - specialists specialise.";
        p.price = 5600; p.tier = 2; p.style = 2;
        p.tint = Vec3(0.42f, 0.72f, 0.78f);
        p.stats.mass = 0.9f; p.stats.powerDraw = 0.50f; p.stats.sensorRange = 260.0f;
        p.stats.ability = Ability::Homing; p.stats.abilityPower = 1.6f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Sensor;
        p.id = "se_aegis"; p.name = "AEGIS INTERCEPT ARRAY"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "A laser point-defense ring that swats incoming rounds out "
                  "of the air near the hull. It cannot stop a volley - but the "
                  "one shell that mattered, it usually can.";
        p.price = 6400; p.tier = 2; p.style = 2;
        p.tint = Vec3(0.75f, 0.62f, 0.42f);
        p.stats.mass = 1.3f; p.stats.powerDraw = 0.70f; p.stats.sensorRange = 240.0f;
        p.stats.ability = Ability::PointDefense; p.stats.abilityPower = 1.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Sensor;
        p.id = "se_reflex"; p.name = "REFLEX AUTODRIVE"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Predictive fire-solution tracking wired straight into the "
                  "legs: when something big is about to connect, the machine "
                  "steps out of the way before you have seen it.";
        p.price = 8800; p.tier = 3; p.style = 2;
        p.tint = Vec3(0.55f, 0.75f, 0.45f);
        p.stats.mass = 1.0f; p.stats.powerDraw = 0.60f; p.stats.sensorRange = 280.0f;
        p.stats.ability = Ability::AutoDodge; p.stats.abilityPower = 7.5f;
        add(p);
    }


    // ------------------------------------------------ additional LEG patterns
    // Each of these exists to make one PLAYSTYLE possible, not to be a
    // slightly better version of the set above it.
    {
        PartDef p;
        p.slot = Slot::Legs;
        p.id = "lg_gecko"; p.name = "GECKO CLIMBING LIMBS"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Adhesive claw arrays built for vertical work. Slow across "
                  "open ground and lightly built - but there is no wall in the "
                  "field this cannot simply walk up.";
        p.price = 3400; p.tier = 1; p.style = 1;
        p.tint = Vec3(0.48f, 0.62f, 0.52f);
        p.stats.mass = 4.1f; p.stats.powerDraw = 1.05f;
        p.stats.structure = 74.0f; p.stats.armor = 0.6f;
        p.stats.mobility = 6.20f; p.stats.agility = 1.10f; p.stats.jumpPower = 20.0f;
        p.stats.reach = 5.20f; p.stats.stance = 2.30f;
        p.stats.climbGrip = 0.92f; p.stats.climbSpeed = 0.68f;
        p.stats.ability = Ability::Dash; p.stats.abilityPower = 0.9f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Legs;
        p.id = "lg_bastille"; p.name = "BASTILLE SIEGE LIMBS"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Hydraulic outriggers that plant like foundations. They will "
                  "never climb anything, and nothing that hits you will move "
                  "your gun off target either.";
        p.price = 5200; p.tier = 2; p.style = 2;
        p.tint = Vec3(0.55f, 0.53f, 0.48f);
        p.stats.mass = 8.6f; p.stats.powerDraw = 1.55f;
        p.stats.structure = 178.0f; p.stats.armor = 2.6f;
        p.stats.mobility = 5.60f; p.stats.agility = 0.72f; p.stats.jumpPower = 8.0f;
        p.stats.reach = 5.05f; p.stats.stance = 2.55f;
        p.stats.climbGrip = 0.10f; p.stats.climbSpeed = 0.20f;
        p.stats.ability = Ability::Brace; p.stats.abilityPower = 1.35f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Legs;
        p.id = "lg_mantis"; p.name = "MANTIS LEAPER LIMBS"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Spring-loaded launch limbs. Everything about them is the "
                  "jump: over the wall, onto the roof, off the roof onto "
                  "whatever was hiding behind it.";
        p.price = 6100; p.tier = 2; p.style = 3;
        p.tint = Vec3(0.60f, 0.66f, 0.44f);
        p.stats.mass = 4.6f; p.stats.powerDraw = 1.25f;
        p.stats.structure = 88.0f; p.stats.armor = 0.8f;
        p.stats.mobility = 7.90f; p.stats.agility = 1.28f; p.stats.jumpPower = 58.0f;
        p.stats.reach = 5.35f; p.stats.stance = 2.45f;
        p.stats.climbGrip = 0.52f; p.stats.climbSpeed = 0.52f;
        p.stats.ability = Ability::Dash; p.stats.abilityPower = 1.25f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Legs;
        p.id = "lg_courser"; p.name = "COURSER RACE LIMBS"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Competition gear pressed into war service. Fragile, "
                  "power-hungry, and faster across a contested kilometre than "
                  "anything else that walks.";
        p.price = 7400; p.tier = 3; p.style = 1;
        p.tint = Vec3(0.68f, 0.68f, 0.62f);
        p.stats.mass = 3.4f; p.stats.powerDraw = 1.60f;
        p.stats.structure = 62.0f; p.stats.armor = 0.3f;
        p.stats.mobility = 11.20f; p.stats.agility = 1.55f; p.stats.jumpPower = 30.0f;
        p.stats.reach = 5.10f; p.stats.stance = 2.30f;
        p.stats.climbGrip = 0.50f; p.stats.climbSpeed = 0.60f;
        p.stats.ability = Ability::Dash; p.stats.abilityPower = 1.6f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Legs;
        p.id = "lg_atlas"; p.name = "ATLAS PILLAR LIMBS"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "The heaviest walking gear in the catalogue, and the only "
                  "set rated to carry a siege deck at a useful pace. Rebuilt "
                  "actuators; it no longer walks like a wounded animal.";
        p.price = 9200; p.tier = 3; p.style = 4;
        p.tint = Vec3(0.52f, 0.52f, 0.50f);
        p.stats.mass = 10.2f; p.stats.powerDraw = 1.95f;
        p.stats.structure = 240.0f; p.stats.armor = 3.4f;
        p.stats.mobility = 7.20f; p.stats.agility = 0.90f; p.stats.jumpPower = 20.0f;
        p.stats.reach = 5.45f; p.stats.stance = 2.70f;
        p.stats.climbGrip = 0.22f; p.stats.climbSpeed = 0.26f;
        p.stats.ability = Ability::Stabiliser; p.stats.abilityPower = 0.85f;
        add(p);
    }

    // --------------------------------------------- additional SENSOR packages
    {
        PartDef p;
        p.slot = Slot::Sensor;
        p.id = "se_ghostveil"; p.name = "GHOSTVEIL EMITTER"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Broadband masking. Gunners take noticeably longer to settle "
                  "on you, which on open ground is worth more than armour.";
        p.price = 3200; p.tier = 1; p.style = 0;
        p.tint = Vec3(0.44f, 0.50f, 0.62f);
        p.stats.mass = 0.7f; p.stats.powerDraw = 0.42f; p.stats.sensorRange = 190.0f;
        p.stats.ability = Ability::Ghost; p.stats.abilityPower = 1.5f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Sensor;
        p.id = "se_ranger"; p.name = "LONGSHOT RANGEFINDER"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "A dedicated long-baseline optical rangefinder. Nothing "
                  "clever, no automation: it simply makes distant shots land.";
        p.price = 4100; p.tier = 1; p.style = 1;
        p.tint = Vec3(0.70f, 0.66f, 0.48f);
        p.stats.mass = 1.1f; p.stats.powerDraw = 0.38f; p.stats.sensorRange = 320.0f;
        p.stats.ability = Ability::Rangefinder; p.stats.abilityPower = 1.7f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Sensor;
        p.id = "se_warden"; p.name = "WARDEN LOCK SUITE"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Hard target lock with convergence override. Expensive, "
                  "heavy, and it does not miss the thing it decided to kill.";
        p.price = 7200; p.tier = 3; p.style = 2;
        p.tint = Vec3(0.78f, 0.48f, 0.42f);
        p.stats.mass = 1.5f; p.stats.powerDraw = 0.80f; p.stats.sensorRange = 300.0f;
        p.stats.ability = Ability::TargetLock; p.stats.abilityPower = 1.5f;
        add(p);
    }

    // --------------------------------------------- additional ENGINE / ARMOUR
    {
        PartDef p;
        p.slot = Slot::Engine;
        p.id = "en_thermal"; p.name = "THERMAL SINK PLANT"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "Modest output wrapped in an enormous heat sink. Built for "
                  "builds that would rather never stop shooting than shoot hard.";
        p.price = 3600; p.tier = 1; p.style = 2;
        p.tint = Vec3(0.48f, 0.58f, 0.62f);
        p.stats.mass = 3.4f; p.stats.powerOutput = 5.0f;
        p.stats.heatCapacity = 2.2f; p.stats.coolRate = 2.1f;
        // A plant that already runs cold can run COLD: the stealth reactor.
        p.stats.ability = Ability::SilentRun; p.stats.abilityPower = 1.0f;
        p.stats.abilityCooldown = 24.0f; p.stats.abilityDuration = 7.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Armor;
        p.id = "ar_regen"; p.name = "SELF-SEALING LAMINATE"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Thin plating threaded with repair mesh. It stops less than "
                  "a slab array does and it keeps giving structure back "
                  "between fights, which over a long contract is more.";
        p.price = 5400; p.tier = 2; p.style = 1;
        p.tint = Vec3(0.50f, 0.62f, 0.56f);
        p.stats.mass = 4.2f; p.stats.armor = 3.2f; p.stats.structure = 96.0f;
        p.stats.ability = Ability::Regenerator; p.stats.abilityPower = 1.6f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Armor;
        p.id = "ar_bulwark"; p.name = "BULWARK SHIELD PLATE"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Heavy frontal slabs on powered mounts that slam shut on "
                  "command. Enormously heavy; for the moments you decide to "
                  "walk into the fire instead of around it.";
        p.price = 8600; p.tier = 3; p.style = 3;
        p.tint = Vec3(0.60f, 0.56f, 0.50f);
        p.stats.mass = 9.4f; p.stats.armor = 8.6f; p.stats.structure = 168.0f;
        p.stats.ability = Ability::Bulwark; p.stats.abilityPower = 1.5f;
        add(p);
    }

    // =================================================================== WEAPONS
    // -- light: unlimited ammo, heat is the only limit -------------------------
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Light;
        p.id = "wp_pd9"; p.name = "PD-9 AUTOCANNON"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "Standard issue. Endless ammunition, modest punch.";
        p.price = 0; p.tier = 0; p.style = 0;
        p.tint = Vec3(0.42f, 0.44f, 0.46f);
        p.stats.mass = 0.9f; p.stats.powerDraw = 0.20f; p.stats.structure = 10.0f;
        p.weapon.damage = 14.35f; p.weapon.projectileSpeed = 155.0f;
        p.weapon.fireInterval = 0.222f; p.weapon.heatPerShot = 0.060f;
        p.weapon.ammo = AmmoKind::Unlimited; p.weapon.range = 341.0f;
        // Mild walk under sustained fire: enough that tapping is better.
        p.weapon.bloomPerShot = 0.055f; p.weapon.bloomMax = 0.55f;
        p.weapon.bloomRecover = 0.12f;
        p.weapon.tracerColor = Vec3(0.55f, 1.0f, 0.62f);
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Light;
        p.id = "wp_carbine"; p.name = "VK-4 CARBINE"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "Flatter and faster than the PD-9, and it barely warms up. "
                  "Endless ammunition.";
        p.price = 450; p.tier = 0; p.style = 0;
        p.tint = Vec3(0.46f, 0.47f, 0.49f);
        p.stats.mass = 1.0f; p.stats.powerDraw = 0.24f; p.stats.structure = 10.0f;
        p.weapon.damage = 12.71f; p.weapon.projectileSpeed = 240.0f;
        p.weapon.fireInterval = 0.185f; p.weapon.heatPerShot = 0.041f;
        p.weapon.spread = 0.004f;
        p.weapon.ammo = AmmoKind::Unlimited; p.weapon.range = 403.0f;
        p.weapon.bloomPerShot = 0.040f; p.weapon.bloomMax = 0.45f;
        p.weapon.bloomRecover = 0.10f;
        p.weapon.tracerColor = Vec3(0.62f, 1.0f, 0.70f);
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Light;
        p.id = "wp_stiletto"; p.name = "STILETTO BURST RIFLE"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Three-round bursts of very fast, very flat shot. Rewards a "
                  "steady hull more than a quick trigger.";
        p.price = 1900; p.tier = 1; p.size = SizeClass::Light; p.style = 2;
        p.tint = Vec3(0.44f, 0.48f, 0.52f);
        p.stats.mass = 1.2f; p.stats.powerDraw = 0.32f; p.stats.structure = 11.0f;
        p.weapon.damage = 11.0f; p.weapon.projectileSpeed = 420.0f;
        p.weapon.fireInterval = 0.42f; p.weapon.heatPerShot = 0.075f;
        p.weapon.pellets = 3; p.weapon.spread = 0.010f;
        p.weapon.ammo = AmmoKind::Unlimited; p.weapon.range = 520.0f;
        // Three-round bursts with a real gap between them: a rifle, not
        // a hose, and it rewards laying the first round properly.
        p.weapon.burstCount = 3; p.weapon.burstGap = 0.78f;
        p.weapon.bloomPerShot = 0.06f; p.weapon.bloomMax = 0.40f;
        // A burst weapon should shed most of its bloom in the gap: that is
        // what "disciplined" means mechanically.
        p.weapon.bloomRecover = 0.35f;
        p.weapon.tracerColor = Vec3(0.72f, 0.94f, 1.0f);
        p.weapon.tracerLength = 4.2f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Medium;
        p.id = "wp_arclight"; p.name = "ARCLIGHT COIL"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "A sustained particle arc. Short ranged and enormously hot, "
                  "but it does not miss and it does not need leading.";
        p.price = 5800; p.tier = 2; p.style = 4;
        p.tint = Vec3(0.40f, 0.56f, 0.62f);
        p.stats.mass = 2.6f; p.stats.powerDraw = 0.95f; p.stats.structure = 18.0f;
        p.weapon.damage = 9.0f; p.weapon.projectileSpeed = 900.0f;
        p.weapon.fireInterval = 0.055f; p.weapon.heatPerShot = 0.085f;
        p.weapon.spread = 0.002f;
        p.weapon.ammo = AmmoKind::Unlimited; p.weapon.range = 180.0f;
        // A coil that has to charge before it will speak, and hoses
        // wide once it does. Devastating in a held burst, useless in taps.
        p.weapon.spinUp = 1.15f;
        p.weapon.bloomPerShot = 0.045f; p.weapon.bloomMax = 1.10f;
        p.weapon.bloomRecover = 0.30f;
        p.weapon.tracerColor = Vec3(0.62f, 0.92f, 1.0f);
        p.weapon.tracerLength = 6.0f; p.weapon.tracerRadius = 0.11f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Medium;
        p.id = "wp_cinder"; p.name = "CINDER INCENDIARY"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Lobs a burning cluster. Poor against a single target, "
                  "brutal against anything that has bunched up.";
        p.price = 4400; p.tier = 2; p.style = 6;
        p.tint = Vec3(0.56f, 0.42f, 0.30f);
        p.stats.mass = 2.4f; p.stats.powerDraw = 0.55f; p.stats.structure = 16.0f;
        p.weapon.damage = 16.0f; p.weapon.projectileSpeed = 74.0f;
        p.weapon.fireInterval = 0.10f; p.weapon.heatPerShot = 0.30f;
        p.weapon.gravity = -14.0f;
        p.weapon.blastRadius = 8.5f; p.weapon.blastDamage = 34.0f;
        p.weapon.ammo = AmmoKind::Cooldown; p.weapon.cooldownTime = 2.1f;
        p.weapon.range = 320.0f;
        p.weapon.tracerColor = Vec3(1.0f, 0.62f, 0.26f);
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Medium;
        p.id = "wp_static"; p.name = "STATIC LANCE"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "An EMP bolt. Modest damage, but it dumps enormous heat into "
                  "whatever it hits and shuts their guns down.";
        p.price = 6200; p.tier = 3; p.style = 3;
        p.tint = Vec3(0.46f, 0.50f, 0.66f);
        p.stats.mass = 2.2f; p.stats.powerDraw = 0.80f; p.stats.structure = 15.0f;
        p.weapon.damage = 22.0f; p.weapon.projectileSpeed = 260.0f;
        p.weapon.fireInterval = 0.10f; p.weapon.heatPerShot = 0.22f;
        p.weapon.blastRadius = 5.0f; p.weapon.blastDamage = 18.0f;
        p.weapon.ammo = AmmoKind::Cooldown; p.weapon.cooldownTime = 2.6f;
        p.weapon.range = 420.0f;
        p.weapon.tracerColor = Vec3(0.66f, 0.72f, 1.0f);
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Heavy;
        p.id = "wp_breaker"; p.name = "BREAKER RAILGUN"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "One enormous hypersonic slug at a time. Point it and it "
                  "arrives. Reloading is the whole cost.";
        p.price = 13800; p.tier = 3; p.style = 8;
        p.tint = Vec3(0.38f, 0.40f, 0.44f);
        p.stats.mass = 5.4f; p.stats.powerDraw = 1.35f; p.stats.structure = 30.0f;
        p.weapon.damage = 210.0f; p.weapon.projectileSpeed = 780.0f;
        p.weapon.fireInterval = 0.145f; p.weapon.heatPerShot = 0.42f;
        p.weapon.spread = 0.0015f;
        p.weapon.ammo = AmmoKind::Limited; p.weapon.magazine = 8;
        p.weapon.ammoPrice = 1600; p.weapon.range = 900.0f;
        p.weapon.tracerColor = Vec3(0.86f, 0.96f, 1.0f);
        p.weapon.tracerLength = 8.0f; p.weapon.tracerRadius = 0.13f;
        p.weapon.recoil = 1.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Light;
        p.id = "wp_twin14"; p.name = "TWIN-14 CHAINGUN"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "Higher rate of fire, runs hot. Endless ammunition.";
        p.price = 1100; p.tier = 1; p.style = 1;
        p.tint = Vec3(0.40f, 0.42f, 0.45f);
        p.stats.mass = 1.3f; p.stats.powerDraw = 0.35f; p.stats.structure = 12.0f;
        p.weapon.damage = 11.27f; p.weapon.projectileSpeed = 175.0f;
        p.weapon.fireInterval = 0.120f; p.weapon.heatPerShot = 0.054f;
        p.weapon.spread = 0.013f;
        p.weapon.ammo = AmmoKind::Unlimited; p.weapon.range = 310.0f;
        // A chaingun: spools, and walks off target if you lean on it.
        p.weapon.spinUp = 0.85f;
        p.weapon.bloomPerShot = 0.10f; p.weapon.bloomMax = 1.30f;
        p.weapon.bloomRecover = 0.35f;
        p.weapon.tracerColor = Vec3(0.70f, 1.0f, 0.55f);
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Light;
        p.id = "wp_needle"; p.name = "NEEDLE LIGHT GAUSS"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Very fast round, almost no lead required. Slow to cycle.";
        p.price = 2400; p.tier = 2; p.style = 2;
        p.tint = Vec3(0.46f, 0.52f, 0.58f);
        p.stats.mass = 1.5f; p.stats.powerDraw = 0.55f; p.stats.structure = 10.0f;
        p.weapon.damage = 30.75f; p.weapon.projectileSpeed = 330.0f;
        p.weapon.fireInterval = 0.703f; p.weapon.heatPerShot = 0.153f;
        p.weapon.spread = 0.0018f;
        p.weapon.ammo = AmmoKind::Unlimited; p.weapon.range = 465.0f;
        p.weapon.tracerLength = 4.5f; p.weapon.tracerRadius = 0.055f;
        p.weapon.tracerColor = Vec3(0.70f, 0.92f, 1.0f);
        add(p);
    }

    // -- medium: unlimited but gated by a recharge -----------------------------
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Medium;
        p.id = "wp_lance"; p.name = "LANCE RAILGUN"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Hypervelocity slug. Point and hit - no lead, no drop.";
        p.price = 5600; p.tier = 2; p.style = 3;
        p.tint = Vec3(0.44f, 0.50f, 0.58f);
        p.stats.mass = 2.8f; p.stats.powerDraw = 1.15f; p.stats.structure = 18.0f;
        p.weapon.damage = 106.60f; p.weapon.projectileSpeed = 420.0f;
        p.weapon.fireInterval = 0.185f; p.weapon.heatPerShot = 0.510f;
        p.weapon.spread = 0.0009f;
        p.weapon.ammo = AmmoKind::Cooldown; p.weapon.cooldownTime = 3.10f;
        p.weapon.range = 527.0f;
        p.weapon.tracerLength = 7.0f; p.weapon.tracerRadius = 0.07f;
        p.weapon.tracerColor = Vec3(0.80f, 0.95f, 1.0f);
        p.weapon.recoil = 0.8f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Medium;
        p.id = "wp_solaris"; p.name = "SOLARIS PLASMA THROWER"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "A slow ball of plasma. Lead your target generously.";
        p.price = 4900; p.tier = 2; p.style = 4;
        p.tint = Vec3(0.58f, 0.46f, 0.34f);
        p.stats.mass = 2.6f; p.stats.powerDraw = 0.95f; p.stats.structure = 16.0f;
        p.weapon.damage = 69.70f; p.weapon.projectileSpeed = 64.0f;
        p.weapon.fireInterval = 0.185f; p.weapon.heatPerShot = 0.442f;
        p.weapon.blastRadius = 4.0f; p.weapon.blastDamage = 34.20f;
        p.weapon.ammo = AmmoKind::Cooldown; p.weapon.cooldownTime = 2.03f;
        p.weapon.range = 310.0f;
        p.weapon.tracerLength = 1.4f; p.weapon.tracerRadius = 0.24f;
        p.weapon.tracerColor = Vec3(1.0f, 0.72f, 0.35f);
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Medium;
        p.id = "wp_scatter"; p.name = "SCATTER FLAK BATTERY"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "Eight fragments per burst. Devastating close, useless far.";
        p.price = 3400; p.tier = 1; p.style = 5;
        p.tint = Vec3(0.48f, 0.46f, 0.40f);
        p.stats.mass = 2.2f; p.stats.powerDraw = 0.60f; p.stats.structure = 15.0f;
        p.weapon.damage = 16.40f; p.weapon.projectileSpeed = 115.0f;
        p.weapon.fireInterval = 0.185f; p.weapon.heatPerShot = 0.340f;
        p.weapon.pellets = 8; p.weapon.spread = 0.055f;
        p.weapon.ammo = AmmoKind::Cooldown; p.weapon.cooldownTime = 1.49f;
        // A flak battery does not bloom - it is already a cone.
        p.weapon.bloomRecover = 4.0f;
        p.weapon.range = 170.0f;
        p.weapon.tracerColor = Vec3(1.0f, 0.88f, 0.55f);
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Medium;
        p.id = "wp_thumper"; p.name = "THUMPER GRENADE LAUNCHER"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Lobbed charges that arc over cover. Splash damage.";
        p.price = 3900; p.tier = 1; p.style = 6;
        p.tint = Vec3(0.50f, 0.48f, 0.38f);
        p.stats.mass = 2.4f; p.stats.powerDraw = 0.55f; p.stats.structure = 16.0f;
        p.weapon.damage = 61.50f; p.weapon.projectileSpeed = 74.0f;
        p.weapon.fireInterval = 0.185f; p.weapon.heatPerShot = 0.306f;
        p.weapon.gravity = -11.0f;
        p.weapon.blastRadius = 5.5f; p.weapon.blastDamage = 41.80f;
        p.weapon.ammo = AmmoKind::Cooldown; p.weapon.cooldownTime = 1.82f;
        p.weapon.range = 294.0f;
        p.weapon.tracerLength = 0.9f; p.weapon.tracerRadius = 0.16f;
        p.weapon.tracerColor = Vec3(1.0f, 0.80f, 0.40f);
        add(p);
    }

    // -- heavy: limited rounds, bought or scavenged -----------------------------
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Heavy;
        p.id = "wp_swarm"; p.name = "SWARM MISSILE RACK"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Slow missiles with a wide blast. Finite rounds. Accepts "
                  "seeker guidance.";
        p.price = 7400; p.tier = 2; p.style = 7;
        p.tint = Vec3(0.46f, 0.48f, 0.44f);
        p.stats.mass = 3.9f; p.stats.powerDraw = 0.70f; p.stats.structure = 22.0f;
        p.weapon.damage = 86.10f; p.weapon.projectileSpeed = 58.0f;
        p.weapon.fireInterval = 0.644f; p.weapon.heatPerShot = 0.204f;
        p.weapon.blastRadius = 6.5f; p.weapon.blastDamage = 49.40f;
        p.weapon.ammo = AmmoKind::Limited; p.weapon.magazine = 20; p.weapon.ammoPrice = 520;
        p.weapon.range = 356.0f;
        p.weapon.tracerLength = 1.6f; p.weapon.tracerRadius = 0.13f;
        p.weapon.tracerColor = Vec3(1.0f, 0.66f, 0.42f);
        p.weapon.homingCapable = true;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Heavy;
        p.id = "wp_hammerfall"; p.name = "HAMMERFALL CANNON"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "One enormous shell at a time. Finite rounds, decisive hits.";
        p.price = 9800; p.tier = 3; p.style = 8;
        p.tint = Vec3(0.44f, 0.44f, 0.42f);
        p.stats.mass = 5.2f; p.stats.powerDraw = 0.85f; p.stats.structure = 26.0f;
        p.weapon.damage = 338.25f; p.weapon.projectileSpeed = 250.0f;
        p.weapon.fireInterval = 4.292f; p.weapon.heatPerShot = 0.476f;
        p.weapon.blastRadius = 4.5f; p.weapon.blastDamage = 85.50f;
        p.weapon.ammo = AmmoKind::Limited; p.weapon.magazine = 13; p.weapon.ammoPrice = 760;
        p.weapon.range = 496.0f;
        p.weapon.tracerLength = 3.4f; p.weapon.tracerRadius = 0.16f;
        p.weapon.tracerColor = Vec3(1.0f, 0.90f, 0.62f);
        p.weapon.recoil = 1.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Heavy;
        p.id = "wp_obelisk"; p.name = "OBELISK SIEGE MORTAR"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Indirect fire in a high arc. Enormous blast, very finite rounds.";
        p.price = 12500; p.tier = 3; p.style = 9;
        p.tint = Vec3(0.42f, 0.42f, 0.40f);
        p.stats.mass = 6.4f; p.stats.powerDraw = 0.75f; p.stats.structure = 28.0f;
        p.weapon.damage = 266.50f; p.weapon.projectileSpeed = 66.0f;
        p.weapon.fireInterval = 5.902f; p.weapon.heatPerShot = 0.408f;
        p.weapon.gravity = -17.0f;
        p.weapon.blastRadius = 10.0f; p.weapon.blastDamage = 152.00f;
        p.weapon.ammo = AmmoKind::Limited; p.weapon.magazine = 10; p.weapon.ammoPrice = 880;
        p.weapon.range = 465.0f;
        p.weapon.tracerLength = 1.2f; p.weapon.tracerRadius = 0.22f;
        p.weapon.tracerColor = Vec3(1.0f, 0.78f, 0.45f);
        p.weapon.recoil = 1.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Light;
        p.id = "wp_javelin"; p.name = "JAVELIN AT MISSILE"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "A single-tube anti-armour missile that fits ANY mount on "
                  "any frame. Slow, finite, and it ruins someone's day. "
                  "Accepts seeker guidance.";
        p.price = 2600; p.tier = 1; p.style = 7;
        p.tint = Vec3(0.48f, 0.50f, 0.46f);
        p.stats.mass = 1.1f; p.stats.powerDraw = 0.15f;
        p.weapon.damage = 64.0f; p.weapon.projectileSpeed = 46.0f;
        p.weapon.fireInterval = 3.48f; p.weapon.heatPerShot = 0.08f;
        p.weapon.blastRadius = 3.4f; p.weapon.blastDamage = 30.0f;
        p.weapon.homingCapable = true;
        p.weapon.ammo = AmmoKind::Limited; p.weapon.magazine = 7; p.weapon.ammoPrice = 260;
        p.weapon.range = 320.0f;
        p.weapon.tracerLength = 2.2f; p.weapon.tracerRadius = 0.15f;
        p.weapon.tracerColor = Vec3(1.0f, 0.55f, 0.30f);
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Light;
        p.id = "wp_hive"; p.name = "HIVE ROCKET POD"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "A brick of cheap unguided rockets that mounts anywhere. "
                  "Empty it into a grid square and bill them for the square.";
        p.price = 1900; p.tier = 1; p.style = 7;
        p.tint = Vec3(0.46f, 0.46f, 0.42f);
        p.stats.mass = 1.4f; p.stats.powerDraw = 0.20f;
        p.weapon.damage = 12.0f; p.weapon.projectileSpeed = 84.0f;
        p.weapon.fireInterval = 0.319f; p.weapon.heatPerShot = 0.05f;
        p.weapon.spread = 0.030f;
        p.weapon.blastRadius = 2.4f; p.weapon.blastDamage = 8.0f;
        p.weapon.ammo = AmmoKind::Limited; p.weapon.magazine = 25; p.weapon.ammoPrice = 220;
        p.weapon.range = 240.0f;
        p.weapon.tracerLength = 1.8f; p.weapon.tracerRadius = 0.11f;
        p.weapon.tracerColor = Vec3(1.0f, 0.72f, 0.38f);
        add(p);
    }
    {
        // The rotary. Enormous sustained rate, a long spool, and a cone that
        // opens right up if you lean on the trigger - the gun that teaches a
        // player what bloom is. Built for a machine that can stand still.
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Medium;
        p.id = "wp_vulcan"; p.name = "VULCAN ROTARY"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "Six barrels and no patience. Takes a second and a half to "
                  "wind up, then puts out more metal than anything its size. "
                  "Hold it too long and it walks off the target entirely.";
        p.price = 6200; p.tier = 2; p.style = 4;
        p.tint = Vec3(0.42f, 0.44f, 0.40f);
        p.stats.mass = 3.4f; p.stats.powerDraw = 1.30f; p.stats.structure = 22.0f;
        p.weapon.damage = 13.6f; p.weapon.projectileSpeed = 200.0f;
        p.weapon.fireInterval = 0.062f; p.weapon.heatPerShot = 0.038f;
        p.weapon.spread = 0.011f;
        p.weapon.ammo = AmmoKind::Unlimited; p.weapon.range = 300.0f;
        p.weapon.spinUp = 1.5f;
        p.weapon.bloomPerShot = 0.075f; p.weapon.bloomMax = 2.20f;
        // Recovery has to be well under (rounds per second x bloomPerShot) or
        // the cone reaches equilibrium two frames in and never opens at all -
        // which is exactly what happened at 1.6: sixteen rounds a second put
        // 1.21 on, this took 1.6 off, and the measured peak was 0.08 of a
        // designed 2.20. At 0.45 a held trigger walks the group right out
        // over about three seconds and letting go clears it in one.
        p.weapon.bloomRecover = 0.45f;
        p.weapon.tracerLength = 2.2f; p.weapon.tracerRadius = 0.06f;
        p.weapon.tracerColor = Vec3(1.0f, 0.86f, 0.42f);
        p.weapon.recoil = 0.30f;
        add(p);
    }
    {
        // The opposite discipline: two heavy rounds, then a long wait. It
        // does not reward holding the trigger at all, which makes it the
        // weapon for a pilot who picks their moment.
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Medium;
        p.id = "wp_verdict"; p.name = "VERDICT DOUBLE"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "A two-round salvo from a break-action pair, then four "
                  "seconds of nothing while it reloads. Every shot has to "
                  "count, and every shot does.";
        p.price = 5800; p.tier = 2; p.style = 3;
        p.tint = Vec3(0.50f, 0.46f, 0.40f);
        p.stats.mass = 2.9f; p.stats.powerDraw = 0.80f; p.stats.structure = 24.0f;
        p.weapon.damage = 96.0f; p.weapon.projectileSpeed = 300.0f;
        p.weapon.fireInterval = 0.26f; p.weapon.heatPerShot = 0.34f;
        p.weapon.spread = 0.0022f;
        p.weapon.blastRadius = 2.4f; p.weapon.blastDamage = 22.0f;
        p.weapon.ammo = AmmoKind::Unlimited; p.weapon.range = 420.0f;
        p.weapon.burstCount = 2; p.weapon.burstGap = 4.0f;
        p.weapon.bloomPerShot = 0.05f; p.weapon.bloomMax = 0.15f;
        p.weapon.bloomRecover = 5.0f;
        p.weapon.tracerLength = 4.0f; p.weapon.tracerRadius = 0.11f;
        p.weapon.tracerColor = Vec3(1.0f, 0.72f, 0.40f);
        p.weapon.recoil = 0.95f;
        add(p);
    }
    {
        // A light burst weapon that fits anywhere, so a cheap frame has an
        // answer to the marksmen and mortar crews now shooting at it from
        // two hundred metres.
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Light;
        p.id = "wp_kestrel"; p.name = "KESTREL MARKSMAN RIFLE"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "A long light rifle in four-round bursts. Flat, quiet, and "
                  "it reaches further than anything else that fits a chin "
                  "mount. Answer their spotters with your own.";
        p.price = 3200; p.tier = 1; p.style = 2;
        p.tint = Vec3(0.44f, 0.52f, 0.50f);
        p.stats.mass = 1.5f; p.stats.powerDraw = 0.48f; p.stats.structure = 12.0f;
        p.weapon.damage = 24.0f; p.weapon.projectileSpeed = 460.0f;
        p.weapon.fireInterval = 0.20f; p.weapon.heatPerShot = 0.085f;
        p.weapon.spread = 0.0012f;
        p.weapon.ammo = AmmoKind::Unlimited; p.weapon.range = 560.0f;
        p.weapon.burstCount = 4; p.weapon.burstGap = 1.25f;
        p.weapon.bloomPerShot = 0.05f; p.weapon.bloomMax = 0.30f;
        p.weapon.bloomRecover = 0.45f;
        p.weapon.tracerLength = 5.0f; p.weapon.tracerRadius = 0.045f;
        p.weapon.tracerColor = Vec3(0.72f, 1.0f, 0.90f);
        p.weapon.recoil = 0.42f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Medium;
        p.id = "wp_flechette"; p.name = "SHRIKE FLECHETTE MORTAR"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "Lobs a canister that opens into a fan of ballistic darts. "
                  "Area denial in a single arcing round.";
        p.price = 5400; p.tier = 2; p.style = 9;
        p.tint = Vec3(0.44f, 0.46f, 0.44f);
        p.stats.mass = 3.2f; p.stats.powerDraw = 0.45f;
        p.weapon.damage = 96.0f; p.weapon.projectileSpeed = 74.0f;
        p.weapon.fireInterval = 4.205f; p.weapon.heatPerShot = 0.22f;
        p.weapon.gravity = -16.0f; p.weapon.pellets = 7; p.weapon.spread = 0.05f;
        p.weapon.blastRadius = 1.6f; p.weapon.blastDamage = 7.0f;
        p.weapon.ammo = AmmoKind::Limited; p.weapon.magazine = 14; p.weapon.ammoPrice = 380;
        p.weapon.range = 330.0f;
        p.weapon.tracerLength = 1.6f; p.weapon.tracerRadius = 0.12f;
        p.weapon.tracerColor = Vec3(0.85f, 0.95f, 0.55f);
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Medium;
        p.id = "wp_longspur"; p.name = "LONGSPUR AP RIFLE"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "A precision anti-materiel rifle with the longest sight line "
                  "in the catalogue. Made for the gunner scope.";
        p.price = 6800; p.tier = 2; p.style = 8;
        p.tint = Vec3(0.45f, 0.48f, 0.47f);
        p.stats.mass = 2.9f; p.stats.powerDraw = 0.55f;
        p.weapon.damage = 74.0f; p.weapon.projectileSpeed = 560.0f;
        p.weapon.fireInterval = 2.4f; p.weapon.heatPerShot = 0.24f;
        p.weapon.spread = 0.0012f;
        p.weapon.ammo = AmmoKind::Cooldown; p.weapon.cooldownTime = 2.4f;
        p.weapon.range = 520.0f;
        p.weapon.tracerLength = 7.0f; p.weapon.tracerRadius = 0.09f;
        p.weapon.tracerColor = Vec3(0.65f, 0.95f, 1.0f);
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Heavy;
        p.id = "wp_maul"; p.name = "MAUL SIEGE HOWITZER"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "Direct-fire demolition. Slower and blunter than the Obelisk "
                  "arc, twice as violent up close.";
        p.price = 10800; p.tier = 3; p.style = 9;
        p.tint = Vec3(0.42f, 0.42f, 0.40f);
        p.stats.mass = 5.8f; p.stats.powerDraw = 0.85f; p.stats.structure = 24.0f;
        p.weapon.damage = 210.0f; p.weapon.projectileSpeed = 120.0f;
        p.weapon.fireInterval = 5.22f; p.weapon.heatPerShot = 0.40f;
        p.weapon.gravity = -6.0f;
        p.weapon.blastRadius = 7.5f; p.weapon.blastDamage = 110.0f;
        p.weapon.ammo = AmmoKind::Limited; p.weapon.magazine = 11; p.weapon.ammoPrice = 720;
        p.weapon.range = 300.0f;
        p.weapon.tracerLength = 3.0f; p.weapon.tracerRadius = 0.24f;
        p.weapon.tracerColor = Vec3(1.0f, 0.60f, 0.30f);
        p.weapon.recoil = 1.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Heavy;
        p.id = "wp_pike"; p.name = "PIKE HEAVY GAUSS"; p.maker = "ORBEC DYNAMICS";
        p.blurb = "The longest gun there is. A hypersonic slug, a five-second "
                  "recharge, and a flash the whole map sees.";
        p.price = 13800; p.tier = 3; p.style = 8;
        p.tint = Vec3(0.44f, 0.47f, 0.50f);
        p.stats.mass = 5.2f; p.stats.powerDraw = 1.30f; p.stats.structure = 20.0f;
        p.weapon.damage = 168.0f; p.weapon.projectileSpeed = 640.0f;
        p.weapon.fireInterval = 5.0f; p.weapon.heatPerShot = 0.55f;
        p.weapon.spread = 0.0008f;
        p.weapon.ammo = AmmoKind::Cooldown; p.weapon.cooldownTime = 5.0f;
        p.weapon.range = 560.0f;
        p.weapon.tracerLength = 10.0f; p.weapon.tracerRadius = 0.13f;
        p.weapon.tracerColor = Vec3(0.55f, 0.85f, 1.0f);
        p.weapon.recoil = 1.0f;
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Light;
        p.id = "wp_lash"; p.name = "LASH DEFENSE GUN"; p.maker = "KIRIN INDUSTRIAL";
        p.blurb = "A cheap, fast backup gun for the mount you had nothing "
                  "better for. Shreds infantry and drones; annoys armour.";
        p.price = 800; p.tier = 0; p.style = 5;
        p.tint = Vec3(0.50f, 0.52f, 0.48f);
        p.stats.mass = 0.8f; p.stats.powerDraw = 0.18f;
        p.weapon.damage = 5.5f; p.weapon.projectileSpeed = 210.0f;
        p.weapon.fireInterval = 0.09f; p.weapon.heatPerShot = 0.030f;
        p.weapon.spread = 0.014f;
        p.weapon.range = 150.0f;
        p.weapon.tracerLength = 2.2f; p.weapon.tracerRadius = 0.05f;
        p.weapon.tracerColor = Vec3(1.0f, 0.85f, 0.45f);
        add(p);
    }
    {
        PartDef p;
        p.slot = Slot::Weapon; p.size = SizeClass::Medium;
        p.id = "wp_ember"; p.name = "EMBER CLUSTER MORTAR"; p.maker = "HELIOGON ARSENAL";
        p.blurb = "A short-barrel mortar throwing a cluster shell: five "
                  "bomblets walk across the impact point. Feeds on salvage.";
        p.price = 4200; p.tier = 1; p.style = 9;
        p.tint = Vec3(0.46f, 0.44f, 0.42f);
        p.stats.mass = 2.6f; p.stats.powerDraw = 0.35f;
        p.weapon.damage = 60.0f; p.weapon.projectileSpeed = 58.0f;
        p.weapon.fireInterval = 3.19f; p.weapon.heatPerShot = 0.18f;
        p.weapon.gravity = -14.0f; p.weapon.pellets = 5; p.weapon.spread = 0.04f;
        p.weapon.blastRadius = 2.8f; p.weapon.blastDamage = 12.0f;
        p.weapon.ammo = AmmoKind::Limited; p.weapon.magazine = 17; p.weapon.ammoPrice = 300;
        p.weapon.range = 280.0f;
        p.weapon.tracerLength = 1.4f; p.weapon.tracerRadius = 0.14f;
        p.weapon.tracerColor = Vec3(1.0f, 0.68f, 0.35f);
        add(p);
    }

}

const PartCatalog& PartCatalog::instance() {
    static const PartCatalog catalog;
    return catalog;
}

const PartDef* PartCatalog::find(const std::string& id) const {
    if (id.empty()) return nullptr;
    for (const PartDef& p : parts_) if (p.id == id) return &p;
    return nullptr;
}

std::vector<const PartDef*> PartCatalog::bySlot(Slot slot, int maxTier) const {
    std::vector<const PartDef*> out;
    for (const PartDef& p : parts_)
        if (p.slot == slot && p.tier <= maxTier) out.push_back(&p);
    return out;
}

const PartDef* PartCatalog::starter(Slot slot) const {
    switch (slot) {
        case Slot::Chassis: return find("ch_wasp");
        case Slot::Legs:    return find("lg_strider");
        case Slot::Engine:  return find("en_civic");
        case Slot::Armor:   return find("ar_none");
        case Slot::Sensor:  return find("se_basic");
        case Slot::Weapon:  return find("wp_pd9");
        default:            return nullptr;
    }
}

// ------------------------------------------------------------------- loadout

const PartDef* Loadout::part(Slot s) const {
    const PartCatalog& cat = PartCatalog::instance();
    switch (s) {
        case Slot::Chassis: return cat.find(chassis);
        case Slot::Legs:    return cat.find(legs);
        case Slot::Engine:  return cat.find(engine);
        case Slot::Armor:   return cat.find(armor);
        case Slot::Sensor:  return cat.find(sensor);
        default:            return nullptr;
    }
}

void Loadout::ensureWeaponSlots() {
    const PartDef* ch = part(Slot::Chassis);
    const size_t n = ch ? ch->hardpoints.size() : 0;
    weapons.resize(n);
    // Group assignment survives refits per hardpoint. New mounts default by
    // weight: light guns on the left mouse, anything heavier on the right, so
    // a fresh build already has the sensible trigger layout.
    weaponGroups.resize(n, -1);
    const PartCatalog& cat = PartCatalog::instance();
    // First pass: weight decides, and remember whether either trigger ended
    // up with nothing on it.
    int onLeft = 0, onRight = 0;
    for (size_t i = 0; i < n; ++i) {
        if (weaponGroups[i] >= 0) {
            if (!weapons[i].empty()) (weaponGroups[i] ? onRight : onLeft)++;
            continue;
        }
        const PartDef* w = cat.find(weapons[i]);
        weaponGroups[i] = (w && w->size != SizeClass::Light) ? 1 : 0;
        if (w) (weaponGroups[i] ? onRight : onLeft)++;
    }
    // A build of two identical light guns used to put EVERYTHING on the left
    // trigger, so the right mouse button did nothing and the whole fire-group
    // system looked like it did not exist. If a trigger is empty and there is
    // more than one gun fitted, split the mounts across the two.
    if (onLeft + onRight >= 2 && (onLeft == 0 || onRight == 0)) {
        const int want = (onRight == 0) ? 1 : 0;
        for (size_t i = n; i-- > 0;) {
            if (weapons[i].empty()) continue;
            weaponGroups[i] = want;     // move the LAST fitted mount over
            break;
        }
    }
}

// ------------------------------------------------------------- derived stats

MechStats deriveStats(const Loadout& loadout) {
    const PartCatalog& cat = PartCatalog::instance();
    MechStats s;

    const PartDef* chassis = loadout.part(Slot::Chassis);
    const PartDef* legs = loadout.part(Slot::Legs);
    const PartDef* engine = loadout.part(Slot::Engine);
    const PartDef* armor = loadout.part(Slot::Armor);
    const PartDef* sensor = loadout.part(Slot::Sensor);

    float mass = 0.0f, draw = 0.0f, structure = 0.0f, armorVal = 0.0f;
    auto accumulate = [&](const PartDef* p) {
        if (!p) return;
        mass += p->stats.mass;
        draw += p->stats.powerDraw;
        structure += p->stats.structure;
        armorVal += p->stats.armor;
    };
    accumulate(chassis);
    accumulate(legs);
    accumulate(engine);
    accumulate(armor);
    accumulate(sensor);
    for (const std::string& w : loadout.weapons) accumulate(cat.find(w));

    s.mass = std::max(mass, 0.5f);
    s.powerOutput = engine ? engine->stats.powerOutput : 1.0f;
    s.powerDraw = draw;

    // Overdrawing does not stop the machine, it derates it. That keeps a
    // too-greedy loadout playable but clearly worse, which is more interesting
    // than simply refusing to let the player equip it.
    s.powerMargin = (draw <= 1e-4f) ? 1.0f : clampf(s.powerOutput / draw, 0.0f, 1.0f);
    s.overdrawn = draw > s.powerOutput + 1e-4f;
    const float powerFactor = s.overdrawn ? (0.55f + 0.45f * s.powerMargin) : 1.0f;

    // Mass tells against speed but is capped so a heavy build stays viable.
    // Mass still tells against speed, but the FLOOR is much higher: at 0.42
     // a siege deck moved at a walking pace nobody enjoyed piloting across a
     // long map. A heavy machine should feel ponderous, not punitive.
    const float massFactor = clampf(kReferenceMass / s.mass, 0.78f, 1.40f);

    const float mobility = legs ? legs->stats.mobility : 5.0f;
    const float agility = legs ? legs->stats.agility : 1.0f;

    // Drive authority: how much reactor is left after everything else has
    // been fed. A machine running on the margin walks; one with headroom
    // drives its legs hard. Range is about 0.85x (starving) to 1.30x
    // (a fusion core in a light frame).
    const float surplus = (draw > 1e-4f) ? (s.powerOutput - draw) / std::max(draw, 0.5f)
                                         : 1.0f;
    // A wide spread on purpose: this is the second mobility axis. A heavy
    // machine on its starting cell crawls; the same machine with a fusion
    // core behind it moves like a lighter one. Buying reactor IS buying
    // speed, which is what gives a siege build a route back to being fun.
    const float driveFactor = clampf(0.86f + 0.38f * clampf(surplus, 0.0f, 1.4f),
                                     0.84f, 1.38f);
    s.driveFactor = driveFactor;
    // The 1.45 was for years a hidden multiplier applied inside locomotion -
    // originally the sprint button, then, when sprint was removed, a constant
    // that every machine ran at all the time. That made the workshop lie:
    // it advertised a SPD the machine beat by half as much again, so no
    // comparison between two leg sets was in real units. It belongs here,
    // where the number the pilot reads is the number the machine does.
    // The chassis trait, carried through so everything downstream can read it.
    if (const PartDef* chp = loadout.part(Slot::Chassis)) s.trait = chp->stats.trait;

    s.maxSpeed = mobility * massFactor * powerFactor * driveFactor * 1.45f;
    // Two traits touch the derived numbers directly; the rest are read at the
    // point of use (damage, spread, traverse, salvage).
    if (s.trait == Trait::Sprinter) s.maxSpeed *= 1.10f;
    s.acceleration = s.maxSpeed * 3.4f;
    if (s.trait == Trait::Nimble) s.acceleration *= 1.25f;
    s.turnRate = 2.15f * agility * clampf(massFactor, 0.55f, 1.25f) * powerFactor;
    if (s.trait == Trait::Nimble) s.turnRate *= 1.35f;
    if (s.trait == Trait::Gunnery) s.turnRate *= 1.15f;
    s.stepCadence = agility * clampf(massFactor, 0.6f, 1.3f);
    s.jumpImpulse = (legs ? legs->stats.jumpPower : 0.0f) * clampf(massFactor, 0.45f, 1.25f) * powerFactor;
    s.legReach = legs ? legs->stats.reach : 4.0f;
    s.standHeight = legs ? legs->stats.stance : 1.9f;

    // Climbing. A limb's grip rating is the ceiling; carrying more mass than the
    // reference eats into it, so the same claws that hold a scout to a wall will
    // not hold a siege deck. This is what makes wall-running a light-build
    // speciality rather than something every mech does.
    const float gripRating = legs ? legs->stats.climbGrip : 0.0f;
    const float gripMass = clampf(kReferenceMass / (s.mass * 0.85f), 0.30f, 1.30f);
    const float grip = clampf(gripRating * gripMass, 0.0f, 1.0f);
    s.climbAngle = deg2rad(45.0f + grip * 135.0f);
    s.canClimbWalls = s.climbAngle >= deg2rad(91.0f);
    s.climbSpeedFactor = clampf((legs ? legs->stats.climbSpeed : 0.4f) * powerFactor, 0.14f, 0.70f);

    s.maxHealth = std::max(structure, 20.0f);
    s.armor = armorVal;
    s.sensorRange = sensor ? sensor->stats.sensorRange : 120.0f;
    // A fire platform is built around its optics: whatever sensor you bolt on,
    // this hull sees a good deal further with it.
    if (s.trait == Trait::Spotter) s.sensorRange *= 1.40f;
    // Abilities. One active per slot on its own key - legs Q, engine E,
    // armour R, sensor F - so nothing ever overrides anything. Every passive
    // applies as before.
    {
        const PartDef* order[] = { legs, engine, armor, sensor };
        for (int slot = 0; slot < 4; ++slot) {
            const PartDef* p = order[slot];
            if (!p || p->stats.ability == Ability::None) continue;
            if (abilityIsActive(p->stats.ability)) {
                s.actives[slot].kind = p->stats.ability;
                s.actives[slot].power = p->stats.abilityPower;
                s.actives[slot].cooldown = p->stats.abilityCooldown;
                s.actives[slot].duration = p->stats.abilityDuration;
            } else {
                const int idx = static_cast<int>(p->stats.ability);
                s.hasPassive[idx] = true;
                s.passivePower[idx] = std::max(s.passivePower[idx], p->stats.abilityPower);
            }
        }
    }

    // Directional armour comes from the hull.
    s.frontDamageMult = chassis ? chassis->stats.frontDamageMult : 0.70f;
    s.rearDamageMult = chassis ? chassis->stats.rearDamageMult : 1.40f;

    // Hull family rules: the SHAPE of the chassis is a rule set, not a skin.
    s.hullFamily = chassis ? chassis->family : HullFamily::Turreted;
    switch (s.hullFamily) {
        case HullFamily::Casemate:
            // The guns are the hull. A sliver of traverse for fine laying;
            // everything else is done with the legs.
            s.turretYawLimit = deg2rad(11.0f);
            break;
        case HullFamily::LowProfile:
            // A visibly smaller machine: lower stance, smaller target.
            s.profileScale = 0.78f;
            s.standHeight *= 0.85f;
            break;
        case HullFamily::Artillery:
            // The rack only speaks planted: anything above a light gun holds
            // fire until the machine is still.
            s.plantToFire = true;
            break;
        default:
            break;
    }

    s.heatCapacity = engine ? engine->stats.heatCapacity : 1.0f;
    s.coolRate = engine ? engine->stats.coolRate : 0.30f;
    return s;
}

std::string loadoutSummary(const Loadout& loadout, const MechStats& stats) {
    (void)loadout;
    char buf[192];
    std::snprintf(buf, sizeof(buf),
                  "MASS %5.1ft   PWR %4.1f/%4.1f MW   HP %4.0f   ARM %2.0f   SPD %4.1f m/s",
                  stats.mass, stats.powerDraw, stats.powerOutput,
                  stats.maxHealth, stats.armor, stats.maxSpeed);
    return std::string(buf);
}

} // namespace sb
