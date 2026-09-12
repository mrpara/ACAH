// campaign.h - levels, waves, money and the run that ties them together.
//
// Twelve handcrafted missions, then an endless ladder that keeps escalating.
// Money comes from kills and is priced by how dangerous the thing you killed
// actually was, so clearing a wave of siege platforms pays properly and farming
// scouts does not. Dying costs you the mission, never your machine: you retry
// the level with everything you own, because losing a store purchase to a bad
// landing is the kind of thing that makes people stop playing.
#pragma once

#include <string>
#include <vector>
#include "ai.h"
#include "combat.h"
#include "math3d.h"
#include "mech.h"
#include "parts.h"
#include "props.h"
#include "units.h"
#include "world.h"

namespace sb {

// One group of enemies that arrives together.
struct WaveSpec {
    std::vector<Archetype> enemies;
    float delay = 0.0f;          // seconds after the previous wave clears
};

// What one segment of a mission asks of the pilot. A mission is a short chain
// of these laid out roughly along a line across the arena - clearing one is a
// checkpoint - and the core of almost every segment is the same: push forward,
// wreck the defences, and deal with whatever the objective adds on top.
enum class ObjectiveKind : int {
    ClearHostiles = 0,  // kill every machine this segment spawned
    DestroyMarked,      // destroy the marked installations
    ReachZone,          // get to the marked position
    Escort,             // keep the friendly vehicle alive until it arrives
    HoldZone,           // hold the marked position for the timer
    Rampage,            // destroy as much as possible before the timer
    KillTarget,         // destroy the named elite machine(s)
    Convoy,             // destroy the moving column before it escapes
    Blackout,           // destroy the radar masts; noise brings reinforcements
    Breakthrough        // cross the fortified line while the artillery walks in
};

struct ObjectiveSpec {
    ObjectiveKind kind = ObjectiveKind::ClearHostiles;
    std::string label;           // what the HUD calls it
    int count = 0;               // targets to destroy, where it applies
    float timer = 0.0f;          // seconds, where it applies
    float zoneRadius = 18.0f;
    // Hostile spidertanks arriving during this segment (rare, strong).
    std::vector<WaveSpec> waves;
    // The garrison of small units defending this segment.
    int troopers = 0, atTeams = 0, apcs = 0, tanks = 0, turrets = 0, drones = 0;
    // The second tier. These are not "more enemies" - they are enemies that
    // pose a question. A marksman makes standing in the open a decision; a
    // mortar makes standing ANYWHERE for long a decision; a jammer takes your
    // instruments away until you go and deal with it; a warden means the
    // strongpoint has to be taken apart in the right order.
    int marksmen = 0, mortars = 0, jammers = 0, wardens = 0;
    // Long-range OVERWATCH guns: tank/turret positions placed far off the
    // lane with sightlines onto it, engaging from 150-200 m. They are what
    // gives snipers something to duel, brawlers something to rush, and
    // climbers a reason to take the rooftops.
    int overwatch = 0;
    // True on the marching segments the long-war pass inserts: the fight
    // along the route is PRESSURE, not a checklist. Reaching the far end
    // advances the mission whether or not the last picket died. Only real
    // elimination objectives make you kill everything.
    bool advanceOnReach = false;
};

struct LevelDef {
    std::string arenaId;
    std::string name;            // mission name, distinct from the arena name
    std::string briefing;
    int power = 1;               // feeds enemy tier and AI difficulty
    float difficulty = 0.0f;     // 0..1, sharpens the AI
    std::vector<WaveSpec> waves; // legacy path: one Clear segment if objectives empty
    std::vector<ObjectiveSpec> objectives;
    int completionBonus = 0;
    int propBudget = 44;         // how furnished with destructibles the map is
    int musicStyle = 0;          // which synth flavour scores it
    bool bossMission = false;    // distinct music, elite health, named enemy
    uint32_t seed = 1u;
};

// The twelve handcrafted missions.
const std::vector<LevelDef>& campaignLevels();

// Mission 13 and beyond, generated. `index` is zero-based and continues past
// the handcrafted set.
LevelDef endlessLevel(int index);

// Everything the player keeps between missions.
struct PlayerProfile {
    Loadout loadout;
    int cash = 0;
    int level = 0;               // zero-based index of the mission to play next
    int maxCleared = -1;         // highest mission index ever cleared
    int kills = 0;
    int deaths = 0;
    int totalEarned = 0;
    // Spare magazines owned per weapon id, carried between missions.
    std::vector<std::pair<std::string, int>> ammoStock;

