#include "campaign.h"

#include "props.h"
#include "units.h"

#include <cstdio>
#include <cstring>

#include <algorithm>
#include <cmath>

namespace sb {

namespace {

constexpr float kWaveGap = 4.0f;        // breathing room between waves
constexpr float kEndDelay = 2.6f;       // beat before the result screen
constexpr int kMaxEnemiesAlive = 10;    // the rasterizer's practical budget
constexpr float kStallTimeout = 14.0f;  // quiet seconds before hostiles are told to close
// What a failed attempt is worth. This is the whole recovery loop, and it has
// to scale with the mission's power or it stops working exactly where it is
// needed: at power eight a part costs five to fourteen thousand credits, and
// a floor written for the early game paid a few hundred, so a pilot who hit a
// wall failed the same contract five times in a row without their machine
// getting one credit better. Now two failed attempts fund a real upgrade -
// which, with a retry replaying the identical ground, means a wall is a thing
// you learn and then buy your way through rather than a dead end.
constexpr float kFailureSalvage = 0.45f; // share of earnings kept on a failed attempt
constexpr int kFailureFloor = 80;        // flat recovery payment on a failed attempt
constexpr int kFailureFloorPerPower = 58;
// Every source of income is halved through this. Wave 15: the user found
// progression far too quick with missions twice as long - every long march
// was paying twice as much wrecking and clearance too. Applied at each
// source (not at the payout) so the debrief ledger still adds up.
constexpr float kPayScale = 0.5f;
inline int pay(int amount) { return static_cast<int>(amount * kPayScale + 0.5f); }
constexpr float kSilenceRelocate = 22.0f; // quiet seconds before hostiles are moved to the player



} // namespace

// ------------------------------------------------------------------- levels

const std::vector<LevelDef>& campaignLevels() {
    static const std::vector<LevelDef> levels = [] {
        std::vector<LevelDef> v;
        using A = Archetype;
        using O = ObjectiveKind;

        auto add = [&](const char* arena, const char* name, const char* brief,
                       int power, float diff, std::vector<ObjectiveSpec> objectives,
                       int bonus, uint32_t seed, int music, bool boss = false,
                       int propBudget = 58) {
            LevelDef d;
            d.arenaId = arena;
            d.name = name;
            d.briefing = brief;
            d.power = power;
            d.difficulty = diff;
            d.objectives = std::move(objectives);
            d.completionBonus = pay(bonus);
            d.seed = seed;
            d.musicStyle = music;
            d.bossMission = boss;
            d.propBudget = propBudget;
            v.push_back(d);
        };

        // A BOUNTY ELITE: one named machine, hand-built, with the weapon it
        // is known for. Killing it puts that weapon in the workshop for free.
        auto elite = [](A brain, const char* callsign, const char* chassis, const char* legs,
                        const char* engine, const char* armor, const char* sensor,
                        std::vector<std::string> weapons, const char* drop) {
            WaveSpec w;
            w.enemies = {brain};
            w.elite = true;
            w.callsign = callsign;
            w.eliteLoadout.chassis = chassis;
            w.eliteLoadout.legs = legs;
            w.eliteLoadout.engine = engine;
            w.eliteLoadout.armor = armor;
            w.eliteLoadout.sensor = sensor;
            w.eliteLoadout.ensureWeaponSlots();
            for (size_t i = 0; i < weapons.size() && i < w.eliteLoadout.weapons.size(); ++i)
                w.eliteLoadout.weapons[i] = weapons[i];
            w.dropWeapon = drop;
            return w;
        };

        auto obj = [](O kind, const char* label, int count, float timer,
                      std::vector<WaveSpec> waves = {}, int troopers = 0,
                      int atTeams = 0, int apcs = 0, int tanks = 0,
                      int turrets = 0, int drones = 0) {
            ObjectiveSpec o;
            o.kind = kind;
            o.label = label;
            o.count = count;
            o.timer = timer;
            o.waves = std::move(waves);
            o.troopers = troopers; o.atTeams = atTeams; o.apcs = apcs;
            o.tanks = tanks; o.turrets = turrets; o.drones = drones;
            return o;
        };

        // ================= ACT I : THE CITY CONTRACTS =====================
        // Corporate war in a ruined district. The first act teaches the loop:
        // push up the lane, wreck the defences, do the job, get paid.

        add("ruined_district", "FIRST CONTRACT",
            "Helion Combine has written off this district. Their insurers "
            "have not. Burn the listed fuel stores and walk out the far side. "
            "Expect security troopers - your hull barely notices small arms.",
            1, 0.05f,
            {obj(O::DestroyMarked, "BURN THE FUEL STORES", 3, 0.0f, {}, 5, 0, 0, 0, 0, 0),
             obj(O::ReachZone, "ADVANCE UP THE LANE", 0, 0.0f, {}, 4, 1, 0, 0, 0, 0),
             obj(O::ReachZone, "REACH THE EXTRACTION POINT", 0, 0.0f, {}, 3, 1, 0, 0, 0, 0)},
            720, 91001u, 2);

        // The high-rise ruins, and the mission that teaches the player the
        // third dimension is not decoration. Between the depot and the plaza
        // sits a single machine called MAGPIE: adhesive claw legs, a light
        // gauss rifle, no armour worth the name, and a pilot who will not
        // stand and fight for one second longer than they have to. It climbs
        // a tower, shoots from a hundred metres, and the moment it is looked
        // at it goes over the parapet and sets up somewhere else. There is no
        // garrison in that segment on purpose: it is a duel, and the lesson is
        // that in this district the way to reach something is up.
        add("downtown", "STREET SWEEP",
            "A garrison holds the core blocks: infantry, wheeled armour, one "
            "gun emplacement. Sweep the lane. Anything that burns is billable. "
            "One more thing - Helion keeps a spotter in these towers. Light "
            "frame, long gun, and it will not be where you last saw it.",
            2, 0.12f,
            {obj(O::ClearHostiles, "CLEAR THE FORWARD BLOCKS", 0, 0.0f, {}, 7, 1, 1, 0, 0, 0),
             obj(O::DestroyMarked, "DEMOLISH THE DEPOT", 3, 0.0f, {}, 4, 1, 1, 0, 1, 0),
             obj(O::KillTarget, "HUNT THE SPOTTER - CALLSIGN MAGPIE", 1, 0.0f,
                 {WaveSpec{{A::Stalker}, 0.0f}}, 0, 0, 0, 0, 0, 0),
             obj(O::ReachZone, "PUSH TO THE PLAZA", 0, 0.0f, {}, 3, 1, 0, 0, 0, 2)},
            940, 91002u, 0);

        add("canyon", "TOLL ROAD",
            "A resupply column is running the canyon road. Nothing that enters "
            "the canyon leaves it. You have until they reach the north mouth.",
            3, 0.18f,
            {obj(O::ReachZone, "MARCH INTO THE CANYON", 0, 0.0f, {}, 5, 2, 1, 0, 0, 2),
             obj(O::DestroyMarked, "BLIND THE ROAD WATCH", 3, 0.0f, {}, 4, 1, 1, 0, 1, 1),
             obj(O::KillTarget, "BOUNTY: THE TOLLKEEPER", 0, 0.0f,
                 {elite(A::Brawler, "TOLLKEEPER", "ch_mule", "lg_anvil", "en_milspec",
                        "ar_ceramic", "se_basic", {"wp_cinder", "wp_cinder", "wp_thumper"},
                        "wp_cinder")}, 4, 2, 1, 0, 1, 2),
             obj(O::Convoy, "DESTROY THE COLUMN", 4, 340.0f, {}, 2, 1, 0, 0, 0, 2)},
            1160, 91003u, 1);

        add("forest", "RELAY BLACKOUT",
            "Their air cover is guided from four relay masts under the trees. "
            "Drop the masts. Every one still standing keeps the drones coming.",
            4, 0.24f,
            {[&] { ObjectiveSpec o = obj(O::Blackout, "DROP THE RELAY MASTS", 4, 0.0f, {}, 3, 1, 0, 0, 0, 1);
                   o.lightsOut = true;   // the grid goes with the masts
                   return o; }(),
             obj(O::DestroyMarked, "BURN THE FOREST FUEL DUMP IN THE DARK", 2, 0.0f, {}, 3, 1, 1, 0, 0, 1),
             [&] { ObjectiveSpec o = obj(O::ReachZone, "EXFILTRATE THROUGH THE TREELINE", 0, 0.0f, {}, 3, 1, 0, 0, 0, 1);
                   o.ambush = true;      // the treeline was waiting
                   return o; }(),
             obj(O::ClearHostiles, "BREAK THE AMBUSH", 0, 0.0f, {}, 2, 1, 0, 0, 0, 1)},
            1400, 91004u, 6);

        add("industrial", "THE WARDEN",
            "The yard has a keeper: a Combine spidertank, ex-military, "
            "well kept. It knows you are coming. Kill it and the yard is ours.",
            5, 0.32f,
            {obj(O::ReachZone, "ENTER THE YARD", 0, 0.0f, {}, 4, 1, 1, 0, 0, 0),
             obj(O::ClearHostiles, "BREAK THE OUTER GUARD", 0, 0.0f, {}, 5, 2, 1, 1, 1, 0),
             obj(O::KillTarget, "DESTROY THE WARDEN", 0, 0.0f,
                 {WaveSpec{{A::Brawler, A::Skirmisher}, 0.0f}}, 0, 1, 0, 0, 0, 2)},
            1820, 91005u, 3, true);

        // ================= ACT II : THE OPEN COUNTRY ======================

        add("mesa", "HIGH GROUND",
            "An observation post on the terraces is feeding them everything "
            "we move. Take the shelf, wreck the installation, and hold it "
            "while our lifters come in.",
            6, 0.38f,
            {obj(O::ReachZone, "CLIMB TO THE TERRACES", 0, 0.0f, {}, 4, 1, 1, 0, 1, 0),
             obj(O::DestroyMarked, "WRECK THE OBSERVATION POST", 3, 0.0f, {}, 5, 2, 0, 1, 2, 0),
             obj(O::HoldZone, "HOLD THE SHELF", 0, 80.0f,
                 {WaveSpec{{A::Skirmisher}, 0.0f}}, 4, 1, 1, 0, 0, 2),
             obj(O::KillTarget, "BOUNTY: THE HIGHWAYMAN", 0, 0.0f,
                 {elite(A::Lancer, "HIGHWAYMAN", "ch_shrike", "lg_grasshopper", "en_milspec",
                        "ar_ceramic", "se_track", {"wp_stiletto", "wp_stiletto", "wp_swarm"},
                        "wp_swarm")}, 2, 1, 0, 0, 0, 2)},
            2160, 91006u, 5);

        add("causeway", "GREY SOUND",
            "A recovery crawler has to cross the sound tonight, and the only "
            "way over is the causeway chain. Machines cannot swim - theirs or "
            "ours. Keep the crawler alive to the far shore.",
            7, 0.44f,
            {[&] { ObjectiveSpec o = obj(O::Escort, "ESCORT THE CRAWLER ACROSS", 0, 0.0f,
                                         {WaveSpec{{A::Skirmisher}, 0.0f}}, 4, 3, 1, 0, 1, 2);
                   o.ambush = true;      // they let it get halfway
                   return o; }()},
            2520, 91007u, 2, false, 36);

        add("underworks", "THE UNDERWORKS",
            "They pulled back into the service galleries under the old works. "
            "Enclosed ground: no sky, no jumping clear. Go in after them and "
            "clear it to the far junction.",
            8, 0.50f,
            {obj(O::ClearHostiles, "CLEAR THE GALLERY APPROACH", 0, 0.0f,
                 {WaveSpec{{A::Brawler}, 0.0f}}, 6, 2, 1, 1, 1, 0),
             obj(O::DestroyMarked, "WRECK THE PUMP HALLS", 3, 0.0f, {}, 4, 2, 1, 0, 1, 0),
             obj(O::ReachZone, "REACH THE FAR JUNCTION", 0, 0.0f, {}, 4, 2, 0, 1, 1, 0)},
            2930, 91008u, 4, false, 52);

        add("metro", "SCORCHED LEDGER",
            "Open contract: everything Combine-flagged in the transit district "
            "is billable at full rate for the next four minutes of satellite "
            "window. Street grid, a garrison in every block, and their "
            "accountant is in there somewhere with a long gun. Spend it well.",
            9, 0.55f,
            {obj(O::ReachZone, "MARCH TO THE DEPOT LINE", 0, 0.0f, {}, 3, 2, 1, 0, 1, 1),
             obj(O::Rampage, "DESTRUCTION WINDOW", 1400, 240.0f,
                 {WaveSpec{{A::Lancer, A::Skirmisher}, 0.0f}}, 6, 2, 2, 1, 0, 3),
             obj(O::KillTarget, "BOUNTY: THE AUDITOR", 0, 0.0f,
                 {elite(A::Sniper, "AUDITOR", "ch_viper", "lg_strider", "en_halcyon",
                        "ar_weave", "se_lidar", {"wp_lance", "wp_needle"}, "wp_lance")},
                 3, 2, 0, 0, 0, 2),
             obj(O::ReachZone, "WALK OUT OF THE GRID", 0, 0.0f, {}, 2, 1, 0, 0, 0, 2)},
            3360, 91009u, 5, false, 64);

        add("outpost", "FORTRESS GATE",
            "The ridge line is fortified end to end and their guns are "
            "pre-registered on every approach. Punch one hole and get "
            "through it before the barrage walks onto you.",
            10, 0.62f,
            // Four emplacements, two tanks, a siege spidertank and the
            // long-war pass's own overwatch on top came to eight heavy guns
            // bearing on the opening thirty seconds, and the run died there
            // five attempts running. A fortified line should be the hardest
            // thing in the act, not a wall you cannot approach: two
            // emplacements and one tank still reads as a fortress, and the
            // rocket teams behind it still punish a straight approach.
            // The FORTRESS: a strongpoint with weak points, taken in order.
            // Two shield pylons cover the gate and its towers; nothing under
            // them can be scratched until the pylons fall. Then the towers.
            // Then the gate itself - an armoured slab that takes a siege to
            // open - while the relief sieger walks in behind you.
            {obj(O::Breakthrough, "BREACH THE FORTIFIED LINE", 0, 0.0f,
                 {WaveSpec{{A::Sieger}, 55.0f}}, 4, 3, 1, 1, 2, 0),
             [&] { ObjectiveSpec o = obj(O::KillUnits, "DROP THE SHIELD GENERATORS", 2, 0.0f, {}, 3, 2, 0, 0, 1, 0);
                   o.killKind = UnitKind::ShieldPylon; o.pylons = 2; return o; }(),
             [&] { ObjectiveSpec o = obj(O::KillUnits, "SILENCE THE TOWERS", 3, 0.0f, {}, 2, 2, 0, 1, 3, 0);
                   o.killKind = UnitKind::Turret; return o; }(),
             [&] { ObjectiveSpec o = obj(O::DestroyMarked, "BREACH THE GATE", 1, 0.0f,
                                         {WaveSpec{{A::Brawler, A::Brawler}, 40.0f}}, 3, 2, 0, 1, 0, 0);
                   o.gateHealth = 520.0f; o.pylons = 1; return o; }(),
             obj(O::ReachZone, "CROSS THE KILLING GROUND", 0, 0.0f, {}, 3, 2, 0, 1, 1, 0),
             obj(O::DestroyMarked, "SILENCE THE BATTERY", 2, 0.0f, {}, 3, 2, 0, 1, 2, 0)},
            3960, 91010u, 3, true);

        // ================= ACT III : THE HEARTLAND ========================

        add("highlands", "STORM LINE",
            "Their counter-attack forms up in the high valleys. Meet it on "
            "the ridge, break it, and hold the pass until it stops coming.",
            11, 0.70f,
            {[&] { ObjectiveSpec o = obj(O::Outrun, "OUTRUN THE BARRAGE TO THE RIDGE", 0, 0.0f, {}, 3, 2, 1, 0, 0, 1);
                   o.barrageSpeed = 7.5f; return o; }(),
             obj(O::ClearHostiles, "BREAK THE FIRST ECHELON", 0, 0.0f,
                 {WaveSpec{{A::WallRunner, A::Skirmisher}, 0.0f}}, 5, 3, 2, 2, 0, 2),
             obj(O::HoldZone, "HOLD THE PASS", 0, 100.0f,
                 {WaveSpec{{A::Brawler, A::Sniper}, 0.0f}}, 5, 2, 1, 1, 0, 3)},
            4560, 91011u, 0);

        add("refinery", "COLUMN ZERO",
            "Their armoured reserve is refuelling its way down Ironworks Row "
            "in one column - hulls, carriers, everything. Break into the row, "
            "then kill it among its own fuel farms. Nothing drives out.",
            12, 0.76f,
            {obj(O::Breakthrough, "BREAK INTO THE ROW", 0, 0.0f,
                 {WaveSpec{{A::Brawler}, 0.0f}}, 4, 2, 1, 1, 2, 0),
             obj(O::Convoy, "ANNIHILATE THE RESERVE COLUMN", 6, 560.0f,
                 {WaveSpec{{A::Lancer, A::Lancer}, 0.0f}}, 2, 2, 0, 0, 2, 2),
             obj(O::DestroyMarked, "BURN THE FUEL FARMS", 2, 0.0f, {}, 2, 1, 0, 0, 1, 0),
             obj(O::KillTarget, "BOUNTY: THE ANVIL", 0, 0.0f,
                 {elite(A::Sieger, "ANVIL", "ch_ferrum", "lg_titan", "en_pyre",
                        "ar_reactive", "se_wide", {"wp_hammerfall", "wp_vulcan", "wp_javelin"},
                        "wp_hammerfall")}, 2, 2, 0, 0, 0, 0)},
            5160, 91012u, 4);

        add("oldtown", "DEEP RELAY",
            "The old quarter hides the uplink chain for their whole southern "
            "grid: masts on the tenement roofs, sappers in the alleys, and a "
            "pair of climbing frames patrolling the rooftops. Kill the chain, "
            "kill the climbers, leave.",
            13, 0.82f,
            {[&] { ObjectiveSpec o = obj(O::Blackout, "KILL THE UPLINK CHAIN", 5, 0.0f, {}, 4, 3, 1, 1, 1, 3);
                   o.sappers = 3; o.lightsOut = true; return o; }(),
             obj(O::KillTarget, "DESTROY THE PATROL FRAMES", 0, 0.0f,
                 {WaveSpec{{A::WallRunner, A::WallRunner}, 0.0f}}, 2, 1, 0, 0, 0, 2),
             [&] { ObjectiveSpec o = obj(O::ReachZone, "LEAVE THE QUARTER", 0, 0.0f, {}, 3, 1, 0, 0, 0, 2);
                   o.ambush = true; return o; }(),
             obj(O::ClearHostiles, "FIGHT OUT OF THE AMBUSH", 0, 0.0f, {}, 2, 1, 0, 0, 0, 0)},
            5760, 91013u, 6);

        add("causeway", "LAST CROSSING",
            "One bridge chain left between us and the arcology. They know it "
            "too. Get the demolition crawler to the far anchorage - lose it "
            "and there is no second crawler.",
            14, 0.88f,
            {obj(O::Escort, "ESCORT THE DEMOLITION CRAWLER", 0, 0.0f,
                 {WaveSpec{{A::Skirmisher, A::Sniper}, 0.0f}}, 4, 4, 1, 1, 2, 3),
             obj(O::KillTarget, "BOUNTY: THE HERON", 0, 0.0f,
                 {elite(A::WallRunner, "HERON", "ch_shrike", "lg_gecko", "en_milspec",
                        "ar_weave", "se_seeker", {"wp_arclight", "wp_arclight"}, "wp_arclight")},
                 2, 2, 0, 0, 1, 2),
             [&] { ObjectiveSpec o = obj(O::ReachZone, "THE CHAIN IS WIRED - GET ACROSS", 0, 0.0f, {}, 2, 2, 0, 0, 1, 2);
                   o.collapse = true; return o; }(),
             obj(O::Breakthrough, "FORCE THE ANCHORAGE", 0, 0.0f,
                 {WaveSpec{{A::Brawler, A::Brawler}, 40.0f}}, 3, 2, 1, 1, 2, 0)},
            6480, 91014u, 3, false, 40);

        add("arcology", "ACAH",
            "The arcology shell is the last address on the contract. Inside "
            "is everything they have left, and the machine they built it "
            "around. Finish this and the ledger closes.",
            15, 0.95f,
            {obj(O::Breakthrough, "BREACH THE SHELL", 0, 0.0f,
                 {WaveSpec{{A::WallRunner, A::Lancer}, 0.0f}}, 5, 3, 1, 2, 3, 2),
             obj(O::DestroyMarked, "DESTROY THE CORE PLANTS", 3, 0.0f,
                 {WaveSpec{{A::Sieger, A::Sniper}, 0.0f}}, 3, 2, 1, 1, 2, 2),
             obj(O::KillTarget, "DESTROY THE ARCHITECT", 0, 0.0f,
                 {WaveSpec{{A::Sieger, A::WallRunner, A::Brawler}, 0.0f}}, 0, 2, 0, 0, 0, 3)},
            8600, 91015u, 3, true, 56);

        // ---- the LONG WAR pass -----------------------------------------
        // Ten-to-fifteen-minute contracts: every mission becomes a chain of
        // at least four segments. Marching segments with long-range
        // OVERWATCH guns go in between the original fights - open-ground
        // stretches where tanks and emplacements engage from 150-200 m -
        // and every original fight gains overwatch of its own. Later
        // missions put a patrol spidertank on every other march.
        size_t missionIdx = 0;
        for (LevelDef& d : v) {
            const size_t mi = missionIdx++;
            // The causeway missions keep their hand-built chains: their
            // route IS the causeway, their pacing comes from the crawler,
            // and an overwatch position 150 m off that lane is open sea.
            if (d.arenaId == "causeway") continue;
            const int power = d.power;
            // A deep pool, walked from a per-mission offset. With four names
            // every contract in the campaign ran the same three orders in the
            // same order, so a seven-segment mission read as one segment
            // repeated and the player had no way to tell from the panel
            // whether they were making progress. Different words for the same
            // job is most of what "objective clarity" means over a long
            // contract.
            static const char* marchNames[] = {
                "ADVANCE UNDER OVERWATCH", "CROSS THE OPEN GROUND",
                "PUSH THE GUN LINE",       "SILENCE THE HIGH GUNS",
                "TAKE THE NEXT BLOCK",     "FORCE THE CHOKE POINT",
                "BREAK THE PICKET LINE",   "WORK UP THE ROUTE",
                "CLEAR THE APPROACHES",    "ROLL UP THE FLANK",
                "OPEN THE ROAD",           "CARRY THE CROSSING",
                "PUSH THROUGH THE KILL ZONE", "TURN THE STRONGPOINT",
                "WALK DOWN THE BATTERY",   "TAKE THE FAR SIDE"};
            const size_t nMarch = sizeof(marchNames) / sizeof(marchNames[0]);
            const size_t nameBase = (mi * 5u) % nMarch;
            std::vector<ObjectiveSpec> chain;
            size_t inserted = 0, k = 0;
            const size_t orig = d.objectives.size();
            for (size_t i = 0; i < orig; ++i) {
                if (i > 0 && orig + inserted < 12) {
                    ObjectiveSpec m2;
                    // CLEAR, not reach: an open-ground stretch is only an
                    // objective if the guns covering it must actually die.
                    m2.kind = ObjectiveKind::ClearHostiles;
                    m2.advanceOnReach = true;      // arriving is enough
                    m2.label = marchNames[(nameBase + k++) % nMarch];
                    // A route is PICKETED: scrub infantry all along it, with
                    // rocket teams as the thing that actually hurts. They are
                    // cheap to kill and constant, which is what makes the
                    // march feel like enemy ground.
                    // FEWER bodies as the campaign goes on, not more: the
                    // headcount budget is the same either way, so every slot
                    // a second-tier unit takes is a trooper that does not
                    // spawn. A route at power ten is a marksman, a mortar
                    // section, an armoured pocket and a handful of infantry -
                    // not fifteen riflemen.
                    m2.troopers = std::max(2, 4 + power / 2 - power / 4);
                    m2.atTeams = 1 + power / 3;
                    m2.tanks = (power >= 3) ? 1 + power / 6 : 0;
                    m2.apcs = (power >= 2) ? 1 + power / 6 : 1;
                    m2.drones = (power >= 3) ? 2 : 1;
                    m2.overwatch = power / 4;
                    // The second tier arrives on the march segments, which is
                    // where the player has room to react to it.
                    m2.marksmen = (power >= 5) ? 1 + (power >= 11 ? 1 : 0) : 0;
                    m2.mortars  = (power >= 7 && (k % 2u) == 1u) ? 1 : 0;
                    m2.jammers  = (power >= 8 && (k % 3u) == 1u) ? 1 : 0;
                    if (power >= 6 && (k % 2u) == 0u)
                        m2.waves.push_back(WaveSpec{{Archetype::Skirmisher}, 0.0f});
                    if (power >= 11 && (k % 3u) == 2u) m2.launchers = 1;
                    if (power >= 10 && (k % 3u) == 0u) m2.gunships = 1;
                    chain.push_back(m2);
                    ++inserted;
                }
                ObjectiveSpec o = d.objectives[i];
                // Convoys keep their own escort as the fight - stacking
                // overwatch tanks on top made the column untouchable inside
                // its timer.
                if (o.kind != ObjectiveKind::Convoy &&
                    o.kind != ObjectiveKind::KillTarget &&
                    o.kind != ObjectiveKind::KillUnits) {
                    o.overwatch = std::max(o.overwatch, power / 5);
                    // Half what it was. A real objective used to gain a
                    // squad's worth of extra infantry on top of whatever it
                    // was written with, which is how "fewer but more
                    // dangerous" kept turning back into a crowd.
                    o.troopers += 2 + power / 4;
                    o.atTeams += 1 + power / 4;
                    // A defended objective is where a WARDEN belongs: it is
                    // the thing that makes a strongpoint a strongpoint, and
                    // it turns "shoot everything" into "shoot the right thing
                    // first". Marksmen cover the approach to it.
                    if (power >= 6) o.wardens = std::max(o.wardens, 1);
                    if (power >= 8) o.marksmen += 1;
                    if (power >= 10) o.jammers = std::max(o.jammers, 1);
                    // The third tier on the real objectives: a gunship over
                    // every defended position from the second act on, rocket
                    // trucks behind the late ones, a shield pylon on the
                    // hardest, sappers in the alleys of every city.
                    if (power >= 7) o.gunships = std::max(o.gunships, 1);
                    if (power >= 9 && (i % 2u) == 1u) o.launchers = std::max(o.launchers, 1);
                    if (power >= 12 && o.kind == ObjectiveKind::DestroyMarked)
                        o.pylons = std::max(o.pylons, 1);
                    if (power >= 8 && arenaById(d.arenaId) && arenaById(d.arenaId)->urban)
                        o.sappers = std::max(o.sappers, 3);
                }
                chain.push_back(o);
            }
            // Short chains get fighting segments on the end - the last one
            // an exfil march, the rest gun lines to break.
            // Nine stops minimum on a mile-and-a-half route: a segment every
            // two hundred metres or so, which is a fight every forty seconds
            // of walking rather than one every fifteen.
            while (chain.size() < 9) {
                ObjectiveSpec m2;
                m2.kind = (chain.size() >= 8) ? ObjectiveKind::ReachZone
                                              : ObjectiveKind::ClearHostiles;
                m2.advanceOnReach = true;
                static const char* exitNames[] = {
                    "WALK OUT UNDER FIRE", "EXFILTRATE THE SECTOR",
                    "GET CLEAR OF THE GROUND", "MAKE THE PICKUP POINT"};
                m2.label = (m2.kind == ObjectiveKind::ReachZone)
                               ? exitNames[mi % 4]
                               : marchNames[(nameBase + k++) % nMarch];
                m2.troopers = std::max(2, 4 + power / 2 - power / 4);
                m2.atTeams = 1 + power / 3;
                m2.tanks = (power >= 3) ? 1 + power / 6 : 0;
                m2.apcs = (power >= 2) ? 1 + power / 6 : 1;
                m2.drones = 2;
                m2.overwatch = power / 4;
                m2.marksmen = (power >= 5) ? 1 : 0;
                m2.mortars  = (power >= 7) ? 1 : 0;
                chain.push_back(m2);
            }
            d.objectives = std::move(chain);
        }

        return v;
    }();
    return levels;
}

LevelDef endlessLevel(int index) {
    // Index 12 is the first endless mission. Difficulty keeps climbing but the
    // curve flattens, so it stays hard rather than becoming a wall.
    const int n = std::max(0, index);
    const int beyond = n - static_cast<int>(campaignLevels().size()) + 1;
    Rng rng(static_cast<uint32_t>(0x5EED0000u + n * 2654435761u));

    const std::vector<ArenaDef>& arenas = arenaCatalog();
    LevelDef d;
    d.arenaId = arenas[static_cast<size_t>(rng.next() % arenas.size())].id;
    d.name = "DEEP PATROL " + std::to_string(beyond);
    d.briefing = "No mission profile. Hostile density is above anything on "
                 "record. Kill what you can and hold the sector.";
    d.power = 12 + beyond;
    d.difficulty = clampf(0.92f + beyond * 0.01f, 0.0f, 1.0f);
    d.completionBonus = pay(7200 + beyond * 1700);
    d.seed = 0x5EED0000u + static_cast<uint32_t>(n) * 7919u;

    const int waves = 3 + std::min(3, beyond / 3);
    for (int w = 0; w < waves; ++w) {
        WaveSpec ws;
        ws.delay = (w == 0) ? 0.0f : kWaveGap;
        const int count = std::min(6, 3 + (beyond + w) / 3);
        for (int i = 0; i < count; ++i)
            ws.enemies.push_back(static_cast<Archetype>(
                rng.next() % static_cast<uint32_t>(Archetype::Count)));
        d.waves.push_back(ws);
    }
    return d;
}

// ------------------------------------------------------------------ profile

int PlayerProfile::ammoFor(const std::string& weaponId) const {
    for (const auto& e : ammoStock)
        if (e.first == weaponId) return e.second;
    return 0;
}

void PlayerProfile::addAmmo(const std::string& weaponId, int rounds) {
    if (weaponId.empty() || rounds <= 0) return;
    for (auto& e : ammoStock)
        if (e.first == weaponId) { e.second += rounds; return; }
    ammoStock.emplace_back(weaponId, rounds);
}

bool PlayerProfile::spendAmmo(const std::string& weaponId, int rounds) {
    for (auto& e : ammoStock) {
        if (e.first != weaponId) continue;
        if (e.second < rounds) return false;
        e.second -= rounds;
        return true;
    }
    return false;
}

PlayerProfile newProfile() {
    PlayerProfile p;
    const PartCatalog& cat = PartCatalog::instance();
    auto id = [&](Slot s) {
        const PartDef* d = cat.starter(s);
        return d ? d->id : std::string();
    };
    p.loadout.chassis = id(Slot::Chassis);
    p.loadout.legs = id(Slot::Legs);
    p.loadout.engine = id(Slot::Engine);
    p.loadout.armor = id(Slot::Armor);
    p.loadout.sensor = id(Slot::Sensor);
    p.loadout.ensureWeaponSlots();
    for (std::string& w : p.loadout.weapons) w = "wp_pd9";
    // Enough to buy one meaningful upgrade after the first mission, not enough
    // to skip the early curve.
    p.cash = 600;
    return p;
}

// ------------------------------------------------------------- persistence

bool saveProfile(const PlayerProfile& p, const char* path) {
    std::FILE* f = std::fopen(path, "w");
    if (!f) return false;
    std::fprintf(f, "acah_save 2\n");
    std::fprintf(f, "cash %d\n", p.cash);
    std::fprintf(f, "level %d\n", p.level);
    std::fprintf(f, "maxCleared %d\n", p.maxCleared);
    std::fprintf(f, "kills %d\n", p.kills);
    std::fprintf(f, "deaths %d\n", p.deaths);
    std::fprintf(f, "totalEarned %d\n", p.totalEarned);
    std::fprintf(f, "chassis %s\n", p.loadout.chassis.c_str());
    std::fprintf(f, "legs %s\n", p.loadout.legs.c_str());
    std::fprintf(f, "engine %s\n", p.loadout.engine.c_str());
    std::fprintf(f, "armor %s\n", p.loadout.armor.c_str());
    std::fprintf(f, "sensor %s\n", p.loadout.sensor.c_str());
    for (size_t i = 0; i < p.loadout.weapons.size(); ++i) {
        const int g = (i < p.loadout.weaponGroups.size()) ? p.loadout.weaponGroups[i] : 0;
        std::fprintf(f, "weapon %zu %d %s\n", i, g,
                     p.loadout.weapons[i].empty() ? "-" : p.loadout.weapons[i].c_str());
    }
    for (const auto& a : p.ammoStock)
        if (a.second > 0) std::fprintf(f, "ammo %s %d\n", a.first.c_str(), a.second);
    for (const std::string& u : p.unlocked) std::fprintf(f, "unlocked %s\n", u.c_str());
    std::fclose(f);
    return true;
}

bool loadProfile(PlayerProfile& p, const char* path) {
    std::FILE* f = std::fopen(path, "r");
    if (!f) return false;
    char line[256];
    if (!std::fgets(line, sizeof(line), f) ||
        std::strncmp(line, "acah_save", 9) != 0) {
        std::fclose(f);
        return false;
    }
    p = newProfile();
    p.ammoStock.clear();
    char key[64], sval[128];
    int ival = 0, ival2 = 0;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::sscanf(line, "%63s", key) != 1) continue;
        if (!std::strcmp(key, "cash") && std::sscanf(line, "%*s %d", &ival) == 1) p.cash = ival;
        else if (!std::strcmp(key, "level") && std::sscanf(line, "%*s %d", &ival) == 1) p.level = ival;
        else if (!std::strcmp(key, "maxCleared") && std::sscanf(line, "%*s %d", &ival) == 1) p.maxCleared = ival;
        else if (!std::strcmp(key, "kills") && std::sscanf(line, "%*s %d", &ival) == 1) p.kills = ival;
        else if (!std::strcmp(key, "deaths") && std::sscanf(line, "%*s %d", &ival) == 1) p.deaths = ival;
        else if (!std::strcmp(key, "totalEarned") && std::sscanf(line, "%*s %d", &ival) == 1) p.totalEarned = ival;
        else if (!std::strcmp(key, "chassis") && std::sscanf(line, "%*s %127s", sval) == 1) p.loadout.chassis = sval;
        else if (!std::strcmp(key, "legs") && std::sscanf(line, "%*s %127s", sval) == 1) p.loadout.legs = sval;
        else if (!std::strcmp(key, "engine") && std::sscanf(line, "%*s %127s", sval) == 1) p.loadout.engine = sval;
        else if (!std::strcmp(key, "armor") && std::sscanf(line, "%*s %127s", sval) == 1) p.loadout.armor = sval;
        else if (!std::strcmp(key, "sensor") && std::sscanf(line, "%*s %127s", sval) == 1) p.loadout.sensor = sval;
        else if (!std::strcmp(key, "weapon") &&
                 std::sscanf(line, "%*s %d %d %127s", &ival, &ival2, sval) == 3) {
            p.loadout.ensureWeaponSlots();
            if (ival >= 0 && ival < static_cast<int>(p.loadout.weapons.size())) {
                p.loadout.weapons[static_cast<size_t>(ival)] =
                    std::strcmp(sval, "-") ? sval : "";
                p.loadout.weaponGroups[static_cast<size_t>(ival)] = ival2;
            }
        } else if (!std::strcmp(key, "ammo") &&
                   std::sscanf(line, "%*s %127s %d", sval, &ival) == 2) {
            p.addAmmo(sval, ival);
        } else if (!std::strcmp(key, "unlocked") && std::sscanf(line, "%*s %127s", sval) == 1) {
            if (!p.hasUnlocked(sval)) p.unlocked.push_back(sval);
        }
    }
    std::fclose(f);
    // Anything the file references that the catalog does not know reverts to
    // the starter part rather than crashing the store.
    const PartCatalog& cat = PartCatalog::instance();
    auto check = [&](std::string& id, Slot slot) {
        if (!id.empty() && !cat.find(id)) {
            const PartDef* d = cat.starter(slot);
            id = d ? d->id : "";
        }
    };
    check(p.loadout.chassis, Slot::Chassis);
    check(p.loadout.legs, Slot::Legs);
    check(p.loadout.engine, Slot::Engine);
    check(p.loadout.armor, Slot::Armor);
    check(p.loadout.sensor, Slot::Sensor);
    p.loadout.ensureWeaponSlots();
    return true;
}

