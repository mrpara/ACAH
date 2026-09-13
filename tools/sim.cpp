// sim.cpp - a headless campaign runner.
//
// Plays missions with a scripted pilot and reports what actually happened:
// whether enemies engage, whether damage lands, whether waves advance and
// missions resolve. This is the regression test for the combat, AI and
// campaign layers, none of which can be judged from a screenshot.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>

#include "../src/core/campaign.h"
#include "../src/core/store.h"

using namespace sb;

namespace {

int argInt(int argc, char** argv, const char* name, int fallback) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], name) == 0) return std::atoi(argv[i + 1]);
    return fallback;
}

bool argFlag(int argc, char** argv, const char* name) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], name) == 0) return true;
    return false;
}

// A pilot that is competent but not superhuman: it closes to a working range,
// circles, and shoots at where the nearest enemy is going. Good enough to win
// the early missions, which is exactly the bar the test needs.
// Which enemy the pilot is currently committed to. Splitting fire between two
// machines that are both circling means neither ever dies, which is a mistake
// no real player makes twice.
int gLockedTarget = -1;

// The scripted pilot understands objectives now: it picks a focus - a marked
// installation, a convoy vehicle, the nearest hostile machine, or the zone the
// objective points at - drives toward it, and shoots whatever it is focused
// on. It remains deliberately average: no cover use, no high ground.
MechInput pilot(const Mission& m, float t) {
    MechInput in;
    const Mech& self = m.player();

    // A human unsticks themselves; the scripted pilot has to be told how.
    // If the machine has not moved for a few seconds while trying to, jump
    // and sidestep before resuming the route.
    // Net progress over a rolling window, not per-frame motion: a machine
    // grinding against a scarp jitters half a metre forever and never trips a
    // per-frame stillness test.
    static Vec3 sAnchor(0.0f);
    static float sWindow = 0.0f;
    static float sEscape = 0.0f;
    static int sEscapes = 0;
    static bool sTraveling = false;   // set below: was the pilot trying to go somewhere
    sWindow += 1.0f / 60.0f;
    if (sWindow > 3.0f) {
        if (sTraveling && length(self.position() - sAnchor) < 2.5f && sEscape <= 0.0f) {
            sEscape = 2.4f + 1.2f * (sEscapes % 3);   // longer detours if repeating
            ++sEscapes;
        } else if (length(self.position() - sAnchor) >= 2.5f) {
            sEscapes = 0;
        }
        sAnchor = self.position();
        sWindow = 0.0f;
    }
    static Vec3 sEscapeGoal(0.0f);
    if (sEscape > 0.0f) {
        sEscape -= 1.0f / 60.0f;
        in.jumpHeld = sEscape > 1.6f;             // charge then release
        Vec3 dir2;
        if (sEscapes >= 3 && lengthSq(flattenY(sEscapeGoal - self.position())) > 4.0f) {
            // Third strike: stop feeling for a way around and go OVER -
            // full-charge jump straight at the goal. In the dense arenas the
            // roofline is the only reliable route the scripted pilot has.
            dir2 = normalize(flattenY(sEscapeGoal - self.position()));
        } else {
            // A consistent sideways heading per escape, alternating sides.
            const float ang = (sEscapes % 2 ? 1.0f : -1.0f) * (PI * 0.55f);
            const Vec3 fwd = flattenY(self.forward());
            dir2 = Vec3(fwd.x * std::cos(ang) - fwd.z * std::sin(ang), 0.0f,
                        fwd.x * std::sin(ang) + fwd.z * std::cos(ang));
        }
        in.moveWorld = normalize(dir2 + Vec3(0.001f, 0.0f, 0.0f));
        in.throttle = 1.0f;
        in.aimPoint = self.position() + self.forward() * 30.0f;
        return in;
    }
    const ObjectiveSpec* obj = m.currentObjective();

    Vec3 focusPos = m.objectiveZone();       // where to go
    Vec3 shootPos(0.0f);                     // what to shoot
    Vec3 shootVel(0.0f);
    bool haveShoot = false;
    float standoff = 6.0f;

    auto nearestHostileThing = [&](Vec3* outPos, Vec3* outVel) -> float {
        float best = 1e9f;
        const Mech* n = m.nearestEnemy();
        if (n) {
            best = length(n->hitCentre() - self.position());
            *outPos = n->hitCentre();
            *outVel = n->velocity();
        }
        for (const Unit& u : m.units()) {
            if (!u.alive() || u.team() != Team::Hostile) continue;
            const float d = length(u.hitCentre() - self.position());
            if (d < best) { best = d; *outPos = u.hitCentre(); *outVel = Vec3(0.0f); }
        }
        return best;
    };

    const ObjectiveKind kind = obj ? obj->kind : ObjectiveKind::ClearHostiles;
    switch (kind) {
        case ObjectiveKind::DestroyMarked:
        case ObjectiveKind::Blackout: {
            float best = 1e9f;
            for (int i : m.markedProps()) {
                const Destructible& d = m.props()[static_cast<size_t>(i)];
                if (!d.alive) continue;
                const float dd = length(d.hitCentre() - self.position());
                if (dd < best) { best = dd; shootPos = d.hitCentre(); haveShoot = true; }
            }
            if (haveShoot) {
                // Far out, walk to the OBJECTIVE ZONE - a guaranteed
                // standable, findable point the targets ring - and only chase
                // the individual installation once inside its neighbourhood.
                // Chasing a mast through 200 m of forest is how the scripted
                // pilot got lost; the zone is the reliable waypoint.
                focusPos = (best > 70.0f) ? m.objectiveZone() : shootPos;
                standoff = 26.0f;
            }
            break;
        }
        case ObjectiveKind::Convoy:
        case ObjectiveKind::KillUnits: {
            float best = 1e9f;
            for (int i : m.markedUnits()) {
                const Unit& u = m.units()[static_cast<size_t>(i)];
                if (!u.alive()) continue;
                const float dd = length(u.hitCentre() - self.position());
                if (dd < best) { best = dd; shootPos = u.hitCentre(); haveShoot = true; }
            }
            if (haveShoot) { focusPos = shootPos; standoff = 30.0f; }
            break;
        }
        case ObjectiveKind::KillTarget:
        case ObjectiveKind::ClearHostiles:
        case ObjectiveKind::Rampage: {
            Vec3 p, vv;
            if (nearestHostileThing(&p, &vv) < 1e8f) {
                shootPos = p; shootVel = vv; haveShoot = true;
                focusPos = p; standoff = 22.0f;
            }
            break;
        }
        case ObjectiveKind::Escort: {
            // Stay with the charge, shoot what threatens it.
            if (m.escortIndex() >= 0) {
                const Unit& esc = m.units()[static_cast<size_t>(m.escortIndex())];
                if (esc.alive()) {
                    focusPos = esc.position();
                    standoff = 10.0f;
                    // The crawler is amphibious and will wade between decks;
                    // the pilot is NOT. Follow its progress along the lane
                    // line (the causeway), never its wake through the sound.
                    if (m.world().hasWater()) {
                        const Vec3 axis = m.missionAxisDir();
                        focusPos = axis * dot(esc.position(), axis) +
                                   Vec3(0.0f, esc.position().y, 0.0f);
                        standoff = 6.0f;
                    }
                }
            }
            Vec3 p, vv;
            if (nearestHostileThing(&p, &vv) < 190.0f) {
                shootPos = p; shootVel = vv; haveShoot = true;
            }
            break;
        }
        case ObjectiveKind::ReachZone:
        case ObjectiveKind::Breakthrough:
        case ObjectiveKind::Outrun:
        case ObjectiveKind::HoldZone:
        default: {
            Vec3 p, vv;
            if (nearestHostileThing(&p, &vv) < 190.0f) {
                shootPos = p; shootVel = vv; haveShoot = true;
            }
            break;
        }
    }

    // Movement: close on the focus, orbit at standoff once there. Long legs
    // of travel are broken into sub-goals sampled from open ground - spawn
    // points are guaranteed standable and outside buildings, so hopping
    // between the best of a handful is a poor man's navmesh, and enough to
    // get a scripted pilot through the dense grid arenas.
    // A gun on a ROOF nearby: hugging the building's blind side means never
    // getting a sight line. Back out for the angle, then fight.
    if (haveShoot) {
        const float upDiff = shootPos.y - self.position().y;
        const float flatD = length(flattenY(shootPos - self.position()));
        if (upDiff > 3.5f && flatD < 45.0f) {
            focusPos = self.position() +
                       normalize(flattenY(self.position() - shootPos) +
                                 Vec3(1e-3f, 0.0f, 0.0f)) * 40.0f;
            standoff = 6.0f;
        }
    }

    // Hurt? Go and weld. Repair salvage is the only healing in a mission now,
    // so a pilot that ignores it is not measuring the game the way a human
    // plays it.
    if (self.healthFraction() < 0.62f) {
        float bestR = 90.0f * 90.0f;
        Vec3 bestPos(0.0f);
        bool found = false;
        for (const AmmoPickup& pk : m.combat().pickups()) {
            if (pk.taken || !pk.isRepair()) continue;
            const float d = lengthSq(pk.pos - self.position());
            if (d < bestR) { bestR = d; bestPos = pk.pos; found = true; }
        }
        if (found) { focusPos = bestPos; standoff = 1.5f; }
    }

    Vec3 navTarget = focusPos;
    {
        static Vec3 sSubGoal(0.0f);
        static float sSubTimer = 0.0f;
        static Rng sNavRng(777u);
        const float distToFocus = length(flattenY(focusPos - self.position()));
        sSubTimer -= 1.0f / 60.0f;
        if (distToFocus > 70.0f) {
            const bool reached = length(flattenY(sSubGoal - self.position())) < 12.0f;
            if (reached || sSubTimer <= 0.0f) {
                sSubTimer = 10.0f;
                float bestScore = 1e9f;
                Vec3 best = focusPos;
                if (m.world().hasWater()) {
                    // Island maps: dry spawn points are all on THIS island, so
                    // hopping between them never crosses the sound. The lane
                    // axis IS the causeway line - walk sub-goals straight down
                    // it toward the focus and the decks carry you across.
                    const Vec3 axis = m.missionAxisDir();
                    const float here = dot(self.position(), axis);
                    const float want = dot(focusPos, axis);
                    const float step = (want > here) ? 34.0f : -34.0f;
                    best = axis * (here + step);
                    // Aim for the deck TOP, not the shadow under it: cast down
                    // from above so the sub-goal is the road surface.
                    const SurfaceHit h = m.world().findFoothold(
                        best + Vec3(0.0f, 12.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f),
                        2.0f, 24.0f);
                    best.y = h.hit ? h.point.y : self.position().y;
                } else {
                    for (int c = 0; c < 8; ++c) {
                        const Vec3 cand = m.world().findSpawnPoint(
                            sNavRng, self.position(), 26.0f, 48.0f);
                        const float score = length(flattenY(focusPos - cand));
                        if (score < bestScore) { bestScore = score; best = cand; }
                    }
                }
                sSubGoal = best;
            }
            navTarget = sSubGoal;
        }
    }
    sEscapeGoal = navTarget;
    const Vec3 to = navTarget - self.position();
    const float dist = length(flattenY(to));
    sTraveling = dist > standoff + 9.0f;   // stuck detection only when en route
    const Vec3 dir = normalize(flattenY(to) + Vec3(0.0f, 0.0f, 1e-4f));
    const Vec3 right = normalize(cross(Vec3(0.0f, 1.0f, 0.0f), dir));
    if (dist > standoff) {
        in.moveWorld = dir;
        in.throttle = 1.0f;
    } else {
        in.moveWorld = normalize(right + dir * 0.15f);
        in.throttle = 0.7f;
    }
    // JINK. Enemies lead their shots now, so a pilot that walks in a
    // straight line gets hit by everything - which is true for the player
    // too, and is the point. The scripted pilot keeps a crossing component
    // while it is being shot at, so the harness measures a mission the way
    // a competent human would play it rather than the way a bollard would.
    if (haveShoot) {
        static float sJink = 0.0f;
        static float sJinkSign = 1.0f;
        sJink -= 1.0f / 60.0f;
        if (sJink <= 0.0f) {
            sJink = 1.1f + 0.9f * ((int)(t * 7.0f) % 3);
            sJinkSign = -sJinkSign;
        }
        const Vec3 toFoe = normalize(flattenY(shootPos - self.position()) +
                                     Vec3(1e-4f, 0.0f, 0.0f));
        const Vec3 cross2 = normalize(cross(Vec3(0.0f, 1.0f, 0.0f), toFoe));
        in.moveWorld = normalize(in.moveWorld + cross2 * (sJinkSign * 0.85f));
        in.throttle = 1.0f;
    }
    // Roof-marooned. Climbing works well enough now that the pilot regularly
    // SUMMITS buildings on its way somewhere and then circles the parapet
    // over a ground-level target forever. Standing well above the goal and
    // close to it in plan: drive straight at it and hop off the edge.
    {
        static float sDrop = 0.0f;
        const float groundAtGoal =
            m.world().terrain().height(navTarget.x, navTarget.z);
        const float above = self.position().y - groundAtGoal;
        if (above > 4.5f && dist < 70.0f && dist > 4.0f &&
            self.state() != MechState::Airborne) {
            in.moveWorld = dir;
            in.throttle = 1.0f;
            sDrop += 1.0f / 60.0f;
            in.jumpHeld = sDrop < 0.4f;        // short charge, then release
            if (sDrop > 0.9f) sDrop = 0.0f;
        } else {
            sDrop = 0.0f;
        }
    }

    if (haveShoot) {
        Vec3 aim = shootPos;
        float speed = 150.0f;
        for (const MountedWeapon& w : self.weapons())
            if (w.part) speed = std::max(speed, w.part->weapon.projectileSpeed);
        Vec3 led;
        if (leadTarget(self.hitCentre(), shootPos, shootVel, speed, &led)) aim = led;
        // Elevated target: aim at its upper body, not its centre - a roof
        // gun's centre is often a metre behind the parapet line.
        if (shootPos.y - self.position().y > 3.5f) aim.y += 0.9f;
        in.aimPoint = aim;
        const float sr = length(shootPos - self.position());
        in.fireHeld = sr < 110.0f;
        in.fire2Held = sr < 80.0f;
    } else {
        in.aimPoint = self.position() + self.forward() * 40.0f;
    }
    (void)t;
    (void)gLockedTarget;
    return in;
}

} // namespace