    int ammoFor(const std::string& weaponId) const;
    void addAmmo(const std::string& weaponId, int rounds);
    bool spendAmmo(const std::string& weaponId, int rounds);
};

PlayerProfile newProfile();

// Plain-text persistence, one keyed value per line. The profile is the whole
// save: which mission, what machine, how much money, whose ammunition.
bool saveProfile(const PlayerProfile& p, const char* path);
bool loadProfile(PlayerProfile& p, const char* path);

enum class MissionPhase : int {
    Briefing = 0,
    Fighting,
    WaveGap,
    Cleared,
    Failed
};

// The after-action ledger: everything the debrief screen itemises and the
// payout is computed from.
struct MissionLedger {
    int mechKills = 0;
    int unitKills[6] = {0, 0, 0, 0, 0, 0};   // by UnitKind
    int propsDestroyed = 0;
    int objectivesDone = 0;
    // Kept for the profile's lifetime death count and for the debrief line.
    // It no longer docks anything: dying ends the contract and sends you back
    // to the start of the mission, and charging a second time for the same
    // mistake is a penalty on top of a penalty.
    int checkpointDeaths = 0;
    int cashKills = 0;                       // bounties
    int cashDestruction = 0;                 // the wrecking ledger
    int cashObjectives = 0;
    int cashTimeBonus = 0;
    int cashClearance = 0;                   // sectors closed out properly
    int sectorsCleared = 0;
    float missionTime = 0.0f;
    int total(int completionBonus) const {
        const int t = cashKills + cashDestruction + cashObjectives +
                      cashTimeBonus + cashClearance + completionBonus;
        return t < 0 ? 0 : t;
    }
};

// Live state of one mission attempt.
class Mission {
public:
    // Builds the world, spawns the player and arms the first wave. The mission
    // starts in the Briefing phase: everything exists and animates, but the
    // hostiles do not think, move or shoot until combat is released.
    void begin(const LevelDef& level, PlayerProfile& profile);

    // Hands control over and lets the shooting start. Called when the player
    // deploys off the briefing screen.
    void beginCombat();

    // Test scaffolding: force the mission to a clean Cleared state. The
    // controls harness uses it to reach the workshop without playing a full
    // objective chain - the store is the test subject there, not the pilot.
    // More scaffolding: jump straight to objective N so a probe can inspect
    // every segment's marked targets without playing the chain.
    void debugStartObjective(int i) { startObjective(i); }

    void debugComplete() {
        phase_ = MissionPhase::Cleared;
        endTimer_ = 10.0f;
        cashEarned_ += level_.completionBonus;
    }

    void update(float dt, const MechInput& playerInput);

    void submit(Rasterizer& raster, const Vec3& viewPos, float viewDistance,
                bool skipPlayer = false, float eyeRadius = 0.0f) const;

    // ------------------------------------------------------------- state ----
    MissionPhase phase() const { return phase_; }
    const World& world() const { return world_; }
    const LevelDef& level() const { return level_; }
    Mech& player() { return mechs_[0]; }
    const Mech& player() const { return mechs_[0]; }
    Combat& combat() { return combat_; }
    const Combat& combat() const { return combat_; }
    // The last frame's combat traffic - who fired, what landed where. The
    // audio layer reads this to give every shot and every hit a voice.
    const CombatEvents& lastCombatEvents() const { return lastEvents_; }

