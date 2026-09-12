#include "combat.h"

#include "props.h"
#include "units.h"

#include <algorithm>
#include <cmath>

namespace sb {

namespace {

// A round is stepped in substeps small enough that it never tunnels through a
// two-metre-wide machine. At 420 m/s and 60 Hz a single step is seven metres,
// which would skip clean through anything.
constexpr float kMaxStep = 1.1f;

constexpr float kPickupRadius = 3.6f;   // generous: walking NEAR ammo takes it
constexpr int kMaxProjectiles = 512;

} // namespace

bool segmentHitsSphere(const Vec3& a, const Vec3& b, const Vec3& centre,
                       float radius, float* tOut) {
    const Vec3 d = b - a;
    const Vec3 m = a - centre;
    const float dd = dot(d, d);
    if (dd < 1e-8f) {
        if (lengthSq(m) <= radius * radius) { if (tOut) *tOut = 0.0f; return true; }
        return false;
    }
    // Closest approach along the segment, clamped to its ends.
    float t = clampf(-dot(m, d) / dd, 0.0f, 1.0f);
    const Vec3 closest = a + d * t;
    if (lengthSq(closest - centre) <= radius * radius) {
        if (tOut) *tOut = t;
        return true;
    }
    return false;
}

bool leadTarget(const Vec3& shooter, const Vec3& target, const Vec3& targetVel,
                float projectileSpeed, Vec3* aimPointOut) {
    // Solve |target + v*t - shooter| = speed * t for the smallest positive t.
    // This is the quadratic (v.v - s^2) t^2 + 2 (v.r) t + r.r = 0.
    const Vec3 r = target - shooter;
    const float a = dot(targetVel, targetVel) - projectileSpeed * projectileSpeed;
    const float b = 2.0f * dot(targetVel, r);
    const float c = dot(r, r);

    float t = -1.0f;
    if (std::fabs(a) < 1e-4f) {
        // Target closing at exactly projectile speed: the equation goes linear.
        if (std::fabs(b) > 1e-6f) t = -c / b;
    } else {
        const float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) return false;           // it simply outruns the round
        const float sq = std::sqrt(disc);
        const float t0 = (-b - sq) / (2.0f * a);
        const float t1 = (-b + sq) / (2.0f * a);
        // Prefer the earlier of the two positive solutions.
        if (t0 > 0.0f && t1 > 0.0f) t = std::min(t0, t1);
        else t = std::max(t0, t1);
    }
    if (t <= 0.0f || t > 8.0f) return false;
    if (aimPointOut) *aimPointOut = target + targetVel * t;
    return true;
}

// ---------------------------------------------------------------------------

void Combat::reset() {
    shots_.clear();
    effects_.clear();
    pickups_.clear();
}

void Combat::ensureMeshes() const {
    if (meshesBuilt_) return;
    // A tracer is a unit-length sliver along +Z; the draw scales it to the
    // weapon's tracer length and points it down the velocity vector.
    tracerMesh_ = makeBox(Vec3(1.0f, 1.0f, 0.5f), Vec3(1.0f, 1.0f, 1.0f));
    blastMesh_ = makeSphere(1.0f, 6, 8, Vec3(1.0f, 1.0f, 1.0f));
    meshesBuilt_ = true;
}

