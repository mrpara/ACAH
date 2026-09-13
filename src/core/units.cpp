#include "units.h"

#include "combat.h"

namespace sb {

namespace {

constexpr float kGravity = -24.0f;

// Colour language: PMC hardware is slate and gunmetal with hazard-orange
// accents, so everything hostile-but-small reads as one family and never gets
// confused with the green-tinted player machine or the bone-white elites.
const Vec3 kArmorGrey{0.42f, 0.44f, 0.47f};
const Vec3 kArmorDark{0.26f, 0.27f, 0.30f};
const Vec3 kAccent{0.95f, 0.52f, 0.18f};
const Vec3 kLens{1.0f, 0.25f, 0.2f};

WeaponDef rifleDef() {
    WeaponDef w;
    w.damage = 3.6f;
    w.projectileSpeed = 170.0f;
    w.fireInterval = 0.18f;
    w.spread = 0.020f;
    w.range = 110.0f;
    w.tracerLength = 2.0f;
    w.tracerRadius = 0.05f;
    w.tracerColor = Vec3(1.0f, 0.78f, 0.35f);
    return w;
}

WeaponDef launcherDef() {
    WeaponDef w;
    // Wave 15: the rocket is the thing that should make you take cover.
    w.damage = 58.0f;
    w.projectileSpeed = 62.0f;
    w.fireInterval = 3.0f;
    w.spread = 0.012f;
    w.blastRadius = 4.0f;
    w.blastDamage = 30.0f;
    w.range = 130.0f;
    w.tracerLength = 1.6f;
    w.tracerRadius = 0.16f;
    w.tracerColor = Vec3(1.0f, 0.45f, 0.2f);
    return w;
}

WeaponDef cupolaDef() {
    WeaponDef w;
    w.damage = 5.4f;
    w.projectileSpeed = 190.0f;
    w.fireInterval = 0.14f;
    w.spread = 0.016f;
    w.range = 130.0f;
    w.tracerLength = 2.4f;
    w.tracerRadius = 0.06f;
    w.tracerColor = Vec3(1.0f, 0.82f, 0.4f);
    return w;
}

WeaponDef tankGunDef() {
    WeaponDef w;
    w.damage = 64.0f;
    w.projectileSpeed = 150.0f;
    w.fireInterval = 3.1f;
    w.spread = 0.004f;
    w.blastRadius = 3.0f;
    w.blastDamage = 20.0f;
    w.range = 210.0f;
    w.tracerLength = 4.5f;
    w.tracerRadius = 0.12f;
    w.tracerColor = Vec3(1.0f, 0.62f, 0.3f);
    return w;
}

WeaponDef marksmanDef() {
    // An anti-materiel rifle. Flat, fast, accurate and slow to cycle: it does
    // not suppress, it punishes standing still. The tracer is thin and pale so
    // a shot that misses still tells you where the shooter is.
    WeaponDef w;
    w.damage = 34.0f;
    w.projectileSpeed = 420.0f;
    w.fireInterval = 4.1f;
    w.spread = 0.0016f;
    w.range = 330.0f;
    w.tracerLength = 6.5f;
    w.tracerRadius = 0.045f;
    w.tracerColor = Vec3(0.85f, 0.95f, 1.0f);
    return w;
}

WeaponDef mortarDef() {
    // Indirect. Slow, lobbed, generous blast, and it does not care whether it
    // can see you - it fires at where you were a second ago. The answer is to
    // not be there, or to close inside its minimum.
    WeaponDef w;
    w.damage = 26.0f;
    w.projectileSpeed = 46.0f;
    w.fireInterval = 5.0f;
    w.spread = 0.030f;
    w.blastRadius = 7.5f;
    w.blastDamage = 30.0f;
    w.range = 250.0f;
    w.tracerLength = 1.2f;
    w.tracerRadius = 0.18f;
    w.tracerColor = Vec3(0.95f, 0.72f, 0.35f);
    return w;
}

WeaponDef jammerDef() {
    // A token gun. The jammer's weapon is the bubble, not the barrel.
    WeaponDef w;
    w.damage = 2.4f;
    w.projectileSpeed = 170.0f;
    w.fireInterval = 0.30f;
    w.spread = 0.030f;
    w.range = 90.0f;
    w.tracerLength = 1.6f;
    w.tracerRadius = 0.05f;
    w.tracerColor = Vec3(0.65f, 0.85f, 1.0f);
    return w;
}

WeaponDef wardenDef() {
    // Barely armed on purpose. Everything a Warden costs you, it costs you by
    // keeping something else alive.
    WeaponDef w;
    w.damage = 3.0f;
    w.projectileSpeed = 180.0f;
    w.fireInterval = 0.26f;
    w.spread = 0.024f;
    w.range = 95.0f;
    w.tracerLength = 1.8f;
    w.tracerRadius = 0.05f;
    w.tracerColor = Vec3(0.7f, 1.0f, 0.75f);
    return w;
}

WeaponDef turretGunDef() {
    // A heavy emplacement cannon, not a bullet hose: one slow bright shell
    // every few seconds that HURTS but can be seen coming and stepped
    // around. Emplacements are the tactical problem of a strongpoint - the
    // thing you approach with a plan - not a chip-damage tax.
    WeaponDef w;
    w.damage = 38.0f;
    w.projectileSpeed = 130.0f;
    w.fireInterval = 2.2f;
    w.spread = 0.006f;
    w.blastRadius = 2.4f;
    w.blastDamage = 14.0f;
    w.range = 170.0f;
    w.tracerLength = 4.2f;
    w.tracerRadius = 0.15f;
    w.tracerColor = Vec3(1.0f, 0.5f, 0.9f);
    return w;
}

WeaponDef gunshipGunDef() {
    // A 20 mm autocannon on a gimbal: fast, heavy-ish rounds in long
    // bursts. It hurts because it never has to stop to reload and never
    // has to find a line of sight from the ground.
    WeaponDef w;
    w.damage = 7.5f;
    w.projectileSpeed = 260.0f;
    w.fireInterval = 0.11f;
    w.spread = 0.020f;
    w.blastRadius = 1.4f;
    w.blastDamage = 4.0f;
    w.range = 170.0f;
    w.tracerLength = 3.0f;
    w.tracerRadius = 0.07f;
    w.tracerColor = Vec3(1.0f, 0.75f, 0.35f);
    return w;
}

WeaponDef launcherRocketDef() {
    // Salvo artillery: six rockets in a second and a half, arcing in from
    // two hundred metres. Each one is a mortar bomb; the salvo is the point.
    WeaponDef w;
    w.damage = 18.0f;
    w.projectileSpeed = 58.0f;
    w.fireInterval = 0.22f;
    w.spread = 0.045f;
    w.blastRadius = 5.5f;
    w.blastDamage = 24.0f;
    w.range = 300.0f;
    w.tracerLength = 2.6f;
    w.tracerRadius = 0.20f;
    w.tracerColor = Vec3(1.0f, 0.55f, 0.25f);
    return w;
}

WeaponDef unarmedDef() {
    WeaponDef w;
    w.damage = 0.0f;
    w.projectileSpeed = 100.0f;
    w.fireInterval = 10.0f;
    w.range = 0.0f;
    return w;
}

WeaponDef droneGunDef() {
    WeaponDef w;
    w.damage = 3.0f;
    w.projectileSpeed = 150.0f;
    w.fireInterval = 0.14f;
    w.spread = 0.030f;
    w.range = 70.0f;
    w.tracerLength = 1.8f;
    w.tracerRadius = 0.045f;
    w.tracerColor = Vec3(0.5f, 0.85f, 1.0f);
    return w;
}

} // namespace

const char* unitKindName(UnitKind k) {
    switch (k) {
        case UnitKind::Trooper:   return "TROOPER";
        case UnitKind::ATTrooper: return "AT TEAM";
        case UnitKind::APC:       return "APC";
        case UnitKind::Tank:      return "GUN TANK";
        case UnitKind::Turret:    return "TURRET";
        case UnitKind::Drone:     return "DRONE";
        case UnitKind::Marksman:  return "MARKSMAN";
        case UnitKind::Mortar:    return "MORTAR CREW";
        case UnitKind::Jammer:    return "JAMMER";
        case UnitKind::Warden:    return "WARDEN";
        case UnitKind::Gunship:   return "GUNSHIP";
        case UnitKind::Launcher:  return "ROCKET TRUCK";
        case UnitKind::ShieldPylon: return "SHIELD PYLON";
        case UnitKind::Sapper:    return "SAPPER";
        default:                  return "?";
    }
}

const UnitStats& unitStats(UnitKind k) {
    static UnitStats table[static_cast<int>(UnitKind::Count)];
    static bool built = false;
    if (!built) {
        built = true;
        {
            UnitStats& s = table[static_cast<int>(UnitKind::Trooper)];
            s.maxHealth = 9.0f; s.speed = 4.2f; s.radius = 0.55f; s.height = 1.15f;
            s.preferredRange = 34.0f; s.engageRange = 80.0f;
            s.weapon = rifleDef();
            s.burstLen = 0.7f; s.burstPause = 1.3f;
            s.bounty = 14; s.crushable = true;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::ATTrooper)];
            s.maxHealth = 14.0f; s.speed = 3.4f; s.radius = 0.6f; s.height = 1.2f;
            s.preferredRange = 60.0f; s.engageRange = 120.0f;
            s.weapon = launcherDef();
            s.burstLen = 0.2f; s.burstPause = 3.0f;
            s.bounty = 52; s.crushable = true;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::APC)];
            s.maxHealth = 65.0f; s.speed = 7.5f; s.turnRate = 1.6f;
            s.radius = 1.9f; s.height = 2.0f;
            s.preferredRange = 45.0f; s.engageRange = 110.0f;
            s.weapon = cupolaDef();
            s.burstLen = 1.2f; s.burstPause = 1.6f;
            s.bounty = 90;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::Tank)];
            s.maxHealth = 170.0f; s.speed = 4.6f; s.turnRate = 1.1f;
            s.radius = 2.5f; s.height = 1.9f;
            s.preferredRange = 90.0f; s.engageRange = 190.0f;
            s.weapon = tankGunDef();
            s.burstLen = 0.1f; s.burstPause = 2.6f;
            s.bounty = 180;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::Turret)];
            s.maxHealth = 110.0f; s.speed = 0.0f; s.radius = 1.2f; s.height = 1.9f;
            s.preferredRange = 100.0f; s.engageRange = 160.0f;
            s.weapon = turretGunDef();
            s.burstLen = 0.1f; s.burstPause = 1.0f;
            s.bounty = 113;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::Drone)];
            s.maxHealth = 12.0f; s.speed = 9.0f; s.turnRate = 4.0f;
            s.radius = 0.8f; s.height = 0.4f;
            s.preferredRange = 26.0f; s.engageRange = 70.0f;
            s.weapon = droneGunDef();
            s.burstLen = 0.9f; s.burstPause = 1.1f;
            s.bounty = 40; s.hoverHeight = 7.5f;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::Marksman)];
            s.maxHealth = 16.0f; s.speed = 3.0f; s.radius = 0.55f; s.height = 1.15f;
            s.preferredRange = 190.0f; s.engageRange = 320.0f;
            s.weapon = marksmanDef();
            s.burstLen = 0.1f; s.burstPause = 3.4f;
            s.bounty = 120; s.crushable = true;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::Mortar)];
            s.maxHealth = 34.0f; s.speed = 2.0f; s.radius = 0.9f; s.height = 1.1f;
            s.preferredRange = 165.0f; s.engageRange = 250.0f;
            s.minRange = 55.0f;
            s.weapon = mortarDef();
            s.burstLen = 0.1f; s.burstPause = 4.6f;
            s.bounty = 150; s.crushable = true;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::Jammer)];
            s.maxHealth = 78.0f; s.speed = 6.8f; s.turnRate = 1.8f;
            s.radius = 1.9f; s.height = 2.2f;
            s.preferredRange = 80.0f; s.engageRange = 130.0f;
            s.weapon = jammerDef();
            s.burstLen = 1.0f; s.burstPause = 1.4f;
            s.bounty = 190;
            s.supportRadius = 110.0f;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::Warden)];
            s.maxHealth = 130.0f; s.speed = 5.2f; s.turnRate = 1.5f;
            s.radius = 1.7f; s.height = 2.0f;
            s.preferredRange = 55.0f; s.engageRange = 110.0f;
            s.weapon = wardenDef();
            s.burstLen = 1.1f; s.burstPause = 1.5f;
            s.bounty = 230;
            s.supportRadius = 46.0f; s.supportRate = 7.5f;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::Gunship)];
            s.maxHealth = 95.0f; s.speed = 13.0f; s.turnRate = 3.0f;
            s.radius = 1.8f; s.height = 1.2f;
            s.preferredRange = 70.0f; s.engageRange = 160.0f;
            s.weapon = gunshipGunDef();
            s.burstLen = 1.3f; s.burstPause = 2.2f;
            s.bounty = 260; s.hoverHeight = 17.0f;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::Launcher)];
            s.maxHealth = 72.0f; s.speed = 7.0f; s.turnRate = 1.5f;
            s.radius = 1.9f; s.height = 2.4f;
            s.preferredRange = 190.0f; s.engageRange = 280.0f;
            s.minRange = 60.0f;
            s.weapon = launcherRocketDef();
            s.burstLen = 1.4f; s.burstPause = 9.0f;
            s.bounty = 240;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::ShieldPylon)];
            s.maxHealth = 140.0f; s.speed = 0.0f; s.radius = 1.4f; s.height = 6.0f;
            s.preferredRange = 0.0f; s.engageRange = 0.0f;
            s.weapon = unarmedDef();
            s.burstLen = 0.0f; s.burstPause = 99.0f;
            s.bounty = 210;
            s.supportRadius = 34.0f;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::Sapper)];
            s.maxHealth = 16.0f; s.speed = 10.5f; s.turnRate = 5.0f;
            s.radius = 0.7f; s.height = 0.7f;
            s.preferredRange = 0.0f; s.engageRange = 0.0f;
            s.weapon = unarmedDef();
            s.burstLen = 0.0f; s.burstPause = 99.0f;
            s.bounty = 45; s.crushable = true;
        }
    }
    return table[static_cast<int>(k)];
}