// ------------------------------------------------------------------ bounty

float enemyHealthScale(int power) {
    // Tuned against a scripted pilot of roughly average competence playing the
    // campaign straight through and shopping sensibly between missions. The
    // curve is deliberately shallow early: the first third of the campaign is
    // where the player is still learning to lead a shot, and a machine that
    // out-lasts them there reads as unfair rather than hard.
    // Eased down after playtesting said the middle of the campaign was a wall.
    // Enemies are still tougher every mission, just on a gentler slope, and
    // the ceiling is lower so the endless ladder does not become a sponge.
    // Wave 15: the first act's spidertanks were "WAY too tough" in hull and
    // plate. Lower floor and a gentler start; the late curve is unchanged.
    return clampf(0.19f + static_cast<float>(power) * 0.036f, 0.19f, 1.05f);
}

float enemyArmorScale(int power) {
    // Hostile plate is thinned in the first act (half at power 1, full by
    // power 8) so an early enemy machine dies to the guns the player can
    // afford, and the armour meta arrives with the tiers that answer it.
    return clampf(0.45f + static_cast<float>(power) * 0.07f, 0.45f, 1.0f);
}

float enemyDamageScale(int power) {
    // Hostile fire starts at just over a third of its rating and reaches full
    // strength around the eleventh mission. The player's own guns are never
    // scaled - what they buy is what it does - so the curve reads as the
    // opposition getting more dangerous rather than the player's kit getting
    // quietly better, which is the honest version of the same difficulty ramp.
    // Wave 15: raised - "little reason to take cover or dodge". Half rating
    // on the first contract, full by the tenth, a touch over after.
    return clampf(0.42f + static_cast<float>(power) * 0.052f, 0.42f, 1.15f);
}