void Combat::spawnShots(const std::vector<ShotRequest>& shots, Rng& rng) {
    for (const ShotRequest& s : shots) {
        if (!s.weapon) continue;
        const WeaponDef& w = *s.weapon;
        const int pellets = std::max(1, w.pellets);
        for (int p = 0; p < pellets; ++p) {
            if (static_cast<int>(shots_.size()) >= kMaxProjectiles) return;

            // Scatter inside a cone. Two perpendicular offsets applied to the
            // direction is close enough to a proper cone at these angles and
            // costs two random numbers instead of a trig pair.
            Vec3 dir = normalize(s.direction);
            Vec3 side = cross(dir, Vec3(0.0f, 1.0f, 0.0f));
            if (lengthSq(side) < 1e-6f) side = Vec3(1.0f, 0.0f, 0.0f);
            side = normalize(side);
            const Vec3 upv = cross(side, dir);
            const float cone = w.spread * clampf(s.spreadScale, 0.0f, 4.0f);
            const float sx = rng.range(-cone, cone);
            const float sy = rng.range(-cone, cone);
            dir = normalize(dir + side * sx + upv * sy);

            Projectile pr;
            pr.pos = s.origin;
            pr.prev = s.origin;
            pr.vel = dir * w.projectileSpeed;
            pr.gravity = w.gravity;
            // Pellets split the listed damage between them, so a flak burst that
            // lands whole is worth the same as a solid slug of the same rating.
            const float teamScale = (s.team == Team::Player) ? 1.0f : hostileDamage_;
            pr.damage = w.damage * teamScale / static_cast<float>(pellets);
            pr.blastRadius = w.blastRadius * s.blastScale;
            pr.blastDamage = w.blastDamage * teamScale * s.blastScale;
            pr.life = w.range * s.rangeScale / std::max(w.projectileSpeed, 1.0f);
            pr.tracerLength = w.tracerLength;
            pr.tracerRadius = w.tracerRadius;
            pr.color = w.tracerColor;
            pr.team = s.team;
            pr.shooter = s.shooter;
            pr.homing = s.homing;
            shots_.push_back(pr);
        }

        // Muzzle bloom. Heavy guns get a genuinely big flash and a short
        // smoke puff; the flash IS the weight of the shot.
        Effect e;
        e.pos = s.origin;
        e.shooter = s.shooter;
        e.color = w.tracerColor * 1.5f;
        e.radius = 0.20f + w.damage * 0.010f;
        e.growth = 2.6f + w.damage * 0.05f;
        e.life = e.maxLife = 0.075f + w.damage * 0.0006f;
        effects_.push_back(e);
        if (w.damage >= 30.0f) {
            Effect sm;
            sm.shooter = s.shooter;
            sm.pos = s.origin + normalize(s.direction) * 1.2f;
            sm.vel = normalize(s.direction) * 2.0f + Vec3(0.0f, 1.2f, 0.0f);
            sm.color = Vec3(0.34f, 0.33f, 0.30f);
            sm.radius = 0.35f;
            sm.growth = 1.8f;
            sm.life = sm.maxLife = 0.35f;
            effects_.push_back(sm);
        }

        // The report. One per trigger pull, whoever pulled it.
        if (pendingFire_.size() < 96) {
            FireEvent f;
            f.pos = s.origin;
            f.weapon = s.weapon;
            f.team = s.team;
            f.shooter = s.shooter;
            pendingFire_.push_back(f);
        }
    }
}

void Combat::explosion(const Vec3& at, float radius, const Vec3& color, Rng& rng) {
    Effect bloom;
    bloom.pos = at;
    bloom.color = color;
    bloom.radius = radius * 0.35f;
    bloom.growth = radius * 2.4f;
    bloom.life = bloom.maxLife = 0.42f;
    effects_.push_back(bloom);

    const int sparks = 6 + static_cast<int>(radius * 3.0f);
    for (int i = 0; i < sparks; ++i) {
        Effect s;
        s.pos = at;
        s.vel = normalize(Vec3(rng.range(-1.0f, 1.0f), rng.range(-0.2f, 1.0f),
                               rng.range(-1.0f, 1.0f))) * rng.range(4.0f, 13.0f);
        s.color = lerp(color, Vec3(1.0f, 0.92f, 0.60f), rng.unit());
        s.radius = 0.07f;
        s.growth = -0.05f;
        s.gravity = -18.0f;
        s.life = s.maxLife = rng.range(0.25f, 0.7f);
        s.spark = true;
        effects_.push_back(s);
    }
}

bool Combat::interceptOne(const Vec3& center, float radius, Team defender) {
    int best = -1;
    float bestD = radius * radius;
    for (size_t i = 0; i < shots_.size(); ++i) {
        const Projectile& p = shots_[i];
        if (!p.alive || p.team == defender) continue;
        // Only rounds actually closing on the defender are worth a shot.
        if (dot(p.vel, center - p.pos) <= 0.0f) continue;
        const float d = lengthSq(p.pos - center);
        if (d < bestD) { bestD = d; best = static_cast<int>(i); }
    }
    if (best < 0) return false;
    Projectile& p = shots_[static_cast<size_t>(best)];
    p.alive = false;
    // The intercept flash: a sharp little star where the round died.
    Effect e;
    e.pos = p.pos;
    e.color = Vec3(1.0f, 0.9f, 0.55f);
    e.radius = 0.3f;
    e.growth = 3.5f;
    e.life = e.maxLife = 0.12f;
    effects_.push_back(e);
    return true;
}