float Unit::frand() {
    rng_ = rng_ * 1664525u + 1013904223u;
    return static_cast<float>(rng_ >> 8) / 16777216.0f;
}

void Unit::init(UnitKind kind, const Vec3& pos, float yaw, uint32_t seed) {
    kind_ = kind;
    pos_ = pos;
    anchor_ = pos;
    alerted_ = false;
    yaw_ = gunYaw_ = yaw;
    rng_ = seed ? seed : 1u;
    health_ = unitStats(kind).maxHealth;
    alive_ = true;
    pauseTimer_ = 0.5f + frand() * 1.5f;
    animPhase_ = frand() * TAU;
    wander_ = normalize(Vec3(frand() * 2.0f - 1.0f, 0.0f, frand() * 2.0f - 1.0f) +
                        Vec3(0.0f, 0.0f, 1e-3f));
}

void Unit::applyDamage(float amount) {
    if (!alive_) return;
    alerted_ = true;               // taking fire wakes the whole reflex
    // Under a shield: the round is turned, the unit still knows. Anything
    // absurd (a crush, an EMP kill) goes through.
    if (shielded_ && amount < 900.0f) { damageFlash_ = 0.5f; return; }
    health_ -= amount;
    damageFlash_ = 1.0f;
    if (health_ <= 0.0f) {
        health_ = 0.0f;
        alive_ = false;
    }
}

void Unit::settleWreck(const World& world) {
    // Ground the corpse (a dead drone falls out of the sky) and tip it by a
    // hash of where it died, so a field of wrecks does not share one pose.
    const float g = world.terrain().height(pos_.x, pos_.z);
    pos_.y = std::max(g, world.hasWater() ? world.waterLevel() - 0.4f : g);
    const uint32_t hsh = static_cast<uint32_t>(pos_.x * 57.0f) * 2654435761u ^
                         static_cast<uint32_t>(pos_.z * 91.0f) * 2246822519u;
    wreckRoll_ = (static_cast<float>(hsh & 1023u) / 1023.0f - 0.5f) * 0.42f;
    wreckPitch_ = (static_cast<float>((hsh >> 10) & 1023u) / 1023.0f - 0.5f) * 0.30f;
}