int bountyFor(const Loadout& l, const MechStats& s, int power) {
    const float threat = threatRating(l, s);
    // Roughly linear in threat with a mild power multiplier, so a tough enemy
    // in a late mission is worth several early ones but not a hundred.
    // Calibrated so a player who clears every mission can afford roughly one
    // meaningful upgrade per mission across the campaign. Under-paying here
    // does not make the game harder in an interesting way - it just means the
    // enemy curve outruns the machine you are allowed to build.
    // Cut hard from what it used to be. A pair of enemy spidertanks was
    // paying out nearly eight thousand credits - more than the wrecking, the
    // objectives, the time bonus and the completion bonus of that mission put
    // together - which meant the carefully drawn completion curve decided
    // nothing at all and one lucky boss kill bought two tiers of upgrade.
    // Killing an enemy machine should be a good day's work, not a jackpot.
    const float payout = threat * 2.6f * (0.85f + power * 0.045f) * kPayScale;
    return std::max(15, static_cast<int>(payout));
}

// ----------------------------------------------------------------- mission

void Mission::begin(const LevelDef& level, PlayerProfile& profile) {
    level_ = level;
    rng_ = Rng(level.seed ? level.seed : 1u);
    combat_.reset();
    combat_.setHostileDamageScale(enemyDamageScale(level.power));
    shots_.clear();
    salvage_.clear();

    const ArenaDef* arena = arenaById(level.arenaId);
    world_.generate(arena ? *arena : arenaByIndex(0), level.seed);

    mechs_.clear();
    brains_.clear();
    // Reserve up front: the mech vector must never reallocate mid-mission,
    // because projectiles hold indices into it and the AI holds pointers.
    mechs_.reserve(1 + kMaxEnemiesAlive * 2);
    brains_.reserve(1 + kMaxEnemiesAlive * 2);

    Mech player;
    const Vec3 spawn = world_.findSpawnPoint(rng_, Vec3(0.0f, 0.0f, 0.0f), 0.0f, 20.0f);
    player.init(world_, profile.loadout, spawn, rng_.range(0.0f, 6.28f),
                Team::Player, level.seed * 31u + 5u);
    player.setIndex(0);
    // Load the magazines the player owns into the guns that take them.
    for (MountedWeapon& w : player.weapons()) {
        if (!w.part || w.part->weapon.ammo != AmmoKind::Limited) continue;
        w.rounds = w.part->weapon.magazine;
        w.reserve = profile.ammoFor(w.part->id);
    }
    mechs_.push_back(std::move(player));
    brains_.emplace_back();

    // Briefing, not Fighting. The first wave is spawned so the sector is
    // populated behind the briefing text, but nothing hostile acts until the
    // player has actually deployed - being shot at while reading the mission
    // brief is not tension, it is a bug.
    phase_ = MissionPhase::Briefing;
    waveIndex_ = -1;
    waveTimer_ = 0.0f;
    elapsed_ = 0.0f;
    endTimer_ = 0.0f;
    cashEarned_ = 0;
    quietFor_ = 0.0f;
    hunting_ = false;
    stuckFor_.assign(1, 0.0f);
    lastEnemyPos_.assign(1, Vec3(0.0f, 0.0f, 0.0f));
    defenseTimer_.assign(1, 0.0f);

    // ---- the ACAH layer ---------------------------------------------------
    ledger_ = MissionLedger();
    units_.clear();
    props_.clear();
    markedProps_.clear();
    markedUnits_.clear();
    markedMechs_.clear();
    escortUnit_ = -1;
    alarm_ = 0.0f;
    objectiveIndex_ = -1;
    objectiveProgress_ = objectiveTarget_ = 0;
    objectiveTimer_ = 0.0f;
    blackout_ = blackoutTarget_ = 0.0f;
    barrageAlong_ = -1e9f;
    barrageTimer_ = 0.0f;
    collapseTimer_ = -1.0f;
    collapsed_ = false;
    ambushSprung_ = escortAmbushSprung_ = false;

    // A mission without an explicit objective chain is the old shape: one
    // segment, clear everything. Endless patrols still use it.
    if (level_.objectives.empty()) {
        ObjectiveSpec clear;
        clear.kind = ObjectiveKind::ClearHostiles;
        clear.label = "DESTROY ALL HOSTILE MACHINES";
        clear.waves = level_.waves;
        level_.objectives.push_back(clear);
    }

    // The mission runs along a line: the pilot starts at one end and the
    // objectives are laid down the lane, which is what makes a level a place
    // you fight THROUGH rather than an arena you stand in.
    {
        // The lane runs along the arena's structural grain on every map: the
        // causeway chain over water, the street grid downtown, the line of a
        // canyon or gallery run. Objectives then follow the geometry the
        // generator actually built, which is what makes a mission read as a
        // route rather than a scavenger hunt.
        // Note the convention flip: structures use (cos, sin) in xz, the
        // mission axis uses (sin, cos) - the angle converts as PI/2 - a.
        laneFlip_ = rng_.unit() < 0.5f;
        const float ang = (PI * 0.5f - world_.mainAxisAngle()) + (laneFlip_ ? PI : 0.0f);
        missionAxis_ = Vec3(std::sin(ang), 0.0f, std::cos(ang));
        const Vec3 start = lanePoint(0.0f);
        Mech& p = mechs_[0];
        p.init(world_, profile.loadout, start,
               std::atan2(missionAxis_.x, missionAxis_.z), Team::Player,
               level_.seed * 31u + 5u);
        p.setIndex(0);
        for (MountedWeapon& w : p.weapons()) {
            if (!w.part || w.part->weapon.ammo != AmmoKind::Limited) continue;
            w.rounds = w.part->weapon.magazine;
            w.reserve = profile.ammoFor(w.part->id);
        }
        checkpointPos_ = p.position();
    }

    // Furnish the arena with things worth destroying, concentrated along the
    // mission lane so the destruction economy lines the route.
    props_ = furnishArena(world_, level_.seed * 7919u + 3u,
                          level_.propBudget + level_.propBudget / 2,
                          missionAxis_);

    startObjective(0);
}

void Mission::beginCombat() {
    if (phase_ == MissionPhase::Briefing) phase_ = MissionPhase::Fighting;
}

void Mission::spawnWave(int index) {
    if (index < 0 || index >= static_cast<int>(level_.waves.size())) return;
    waveIndex_ = index;
    spawnMechWave(level_.waves[static_cast<size_t>(index)], mechs_[0].position());
}

void Mission::spawnMechWave(const WaveSpec& w, const Vec3& around) {
    // Reactive plating is good for one heavy hit per wave, on both sides.
    for (Mech& m : mechs_) m.resetWaveState();

    for (Archetype a : w.enemies) {
        if (static_cast<int>(mechs_.size()) >= 1 + kMaxEnemiesAlive * 3) break;

        Loadout l = w.elite ? w.eliteLoadout : enemyLoadout(a, level_.power, rng_);
        if (w.elite) l.ensureWeaponSlots();
        const MechStats st = deriveStats(l);

        // Snipers and wall-runners want distance; brawlers start closer so the
        // fight opens immediately rather than with a long walk.
        const float minD = (a == Archetype::Sniper) ? 70.0f
                         : (a == Archetype::Stalker) ? 105.0f
                         : (a == Archetype::Brawler) ? 34.0f : 48.0f;
        Vec3 spawn = world_.findSpawnPoint(rng_, around, minD, minD + 55.0f);

        // A stalker starts ON A ROOF. This is not a convenience: on the ground
        // it is slower than the player, so a pilot who simply walks at it can
        // run it down before it ever reaches a wall, and the encounter the
        // mission promises - a gunman up in the towers who will not be where
        // you last saw him - never happens at all. Putting it where it belongs
        // at the first frame means the fight opens the way it is meant to, and
        // everything the brain does from there is about STAYING up.
        if (a == Archetype::Stalker) {
            // Measured from the PLAYER, not from the objective zone. This was
            // the bug that made the whole encounter a no-show: the zone for a
            // segment sits a long way up the route from where the pilot is
            // standing when it starts, so the stalker was set up six hundred
            // metres away - twice its own sensor range - and simply stood on
            // a roof doing nothing at all, which is exactly what was reported.
            const Vec3 from = mechs_[0].position();
            const Obstacle* perch = nullptr;
            float bestScore = 1e9f;
            for (const Obstacle& o : world_.obstacles()) {
                if (!o.active || !o.climbable) continue;
                const float h = o.half.y * 2.0f;
                if (h < 13.0f) continue;                 // a real tower only
                const float d = lengthXZ(o.center - from);
                // In sight from the first frame. A duel that opens with a
                // two-minute walk is not an opening, and out past its sensor
                // range the machine has no target and no behaviour.
                if (d < 75.0f || d > 155.0f) continue;
                // A FIRING position, not the highest point in the district.
                // The turret depresses 38 degrees; from the top of the tallest
                // ruin on the map the street below is out of arc, and the
                // encounter opens with a sniper who cannot shoot.
                const float score = std::fabs(d - 110.0f) * 0.6f +
                                    std::fabs(h - 26.0f) * 2.0f;
                if (score < bestScore) { bestScore = score; perch = &o; }
            }
            if (perch) {
                const Vec3 top(perch->center.x,
                               perch->center.y + perch->half.y + 2.6f,
                               perch->center.z);
                // Only if the deck is really standing there: a collapsed ruin
                // with a hole where its roof was would drop the machine
                // straight through the building it is supposed to be on.
                const SurfaceHit deck = world_.findFoothold(
                    top + Vec3(0.0f, 3.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f),
                    3.5f, 7.0f);
                if (deck.hit && deck.normal.y > 0.7f)
                    spawn = Vec3(top.x, deck.point.y + 2.2f, top.z);
            }
            if (!perch) {
                // No tower in the window: put it on the ground but IN RANGE,
                // and let it find its own roof. Anything is better than a
                // machine parked outside its own sensors.
                spawn = world_.findSpawnPoint(rng_, mechs_[0].position(),
                                              95.0f, 150.0f);
            }
        }

        Mech e;
        e.init(world_, l, spawn, rng_.range(0.0f, 6.28f), Team::Hostile,
               rng_.next() | 1u);
        e.setIndex(static_cast<int>(mechs_.size()));
        // Early enemies are deliberately fragile. The part catalog's cheapest
        // hull is still a real machine, so without a scale the first mission's
        // "derelict patrol units" would be as tough as the player is.
        // The early-enemy health scale exists so the first missions are not
        // fought against machines as tough as the player's. A named set-piece
        // is the opposite case: at power two the scale took MAGPIE from 224
        // structure to SEVENTY-SIX, so the duellist the mission is built
        // around died to one burst. It keeps a floor of its own.
        const float hpScale = (a == Archetype::Stalker)
                                  ? std::max(enemyHealthScale(level_.power), 1.15f)
                                  : enemyHealthScale(level_.power);
        e.scaleHealth(hpScale * (level_.bossMission ? 1.8f : 1.0f));
        if (!(a == Archetype::Stalker && level_.power <= 3))
            e.scaleArmor(enemyArmorScale(level_.power));
        // A stalker is a named set-piece, not a line unit: it is the hardest
        // single machine in the early game to actually corner, and cornering
        // it should be worth the twenty minutes it takes. Its own frame is
        // feather-light, so the threat formula - which prices armour and guns
        // - badly under-rates what it costs the player to kill.
        const float bountyMul = (level_.bossMission ? 2.0f : 1.0f) *
                                ((a == Archetype::Stalker) ? 2.6f : 1.0f) *
                                (w.elite ? 2.2f : 1.0f);
        e.setBounty(static_cast<int>(bountyFor(l, st, level_.power) * bountyMul));
        // A bounty elite is a named machine with a real crew: it keeps more
        // of its structure than the line does, and it says who it is.
        if (w.elite) {
            e.setCallsign(w.callsign);
            e.scaleHealth(1.35f);
        }
        e.refillAmmo();
        mechs_.push_back(std::move(e));

        AiController brain;
        brain.init(configFor(a, level_.difficulty, rng_), rng_.next() | 1u);
        brain.setHunting(hunting_);
        brains_.push_back(brain);
        stuckFor_.push_back(0.0f);
        lastEnemyPos_.push_back(mechs_.back().position());
        defenseTimer_.push_back(0.0f);
    }
}

int Mission::enemiesAlive() const {
    int n = 0;
    for (size_t i = 1; i < mechs_.size(); ++i)
        if (mechs_[i].alive()) ++n;
    return n;
}

const Mech* Mission::nearestEnemy() const {
    const Mech* best = nullptr;
    float bestD = 1e9f;
    for (size_t i = 1; i < mechs_.size(); ++i) {
        if (!mechs_[i].alive()) continue;
        const float d = lengthSq(mechs_[i].position() - mechs_[0].position());
        if (d < bestD) { bestD = d; best = &mechs_[i]; }
    }
    return best;
}