void Combat::applySplash(const Projectile& p, const Vec3& at,
                         std::vector<Mech*>& mechs, CombatEvents& ev,
                         std::vector<Unit>* units, std::vector<Destructible>* props) {
    if (p.blastRadius <= 0.0f) return;
    for (size_t i = 0; i < mechs.size(); ++i) {
        Mech* m = mechs[i];
        if (!m || !m->alive()) continue;
        const Vec3 to = m->hitCentre() - at;
        const float d = length(to) - m->hitRadius();
        if (d >= p.blastRadius) continue;
        // Linear falloff. Splash ignores team: a rocket at your own feet hurts.
        const float f = 1.0f - clampf(d / p.blastRadius, 0.0f, 1.0f);
        // How much of a blast a machine actually catches depends on how much
        // of it there is. A siege deck is a barn door; a low-profile raider
        // is not. This matters more than it sounds: a fast light frame can
        // dodge aimed fire - the gunners' groups open right up against it -
        // but splash ignores accuracy entirely, so before this the one thing
        // a dodge build could not do anything about was also the thing that
        // killed it. Scaled off a mid-weight hull, so nothing changes for the
        // machines the numbers were tuned against.
        const float bulk = clampf(m->hitRadius() / 2.3f, 0.62f, 1.22f);
        const float dmg = p.blastDamage * f * bulk;
        const bool wasAlive = m->alive();
        m->applyDamage(dmg, normalize(to + Vec3(0.0f, 0.01f, 0.0f)));
        if (m->team() == Team::Player) ev.playerDamageTaken += dmg;
        else if (p.team == Team::Player) ev.playerDamageDealt += dmg;
        if (wasAlive && !m->alive()) ev.destroyed.push_back(static_cast<int>(i));
    }
    // The small war dies in area fire wholesale; that is much of the fun.
    if (units) {
        for (size_t i = 0; i < units->size(); ++i) {
            Unit& u = (*units)[i];
            if (!u.alive()) continue;
            const float d = length(u.hitCentre() - at) - u.hitRadius();
            if (d >= p.blastRadius) continue;
            const float f = 1.0f - clampf(d / p.blastRadius, 0.0f, 1.0f);
            u.applyDamage(p.blastDamage * f);
            if (!u.alive()) ev.unitsKilled.push_back(static_cast<int>(i));
        }
    }
    if (props) {
        for (size_t i = 0; i < props->size(); ++i) {
            Destructible& d2 = (*props)[i];
            if (!d2.alive) continue;
            const float d = length(d2.hitCentre() - at) - d2.hitRadius;
            if (d >= p.blastRadius) continue;
            const float f = 1.0f - clampf(d / p.blastRadius, 0.0f, 1.0f);
            d2.applyDamage(p.blastDamage * f);
            if (!d2.alive) ev.propsKilled.push_back(static_cast<int>(i));
        }
    }
}

void Combat::impact(Projectile& p, const Vec3& at, const Vec3& normal,
                    const World& world, std::vector<Mech*>& mechs,
                    CombatEvents& ev, std::vector<Unit>* units,
                    std::vector<Destructible>* props, int surface) {
    (void)world;
    p.alive = false;
    if (ev.impacts.size() < 96) {
        ImpactEvent ie;
        ie.pos = at;
        ie.surface = (p.blastRadius > 0.0f) ? 2 : surface;
        ie.energy = (p.blastRadius > 0.0f)
                        ? clampf(p.blastRadius / 6.0f, 0.4f, 2.0f)
                        : clampf(p.damage / 18.0f, 0.25f, 1.5f);
        ev.impacts.push_back(ie);
    }
    if (p.blastRadius > 0.0f) {
        explosion(at, p.blastRadius, p.color, rng_);
        applySplash(p, at, mechs, ev, units, props);
    } else {
        // A small directional spark spray off the surface.
        Effect bloom;
        bloom.pos = at + normal * 0.05f;
        bloom.color = p.color;
        bloom.radius = 0.10f;
        bloom.growth = 1.1f;
        bloom.life = bloom.maxLife = 0.14f;
        effects_.push_back(bloom);
        for (int i = 0; i < 3; ++i) {
            Effect s;
            s.pos = at;
            s.vel = normalize(normal + Vec3(rng_.range(-0.7f, 0.7f), rng_.range(-0.3f, 0.7f),
                                            rng_.range(-0.7f, 0.7f))) * rng_.range(3.0f, 8.0f);
            s.color = lerp(p.color, Vec3(1.0f, 0.95f, 0.7f), 0.5f);
            s.radius = 0.05f;
            s.gravity = -18.0f;
            s.life = s.maxLife = rng_.range(0.12f, 0.32f);
            s.spark = true;
            effects_.push_back(s);
        }
    }
}