void Unit::update(float dt, const World& world, const Vec3& targetPos,
                  bool targetVisible, std::vector<ShotRequest>& shots, int unitId,
                  const Vec3& targetVel) {
    if (!alive_) return;
    damageFlash_ = damp(damageFlash_, 0.0f, 6.0f, dt);
    const UnitStats& st = stats();

    // Ground units drown too - being pushed off a causeway is fatal to an APC
    // exactly the way it is to a spidertank.
    if (world.hasWater() && kind_ != UnitKind::Drone && !amphibious_ &&
        pos_.y + st.height * 0.6f < world.waterLevel()) {
        applyDamage(60.0f * dt + 1.0f);
        if (!alive_) return;
    }

    const Vec3 toTarget = targetPos - pos_;
    const float dist = length(toTarget);
    const Vec3 flatTo = flattenY(toTarget);
    const float flatDist = std::max(length(flatTo), 1e-3f);

    // ---- movement ---------------------------------------------------------
    aiTimer_ -= dt;
    if (aiTimer_ <= 0.0f) {
        aiTimer_ = 0.8f + frand() * 1.6f;
        // Re-roll the sideways bias so squads drift apart instead of forming
        // a queue on the straight line to the player.
        wander_ = normalize(Vec3(frand() * 2.0f - 1.0f, 0.0f, frand() * 2.0f - 1.0f) +
                            Vec3(1e-3f, 0.0f, 0.0f));
    }

    // Garrison discipline. A strongpoint HOLDS its ground until the player
    // actually reaches it or opens fire on it - and even alerted, it fights
    // from its position rather than marching across the map. This is what
    // turns "an endless trickle of small enemies" into pockets you approach,
    // read, and take apart one at a time. March/convoy units (mode 1) and
    // escorts are exempt: the route owns them.
    if (!alerted_ && mode_ != 1) {
        if (targetVisible && flatDist < st.engageRange * 1.05f) alerted_ = true;
        else if (flatDist < st.preferredRange * 0.8f) alerted_ = true;
    }

    Vec3 wish(0.0f);
    float wadeSlow = 1.0f;
    if (amphibious_ && world.hasWater() &&
        world.terrain().height(pos_.x, pos_.z) < world.waterLevel())
        wadeSlow = 0.55f;
    if (mode_ == 1 && st.speed > 0.0f) {
        // On the march: the route owns the wheels, the gun owns the target.
        // Convoy pace, not flank speed - a column that outruns its pursuer is
        // a mission nobody can complete.
        Vec3 marchTarget = goal_;
        if (routeAt_ < route_.size()) {
            marchTarget = route_[routeAt_];
            if (length(flattenY(marchTarget - pos_)) < 10.0f) {
                ++routeAt_;
                if (routeAt_ < route_.size()) marchTarget = route_[routeAt_];
                else marchTarget = goal_;
            }
        }
        const Vec3 toGoal = flattenY(marchTarget - pos_);
        if (lengthSq(toGoal) > 4.0f) {
            Vec3 dir = normalize(toGoal);
            // Water check six metres out: is there anything to drive on there
            // - dry ground or a causeway deck? If not, steer back onto the
            // road line instead of shipping the vehicle. A unit with an
            // explicit deck route trusts the route instead: two steering
            // authorities fighting each other is how the crawler thrashed at
            // the shoreline without ever choosing either.
            if (hasRoad_ && world.hasWater() && route_.empty()) {
                const Vec3 ahead = pos_ + dir * 6.0f;
                const SurfaceHit h = world.findFoothold(ahead + Vec3(0.0f, 1.0f, 0.0f),
                                                        Vec3(0.0f, 1.0f, 0.0f),
                                                        2.6f, 5.0f);
                const bool supported = h.hit && h.point.y > world.waterLevel() + 0.15f;
                if (!supported) {
                    Vec3 lat = pos_ - roadPoint_;
                    lat -= roadDir_ * dot(lat, roadDir_);
                    lat.y = 0.0f;
                    const float off = length(lat);
                    const float alongSign =
                        dot(flattenY(goal_ - pos_), roadDir_) >= 0.0f ? 1.0f : -1.0f;
                    // Well off the road: drive straight back to it, no
                    // forward progress at all - a weak blend here kept the
                    // crawler marching parallel to the causeway thirty
                    // metres out in the sea until it sank.
                    if (off > 5.0f) {
                        dir = normalize(lat * (-1.0f / off) +
                                        roadDir_ * (alongSign * 0.15f));
                    } else {
                        dir = normalize(roadDir_ * alongSign -
                                        (off > 1e-3f ? lat * (0.5f / off) : Vec3(0.0f)) +
                                        Vec3(1e-4f, 0.0f, 0.0f));
                    }
                }
            }
            wish = dir * (hold_ ? 0.16f : marchPace_) * wadeSlow;
        }
    } else if (st.speed > 0.0f && !alerted_) {
        // Unalerted: hold the strongpoint - drift around the anchor, never
        // toward the player.
        const Vec3 toAnchor = flattenY(anchor_ - pos_);
        if (lengthSq(toAnchor) > 100.0f) wish = normalize(toAnchor) * 0.5f;
        else wish = wander_ * 0.22f;
    } else if (kind_ == UnitKind::Sapper) {
        // Straight at the hull, flat out, weaving a little so a gun line
        // has to track it. The charge goes off when it arrives.
        const Vec3 fwdTo = flatTo / flatDist;
        const Vec3 side(fwdTo.z, 0.0f, -fwdTo.x);
        wish = normalize(fwdTo + side * (std::sin(animPhase_ * 0.9f) * 0.35f));
        blow_ = flatDist < 4.5f && std::fabs(toTarget.y) < 6.0f;
    } else if (kind_ == UnitKind::Launcher && scootTimer_ > 0.0f) {
        // Shoot and scoot: the salvo is away, the truck is leaving.
        scootTimer_ -= dt;
        const Vec3 toAnchor = flattenY(anchor_ - pos_);
        wish = (lengthSq(toAnchor) > 9.0f) ? normalize(toAnchor) : Vec3(0.0f);
    } else if (st.speed > 0.0f) {
        const Vec3 fwdTo = flatTo / flatDist;
        if (flatDist > st.engageRange * 1.6f) {
            // The player broke contact by a wide margin: hold ground near
            // the anchor rather than pursuing across the arena.
            const Vec3 toAnchor = flattenY(anchor_ - pos_);
            wish = (lengthSq(toAnchor) > 400.0f) ? normalize(toAnchor) * 0.5f
                                                 : wander_ * 0.3f;
        } else if (flatDist > st.preferredRange * 1.15f) {
            wish = fwdTo * 0.85f + wander_ * 0.35f;
        } else if (flatDist < st.preferredRange * 0.55f) {
            // Too close to something forty times their weight: back off while
            // keeping the gun on it.
            wish = fwdTo * -0.75f + wander_ * 0.3f;
        } else {
            wish = wander_ * 0.55f;    // hold the band, keep moving
        }
        wish = flattenY(wish);
        if (lengthSq(wish) > 1e-5f) wish = normalize(wish);
    }

    if (kind_ == UnitKind::Drone || kind_ == UnitKind::Gunship) {
        // Hover: chase a point above the target band, bobbing.
        const Vec3 desired = pos_ + wish * st.speed;
        const float groundY = world.terrain().height(pos_.x, pos_.z);
        const float wantY = groundY + st.hoverHeight +
                            std::sin(animPhase_ * 0.7f) * 0.8f;
        vel_.x = damp(vel_.x, (desired.x - pos_.x) * 1.2f, 3.0f, dt) ;
        vel_.z = damp(vel_.z, (desired.z - pos_.z) * 1.2f, 3.0f, dt);
        vel_.y = clampf((wantY - pos_.y) * 2.2f, -6.0f, 6.0f);
        pos_ += vel_ * dt;
        pos_ = world.resolveCollision(pos_, st.radius);
    } else if (st.speed > 0.0f) {
        // A bank is climbed at a crawl, not at road speed.
        const float gradeSlow = 1.0f / (1.0f + grade_ * 2.2f);
        vel_.x = damp(vel_.x, wish.x * st.speed * gradeSlow, 6.0f, dt);
        vel_.z = damp(vel_.z, wish.z * st.speed * gradeSlow, 6.0f, dt);
        pos_.x += vel_.x * dt;
        pos_.z += vel_.z * dt;
        pos_ = world.resolveCollision(pos_, st.radius);
        // Ground clamp, cheap and absolute: these things do not jump.
        const float g = world.terrain().height(pos_.x, pos_.z);
        // Standing on a box counts too - a trooper on a container is fine.
        // The generous upward cast lets a vehicle mount a causeway deck or a
        // low ledge from its end instead of nosing into the side forever.
        const SurfaceHit h = world.findFoothold(pos_ + Vec3(0.0f, 1.0f, 0.0f),
                                                Vec3(0.0f, 1.0f, 0.0f), 2.6f, 4.0f);
        const float floorY = (h.hit && h.point.y > g) ? h.point.y : g;
        // The hull CLIMBS the ground rather than snapping to it. A damp at
        // fourteen a second put a tank on top of a four-metre bank in a
        // tenth of a second - from the player's seat the thing warped from
        // the bottom of the slope to the top. Rising is rate-limited to what
        // the tracks could do (a grade at travel speed, never more than a
        // few metres a second); dropping is faster, it is gravity.
        {
            const float dy = floorY - pos_.y;
            const float upRate = std::max(1.2f, st.speed * 0.75f);
            const float step = (dy > 0.0f) ? std::min(dy, upRate * dt)
                                           : std::max(dy, -9.0f * dt);
            pos_.y += (std::fabs(dy) < 0.02f) ? dy : step;
        }
    }
    // Lie on the ground: pitch and roll follow the terrain under the
    // footprint, so a vehicle on a bank is ANGLED on it. Sampled fore-aft
    // and side-side a body length apart, smoothed like suspension. Standing
    // on a deck (floor above the terrain) the hull is level. Infantry stay
    // upright - people do.
    {
        const bool lies = kind_ == UnitKind::APC || kind_ == UnitKind::Tank ||
                          kind_ == UnitKind::Jammer || kind_ == UnitKind::Warden ||
                          kind_ == UnitKind::Turret || kind_ == UnitKind::Launcher ||
                          kind_ == UnitKind::Sapper;
        float wantP = 0.0f, wantR = 0.0f, wantG = 0.0f;
        if (lies) {
            const float L = std::max(st.radius * 1.2f, 1.6f);
            const Vec3 f(std::sin(yaw_), 0.0f, std::cos(yaw_));
            const Vec3 r(std::cos(yaw_), 0.0f, -std::sin(yaw_));
            const float g0 = world.terrain().height(pos_.x, pos_.z);
            if (pos_.y < g0 + 0.6f) {
                const float hF = world.terrain().height(pos_.x + f.x * L, pos_.z + f.z * L);
                const float hB = world.terrain().height(pos_.x - f.x * L, pos_.z - f.z * L);
                const float hR = world.terrain().height(pos_.x + r.x * L, pos_.z + r.z * L);
                const float hL = world.terrain().height(pos_.x - r.x * L, pos_.z - r.z * L);
                wantP = clampf(-std::atan2(hF - hB, 2.0f * L), -0.55f, 0.55f);
                wantR = clampf(std::atan2(hR - hL, 2.0f * L), -0.45f, 0.45f);
                const Vec3 travel = flattenY(vel_);
                if (lengthSq(travel) > 0.2f) {
                    const Vec3 t = normalize(travel);
                    const float hA = world.terrain().height(pos_.x + t.x * L, pos_.z + t.z * L);
                    wantG = clampf((hA - g0) / L, 0.0f, 1.5f);
                }
            }
        }
        slopePitch_ = damp(slopePitch_, wantP, 6.0f, dt);
        slopeRoll_ = damp(slopeRoll_, wantR, 6.0f, dt);
        grade_ = damp(grade_, wantG, 4.0f, dt);
    }

    // Hull yaw follows travel for vehicles, the enemy for infantry.
    const float wantYaw = (kind_ == UnitKind::APC || kind_ == UnitKind::Tank) &&
                          lengthSq(vel_) > 0.2f
                              ? std::atan2(vel_.x, vel_.z)
                              : std::atan2(flatTo.x, flatTo.z);
    float dy = wantYaw - yaw_;
    while (dy > PI) dy -= TAU;
    while (dy < -PI) dy += TAU;
    yaw_ += clampf(dy, -st.turnRate * dt, st.turnRate * dt);

    // The gun tracks the target regardless of the hull.
    const float wantGunYaw = std::atan2(flatTo.x, flatTo.z);
    float gy = wantGunYaw - gunYaw_;
    while (gy > PI) gy -= TAU;
    while (gy < -PI) gy += TAU;
    gunYaw_ += clampf(gy, -3.0f * dt, 3.0f * dt);
    const float wantPitch = std::atan2(toTarget.y - st.height * 0.4f, flatDist);
    gunPitch_ = damp(gunPitch_, clampf(wantPitch, -0.5f, 1.25f), 6.0f, dt);

    // The walk cycle runs with SPEED, not with time: a standing trooper's
    // legs used to scissor at two radians a second forever, which read as
    // everyone jogging on the spot. Now the phase is driven by distance
    // covered (a stride per ~1.3 m for infantry, wheels by their radius),
    // plus a constant only for things that genuinely idle-animate (rotors).
    {
        const float sp = length(flattenY(vel_));
        const float infantry = (kind_ == UnitKind::Trooper || kind_ == UnitKind::ATTrooper ||
                                kind_ == UnitKind::Marksman || kind_ == UnitKind::Mortar);
        const float rate = infantry ? sp * 4.8f
                         : (kind_ == UnitKind::Drone) ? 22.0f
                         : (kind_ == UnitKind::Warden) ? sp * 1.4f + 0.6f
                         : sp * 2.9f;                    // wheels: ~0.34 m radius
        animPhase_ += dt * rate;
        if (animPhase_ > TAU * 64.0f) animPhase_ -= TAU * 64.0f;
        stride_ = damp(stride_, clampf(sp / std::max(st.speed * 0.35f, 0.5f), 0.0f, 1.0f), 8.0f, dt);
        // Hull dynamics for the vehicles: roll into a turn, pitch under
        // acceleration, both smoothed so they read as suspension.
        float dy = yaw_ - prevYaw_;
        while (dy > PI) dy -= TAU;
        while (dy < -PI) dy += TAU;
        prevYaw_ = yaw_;
        yawRate_ = damp(yawRate_, dy / std::max(dt, 1e-4f), 5.0f, dt);
        const Vec3 dv = (vel_ - prevVel_) * (1.0f / std::max(dt, 1e-4f));
        prevVel_ = vel_;
        const Vec3 fwd(std::sin(yaw_), 0.0f, std::cos(yaw_));
        accelPitch_ = damp(accelPitch_, clampf(dot(dv, fwd) * -0.02f, -0.12f, 0.12f), 4.0f, dt);
        recoil_ = std::max(0.0f, recoil_ - dt * 3.2f);
    }

    // ---- firing -----------------------------------------------------------
    fireCooldown_ = std::max(0.0f, fireCooldown_ - dt);
    // Knocked out by an EMP: the vehicle still rolls, the gun says nothing.
    if (suppressed_ > 0.0f) {
        suppressed_ -= dt;
        burstTimer_ = 0.0f;
        return;
    }
    // Indirect fire does not need line of sight - that is the whole point of
    // it, and it is what makes a mortar section a problem you have to go and
    // SOLVE rather than one you can wait out behind a wall. It does need the
    // range, at both ends: inside the minimum the tube cannot be depressed and
    // the crew are four men with sidearms.
    const bool indirect = st.minRange > 0.0f;
    if (st.weapon.damage <= 0.0f || dist > st.engageRange || (indirect && dist < st.minRange)) {
        burstTimer_ = 0.0f;
        return;
    }
    if (!targetVisible && !indirect) {
        burstTimer_ = 0.0f;
        return;
    }
    if (burstTimer_ > 0.0f) {
        burstTimer_ -= dt;
        if (burstTimer_ <= 0.0f) {
            pauseTimer_ = st.burstPause * (0.7f + frand() * 0.6f);
            if (kind_ == UnitKind::Launcher) {
                // The salvo gave the position away: move forty metres
                // sideways before the next one. Counter-battery has to
                // chase it.
                const Vec3 fwdTo = flatTo / flatDist;
                const Vec3 side(fwdTo.z, 0.0f, -fwdTo.x);
                anchor_ = pos_ + side * ((frand() < 0.5f ? -1.0f : 1.0f) * 40.0f) -
                          fwdTo * 8.0f;
                scootTimer_ = 6.5f;
            }
        }
    } else {
        pauseTimer_ -= dt;
        if (pauseTimer_ <= 0.0f) burstTimer_ = st.burstLen;
        return;
    }
    // Gun must roughly bear before firing; stops APCs shooting through their
    // own hulls while the cupola swings.
    if (std::fabs(gy) > 0.5f && !indirect) return;
    if (fireCooldown_ > 0.0f) return;

    const Vec3 muzzle = pos_ + Vec3(0.0f, st.height, 0.0f) +
                        Vec3(std::sin(gunYaw_), 0.0f, std::cos(gunYaw_)) *
                            (st.radius + 0.5f);
    // LEAD the target: aim where it will be when the round arrives. Every
    // gunner does this now, which is most of why a handful of enemies can
    // be dangerous where a crowd of them was only noisy.
    Vec3 aim = targetPos + Vec3(0.0f, 0.6f, 0.0f);
    {
        const float shotSpeed = std::max(st.weapon.projectileSpeed, 20.0f);
        const float flight = clampf(length(aim - muzzle) / shotSpeed, 0.0f, 3.0f);
        aim += targetVel * flight;
    }
    Vec3 dir = aim - muzzle;
    if (lengthSq(dir) < 1e-4f) return;
    // Rounds fall. An emplacement lays its gun for the range, the way a
    // gunner would; without that every turret past a hundred metres would
    // be shooting into the dirt short of the target.
    dir = ballisticAim(muzzle, aim, st.weapon.projectileSpeed,
                       shotGravity(st.weapon));
    if (indirect) {
        // Lofted, so the round arcs over whatever is between the tube and the
        // target instead of drilling through it, and so the player can see it
        // coming and walk out from under it.
        dir = normalize(dir);
        dir.y += 0.55f;
    }

    ShotRequest s;
    s.origin = muzzle;
    s.direction = normalize(dir);
    s.weapon = &st.weapon;
    // ...but the solution degrades against a machine that is actually
    // MOVING. A parked target is hit by everything; one crossing at speed
    // spoils the lead, and the faster it goes the worse the group. This is
    // the dodge build's defence, and it is why standing still to trade
    // shots is now a decision with a price.
    {
        const Vec3 flatVel = flattenY(targetVel);
        // Set by the mission when the thing being shot at is a low-profile
        // hull: a small silhouette at two hundred metres is genuinely harder
        // to group on, and that is the whole argument for the frame.
        // (targetProfile_ is folded into the assignment below.)
        const Vec3 toTgt = normalize(flattenY(aim - muzzle) + Vec3(1e-4f, 0.0f, 0.0f));
        // Only the component ACROSS the line of fire helps: charging
        // straight at a gun does not make you harder to hit.
        const Vec3 lateral = flatVel - toTgt * dot(flatVel, toTgt);
        const float cross = length(lateral);
        const float rangeK = clampf(dist / 90.0f, 0.35f, 1.8f);
        // ASSIGNMENT, not multiply, was throwing away the low-profile term
        // set four lines up - so the frame whose entire argument is "harder to
        // hit" was no harder to hit than anything else. Both terms count now.
        //
        // And the movement term is stronger. Speed had to be a real defence
        // for a dodge build to be a build at all: at fourteen metres a second
        // across a gun line at sixty metres this roughly triples the group,
        // which is the difference between a light frame being a choice and
        // being a way to die faster.
        s.spreadScale = targetProfile_ *
                        (1.0f + clampf(cross / 5.0f, 0.0f, 2.4f) * rangeK);
    }
    s.team = team_;
    s.shooter = -100 - unitId;   // never collides with a mech index
    shots.push_back(s);
    fireCooldown_ = st.weapon.fireInterval;
    // The gun kicks: a heavy shot throws the tube back its full travel, a
    // rifle round barely twitches it.
    recoil_ = st.weapon.damage >= 20.0f ? 1.0f : 0.35f;
}