// The last line of defence against a mission that cannot be finished. An enemy
// that has stopped moving entirely - wedged in a canyon wall, dropped onto a
// ledge it cannot leave - is not a challenge, it is a dead end. Rather than
// leave the player walking the map hunting a machine that will never come,
// pick it up and put it back into the fight.
void Mission::rescueStuckEnemies(float dt) {
    for (size_t i = 1; i < mechs_.size(); ++i) {
        Mech& m = mechs_[i];
        if (!m.alive()) { stuckFor_[i] = 0.0f; continue; }

        const float moved = length(m.position() - lastEnemyPos_[i]);
        lastEnemyPos_[i] = m.position();
        // Only count it as stuck while the machine is actually trying to close.
        const bool shouldBeMoving = hunting_ || brains_[i].state() == AiState::Approach;
        if (shouldBeMoving && moved < 0.02f * (dt * 60.0f)) stuckFor_[i] += dt;
        else stuckFor_[i] = std::max(0.0f, stuckFor_[i] - dt * 3.0f);

        // Two ways to earn a relocation: this machine personally stopped
        // moving, or the whole fight has been silent long enough that nobody
        // is finding anybody. The second catches the cases the first cannot -
        // a machine happily circling a spot the player will never walk to.
        // A stalker being out of contact is the stalker working; it gets
        // a long rope before the relocation rule fires.
        const bool longSilence = quietFor_ > (brains_[i].isStalker() ? 80.0f : kSilenceRelocate);
        if (stuckFor_[i] > 6.0f || longSilence) {
            // Re-initialising is the only way to reset the gait cleanly, but it
            // also restores full health - so carry the damage across, otherwise
            // being stuck would be a reward.
            const float hurt = m.healthFraction();
            const int bounty = m.bounty();
            // Put it somewhere the player can actually be shot from. A spawn
            // point that merely exists is not enough - on stepped terrain the
            // machine can land on a shelf it cannot leave and the fight stalls
            // again ten seconds later. Insist on line of sight, and fall back
            // to a plain spawn only if nothing clear turns up.
            Vec3 to = world_.findSpawnPoint(rng_, mechs_[0].position(), 42.0f, 70.0f);
            for (int tries = 0; tries < 12; ++tries) {
                const Vec3 cand = world_.findSpawnPoint(rng_, mechs_[0].position(),
                                                        30.0f, 55.0f);
                if (world_.insideStructure(cand, 3.0f)) continue;
                if (world_.lineOfSight(cand + Vec3(0.0f, 2.0f, 0.0f),
                                       mechs_[0].hitCentre())) {
                    to = cand;
                    break;
                }
            }
            m.init(world_, m.loadout(), to, rng_.range(0.0f, 6.28f), Team::Hostile,
                   rng_.next() | 1u);
            m.setIndex(static_cast<int>(i));
            m.scaleHealth(enemyHealthScale(level_.power));
            m.scaleArmor(enemyArmorScale(level_.power));
            m.applyDamage(m.health() * (1.0f - hurt), Vec3(0.0f, 1.0f, 0.0f));
            m.setBounty(bounty);
            m.refillAmmo();
            stuckFor_[i] = 0.0f;
            lastEnemyPos_[i] = m.position();
            relocated_ = true;
        }
    }
    // One relocation pass per silence, not one per frame.
    if (relocated_) { quietFor_ = 0.0f; relocated_ = false; }
}

void Mission::handleDestroyed(const CombatEvents& ev) {
    for (int idx : ev.destroyed) {
        if (idx <= 0 || idx >= static_cast<int>(mechs_.size())) continue;
        Mech& m = mechs_[static_cast<size_t>(idx)];
        cashEarned_ += m.bounty();
        ledger_.mechKills += 1;
        ledger_.cashKills += m.bounty();
        // Wreck: a big bloom plus whatever heavy ammunition it was carrying.
        combat_.explosion(m.hitCentre(), 3.4f, Vec3(1.0f, 0.72f, 0.34f), rng_);
        combat_.dropSalvage(m, rng_);
        // A bounty elite's signature weapon comes off the wreck.
        if (!m.callsign().empty()) {
            ledger_.eliteKilled = m.callsign();
            const ObjectiveSpec* spec = currentObjective();
            if (spec)
                for (const WaveSpec& w : spec->waves)
                    if (w.elite && w.callsign == m.callsign() && !w.dropWeapon.empty())
                        ledger_.salvagedPart = w.dropWeapon;
            combat_.explosion(m.hitCentre(), 5.5f, Vec3(1.0f, 0.85f, 0.45f), rng_);
        }
    }
}

std::string Mission::eliteCallsign() const {
    for (int i : markedMechs_)
        if (i > 0 && i < static_cast<int>(mechs_.size()) && mechs_[static_cast<size_t>(i)].alive() &&
            !mechs_[static_cast<size_t>(i)].callsign().empty())
            return mechs_[static_cast<size_t>(i)].callsign();
    return std::string();
}

Vec3 Mission::hiddenSpot(const Vec3& near, const Vec3& from) {
    // Sample around `near`; keep the candidate that is closest to a wall and
    // out of sight of `from`. Alleys and building corners win; open ground
    // only if there is nothing else.
    Vec3 best = near;
    float bestScore = -1e9f;
    for (int i = 0; i < 18; ++i) {
        const float ang = rng_.range(0.0f, TAU);
        const float rad = rng_.range(6.0f, 34.0f);
        Vec3 c = near + Vec3(std::sin(ang) * rad, 0.0f, std::cos(ang) * rad);
        if (world_.insideStructure(c, 1.2f)) continue;
        if (world_.hasWater() && world_.terrain().height(c.x, c.z) < world_.waterLevel() + 0.3f) continue;
        c.y = world_.terrain().height(c.x, c.z);
        float wall = 1e9f;
        for (const Obstacle& o : world_.obstacles()) {
            if (!o.active || o.kind != ObstacleKind::Building || o.half.y < 2.5f) continue;
            const float d = lengthXZ(o.center - c) - std::max(o.half.x, o.half.z);
            if (d < wall) wall = d;
        }
        const bool seen = world_.lineOfSight(c + Vec3(0.0f, 1.4f, 0.0f), from + Vec3(0.0f, 2.5f, 0.0f));
        const float score = (seen ? 0.0f : 40.0f) - std::min(wall, 40.0f);
        if (score > bestScore) { bestScore = score; best = c; }
    }
    return best;
}

void Mission::spawnAmbush(const Vec3& at) {
    // Out of the alleys: rocket teams first, riflemen behind, one vehicle
    // pulling round the corner, and at the higher powers a pack of sappers.
    const int p = level_.power;
    const int atTeams = 2 + p / 5;
    const int troopers = 3 + p / 4;
    for (int i = 0; i < atTeams; ++i)
        spawnUnitAlerted(UnitKind::ATTrooper, hiddenSpot(at, at), 0, Vec3(0.0f));
    for (int i = 0; i < troopers; ++i)
        spawnUnitAlerted(UnitKind::Trooper, hiddenSpot(at, at), 0, Vec3(0.0f));
    spawnUnitAlerted(p >= 7 ? UnitKind::Tank : UnitKind::APC,
                     at + missionAxis_ * 38.0f + Vec3(rng_.range(-15.0f, 15.0f), 0.0f, 0.0f), 0, Vec3(0.0f));
    if (p >= 8)
        for (int i = 0; i < 3; ++i)
            spawnUnitAlerted(UnitKind::Sapper, hiddenSpot(at, at), 0, Vec3(0.0f));
    if (p >= 11) spawnUnitAlerted(UnitKind::Gunship, at - missionAxis_ * 60.0f, 0, Vec3(0.0f));
    alarm_ = 1.0f;
}

void Mission::collapseDecksBehind(float along) {
    // Every causeway deck (and the pillars under it) behind the line goes,
    // with a chain of blasts down the road so the pilot sees it happen.
    const float startAlong = dot(checkpointPos_, missionAxis_);
    int blown = 0;
    for (const Obstacle& o : world_.obstacles()) {
        if (!o.active || o.kind != ObstacleKind::Building) continue;
        if (o.half.y > 0.5f || std::max(o.half.x, o.half.z) < 10.0f) continue;   // decks only
        const float a = dot(o.center, missionAxis_);
        if (a > along || a < startAlong - 12.0f) continue;
        if (world_.demolishNear(o.center, 4.0f) > 0) {
            combat_.explosion(o.center + Vec3(0.0f, 1.0f, 0.0f), 6.0f, Vec3(1.0f, 0.6f, 0.3f), rng_);
            ++blown;
        }
    }
    (void)blown;
    collapsed_ = true;
}

void Mission::fireShell(const Vec3& at, float scatter) {
    static WeaponDef shell = [] {
        WeaponDef w;
        w.damage = 8.0f;
        w.projectileSpeed = 70.0f;
        w.blastRadius = 6.0f;
        w.blastDamage = 34.0f;
        w.gravity = -22.0f;
        w.range = 700.0f;
        w.tracerLength = 2.2f;
        w.tracerRadius = 0.2f;
        w.tracerColor = Vec3(1.0f, 0.5f, 0.2f);
        return w;
    }();
    const Vec3 fall = at + Vec3(rng_.range(-scatter, scatter), 0.0f, rng_.range(-scatter, scatter));
    ShotRequest sr;
    sr.origin = fall + Vec3(rng_.range(-20.0f, 20.0f), 85.0f, rng_.range(-20.0f, 20.0f));
    sr.direction = normalize(fall - sr.origin);
    sr.weapon = &shell;
    sr.team = Team::Hostile;
    sr.shooter = -1;
    shots_.push_back(sr);
}

void Mission::updateShields() {
    // Everything hostile under a live pylon is immune this frame - the
    // pylon itself excepted, so the answer is always the same: kill the
    // pylon first. The player is never shielded by an enemy pylon.
    std::vector<std::pair<Vec3, float>> domes;
    for (const Unit& u : units_)
        if (u.alive() && u.kind() == UnitKind::ShieldPylon && u.team() == Team::Hostile)
            domes.push_back({u.position(), u.stats().supportRadius});
    auto covered = [&](const Vec3& p) {
        for (const auto& d : domes)
            if (lengthSq(flattenY(p - d.first)) < d.second * d.second) return true;
        return false;
    };
    for (Unit& u : units_) {
        if (!u.alive()) continue;
        u.setShielded(u.team() == Team::Hostile && u.kind() != UnitKind::ShieldPylon &&
                      !domes.empty() && covered(u.position()));
    }
    for (Destructible& d : props_) d.shielded = !domes.empty() && covered(d.pos);
    for (size_t i = 1; i < mechs_.size(); ++i)
        if (mechs_[i].alive())
            mechs_[i].setShielded(!domes.empty() && covered(mechs_[i].position()));
}