    int waveIndex() const { return waveIndex_; }
    int waveCount() const { return static_cast<int>(level_.waves.size()); }
    // ------------------------------------------------------ objectives ----
    int objectiveIndex() const { return objectiveIndex_; }
    int objectiveCount() const { return static_cast<int>(level_.objectives.size()); }
    const ObjectiveSpec* currentObjective() const {
        return (objectiveIndex_ >= 0 && objectiveIndex_ < objectiveCount())
                   ? &level_.objectives[static_cast<size_t>(objectiveIndex_)]
                   : nullptr;
    }
    const Vec3& objectiveZone() const { return objectiveZone_; }
    float objectiveTimer() const { return objectiveTimer_; }
    int objectiveProgress() const { return objectiveProgress_; }
    int objectiveTarget() const { return objectiveTarget_; }
    const MissionLedger& ledger() const { return ledger_; }
    // 0 = clear, 1 = sitting on top of a jammer. Cuts fire-control assist and
    // fogs the radar; the HUD says so.
    float jamStrength() const { return jamStrength_; }
    // Where a Warden is putting armour back on this frame, if anywhere, so
    // the game layer can show and sound it. Empty most of the time.
    const std::vector<Vec3>& repairPulses() const { return repairPulses_; }
    // Set the frame the player's reactor discharges, drained by the game.
    bool takeEmpFlash() { const bool f = empFlash_; empFlash_ = false; return f; }
    const std::vector<Unit>& units() const { return units_; }
    const std::vector<Destructible>& props() const { return props_; }
    // The lane's direction of travel - the axis missions run along. Tools
    // use it as a poor man's road network on island maps.
    const Vec3& missionAxisDir() const { return missionAxis_; }
    const std::vector<int>& markedProps() const { return markedProps_; }
    const std::vector<int>& markedUnits() const { return markedUnits_; }
    const std::vector<int>& markedMechs() const { return markedMechs_; }
    int escortIndex() const { return escortUnit_; }
    bool playerCarrying() const { return false; }
    float alarmLevel() const { return alarm_; }
    int enemiesAlive() const;
    int cashEarned() const { return cashEarned_; }
    float waveTimer() const { return waveTimer_; }
    float missionTime() const { return elapsed_; }
    // True when the stall guard has ordered the remaining hostiles to close.
    bool hunting() const { return hunting_; }
    // Damage the player landed this frame, so the HUD can flash a hit marker.
    float damageDealtThisFrame() const { return dealtThisFrame_; }

    // The nearest live hostile, for the HUD's threat marker.
    const Mech* nearestEnemy() const;
    const std::vector<Mech>& mechs() const { return mechs_; }
    const std::vector<AiController>& brains() const { return brains_; }

    // Called by the game when the player accepts the result, to fold winnings
    // and salvage back into the profile.
    void settle(PlayerProfile& profile) const;

private:
    void spawnWave(int index);
    void spawnMechWave(const WaveSpec& w, const Vec3& around);
    void handleDestroyed(const CombatEvents& ev);
    void rescueStuckEnemies(float dt);
    void startObjective(int index);
    void updateObjective(float dt);
    void completeObjective();
    void spawnGarrison(const ObjectiveSpec& spec, const Vec3& around);
    void spawnUnit(UnitKind kind, const Vec3& near, int mode, const Vec3& goal);
    // Reinforcements arrive already alerted - they were sent at the player.
    void spawnUnitAlerted(UnitKind kind, const Vec3& near, int mode, const Vec3& goal);
    // Rooftop emplacement placement; false when no building top serves.
    bool spawnTurretOnRoof(const Vec3& near);
    // Drops a magazine crate suited to the player's own heavy guns.
    void dropAmmoCrate(const Vec3& at);
    void onUnitKilled(int idx);
    void onPropKilled(int idx);
    void respawnAtCheckpoint();
    // Position along the mission route, t 0..1. The route sweeps across the
    // district rather than running dead straight (see lanePoint), which is
    // what makes a seven-segment contract a march instead of a corridor;
    // `sweep` scales that curve, and a ROAD COLUMN passes a low value because
    // a convoy follows a road, not a slalom - and because at full sweep its
    // route is nearly twice as long as the timer was written for.
    Vec3 lanePoint(float t, float sweep = 1.0f) const;