// ------------------------------------------------------------------ meshes

const UnitMeshLibrary& UnitMeshLibrary::instance() {
    static UnitMeshLibrary lib = [] {
        UnitMeshLibrary l;
        l.trooperBody = makeChamferBox(Vec3(0.26f, 0.38f, 0.18f), 0.06f, kArmorGrey);
        l.trooperHead = makeChamferBox(Vec3(0.13f, 0.12f, 0.14f), 0.04f, kArmorDark);
        l.trooperLimb = makeBox(Vec3(0.07f, 0.30f, 0.07f), kArmorDark);
        l.launcher = makeCylinder(0.09f, 0.09f, 0.9f, 6, true, true, kArmorDark);
        l.apcHull = makeSlopedBox(Vec3(1.45f, 0.45f, 1.05f), Vec3(1.15f, 0.35f, 0.85f),
                                  0.75f, kArmorGrey);
        l.wheel = makeCylinder(0.34f, 0.34f, 0.26f, 8, true, true, kArmorDark);
        l.cupola = makeChamferBox(Vec3(0.4f, 0.22f, 0.4f), 0.08f, kArmorDark);
        l.tankHull = makeSlopedBox(Vec3(1.9f, 0.5f, 1.25f), Vec3(1.55f, 0.4f, 1.05f),
                                   0.85f, kArmorGrey);
        l.tankTurret = makeChamferBox(Vec3(0.95f, 0.38f, 0.85f), 0.14f, kArmorGrey);
        l.tankBarrel = makeCylinder(0.11f, 0.09f, 2.6f, 8, true, true, kArmorDark);
        l.turretBase = makeCylinder(1.0f, 0.85f, 1.1f, 8, true, true, kArmorDark);
        l.turretHead = makeChamferBox(Vec3(0.6f, 0.35f, 0.55f), 0.1f, kArmorGrey);
        l.turretBarrel = makeCylinder(0.08f, 0.07f, 1.7f, 6, true, true, kArmorDark);
        l.droneBody = makeChamferBox(Vec3(0.45f, 0.14f, 0.45f), 0.08f, kArmorGrey);
        l.rotor = makeBox(Vec3(0.38f, 0.015f, 0.05f), kArmorDark);
        l.skirt = makeBox(Vec3(1.55f, 0.22f, 0.10f), kArmorDark);
        l.exhaust = makeCylinder(0.09f, 0.09f, 0.55f, 6, true, true, kArmorDark);
        l.brake = makeCylinder(0.16f, 0.16f, 0.30f, 6, true, true, kArmorDark);
        l.antenna = makeCylinder(0.02f, 0.02f, 1.1f, 4, true, true, kArmorDark);
        l.drum = makeCylinder(0.28f, 0.28f, 0.40f, 8, true, true, kArmorDark);
        l.bullbar = makeBox(Vec3(1.0f, 0.16f, 0.08f), kArmorDark);
        l.gunpod = makeCylinder(0.07f, 0.05f, 0.55f, 6, true, true, kArmorDark);
        l.longBarrel = makeCylinder(0.045f, 0.035f, 1.75f, 6, true, true, kArmorDark);
        l.bipod = makeBox(Vec3(0.035f, 0.30f, 0.035f), kArmorDark);
        l.mortarTube = makeCylinder(0.15f, 0.13f, 1.05f, 8, true, true, kArmorDark);
        l.baseplate = makeCylinder(0.52f, 0.52f, 0.09f, 8, true, true, kArmorDark);
        l.dish = makeCylinder(0.06f, 0.85f, 0.32f, 10, true, true, kArmorGrey);
        l.dishMast = makeCylinder(0.09f, 0.07f, 1.0f, 6, true, true, kArmorDark);
        l.repairArm = makeBox(Vec3(0.07f, 0.07f, 0.85f), kArmorDark);
        l.walkLeg = makeBox(Vec3(0.13f, 0.62f, 0.13f), kArmorDark);
        // Infantry: a power suit is bulk at the shoulders and a pack on the
        // back, thick jointed limbs, a rifle held across the chest. Limb
        // meshes hang from their top (base at y=0, extending down) so a joint
        // is a rotation at the origin.
        l.thigh = makeChamferBox(Vec3(0.085f, 0.19f, 0.085f), 0.03f, kArmorDark);
        l.shin = makeChamferBox(Vec3(0.07f, 0.19f, 0.07f), 0.025f, kArmorGrey);
        l.upperArm = makeChamferBox(Vec3(0.065f, 0.17f, 0.065f), 0.02f, kArmorDark);
        l.rifle = makeBox(Vec3(0.045f, 0.07f, 0.42f), kArmorDark);
        l.backpack = makeChamferBox(Vec3(0.17f, 0.20f, 0.10f), 0.04f, kArmorDark);
        l.pauldron = makeChamferBox(Vec3(0.11f, 0.07f, 0.12f), 0.04f, kArmorGrey);
        l.visor = makeBox(Vec3(0.09f, 0.03f, 0.02f), kLens);
        l.boot = makeChamferBox(Vec3(0.08f, 0.05f, 0.13f), 0.02f, kArmorDark);
        // Vehicles.
        l.track = makeChamferBox(Vec3(0.34f, 0.33f, 1.85f), 0.10f, kArmorDark);
        l.roadWheel = makeCylinder(0.27f, 0.27f, 0.20f, 8, true, true, kArmorGrey);
        l.hub = makeCylinder(0.12f, 0.12f, 0.30f, 6, true, true, kArmorGrey);
        l.hatch = makeCylinder(0.34f, 0.34f, 0.10f, 8, true, true, kArmorDark);
        l.viewport = makeBox(Vec3(0.30f, 0.07f, 0.03f), Vec3(0.05f, 0.05f, 0.06f));
        l.mantlet = makeChamferBox(Vec3(0.42f, 0.30f, 0.22f), 0.08f, kArmorDark);
        l.stowage = makeChamferBox(Vec3(0.22f, 0.16f, 0.45f), 0.04f, kArmorDark);
        l.smokeTubes = makeBox(Vec3(0.22f, 0.06f, 0.06f), kArmorDark);
        l.commanderCupola = makeCylinder(0.30f, 0.26f, 0.26f, 8, true, true, kArmorGrey);
        l.sandbag = makeChamferBox(Vec3(0.55f, 0.22f, 0.30f), 0.12f, Vec3(0.40f, 0.36f, 0.28f));
        l.rotorGuard = makeCylinder(0.46f, 0.46f, 0.04f, 12, false, false, kArmorDark);
        l.tailBoom = makeBox(Vec3(0.06f, 0.05f, 0.45f), kArmorDark);
        l.fender = makeBox(Vec3(0.12f, 0.05f, 0.42f), kArmorGrey);
        // Wave 15 vehicles.
        l.apcLower = makeSlopedBox(Vec3(1.05f, 0.0f, 2.25f), Vec3(1.25f, 0.0f, 2.35f), 0.62f, kArmorGrey);
        l.apcUpper = makeSlopedBox(Vec3(1.25f, 0.0f, 1.75f), Vec3(1.02f, 0.0f, 1.45f), 0.78f, kArmorGrey);
        l.apcGlacis = makeSlopedBox(Vec3(1.2f, 0.0f, 0.62f), Vec3(1.0f, 0.0f, 0.10f), 0.74f, kArmorGrey);
        l.apcTurret = makeSlopedBox(Vec3(0.62f, 0.0f, 0.66f), Vec3(0.48f, 0.0f, 0.5f), 0.42f, kArmorGrey);
        l.apcGun = makeCylinder(0.07f, 0.055f, 1.25f, 6, true, true, kArmorDark);
        l.apcRamp = makeChamferBox(Vec3(0.95f, 0.55f, 0.06f), 0.04f, kArmorDark);
        l.headlight = makeBox(Vec3(0.11f, 0.06f, 0.03f), Vec3(0.9f, 0.85f, 0.6f));
        l.wheelBig = makeCylinder(0.46f, 0.46f, 0.34f, 10, true, true, kArmorDark);
        l.tankTub = makeBox(Vec3(1.35f, 0.34f, 2.85f), kArmorDark);
        l.tankUpper = makeSlopedBox(Vec3(1.62f, 0.0f, 2.95f), Vec3(1.45f, 0.0f, 2.15f), 0.58f, kArmorGrey);
        l.tankDeck = makeChamferBox(Vec3(1.30f, 0.10f, 0.95f), 0.05f, kArmorGrey);
        l.grille = makeBox(Vec3(1.05f, 0.03f, 0.10f), kArmorDark);
        l.tankTurretW = makeSlopedBox(Vec3(1.35f, 0.0f, 1.30f), Vec3(1.0f, 0.0f, 0.95f), 0.66f, kArmorGrey);
        l.bustle = makeChamferBox(Vec3(1.05f, 0.26f, 0.42f), 0.06f, kArmorDark);
        l.cheek = makeChamferBox(Vec3(0.34f, 0.22f, 0.30f), 0.05f, kArmorDark);
        l.tankGun = makeCylinder(0.135f, 0.10f, 4.1f, 8, true, true, kArmorDark);
        l.sprocket = makeCylinder(0.33f, 0.33f, 0.24f, 8, true, true, kArmorDark);
        l.mudguard = makeBox(Vec3(0.40f, 0.04f, 0.55f), kArmorGrey);
        l.wardenBody = makeChamferBox(Vec3(0.85f, 0.42f, 1.15f), 0.14f, kArmorGrey);
        l.wardenCab = makeChamferBox(Vec3(0.55f, 0.36f, 0.42f), 0.10f, kArmorDark);
        l.wardenHip = makeChamferBox(Vec3(0.14f, 0.55f, 0.14f), 0.04f, kArmorGrey);
        l.wardenShin = makeChamferBox(Vec3(0.10f, 0.62f, 0.10f), 0.03f, kArmorDark);
        l.craneArm = makeBox(Vec3(0.09f, 0.09f, 1.35f), kArmorDark);
        l.toolTip = makeCylinder(0.10f, 0.03f, 0.32f, 6, true, true, kArmorDark);
        l.lamp = makeBox(Vec3(0.08f, 0.08f, 0.04f), Vec3(1.0f, 0.75f, 0.35f));
        // The third tier.
        l.gunshipBody = makeSlopedBox(Vec3(0.9f, 0.0f, 2.6f), Vec3(0.7f, 0.0f, 2.0f), 1.1f, kArmorGrey);
        l.gunshipWing = makeBox(Vec3(2.4f, 0.08f, 0.5f), kArmorDark);
        l.gunshipTail = makeBox(Vec3(0.18f, 0.16f, 1.25f), kArmorDark);
        l.rocketPod = makeChamferBox(Vec3(0.9f, 0.55f, 1.5f), 0.08f, kArmorDark);
        l.pylonBase = makeCylinder(1.5f, 1.2f, 1.0f, 8, true, true, kArmorDark);
        l.pylonMast = makeCylinder(0.35f, 0.22f, 5.0f, 6, true, true, kArmorGrey);
        l.pylonRing = makeCylinder(1.3f, 1.3f, 0.25f, 12, false, false, kLens);
        l.shieldNode = makeSphere(0.45f, 4, 6, kLens);
        l.sapperDome = makeSphere(0.65f, 5, 8, kArmorDark);
        l.sapperLeg = makeBox(Vec3(0.06f, 0.06f, 0.45f), kArmorGrey);
        return l;
    }();
    return lib;
}