void Mission::update(float dt, const MechInput& playerInput) {
    if (phase_ == MissionPhase::Cleared || phase_ == MissionPhase::Failed) {
        // Keep simulating so the wreck and the explosions play out.
        endTimer_ += dt;
    }
    elapsed_ += dt;

    shots_.clear();

    // Player first, so the AI reacts to where the player is this frame.
    if (mechs_[0].alive())
        mechs_[0].update(dt, world_, playerInput, shots_);

    // During the briefing the hostiles hold: they still animate, so the sector
    // is not a freeze-frame, but they do not hunt, aim or fire.
    const bool briefing = (phase_ == MissionPhase::Briefing);
    const Mech* target = mechs_[0].alive() ? &mechs_[0] : nullptr;
    for (size_t i = 1; i < mechs_.size(); ++i) {
        if (!mechs_[i].alive()) continue;
        MechInput in;
        if (briefing) {
            // Stand and idle, turret forward.
            in.aimPoint = mechs_[i].position() + mechs_[i].forward() * 40.0f;
        } else {
            in = brains_[i].think(dt, world_, mechs_[i], target);
        }
        mechs_[i].update(dt, world_, in, shots_);
    }

    rescueStuckEnemies(dt);

    // ---- the small war ----------------------------------------------------
    // Each unit fights the nearest thing it hates: the player, or the escort
    // charge when one is on the road (the column exists to be attacked, and an
    // AI that ignores it makes escorting pointless).
    if (!briefing) {
        const Vec3 playerPos = mechs_[0].hitCentre();
        for (size_t i = 0; i < units_.size(); ++i) {
            Unit& u = units_[i];
            if (!u.alive()) { u.tickDead(dt); continue; }
            Vec3 target = playerPos;
            if (u.team() == Team::Player) {
                // The escort shoots hostiles near it.
                float bestD = 1e9f;
                for (const Unit& h : units_) {
                    if (!h.alive() || h.team() != Team::Hostile) continue;
                    const float d = lengthSq(h.position() - u.position());
                    if (d < bestD) { bestD = d; target = h.hitCentre(); }
                }
            } else if (escortUnit_ >= 0 &&
                       units_[static_cast<size_t>(escortUnit_)].alive()) {
                // The crawler draws fire only from units the player is not
                // pressuring. A ten-metre walking tank inside your engagement
                // envelope IS the priority target - everything focusing the
                // escort while ignoring the thing shredding them made escorts
                // unkeepable and read as suicidal AI both at once.
                const Unit& esc = units_[static_cast<size_t>(escortUnit_)];
                const float dPlayer = length(playerPos - u.position());
                const float dEsc = length(esc.position() - u.position());
                if (dEsc < dPlayer && dPlayer > u.stats().engageRange * 0.65f)
                    target = esc.hitCentre();
            }
            // Line of sight, then SIGNATURE. A machine running dark is not
            // invisible - walk into a trooper's lap and he will shoot you -
            // but the range at which a gun line picks you up collapses, which
            // is what makes a stealth reactor a real way to cross open ground.
            bool visible =
                world_.lineOfSight(u.hitCentre() + Vec3(0.0f, 0.4f, 0.0f), target);
            // The lights are out: nothing without its own sensors sees past
            // a stone's throw. The pilot's radar still works.
            if (visible && blackout_ > 0.5f && u.team() == Team::Hostile &&
                lengthSq(target - u.hitCentre()) > 55.0f * 55.0f)
                visible = false;
            if (visible && u.team() == Team::Hostile) {
                const bool atPlayer =
                    lengthSq(target - mechs_[0].hitCentre()) < 4.0f;
                if (atPlayer) {
                    const float sig = mechs_[0].signature();
                    if (sig < 0.999f &&
                        length(target - u.hitCentre()) >
                            u.stats().engageRange * sig)
                        visible = false;
                }
            }
            // Hand the gunner the target's velocity: it leads with it, and
            // a fast crossing target spoils its aim.
            const Vec3 tVel = (lengthSq(target - mechs_[0].hitCentre()) < 4.0f)
                                  ? mechs_[0].velocity() : Vec3(0.0f);
            u.setTargetProfile(
                (lengthSq(target - mechs_[0].hitCentre()) < 4.0f &&
                 mechs_[0].stats().trait == Trait::LowProfile) ? 1.45f : 1.0f);
            u.update(dt, world_, target, visible, shots_, static_cast<int>(i), tVel);
        }

        // ---- EMP surge ----------------------------------------------------
        // The player's reactor discharging. Drones are aircraft with no
        // shielding and simply fall; everything else nearby has its fire
        // control knocked out for a few seconds. This is the crowd answer, and
        // it is the reason to fit an oversized core in a machine that cannot
        // otherwise use eleven megawatts.
        if (mechs_[0].takeEmpPulse()) {
            empFlash_ = true;
            const Vec3 at = mechs_[0].position();
            combat_.explosion(mechs_[0].hitCentre(), 9.0f,
                              Vec3(0.55f, 0.85f, 1.0f), rng_);
            for (Unit& u : units_) {
                if (!u.alive() || u.team() != Team::Hostile) continue;
                const float d = length(u.position() - at);
                if (d > 62.0f) continue;
                if (u.kind() == UnitKind::Drone || u.kind() == UnitKind::Sapper) {
                    u.applyDamage(1e5f);          // out of the sky / the charge fizzles
                    onUnitKilled(static_cast<int>(&u - &units_[0]));
                } else {
                    // Two to five seconds of silence, closer means longer.
                    u.suppress(5.0f - 3.0f * clampf(d / 62.0f, 0.0f, 1.0f));
                }
            }
        }

        // ---- support units ------------------------------------------------
        // The two kinds whose weapon is not a gun. A Warden patches the
        // armour around it, so a strongpoint holding one does not fall until
        // the Warden does - which is the first time in this game that the
        // ORDER you kill things in matters. A Jammer blinds the player's fire
        // control and radar inside its bubble; ignoring it is legal and
        // expensive, and killing it is instantly, obviously worth it.
        // A held reinforcement wave arriving.
        if (pendingWave_ >= 0) {
            pendingWaveTimer_ -= dt;
            if (pendingWaveTimer_ <= 0.0f) {
                const ObjectiveSpec& sp =
                    level_.objectives[static_cast<size_t>(objectiveIndex_)];
                if (pendingWave_ < static_cast<int>(sp.waves.size()))
                    spawnMechWave(sp.waves[static_cast<size_t>(pendingWave_)],
                                  mechs_[0].position());
                pendingWave_ = -1;
            }
        }

        repairPulses_.clear();
        repairPulseTimer_ -= dt;
        jamStrength_ = 0.0f;
        for (const Unit& u : units_) {
            if (!u.alive() || u.team() != Team::Hostile) continue;
            const UnitStats& us = u.stats();
            if (us.supportRadius <= 0.0f) continue;
            if (u.kind() == UnitKind::Jammer) {
                const float d = length(flattenY(mechs_[0].position() - u.position()));
                if (d < us.supportRadius) {
                    // Strongest at the centre, fading to nothing at the rim,
                    // so a pilot can feel their systems coming back as they
                    // pull away and knows which way "away" is.
                    jamStrength_ = std::max(jamStrength_,
                                            1.0f - d / us.supportRadius);
                }
            }
        }
        for (size_t i = 0; i < units_.size(); ++i) {
            const Unit& src = units_[i];
            if (!src.alive() || src.team() != Team::Hostile) continue;
            const UnitStats& us = src.stats();
            if (us.supportRate <= 0.0f) continue;
            for (size_t j = 0; j < units_.size(); ++j) {
                if (i == j) continue;
                Unit& tgt = units_[j];
                if (!tgt.alive() || tgt.team() != Team::Hostile) continue;
                if (tgt.healthFraction() > 0.995f) continue;
                if (length(flattenY(tgt.position() - src.position())) > us.supportRadius)
                    continue;
                tgt.repair(us.supportRate * dt);
                // Report it, a few times a second rather than every frame:
                // the player needs to know their damage is being undone, and
                // by what, but not sixty times a second.
                if (repairPulseTimer_ <= 0.0f && repairPulses_.size() < 6) {
                    repairPulses_.push_back(tgt.hitCentre());
                    repairPulseTimer_ = 0.45f;
                }
            }
        }

        // Sappers arriving. The charge is an EMP: forty points of structure,
        // the fire control knocked sideways, the reactor spiked.
        for (size_t i = 0; i < units_.size(); ++i) {
            Unit& u = units_[i];
            if (!u.alive() || u.kind() != UnitKind::Sapper || !u.armedToBlow()) continue;
            combat_.explosion(u.hitCentre(), 5.0f, Vec3(0.6f, 0.85f, 1.0f), rng_);
            mechs_[0].applyDamage(42.0f * enemyDamageScale(level_.power) + 8.0f,
                                  normalize(mechs_[0].position() - u.position() + Vec3(0.0f, 0.2f, 0.0f)));
            mechs_[0].applyImpulse(normalize(flattenY(mechs_[0].position() - u.position()) +
                                             Vec3(1e-3f, 0.0f, 0.0f)) * 3.0f + Vec3(0.0f, 2.5f, 0.0f));
            jamStrength_ = std::max(jamStrength_, 1.0f);
            u.applyDamage(1e5f);
            onUnitKilled(static_cast<int>(i));
        }

        updateShields();

        // Crushing. Walking a ten-metre machine over a power-suit trooper
        // resolves exactly the way it should.
        for (size_t i = 0; i < units_.size(); ++i) {
            Unit& u = units_[i];
            if (!u.alive() || !u.stats().crushable) continue;
            for (const Mech& m : mechs_) {
                if (!m.alive()) continue;
                const float d = length(flattenY(u.position() - m.position()));
                if (d < m.hitRadius() * 0.9f &&
                    std::fabs(u.position().y - (m.position().y - m.stats().standHeight)) < 2.4f) {
                    u.applyDamage(1000.0f);
                    onUnitKilled(static_cast<int>(i));
                    break;
                }
            }
        }
    }

    combat_.spawnShots(shots_, rng_);

    // ---- specialised sensor automation ------------------------------------
    // Each of these is one sensor doing one thing near-perfectly. They run
    // for every machine that carries the sensor, the player's and the
    // enemy's alike - an automation you can buy is an automation you can meet.
    if (!briefing) {
        while (defenseTimer_.size() < mechs_.size()) defenseTimer_.push_back(0.0f);
        for (size_t i = 0; i < mechs_.size(); ++i) {
            Mech& m = mechs_[i];
            defenseTimer_[i] = std::max(0.0f, defenseTimer_[i] - dt);
            if (!m.alive() || defenseTimer_[i] > 0.0f) continue;
            const MechStats& st = m.stats();
            const int pd = static_cast<int>(Ability::PointDefense);
            const int dodge = static_cast<int>(Ability::AutoDodge);
            if (st.hasPassive[pd]) {
                const float radius = 20.0f + 8.0f * st.passivePower[pd];
                if (combat_.interceptOne(m.hitCentre(), radius, m.team())) {
                    defenseTimer_[i] = 1.9f;
                    continue;
                }
            }
            if (st.hasPassive[dodge]) {
                // Something big, close, and closing: step out of its line.
                for (const Projectile& pr : combat_.projectiles()) {
                    if (!pr.alive || pr.team == m.team()) continue;
                    if (pr.damage < 18.0f) continue;      // small arms are armour's job
                    const Vec3 to = m.hitCentre() - pr.pos;
                    const float d = length(to);
                    if (d > 42.0f) continue;
                    const Vec3 dir = normalize(pr.vel);
                    if (dot(dir, to) / std::max(d, 0.1f) < 0.86f) continue;
                    // Perpendicular escape, away from the round's line.
                    const Vec3 line = to - dir * dot(to, dir);
                    Vec3 side = (lengthSq(line) > 1e-4f)
                                    ? normalize(line)
                                    : normalize(cross(dir, Vec3(0.0f, 1.0f, 0.0f)));
                    m.applyImpulse(side * st.passivePower[dodge]);
                    defenseTimer_[i] = 3.2f;
                    break;
                }
            }
        }
    }

    std::vector<Mech*> ptrs;
    ptrs.reserve(mechs_.size());
    for (Mech& m : mechs_) ptrs.push_back(&m);

    CombatEvents ev;
    combat_.update(dt, world_, ptrs, ev, &units_, &props_);
    dealtThisFrame_ = ev.playerDamageDealt;
    handleDestroyed(ev);
    for (int idx : ev.unitsKilled) onUnitKilled(idx);
    for (int idx : ev.propsKilled) onPropKilled(idx);
    lastEvents_ = ev;
    for (Destructible& d : props_) {
        d.damageFlash = damp(d.damageFlash, 0.0f, 6.0f, dt);
        if (!d.alive) d.deadAge += dt;
    }

    // Stall guard. If nobody has been hit for a while and hostiles are still
    // alive, the two sides have lost each other - a skirmisher holding range
    // across a canyon, or a machine pinned in a corner. Rather than leave the
    // player walking the map looking for it, order everything left to come to
    // them. The threshold is long enough that a deliberate reposition or a
    // reload lull never trips it.
    if (ev.playerDamageDealt > 0.0f || ev.playerDamageTaken > 0.0f) {
        quietFor_ = 0.0f;
        if (hunting_) {
            hunting_ = false;
            for (AiController& b : brains_) b.setHunting(false);
        }
    } else if (phase_ == MissionPhase::Fighting && enemiesAlive() > 0) {
        quietFor_ += dt;
        if (!hunting_ && quietFor_ > kStallTimeout) {
            hunting_ = true;
            for (AiController& b : brains_) b.setHunting(true);
        }
    }

    // ---- set-pieces ---------------------------------------------------------
    blackout_ = damp(blackout_, blackoutTarget_, 0.6f, dt);
    if (phase_ == MissionPhase::Fighting) {
        const ObjectiveSpec* spec = currentObjective();
        // The creeping barrage: a line of fire walking up the lane behind
        // the pilot. Ahead of it is a march; behind it is the end.
        if (spec && spec->kind == ObjectiveKind::Outrun && barrageAlong_ > -1e8f) {
            barrageAlong_ += spec->barrageSpeed * dt;
            barrageTimer_ -= dt;
            if (barrageTimer_ <= 0.0f) {
                barrageTimer_ = 0.32f;
                const Vec3 perp(missionAxis_.z, 0.0f, -missionAxis_.x);
                const Vec3 origin = missionAxis_ * barrageAlong_;
                // Two shells a beat along the line, and if the pilot is
                // behind it, one more on them.
                for (int k = 0; k < 2; ++k)
                    fireShell(origin + perp * rng_.range(-70.0f, 70.0f) +
                                  missionAxis_ * rng_.range(-12.0f, 12.0f), 4.0f);
                const float pAlong = dot(mechs_[0].position(), missionAxis_);
                if (pAlong < barrageAlong_ + 6.0f) fireShell(mechs_[0].position(), 9.0f);
            }
        }
        // The collapsing crossing: past the middle the charges are armed,
        // and twenty seconds later the road behind the pilot is gone.
        if (spec && spec->collapse && !collapsed_) {
            const float startAlong = dot(checkpointPos_, missionAxis_);
            const float zoneAlong = dot(objectiveZone_, missionAxis_);
            const float pAlong = dot(mechs_[0].position(), missionAxis_);
            const float frac = (zoneAlong - startAlong != 0.0f)
                ? (pAlong - startAlong) / (zoneAlong - startAlong) : 0.0f;
            if (collapseTimer_ < 0.0f && frac > 0.45f) collapseTimer_ = 20.0f;
            if (collapseTimer_ >= 0.0f) {
                collapseTimer_ -= dt;
                if (collapseTimer_ <= 0.0f) {
                    collapseDecksBehind(pAlong - 6.0f);
                    collapseTimer_ = -1.0f;
                }
            }
        }
    }

    // ------------------------------------------------------- mission flow --
    if (phase_ == MissionPhase::Briefing) return;   // nothing resolves yet
    if (phase_ == MissionPhase::Fighting || phase_ == MissionPhase::WaveGap) {
        if (!mechs_[0].alive()) {
            // The machine is gone: the contract is lost. No checkpoint, no
            // partial credit - the retry starts the whole level again, on
            // the SAME ground (the seed does not re-roll), so a death is
            // something you learn from rather than something you pay for.
            combat_.explosion(mechs_[0].hitCentre(), 4.2f,
                              Vec3(1.0f, 0.55f, 0.25f), rng_);
            if (phase_ != MissionPhase::Failed) {
                ledger_.checkpointDeaths += 1;     // records the loss, once
                phase_ = MissionPhase::Failed;
                endTimer_ = 0.0f;
            }
        } else {
            updateObjective(dt);
        }
    }
    (void)kEndDelay;
    (void)waveTimer_;
}


// ------------------------------------------------------------ objectives ---

Vec3 Mission::lanePoint(float t, float sweepScale) const {
    // Land missions span most of the arena - the long march is the point.
    // Water maps keep the shorter span: the causeway chain is the route, and
    // a zone pushed past its far anchorage strands the escort and the bot.
    const float extent = world_.arena().extent * (world_.hasWater() ? 0.68f : 0.92f);
    // A route, not a ruler. A straight line from one edge of the arena to the
    // other is about four hundred metres, and a machine that now travels at
    // twelve metres a second walks it in well under a minute - which is how a
    // seven-segment contract still finished in two. The lane sweeps across the
    // ground instead: the advance is always forward along the mission axis,
    // but it crosses the district three times doing it, which roughly triples
    // the march without needing a bigger world or a single extra hostile.
    // Water maps keep the straight run: their route IS the causeway, and a
    // sweep off it is open sea.
    // ONE gentle sweep now, not three: the user asked for linear missions,
    // and the length comes from the arena (2.6x longer than it was) rather
    // than from crossing it back and forth. The quarter-extent bend is
    // enough that the far end is never in sight from the start.
    // The curve itself lives in World::laneAt so the road furniture lines
    // the same route; the mission only decides which end is the start.
    const Vec3 want = world_.laneAt(laneFlip_ ? 1.0f - t : t, sweepScale);
    (void)extent;
    // Snap to real, standable, DRY, outside-a-building ground near the ideal
    // spot. On island maps the naive lane point lands in the sea, and an
    // objective zone underwater is a mission nobody can survive reaching -
    // the escort spawned next to one drowned before the briefing ended.
    Rng r(level_.seed * 40503u + static_cast<uint32_t>(t * 8192.0f) + 11u);
    auto dry = [&](const Vec3& p) {
        if (!world_.hasWater()) return true;
        if (world_.terrain().height(p.x, p.z) > world_.waterLevel() + 0.6f)
            return true;
        // A causeway deck over the sea counts: standing room is standing
        // room, and on an island map the road IS the land half the time.
        const SurfaceHit h = world_.findFoothold(p + Vec3(0.0f, 6.0f, 0.0f),
                                                 Vec3(0.0f, 1.0f, 0.0f),
                                                 4.0f, 14.0f);
        return h.hit && h.point.y > world_.waterLevel() + 0.6f;
    };
    Vec3 best = want;
    best.y = world_.terrain().height(want.x, want.z);
    // Widening rings until land turns up; islands can put the nearest dry
    // ground a fair march from the ideal spot. Within a ring the candidate
    // NEAREST the ideal spot wins, never the first one found: on a wide
    // ring the first dry sample could be a hundred metres BACK down the
    // lane, which put a quarter-way escort zone six metres from the start.
    for (float radius = 10.0f; radius < 140.0f; radius += 18.0f) {
        bool found = false;
        float bestD = 1e9f;
        for (int tries = 0; tries < 26; ++tries) {
            // Stay near the road on every map. On water maps dry ground far
            // off the lane is an island the causeway does not serve; on land
            // a zone that drifts wide undoes the whole linear route.
            const Vec3 perp = Vec3(missionAxis_.z, 0.0f, -missionAxis_.x);
            const float side = world_.hasWater() ? 0.3f : 0.55f;
            Vec3 cand = want + missionAxis_ * r.range(-radius, radius) +
                        perp * r.range(-radius * side, radius * side);
            if (std::fabs(cand.x) > world_.arena().extent - 16.0f ||
                std::fabs(cand.z) > world_.arena().extent - 16.0f) continue;
            if (!dry(cand)) continue;
            if (world_.insideStructure(cand, 2.5f)) continue;
            // Gentle ground only. An objective zone on a scarp face is
            // unreachable for half the leg catalogue, and a mission whose
            // marker cannot be stood on cannot be finished.
            if (world_.terrain().slope(cand.x, cand.z) > 0.38f) continue;
            // Backwards along the lane costs double: a zone behind the
            // ideal spot shortens the segment, ahead of it only moves it.
            const float back = std::max(0.0f, dot(want - cand, missionAxis_));
            const float d = lengthXZ(cand - want) + back;
            if (d >= bestD) continue;
            bestD = d;
            best = cand;
            const SurfaceHit h = world_.findFoothold(cand + Vec3(0.0f, 6.0f, 0.0f),
                                                     Vec3(0.0f, 1.0f, 0.0f),
                                                     4.0f, 14.0f);
            best.y = h.hit ? h.point.y : world_.terrain().height(cand.x, cand.z);
            found = true;
        }
        if (found) break;
    }
    return best;
}

void Mission::spawnUnitAlerted(UnitKind kind, const Vec3& near, int mode,
                               const Vec3& goal) {
    spawnUnit(kind, near, mode, goal);
    if (!units_.empty()) units_.back().forceAlert();
}

void Mission::spawnUnit(UnitKind kind, const Vec3& near, int mode, const Vec3& goal) {
    Vec3 p = near + Vec3(rng_.range(-14.0f, 14.0f), 0.0f, rng_.range(-14.0f, 14.0f));
    auto bad = [&](const Vec3& q) {
        if (world_.insideStructure(q, 1.5f)) return true;
        if (world_.hasWater() && kind != UnitKind::Drone &&
            world_.terrain().height(q.x, q.z) < world_.waterLevel() + 0.3f) {
            const SurfaceHit h = world_.findFoothold(q + Vec3(0.0f, 6.0f, 0.0f),
                                                     Vec3(0.0f, 1.0f, 0.0f),
                                                     4.0f, 14.0f);
            if (!(h.hit && h.point.y > world_.waterLevel() + 0.3f)) return true;
        }
        return false;
    };
    for (int tries = 0; tries < 14; ++tries) {
        if (!bad(p)) break;
        p = near + Vec3(rng_.range(-22.0f, 22.0f), 0.0f, rng_.range(-22.0f, 22.0f));
    }
    // Stand on the standing surface, which over water means the deck, not the
    // seabed underneath it - a unit clamped to the seabed drowns on frame one.
    {
        const SurfaceHit h = world_.findFoothold(p + Vec3(0.0f, 6.0f, 0.0f),
                                                 Vec3(0.0f, 1.0f, 0.0f), 4.0f, 14.0f);
        p.y = h.hit ? h.point.y : world_.terrain().height(p.x, p.z);
    }
    Unit u;
    u.init(kind, p, rng_.range(0.0f, TAU), rng_.next() | 1u);
    u.setMode(mode, goal);
    units_.push_back(u);
}