void Combat::update(float dt, const World& world, std::vector<Mech*>& mechs,
                    CombatEvents& events, std::vector<Unit>* units,
                    std::vector<Destructible>* props) {
    // Hand the frame's shot reports to whoever is listening.
    events.fired.insert(events.fired.end(), pendingFire_.begin(), pendingFire_.end());
    pendingFire_.clear();
    for (Projectile& p : shots_) {
        if (!p.alive) continue;
        p.life -= dt;
        if (p.life <= 0.0f) { p.alive = false; continue; }

        // Warhead rounds drag a smoke trail. Weight you can see: a rocket is
        // a grey line across the sky, a mortar bomb an arc of it.
        if (p.blastRadius > 0.5f && effects_.size() < 700 &&
            (rng_.unit() < clampf(dt * 40.0f, 0.0f, 1.0f))) {
            Effect s;
            s.pos = p.pos;
            s.vel = Vec3(rng_.range(-0.4f, 0.4f), rng_.range(0.2f, 0.9f),
                         rng_.range(-0.4f, 0.4f));
            s.color = Vec3(0.30f, 0.30f, 0.32f);
            s.radius = 0.16f + p.blastRadius * 0.02f;
            s.growth = 0.9f;
            s.life = s.maxLife = 0.55f;
            effects_.push_back(s);
        }

        // Substep so fast rounds cannot tunnel.
        const float travel = length(p.vel) * dt;
        const int steps = std::max(1, std::min(8, static_cast<int>(travel / kMaxStep) + 1));
        const float sdt = dt / static_cast<float>(steps);

        // Guided rounds bend toward the nearest thing they hate. The turn
        // rate is finite, so a missile can still be sidestepped up close and
        // a launch pointed away never comes around fully.
        if (p.homing > 0.0f) {
            Vec3 best(0.0f);
            float bestD = 140.0f * 140.0f;
            bool have = false;
            for (const Mech* m : mechs) {
                if (!m || !m->alive() || m->team() == p.team) continue;
                const float d = lengthSq(m->hitCentre() - p.pos);
                if (d < bestD) { bestD = d; best = m->hitCentre(); have = true; }
            }
            if (units) {
                for (const Unit& u : *units) {
                    if (!u.alive() || u.team() == p.team) continue;
                    const float d = lengthSq(u.hitCentre() - p.pos);
                    if (d < bestD) { bestD = d; best = u.hitCentre(); have = true; }
                }
            }
            if (have) {
                const float speed = length(p.vel);
                const Vec3 want = normalize(best - p.pos);
                const Vec3 cur = normalize(p.vel);
                const Vec3 steered = normalize(lerp(cur, want,
                                                    clampf(p.homing * dt, 0.0f, 0.5f)));
                p.vel = steered * speed;
            }
        }

        for (int s = 0; s < steps && p.alive; ++s) {
            p.prev = p.pos;
            p.vel.y += p.gravity * sdt;
            p.pos += p.vel * sdt;

            // Mechs first: a round that would clip a wall behind a machine
            // should still count as a hit on the machine.
            float bestT = 2.0f;
            int bestMech = -1;
            for (size_t i = 0; i < mechs.size(); ++i) {
                Mech* m = mechs[i];
                if (!m || !m->alive()) continue;
                if (m->team() == p.team) continue;         // no friendly fire on direct hits
                if (static_cast<int>(i) == p.shooter) continue;
                float t;
                if (segmentHitsSphere(p.prev, p.pos, m->hitCentre(), m->hitRadius(), &t) &&
                    t < bestT) {
                    bestT = t;
                    bestMech = static_cast<int>(i);
                }
            }
            // The small war, along the same segment.
            int bestUnit = -1;
            if (units) {
                for (size_t i = 0; i < units->size(); ++i) {
                    Unit& u = (*units)[i];
                    if (!u.alive()) continue;
                    if (u.team() == p.team) continue;
                    if (p.shooter == -100 - static_cast<int>(i)) continue;
                    float t;
                    if (segmentHitsSphere(p.prev, p.pos, u.hitCentre(), u.hitRadius(), &t) &&
                        t < bestT) {
                        bestT = t;
                        bestUnit = static_cast<int>(i);
                        bestMech = -1;
                    }
                }
            }
            // Destructibles have no team; anyone's fire wrecks them.
            int bestProp = -1;
            if (props) {
                for (size_t i = 0; i < props->size(); ++i) {
                    Destructible& d = (*props)[i];
                    if (!d.alive) continue;
                    float t;
                    if (segmentHitsSphere(p.prev, p.pos, d.hitCentre(), d.hitRadius, &t) &&
                        t < bestT) {
                        bestT = t;
                        bestProp = static_cast<int>(i);
                        bestUnit = -1;
                        bestMech = -1;
                    }
                }
            }

            // Then the world, along the same sub-segment.
            const Vec3 seg = p.pos - p.prev;
            const float segLen = length(seg);
            SurfaceHit wh;
            if (segLen > 1e-5f)
                wh = world.raycast(p.prev, seg / segLen, segLen);

            const float worldT = wh.hit && segLen > 1e-5f ? (wh.distance / segLen) : 2.0f;

            if (bestMech >= 0 && bestT <= worldT) {
                Mech* m = mechs[bestMech];
                const Vec3 at = lerp(p.prev, p.pos, bestT);
                const bool wasAlive = m->alive();
                m->applyDamage(p.damage, normalize(p.vel));
                if (m->team() == Team::Player) events.playerDamageTaken += p.damage;
                else if (p.team == Team::Player) events.playerDamageDealt += p.damage;
                if (wasAlive && !m->alive()) events.destroyed.push_back(bestMech);
                impact(p, at, -normalize(p.vel), world, mechs, events, units, props, 1);
            } else if (bestUnit >= 0 && bestT <= worldT) {
                Unit& u = (*units)[bestUnit];
                const Vec3 at = lerp(p.prev, p.pos, bestT);
                u.applyDamage(p.damage);
                if (p.team == Team::Player) events.playerDamageDealt += p.damage;
                if (!u.alive()) events.unitsKilled.push_back(bestUnit);
                impact(p, at, -normalize(p.vel), world, mechs, events, units, props, 1);
            } else if (bestProp >= 0 && bestT <= worldT) {
                Destructible& d = (*props)[bestProp];
                const Vec3 at = lerp(p.prev, p.pos, bestT);
                d.applyDamage(p.damage);
                if (!d.alive) events.propsKilled.push_back(bestProp);
                impact(p, at, -normalize(p.vel), world, mechs, events, units, props, 1);
            } else if (wh.hit) {
                // A destructible's blocking volume IS the destructible: a shot
                // that lands on the box damages the prop. Without this, the
                // hit-sphere was a bullseye inside a crate the collision box
                // caught first, and props soaked fire while never dying.
                if (props && wh.obstacle >= 0) {
                    for (size_t i = 0; i < props->size(); ++i) {
                        Destructible& d = (*props)[i];
                        if (!d.alive || d.obstacle != wh.obstacle) continue;
                        d.applyDamage(p.damage);
                        if (!d.alive) events.propsKilled.push_back(static_cast<int>(i));
                        break;
                    }
                }
                impact(p, wh.point, wh.normal, world, mechs, events, units, props,
                       wh.kind != ObstacleKind::Terrain ? 1 : 0);
            }
        }
    }

    shots_.erase(std::remove_if(shots_.begin(), shots_.end(),
                                [](const Projectile& p) { return !p.alive; }),
                 shots_.end());

    // Effects.
    for (Effect& e : effects_) {
        e.life -= dt;
        e.vel.y += e.gravity * dt;
        e.pos += e.vel * dt;
        e.radius = std::max(0.01f, e.radius + e.growth * dt);
    }
    effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
                                  [](const Effect& e) { return e.life <= 0.0f; }),
                   effects_.end());

    updatePickups(dt, mechs, events);
}