void Unit::submit(Rasterizer& raster, const Vec3& viewPos) const {
    const UnitMeshLibrary& lib = UnitMeshLibrary::instance();
    const float distSq = lengthSq(pos_ - viewPos);
    if (distSq > 260.0f * 260.0f) return;
    const bool near = distSq < 120.0f * 120.0f;
    const bool close = distSq < 55.0f * 55.0f;
    const Vec3 white(1.0f, 1.0f, 1.0f);

    // ---- infantry, jointed --------------------------------------------------
    // One figure for every dismounted unit: torso on a pelvis, two-segment
    // legs with a knee, arms holding the weapon, pack and shoulder plates.
    // `walk` is the stride weight (0 standing, 1 marching), `lean` tips the
    // torso into the run. The legs are driven by animPhase_, which counts
    // DISTANCE, so a standing figure stands.
    auto drawFigure = [&](auto&& draw, const Mat4& hips, const Mat4& aimYaw,
                          float walk, float lean, float crouch, bool launcher,
                          bool prone) {
        const float ph = animPhase_;
        const float swing = std::sin(ph) * 0.55f * walk;          // hip angle
        const float kneeA = std::max(0.0f, std::sin(ph + 0.9f)) * 0.9f * walk;
        const float kneeB = std::max(0.0f, std::sin(ph + PI + 0.9f)) * 0.9f * walk;
        const float bob = std::fabs(std::sin(ph)) * 0.05f * walk;
        const float hipY = prone ? 0.30f : 0.78f - crouch * 0.22f + bob;
        // Torso: on the aim yaw (a soldier turns to shoot), leaning with pace.
        const Mat4 torso = aimYaw * Mat4::translation(Vec3(0.0f, hipY, 0.0f)) *
                           Mat4::rotationX(prone ? 1.35f : lean + crouch * 0.35f);
        draw(lib.trooperBody, torso * Mat4::translation(Vec3(0.0f, 0.30f, 0.0f)), white);
        draw(lib.backpack, torso * Mat4::translation(Vec3(0.0f, 0.34f, -0.24f)), white);
        draw(lib.pauldron, torso * Mat4::translation(Vec3(-0.30f, 0.60f, 0.0f)) *
                               Mat4::rotationZ(0.25f), white);
        draw(lib.pauldron, torso * Mat4::translation(Vec3(0.30f, 0.60f, 0.0f)) *
                               Mat4::rotationZ(-0.25f), white);
        const Mat4 head = torso * Mat4::translation(Vec3(0.0f, 0.80f, 0.04f));
        draw(lib.trooperHead, head, white);
        if (close) draw(lib.visor, head * Mat4::translation(Vec3(0.0f, 0.0f, 0.14f)), kLens, 0.8f);
        // Arms and the weapon. Firing posture raises the gun to the pitch the
        // gunner wants; marching, the arms swing against the legs.
        const float armSwing = -std::sin(ph) * 0.45f * walk;
        const float raise = gunPitch_;
        const Mat4 shoulderL = torso * Mat4::translation(Vec3(-0.30f, 0.56f, 0.02f));
        const Mat4 shoulderR = torso * Mat4::translation(Vec3(0.30f, 0.56f, 0.02f));
        if (launcher) {
            // The tube rides on the right shoulder; the left arm steadies it.
            draw(lib.upperArm, shoulderR * Mat4::rotationX(-1.2f) *
                                   Mat4::translation(Vec3(0.0f, -0.17f, 0.0f)), white);
            draw(lib.upperArm, shoulderL * Mat4::rotationX(-1.6f) * Mat4::rotationZ(-0.5f) *
                                   Mat4::translation(Vec3(0.0f, -0.17f, 0.0f)), white);
            draw(lib.launcher, torso * Mat4::translation(Vec3(0.26f, 0.66f, -0.35f)) *
                                   Mat4::rotationX(PI * 0.5f + raise), kAccent);
        } else {
            draw(lib.upperArm, shoulderR * Mat4::rotationX(-1.05f - raise * 0.5f + armSwing) *
                                   Mat4::translation(Vec3(0.0f, -0.17f, 0.0f)), white);
            draw(lib.upperArm, shoulderL * Mat4::rotationX(-1.35f - raise * 0.5f - armSwing * 0.3f) *
                                   Mat4::rotationZ(-0.35f) *
                                   Mat4::translation(Vec3(0.0f, -0.17f, 0.0f)), white);
            draw(lib.rifle, torso * Mat4::translation(Vec3(0.10f, 0.42f, 0.30f)) *
                                Mat4::rotationX(-raise) *
                                Mat4::translation(Vec3(0.0f, 0.0f, 0.0f)), kArmorDark);
        }
        if (prone) {
            // Legs stretched out behind.
            for (int sx = -1; sx <= 1; sx += 2) {
                const Mat4 hip = hips * Mat4::translation(Vec3(sx * 0.13f, 0.22f, -0.30f)) *
                                 Mat4::rotationX(-1.45f);
                draw(lib.thigh, hip * Mat4::translation(Vec3(0.0f, -0.19f, 0.0f)), white);
                draw(lib.shin, hip * Mat4::translation(Vec3(0.0f, -0.38f, 0.0f)) *
                                   Mat4::translation(Vec3(0.0f, -0.19f, 0.0f)), white);
            }
            return;
        }
        // Legs: hip rotation, then the knee bends the shin back, boot on the end.
        for (int sx = -1; sx <= 1; sx += 2) {
            const float sw = swing * static_cast<float>(sx);
            const float kn = (sx < 0) ? kneeA : kneeB;
            const Mat4 hip = hips * Mat4::translation(Vec3(sx * 0.13f, hipY, 0.0f)) *
                             Mat4::rotationX(sw - crouch * 0.6f);
            draw(lib.thigh, hip * Mat4::translation(Vec3(0.0f, -0.19f, 0.0f)), white);
            const Mat4 knee = hip * Mat4::translation(Vec3(0.0f, -0.38f, 0.0f)) *
                              Mat4::rotationX(-kn - crouch * 0.9f + crouch * 0.6f);
            draw(lib.shin, knee * Mat4::translation(Vec3(0.0f, -0.19f, 0.0f)), white);
            if (near) draw(lib.boot, knee * Mat4::translation(Vec3(0.0f, -0.40f, 0.05f)), white);
        }
    };

    // The dead stay on the field. A vehicle becomes a scorched, tipped hull -
    // turret askew, barrel at the sky - which is both the reward made visible
    // and free cover. Infantry topple and are gone in a couple of seconds.
    if (!alive_) {
        if (distSq > 200.0f * 200.0f) return;
        const Vec3 scorch(0.16f, 0.155f, 0.15f);
        auto drawW = [&](const Mesh& m, const Mat4& model, const Vec3& tint = Vec3(0.16f, 0.155f, 0.15f)) {
            DrawItem it;
            it.mesh = &m;
            it.model = model;
            it.tint = tint;
            it.twoSided = true;
            raster.submit(it);
        };
        const bool figure = kind_ == UnitKind::Trooper || kind_ == UnitKind::ATTrooper ||
                            kind_ == UnitKind::Marksman;
        if (figure) {
            // Topple: the figure folds at the knees and goes over backwards or
            // forwards (by position hash), lies for a beat, then is gone.
            if (deadAge_ > 2.2f) return;
            const float fall = smoothstep01(clampf(deadAge_ / 0.45f, 0.0f, 1.0f));
            const float dir = (static_cast<int>(pos_.x * 7.0f + pos_.z * 3.0f) & 1) ? 1.0f : -1.0f;
            const float fade = 1.0f - clampf((deadAge_ - 1.5f) / 0.7f, 0.0f, 1.0f);
            const Mat4 base = Mat4::translation(pos_) * Mat4::rotationY(yaw_) *
                              Mat4::translation(Vec3(0.0f, 0.12f, 0.0f)) *
                              Mat4::rotationX(dir * fall * 1.5f) *
                              Mat4::translation(Vec3(0.0f, -0.12f, 0.0f));
            const Vec3 tint = lerp(scorch, Vec3(0.45f, 0.44f, 0.45f), fade);
            auto dd = [&](const Mesh& m, const Mat4& model, const Vec3&, float = 0.0f) {
                drawW(m, model, tint);
            };
            drawFigure(dd, base, base, 0.0f, 0.0f, fall * 0.8f, kind_ == UnitKind::ATTrooper, false);
            return;
        }
        const Mat4 wreck = Mat4::translation(pos_) * Mat4::rotationY(yaw_) *
                           Mat4::rotationX(wreckPitch_ + slopePitch_) *
                           Mat4::rotationZ(wreckRoll_ + slopeRoll_);
        switch (kind_) {
            case UnitKind::APC:
            case UnitKind::Jammer:
                drawW(lib.apcLower, wreck * Mat4::translation(Vec3(0.0f, 0.30f, 0.0f)));
                drawW(lib.apcUpper, wreck * Mat4::translation(Vec3(0.1f, 0.85f, -0.35f)) * Mat4::rotationZ(0.12f));
                drawW(lib.apcTurret, wreck * Mat4::translation(Vec3(0.6f, 1.30f, -0.4f)) *
                                         Mat4::rotationZ(0.6f) * Mat4::rotationY(1.1f));
                for (int sx = -1; sx <= 1; sx += 2)
                    for (int i = 0; i < 4; ++i)
                        drawW(lib.wheelBig, wreck * Mat4::translation(Vec3(sx * 1.15f, 0.36f, -1.65f + i * 1.1f)) *
                                                Mat4::rotationZ(PI * 0.5f + ((i + sx) & 1) * 0.3f));
                break;
            case UnitKind::Tank:
                drawW(lib.tankTub, wreck * Mat4::translation(Vec3(0.0f, 0.50f, 0.0f)));
                drawW(lib.tankUpper, wreck * Mat4::translation(Vec3(0.0f, 0.80f, 0.0f)));
                for (int sx = -1; sx <= 1; sx += 2)
                    drawW(lib.track, wreck * Mat4::translation(Vec3(sx * 1.55f, 0.36f, -0.25f)) *
                                         Mat4::scaling(Vec3(1.0f, 1.0f, 1.55f)));
                drawW(lib.tankTurretW, wreck * Mat4::translation(Vec3(0.4f, 1.25f, -0.3f)) *
                                           Mat4::rotationZ(0.45f) * Mat4::rotationY(0.8f));
                drawW(lib.tankGun, wreck * Mat4::translation(Vec3(0.4f, 1.5f, 0.4f)) *
                                       Mat4::rotationX(PI * 0.30f));
                break;
            case UnitKind::Turret:
                drawW(lib.turretBase, wreck);
                drawW(lib.turretHead, wreck * Mat4::translation(Vec3(0.4f, 1.1f, 0.2f)) *
                                          Mat4::rotationZ(0.6f));
                break;
            case UnitKind::Drone:
                drawW(lib.droneBody, wreck * Mat4::translation(Vec3(0.0f, 0.14f, 0.0f)) *
                                         Mat4::rotationZ(0.9f));
                break;
            case UnitKind::Warden:
                drawW(lib.wardenBody, wreck * Mat4::translation(Vec3(0.0f, 0.75f, 0.0f)) * Mat4::rotationZ(0.4f));
                drawW(lib.craneArm, wreck * Mat4::translation(Vec3(0.3f, 0.9f, -1.6f)) * Mat4::rotationX(-0.3f));
                for (int sx = -1; sx <= 1; sx += 2)
                    for (int sz = -1; sz <= 1; sz += 2)
                        drawW(lib.wardenHip, wreck * Mat4::translation(Vec3(sx * 1.1f, 0.4f, sz * 0.95f)) *
                                                 Mat4::rotationZ(sx * 1.2f) * Mat4::rotationX(sz * 0.4f));
                break;
            case UnitKind::Mortar:
                drawW(lib.baseplate, wreck * Mat4::translation(Vec3(0.0f, 0.06f, 0.0f)));
                drawW(lib.mortarTube, wreck * Mat4::translation(Vec3(0.3f, 0.2f, 0.0f)) * Mat4::rotationZ(1.3f));
                break;
            case UnitKind::Gunship:
                drawW(lib.gunshipBody, wreck * Mat4::translation(Vec3(0.0f, 0.3f, 0.0f)) * Mat4::rotationZ(0.7f));
                drawW(lib.gunshipWing, wreck * Mat4::translation(Vec3(0.4f, 0.9f, 0.2f)) * Mat4::rotationZ(0.9f));
                drawW(lib.gunshipTail, wreck * Mat4::translation(Vec3(1.5f, 0.3f, -3.0f)) * Mat4::rotationY(0.6f));
                break;
            case UnitKind::Launcher:
                drawW(lib.apcLower, wreck * Mat4::translation(Vec3(0.0f, 0.30f, 0.0f)));
                drawW(lib.rocketPod, wreck * Mat4::translation(Vec3(0.5f, 1.2f, -0.8f)) *
                                         Mat4::rotationZ(0.8f) * Mat4::rotationX(0.5f));
                for (int sx = -1; sx <= 1; sx += 2)
                    for (int i = 0; i < 4; ++i)
                        drawW(lib.wheelBig, wreck * Mat4::translation(Vec3(sx * 1.15f, 0.36f, -1.65f + i * 1.1f)) *
                                                Mat4::rotationZ(PI * 0.5f));
                break;
            case UnitKind::ShieldPylon:
                drawW(lib.pylonBase, wreck);
                drawW(lib.pylonMast, wreck * Mat4::translation(Vec3(0.6f, 0.9f, 0.0f)) * Mat4::rotationZ(1.25f));
                break;
            case UnitKind::Sapper:
                drawW(lib.sapperDome, wreck * Mat4::translation(Vec3(0.0f, 0.2f, 0.0f)) * Mat4::scaling(Vec3(0.8f, 0.4f, 0.8f)));
                break;
            default: break;
        }
        return;
    }

    const Vec3 flashTint = lerp(Vec3(1.0f, 1.0f, 1.0f), Vec3(1.0f, 0.4f, 0.3f),
                                clampf(damageFlash_, 0.0f, 1.0f));
    auto draw = [&](const Mesh& m, const Mat4& model, const Vec3& tint,
                    float emissive = 0.0f) {
        DrawItem it;
        it.mesh = &m;
        it.model = model;
        it.tint = tint * flashTint;
        it.emissive = emissive;
        raster.submit(it);
    };

    // Vehicle hull frame with its suspension: rolls into a turn, pitches
    // under acceleration, and squats a touch at speed.
    const float roll = clampf(-yawRate_ * 0.10f, -0.09f, 0.09f);
    const Mat4 hull = Mat4::translation(pos_) * Mat4::rotationY(yaw_) *
                      Mat4::rotationX(accelPitch_ + slopePitch_) *
                      Mat4::rotationZ(roll + slopeRoll_);
    // The gun sits on the tilted deck, so it turns in the hull's plane.
    const Mat4 gun = Mat4::translation(pos_) * Mat4::rotationY(yaw_) *
                     Mat4::rotationX(slopePitch_) * Mat4::rotationZ(slopeRoll_) *
                     Mat4::rotationY(gunYaw_ - yaw_);
    const float speedNow = length(flattenY(vel_));

    switch (kind_) {
        case UnitKind::Trooper:
        case UnitKind::ATTrooper: {
            const Mat4 hips = Mat4::translation(pos_) * Mat4::rotationY(yaw_);
            const float lean = 0.12f * stride_;
            // Standing still and shooting, a trooper takes a knee.
            const float crouch = (stride_ < 0.15f && burstTimer_ > 0.0f) ? 0.55f : 0.0f;
            drawFigure(draw, hips, gun, stride_, lean, crouch, kind_ == UnitKind::ATTrooper, false);
            break;
        }
        case UnitKind::APC: {
            // An eight-wheeled carrier: a low tub, a sloped superstructure
            // set back on it, a glacis running up to the driver's slit, a
            // small turret with a long autocannon, and a ramp at the back.
            draw(lib.apcLower, hull * Mat4::translation(Vec3(0.0f, 0.42f, 0.0f)), white);
            draw(lib.apcUpper, hull * Mat4::translation(Vec3(0.0f, 1.02f, -0.35f)), white);
            draw(lib.apcGlacis, hull * Mat4::translation(Vec3(0.0f, 1.02f, 1.55f)), white);
            {
                const float spin = animPhase_;
                for (int sx = -1; sx <= 1; sx += 2)
                    for (int i = 0; i < 4; ++i) {
                        const float z = -1.65f + i * 1.1f;
                        const Mat4 w = hull * Mat4::translation(Vec3(sx * 1.15f, 0.46f, z)) *
                                       Mat4::rotationZ(PI * 0.5f) * Mat4::rotationY(spin);
                        draw(lib.wheelBig, w, white);
                        if (near) draw(lib.hub, w * Mat4::translation(Vec3(0.0f, sx > 0 ? -0.10f : 0.10f, 0.0f)),
                                       Vec3(1.15f, 1.15f, 1.15f));
                        if (near) draw(lib.mudguard, hull * Mat4::translation(Vec3(sx * 1.25f, 0.98f, z)), white);
                    }
            }
            draw(lib.apcTurret, gun * Mat4::translation(Vec3(0.0f, 1.80f, -0.15f)), kAccent);
            draw(lib.apcGun, gun * Mat4::translation(Vec3(0.18f, 2.02f, 0.35f)) *
                                 Mat4::rotationX(PI * 0.5f - gunPitch_) *
                                 Mat4::translation(Vec3(0.0f, -recoil_ * 0.10f, 0.0f)), kArmorDark);
            if (near) {
                draw(lib.hatch, gun * Mat4::translation(Vec3(-0.30f, 2.22f, -0.35f)), white);
                draw(lib.apcRamp, hull * Mat4::translation(Vec3(0.0f, 1.05f, -2.32f)), white);
                draw(lib.bullbar, hull * Mat4::translation(Vec3(0.0f, 0.62f, 2.42f)), white);
                draw(lib.viewport, hull * Mat4::translation(Vec3(0.35f, 1.55f, 1.86f)) *
                                       Mat4::rotationX(0.55f), white, 0.3f);
                draw(lib.headlight, hull * Mat4::translation(Vec3(-0.85f, 0.95f, 2.34f)), white, 0.85f);
                draw(lib.headlight, hull * Mat4::translation(Vec3(0.85f, 0.95f, 2.34f)), white, 0.85f);
                draw(lib.antenna, hull * Mat4::translation(Vec3(-0.95f, 1.80f, -1.4f)) *
                                      Mat4::rotationZ(-0.12f + std::sin(animPhase_ * 0.5f) * 0.04f),
                     white);
                draw(lib.stowage, hull * Mat4::translation(Vec3(-1.08f, 1.45f, -0.6f)), white);
                draw(lib.stowage, hull * Mat4::translation(Vec3(1.08f, 1.45f, -0.6f)), white);
                draw(lib.smokeTubes, hull * Mat4::translation(Vec3(-0.9f, 1.80f, 0.9f)) *
                                         Mat4::rotationY(0.4f), white);
                draw(lib.smokeTubes, hull * Mat4::translation(Vec3(0.9f, 1.80f, 0.9f)) *
                                         Mat4::rotationY(-0.4f), white);
                for (int sx = -1; sx <= 1; sx += 2)
                    draw(lib.exhaust, hull * Mat4::translation(Vec3(sx * 1.15f, 1.20f, -1.9f)) *
                                          Mat4::rotationX(-1.2f), white);
            }
            break;
        }
        case UnitKind::Tank: {
            // A main battle tank with the proportions of one: a long tub
            // between two full-length tracks, a sloped upper hull with an
            // engine deck and grilles, and a wedge turret carrying a gun as
            // long as the hull, a bustle rack, cheek blocks and a cupola.
            draw(lib.tankTub, hull * Mat4::translation(Vec3(0.0f, 0.62f, 0.0f)), white);
            draw(lib.tankUpper, hull * Mat4::translation(Vec3(0.0f, 0.92f, 0.0f)), white);
            draw(lib.tankDeck, hull * Mat4::translation(Vec3(0.0f, 1.52f, -1.55f)), white);
            for (int sx = -1; sx <= 1; sx += 2) {
                draw(lib.track, hull * Mat4::translation(Vec3(sx * 1.55f, 0.42f, -0.25f)) *
                                    Mat4::scaling(Vec3(1.0f, 1.0f, 1.55f)), white);
                if (near) {
                    const float spin = animPhase_;
                    for (int i = 0; i < 7; ++i) {
                        const float z = -2.35f + i * 0.72f;
                        draw(lib.roadWheel,
                             hull * Mat4::translation(Vec3(sx * 1.66f, 0.34f, z)) *
                                 Mat4::rotationZ(PI * 0.5f) * Mat4::rotationY(spin + i),
                             Vec3(0.75f, 0.75f, 0.75f));
                    }
                    draw(lib.sprocket, hull * Mat4::translation(Vec3(sx * 1.66f, 0.55f, 2.75f)) *
                                           Mat4::rotationZ(PI * 0.5f) * Mat4::rotationY(spin), white);
                    draw(lib.sprocket, hull * Mat4::translation(Vec3(sx * 1.66f, 0.50f, -2.95f)) *
                                           Mat4::rotationZ(PI * 0.5f) * Mat4::rotationY(spin), white);
                    draw(lib.skirt, hull * Mat4::translation(Vec3(sx * 1.70f, 0.98f, -0.1f)) *
                                        Mat4::rotationY(PI * 0.5f) * Mat4::scaling(Vec3(1.7f, 1.0f, 1.0f)), white);
                    for (int g = 0; g < 3; ++g)
                        draw(lib.grille, hull * Mat4::translation(Vec3(0.0f, 1.64f, -1.15f - g * 0.42f)), white);
                }
            }
            // Turret with mantlet; the barrel recoils into it on a shot.
            const Mat4 tur = gun * Mat4::translation(Vec3(0.0f, 1.50f, 0.05f));
            draw(lib.tankTurretW, tur, white);
            draw(lib.bustle, tur * Mat4::translation(Vec3(0.0f, 0.30f, -1.45f)), white);
            const Mat4 tube = tur * Mat4::translation(Vec3(0.0f, 0.36f, 0.95f)) *
                              Mat4::rotationX(PI * 0.5f - gunPitch_);
            draw(lib.mantlet, tube * Mat4::translation(Vec3(0.0f, 0.10f, 0.0f)) *
                                  Mat4::rotationX(-PI * 0.5f) * Mat4::scaling(Vec3(1.3f, 1.2f, 1.2f)), white);
            draw(lib.tankGun, tube * Mat4::translation(Vec3(0.0f, -recoil_ * 0.45f, 0.0f)), white);
            if (near) {
                draw(lib.brake, tube * Mat4::translation(Vec3(0.0f, 3.9f - recoil_ * 0.45f, 0.0f)), white);
                draw(lib.cheek, tur * Mat4::translation(Vec3(-0.95f, 0.30f, 0.95f)) * Mat4::rotationY(0.35f), white);
                draw(lib.cheek, tur * Mat4::translation(Vec3(0.95f, 0.30f, 0.95f)) * Mat4::rotationY(-0.35f), white);
                draw(lib.commanderCupola, tur * Mat4::translation(Vec3(-0.55f, 0.66f, -0.45f)), white);
                draw(lib.gunpod, tur * Mat4::translation(Vec3(-0.55f, 0.95f, -0.25f)) *
                                     Mat4::rotationX(PI * 0.5f - 0.15f), kArmorDark);
                draw(lib.hatch, tur * Mat4::translation(Vec3(0.55f, 0.68f, -0.45f)), white);
                draw(lib.stowage, tur * Mat4::translation(Vec3(-1.25f, 0.25f, -0.35f)), white);
                draw(lib.stowage, tur * Mat4::translation(Vec3(1.25f, 0.25f, -0.35f)), white);
                draw(lib.smokeTubes, tur * Mat4::translation(Vec3(-1.05f, 0.42f, 0.55f)) * Mat4::rotationY(0.5f), white);
                draw(lib.smokeTubes, tur * Mat4::translation(Vec3(1.05f, 0.42f, 0.55f)) * Mat4::rotationY(-0.5f), white);
                for (int sx = -1; sx <= 1; sx += 2)
                    draw(lib.exhaust, hull * Mat4::translation(Vec3(sx * 0.95f, 1.45f, -2.85f)) *
                                          Mat4::rotationX(-0.6f), white);
                draw(lib.antenna, tur * Mat4::translation(Vec3(0.95f, 0.60f, -1.1f)) *
                                      Mat4::rotationZ(0.08f + std::sin(animPhase_ * 0.7f) * 0.03f), white);
                draw(lib.headlight, hull * Mat4::translation(Vec3(-1.05f, 1.32f, 2.9f)), white, 0.85f);
                draw(lib.headlight, hull * Mat4::translation(Vec3(1.05f, 1.32f, 2.9f)), white, 0.85f);
            }
            break;
        }
        case UnitKind::Turret: {
            draw(lib.turretBase, hull, white);
            if (near) {
                // A ring of sandbags: a gun emplacement, not a bollard.
                for (int i = 0; i < 8; ++i) {
                    const float a = i * (TAU / 8.0f);
                    draw(lib.sandbag, Mat4::translation(pos_ + Vec3(std::sin(a) * 1.85f, 0.20f, std::cos(a) * 1.85f)) *
                                          Mat4::rotationY(a), white);
                }
            }
            const Mat4 head = gun * Mat4::translation(Vec3(0.0f, 1.45f, 0.0f));
            draw(lib.turretHead, head, white);
            const Mat4 tube = head * Mat4::translation(Vec3(0.0f, 0.0f, 0.5f)) *
                              Mat4::rotationX(PI * 0.5f - gunPitch_);
            draw(lib.turretBarrel, tube * Mat4::translation(Vec3(0.0f, -recoil_ * 0.30f, 0.0f)), white);
            draw(lib.trooperHead, head * Mat4::translation(Vec3(0.0f, 0.3f, 0.35f)), kLens, 0.6f);
            draw(lib.drum, head * Mat4::translation(Vec3(-0.55f, -0.1f, -0.25f)) *
                               Mat4::rotationZ(PI * 0.5f), kAccent);
            break;
        }
        case UnitKind::Drone: {
            // Banks into its own sideways motion, noses into its forward one.
            const Vec3 fwd(std::sin(gunYaw_), 0.0f, std::cos(gunYaw_));
            const Vec3 right(fwd.z, 0.0f, -fwd.x);
            const float bank = clampf(-dot(vel_, right) * 0.05f, -0.35f, 0.35f);
            const float nose = clampf(dot(vel_, fwd) * 0.03f, -0.25f, 0.25f);
            const Mat4 body = Mat4::translation(pos_) * Mat4::rotationY(gunYaw_) *
                              Mat4::rotationX(nose) * Mat4::rotationZ(bank);
            draw(lib.droneBody, body, white);
            for (int i = 0; i < 4; ++i) {
                const float a = (i * PI * 0.5f) + PI * 0.25f;
                const Vec3 arm(std::sin(a) * 0.62f, 0.12f, std::cos(a) * 0.62f);
                draw(lib.rotor, body * Mat4::translation(arm) *
                                    Mat4::rotationY(animPhase_ * 3.0f + i), white);
                draw(lib.rotor, body * Mat4::translation(arm) *
                                    Mat4::rotationY(animPhase_ * 3.0f + i + PI * 0.5f), white);
                if (near) draw(lib.rotorGuard, body * Mat4::translation(arm - Vec3(0.0f, 0.04f, 0.0f)), white);
            }
            draw(lib.trooperHead, body * Mat4::translation(Vec3(0.0f, -0.08f, 0.3f)), kLens, 0.7f);
            draw(lib.gunpod, body * Mat4::translation(Vec3(0.0f, -0.18f, 0.15f)) *
                                 Mat4::rotationX(PI * 0.5f - gunPitch_), white);
            if (near) draw(lib.tailBoom, body * Mat4::translation(Vec3(0.0f, 0.0f, -0.75f)), white);
            break;
        }
        case UnitKind::Marksman: {
            // A rifleman lying behind a very long gun. The prone stance and
            // the barrel are the whole read: low, still, and pointing at you
            // from further away than anything else on the field.
            const Mat4 hips = Mat4::translation(pos_) * Mat4::rotationY(gunYaw_);
            drawFigure(draw, hips, gun, 0.0f, 0.0f, 0.0f, false, true);
            const Mat4 torso = gun * Mat4::translation(Vec3(0.0f, 0.42f, 0.0f)) *
                               Mat4::rotationX(0.22f);
            draw(lib.longBarrel,
                 torso * Mat4::translation(Vec3(0.06f, 0.16f, 0.55f - recoil_ * 0.12f)) *
                     Mat4::rotationX(PI * 0.5f - gunPitch_),
                 kAccent);
            if (near) {
                draw(lib.bipod, torso * Mat4::translation(Vec3(-0.16f, -0.16f, 1.05f)) *
                                    Mat4::rotationZ(0.35f), white);
                draw(lib.bipod, torso * Mat4::translation(Vec3(0.16f, -0.16f, 1.05f)) *
                                    Mat4::rotationZ(-0.35f), white);
            }
            break;
        }
        case UnitKind::Mortar: {
            // A tube on a baseplate with a crew of one. It points at the sky,
            // which is the visual promise that what it fires comes DOWN. The
            // tube slams into the plate on a shot; the loader leans in
            // between rounds.
            draw(lib.baseplate, hull * Mat4::translation(Vec3(0.0f, 0.06f, 0.0f)), white);
            draw(lib.mortarTube,
                 gun * Mat4::translation(Vec3(0.0f, 0.55f - recoil_ * 0.10f, 0.10f)) *
                     Mat4::rotationX(-0.95f),
                 kAccent);
            if (near) {
                draw(lib.bipod, gun * Mat4::translation(Vec3(-0.30f, 0.34f, 0.34f)) *
                                    Mat4::rotationZ(0.30f), white);
                draw(lib.bipod, gun * Mat4::translation(Vec3(0.30f, 0.34f, 0.34f)) *
                                    Mat4::rotationZ(-0.30f), white);
                // Loader: kneels beside the tube and leans over it in the
                // second before each round.
                const float load = clampf((fireCooldown_ - 0.6f) / 0.9f, 0.0f, 1.0f);
                const float leanIn = (1.0f - smoothstep01(load)) * 0.7f;
                const Mat4 crewHips = hull * Mat4::translation(Vec3(-0.85f, 0.0f, -0.20f)) *
                                      Mat4::rotationY(1.1f);
                drawFigure(draw, crewHips, crewHips, 0.0f, leanIn, 0.7f, false, false);
            }
            break;
        }
        case UnitKind::Jammer: {
            // A carrier with the turret replaced by a dish that never stops
            // turning. If you can see the dish, it can see you.
            // The same carrier as the APC, minus the turret, with a mast and a
            // dish where the gun would be and an equipment shelter behind.
            draw(lib.apcLower, hull * Mat4::translation(Vec3(0.0f, 0.42f, 0.0f)), white);
            draw(lib.apcUpper, hull * Mat4::translation(Vec3(0.0f, 1.02f, -0.35f)), white);
            draw(lib.apcGlacis, hull * Mat4::translation(Vec3(0.0f, 1.02f, 1.55f)), white);
            {
                const float spin = animPhase_;
                for (int sx = -1; sx <= 1; sx += 2)
                    for (int i = 0; i < 4; ++i) {
                        const Mat4 w = hull * Mat4::translation(Vec3(sx * 1.15f, 0.46f, -1.65f + i * 1.1f)) *
                                       Mat4::rotationZ(PI * 0.5f) * Mat4::rotationY(spin);
                        draw(lib.wheelBig, w, white);
                        if (near) draw(lib.hub, w * Mat4::translation(Vec3(0.0f, sx > 0 ? -0.10f : 0.10f, 0.0f)),
                                       Vec3(1.15f, 1.15f, 1.15f));
                    }
            }
            draw(lib.drum, hull * Mat4::translation(Vec3(0.0f, 1.95f, -1.0f)) * Mat4::rotationX(PI * 0.5f) *
                               Mat4::scaling(Vec3(2.2f, 1.6f, 2.2f)), kArmorGrey);
            draw(lib.dishMast, hull * Mat4::translation(Vec3(0.0f, 1.80f, 0.05f)) * Mat4::scaling(Vec3(1.0f, 1.4f, 1.0f)), white);
            // The dish sweeps on its own clock, not the gun's: it is looking
            // for you, not aiming at you.
            draw(lib.dish,
                 hull * Mat4::translation(Vec3(0.0f, 3.2f, 0.05f)) *
                     Mat4::rotationY(animPhase_ * 0.9f + speedNow * 0.0f) * Mat4::rotationX(-0.55f),
                 kLens, 0.55f);
            if (near) {
                draw(lib.antenna, hull * Mat4::translation(Vec3(-1.0f, 1.8f, 0.7f)) *
                                      Mat4::rotationZ(-0.16f), white);
                draw(lib.antenna, hull * Mat4::translation(Vec3(1.0f, 1.8f, 0.7f)) *
                                      Mat4::rotationZ(0.16f), white);
                draw(lib.apcRamp, hull * Mat4::translation(Vec3(0.0f, 1.05f, -2.32f)), white);
                draw(lib.viewport, hull * Mat4::translation(Vec3(0.35f, 1.55f, 1.86f)) *
                                       Mat4::rotationX(0.55f), white, 0.3f);
                draw(lib.headlight, hull * Mat4::translation(Vec3(-0.85f, 0.95f, 2.34f)), white, 0.85f);
                draw(lib.headlight, hull * Mat4::translation(Vec3(0.85f, 0.95f, 2.34f)), white, 0.85f);
                draw(lib.lamp, hull * Mat4::translation(Vec3(0.0f, 2.85f, -1.0f)), white,
                     0.5f + 0.5f * std::sin(animPhase_ * 3.0f));
            }
            break;
        }
        case UnitKind::Warden: {
            // A hunched four-legged field crane: a boxy body slung between
            // splayed two-segment legs, a cab at the front with a lit visor,
            // a crane boom over the back and two tool arms working in front.
            // The legs step in diagonal pairs and only when it moves.
            const float step = std::sin(animPhase_) * 0.32f * std::max(stride_, 0.15f);
            const float bodyY = 2.05f + std::fabs(std::sin(animPhase_)) * 0.05f * stride_;
            const Mat4 body = hull * Mat4::translation(Vec3(0.0f, bodyY, 0.0f)) * Mat4::rotationX(0.08f);
            draw(lib.wardenBody, body, white);
            draw(lib.wardenCab, body * Mat4::translation(Vec3(0.0f, 0.25f, 1.15f)), white);
            draw(lib.visor, body * Mat4::translation(Vec3(0.0f, 0.35f, 1.58f)) * Mat4::scaling(Vec3(3.5f, 2.5f, 1.0f)),
                 kLens, 0.7f);
            for (int sx = -1; sx <= 1; sx += 2)
                for (int sz = -1; sz <= 1; sz += 2) {
                    const float pair = static_cast<float>(sx * sz);
                    const float ang = step * pair;
                    const float lift = (pair > 0.0f ? std::max(0.0f, std::sin(animPhase_))
                                                    : std::max(0.0f, -std::sin(animPhase_))) * 0.35f * stride_;
                    // Hip: out and down from the body corner; shin: back in
                    // to the foot, so the leg reads as a bent limb.
                    const Mat4 hip = hull * Mat4::translation(Vec3(sx * 0.85f, bodyY - 0.15f, sz * 0.95f)) *
                                     Mat4::rotationX(ang) * Mat4::rotationZ(sx * 0.62f - sx * lift * 0.5f);
                    draw(lib.wardenHip, hip * Mat4::translation(Vec3(0.0f, -0.55f, 0.0f)), white);
                    const Mat4 knee = hip * Mat4::translation(Vec3(0.0f, -1.10f, 0.0f)) *
                                      Mat4::rotationZ(-sx * 1.05f + sx * lift * 0.4f);
                    draw(lib.wardenShin, knee * Mat4::translation(Vec3(0.0f, -0.62f, 0.0f)), white);
                    if (near) draw(lib.boot, knee * Mat4::translation(Vec3(0.0f, -1.26f, 0.0f)) *
                                              Mat4::scaling(Vec3(2.4f, 2.0f, 2.4f)), white);
                }
            // Two arms working over the front deck: the repair rig at work.
            const float sweep = std::sin(animPhase_ * 2.2f + pos_.x) * 0.6f;
            for (int sx = -1; sx <= 1; sx += 2) {
                const Mat4 arm = body * Mat4::translation(Vec3(sx * 0.6f, 0.30f, 1.0f)) *
                                 Mat4::rotationY(-sx * sweep) * Mat4::rotationX(-0.55f);
                draw(lib.repairArm, arm * Mat4::translation(Vec3(0.0f, 0.0f, 0.85f)), kAccent);
                if (near) draw(lib.toolTip, arm * Mat4::translation(Vec3(0.0f, 0.0f, 1.7f)) *
                                                Mat4::rotationX(PI * 0.5f), white);
            }
            // The crane boom over the back, with a lamp on its tip.
            const float boomA = -0.75f + std::sin(animPhase_ * 0.6f) * 0.08f;
            const Mat4 boom = body * Mat4::translation(Vec3(0.0f, 0.45f, -0.6f)) * Mat4::rotationX(boomA);
            draw(lib.craneArm, boom * Mat4::translation(Vec3(0.0f, 0.0f, -1.35f)), white);
            if (near) {
                draw(lib.lamp, boom * Mat4::translation(Vec3(0.0f, -0.12f, -2.65f)), white, 0.9f);
                draw(lib.drum, body * Mat4::translation(Vec3(0.55f, 0.55f, -0.5f)) * Mat4::rotationX(PI * 0.5f), white);
                draw(lib.drum, body * Mat4::translation(Vec3(-0.55f, 0.55f, -0.5f)) * Mat4::rotationX(PI * 0.5f), white);
                draw(lib.antenna, body * Mat4::translation(Vec3(-0.7f, 0.4f, 0.6f)) * Mat4::rotationZ(-0.2f), white);
            }
            break;
        }
        // ---- the third tier ---------------------------------------------
        case UnitKind::Gunship: {
            // A twin-rotor gunship: fat fuselage, stub wings with a rotor on
            // each tip, a tail boom, a gimballed cannon under the nose.
            const float bank = clampf(-yawRate_ * 0.25f, -0.35f, 0.35f);
            const float nose = clampf(speedNow * 0.02f, 0.0f, 0.25f);
            const Mat4 air = Mat4::translation(pos_) * Mat4::rotationY(yaw_) *
                             Mat4::rotationX(nose) * Mat4::rotationZ(bank);
            draw(lib.gunshipBody, air * Mat4::translation(Vec3(0.0f, -0.4f, 0.0f)), white);
            draw(lib.gunshipWing, air * Mat4::translation(Vec3(0.0f, 0.55f, 0.2f)), white);
            draw(lib.gunshipTail, air * Mat4::translation(Vec3(0.0f, 0.3f, -3.1f)), white);
            draw(lib.wardenCab, air * Mat4::translation(Vec3(0.0f, 0.15f, 1.9f)) *
                                    Mat4::scaling(Vec3(1.1f, 0.9f, 0.9f)), white);
            draw(lib.visor, air * Mat4::translation(Vec3(0.0f, 0.3f, 2.4f)) * Mat4::scaling(Vec3(4.0f, 3.0f, 1.0f)),
                 kLens, 0.6f);
            for (int sx = -1; sx <= 1; sx += 2) {
                const Mat4 hub = air * Mat4::translation(Vec3(sx * 2.4f, 0.7f, 0.2f));
                draw(lib.rotorGuard, hub, white);
                draw(lib.rotor, hub * Mat4::rotationY(animPhase_ * 1.3f + sx), white);
                draw(lib.rotor, hub * Mat4::rotationY(animPhase_ * 1.3f + sx + PI * 0.5f), white);
            }
            {
                const Mat4 pod = Mat4::translation(pos_) * Mat4::rotationY(gunYaw_) *
                                 Mat4::translation(Vec3(0.0f, -1.05f, 1.6f)) *
                                 Mat4::rotationX(PI * 0.5f - gunPitch_);
                draw(lib.gunpod, pod * Mat4::scaling(Vec3(1.6f, 1.8f, 1.6f)) *
                                     Mat4::translation(Vec3(0.0f, -recoil_ * 0.05f, 0.0f)), kArmorDark);
            }
            if (near) {
                draw(lib.lamp, air * Mat4::translation(Vec3(0.0f, -0.2f, -4.3f)), white,
                     std::fmod(animPhase_, 6.0f) < 0.5f ? 1.0f : 0.1f);
                draw(lib.exhaust, air * Mat4::translation(Vec3(-0.6f, 0.5f, -1.4f)) * Mat4::rotationX(-1.2f), white);
                draw(lib.exhaust, air * Mat4::translation(Vec3(0.6f, 0.5f, -1.4f)) * Mat4::rotationX(-1.2f), white);
            }
            break;
        }
        case UnitKind::Launcher: {
            // The APC chassis carrying a twelve-tube rocket pod on an
            // elevating cradle. The pod rises to fire and drops to drive.
            draw(lib.apcLower, hull * Mat4::translation(Vec3(0.0f, 0.42f, 0.0f)), white);
            draw(lib.wardenCab, hull * Mat4::translation(Vec3(0.0f, 1.35f, 1.6f)) *
                                    Mat4::scaling(Vec3(1.8f, 1.0f, 1.2f)), white);
            {
                const float spin = animPhase_;
                for (int sx = -1; sx <= 1; sx += 2)
                    for (int i = 0; i < 4; ++i) {
                        const Mat4 w = hull * Mat4::translation(Vec3(sx * 1.15f, 0.46f, -1.65f + i * 1.1f)) *
                                       Mat4::rotationZ(PI * 0.5f) * Mat4::rotationY(spin);
                        draw(lib.wheelBig, w, white);
                    }
            }
            const float elev = (burstTimer_ > 0.0f || pauseTimer_ < 2.0f) ? 0.55f : 0.15f;
            const Mat4 cradle = gun * Mat4::translation(Vec3(0.0f, 1.55f, -0.6f)) *
                                Mat4::rotationX(-elev);
            draw(lib.rocketPod, cradle * Mat4::translation(Vec3(0.0f, 0.55f, 0.6f)), white);
            if (near) {
                for (int r = 0; r < 3; ++r)
                    for (int c = -1; c <= 1; ++c)
                        draw(lib.exhaust, cradle * Mat4::translation(Vec3(c * 0.55f, 0.25f + r * 0.32f, 2.1f)) *
                                              Mat4::rotationX(PI * 0.5f) * Mat4::scaling(Vec3(1.6f, 0.3f, 1.6f)),
                             Vec3(0.2f, 0.2f, 0.22f));
                draw(lib.viewport, hull * Mat4::translation(Vec3(0.0f, 1.55f, 2.2f)), white, 0.3f);
                draw(lib.headlight, hull * Mat4::translation(Vec3(-0.85f, 0.95f, 2.34f)), white, 0.85f);
                draw(lib.headlight, hull * Mat4::translation(Vec3(0.85f, 0.95f, 2.34f)), white, 0.85f);
                draw(lib.antenna, hull * Mat4::translation(Vec3(-0.95f, 1.80f, 0.4f)) * Mat4::rotationZ(-0.12f), white);
            }
            break;
        }
        case UnitKind::ShieldPylon: {
            // A mast with a lit ring at the top, and the shield itself drawn
            // as a ring of nodes at its rim so the pilot can see where it
            // stops. The ring pulses; the nodes drift up and down.
            const Mat4 base = Mat4::translation(pos_) * Mat4::rotationY(yaw_);
            draw(lib.pylonBase, base, white);
            draw(lib.pylonMast, base * Mat4::translation(Vec3(0.0f, 1.0f, 0.0f)), white);
            const float pulse = 0.55f + 0.45f * std::sin(animPhase_ * 0.5f);
            draw(lib.pylonRing, base * Mat4::translation(Vec3(0.0f, 5.6f, 0.0f)), kLens, pulse);
            draw(lib.shieldNode, base * Mat4::translation(Vec3(0.0f, 6.3f, 0.0f)) * Mat4::scaling(Vec3(1.4f)),
                 kLens, pulse);
            {
                const float R = stats().supportRadius;
                for (int i = 0; i < 18; ++i) {
                    const float a = TAU * i / 18.0f + animPhase_ * 0.03f;
                    const float y = 1.2f + 2.4f * (0.5f + 0.5f * std::sin(animPhase_ * 0.4f + i * 1.7f));
                    draw(lib.shieldNode, Mat4::translation(pos_ + Vec3(std::sin(a) * R, y, std::cos(a) * R)),
                         kLens, 0.35f + 0.3f * pulse);
                }
            }
            break;
        }
        case UnitKind::Sapper: {
            // A mine on legs: a low dome with four stub legs scrabbling and a
            // lamp that blinks faster the closer it gets.
            draw(lib.sapperDome, hull * Mat4::translation(Vec3(0.0f, 0.45f, 0.0f)) *
                                     Mat4::scaling(Vec3(1.0f, 0.65f, 1.0f)), white);
            for (int i = 0; i < 4; ++i) {
                const float a = i * PI * 0.5f + PI * 0.25f;
                const float step = std::sin(animPhase_ * 2.0f + i * 1.6f) * 0.5f * std::max(stride_, 0.1f);
                draw(lib.sapperLeg, hull * Mat4::translation(Vec3(0.0f, 0.35f, 0.0f)) * Mat4::rotationY(a + step) *
                                        Mat4::translation(Vec3(0.0f, 0.0f, 0.55f)) * Mat4::rotationX(0.6f), white);
            }
            draw(lib.lamp, hull * Mat4::translation(Vec3(0.0f, 0.9f, 0.0f)), Vec3(1.0f, 0.3f, 0.2f),
                 std::fmod(animPhase_, alerted_ ? 1.2f : 4.0f) < 0.3f ? 1.0f : 0.1f);
            break;
        }
        default: break;
    }
}

} // namespace sb