void Mission::spawnGarrison(const ObjectiveSpec& spec, const Vec3& around) {
    // STRONGPOINTS, not a smear. The old garrison lerped every unit to a
    // random point along the approach, which played as an endless trickle of
    // small contacts - shoot, walk, get chipped, repeat. Now the garrison
    // forms two or three POCKETS along the route, each anchored by whatever
    // armour it has, with the infantry dug in around it. Between pockets the
    // road is quiet; at each one there is a readable position to approach,
    // flank and destroy. Fewer small enemies overall - troopers and drones
    // are texture, the armour is the fight.
    // ---- the HEADCOUNT BUDGET -------------------------------------------
    // A segment is allowed about ten hostiles at once; a designated busy
    // fight (a real objective rather than a march) may run to sixteen.
    // Beyond that a firefight stops being readable and becomes a wall of
    // tracer, which is exactly how mission three ended up with "far too
    // many guns on you". Armour and emplacements are kept - they are the
    // fight - and the scrub infantry is trimmed to fit.
    const bool busy = (spec.kind != ObjectiveKind::ReachZone) && !spec.advanceOnReach;
    // The budget SCALES. A flat ten-and-sixteen meant the third mission met
    // almost the same headcount as the thirteenth, and since the same wave of
    // work made every individual enemy considerably more dangerous, the early
    // missions took the whole difficulty increase with none of the machine to
    // answer it. Early contracts are now genuinely thin; the late ones fill
    // out to the old numbers, by which point the player has the guns for it.
    const int p = level_.power;
    // Wave 15: "a little too easy, little reason to take cover" - the
    // budget grows by a third and the march segments carry real armour.
    const int budget = busy ? std::min(20, 12 + p / 2) : std::min(13, 7 + p / 3);
    int troopers = spec.troopers;
    int drones = spec.drones;
    int atTeams = spec.atTeams;
    {
        // Everything that will be alive at once, including the guns placed
        // by the turret and overwatch passes below.
        auto total = [&] {
            return troopers + atTeams + spec.apcs + spec.tanks + drones +
                   spec.turrets + spec.overwatch + spec.marksmen +
                   spec.mortars + spec.jammers + spec.wardens;
        };
        // Trim in order of how little each kind is missed: troopers, then
        // drones, then rocket teams - never the armour.
        while (total() > budget && troopers > 1) --troopers;
        while (total() > budget && drones > 0) --drones;
        while (total() > budget && atTeams > 1) --atTeams;
    }
    const int strength = troopers + atTeams + spec.apcs + spec.tanks + drones;
    const int pockets = (strength >= 16) ? 5 : (strength >= 10) ? 4
                      : (strength >= 6) ? 3 : (strength >= 3) ? 2 : 1;
    // Pocket centres sit at fixed fractions of the approach, the last one on
    // the objective itself; each is jittered off the lane so two segments
    // never feel identical.
    Vec3 centre[5];
    static const float kAt[5] = {0.22f, 0.40f, 0.58f, 0.78f, 1.0f};
    const Vec3 perp(missionAxis_.z, 0.0f, -missionAxis_.x);
    for (int p2 = 0; p2 < pockets; ++p2) {
        const float t = kAt[5 - pockets + p2];
        centre[p2] = lerp(checkpointPos_, around, t) +
                     perp * rng_.range(-26.0f, 26.0f);
    }
    int next = 0;
    auto place = [&](UnitKind k, int n) {
        for (int i = 0; i < n; ++i)
            spawnUnit(k, centre[(next++) % pockets], 0, Vec3(0.0f));
    };
    // Armour first so every pocket is anchored by a vehicle before the
    // infantry fills in around it.
    place(UnitKind::Tank, spec.tanks);
    place(UnitKind::APC, spec.apcs);
    // The support pieces go down before the infantry, because they are what
    // the infantry is standing around. A Warden in the middle of a pocket is
    // the reason that pocket is hard.
    place(UnitKind::Warden, spec.wardens);
    place(UnitKind::Jammer, spec.jammers);
    // Shield pylons stand ON the objective: the pocket around it and the
    // marked targets in it are untouchable until the pylons fall.
    for (int i = 0; i < spec.pylons; ++i) {
        const float ang = TAU * static_cast<float>(i) / std::max(1, spec.pylons) + 0.7f;
        Vec3 at = around + Vec3(std::sin(ang), 0.0f, std::cos(ang)) * (spec.pylons > 1 ? 22.0f : 6.0f);
        for (int tries = 0; tries < 10 && world_.insideStructure(at, 2.5f); ++tries)
            at = around + Vec3(rng_.range(-28.0f, 28.0f), 0.0f, rng_.range(-28.0f, 28.0f));
        spawnUnit(UnitKind::ShieldPylon, at, 0, Vec3(0.0f));
        units_.back().teleport(Vec3(at.x, world_.terrain().height(at.x, at.z), at.z));
    }
    // In the city the infantry waits in the ALLEYS - against a wall, out of
    // sight of the road - and comes out when the pilot is past. On open
    // ground there is nothing to hide behind and they dig in as before.
    const bool urban = world_.arena().urban;
    for (int i = 0; i < atTeams; ++i) {
        const Vec3 c = centre[(next++) % pockets];
        if (urban) spawnUnit(UnitKind::ATTrooper, hiddenSpot(c, lerp(checkpointPos_, c, 0.6f)), 0, Vec3(0.0f));
        else spawnUnit(UnitKind::ATTrooper, c, 0, Vec3(0.0f));
    }
    for (int i = 0; i < troopers; ++i) {
        const Vec3 c = centre[(next++) % pockets];
        if (urban && (i & 1)) spawnUnit(UnitKind::Trooper, hiddenSpot(c, lerp(checkpointPos_, c, 0.6f)), 0, Vec3(0.0f));
        else spawnUnit(UnitKind::Trooper, c, 0, Vec3(0.0f));
    }
    place(UnitKind::Drone, drones);
    // Sappers wait in a pack just off the route and rush when it passes.
    for (int i = 0; i < spec.sappers; ++i) {
        const Vec3 c = centre[(i / 3 + 1) % pockets];
        spawnUnit(UnitKind::Sapper, hiddenSpot(c, lerp(checkpointPos_, c, 0.5f)), 0, Vec3(0.0f));
    }
    // Gunships orbit the objective from the start, alerted: they are the
    // sky, and the sky has no cover.
    for (int i = 0; i < spec.gunships; ++i) {
        spawnUnit(UnitKind::Gunship, around + perp * ((i & 1) ? 40.0f : -40.0f), 0, Vec3(0.0f));
        Unit& g = units_.back();
        g.teleport(g.position() + Vec3(0.0f, 17.0f, 0.0f));
        g.forceAlert();
    }
    // Marksmen and mortars belong BEHIND the line, not in it. Dropped into a
    // pocket they are just fragile riflemen; set back off the lane they are
    // the reason the pocket cannot simply be walked into.
    for (int i = 0; i < spec.marksmen; ++i) {
        const Vec3 base = centre[(next++) % pockets];
        spawnUnit(UnitKind::Marksman,
                  base + missionAxis_ * rng_.range(45.0f, 95.0f) +
                      perp * rng_.range(-70.0f, 70.0f), 0, Vec3(0.0f));
    }
    for (int i = 0; i < spec.mortars; ++i) {
        const Vec3 base = centre[(next++) % pockets];
        spawnUnit(UnitKind::Mortar,
                  base + missionAxis_ * rng_.range(90.0f, 150.0f) +
                      perp * rng_.range(-60.0f, 60.0f), 0, Vec3(0.0f));
    }
    // Rocket trucks sit further back still, and they do not stay put.
    for (int i = 0; i < spec.launchers; ++i) {
        const Vec3 base = centre[(next++) % pockets];
        spawnUnit(UnitKind::Launcher,
                  base + missionAxis_ * rng_.range(120.0f, 180.0f) +
                      perp * rng_.range(-70.0f, 70.0f), 0, Vec3(0.0f));
        if (!units_.empty()) units_.back().forceAlert();
    }
    // Turrets guard the objective itself - every other one from a ROOFTOP,
    // where only long guns or a climber can answer it.
    for (int i = 0; i < spec.turrets; ++i) {
        if ((i & 1) == 0 && spawnTurretOnRoof(around)) continue;
        spawnUnit(UnitKind::Turret, around, 0, Vec3(0.0f));
    }
    // OVERWATCH: heavy guns far off the lane with sightlines onto it,
    // alerted from the start - they open up the moment the player walks
    // into their reach. Mostly tanks; every third position is an
    // emplacement, on a roof when one serves. This is the long-range game:
    // duel them at range, rush them, or climb around them.
    for (int i = 0; i < spec.overwatch; ++i) {
        const float side = (i & 1) ? 1.0f : -1.0f;
        const float t2 = rng_.range(0.45f, 0.95f);
        // Off the lane, but not a expedition: far enough to be a long shot,
        // close enough that killing it is a detour and not a search party.
        const Vec3 base = lerp(checkpointPos_, around, t2) +
                          perp * (side * rng_.range(55.0f, 95.0f));
        if ((i % 3) == 2 && spawnTurretOnRoof(base)) continue;
        spawnUnit((i % 3) == 2 ? UnitKind::Turret : UnitKind::Tank, base, 0,
                  Vec3(0.0f));
        if (!units_.empty()) units_.back().forceAlert();
    }
}

bool Mission::spawnTurretOnRoof(const Vec3& near) {
    // A rooftop emplacement: stand a turret on the nearest suitable
    // building top. Returns false when no roof serves.
    int best = -1;
    float bestD = 60.0f * 60.0f;
    for (size_t i = 0; i < world_.obstacles().size(); ++i) {
        const Obstacle& o = world_.obstacles()[i];
        if (!o.active || o.kind != ObstacleKind::Building) continue;
        const float top = o.center.y + o.half.y;
        const float hgt = top - world_.terrain().height(o.center.x, o.center.z);
        if (hgt < 5.0f || hgt > 18.0f) continue;
        if (std::min(o.half.x, o.half.z) < 2.2f) continue;   // room to stand
        const float dd = lengthSq(flattenY(o.center - near));
        if (dd < bestD) { bestD = dd; best = static_cast<int>(i); }
    }
    if (best < 0) return false;
    const Obstacle& o = world_.obstacles()[static_cast<size_t>(best)];
    // Stand it at the LANE-FACING EDGE of the roof, not the centre: a
    // turret in the middle of a wide roof is masked by the parapet from
    // every street angle - it can hit you and you cannot hit it, which is
    // unfair rather than tactical. On the edge its body shows.
    const Vec3 toLane = normalize(flattenY(near - o.center) + Vec3(1e-3f, 0.0f, 0.0f));
    const Vec3 edge = o.center +
                      toLane * (std::min(o.half.x, o.half.z) * 0.62f);
    Unit u;
    u.init(UnitKind::Turret,
           Vec3(edge.x, o.center.y + o.half.y + 0.15f, edge.z),
           rng_.range(0.0f, TAU), rng_.next() | 1u);
    units_.push_back(u);
    return true;
}

void Mission::startObjective(int index) {
    if (index < 0 || index >= objectiveCount()) return;
    objectiveIndex_ = index;
    const ObjectiveSpec& spec = level_.objectives[static_cast<size_t>(index)];
    const float t = static_cast<float>(index + 1) /
                    static_cast<float>(objectiveCount());
    objectiveZone_ = lanePoint(t);
    objectiveTimer_ = spec.timer;
    objectiveProgress_ = 0;
    objectiveTarget_ = std::max(spec.count, 0);
    markedProps_.clear();
    markedUnits_.clear();
    markedMechs_.clear();
    segmentWavesSpawned_ = 0;
    reinforceTimer_ = 9.0f;
    shellTimer_ = 16.0f;
    alarm_ = 0.0f;

    spawnGarrison(spec, objectiveZone_);

    // The segment's spidertanks (rare, strong). A wave with a delay HOLDS:
    // the machine arrives partway through the segment rather than standing in
    // the line from the first frame. That distinction is most of what makes a
    // fortified position fightable - break the guns, then deal with what
    // comes to relieve them - instead of a wall you meet all at once.
    // A KillTarget's quarry always starts on the field: it is the objective.
    pendingWave_ = -1;
    pendingWaveTimer_ = 0.0f;
    if (!spec.waves.empty()) {
        if (spec.waves[0].delay > 0.01f && spec.kind != ObjectiveKind::KillTarget) {
            pendingWave_ = 0;
            pendingWaveTimer_ = spec.waves[0].delay;
        } else {
            const size_t firstNew = mechs_.size();
            spawnMechWave(spec.waves[0], objectiveZone_);
            if (spec.kind == ObjectiveKind::KillTarget)
                for (size_t i = firstNew; i < mechs_.size(); ++i)
                    markedMechs_.push_back(static_cast<int>(i));
        }
    }

    // Set-piece bookkeeping per segment.
    barrageAlong_ = (spec.kind == ObjectiveKind::Outrun)
        ? dot(checkpointPos_, missionAxis_) - 28.0f : -1e9f;
    barrageTimer_ = 0.0f;
    collapseTimer_ = -1.0f;
    collapsed_ = false;
    ambushSprung_ = false;
    escortAmbushSprung_ = false;

    switch (spec.kind) {
        case ObjectiveKind::KillUnits: {
            // Every live unit of the named kind near the zone is the target.
            // The garrison above just spawned them (pylons on the zone, the
            // towers on the roofs around it).
            for (size_t i = 0; i < units_.size(); ++i) {
                const Unit& u = units_[i];
                if (!u.alive() || u.team() != Team::Hostile || u.kind() != spec.killKind) continue;
                if (lengthXZ(u.position() - objectiveZone_) > 140.0f) continue;
                markedUnits_.push_back(static_cast<int>(i));
            }
            // Short of the count: stand more of them up.
            while (static_cast<int>(markedUnits_.size()) < std::max(1, spec.count)) {
                if (spec.killKind == UnitKind::Turret && spawnTurretOnRoof(objectiveZone_)) {
                    markedUnits_.push_back(static_cast<int>(units_.size()) - 1);
                    continue;
                }
                spawnUnit(spec.killKind, objectiveZone_, 0, Vec3(0.0f));
                if (spec.killKind == UnitKind::ShieldPylon) {
                    Unit& u = units_.back();
                    u.teleport(Vec3(u.position().x, world_.terrain().height(u.position().x, u.position().z),
                                    u.position().z));
                }
                markedUnits_.push_back(static_cast<int>(units_.size()) - 1);
            }
            objectiveTarget_ = static_cast<int>(markedUnits_.size());
            break;
        }
        case ObjectiveKind::DestroyMarked:
        case ObjectiveKind::Blackout: {
            if (spec.gateHealth > 0.0f) {
                // THE GATE: one armoured slab across the lane at the zone,
                // built like a bunker and worth a siege. Its shield, if the
                // segment has pylons, is what the pylons are for.
                Vec3 pp = objectiveZone_;
                for (int tries = 0; tries < 8 && world_.insideStructure(pp, 3.0f); ++tries)
                    pp = objectiveZone_ + Vec3(rng_.range(-20.0f, 20.0f), 0.0f, rng_.range(-20.0f, 20.0f));
                pp.y = world_.terrain().height(pp.x, pp.z);
                Destructible d;
                d.style = PropStyle::GuardShed;
                d.pos = pp;
                d.yaw = std::atan2(missionAxis_.x, missionAxis_.z);
                d.maxHealth = d.health = spec.gateHealth;
                d.explosive = true;
                d.blastRadius = 10.0f;
                d.blastDamage = 40.0f;
                d.cashValue = 400;
                d.hitRadius = 5.0f;
                d.hitHeight = 7.0f;
                d.ammoChance = 1.0f;
                Obstacle o;
                o.center = d.pos + Vec3(0.0f, d.hitHeight * 0.5f, 0.0f);
                const Vec3 perp(missionAxis_.z, 0.0f, -missionAxis_.x);
                (void)perp;
                o.half = Vec3(d.hitRadius, d.hitHeight * 0.5f, d.hitRadius);
                o.kind = ObstacleKind::Debris;
                o.climbable = false;
                d.obstacle = world_.addDynamicObstacle(o);
                markedProps_.push_back(static_cast<int>(props_.size()));
                props_.push_back(d);
                world_.finalizeObstacles();
                objectiveTarget_ = 1;
                break;
            }
            // Purpose-built targets in a ring around the zone, so the thing
            // the briefing named is actually there to be destroyed.
            const int n = std::max(1, spec.count);
            for (int i = 0; i < n; ++i) {
                const float ang = TAU * static_cast<float>(i) / n +
                                  rng_.range(-0.3f, 0.3f);
                Vec3 pp = objectiveZone_ + Vec3(std::sin(ang), 0.0f, std::cos(ang)) *
                                               rng_.range(14.0f, 30.0f);
                for (int tries = 0; tries < 8; ++tries) {
                    if (!world_.insideStructure(pp, 2.0f)) break;
                    pp = objectiveZone_ +
                         Vec3(rng_.range(-30.0f, 30.0f), 0.0f, rng_.range(-30.0f, 30.0f));
                }
                pp.y = world_.terrain().height(pp.x, pp.z);
                Destructible d;
                d.style = (spec.kind == ObjectiveKind::Blackout)
                              ? PropStyle::AntennaMast
                              : (i % 2 ? PropStyle::CoolingStack : PropStyle::FuelTank);
                d.pos = pp;
                d.yaw = rng_.range(0.0f, TAU);
                d.maxHealth = d.health =
                    (d.style == PropStyle::AntennaMast) ? 45.0f : 60.0f;
                d.explosive = d.style == PropStyle::FuelTank;
                d.blastRadius = d.explosive ? 8.0f : 0.0f;
                d.blastDamage = d.explosive ? 50.0f : 0.0f;
                d.cashValue = 125;
                d.hitRadius = (d.style == PropStyle::AntennaMast) ? 1.0f : 2.2f;
                d.hitHeight = (d.style == PropStyle::AntennaMast) ? 8.0f : 6.0f;
                Obstacle o;
                o.center = d.pos + Vec3(0.0f, d.hitHeight * 0.5f, 0.0f);
                o.half = Vec3(d.hitRadius, d.hitHeight * 0.5f, d.hitRadius);
                o.kind = ObstacleKind::Debris;
                d.obstacle = world_.addDynamicObstacle(o);
                markedProps_.push_back(static_cast<int>(props_.size()));
                props_.push_back(d);
            }
            world_.finalizeObstacles();
            objectiveTarget_ = n;
            break;
        }
        case ObjectiveKind::Convoy: {
            // The column: vehicles strung down the far half of the lane,
            // driving TOWARD the player's end of the canyon. Head-on intercept
            // geometry: closing speed is theirs plus yours, so first contact
            // comes in under a minute on any seed. The old away-running column
            // fled at nearly the player's own pace and turned the mission into
            // an unwinnable stern chase whenever the opening seconds slipped.
            const int vehicles = std::max(3, spec.count);
            const Vec3 goal = lanePoint(-0.12f, 0.22f);
            for (int i = 0; i < vehicles; ++i) {
                const float ct = 0.88f - 0.07f * i;
                const Vec3 at = lanePoint(clampf(ct, 0.20f, 0.95f), 0.22f);
                const UnitKind k = (i % 3 == 2) ? UnitKind::Tank : UnitKind::APC;
                spawnUnit(k, at, 1, goal);
                units_.back().setMarchPace(0.42f);
                units_.back().setRoad(Vec3(0.0f, 0.0f, 0.0f), missionAxis_);
                markedUnits_.push_back(static_cast<int>(units_.size()) - 1);
            }
            objectiveTarget_ = vehicles;
            break;
        }
        case ObjectiveKind::Escort: {
            spawnUnit(UnitKind::APC, checkpointPos_ + missionAxis_ * 6.0f, 1,
                      objectiveZone_);
            escortUnit_ = static_cast<int>(units_.size()) - 1;
            units_.back().setTeam(Team::Player);
            // The charge is a hardened recovery crawler, not a line APC: the
            // mission is about killing what shoots at it, not about its HP
            // bar evaporating to the first AT team.
            units_.back().scaleHealth(7.0f);
            units_.back().setAmphibious(true);
            units_.back().setMarchPace(0.72f);
            units_.back().setRoad(Vec3(0.0f, 0.0f, 0.0f), missionAxis_);
            // Route the crawler over the actual causeway chain: the deck
            // centres, in lane order, are the only guaranteed-dry line across
            // an island map. A straight march to the goal is a straight march
            // into the sound.
            if (world_.hasWater()) {
                std::vector<std::pair<float, Vec3>> decks;
                for (const Obstacle& o : world_.obstacles()) {
                    if (o.kind != ObstacleKind::Building || !o.active) continue;
                    if (o.half.y > 0.5f && std::max(o.half.x, o.half.z) < 10.0f)
                        continue;
                    if (o.half.y > 0.5f) continue;   // decks are thin slabs
                    if (std::max(o.half.x, o.half.z) < 10.0f) continue;
                    const float along = dot(o.center, missionAxis_);
                    decks.push_back({along, o.center + Vec3(0.0f, o.half.y + 0.2f, 0.0f)});
                }
                std::sort(decks.begin(), decks.end(),
                          [](const auto& a2, const auto& b2) { return a2.first < b2.first; });
                const float startAlong = dot(checkpointPos_, missionAxis_);
                const float goalAlong = dot(objectiveZone_, missionAxis_);
                std::vector<Vec3> route;
                if (goalAlong >= startAlong) {
                    for (const auto& d : decks)
                        if (d.first > startAlong - 6.0f && d.first < goalAlong + 6.0f)
                            route.push_back(d.second);
                } else {
                    for (auto it = decks.rbegin(); it != decks.rend(); ++it)
                        if (it->first < startAlong + 6.0f && it->first > goalAlong - 6.0f)
                            route.push_back(it->second);
                }
                units_.back().setRoute(route);
            }
            break;
        }
        case ObjectiveKind::Rampage:
            // Progress is money: the count is a destruction value to bank.
            objectiveTarget_ = std::max(spec.count, 400);
            break;
        default:
            break;
    }
}