void Combat::updatePickups(float dt, std::vector<Mech*>& mechs, CombatEvents& ev) {
    for (AmmoPickup& pk : pickups_) {
        if (pk.taken) continue;
        pk.bob += dt * 2.4f;
        pk.life -= dt;
        if (pk.life <= 0.0f) { pk.taken = true; continue; }

        // Only the player picks things up; enemies come with what they carry.
        for (Mech* m : mechs) {
            if (!m || !m->alive() || m->team() != Team::Player) continue;
            if (lengthSq(m->position() - pk.pos) > kPickupRadius * kPickupRadius) continue;
            bool used = false;
            if (pk.isRepair()) {
                // Only worth taking if there is damage to undo - otherwise
                // leave it standing for the way back.
                if (m->healthFraction() < 0.995f) {
                    // A salvage rig takes far more out of a wreck than a
                    // gun frame does. This is the utility hull's whole
                    // argument: it is the machine that can afford to fight
                    // its way through rather than run past.
                    const float mul =
                        (m->stats().trait == Trait::Scavenger) ? 1.85f : 1.0f;
                    m->repairStructure(pk.structure * mul);
                    used = true;
                    ev.repairsCollected += 1;
                }
            } else {
                for (MountedWeapon& w : m->weapons()) {
                    if (!w.part || w.part->id != pk.weaponId) continue;
                    w.reserve += (m->stats().trait == Trait::Scavenger)
                                     ? pk.rounds * 2 : pk.rounds;
                    used = true;
                    break;
                }
            }
            if (used) {
                pk.taken = true;
                if (!pk.isRepair()) ev.pickupsCollected += 1;
                Effect e;
                e.pos = pk.pos;
                e.color = Vec3(0.5f, 1.0f, 0.7f);
                e.radius = 0.3f;
                e.growth = 3.0f;
                e.life = e.maxLife = 0.3f;
                effects_.push_back(e);
            }
            break;
        }
    }
    pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(),
                                  [](const AmmoPickup& p) { return p.taken; }),
                   pickups_.end());
}

