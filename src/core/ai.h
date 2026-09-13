// ai.h - hostile spidertank behaviour.
//
// Enemies are not smart, but they are legible: each one has an archetype that
// says how it wants to fight, and a small state machine that reads to the
// player as intent. A brawler closes and circles. A sniper backs off and holds
// a line. A wall-runner ignores the ground entirely and comes at you from the
// side of a building, which is the whole reason the climbing system exists.
#pragma once

#include <string>
#include <vector>
#include "combat.h"
#include "math3d.h"
#include "mech.h"
#include "world.h"

namespace sb {

enum class Archetype : int {
    Brawler = 0,    // closes to knife range and circles
    Skirmisher,     // holds mid range, strafes hard, never stops moving
    Sniper,         // backs off, seeks high ground, fires on a lock
    Lancer,         // fast diagonal charges, disengages after a pass
    WallRunner,     // climbs a structure and attacks from above or the side
    Sieger,         // slow, heavy, walks straight at you and does not stop
    Stalker,        // rooftop sniper: climbs, shoots from a long way off,
                    // breaks contact the moment you look at it, and comes
                    // back down from somewhere else
    Count
};

const char* archetypeName(Archetype a);

enum class AiState : int {
    Idle = 0,
    Approach,
    Engage,
    Reposition,
    Climb,
    Retreat,
    Perch,          // holding a rooftop and shooting from it
    Break           // seen: get off this roof and out of the firing line
};

struct AiConfig {
    Archetype archetype = Archetype::Brawler;
    float preferredRange = 22.0f;      // where it tries to sit
    float rangeTolerance = 8.0f;
    float strafeBias = 1.0f;           // how hard it circles, signed at spawn
    float aggression = 1.0f;           // 0 timid, 1.5 reckless
    float accuracy = 0.85f;            // 0..1, scales aim error and lead quality
    float reactionTime = 0.35f;        // delay before it responds to a change
    bool  willClimb = false;
    // A stalker will not stand and trade. It fires from a roof, and as soon as
    // it has been in the open long enough for the player to answer, it leaves
    // and sets up somewhere else. Zero disables the behaviour entirely.
    float exposureLimit = 0.0f;   // seconds of being shootable before it moves
    // How fast the aim error grows with distance, in metres of scatter per
    // metre of range. A line unit hoses; a marksman's whole identity is that
    // this number is small, which is what lets it be dangerous from a place
    // its target cannot answer from.
    float aimSpread = 0.16f;
};

AiConfig configFor(Archetype a, float difficulty, Rng& rng);

// One brain, one mech. Held by the campaign alongside the mech list.
class AiController {
public:
    void init(const AiConfig& cfg, uint32_t seed);
    // A stalker keeps its own counsel: it is NEVER ordered to close, and the
    // campaign's silence rules leave it alone for far longer, because being
    // out of contact is its whole method rather than a stall.
    bool isStalker() const { return cfg_.exposureLimit > 0.0f; }

    // Produces the input for `self` this frame. `target` may be null or dead,
    // in which case the machine idles.
    MechInput think(float dt, const World& world, const Mech& self,
                    const Mech* target);

    // Forces this machine to close on its target regardless of archetype. The
    // campaign turns this on when a fight has gone quiet, so a mission can
    // never end up unwinnable because a skirmisher is holding range in a corner
    // the player cannot reach.
    void setHunting(bool on) { hunting_ = on; }
    bool hunting() const { return hunting_; }

    AiState state() const { return state_; }
    const AiConfig& config() const { return cfg_; }

private:
    void chooseState(const World& world, const Mech& self, const Mech& target,
                     float range, bool visible);
    Vec3 pickClimbTarget(const World& world, const Mech& self, const Mech& target);

    AiConfig cfg_;
    AiState state_ = AiState::Idle;
    Rng rng_{1u};

    float stateTimer_ = 0.0f;
    float decisionTimer_ = 0.0f;
    float sinceDecision_ = 0.0f;    // seconds since chooseState last ran
    float decisionDt_ = 0.3f;       // what the current decision covers
    float strafeSign_ = 1.0f;
    float strafeTimer_ = 0.0f;
    float fireHold_ = 0.0f;
    float lostSight_ = 0.0f;
    float stuckTimer_ = 0.0f;
    float lastThrottle_ = 0.0f;
    // Where around the target this machine wants to sit, in radians. Assigned
    // per machine so a group spreads around the player instead of stacking on
    // one bearing and all being covered by one burst.
    float bearing_ = 0.0f;
    float bearingDrift_ = 0.0f;
    Vec3 lastPos_{0.0f, 0.0f, 0.0f};
    Vec3 lastKnown_{0.0f, 0.0f, 0.0f};
    Vec3 wander_{0.0f, 0.0f, 0.0f};
    Vec3 climbTarget_{0.0f, 0.0f, 0.0f};
    // The centre of the structure being climbed. The approach point alone is
    // not enough: once the machine is ON the face it has already reached that
    // point, the steering vector collapses to noise, and it slides back down
    // the wall it just gripped. Pressing towards the middle of the building is
    // what "keep pushing into the wall" actually means.
    Vec3 climbCentre_{0.0f, 0.0f, 0.0f};
    bool hasClimbTarget_ = false;
    // Stalker bookkeeping: how long it has been standing in the player's view,
    // where it is running to while breaking contact, and which structure it
    // used last, so it never sets up on the same roof twice running.
    float exposure_ = 0.0f;
    Vec3 breakTo_{0.0f, 0.0f, 0.0f};
    // The ground sniper loop (stalker). A firing position with cover next
    // to it; how long it has held it, what it arrived with, and where it
    // ducks when it leaves. `hideTimer_` is the wait out of sight.
    Vec3 firePos_{0.0f, 0.0f, 0.0f};
    Vec3 coverPos_{0.0f, 0.0f, 0.0f};
    Vec3 lastFirePos_{0.0f, 0.0f, 0.0f};
    bool hasFirePos_ = false;
    bool hasLastFirePos_ = false;
    float dwell_ = 0.0f;
    float arriveHealth_ = 1.0f;
    float hideTimer_ = 0.0f;
    float sinceShot_ = 99.0f;
    float lastHealth_ = 1.0f;
    Vec3 lastPerch_{0.0f, 0.0f, 0.0f};
    bool hasLastPerch_ = false;
    Vec3 failedClimb_{0.0f, 0.0f, 0.0f};   // a face this machine could not get up
    bool hasFailedClimb_ = false;
    int climbFails_ = 0;                    // consecutive failed ascents
    float climbStall_ = 0.0f;               // seconds pressed at a face, going nowhere
    Vec3 pickBreakPoint(const World& world, const Mech& self, const Mech& target);
    // A long-range firing spot with a building to duck behind. Fills
    // firePos_/coverPos_; returns false if nothing usable was found.
    bool pickFirePosition(const World& world, const Mech& self, const Mech& target);
    bool hunting_ = false;
    bool haveSeen_ = false;
};

// The loadout an enemy of this archetype and power level should be built from.
// `power` is roughly the level number: it decides tier, and the campaign turns
// the resulting mass and firepower into a bounty.
Loadout enemyLoadout(Archetype a, int power, Rng& rng);

// A rough "how dangerous is this machine" number, used to price bounties and
// to balance a wave.
float threatRating(const Loadout& l, const MechStats& s);

} // namespace sb