int main(int argc, char** argv) {
    const int missions = argInt(argc, argv, "--missions", 3);
    const int startAt = argInt(argc, argv, "--start", 1);   // 1-based mission
    const int maxSeconds = argInt(argc, argv, "--seconds", 1150);
    const bool verbose = argFlag(argc, argv, "--verbose");
    const bool shopping = !argFlag(argc, argv, "--no-store");
    // --strong: deploy a top-tier machine. Verifies that the boss missions
    // RESOLVE for a properly equipped pilot, separately from whether the
    // scripted pilot can earn one.
    const bool strong = argFlag(argc, argv, "--strong");
    const float dt = 1.0f / 60.0f;

    PlayerProfile profile = newProfile();
    Store store;
    if (startAt > 1) {
        profile.level = startAt - 1;
        profile.maxCleared = startAt - 1;
        profile.cash += 900 * (startAt - 1);   // rough progression stand-in
    }
    if (strong) {
        profile.loadout.chassis = "ch_revenant";
        profile.loadout.legs = "lg_harrier";
        profile.loadout.engine = "en_halcyon";
        profile.loadout.armor = "ar_ablative";
        profile.loadout.sensor = "se_seeker";
        profile.loadout.ensureWeaponSlots();
        for (size_t i = 0; i < profile.loadout.weapons.size(); ++i)
            profile.loadout.weapons[i] = (i < 2) ? "wp_javelin" : "wp_pd9";
        profile.addAmmo("wp_javelin", 200);
        profile.cash += 20000;
    }

    int wins = 0, losses = 0;

    for (int mi = 0; mi < missions; ++mi) {
        const std::vector<LevelDef>& levels = campaignLevels();
        LevelDef level = (profile.level < static_cast<int>(levels.size()))
                             ? levels[static_cast<size_t>(profile.level)]
                             : endlessLevel(profile.level);
        // Same rule the game uses: a retry is a new roll, not a rerun.
        // (No death mixing: a retry replays the identical level.)

        Mission mission;
        mission.begin(level, profile);
        // The sim has no briefing screen to press through, so release combat
        // directly. A mission that begins in Briefing never resolves.
        mission.beginCombat();
        gLockedTarget = -1;

        const float startHp = mission.player().health();
        int frames = 0;
        int lastWave = -1;
        float dealt = 0.0f;
        const int maxFrames = maxSeconds * 60;

        while (frames < maxFrames && mission.phase() != MissionPhase::Cleared &&
               mission.phase() != MissionPhase::Failed) {
            mission.update(dt, pilot(mission, frames * dt));
            dealt += mission.damageDealtThisFrame();
            if (verbose && frames % 60 == 0 && mission.escortIndex() >= 0) {
                const Unit& esc = mission.units()[static_cast<size_t>(mission.escortIndex())];
                std::printf("      ESC pos %.0f,%.1f,%.0f hp %.0f alive %d terr %.1f wl %.1f\n",
                            static_cast<double>(esc.position().x),
                            static_cast<double>(esc.position().y),
                            static_cast<double>(esc.position().z),
                            static_cast<double>(esc.health()), esc.alive() ? 1 : 0,
                            static_cast<double>(mission.world().terrain().height(
                                esc.position().x, esc.position().z)),
                            static_cast<double>(mission.world().waterLevel()));
            }
            if (verbose && frames % 300 == 0) {
                const ObjectiveSpec* ob = mission.currentObjective();
                int gate = 0;
                float nearGate = 1e9f;
                for (const Unit& u : mission.units())
                    if (u.alive() && u.team() == Team::Hostile &&
                        (u.kind() == UnitKind::APC || u.kind() == UnitKind::Tank ||
                         u.kind() == UnitKind::Turret)) {
                        ++gate;
                        nearGate = std::min(nearGate,
                            length(u.position() - mission.player().position()));
                    }
                std::printf("      OBJ %d/%d %-12s prog %d/%d gateUnits %d near %.0f zone %.0f\n",
                            mission.objectiveIndex() + 1, mission.objectiveCount(),
                            ob ? ob->label.c_str() : "-",
                            mission.objectiveProgress(), mission.objectiveTarget(),
                            gate, static_cast<double>(nearGate),
                            static_cast<double>(length(mission.player().position() -
                                                       mission.objectiveZone())));
                for (int i : mission.markedUnits()) {
                    const Unit& u = mission.units()[static_cast<size_t>(i)];
                    if (!u.alive()) continue;
                    std::printf("        mark[%d] %s pos %.0f,%.0f,%.0f hp %.0f dist %.0f\n",
                                i, unitKindName(u.kind()),
                                static_cast<double>(u.position().x),
                                static_cast<double>(u.position().y),
                                static_cast<double>(u.position().z),
                                static_cast<double>(u.health()),
                                static_cast<double>(length(u.position() -
                                    mission.player().position())));
                }
            }
            if (verbose && frames % 30 == 0) {
                std::printf("      t=%5.1f hp %6.1f  state %-10s pos %.0f,%.0f,%.0f  "
                            "shots %d  enemies %d\n",
                            static_cast<double>(frames * dt),
                            static_cast<double>(mission.player().health()),
                            mechStateName(mission.player().state()),
                            static_cast<double>(mission.player().position().x),
                            static_cast<double>(mission.player().position().y),
                            static_cast<double>(mission.player().position().z),
                            mission.combat().liveProjectiles(),
                            mission.enemiesAlive());
                std::printf("            dealt %.0f hunt=%d | enemies:", static_cast<double>(dealt),
                            mission.hunting() ? 1 : 0);
                for (size_t k = 1; k < mission.mechs().size(); ++k) {
                    const Mech& e = mission.mechs()[k];
                    if (!e.alive()) continue;
                    std::printf(" [hp %.0f @ %.0f,%.0f,%.0f d=%.0f]",
                                static_cast<double>(e.health()),
                                static_cast<double>(e.position().x),
                                static_cast<double>(e.position().y),
                                static_cast<double>(e.position().z),
                                static_cast<double>(length(e.position() - mission.player().position())));
                }
                std::printf(" extent=%.0f\n", static_cast<double>(mission.world().extent()));
            }
            if (verbose && mission.waveIndex() != lastWave) {
                lastWave = mission.waveIndex();
                std::printf("    wave %d armed, %d hostiles, player %.0f hp\n",
                            lastWave + 1, mission.enemiesAlive(),
                            static_cast<double>(mission.player().health()));
            }
            ++frames;
        }

        const bool cleared = mission.phase() == MissionPhase::Cleared;
        const char* verdict = cleared ? "CLEARED"
                            : (mission.phase() == MissionPhase::Failed ? "FAILED" : "TIMEOUT");
        std::printf("mission %2d  %-16s %-16s %-8s  %5.1fs  hp %3.0f/%3.0f  "
                    "kills %d/%d  cash +%d\n",
                    profile.level + 1, level.name.c_str(),
                    mission.world().arena().name.c_str(), verdict,
                    static_cast<double>(frames * dt),
                    static_cast<double>(mission.player().health()),
                    static_cast<double>(startHp),
                    static_cast<int>(mission.mechs().size()) - 1 - mission.enemiesAlive(),
                    static_cast<int>(mission.mechs().size()) - 1,
                    mission.cashEarned());
        {
            // Where the money actually came from, and what the player ends up
            // banking after the replay and failure rules are applied - the
            // headline figure above is gross, pre-settlement.
            const MissionLedger& L = mission.ledger();
            const int gross = mission.cashEarned();
            const int banked = cleared
                ? ((profile.level <= profile.maxCleared) ? gross / 4 : gross)
                : (static_cast<int>(gross * 0.45f) + 160 + level.power * 115);
            std::printf("    pay: kills %d  wrecking %d  objectives %d  time %d"
                        "  clearance %d (%d sectors)  completion %d  [%d props]"
                        "  -> gross %d, BANKED %d\n",
                        L.cashKills, L.cashDestruction, L.cashObjectives,
                        L.cashTimeBonus, L.cashClearance, L.sectorsCleared,
                        level.completionBonus, L.propsDestroyed, gross, banked);
        }

        if (mission.phase() == MissionPhase::Cleared) ++wins;
        else if (mission.phase() == MissionPhase::Failed) ++losses;
        else break;   // a timeout means something is stuck; stop rather than loop
        (void)cleared;

        mission.settle(profile);

        // Between missions, buy the best thing affordable in every slot. Price
        // is a decent proxy for quality here because the catalog is priced by
        // power, and it exercises the whole store path: listing, comparison,
        // fitting and the trade-in.
        if (shopping) {
            store.open(profile);
            const std::vector<Slot>& slots = storeSlots();
            // Hull before guns, in two passes. The old single pass walked the
            // slot list in menu order, which puts weapons first, and a bot that
            // spends its entire purse on a gun and then cannot afford a chassis
            // is not a model of any human player - it just made the economy
            // look unsurvivable when what was broken was the shopping.
            for (int pass = 0; pass < 2; ++pass) {
                for (size_t si = 0; si < slots.size(); ++si) {
                    const bool isGun = (slots[si] == Slot::Weapon);
                    if ((pass == 0) == isGun) continue;
                    // Walk the slot list to the one we want.
                    while (store.slot() != slots[si]) store.moveCursor(1);

                    const int mounts = (slots[si] == Slot::Weapon)
                        ? std::max<int>(1, static_cast<int>(store.currentLoadout().weapons.size()))
                        : 1;
                    for (int mount = 0; mount < mounts; ++mount) {
                        store.confirm();      // into the parts pane
                        // Find the priciest affordable entry.
                        const int n = static_cast<int>(store.listing().size());
                        // Never take a cheaper part than the one already fitted:
                        // there is no refund on a downgrade, so a bot that just
                        // buys the priciest affordable thing will happily spend
                        // its winnings making the machine worse.
                        const PartDef* have = store.fitted();
                        int bestIdx = -1;
                        int bestPrice = have ? have->price : -1;
                        for (int i = 0; i < n; ++i) {
                            const PartDef* p = store.listing()[static_cast<size_t>(i)];
                            if (p->price > bestPrice && p->price <= profile.cash)
                                { bestPrice = p->price; bestIdx = i; }
                        }
                        if (bestIdx >= 0) {
                            while (store.cursor() != bestIdx) store.moveCursor(1);
                            // Refuse anything that would overdraw the reactor:
                            // buying a gun the engine cannot feed derates the
                            // whole machine, which is worse than not buying it.
                            // And refuse anything that leaves the machine
                            // slower than 4.2 m/s: the maps are long routes
                            // now, and a bot that armours itself down to a
                            // crawl times out on travel alone - a mistake a
                            // human corrects after one mission.
                            // 6.1 m/s, not 4.2: ground pace carries the run
                            // multiplier in the derived stat now, so the old
                            // floor was letting the bot armour itself down to
                            // what used to be a three-metre-a-second crawl.
                            if (store.canAfford() && !store.alreadyFitted() &&
                                !store.previewStats().overdrawn &&
                                store.previewStats().maxSpeed >= 6.1f)
                                store.confirm();
                        }
                        store.back();
                        if (slots[si] == Slot::Weapon) store.nextSlot(1);
                    }
                }
            }
            const MechStats st = deriveStats(store.currentLoadout());
            const Loadout& cl = store.currentLoadout();
            std::printf("    workshop: %s | cash %d\n",
                        loadoutSummary(cl, st).c_str(), profile.cash);
            std::printf("      fit: %s / %s / %s / %s / %s | guns:",
                        cl.chassis.c_str(), cl.legs.c_str(), cl.engine.c_str(),
                        cl.armor.c_str(), cl.sensor.c_str());
            for (const std::string& w : cl.weapons)
                std::printf(" %s", w.empty() ? "-" : w.c_str());
            std::printf("\n");
            store.close();
        }
    }

    std::printf("\n%d cleared, %d failed | balance %d cr | %d kills over the run\n",
                wins, losses, profile.cash, profile.kills);
    return (wins > 0) ? 0 : 1;
}