void Combat::addRepair(const Vec3& pos, float structure) {
    if (structure <= 0.0f) return;
    AmmoPickup pk;
    pk.pos = pos + Vec3(0.0f, 0.45f, 0.0f);
    pk.structure = structure;
    pk.life = 150.0f;               // repairs wait longer than ammunition
    pickups_.push_back(pk);
}

void Combat::addPickup(const Vec3& pos, const std::string& weaponId, int rounds) {
    if (rounds <= 0 || weaponId.empty()) return;
    AmmoPickup pk;
    pk.pos = pos;
    pk.weaponId = weaponId;
    pk.rounds = rounds;
    pk.bob = rng_.range(0.0f, 6.28f);
    pickups_.push_back(pk);
}

void Combat::dropSalvage(const Mech& victim, Rng& rng) {
    // A destroyed machine leaves whatever heavy ammunition it was still holding,
    // plus a share of a fresh magazine. Clearing the field of a heavy squad is
    // how you keep a limited-ammo weapon fed without paying for it.
    for (const MountedWeapon& w : victim.weapons()) {
        if (!w.part) continue;
        if (w.part->weapon.ammo != AmmoKind::Limited) continue;
        const int carried = w.rounds + w.reserve;
        const int drop = std::max(1, carried / 2 + w.part->weapon.magazine / 4);
        const Vec3 at = victim.position() +
                        Vec3(rng.range(-1.6f, 1.6f), 0.6f, rng.range(-1.6f, 1.6f));
        addPickup(at, w.part->id, drop);
    }
}

int Combat::liveProjectiles() const { return static_cast<int>(shots_.size()); }