    World world_;
    LevelDef level_;
    Combat combat_;

    // Index 0 is always the player. Enemies follow. The vector is never
    // reordered mid-mission because projectiles carry indices into it.
    std::vector<Mech> mechs_;
    std::vector<AiController> brains_;   // parallel to mechs_, entry 0 unused
    std::vector<ShotRequest> shots_;
    CombatEvents lastEvents_;

    MissionPhase phase_ = MissionPhase::Briefing;
    int waveIndex_ = -1;
    float waveTimer_ = 0.0f;
    float elapsed_ = 0.0f;
    float endTimer_ = 0.0f;
    int cashEarned_ = 0;
    float dealtThisFrame_ = 0.0f;
    // Seconds since anything took damage. A fight that has gone quiet for long
    // enough means the two sides cannot reach each other, which would otherwise
    // leave the mission unwinnable.
    float quietFor_ = 0.0f;
    bool hunting_ = false;
    // Per-enemy "has not moved" timers, for the relocation failsafe.
    std::vector<float> stuckFor_;
    std::vector<float> defenseTimer_;   // per mech: point-defense / dodge cooldown
    // How badly the player's systems are being jammed this frame, 0..1.
    // Recomputed every update from whatever Jammers are alive and near.
    float jamStrength_ = 0.0f;
    std::vector<Vec3> repairPulses_;
    bool empFlash_ = false;
    float repairPulseTimer_ = 0.0f;
    int pendingWave_ = -1;          // a spidertank wave held back by its delay
    float pendingWaveTimer_ = 0.0f;
    std::vector<Vec3> lastEnemyPos_;
    bool relocated_ = false;
    Rng rng_{1u};

    // Salvaged magazines picked up during the mission, folded into the profile
    // on settle so a mission you fail does not bank the pickups.
    mutable std::vector<std::pair<std::string, int>> salvage_;

    // ---- the ACAH layer: objectives, units, destructibles ----------------
    std::vector<Unit> units_;
    std::vector<Destructible> props_;
    std::vector<int> markedProps_;     // indices of current objective targets
    std::vector<int> markedUnits_;
    std::vector<int> markedMechs_;
    int escortUnit_ = -1;
    Vec3 escortLastPos_{0.0f, 0.0f, 0.0f};
    float escortStuckFor_ = 0.0f;
    MissionLedger ledger_;
    int objectiveIndex_ = -1;
    Vec3 objectiveZone_{0.0f, 0.0f, 0.0f};
    float objectiveTimer_ = 0.0f;
    int objectiveProgress_ = 0;
    int objectiveTarget_ = 0;
    float alarm_ = 0.0f;               // Blackout: how loud the sector is
    float reinforceTimer_ = 0.0f;
    float shellTimer_ = 0.0f;          // Breakthrough: the walking barrage
    Vec3 missionAxis_{0.0f, 0.0f, 1.0f};
    bool laneFlip_ = false;      // the mission runs the lane end-to-start
    Vec3 checkpointPos_{0.0f, 0.0f, 0.0f};
    float checkpointHealth_ = 1.0f;    // fraction restored on respawn
    int segmentWavesSpawned_ = 0;
};

// How tough an enemy of this power level is, as a multiplier on its structure.
float enemyHealthScale(int power);

// How hard hostile weapons hit, as a fraction of their listed rating, for a
// mission of this power. The player's guns are never scaled.
float enemyDamageScale(int power);

// Bounty for destroying a machine of this loadout at this power level.
int bountyFor(const Loadout& l, const MechStats& s, int power);

} // namespace sb
