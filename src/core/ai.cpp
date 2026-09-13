#include "ai.h"

#include <algorithm>
#include <cmath>

namespace sb {

const char* archetypeName(Archetype a) {
    switch (a) {
        case Archetype::Brawler:    return "BRAWLER";
        case Archetype::Skirmisher: return "SKIRMISHER";
        case Archetype::Sniper:     return "SNIPER";
        case Archetype::Lancer:     return "LANCER";
        case Archetype::WallRunner: return "WALL-RUNNER";
        case Archetype::Sieger:     return "SIEGER";
        case Archetype::Stalker:    return "STALKER";
        default:                    return "UNKNOWN";
    }
}

AiConfig configFor(Archetype a, float difficulty, Rng& rng) {
    AiConfig c;
    c.archetype = a;
    // Difficulty sharpens the shooting and shortens the thinking, but never
    // makes an enemy perfect: a ceiling on accuracy keeps late levels about
    // volume of fire rather than unavoidable hits.
    const float d = clampf(difficulty, 0.0f, 1.0f);
    c.accuracy = clampf(0.50f + d * 0.34f, 0.0f, 0.88f);
    // Sharper reactions and a harder push than the original numbers. Enemies
    // that hang back at their preferred range and pause between bursts read as
    // passive; the fight only feels dangerous when they close.
    c.reactionTime = lerpf(0.40f, 0.12f, d);
    c.aggression = lerpf(1.05f, 1.55f, d);
    c.strafeBias = 1.0f;

    switch (a) {
        case Archetype::Brawler:
            c.preferredRange = 26.0f; c.rangeTolerance = 9.0f;
            c.aggression *= 1.25f;
            break;
        case Archetype::Skirmisher:
            c.preferredRange = 44.0f; c.rangeTolerance = 13.0f;
            c.strafeBias = 1.6f;
            break;
        case Archetype::Sniper:
            c.preferredRange = 95.0f; c.rangeTolerance = 24.0f;
            c.strafeBias = 0.35f;
            c.accuracy = clampf(c.accuracy + 0.05f, 0.0f, 0.96f);
            c.aimSpread = 0.055f;
            break;
        case Archetype::Lancer:
            c.preferredRange = 34.0f; c.rangeTolerance = 18.0f;
            c.strafeBias = 1.9f;
            c.aggression *= 1.15f;
            break;
        case Archetype::WallRunner:
            c.preferredRange = 40.0f; c.rangeTolerance = 16.0f;
            c.strafeBias = 1.3f;
            c.willClimb = true;
            break;
        case Archetype::Sieger:
            c.preferredRange = 58.0f; c.rangeTolerance = 20.0f;
            c.strafeBias = 0.15f;
            c.aggression *= 0.85f;
            break;
        case Archetype::Stalker:
            // A duellist, not a line unit. It wants to be a long way off and
            // high up, it shoots well, and it will not stand still to be shot
            // back at: a few seconds in the open and it is gone over the
            // parapet. The aggression is low because aggression here means
            // "walks towards you", which is the one thing it must not do.
            c.preferredRange = 105.0f; c.rangeTolerance = 38.0f;
            c.strafeBias = 0.8f;
            c.aggression *= 0.70f;
            // It is a MARKSMAN. The generic gunnery model scatters by a
            // sixth of a metre per metre of range, which at the two hundred
            // metres this thing likes is a thirty-metre group - wider than
            // the cone its own trigger gate will open for, so the old
            // stalker stood on its roof holding fire and, when it did shoot,
            // could not have hit. A tenth of that, and a floor under the
            // hand, is the difference between a sniper and a scarecrow.
            c.accuracy = clampf(std::max(c.accuracy + 0.10f, 0.80f), 0.0f, 0.94f);
            c.aimSpread = 0.018f;
            c.reactionTime *= 0.75f;
            c.willClimb = true;
            c.exposureLimit = lerpf(4.6f, 2.8f, d);
            break;
        default: break;
    }
    // A little per-machine variation so a wave of the same type does not move
    // as one block.
    c.preferredRange *= rng.range(0.88f, 1.14f);
    c.accuracy = clampf(c.accuracy * rng.range(0.93f, 1.05f), 0.0f, 0.96f);
    return c;
}

void AiController::init(const AiConfig& cfg, uint32_t seed) {
    cfg_ = cfg;
    rng_ = Rng(seed ? seed : 1u);
    state_ = AiState::Idle;
    strafeSign_ = rng_.unit() < 0.5f ? -1.0f : 1.0f;
    strafeTimer_ = rng_.range(1.5f, 4.0f);
    decisionTimer_ = rng_.range(0.0f, cfg_.reactionTime);
    wander_ = normalize(Vec3(rng_.range(-1.0f, 1.0f), 0.0f, rng_.range(-1.0f, 1.0f)));
    bearing_ = rng_.range(0.0f, TAU);
    bearingDrift_ = rng_.range(0.18f, 0.42f) * (rng_.unit() < 0.5f ? -1.0f : 1.0f);
}

Vec3 AiController::pickClimbTarget(const World& world, const Mech& self,
                                   const Mech& target) {
    // Look for a climbable face on a structure roughly between us and the
    // target, and aim for its base. The mech's own wall-intent logic takes over
    // once it walks into the surface, so all the brain has to do is choose a
    // wall and press against it.
    const std::vector<Obstacle>& obs = world.obstacles();
    float best = 1e9f;
    Vec3 bestPoint = self.position();
    bool found = false;

    const bool stalker = (cfg_.archetype == Archetype::Stalker);
    for (const Obstacle& o : obs) {
        if (!o.climbable || !o.active) continue;
        // Nothing pole-shaped, whatever it is flagged as: a climb needs a
        // FACE the feet can spread across, and a perch needs deck.
        // (Buildings are hollow: their walls are half-metre slabs, so
        // thinness alone says nothing. A pole is thin BOTH ways.)
        if (o.kind == ObstacleKind::Pillar || std::max(o.half.x, o.half.z) < 2.0f) continue;
        const float h = o.half.y * 2.0f;
        // A stalker wants a TOWER, not a garden wall: it is going up there to
        // shoot over a district, and a six-metre block gives it neither the
        // sightline nor the cover the whole behaviour depends on.
        if (h < (stalker ? 10.0f : 6.0f)) continue;
        // ...and not a skyscraper either. A gun that only depresses 38
        // degrees cannot reach the street from fifty metres up, and neither
        // can the pilot get back DOWN from there without a fall that costs
        // more structure than the machine has: measured, a stalker that
        // picked a fifty-metre tower died of its own descent one fight in
        // four. Ten to thirty-four metres is the band where the behaviour
        // actually works.
        if (stalker && h > 34.0f) continue;
        // And the same limit measured from the GROUND, because a thirty-metre
        // block standing on a twenty-metre podium is a fifty-metre fall like
        // any other, and the machine will happily hop from roof to roof until
        // it is somewhere it cannot come down from.
        if (stalker && (o.center.y + o.half.y) -
                           world.terrain().height(o.center.x, o.center.z) > 36.0f)
            continue;
        const float dSelf = lengthXZ(o.center - self.position());
        if (dSelf > (stalker ? 160.0f : 70.0f)) continue;
        const float dTarget = lengthXZ(o.center - target.position());
        // Prefer a structure near the target: getting above them is the point.
        float score = dSelf * 0.6f + dTarget * 1.4f;
        if (stalker) {
            // Different priorities entirely. Height is the prize; distance
            // from the player is a virtue rather than a cost, because the
            // stalker shoots from a hundred metres and does not want to be
            // stood on top of the thing it is shooting at; and the roof it
            // just left is the one place it will not go back to, because a
            // sniper who returns to the same window is a sniper you have
            // already killed.
            // Height is a means, not an end. The turret only depresses 38
            // degrees, so from the top of a fifty-metre tower the street
            // below is literally unshootable - the machine perches
            // magnificently and never fires a round. What it wants is a
            // firing position: high enough to see over the district, low
            // enough that the gun still reaches the ground at the range it
            // likes to shoot from. About twenty-five metres, and a good long
            // way off.
            score = dSelf * 0.7f + std::fabs(h - 25.0f) * 2.2f -
                    clampf(dTarget, 0.0f, 150.0f) * 0.75f;
            if (hasLastPerch_ && lengthXZ(o.center - lastPerch_) < 22.0f)
                score += 900.0f;
        }
        // A face this machine already failed to get up is not tried again
        // this fight. Whatever the reason - a ledge it could not read, a
        // wall it kept peeling off - repeating the attempt is the behaviour
        // the player reported as "ignores me and scrabbles at a wall".
        if (hasFailedClimb_ && lengthXZ(o.center - failedClimb_) < 16.0f)
            score += 900.0f;
        if (score < best) {
            best = score;
            // Approach the face that looks at us.
            const Vec3 toSelf = normalize(flattenY(self.position() - o.center));
            bestPoint = o.center + toSelf * (std::max(o.half.x, o.half.z) + 1.2f);
            bestPoint.y = o.center.y - o.half.y;
            climbCentre_ = o.center;
            found = true;
        }
    }
    hasClimbTarget_ = found;
    return bestPoint;
}

Vec3 AiController::pickBreakPoint(const World& world, const Mech& self,
                                 const Mech& target) {
    // Where to run when you have been seen. The answer is: behind something,
    // away from the shooter. Sample bearings that lead away from the target
    // and score them by how much solid geometry ends up between the two of
    // you - which is exactly what "taking cover" means and is cheap to
    // measure with the same solid test everything else uses.
    const Vec3 selfPos = self.position();
    const Vec3 away = normalize(flattenY(selfPos - target.position()) +
                                Vec3(0.0f, 0.0f, 1e-4f));
    Vec3 best = selfPos + away * 30.0f;
    float bestScore = -1e9f;
    for (int i = 0; i < 12; ++i) {
        const float a2 = (static_cast<float>(i) / 12.0f) * TAU;
        const Vec3 dir(std::cos(a2), 0.0f, std::sin(a2));
        if (dot(dir, away) < -0.25f) continue;          // never towards the gun
        // Far enough to be a real disengagement. Thirty metres is a sidestep:
        // the player simply keeps walking and is on top of the machine again
        // before it has found a wall.
        const float reach = 58.0f + 38.0f * rng_.unit();
        Vec3 cand = selfPos + dir * reach;
        cand.y = world.terrain().height(cand.x, cand.z) + 2.0f;
        if (world.insideSolid(cand, 2.0f)) continue;
        // How much wall is on the line from the target to that spot.
        int blocked = 0;
        for (int k = 1; k <= 16; ++k) {
            const Vec3 p = lerp(target.hitCentre(), cand + Vec3(0.0f, 2.0f, 0.0f),
                                static_cast<float>(k) / 17.0f);
            if (world.insideSolid(p, 0.4f)) ++blocked;
        }
        const float score = static_cast<float>(blocked) * 12.0f +
                            dot(dir, away) * 6.0f + reach * 0.1f;
        if (score > bestScore) { bestScore = score; best = cand; }
    }
    return best;
}

bool AiController::pickFirePosition(const World& world, const Mech& self,
                                    const Mech& target) {
    // Sample a ring of spots around the target at sniping range and score
    // each on the three things a marksman wants: a line to the target from
    // there, a wall a few strides away that BREAKS that line (cover), and
    // not too far a walk from here. A hurt machine pushes the ring out.
    const float hp = self.healthFraction();
    const float rMin = hp < 0.45f ? 190.0f : 140.0f;
    const float rMax = hp < 0.45f ? 300.0f : 240.0f;
    const Vec3 tPos = target.hitCentre();
    const Vec3 selfPos = self.position();
    const float extent = world.extent() - 20.0f;
    float bestScore = -1e9f;
    Vec3 bestFire, bestCover;
    bool found = false;
    const int kCand = 28;
    for (int i = 0; i < kCand; ++i) {
        const float a = rng_.range(0.0f, TAU);
        const float r = rng_.range(rMin, rMax);
        Vec3 c = tPos + Vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
        if (std::fabs(c.x) > extent || std::fabs(c.z) > extent) continue;
        c.y = world.terrain().height(c.x, c.z);
        if (world.hasWater() && c.y < world.waterLevel() + 0.5f) continue;
        if (world.terrain().slope(c.x, c.z) > 0.40f) continue;
        if (world.insideSolid(c + Vec3(0.0f, 2.0f, 0.0f), 2.2f)) continue;
        if (world.insideStructure(c, 1.0f)) continue;
        const Vec3 eye = c + Vec3(0.0f, 2.6f, 0.0f);
        if (!world.lineOfSight(eye, tPos)) continue;      // must be able to shoot
        // Cover: a spot within 14 m from which the target is NOT visible,
        // reachable without crossing the target's line.
        bool cover = false;
        Vec3 coverAt = c;
        for (int k = 0; k < 8 && !cover; ++k) {
            const float ca = (static_cast<float>(k) / 8.0f) * TAU;
            for (float d = 6.0f; d <= 14.0f; d += 4.0f) {
                Vec3 cp = c + Vec3(std::cos(ca) * d, 0.0f, std::sin(ca) * d);
                cp.y = world.terrain().height(cp.x, cp.z);
                if (world.insideSolid(cp + Vec3(0.0f, 2.0f, 0.0f), 2.0f)) continue;
                if (world.insideStructure(cp, 1.0f)) continue;
                if (!world.lineOfSight(cp + Vec3(0.0f, 2.6f, 0.0f), tPos)) { cover = true; coverAt = cp; break; }
            }
        }
        const float walk = lengthXZ(c - selfPos);
        float score = 40.0f + (cover ? 35.0f : 0.0f) - walk * 0.35f;
        // Prefer the far side of the ring when hurt, the middle otherwise.
        score -= std::fabs(r - (hp < 0.45f ? rMax * 0.9f : (rMin + rMax) * 0.5f)) * 0.08f;
        if (hasLastFirePos_ && lengthXZ(c - lastFirePos_) < 60.0f) score -= 30.0f;
        if (score > bestScore) { bestScore = score; bestFire = c; bestCover = coverAt; found = true; }
    }
    if (!found) return false;
    firePos_ = bestFire;
    coverPos_ = bestCover;
    hasFirePos_ = true;
    return true;
}

void AiController::chooseState(const World& world, const Mech& self,
                               const Mech& target, float range, bool visible) {
    const float near = cfg_.preferredRange - cfg_.rangeTolerance;
    const float far = cfg_.preferredRange + cfg_.rangeTolerance;
    const float hp = self.healthFraction();

    // Badly hurt machines back off once, then come back in. They do not flee
    // the level: a fight that ends with the enemy running away is not a fight.
    // A stalker never uses the generic retreat: breaking contact IS its normal
    // behaviour, it does it from cover and from height, and dropping into a
    // straight-line run away from the player throws away every advantage it
    // has. Its own loop below handles being hurt.
    if (cfg_.exposureLimit <= 0.0f &&
        hp < 0.22f && cfg_.aggression < 1.2f && state_ != AiState::Retreat &&
        rng_.unit() < 0.5f) {
        state_ = AiState::Retreat;
        stateTimer_ = rng_.range(1.6f, 3.2f);
        return;
    }
    if (state_ == AiState::Retreat && stateTimer_ > 0.0f) return;

    // How far above us the target is, as an angle. A player who has climbed a
    // building is not "far away", they are *up*, and a machine that only knows
    // about horizontal range will happily stand at the foot of the wall being
    // shot at.
    const Vec3 rel = target.hitCentre() - self.hitCentre();
    const float flat = std::max(lengthXZ(rel), 0.5f);
    const float elevation = std::atan2(rel.y, flat);
    const bool targetIsUp = elevation > deg2rad(38.0f);

    // ---- the stalker's loop ------------------------------------------------
    // A marksman working the district: pick a firing position at long range
    // with a wall to duck behind, walk to it, shoot until either the enemy
    // has been looking at it for a few seconds or a chunk of hull is gone,
    // duck behind the wall, wait out of sight, pick somewhere ELSE. A roof
    // is one kind of firing position when a tower is handy. It is never
    // still for long, and when hurt it opens the range and hides longer.
    // Written before the general rules because the general rules would
    // talk it into standing and trading.
    if (cfg_.exposureLimit > 0.0f) {
        const bool hurt = hp < 0.45f;
        const bool elevated =
            self.position().y >
            world.terrain().height(self.position().x, self.position().z) + 7.0f;
        // Damage taken since the last decision is what a sniper reacts to.
        const float bitten = std::max(0.0f, lastHealth_ - hp);
        lastHealth_ = hp;

        auto leave = [&](float hideFor) {
            hasLastFirePos_ = true;
            lastFirePos_ = self.position();
            lastPerch_ = self.position();
            hasLastPerch_ = true;
            // Duck to the cover picked with the position, or find one now.
            if (!hasFirePos_ || lengthXZ(coverPos_ - self.position()) > 40.0f)
                coverPos_ = pickBreakPoint(world, self, target);
            breakTo_ = coverPos_;
            state_ = AiState::Break;
            stateTimer_ = 7.0f;
            hideTimer_ = hideFor;
            exposure_ = 0.0f;
        };
        auto nextPosition = [&]() {
            // A tower now and then, if one is in reach; otherwise ground.
            // A tower is the POINT of this machine, not a flourish it tries
            // one time in three. It goes up whenever there is anything to go
            // up, and a couple of bad attempts stand it down for a while
            // rather than for the rest of the fight - the old counter never
            // reset outside a successful climb, so two snagged ascents
            // turned the rooftop sniper into a man with a rifle in a street.
            if (self.stats().canClimbWalls && climbFails_ < 3) {
                climbTarget_ = pickClimbTarget(world, self, target);
                if (hasClimbTarget_) {
                    state_ = AiState::Climb;
                    // Twenty-five metres of wall at a third of walking pace
                    // is not an eleven-second job.
                    stateTimer_ = 22.0f;
                    climbStall_ = 0.0f;
                    return;
                }
            }
            if (pickFirePosition(world, self, target)) {
                state_ = AiState::Approach;
                stateTimer_ = 14.0f;
            } else {
                // Nowhere to snipe from: open the range and try again shortly.
                breakTo_ = pickBreakPoint(world, self, target);
                state_ = AiState::Break;
                stateTimer_ = 4.0f;
                hideTimer_ = 1.0f;
            }
        };

        if (state_ == AiState::Idle) {
            // The mission puts this thing on a tower on purpose. Walking
            // straight off it to look for somewhere to shoot from was both
            // the first thing it ever did and a twenty-five metre fall: if
            // it is already up, it is already where it wants to be.
            if (elevated) {
                state_ = AiState::Perch;
                stateTimer_ = 26.0f;
                dwell_ = 0.0f;
                exposure_ = 0.0f;
                arriveHealth_ = hp;
                climbFails_ = 0;
                return;
            }
            nextPosition();
            return;
        }

        if (state_ == AiState::Break) {
            // Running for the wall. Once out of sight (or there), hide.
            const bool there = lengthXZ(breakTo_ - self.position()) < 5.0f;
            if ((!visible && lostSight_ > 0.5f) || there || stateTimer_ <= 0.0f) {
                state_ = AiState::Retreat;
                stateTimer_ = hideTimer_ * (hurt ? 1.8f : 1.0f);
            }
            return;
        }
        if (state_ == AiState::Retreat) {
            // Hiding. If the enemy walks round the wall, keep going; when the
            // wait is up, set up again somewhere else.
            if (visible && range < 60.0f) {
                breakTo_ = pickBreakPoint(world, self, target);
                stateTimer_ = std::max(stateTimer_, 2.5f);
                state_ = AiState::Break;
                hideTimer_ = 2.0f;
                return;
            }
            // Seen from further off while "hiding": the wall is no wall.
            // Give up on it and set up again somewhere else.
            if (visible && lostSight_ < 0.2f && stateTimer_ < hideTimer_ * 0.7f) { nextPosition(); return; }
            if (stateTimer_ <= 0.0f) nextPosition();
            return;
        }
        if (state_ == AiState::Approach) {
            // Walking to the firing position. Arrive -> Engage. Caught in
            // the open on the way (hit hard, or the enemy close) -> leave.
            if (lengthXZ(firePos_ - self.position()) < 6.0f) {
                state_ = AiState::Engage;
                stateTimer_ = 30.0f;
                dwell_ = 0.0f;
                exposure_ = 0.0f;
                arriveHealth_ = hp;
                return;
            }
            if (range < 38.0f || bitten > 0.10f || stateTimer_ <= 0.0f) { leave(2.0f); return; }
            return;
        }
        if (state_ == AiState::Engage) {
            // Shooting from the position. Exposure counts while the enemy
            // can see it; a bite of more than 12% of the hull since arriving
            // ends the stay at once; and nothing holds a spot past 12 s.
            dwell_ += decisionDt_;
            exposure_ += visible ? decisionDt_ : -(decisionDt_) * 0.5f;
            exposure_ = std::max(exposure_, 0.0f);
            const bool bled = (arriveHealth_ - hp) > 0.12f;
            // Close enough that the enemy's own guns are accurate. Further
            // out it is winning the exchange and should keep shooting.
            const bool closing = range < 55.0f;
            if (exposure_ > cfg_.exposureLimit + 2.0f || bled || closing || dwell_ > 12.0f) {
                leave(hurt ? 4.0f : rng_.range(1.5f, 3.0f));
                return;
            }
            return;
        }

        if (state_ == AiState::Perch) {
            // Being SEEN from four hundred metres is not exposure - it is the
            // job. What costs a sniper its position is being seen from
            // somewhere that can shoot back, and the old rule counted every
            // second of eye contact at any distance, so the machine spent
            // the whole fight climbing down off towers it had just climbed.
            const bool answerable = visible && range < cfg_.preferredRange * 1.1f;
            exposure_ += answerable ? decisionDt_ : -(decisionDt_);
            exposure_ = std::max(exposure_, 0.0f);
            dwell_ += decisionDt_;
            const Vec3 down = target.hitCentre() - self.hitCentre();
            const float depress = -std::atan2(down.y, std::max(lengthXZ(down), 1.0f));
            const bool cornered = range < 26.0f || depress > deg2rad(34.0f);
            const bool bled = (arriveHealth_ - hp) > 0.15f;
            // ...and a perch it is not being hurt on is worth holding for a
            // good deal longer than a spot in the street.
            if (exposure_ > cfg_.exposureLimit || cornered || !elevated || bled || dwell_ > 34.0f) {
                lastPerch_ = self.position();
                hasLastPerch_ = true;
                breakTo_ = pickBreakPoint(world, self, target);
                state_ = AiState::Break;
                stateTimer_ = 5.5f;
                hideTimer_ = 2.0f;
                return;
            }
            return;
        }

        if (state_ == AiState::Climb) {
            if (elevated && !self.onWall()) {
                state_ = AiState::Perch;
                exposure_ = 0.0f;
                dwell_ = 0.0f;
                arriveHealth_ = hp;
                stateTimer_ = 20.0f;
                climbFails_ = 0;
                return;
            }
            const float climbed = self.position().y -
                                  world.terrain().height(self.position().x,
                                                         self.position().z);
            const bool atFace = lengthXZ(climbTarget_ - self.position()) < 6.0f;
            if (atFace) climbStall_ += decisionDt_;
            else climbStall_ = 0.0f;
            const bool started = climbed > 4.0f;
            if ((climbStall_ > 3.0f && climbed < 3.5f && !self.onWall()) ||
                (range < 18.0f && !elevated && !started) || stateTimer_ <= 0.0f) {
                failedClimb_ = climbCentre_;
                hasFailedClimb_ = true;
                climbStall_ = 0.0f;
                ++climbFails_;
                leave(2.0f);
                return;
            }
            return;
        }
        // Any other state (a generic Retreat from the hurt rule, etc.): reset.
        nextPosition();
        return;
    }

    // Anything that can hold a wall goes after a climber, whether or not it is
    // a designated wall specialist.
    const bool canFollowUp = self.stats().canClimbWalls;
    if ((cfg_.willClimb || targetIsUp) && canFollowUp && !self.onWall()) {
        // Climb when the target is hard to reach on foot, above us, or just
        // periodically to keep the threat interesting.
        const bool wants = targetIsUp || !visible || range > far * 1.2f;
        if (wants && state_ != AiState::Climb &&
            rng_.unit() < (targetIsUp ? 0.9f : 0.55f)) {
            climbTarget_ = pickClimbTarget(world, self, target);
            if (hasClimbTarget_) {
                state_ = AiState::Climb;
                stateTimer_ = 8.0f;
                return;
            }
        }
    }
    if (state_ == AiState::Climb) {
        // Done climbing once we are actually on a wall with a clear shot, or
        // once the attempt has taken too long.
        if ((self.onWall() && visible) || stateTimer_ <= 0.0f) state_ = AiState::Engage;
        else return;
    }

    // Standing directly under a climber is the worst place to be: the turret
    // cannot elevate that far and the shot is blocked by the wall. Back off
    // until the angle is shootable.
    if (targetIsUp && !self.onWall()) {
        state_ = AiState::Reposition;
        return;
    }

    if (!visible) {
        state_ = AiState::Approach;
    } else if (range > far) {
        state_ = AiState::Approach;
    } else if (range < near) {
        state_ = (cfg_.archetype == Archetype::Sieger) ? AiState::Engage
                                                       : AiState::Reposition;
    } else {
        state_ = AiState::Engage;
    }
}

MechInput AiController::think(float dt, const World& world, const Mech& self,
                              const Mech* targetPtr) {
    MechInput in;
    stateTimer_ = std::max(0.0f, stateTimer_ - dt);
    strafeTimer_ -= dt;
    decisionTimer_ -= dt;
    fireHold_ = std::max(0.0f, fireHold_ - dt);

    if (!targetPtr || !targetPtr->alive() || !self.alive()) {
        state_ = AiState::Idle;
        // Idle machines drift so a level never looks frozen.
        if (strafeTimer_ <= 0.0f) {
            wander_ = normalize(Vec3(rng_.range(-1.0f, 1.0f), 0.0f, rng_.range(-1.0f, 1.0f)));
            strafeTimer_ = rng_.range(2.0f, 5.0f);
        }
        in.moveWorld = wander_;
        in.throttle = 0.25f;
        in.aimPoint = self.position() + self.forward() * 30.0f;
        return in;
    }

    const Mech& target = *targetPtr;
    const Vec3 selfPos = self.position();
    const Vec3 targetPos = target.hitCentre();
    const Vec3 toTarget = targetPos - selfPos;
    const float range = length(toTarget);
    const Vec3 dirFlat = normalize(flattenY(toTarget) + Vec3(0.0f, 0.0f, 1e-4f));

    // Enemy machines read the target's signature the same way the small units
    // do: a spidertank running dark drops off their sensors at a fraction of
    // the usual range, and they fall back on its last known position - which
    // is exactly the window a repositioning build is buying.
    const bool inSensorRange =
        range < self.stats().sensorRange * target.signature();
    const bool visible = inSensorRange && world.lineOfSight(self.hitCentre(), targetPos);
    if (visible) { lastKnown_ = targetPos; lostSight_ = 0.0f; haveSeen_ = true; }
    else lostSight_ += dt;
    // A machine that has never laid eyes on its target has no "last known
    // position" to walk to. Without this it heads for the default - the world
    // origin - which is often exactly where it is already standing, and the
    // fight never starts.
    if (!haveSeen_) lastKnown_ = targetPos;

    sinceDecision_ += dt;
    if (hunting_ && !isStalker()) {
        // Hunting overrides everything: walk at the target and keep walking.
        state_ = AiState::Approach;
        decisionTimer_ = 0.25f;
    } else if (decisionTimer_ <= 0.0f) {
        // The time this decision covers: the exposure and dwell clocks in
        // chooseState add it. (They used to add the timer's leftover, which
        // is ~0 at this point - so a perched stalker's exposure clock barely
        // moved and it sat on its roof "mostly stationary".)
        decisionDt_ = std::max(sinceDecision_, 0.05f);
        sinceDecision_ = 0.0f;
        chooseState(world, self, target, range, visible);
        decisionTimer_ = cfg_.reactionTime * rng_.range(0.8f, 1.3f);
    }

    if (strafeTimer_ <= 0.0f) {
        strafeSign_ = -strafeSign_;
        strafeTimer_ = rng_.range(1.4f, 3.6f);
    }

    // If we have barely moved while trying to, something is in the way: pick a
    // new direction rather than grinding against a wall forever. This tests
    // *last* frame's throttle, because this frame's has not been chosen yet.
    if (lengthSq(selfPos - lastPos_) < 0.0004f && lastThrottle_ > 0.1f) stuckTimer_ += dt;
    else stuckTimer_ = std::max(0.0f, stuckTimer_ - dt * 0.5f);
    lastPos_ = selfPos;
    if (stuckTimer_ > 1.2f) {
        strafeSign_ = -strafeSign_;
        stuckTimer_ = 0.0f;
    }

    const Vec3 right = normalize(cross(Vec3(0.0f, 1.0f, 0.0f), dirFlat));
    Vec3 move(0.0f, 0.0f, 0.0f);
    float throttle = 0.0f;

    // Shared by both rooftop states: is there deck under a step in this
    // direction? Probed at two distances, because a single probe at four
    // metres steps clean over a parapet two metres away - which is how a
    // machine that is supposed to be holding a roof ends up on the
    // pavement, and then dead, in the middle of its own withdrawal.
    auto footingAlong = [&](const Vec3& dir) {
        for (float d : {2.4f, 4.2f}) {
            const Vec3 probe = selfPos + dir * d;
            const SurfaceHit h = world.findFoothold(
                probe + Vec3(0.0f, 1.4f, 0.0f), Vec3(0.0f, 1.0f, 0.0f),
                1.6f, 3.2f);
            if (!h.hit || h.normal.y < 0.6f) return false;
        }
        return true;
    };


    const bool stalker = isStalker();
    switch (state_) {
        case AiState::Approach: {
            if (stalker && hasFirePos_) {
                // To the firing position, not to the enemy. Off a roof at
                // walking pace, not at a sprint: the firing position it is
                // walking to is on the ground, the way down is over a
                // parapet, and the descend flag the commit below sets only
                // takes hold if the machine is still deciding to go rather
                // than already in the air. A stalker that sprints off a
                // fifty-metre tower kills itself, which it used to do
                // roughly once in four fights.
                const Vec3 d = flattenY(firePos_ - selfPos);
                move = (lengthSq(d) > 1.0f) ? normalize(d) : Vec3(0.0f);
                const float ground = world.terrain().height(selfPos.x, selfPos.z);
                const bool high = selfPos.y - ground > 6.0f;
                throttle = (high && lengthSq(move) > 1e-4f && !footingAlong(move))
                               ? 0.45f : 1.0f;
                break;
            }
            // While hunting, head for where the target actually is. Hunting is
            // the campaign's answer to a fight that has stopped happening, so
            // it deliberately ignores what this machine can see.
            const Vec3 goal = (visible || hunting_) ? targetPos : lastKnown_;
            Vec3 d = normalize(flattenY(goal - selfPos) + Vec3(0.0f, 0.0f, 1e-4f));
            // A slight sideways component so approaches are not dead-straight
            // lines the player can hold an aim on.
            move = normalize(d + right * strafeSign_ * 0.30f * cfg_.strafeBias);
            throttle = 1.0f;
            if (lostSight_ > 6.0f) {
                // Given up on the last known position: sweep.
                move = normalize(d + right * strafeSign_ * 0.8f);
            }
            break;
        }
        case AiState::Engage: {
            if (stalker) {
                // Holding the firing position: a slow shuffle of a couple of
                // metres either way so it is never a still silhouette, and
                // a step back toward the spot if it has drifted.
                const Vec3 back = flattenY(firePos_ - selfPos);
                if (length(back) > 3.5f) { move = normalize(back); throttle = 0.6f; }
                else { move = right * strafeSign_; throttle = 0.22f; }
                break;
            }
            // Every machine is assigned its own bearing around the target and
            // drifts slowly along it. Without this they all converge on the
            // same arc and a group of four can be covered by one burst; with
            // it they spread out and the player has to turn.
            bearing_ += bearingDrift_ * dt * cfg_.strafeBias;
            const Vec3 want = targetPos +
                              Vec3(std::cos(bearing_), 0.0f, std::sin(bearing_)) *
                              cfg_.preferredRange;
            Vec3 toStation = flattenY(want - selfPos);
            const float stationDist = length(toStation);
            if (stationDist > 1.0f) {
                move = toStation / stationDist;
                throttle = clampf(0.5f + stationDist * 0.05f, 0.0f, 1.0f);
            } else {
                // On station: strafe so the machine is never a still target.
                move = right * strafeSign_;
                throttle = 0.55f;
            }
            break;
        }
        case AiState::Reposition: {
            // Too close, or stuck underneath a climber. Backing off does two
            // jobs: it opens the range, and against a target up a wall it drops
            // the elevation angle into something the turret can actually reach.
            const Vec3 relNow = targetPos - self.hitCentre();
            const float flatNow = std::max(lengthXZ(relNow), 0.5f);
            const bool under = std::atan2(relNow.y, flatNow) > deg2rad(38.0f);
            const float away = under ? 1.0f : 0.9f;
            move = normalize(-dirFlat * away + right * strafeSign_ * cfg_.strafeBias);
            throttle = 1.0f;
            break;
        }
        case AiState::Climb: {
            const Vec3 d = flattenY(climbTarget_ - selfPos);
            if (lengthXZ(d) < 3.5f || self.onWall()) {
                // Against the wall: keep pressing INTO it and drive upward.
                // The press has to aim at the middle of the building, not at
                // the approach point on its skin - by the time the limbs have
                // gripped, that point is underfoot, the steering vector is
                // whatever rounding error is left, and the machine peels off
                // the face and falls down it. Aiming through the wall is what
                // a climber's own controls do when a pilot holds forward.
                Vec3 into = flattenY(climbCentre_ - selfPos);
                if (lengthSq(into) < 1e-4f) into = flattenY(climbTarget_ - selfPos);
                move = normalize(into + Vec3(0.0f, 0.0f, 1e-4f));
                // Up, but not forever. A climber holding "forward and up"
                // against a face does not stop at the roof it chose - it
                // carries on up whatever taller core the building has, and
                // then has to get down from there. A stalker that ran out of
                // wall at fifty metres died of the descent. Past the height
                // the perch wants, it presses into the face without the lift
                // and tops out on the first deck the gait can reach.
                const float up = selfPos.y -
                                 world.terrain().height(selfPos.x, selfPos.z);
                if (self.onWall()) {
                    // Up, but not forever, and back DOWN if the face turned
                    // out to be taller than the perch wanted. A climber
                    // holding "forward and up" does not stop at the roof it
                    // chose - it carries on up whatever taller core the
                    // building has, and then has to get down from there. On
                    // the wall, going down is free; walking off the top of it
                    // is what was killing the machine.
                    const float lift = (!stalker || up < 30.0f) ? 1.15f
                                     : (up > 34.0f)             ? -0.9f
                                                                : 0.0f;
                    if (std::fabs(lift) > 1e-3f)
                        move = normalize(move + Vec3(0.0f, 1.0f, 0.0f) * lift);
                }
                throttle = 1.0f;
            } else {
                move = normalize(d);
                throttle = 1.0f;
            }
            break;
        }
        case AiState::Retreat: {
            if (stalker) {
                // Hiding behind the wall: stay put unless the enemy can see
                // in, then shuffle round it.
                if (visible) { move = normalize(-dirFlat + right * strafeSign_ * 0.8f); throttle = 0.8f; }
                else { move = Vec3(0.0f); throttle = 0.0f; }
                break;
            }
            move = normalize(-dirFlat + right * strafeSign_ * 0.5f);
            throttle = 1.0f;
            break;
        }
        case AiState::Perch: {
            // Take the parapet. A sniper standing in the MIDDLE of a roof can
            // see nothing: the line to a target in the street below clips the
            // deck it is standing on, so the machine holds its fire and the
            // whole encounter turns into a silhouette that never shoots. It
            // has to walk to the lip on the target's side, and then hold
            // there - shuffling along the edge, because a still shape on a
            // skyline is a free kill, but never over it.
            if (footingAlong(dirFlat)) {
                // Not at the lip yet: walk out to it.
                move = dirFlat;
                throttle = 0.55f;
            } else if (footingAlong(right * strafeSign_)) {
                // On the parapet with the street below: sidle along it.
                move = right * strafeSign_;
                throttle = 0.30f;
            } else if (footingAlong(right * -strafeSign_)) {
                strafeSign_ = -strafeSign_;
                move = right * strafeSign_;
                throttle = 0.30f;
            } else {
                // A corner of the deck with nothing safe in reach: hold it.
                move = dirFlat;
                throttle = 0.0f;
            }
            break;
        }
        case AiState::Break: {
            // Breaking contact. How that is done depends entirely on how far
            // up it is, and getting this wrong killed the encounter outright:
            // a machine that simply ran for a ground-level cover point walked
            // off the top of a forty-metre tower and died of the landing, so
            // the duel ended with the duellist killing itself.
            const float ground = world.terrain().height(selfPos.x, selfPos.z);
            const float high = selfPos.y - ground;
            if (high > 14.0f) {
                // Too high to jump. Withdraw ACROSS the roof, away from the
                // shooter - back from the parapet, out of the firing line,
                // and along to the far end where it will come up somewhere
                // the player is no longer looking. It stays a rooftop fight,
                // which is the fight this machine wants.
                // Try, in order: back and to one side, straight back, along
                // the parapet either way. Take the first that has deck under
                // it. A fall from up here does the player's job for them, so
                // standing still is always better than a hopeful step.
                const Vec3 opts[4] = {
                    normalize(-dirFlat + right * strafeSign_ * 0.7f),
                    -dirFlat,
                    right * strafeSign_,
                    right * -strafeSign_,
                };
                bool found = false;
                for (const Vec3& o : opts) {
                    if (!footingAlong(o)) continue;
                    move = o;
                    throttle = 1.0f;
                    found = true;
                    break;
                }
                if (!found) {
                    // Cornered on the deck. Hold it: the parapet is still
                    // better cover than the street, and it can still shoot.
                    move = -dirFlat;
                    throttle = 0.0f;
                    strafeSign_ = -strafeSign_;
                }
            } else {
                // Low enough that the drop is survivable: go, and use
                // everything - the claw limbs' own dash and a leap to clear
                // the gap rather than walking all the way round.
                Vec3 d = flattenY(breakTo_ - selfPos);
                const float dd = length(d);
                move = (dd > 1.0f) ? d / dd : right * strafeSign_;
                throttle = 1.0f;
            }
            break;
        }
        case AiState::Idle:
        default:
            move = wander_;
            throttle = 0.2f;
            break;
    }

    // Arena leash. A machine that wanders to the boundary is pinned there by
    // the world clamp and stops being part of the fight, so steer it home well
    // before it gets there.
    const float edge = world.extent() - 24.0f;
    if (std::fabs(selfPos.x) > edge || std::fabs(selfPos.z) > edge) {
        const Vec3 home = normalize(flattenY(Vec3(0.0f, 0.0f, 0.0f) - selfPos) +
                                    Vec3(0.0f, 0.0f, 1e-4f));
        move = normalize(move + home * 1.4f);
    }

    in.moveWorld = move;
    in.throttle = throttle * clampf(cfg_.aggression, 0.4f, 1.0f);
    if (stalker) in.throttle = throttle;          // its pace is its own
    if (hunting_ && !stalker) in.throttle = 1.0f;

    // Coming DOWN off a roof is a climb, not a fall. Nothing in here ever
    // asked for it - the descend flag was written for the player and no
    // machine had ever set it - so every AI descent was a walk off a
    // parapet, and the claw-limbed sniper the whole encounter is built
    // around was killing itself on its own landings. If the machine is up,
    // it is driving, and there is no deck where it is driving, it goes over
    // the edge deliberately and climbs down the face.
    if (in.throttle > 0.25f && lengthSq(in.moveWorld) > 1e-4f) {
        const float ground = world.terrain().height(selfPos.x, selfPos.z);
        if (selfPos.y - ground > 6.0f && !footingAlong(normalize(in.moveWorld)))
            in.wantDescend = true;
    }
    lastThrottle_ = in.throttle;

    // ------------------------------------------------------------ aiming ----
    // Lead the shot with the fastest weapon actually fitted, then degrade it by
    // accuracy so a weak enemy misses in a way that looks like bad gunnery
    // rather than random spray.
    float bestSpeed = 120.0f;
    for (const MountedWeapon& w : self.weapons()) {
        if (w.part) bestSpeed = std::max(bestSpeed, w.part->weapon.projectileSpeed);
    }
    Vec3 aim = visible ? targetPos : lastKnown_;
    Vec3 led;
    if (visible && leadTarget(self.hitCentre(), targetPos, target.velocity(),
                              bestSpeed, &led)) {
        // Blend between no lead and perfect lead by accuracy: a poor gunner
        // under-leads, which is exactly the mistake a human makes.
        aim = lerp(targetPos, led, cfg_.accuracy);
    }
    // The error has to be measured against the size of the thing being shot
    // at, not in absolute metres. A machine is about 1.5 m in radius, so a
    // half-metre error is no error at all - which is how a nominally poor
    // gunner ends up never missing. Scaling hard with range is what makes
    // distance a real defence and closing the gap a real decision.
    const float err = (1.0f - cfg_.accuracy) * (1.6f + range * cfg_.aimSpread);
    aim += Vec3(rng_.range(-err, err), rng_.range(-err * 0.7f, err * 0.7f),
                rng_.range(-err, err));
    in.aimPoint = aim;

    // ------------------------------------------------------------- firing ---
    bool wantFire = false;
    if (visible && range < self.stats().sensorRange) {
        const Vec3 aimDir = self.aimDirection();
        // Against the solution the gunner BELIEVES in, not against the truth.
        // Scoring the turret's facing on the true bearing counted the aim
        // error twice - once in where the turret was pointed and again in
        // whether it was allowed to shoot - so a gunner with any scatter at
        // all simply never pulled the trigger at range.
        const Vec3 want = normalize(aim - self.hitCentre());
        // Fire once the turret is on target, where "on target" is measured
        // against how big the target actually looks from here. A fixed cone is
        // wrong at both ends: far too loose at ten metres and so tight at sixty
        // that the machine never shoots at all.
        const float apparent = std::atan2(target.hitRadius(), std::max(range, 1.0f));
        const float allow = std::cos(clampf(apparent * 2.0f + deg2rad(1.5f),
                                            deg2rad(1.0f), deg2rad(14.0f)));
        if (dot(aimDir, want) > allow) wantFire = true;
    }
    // Trigger discipline: fire in bursts with gaps, not a continuous stream.
    // A poor gunner also pauses more, which is most of what separates an early
    // enemy from a late one in how the fight actually feels.
    if (wantFire && fireHold_ <= 0.0f) {
        in.fireHeld = true;
        const float pauseChance = lerpf(0.035f, 0.006f, cfg_.accuracy);
        if (rng_.unit() < pauseChance)
            fireHold_ = rng_.range(0.3f, lerpf(1.4f, 0.5f, cfg_.accuracy));
    }

    // Breaking contact is the one moment a stalker spends everything it has:
    // the legs' own ability (a dash, on the claw limbs it fields) and a jump
    // to clear the parapet or the gap between two roofs rather than walking
    // all the way round. This is what makes the disengagement read as
    // deliberate rather than as a machine wandering off.
    if (state_ == AiState::Break) {
        in.ability[0] = true;
        if (self.stats().jumpImpulse > 6.0f && rng_.unit() < 0.035f)
            in.jumpHeld = true;
    }

    // Jump a gap or a barrier when blocked and capable.
    if (stuckTimer_ > 0.6f && self.stats().jumpImpulse > 6.0f && rng_.unit() < 0.05f)
        in.jumpHeld = true;

    return in;
}

// ---------------------------------------------------------------- loadouts

float threatRating(const Loadout& l, const MechStats& s) {
    float guns = 0.0f;
    const PartCatalog& cat = PartCatalog::instance();
    for (const std::string& id : l.weapons) {
        if (id.empty()) continue;
        const PartDef* p = cat.find(id);
        if (!p) continue;
        const WeaponDef& w = p->weapon;
        // Damage per second, with splash counted at half weight because it is
        // situational, and a bonus for speed because fast rounds actually land.
        const float dps = (w.damage * std::max(1, w.pellets) +
                           w.blastDamage * 0.5f) /
                          std::max(0.05f, w.ammo == AmmoKind::Cooldown ? w.cooldownTime
                                                                       : w.fireInterval);
        guns += dps * clampf(w.projectileSpeed / 200.0f, 0.55f, 1.35f);
    }
    // Durability and mobility matter roughly as much as firepower.
    const float tough = s.maxHealth * (1.0f + s.armor * 0.09f) * 0.06f;
    const float mob = s.maxSpeed * 1.4f + (s.canClimbWalls ? 8.0f : 0.0f);
    return guns * 0.55f + tough + mob;
}

Loadout enemyLoadout(Archetype a, int power, Rng& rng) {
    const PartCatalog& cat = PartCatalog::instance();
    // Equipment tier, spread rather than stepped. As a hard threshold this was
    // a cliff: every hostile in the mission upgraded on the same mission, and
    // the run died there - mission seven killed a fully-shopped pilot in eight
    // seconds because the whole wave picked up tier-two guns at once. Rolling
    // the fractional part per machine means the better kit arrives a few units
    // at a time over two or three missions, so the player meets one of the new
    // thing before they meet a wave of it. It also stops a wave reading as a
    // single block of identical machines.
    const float tierF = clampf(static_cast<float>(power) / 4.0f, 0.0f, 3.0f);
    int tier = static_cast<int>(tierF);
    if (rng.unit() < (tierF - static_cast<float>(tier))) ++tier;
    tier = static_cast<int>(clampf(static_cast<float>(tier), 0.0f, 3.0f));

    auto pick = [&](Slot s, const std::vector<std::string>& want) -> std::string {
        // Walk the wish list from best to worst and take the first one this
        // power level is allowed to field.
        for (const std::string& id : want) {
            const PartDef* p = cat.find(id);
            if (p && p->tier <= tier) return id;
        }
        const PartDef* fb = cat.starter(s);
        return fb ? fb->id : std::string();
    };

    Loadout l;
    switch (a) {
        case Archetype::Brawler:
            l.chassis = pick(Slot::Chassis, {"ch_ferrum", "ch_revenant", "ch_mule", "ch_wasp"});
            l.legs = pick(Slot::Legs, {"lg_anvil", "lg_strider"});
            l.armor = pick(Slot::Armor, {"ar_reactive", "ar_ceramic", "ar_weave", "ar_none"});
            break;
        case Archetype::Skirmisher:
            l.chassis = pick(Slot::Chassis, {"ch_shrike", "ch_mule", "ch_wasp"});
            l.legs = pick(Slot::Legs, {"lg_harrier", "lg_strider"});
            l.armor = pick(Slot::Armor, {"ar_ceramic", "ar_weave", "ar_none"});
            break;
        case Archetype::Sniper:
            l.chassis = pick(Slot::Chassis, {"ch_shrike", "ch_mule", "ch_wasp"});
            l.legs = pick(Slot::Legs, {"lg_strider", "lg_harrier"});
            l.armor = pick(Slot::Armor, {"ar_weave", "ar_none"});
            break;
        case Archetype::Lancer:
            l.chassis = pick(Slot::Chassis, {"ch_shrike", "ch_wasp"});
            l.legs = pick(Slot::Legs, {"lg_grasshopper", "lg_harrier", "lg_strider"});
            l.armor = pick(Slot::Armor, {"ar_ceramic", "ar_none"});
            break;
        case Archetype::WallRunner:
            l.chassis = pick(Slot::Chassis, {"ch_shrike", "ch_wasp"});
            // Light frame and grippy legs: a heavy machine cannot hold a wall,
            // so a wall-runner is by construction fragile.
            l.legs = "lg_harrier";
            l.armor = pick(Slot::Armor, {"ar_weave", "ar_none"});
            break;
        case Archetype::Stalker:
            // A specialist, and deliberately NOT tier-gated: the whole point
            // of this machine is that it is built for one job and fields the
            // kit for it whatever mission it turns up in. Adhesive claw legs
            // that will hold any wall in the game, the lightest frame that
            // will carry a long gun, a rangefinder instead of armour, and
            // almost no plating - because everything it survives, it survives
            // by not being where you are shooting.
            // The RAIDER frame specifically, tier or no tier: its shoulder
            // mounts are the only light-machine hardpoints in the catalog
            // that will take a railgun, and a sniper without a sniper's rifle
            // is just a machine standing on a roof. On a starter frame this
            // encounter was doing five points of damage in a minute.
            l.chassis = "ch_shrike";
            l.legs = "lg_gecko";
            l.armor = "ar_none";
            break;
        case Archetype::Sieger:
            l.chassis = pick(Slot::Chassis, {"ch_judicator", "ch_bastion", "ch_revenant", "ch_mule"});
            l.legs = pick(Slot::Legs, {"lg_titan", "lg_anvil", "lg_strider"});
            l.armor = pick(Slot::Armor, {"ar_ablative", "ar_reactive", "ar_weave", "ar_none"});
            break;
        default: break;
    }
    // A stalker lives or dies on being somewhere else, so its reactor is not
    // tier-gated either: on a starter cell this machine is slower than the
    // player, which makes every disengagement a fighting withdrawal it loses.
    l.engine = (a == Archetype::Stalker)
                   ? std::string("en_milspec")
                   : pick(Slot::Engine, {"en_halcyon", "en_milspec", "en_civic"});
    l.sensor = (a == Archetype::Stalker)
                   ? std::string("se_ranger")
                   : pick(Slot::Sensor, {"se_lidar", "se_track", "se_basic"});
    l.ensureWeaponSlots();

    // Weapons by role, size-matched to whatever the chassis actually offers.
    std::vector<std::string> wish;
    switch (a) {
        case Archetype::Brawler:    wish = {"wp_scatter", "wp_twin14", "wp_pd9"}; break;
        case Archetype::Skirmisher: wish = {"wp_needle", "wp_twin14", "wp_pd9"}; break;
        case Archetype::Sniper:     wish = {"wp_lance", "wp_needle", "wp_pd9"}; break;
        case Archetype::Lancer:     wish = {"wp_needle", "wp_pd9", "wp_twin14"}; break;
        case Archetype::WallRunner: wish = {"wp_needle", "wp_pd9"}; break;
        case Archetype::Sieger:     wish = {"wp_solaris", "wp_thumper", "wp_twin14", "wp_pd9"}; break;
        // A railgun on the shoulders and a light gauss in the chin: one heavy
        // aimed shot every three seconds, with something to fill the gap. The
        // long cooldown is the point - it gives the duel a rhythm the player
        // can read and move against, instead of a stream of small hits.
        case Archetype::Stalker:    wish = {"wp_lance", "wp_needle", "wp_pd9"}; break;
        default: wish = {"wp_pd9"}; break;
    }

    const PartDef* ch = cat.find(l.chassis);
    for (size_t i = 0; i < l.weapons.size(); ++i) {
        const SizeClass mountSize = (ch && i < ch->hardpoints.size())
                                        ? ch->hardpoints[i].size : SizeClass::Light;
        std::string chosen;
        for (const std::string& id : wish) {
            const PartDef* p = cat.find(id);
            if (!p) continue;
            if (static_cast<int>(p->size) > static_cast<int>(mountSize)) continue;
            // The stalker's gun is its identity, so it is exempt from the
            // power-tier gate: a duellist that turns up in mission three with
            // a starter autocannon is not the encounter the mission promises.
            if (p->tier > tier && a != Archetype::Stalker) continue;
            chosen = id;
            break;
        }
        if (chosen.empty()) chosen = "wp_pd9";
        l.weapons[i] = chosen;
    }

    // The stalker's rack is written by hand, because the generic "best gun
    // that fits each mount" rule gave it TWO railguns - a two-hundred-point
    // volley every three seconds, which killed a starter machine in half a
    // minute and made a mission-two encounter the hardest fight in the game.
    // One rifle, one light gun to fill the gap between shots. That is a
    // sniper; two rifles is an execution.
    if (a == Archetype::Stalker) {
        bool rifleFitted = false;
        for (size_t i = 0; i < l.weapons.size(); ++i) {
            const SizeClass mountSize = (ch && i < ch->hardpoints.size())
                                            ? ch->hardpoints[i].size : SizeClass::Light;
            if (!rifleFitted &&
                static_cast<int>(SizeClass::Medium) <= static_cast<int>(mountSize)) {
                l.weapons[i] = "wp_lance";
                rifleFitted = true;
            } else {
                l.weapons[i] = "wp_needle";
            }
        }
        if (!rifleFitted && !l.weapons.empty()) l.weapons[0] = "wp_needle";
    }

    // Occasionally leave a mount empty on low-power enemies so early waves are
    // not all fully kitted.
    if (power <= 2 && l.weapons.size() > 1 && rng.unit() < 0.5f &&
        a != Archetype::Stalker)
        l.weapons.back().clear();

    return l;
}

} // namespace sb