void Combat::submit(Rasterizer& raster, const Vec3& viewPos, float eyeRadius) const {
    ensureMeshes();

    // How much of a player-owned thing to draw at this distance from the eye
    // in first person: nothing inside a third of the radius, ramping to full
    // at the radius. Measured against the ROUND, so a burst that has left
    // the machine comes into view as it goes, which is the tracer doing its
    // job (showing where the fire is going) without doing it in your face.
    auto ownScale = [&](const Vec3& at, int shooter) {
        if (eyeRadius <= 0.0f || shooter != 0) return 1.0f;
        const float d = length(at - viewPos);
        return clampf((d - eyeRadius * 0.33f) / (eyeRadius * 0.67f), 0.0f, 1.0f);
    };

    for (const Projectile& p : shots_) {
        const float sp = length(p.vel);
        if (sp < 1e-3f) continue;
        const float own = ownScale(p.pos, p.shooter);
        if (own <= 0.02f) continue;
        const Vec3 dir = p.vel / sp;
        // Tracers stretch with speed: a slow plasma bolt is a ball, a lance
        // round is a streak. That difference is the visual cue for how much
        // you have to lead.
        const float len = p.tracerLength * clampf(sp / 200.0f, 0.35f, 1.8f) * (0.4f + 0.6f * own);

        Vec3 up(0.0f, 1.0f, 0.0f);
        if (std::fabs(dot(dir, up)) > 0.98f) up = Vec3(1.0f, 0.0f, 0.0f);
        const Vec3 right = normalize(cross(up, dir));
        const Vec3 realUp = cross(dir, right);

        DrawItem it;
        it.mesh = &tracerMesh_;
        it.model = Mat4::translation(p.pos) *
                   Mat4::basis(right, realUp, dir) *
                   Mat4::scaling(Vec3(p.tracerRadius * (0.35f + 0.65f * own),
                                      p.tracerRadius * (0.35f + 0.65f * own), len));
        it.tint = p.color * (0.45f + 0.55f * own);
        it.emissive = 1.0f;
        it.rim = 0.0f;
        raster.submit(it);
    }

    for (const Effect& e : effects_) {
        const float t = clampf(e.life / std::max(e.maxLife, 1e-4f), 0.0f, 1.0f);
        const float own = ownScale(e.pos, e.shooter);
        if (own <= 0.02f) continue;
        DrawItem it;
        it.mesh = &blastMesh_;
        it.model = Mat4::translation(e.pos) * Mat4::scaling(Vec3(e.radius * (0.3f + 0.7f * own)));
        // Fades by dimming rather than by alpha: the rasterizer is opaque-only,
        // and against the ASCII ramp a dimming ball reads as a dying flash.
        it.tint = e.color * (0.25f + 0.75f * t) * (0.5f + 0.5f * own);
        it.emissive = 1.0f;
        it.rim = 0.0f;
        raster.submit(it);
    }

    for (const AmmoPickup& pk : pickups_) {
        const float bob = std::sin(pk.bob) * 0.14f;
        // Blink out as it nears despawn so the player knows it is going.
        const float fade = pk.life < 8.0f ? (0.35f + 0.65f * (0.5f + 0.5f * std::sin(pk.life * 9.0f)))
                                          : 1.0f;
        DrawItem it;
        it.mesh = &blastMesh_;
        it.model = Mat4::translation(pk.pos + Vec3(0.0f, bob, 0.0f)) *
                   Mat4::rotationY(pk.bob * 0.5f) *
                   Mat4::scaling(Vec3(0.30f, 0.42f, 0.30f));
        // Ammunition is green; a repair crate is a warm amber cross, so at a
        // glance you know whether that thing on the ground is rounds or hull.
        it.tint = (pk.isRepair() ? Vec3(1.0f, 0.72f, 0.30f)
                                 : Vec3(0.45f, 1.0f, 0.60f)) * fade;
        it.emissive = 0.85f;
        it.rim = 1.0f;
        raster.submit(it);
        if (pk.isRepair()) {
            // A second bar across it: the salvage cross.
            it.model = Mat4::translation(pk.pos + Vec3(0.0f, bob, 0.0f)) *
                       Mat4::rotationY(pk.bob * 0.5f) *
                       Mat4::scaling(Vec3(0.46f, 0.16f, 0.30f));
            raster.submit(it);
        }
    }

    (void)viewPos;
}

} // namespace sb
