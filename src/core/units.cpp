#include "units.h"

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
    w.damage = 42.0f;
    w.projectileSpeed = 58.0f;
    w.fireInterval = 3.2f;
    w.spread = 0.012f;
    w.blastRadius = 3.6f;
    w.blastDamage = 22.0f;
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
    w.damage = 56.0f;
    w.projectileSpeed = 150.0f;
    w.fireInterval = 3.3f;
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
            s.radius = 1.6f; s.height = 1.6f;
            s.preferredRange = 45.0f; s.engageRange = 110.0f;
            s.weapon = cupolaDef();
            s.burstLen = 1.2f; s.burstPause = 1.6f;
            s.bounty = 90;
        }
        {
            UnitStats& s = table[static_cast<int>(UnitKind::Tank)];
            s.maxHealth = 170.0f; s.speed = 4.6f; s.turnRate = 1.1f;
            s.radius = 2.1f; s.height = 1.5f;
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
            s.radius = 1.5f; s.height = 1.7f;
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

    if (kind_ == UnitKind::Drone) {
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
        vel_.x = damp(vel_.x, wish.x * st.speed, 6.0f, dt);
        vel_.z = damp(vel_.z, wish.z * st.speed, 6.0f, dt);
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
        pos_.y = damp(pos_.y, floorY, 14.0f, dt);
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

    animPhase_ += dt * (2.0f + length(flattenY(vel_)) * 1.6f +
                        (kind_ == UnitKind::Drone ? 20.0f : 0.0f));
    if (animPhase_ > TAU * 64.0f) animPhase_ -= TAU * 64.0f;

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
    if (dist > st.engageRange || (indirect && dist < st.minRange)) {
        burstTimer_ = 0.0f;
        return;
    }
    if (!targetVisible && !indirect) {
        burstTimer_ = 0.0f;
        return;
    }
    if (burstTimer_ > 0.0f) {
        burstTimer_ -= dt;
        if (burstTimer_ <= 0.0f) pauseTimer_ = st.burstPause * (0.7f + frand() * 0.6f);
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
        return l;
    }();
    return lib;
}

void Unit::submit(Rasterizer& raster, const Vec3& viewPos) const {
    const UnitMeshLibrary& lib = UnitMeshLibrary::instance();
    const float distSq = lengthSq(pos_ - viewPos);
    if (distSq > 260.0f * 260.0f) return;

    // The dead stay on the field. A vehicle becomes a scorched, tipped hull -
    // turret askew, barrel at the sky - which is both the reward made visible
    // and free cover. Infantry are too small to leave anything.
    if (!alive_) {
        if (kind_ == UnitKind::Trooper || kind_ == UnitKind::ATTrooper) return;
        if (distSq > 200.0f * 200.0f) return;
        const Vec3 scorch(0.16f, 0.155f, 0.15f);
        auto drawW = [&](const Mesh& m, const Mat4& model) {
            DrawItem it;
            it.mesh = &m;
            it.model = model;
            it.tint = scorch;
            it.twoSided = true;
            raster.submit(it);
        };
        const Mat4 wreck = Mat4::translation(pos_) * Mat4::rotationY(yaw_) *
                           Mat4::rotationX(wreckPitch_) * Mat4::rotationZ(wreckRoll_);
        switch (kind_) {
            case UnitKind::APC:
                drawW(lib.apcHull, wreck * Mat4::translation(Vec3(0.0f, 0.42f, 0.0f)));
                drawW(lib.cupola, wreck * Mat4::translation(Vec3(0.35f, 1.05f, -0.4f)) *
                                      Mat4::rotationZ(0.5f));
                break;
            case UnitKind::Tank:
                drawW(lib.tankHull, wreck * Mat4::translation(Vec3(0.0f, 0.5f, 0.0f)));
                drawW(lib.tankTurret, wreck * Mat4::translation(Vec3(0.3f, 1.15f, -0.3f)) *
                                          Mat4::rotationZ(0.35f) * Mat4::rotationY(0.8f));
                drawW(lib.tankBarrel, wreck * Mat4::translation(Vec3(0.3f, 1.3f, 0.4f)) *
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

    const Mat4 hull = Mat4::translation(pos_) * Mat4::rotationY(yaw_);
    const Mat4 gun = Mat4::translation(pos_) * Mat4::rotationY(gunYaw_);
    const bool near = distSq < 120.0f * 120.0f;

    switch (kind_) {
        case UnitKind::Trooper:
        case UnitKind::ATTrooper: {
            const float bob = std::sin(animPhase_ * 2.4f) * 0.05f;
            const Mat4 torso = gun * Mat4::translation(Vec3(0.0f, 0.72f + bob, 0.0f));
            draw(lib.trooperBody, torso, Vec3(1.0f, 1.0f, 1.0f));
            draw(lib.trooperHead, torso * Mat4::translation(Vec3(0.0f, 0.50f, 0.02f)),
                 Vec3(1.0f, 1.0f, 1.0f));
            if (near) {
                // Two legs, scissoring with the walk.
                const float sw = std::sin(animPhase_ * 2.4f) * 0.5f;
                draw(lib.trooperLimb,
                     hull * Mat4::translation(Vec3(-0.14f, 0.32f, 0.0f)) *
                         Mat4::rotationX(sw),
                     Vec3(1.0f, 1.0f, 1.0f));
                draw(lib.trooperLimb,
                     hull * Mat4::translation(Vec3(0.14f, 0.32f, 0.0f)) *
                         Mat4::rotationX(-sw),
                     Vec3(1.0f, 1.0f, 1.0f));
            }
            if (kind_ == UnitKind::ATTrooper) {
                draw(lib.launcher,
                     torso * Mat4::translation(Vec3(0.24f, 0.18f, -0.35f)) *
                         Mat4::rotationX(PI * 0.5f + gunPitch_),
                     kAccent);
            }
            break;
        }
        case UnitKind::APC: {
            draw(lib.apcHull, hull * Mat4::translation(Vec3(0.0f, 0.55f, 0.0f)),
                 Vec3(1.0f, 1.0f, 1.0f));
            if (near) {
                const float spin = animPhase_ * 2.0f;
                for (int sx = -1; sx <= 1; sx += 2)
                    for (int i = 0; i < 3; ++i) {
                        const float z = -0.95f + i * 0.95f;
                        draw(lib.wheel,
                             hull * Mat4::translation(Vec3(sx * 1.05f, 0.34f, z)) *
                                 Mat4::rotationZ(PI * 0.5f) * Mat4::rotationY(spin),
                             Vec3(1.0f, 1.0f, 1.0f));
                    }
            }
            draw(lib.cupola, gun * Mat4::translation(Vec3(0.0f, 1.35f, 0.0f)), kAccent);
            if (near) {
                draw(lib.bullbar, hull * Mat4::translation(Vec3(0.0f, 0.55f, 1.15f)),
                     Vec3(1.0f, 1.0f, 1.0f));
                draw(lib.antenna, hull * Mat4::translation(Vec3(-0.85f, 1.35f, -0.7f)) *
                                      Mat4::rotationZ(-0.12f),
                     Vec3(1.0f, 1.0f, 1.0f));
            }
            break;
        }
        case UnitKind::Tank: {
            draw(lib.tankHull, hull * Mat4::translation(Vec3(0.0f, 0.65f, 0.0f)),
                 Vec3(1.0f, 1.0f, 1.0f));
            const Mat4 tur = gun * Mat4::translation(Vec3(0.0f, 1.45f, -0.1f));
            draw(lib.tankTurret, tur, Vec3(1.0f, 1.0f, 1.0f));
            draw(lib.tankBarrel,
                 tur * Mat4::translation(Vec3(0.0f, 0.05f, 0.7f)) *
                     Mat4::rotationX(PI * 0.5f - gunPitch_),
                 Vec3(1.0f, 1.0f, 1.0f));
            if (near) {
                // Muzzle brake at the end of the tube, exhausts on the deck,
                // side skirts over the wheel line: the tank furniture.
                draw(lib.brake,
                     tur * Mat4::translation(Vec3(0.0f, 0.05f, 0.7f)) *
                         Mat4::rotationX(PI * 0.5f - gunPitch_) *
                         Mat4::translation(Vec3(0.0f, 2.45f, 0.0f)),
                     Vec3(1.0f, 1.0f, 1.0f));
                for (int sx = -1; sx <= 1; sx += 2) {
                    draw(lib.skirt,
                         hull * Mat4::translation(Vec3(sx * 1.38f, 0.62f, 0.0f)) *
                             Mat4::rotationY(PI * 0.5f),
                         Vec3(1.0f, 1.0f, 1.0f));
                    draw(lib.exhaust,
                         hull * Mat4::translation(Vec3(sx * 0.55f, 1.15f, -1.35f)) *
                             Mat4::rotationX(-0.5f),
                         Vec3(1.0f, 1.0f, 1.0f));
                }
            }
            if (near) {
                const float spin = animPhase_ * 2.0f;
                for (int sx = -1; sx <= 1; sx += 2)
                    for (int i = 0; i < 4; ++i) {
                        const float z = -1.25f + i * 0.85f;
                        draw(lib.wheel,
                             hull * Mat4::translation(Vec3(sx * 1.35f, 0.34f, z)) *
                                 Mat4::rotationZ(PI * 0.5f) * Mat4::rotationY(spin),
                             Vec3(0.8f, 0.8f, 0.8f));
                    }
            }
            break;
        }
        case UnitKind::Turret: {
            draw(lib.turretBase, hull, Vec3(1.0f, 1.0f, 1.0f));
            const Mat4 head = gun * Mat4::translation(Vec3(0.0f, 1.45f, 0.0f));
            draw(lib.turretHead, head, Vec3(1.0f, 1.0f, 1.0f));
            draw(lib.turretBarrel,
                 head * Mat4::translation(Vec3(0.0f, 0.0f, 0.5f)) *
                     Mat4::rotationX(PI * 0.5f - gunPitch_),
                 Vec3(1.0f, 1.0f, 1.0f));
            draw(lib.trooperHead, head * Mat4::translation(Vec3(0.0f, 0.3f, 0.35f)),
                 kLens, 0.6f);
            draw(lib.drum, head * Mat4::translation(Vec3(-0.55f, -0.1f, -0.25f)) *
                               Mat4::rotationZ(PI * 0.5f),
                 kAccent);
            break;
        }
        case UnitKind::Drone: {
            const Mat4 body = Mat4::translation(pos_) * Mat4::rotationY(gunYaw_) *
                              Mat4::rotationX(clampf(vel_.z * 0.02f, -0.2f, 0.2f));
            draw(lib.droneBody, body, Vec3(1.0f, 1.0f, 1.0f));
            for (int i = 0; i < 4; ++i) {
                const float a = (i * PI * 0.5f) + PI * 0.25f;
                draw(lib.rotor,
                     body * Mat4::translation(Vec3(std::sin(a) * 0.62f, 0.12f,
                                                   std::cos(a) * 0.62f)) *
                         Mat4::rotationY(animPhase_ * 3.0f + i),
                     Vec3(1.0f, 1.0f, 1.0f));
            }
            draw(lib.trooperHead, body * Mat4::translation(Vec3(0.0f, -0.08f, 0.3f)),
                 kLens, 0.7f);
            draw(lib.gunpod, body * Mat4::translation(Vec3(0.0f, -0.18f, 0.15f)) *
                                 Mat4::rotationX(PI * 0.5f - gunPitch_),
                 Vec3(1.0f, 1.0f, 1.0f));
            break;
        }
        case UnitKind::Marksman: {
            // A rifleman lying behind a very long gun. The prone stance and
            // the barrel are the whole read: low, still, and pointing at you
            // from further away than anything else on the field.
            const Mat4 torso = gun * Mat4::translation(Vec3(0.0f, 0.42f, 0.0f)) *
                               Mat4::rotationX(0.22f);
            draw(lib.trooperBody, torso, Vec3(1.0f, 1.0f, 1.0f));
            draw(lib.trooperHead, torso * Mat4::translation(Vec3(0.0f, 0.44f, 0.10f)),
                 Vec3(1.0f, 1.0f, 1.0f));
            draw(lib.longBarrel,
                 torso * Mat4::translation(Vec3(0.06f, 0.16f, 0.55f)) *
                     Mat4::rotationX(PI * 0.5f - gunPitch_),
                 kAccent);
            if (near) {
                draw(lib.bipod, torso * Mat4::translation(Vec3(-0.16f, -0.16f, 1.05f)) *
                                    Mat4::rotationZ(0.35f),
                     Vec3(1.0f, 1.0f, 1.0f));
                draw(lib.bipod, torso * Mat4::translation(Vec3(0.16f, -0.16f, 1.05f)) *
                                    Mat4::rotationZ(-0.35f),
                     Vec3(1.0f, 1.0f, 1.0f));
                draw(lib.trooperHead,
                     torso * Mat4::translation(Vec3(0.06f, 0.30f, 0.30f)), kLens, 0.6f);
            }
            break;
        }
        case UnitKind::Mortar: {
            // A tube on a baseplate with a crew of one. It points at the sky,
            // which is the visual promise that what it fires comes DOWN.
            draw(lib.baseplate, hull * Mat4::translation(Vec3(0.0f, 0.06f, 0.0f)),
                 Vec3(1.0f, 1.0f, 1.0f));
            draw(lib.mortarTube,
                 gun * Mat4::translation(Vec3(0.0f, 0.55f, 0.10f)) *
                     Mat4::rotationX(-0.95f),
                 kAccent);
            if (near) {
                draw(lib.bipod, gun * Mat4::translation(Vec3(-0.30f, 0.34f, 0.34f)) *
                                    Mat4::rotationZ(0.30f),
                     Vec3(1.0f, 1.0f, 1.0f));
                draw(lib.bipod, gun * Mat4::translation(Vec3(0.30f, 0.34f, 0.34f)) *
                                    Mat4::rotationZ(-0.30f),
                     Vec3(1.0f, 1.0f, 1.0f));
                const float bob = std::sin(animPhase_ * 1.6f) * 0.04f;
                const Mat4 crew = hull * Mat4::translation(Vec3(-0.75f, 0.70f + bob, -0.30f));
                draw(lib.trooperBody, crew, Vec3(1.0f, 1.0f, 1.0f));
                draw(lib.trooperHead, crew * Mat4::translation(Vec3(0.0f, 0.48f, 0.02f)),
                     Vec3(1.0f, 1.0f, 1.0f));
            }
            break;
        }
        case UnitKind::Jammer: {
            // A carrier with the turret replaced by a dish that never stops
            // turning. If you can see the dish, it can see you.
            draw(lib.apcHull, hull * Mat4::translation(Vec3(0.0f, 0.55f, 0.0f)),
                 Vec3(1.0f, 1.0f, 1.0f));
            if (near) {
                const float spin = animPhase_ * 2.0f;
                for (int sx = -1; sx <= 1; sx += 2)
                    for (int i = 0; i < 3; ++i)
                        draw(lib.wheel,
                             hull * Mat4::translation(Vec3(sx * 1.05f, 0.34f,
                                                           -0.95f + i * 0.95f)) *
                                 Mat4::rotationZ(PI * 0.5f) * Mat4::rotationY(spin),
                             Vec3(1.0f, 1.0f, 1.0f));
            }
            draw(lib.dishMast, hull * Mat4::translation(Vec3(0.0f, 1.35f, -0.15f)),
                 Vec3(1.0f, 1.0f, 1.0f));
            // The dish sweeps on its own clock, not the gun's: it is looking
            // for you, not aiming at you.
            draw(lib.dish,
                 hull * Mat4::translation(Vec3(0.0f, 2.05f, -0.15f)) *
                     Mat4::rotationY(animPhase_ * 0.9f) * Mat4::rotationX(-0.55f),
                 kLens, 0.55f);
            if (near) {
                draw(lib.antenna, hull * Mat4::translation(Vec3(-0.9f, 1.3f, 0.7f)) *
                                      Mat4::rotationZ(-0.16f),
                     Vec3(1.0f, 1.0f, 1.0f));
                draw(lib.antenna, hull * Mat4::translation(Vec3(0.9f, 1.3f, 0.7f)) *
                                      Mat4::rotationZ(0.16f),
                     Vec3(1.0f, 1.0f, 1.0f));
            }
            break;
        }
        case UnitKind::Warden: {
            // A four-legged repair gantry: tall, slow, and unmistakable, with
            // arms that work whether or not it is shooting at you.
            const float step = std::sin(animPhase_ * 1.5f) * 0.35f;
            draw(lib.apcHull, hull * Mat4::translation(Vec3(0.0f, 1.35f, 0.0f)),
                 Vec3(1.0f, 1.0f, 1.0f));
            for (int sx = -1; sx <= 1; sx += 2)
                for (int sz = -1; sz <= 1; sz += 2)
                    draw(lib.walkLeg,
                         hull * Mat4::translation(Vec3(sx * 0.95f, 0.62f, sz * 0.75f)) *
                             Mat4::rotationX(step * static_cast<float>(sx * sz)),
                         Vec3(1.0f, 1.0f, 1.0f));
            // Two arms sweeping over the deck: the repair rig at work.
            const float sweep = std::sin(animPhase_ * 2.2f) * 0.6f;
            draw(lib.repairArm,
                 hull * Mat4::translation(Vec3(-0.7f, 2.0f, 0.25f)) *
                     Mat4::rotationY(sweep) * Mat4::rotationX(0.5f),
                 kAccent);
            draw(lib.repairArm,
                 hull * Mat4::translation(Vec3(0.7f, 2.0f, 0.25f)) *
                     Mat4::rotationY(-sweep) * Mat4::rotationX(0.5f),
                 kAccent);
            draw(lib.cupola, gun * Mat4::translation(Vec3(0.0f, 2.1f, -0.35f)),
                 Vec3(1.0f, 1.0f, 1.0f));
            if (near)
                draw(lib.drum, hull * Mat4::translation(Vec3(0.0f, 2.1f, -0.95f)) *
                                   Mat4::rotationX(PI * 0.5f),
                     Vec3(1.0f, 1.0f, 1.0f));
            break;
        }
        default: break;
    }
}

} // namespace sb