void Mission::completeObjective() {
    const ObjectiveSpec& spec = level_.objectives[static_cast<size_t>(objectiveIndex_)];
    ledger_.objectivesDone += 1;
    // The trap: the zone was the bait.
    if (spec.ambush && spec.kind != ObjectiveKind::Escort && !ambushSprung_) {
        ambushSprung_ = true;
        spawnAmbush(mechs_[0].position());
    }
    // The lights go out with this objective and stay out.
    if (spec.lightsOut) blackoutTarget_ = 1.0f;
    // The backbone of the payout, and now genuinely the backbone: with the
    // wrecking economy and the kill bounties cut back to seasoning, what a
    // contract pays is what the contract was FOR.
    const int bonus = pay(58 + level_.power * 19);
    ledger_.cashObjectives += bonus;
    cashEarned_ += bonus;

    // SECTOR CLEARANCE. The single most-reported problem with this game's
    // pacing is that it rewards running: enemies keep arriving, fighting them
    // costs structure, and the payout came from finishing the contract, so the
    // optimal play was to sprint the route and never turn round. That is not
    // the game anyone wants to play. Closing a segment out with nothing
    // hostile left standing near it now pays about as much as the objective
    // itself - so taking the ground is a real alternative to outrunning it,
    // and the repair salvage that wrecked armour leaves behind means clearing
    // a strongpoint is also how you top the machine up for the next one.
    {
        bool anyLeft = false;
        for (const Unit& u : units_) {
            if (!u.alive() || u.team() != Team::Hostile) continue;
            if (length(flattenY(u.position() - objectiveZone_)) < 130.0f) {
                anyLeft = true;
                break;
            }
        }
        if (!anyLeft)
            for (size_t i = 1; i < mechs_.size(); ++i) {
                if (!mechs_[i].alive()) continue;
                if (length(flattenY(mechs_[i].position() - objectiveZone_)) < 130.0f) {
                    anyLeft = true;
                    break;
                }
            }
        if (!anyLeft) {
            const int clearPay = pay(70 + level_.power * 26);
            ledger_.cashClearance += clearPay;
            ledger_.sectorsCleared += 1;
            cashEarned_ += clearPay;
        }
    }

    // Between pushes the crew patches what they can reach. A PARTIAL repair,
    // not a top-up: this used to restore the machine to 78% of maximum at
    // every segment, which across a seven-segment contract is six free heals
    // and quietly undid every difficulty change ever made to this game. It
    // also flatly contradicted what the game tells the player - that the only
    // healing in a mission is the salvage that wrecked armour leaves behind.
    // Now the salvage is genuinely the main source and this is the margin.
    checkpointPos_ = mechs_[0].position();
    mechs_[0].repairStructure(mechs_[0].stats().maxHealth * 0.26f);
    // Field resupply: two crates at the checkpoint, for the heavy guns the
    // player actually runs. A fifteen-minute contract cannot be fought on
    // one magazine.
    for (int c = 0; c < 2; ++c)
        dropAmmoCrate(checkpointPos_ +
                      Vec3(rng_.range(-6.0f, 6.0f), 0.0f, rng_.range(-6.0f, 6.0f)));

    if (objectiveIndex_ + 1 < objectiveCount()) {
        startObjective(objectiveIndex_ + 1);
    } else if (phase_ != MissionPhase::Cleared) {
        phase_ = MissionPhase::Cleared;
        endTimer_ = 0.0f;
        // Time bonus: a par of ninety seconds per segment, paying down to
        // nothing at double par. Kept simple enough to read on the debrief.
        const float par = 115.0f * static_cast<float>(objectiveCount());
        const float frac = clampf(2.0f - elapsed_ / par, 0.0f, 1.0f);
        ledger_.cashTimeBonus = pay(static_cast<int>(frac * (55.0f + 15.0f * level_.power)));
        cashEarned_ += ledger_.cashTimeBonus + level_.completionBonus;
        ledger_.missionTime = elapsed_;
    }
}

void Mission::respawnAtCheckpoint() {
    Mech& p = mechs_[0];
    ledger_.checkpointDeaths += 1;
    p.init(world_, p.loadout(), checkpointPos_,
           std::atan2(missionAxis_.x, missionAxis_.z), Team::Player,
           level_.seed * 91u + static_cast<uint32_t>(ledger_.checkpointDeaths));
    p.setIndex(0);
    p.restoreHealth(0.78f);
    p.refillAmmo();
    // A beat of quiet: shove everything hostile back a little by resetting
    // their burst state is more machinery than it is worth; distance does it.
}

void Mission::onUnitKilled(int idx) {
    if (idx < 0 || idx >= static_cast<int>(units_.size())) return;
    Unit& u = units_[static_cast<size_t>(idx)];
    const UnitStats& st = u.stats();
    u.settleWreck(world_);
    combat_.explosion(u.hitCentre(),
                      st.radius > 1.0f ? 2.8f : 1.1f,
                      Vec3(1.0f, 0.62f, 0.3f), rng_);
    if (u.team() == Team::Player) return;   // no bounty for losing your escort
    ledger_.unitKills[static_cast<int>(u.kind())] += 1;
    // A tank is a KILL, not a demolished building. This was booked under the
    // wrecking ledger, which made the debrief read as though the player were
    // being paid to shoot scenery and hid the fact that clearing ground units
    // was already most of a mission's income - a mislabel that cost a whole
    // round of economy tuning aimed at the wrong number.
    ledger_.cashKills += pay(st.bounty);
    cashEarned_ += pay(st.bounty);
    // Vehicles leave usable ammunition. Armour is a reliable resupply now;
    // even AT teams cough up a rocket sometimes.
    const bool armour = (u.kind() == UnitKind::APC || u.kind() == UnitKind::Tank ||
                         u.kind() == UnitKind::Turret);
    const float chance = armour ? 0.85f : (u.kind() == UnitKind::ATTrooper ? 0.35f : 0.0f);
    if (chance > 0.0f && rng_.unit() < chance)
        dropAmmoCrate(u.position());
    // Wrecked armour is a parts bin. A gun tank is worth a real weld; an
    // APC or emplacement a smaller one. This is the ONLY healing in a
    // mission now, which is what makes clearing a pocket worth the risk.
    if (armour) {
        const float amount = (u.kind() == UnitKind::Tank) ? 90.0f
                           : (u.kind() == UnitKind::Turret) ? 62.0f : 48.0f;
        combat_.addRepair(u.position(), amount);
    }
}

void Mission::dropAmmoCrate(const Vec3& at) {
    // What the crate holds: a magazine for a heavy gun THE PLAYER IS
    // CARRYING, whenever they carry one. Dropping a random catalogue
    // weapon's ammunition meant most crates were for guns you did not own,
    // which is why heavy munitions felt like they ran dry and never came
    // back. Only if the player runs no limited guns does it fall back to
    // the catalogue.
    const PartCatalog& cat = PartCatalog::instance();
    std::vector<const PartDef*> mine, any;
    for (const MountedWeapon& mw : mechs_[0].weapons())
        if (mw.part && mw.part->weapon.ammo == AmmoKind::Limited)
            mine.push_back(mw.part);
    if (mine.empty())
        for (const PartDef& pd : cat.all())
            if (pd.slot == Slot::Weapon && pd.weapon.ammo == AmmoKind::Limited)
                any.push_back(&pd);
    const std::vector<const PartDef*>& pool = mine.empty() ? any : mine;
    if (pool.empty()) return;
    const PartDef* pick = pool[rng_.next() % pool.size()];
    combat_.addPickup(at + Vec3(0.0f, 0.3f, 0.0f), pick->id,
                      std::max(1, pick->weapon.magazine));
}

void Mission::onPropKilled(int idx) {
    if (idx < 0 || idx >= static_cast<int>(props_.size())) return;
    Destructible& d = props_[static_cast<size_t>(idx)];
    if (d.obstacle >= 0) world_.disableObstacle(d.obstacle);
    // Which way it falls: away from whoever is closest to it - the player,
    // almost always - so a chimney comes down away from the machine that
    // shot it and a mast folds across the street.
    {
        const Vec3 away = flattenY(d.pos - mechs_[0].position());
        d.fallYaw = (lengthSq(away) > 1e-3f) ? std::atan2(away.x, away.z)
                                             : d.yaw + 1.2f;
        d.deadAge = 0.0f;
    }
    ledger_.propsDestroyed += 1;
    ledger_.cashDestruction += pay(d.cashValue);
    cashEarned_ += pay(d.cashValue);

    combat_.explosion(d.hitCentre(), d.explosive ? d.blastRadius * 0.6f : 1.6f,
                      d.explosive ? Vec3(1.0f, 0.55f, 0.2f) : Vec3(0.9f, 0.85f, 0.7f),
                      rng_);

    // The chain: an explosive prop is a weapon, and cooking one off next to a
    // supply dump is meant to level the dump. Damage radiates to everything,
    // props included, so chains propagate on later frames as those die.
    if (d.explosive) {
        for (Mech& m : mechs_) {
            if (!m.alive()) continue;
            const float dist = length(m.hitCentre() - d.hitCentre()) - m.hitRadius();
            if (dist < d.blastRadius)
                m.applyDamage(d.blastDamage *
                                  (1.0f - clampf(dist / d.blastRadius, 0.0f, 1.0f)),
                              normalize(m.hitCentre() - d.hitCentre() +
                                        Vec3(0.0f, 0.01f, 0.0f)));
        }
        for (size_t i = 0; i < units_.size(); ++i) {
            Unit& u = units_[i];
            if (!u.alive()) continue;
            const float dist = length(u.hitCentre() - d.hitCentre()) - u.hitRadius();
            if (dist < d.blastRadius) {
                u.applyDamage(d.blastDamage *
                              (1.0f - clampf(dist / d.blastRadius, 0.0f, 1.0f)));
                if (!u.alive()) onUnitKilled(static_cast<int>(i));
            }
        }
        for (size_t i = 0; i < props_.size(); ++i) {
            if (static_cast<int>(i) == idx) continue;
            Destructible& other = props_[i];
            if (!other.alive) continue;
            const float dist = length(other.hitCentre() - d.hitCentre()) - other.hitRadius;
            if (dist < d.blastRadius) {
                other.applyDamage(d.blastDamage *
                                  (1.0f - clampf(dist / d.blastRadius, 0.0f, 1.0f)));
                if (!other.alive) onPropKilled(static_cast<int>(i));
            }
        }
    }

    if (rng_.unit() < d.ammoChance * 1.7f) dropAmmoCrate(d.pos);
}

void Mission::updateObjective(float dt) {
    if (objectiveIndex_ < 0 || objectiveIndex_ >= objectiveCount()) return;
    const ObjectiveSpec& spec = level_.objectives[static_cast<size_t>(objectiveIndex_)];
    const Vec3 playerPos = mechs_[0].position();
    const float toZone = length(flattenY(playerPos - objectiveZone_));

    switch (spec.kind) {
        case ObjectiveKind::ClearHostiles: {
            // Segment waves chain like the old missions did.
            int alive = 0;
            for (size_t i = 1; i < mechs_.size(); ++i)
                if (mechs_[i].alive()) ++alive;
            // Only the armour gates the segment. Requiring the last trooper
            // in a treeline somewhere made "clear the blocks" a hide-and-seek
            // timeout; infantry stragglers are texture, not an objective.
            // What GATES the segment is what still holds the route: hostile
            // armour near the lane corridor, or near the objective, or near
            // the player. A gun tank that wandered two hundred metres off
            // into the hills is not an objective - hunting it was the "loop
            // around looking for the last enemy" that made a march segment
            // feel broken. It stays alive and dangerous; it just does not
            // hold the mission hostage.
            int aliveUnits = 0;
            {
                const Vec3 perpAxis(missionAxis_.z, 0.0f, -missionAxis_.x);
                for (const Unit& u : units_) {
                    if (!u.alive() || u.team() != Team::Hostile) continue;
                    if (!(u.kind() == UnitKind::APC || u.kind() == UnitKind::Tank ||
                          u.kind() == UnitKind::Turret)) continue;
                    const float offLane = std::fabs(dot(u.position(), perpAxis) -
                                                    dot(objectiveZone_, perpAxis));
                    const bool nearZone = length(flattenY(u.position() - objectiveZone_)) < 95.0f;
                    const bool nearPlayer = length(flattenY(u.position() - playerPos)) < 110.0f;
                    if (offLane < 95.0f || nearZone || nearPlayer) ++aliveUnits;
                }
            }
            if (alive == 0 &&
                segmentWavesSpawned_ + 1 < static_cast<int>(spec.waves.size())) {
                ++segmentWavesSpawned_;
                spawnMechWave(spec.waves[static_cast<size_t>(segmentWavesSpawned_)],
                              objectiveZone_);
            } else if (spec.advanceOnReach && alive == 0 &&
                       toZone < spec.zoneRadius * 1.6f) {
                // Marched the length of it and stood on the far end: the
                // route is taken. Whatever is still shooting from the flanks
                // stays alive and stays dangerous - it simply does not hold
                // the contract hostage while the pilot goes looking for it.
                completeObjective();
            } else if (alive == 0 && aliveUnits == 0) {
                completeObjective();
            } else if (alive == 0 && aliveUnits > 0 && quietFor_ > 18.0f) {
                // Straggler recovery, unit edition: the last APC idling in a
                // gully two ridges away is not a finale, it is a scavenger
                // hunt. Bring the survivors to the fight.
                for (Unit& u : units_) {
                    if (!u.alive() || u.team() != Team::Hostile) continue;
                    if (!(u.kind() == UnitKind::APC || u.kind() == UnitKind::Tank ||
                          u.kind() == UnitKind::Turret)) continue;
                    // Eighteen silent seconds means neither side can see
                    // the other, whatever the range - a gate unit 45 m away
                    // across a mesa cliff is as unreachable as one 300 m out.
                    if (length(u.position() - mechs_[0].position()) < 30.0f) continue;
                    // Deliver the survivor to the player's OWN ground, up
                    // the lane where they are headed anyway. A spawn-point
                    // scan could pick a different terrace on a mesa map and
                    // leave the two staring across the same cliff forever.
                    Vec3 to = mechs_[0].position() + missionAxis_ * 45.0f +
                              Vec3(rng_.range(-12.0f, 12.0f), 0.0f,
                                   rng_.range(-12.0f, 12.0f));
                    const SurfaceHit h = world_.findFoothold(
                        to + Vec3(0.0f, 6.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f),
                        4.0f, 20.0f);
                    to.y = h.hit ? h.point.y
                                 : world_.terrain().height(to.x, to.z);
                    u.teleport(to);
                    u.forceAlert();
                }
                quietFor_ = 0.0f;
            }
            break;
        }
        case ObjectiveKind::DestroyMarked:
        case ObjectiveKind::Blackout: {
            int down = 0;
            for (int i : markedProps_)
                if (!props_[static_cast<size_t>(i)].alive) ++down;
            objectiveProgress_ = down;
            if (down >= objectiveTarget_) completeObjective();
            // Blackout: noise brings company. The alarm rises while any
            // hostile can see the player, and reinforcements arrive at a rate
            // the surviving radar chain sets.
            if (spec.kind == ObjectiveKind::Blackout) {
                bool seen = false;
                for (const Unit& u : units_) {
                    if (!u.alive() || u.team() != Team::Hostile) continue;
                    if (length(u.position() - playerPos) < u.stats().engageRange * 0.85f &&
                        world_.lineOfSight(u.hitCentre(), mechs_[0].hitCentre())) {
                        seen = true;
                        break;
                    }
                }
                alarm_ = clampf(alarm_ + (seen ? dt * 0.22f : -dt * 0.05f), 0.0f, 1.0f);
                const int mastsLeft = objectiveTarget_ - down;
                reinforceTimer_ -= dt * (0.4f + alarm_ * 1.6f);
                if (reinforceTimer_ <= 0.0f && mastsLeft > 0) {
                    reinforceTimer_ = 9.0f + 7.0f / static_cast<float>(mastsLeft);
                    spawnUnitAlerted(UnitKind::Drone, playerPos + missionAxis_ * 40.0f, 0,
                              Vec3(0.0f));
                    spawnUnitAlerted(UnitKind::Trooper, playerPos + missionAxis_ * 45.0f, 0,
                              Vec3(0.0f));
                }
            }
            break;
        }
        case ObjectiveKind::KillUnits: {
            int down = 0;
            for (int i : markedUnits_)
                if (!units_[static_cast<size_t>(i)].alive()) ++down;
            objectiveProgress_ = down;
            if (down >= objectiveTarget_) completeObjective();
            break;
        }
        case ObjectiveKind::Outrun: {
            // Reach the far end before the fire does. Being behind the line
            // is survivable for a few seconds and fatal for many.
            const float zoneAlong2 = dot(objectiveZone_, missionAxis_);
            const float pAlong2 = dot(playerPos, missionAxis_);
            objectiveProgress_ = static_cast<int>(std::max(0.0f, pAlong2 - barrageAlong_));
            if (toZone < spec.zoneRadius || pAlong2 >= zoneAlong2 - 4.0f) completeObjective();
            break;
        }
        case ObjectiveKind::ReachZone:
        case ObjectiveKind::Breakthrough: {
            // Crossing the LINE is the objective, not parking on a coin: the
            // zone may snap against the fortification itself, and a machine
            // pressed against the wall two metres short of the marker has
            // plainly broken through.
            const float zoneAlong2 = dot(objectiveZone_, missionAxis_);
            const float pAlong2 = dot(playerPos, missionAxis_);
            if (toZone < spec.zoneRadius || pAlong2 >= zoneAlong2 - 4.0f)
                completeObjective();
            // The walking barrage: stay mobile or be found by it.
            if (spec.kind == ObjectiveKind::Breakthrough) {
                shellTimer_ -= dt;
                if (shellTimer_ <= 0.0f) {
                    shellTimer_ = 3.2f + rng_.unit() * 2.2f;
                    static WeaponDef shell = [] {
                        WeaponDef w;
                        w.damage = 8.0f;
                        w.projectileSpeed = 70.0f;
                        w.blastRadius = 5.5f;
                        w.blastDamage = 30.0f;
                        w.gravity = -22.0f;
                        w.range = 700.0f;
                        w.tracerLength = 2.2f;
                        w.tracerRadius = 0.2f;
                        w.tracerColor = Vec3(1.0f, 0.5f, 0.2f);
                        return w;
                    }();
                    const Vec3 fall = playerPos +
                                      Vec3(rng_.range(-14.0f, 14.0f), 0.0f,
                                           rng_.range(-14.0f, 14.0f));
                    ShotRequest sr;
                    sr.origin = fall + Vec3(rng_.range(-20.0f, 20.0f), 85.0f,
                                            rng_.range(-20.0f, 20.0f));
                    sr.direction = normalize(fall - sr.origin);
                    sr.weapon = &shell;
                    sr.team = Team::Hostile;
                    sr.shooter = -1;
                    shots_.push_back(sr);
                }
            }
            break;
        }
        case ObjectiveKind::Escort: {
            if (escortUnit_ < 0) { completeObjective(); break; }
            Unit& escM = units_[static_cast<size_t>(escortUnit_)];
            // The reverse: halfway across, the road ahead of the crawler
            // fills with the people who let it get this far.
            if (spec.ambush && !escortAmbushSprung_) {
                const float startAlong = dot(checkpointPos_, missionAxis_);
                const float zoneAlong = dot(objectiveZone_, missionAxis_);
                const float eAlong = dot(escM.position(), missionAxis_);
                const float frac = (zoneAlong - startAlong != 0.0f)
                    ? (eAlong - startAlong) / (zoneAlong - startAlong) : 0.0f;
                if (frac > 0.5f) {
                    escortAmbushSprung_ = true;
                    spawnAmbush(escM.position() + missionAxis_ * 30.0f);
                }
            }
            // The crawler does not outrun its own protection. When the player
            // falls behind it creeps, which is what turns "the AI drove into
            // the guns and died" into an escort the player can actually do.
            // Mid-ford the crawler never waits: holding station in deep
            // water while the player finds the causeway is how an escort
            // starves a mission clock to death. It waits on dry ground only.
            const bool inWater = world_.hasWater() &&
                world_.terrain().height(escM.position().x, escM.position().z) <
                    world_.waterLevel();
            escM.setHold(!inWater && length(escM.position() - playerPos) > 42.0f);
            const Unit& esc = escM;
            if (!esc.alive()) {
                // Disabled, not destroyed: the crawler grinds to a halt,
                // repairs itself, and the ledger takes the hit. An instant
                // mission-fail here made the whole mission hinge on seconds
                // of concentrated AT fire the player may not even see.
                ledger_.checkpointDeaths += 1;
                combat_.explosion(escM.hitCentre(), 2.6f, Vec3(1.0f, 0.6f, 0.3f), rng_);
                escM.revive(0.45f);
                // Recovery drags it to solid footing: a crawler revived in
                // the water it just drowned in simply drowns again, forever.
                {
                    Vec3 at = escM.position();
                    Rng r(level_.seed * 77u + static_cast<uint32_t>(elapsed_ * 16.0f));
                    for (float radius = 6.0f; radius < 90.0f; radius += 12.0f) {
                        bool ok = false;
                        for (int tries = 0; tries < 20; ++tries) {
                            Vec3 cand = escM.position() +
                                        Vec3(r.range(-radius, radius), 0.0f,
                                             r.range(-radius, radius));
                            const SurfaceHit h = world_.findFoothold(
                                cand + Vec3(0.0f, 6.0f, 0.0f),
                                Vec3(0.0f, 1.0f, 0.0f), 4.0f, 14.0f);
                            const float standY = h.hit ? h.point.y
                                : world_.terrain().height(cand.x, cand.z);
                            if (world_.hasWater() &&
                                standY < world_.waterLevel() + 0.3f) continue;
                            if (world_.insideStructure(cand, 1.5f)) continue;
                            cand.y = standY;
                            at = cand;
                            ok = true;
                            break;
                        }
                        if (ok) break;
                    }
                    escM.teleport(at);
                }
                break;
            }
            // The recovery winch. A crawler that has stopped making progress
            // while free to move - wedged on a deck lip, pinned on a pillar -
            // gets dragged a stretch down the road. Rare, visible, and the
            // alternative is a mission that can time out through no fault of
            // the pilot's.
            {
                const float moved = length(esc.position() - escortLastPos_);
                escortLastPos_ = esc.position();
                const bool held = length(esc.position() - playerPos) > 42.0f;
                // A slow creep is not progress: anything under ~0.6 m/s for
                // this long counts as wedged. (The old dual threshold let a
                // crawler zeno-creep at millimetres a frame forever.)
                if (moved < dt * 0.6f) escortStuckFor_ += dt;
                else escortStuckFor_ = std::max(0.0f, escortStuckFor_ - dt * 2.0f);
                // Winch order: hop FORWARD first, whatever the player is
                // doing - a deck wedge is a deck wedge. Only when no hop
                // finds standing room AND the player is far does the crawler
                // recover BACKWARD to its cover.
                if (escortStuckFor_ > (held ? 14.0f : 7.0f)) {
                    escortStuckFor_ = 0.0f;
                    bool hopped = false;
                    for (float hopD = 14.0f; hopD <= 54.0f; hopD += 13.0f) {
                        Vec3 hop = esc.position() + missionAxis_ * hopD;
                        const SurfaceHit h = world_.findFoothold(
                            hop + Vec3(0.0f, 8.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f),
                            4.0f, 18.0f);
                        hop.y = h.hit ? h.point.y
                                      : world_.terrain().height(hop.x, hop.z);
                        if (!world_.hasWater() ||
                            hop.y > world_.waterLevel() + 0.3f) {
                            escM.teleport(hop);
                            hopped = true;
                            break;
                        }
                    }
                    if (!hopped && held) {
                        Vec3 back = playerPos + missionAxis_ * 14.0f;
                        const SurfaceHit h = world_.findFoothold(
                            back + Vec3(0.0f, 8.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f),
                            4.0f, 18.0f);
                        back.y = h.hit ? h.point.y
                                       : world_.terrain().height(back.x, back.z);
                        if (!world_.hasWater() ||
                            back.y > world_.waterLevel() + 0.3f)
                            escM.teleport(back);
                    }
                }
            }
            // Arrival is crossing the lane, not parking on a coin: the far
            // anchorage the zone snapped to may sit off the road across open
            // water, and "reach the exact marker" would strand the mission.
            const float escAlong = dot(esc.position(), missionAxis_);
            const float zoneAlong = dot(objectiveZone_, missionAxis_);
            if (length(flattenY(esc.position() - objectiveZone_)) < spec.zoneRadius ||
                escAlong >= zoneAlong - 8.0f) {
                escortUnit_ = -1;
                completeObjective();
            }
            break;
        }
        case ObjectiveKind::HoldZone: {
            const bool inside = toZone < spec.zoneRadius;
            if (inside) objectiveTimer_ -= dt;
            reinforceTimer_ -= dt;
            if (reinforceTimer_ <= 0.0f) {
                reinforceTimer_ = 8.0f + rng_.unit() * 5.0f;
                spawnUnitAlerted(rng_.unit() < 0.3f ? UnitKind::APC : UnitKind::Trooper,
                                 objectiveZone_ + missionAxis_ * 55.0f, 0, Vec3(0.0f));
                spawnUnitAlerted(UnitKind::Drone,
                                 objectiveZone_ - missionAxis_ * 50.0f, 0, Vec3(0.0f));
            }
            if (objectiveTimer_ <= 0.0f) completeObjective();
            break;
        }
        case ObjectiveKind::Rampage: {
            objectiveTimer_ -= dt;
            objectiveProgress_ = ledger_.cashDestruction;
            if (objectiveTimer_ <= 0.0f) completeObjective();
            break;
        }
        case ObjectiveKind::KillTarget: {
            bool anyAlive = false;
            int down = 0;
            for (int i : markedMechs_) {
                if (mechs_[static_cast<size_t>(i)].alive()) anyAlive = true;
                else ++down;
            }
            objectiveProgress_ = down;
            objectiveTarget_ = static_cast<int>(markedMechs_.size());
            if (!anyAlive) completeObjective();
            break;
        }
        case ObjectiveKind::Convoy: {
            objectiveTimer_ -= dt;
            int down = 0;
            bool escaped = false;
            const float edge = world_.arena().extent * 0.92f;
            for (int i : markedUnits_) {
                const Unit& u = units_[static_cast<size_t>(i)];
                if (!u.alive()) { ++down; continue; }
                if (std::fabs(u.position().x) > edge ||
                    std::fabs(u.position().z) > edge) escaped = true;
            }
            objectiveProgress_ = down;
            if (down >= objectiveTarget_) { completeObjective(); break; }
            if (escaped || objectiveTimer_ <= 0.0f) {
                phase_ = MissionPhase::Failed;
                endTimer_ = 0.0f;
            }
            break;
        }
        default: break;
    }
}

void Mission::settle(PlayerProfile& profile) const {
    // A bounty elite's weapon is kept whether or not the contract closed:
    // it was pulled off the wreck, not paid out.
    if (!ledger_.salvagedPart.empty() && !profile.hasUnlocked(ledger_.salvagedPart))
        profile.unlocked.push_back(ledger_.salvagedPart);
    if (phase_ != MissionPhase::Cleared) {
        // A failed attempt still pays for what you actually destroyed, at a
        // reduced rate. Paying nothing sounds like the right punishment until
        // you meet a player who cannot beat a mission with the machine they
        // own and cannot earn the money to improve it - at which point the run
        // is over and the game never says so. Partial salvage turns a wall
        // into a grind, which is recoverable.
        profile.deaths += 1;
        // A flat recovery payment on top of the salvage share. Without it, an
        // attempt where you killed nothing pays nothing, and a player whose
        // machine is simply not good enough for this mission has no way to
        // earn their way out of it.
        const int salvagePay = static_cast<int>(cashEarned_ * kFailureSalvage) +
                               kFailureFloor + level_.power * kFailureFloorPerPower;
        profile.cash += salvagePay;
        profile.totalEarned += salvagePay;
        for (size_t i = 1; i < mechs_.size(); ++i)
            if (!mechs_[i].alive()) profile.kills += 1;
        return;
    }
    {
        int take = cashEarned_;
        // A contract already on the books pays a QUARTER when you run it
        // again: a way out of a wall, never a way to farm. Fresh ground pays
        // in full, so money follows progress.
        const bool replay = profile.level <= profile.maxCleared;
        if (replay) take = take / 4;
        profile.cash += take;
        profile.totalEarned += take;
    }
    profile.deaths += ledger_.checkpointDeaths;
    for (size_t i = 1; i < mechs_.size(); ++i)
        if (!mechs_[i].alive()) profile.kills += 1;
    for (int i = 0; i < kUnitKindCount && i < 16; ++i) profile.kills += ledger_.unitKills[i];
    if (profile.level > profile.maxCleared) profile.maxCleared = profile.level;

    // Bank the magazines still in the player's racks, so salvage picked up on
    // the field carries forward.
    for (const MountedWeapon& w : mechs_[0].weapons()) {
        if (!w.part || w.part->weapon.ammo != AmmoKind::Limited) continue;
        // Replace rather than add: the profile's stock was loaded into the
        // mech at mission start, so what is in the racks now is the truth.
        for (auto& e : profile.ammoStock)
            if (e.first == w.part->id) { e.second = 0; break; }
        profile.addAmmo(w.part->id, w.reserve);
    }
    profile.level += 1;
}

void Mission::submit(Rasterizer& raster, const Vec3& viewPos,
                     float viewDistance, bool skipPlayer, float eyeRadius) const {
    world_.submit(raster, viewPos, viewDistance);
    for (const Unit& u : units_) u.submit(raster, viewPos);
    for (const Destructible& d : props_) submitDestructible(raster, d, viewPos);
    for (size_t i = 0; i < mechs_.size(); ++i) {
        // First-person views sit inside the player's own hull; drawing it
        // would wallpaper the screen with the inside of the turret.
        if (skipPlayer && i == 0) continue;
        if (!mechs_[i].alive()) continue;
        mechs_[i].submit(raster, viewPos);
    }
    combat_.submit(raster, viewPos, eyeRadius);
}

} // namespace sb
