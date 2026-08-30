// ctrl.cpp - a controls harness.
//
// Drives the real Game with synthetic InputState and reports what the machine
// actually did: how far forward pressing W travels, how much the camera drifts
// when you are not touching the mouse, which way the mouse turns the view, and
// how high a jump gets. These are the things a screenshot cannot tell you and
// that are miserable to test by hand.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "../src/core/game.h"

using namespace sb;

namespace {

bool argFlag(int argc, char** argv, const char* name) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], name) == 0) return true;
    return false;
}

// The keys the platform layer actually produces.
enum class Key { Enter, Space, Up, Down, Left, Right, Backspace, Tab, X };

// Exactly the mapping in src/platform/main.cpp. This has to stay in step with
// it, because a harness that invents its own flag combinations tests a game
// nobody can play: the workshop shipped broken precisely because this pressed
// `menuConfirm` alone, which no key produces - the real Enter also raises
// `menuNext`, and that closed the store on the same keystroke.
InputState press(Key k, GameScreen screen) {
    InputState in;
    switch (k) {
        case Key::Enter:
            in.menuConfirm = true;
            in.menuNext = true;
            break;
        case Key::Space:
            if (screen == GameScreen::Store) in.menuDeploy = true;
            else in.jumpHeld = true;
            break;
        case Key::Up:        in.menuUp = true; break;
        case Key::Down:      in.menuDown = true; break;
        case Key::Left:      in.menuLeft = true; break;
        case Key::Right:     in.menuRight = true; break;
        case Key::Backspace: in.menuBack = true; break;
        case Key::Tab:
            if (screen == GameScreen::Store) in.menuToggle = true;
            break;
        case Key::X:         in.menuSell = true; break;
    }
    return in;
}

// Taps a key for one frame, then idles, the way a human keystroke arrives.
void tap(Game& g, Key k, int idleFrames = 3) {
    const float dt = 1.0f / 60.0f;
    g.update(dt, press(k, g.screen()));
    for (int i = 0; i < idleFrames; ++i) {
        InputState idle;
        g.update(dt, idle);
    }
}

// Runs the game past the briefing so the pilot has control.
void deploy(Game& g) {
    tap(g, Key::Enter, 10);
}

} // namespace

int main(int argc, char** argv) {
    const float dt = 1.0f / 60.0f;
    const bool verbose = argFlag(argc, argv, "--verbose");

    // ---------------------------------------------------------- walking ----
    {
        Game g;
        g.init(4242u, 160, 48, 1);
        g.setCellAspect(0.5f);
        deploy(g);

        const Vec3 start = g.player().position();
        const Vec3 camFwd0 = normalize(g.camera().forward);

        InputState in;
        in.forward = true;
        float peakSpeed = 0.0f;
        for (int i = 0; i < 180; ++i) {
            g.update(dt, in);
            peakSpeed = std::max(peakSpeed, g.player().speed());
        }
        const Vec3 moved = g.player().position() - start;
        const Vec3 flat = flattenY(moved);
        const float dist = length(flat);
        // How much of the travel went the way the camera was pointing, versus
        // sideways. A value near 1 means W goes where you are looking.
        const float along = (dist > 1e-3f)
            ? dot(normalize(flat), normalize(flattenY(camFwd0))) : 0.0f;

        std::printf("WALK    3s of W: %.1f m travelled, peak %.1f m/s, "
                    "%.0f%% along the view\n",
                    static_cast<double>(dist), static_cast<double>(peakSpeed),
                    static_cast<double>(along * 100.0f));

        // Strafe direction: D should go to the camera's right.
        Game g2;
        g2.init(4242u, 160, 48, 1);
        g2.setCellAspect(0.5f);
        deploy(g2);
        const Vec3 s2 = g2.player().position();
        const Camera& c2 = g2.camera();
        const Vec3 camRight = normalize(flattenY(c2.right));
        InputState strafe;
        strafe.right = true;
        for (int i = 0; i < 90; ++i) g2.update(dt, strafe);
        const Vec3 d2 = flattenY(g2.player().position() - s2);
        const float rightness = (length(d2) > 1e-3f)
            ? dot(normalize(d2), camRight) : 0.0f;
        std::printf("STRAFE  D goes %.0f%% to the camera's right "
                    "(negative means the keys are mirrored)\n",
                    static_cast<double>(rightness * 100.0f));
    }

    // ------------------------------------------------------- camera drift --
    {
        Game g;
        g.init(4242u, 160, 48, 1);
        g.setCellAspect(0.5f);
        deploy(g);

        // Walk forward with no mouse input at all. A well-behaved third-person
        // camera holds its bearing; one derived from the machine's heading
        // swings around as the body turns.
        InputState in;
        in.forward = true;
        Vec3 prev = normalize(flattenY(g.camera().forward));
        float worst = 0.0f, total = 0.0f;
        for (int i = 0; i < 240; ++i) {
            g.update(dt, in);
            const Vec3 now = normalize(flattenY(g.camera().forward));
            const float step = std::acos(clampf(dot(prev, now), -1.0f, 1.0f));
            worst = std::max(worst, step);
            total += step;
            prev = now;
        }
        std::printf("CAMERA  4s of W, no mouse: view swung %.1f deg total, "
                    "worst %.2f deg/frame\n",
                    static_cast<double>(total * 180.0f / PI),
                    static_cast<double>(worst * 180.0f / PI));
    }

    // --------------------------------------------------------- mouse look --
    {
        Game g;
        g.init(4242u, 160, 48, 1);
        g.setCellAspect(0.5f);
        deploy(g);

        const Vec3 f0 = normalize(g.camera().forward);
        const Vec3 r0 = normalize(g.camera().right);
        InputState in;
        in.mouseDX = 30.0f;                  // push the mouse right
        for (int i = 0; i < 20; ++i) g.update(dt, in);
        const Vec3 f1 = normalize(g.camera().forward);
        const float turnedRight = dot(normalize(flattenY(f1)), r0);

        Game g3;
        g3.init(4242u, 160, 48, 1);
        g3.setCellAspect(0.5f);
        deploy(g3);
        const float y0 = normalize(g3.camera().forward).y;
        InputState down;
        down.mouseDY = 30.0f;                // push the mouse down
        for (int i = 0; i < 20; ++i) g3.update(dt, down);
        const float y1 = normalize(g3.camera().forward).y;

        std::printf("MOUSE   right -> %s (%.2f), down -> %s (%.3f to %.3f)\n",
                    turnedRight > 0.05f ? "view turns RIGHT" :
                    turnedRight < -0.05f ? "view turns LEFT (inverted)" : "no turn",
                    static_cast<double>(turnedRight),
                    y1 < y0 - 0.01f ? "view tilts DOWN" :
                    y1 > y0 + 0.01f ? "view tilts UP (inverted)" : "no tilt",
                    static_cast<double>(y0), static_cast<double>(y1));
        (void)f0;
    }

    // ----------------------------------------------- fire groups & toggles --
    {
        Game g;
        g.init(4242u, 160, 48, 1);
        g.setCellAspect(0.5f);
        deploy(g);
        InputState idle;
        for (int i = 0; i < 30; ++i) g.update(dt, idle);

        auto firedMask = [&](bool lmb, bool rmb, int toggleFirst) {
            // Fresh cooldowns, then hold the trigger for half a second.
            InputState wait;
            for (int i = 0; i < 90; ++i) g.update(dt, wait);
            if (toggleFirst) {
                InputState t;
                t.toggleMask = toggleFirst;
                g.update(dt, t);
            }
            InputState fire;
            fire.fireHeld = lmb;
            fire.fire2Held = rmb;
            int mask = 0;
            for (int i = 0; i < 30; ++i) {
                g.update(dt, fire);
                const std::vector<MountedWeapon>& ws = g.player().weapons();
                for (size_t w = 0; w < ws.size(); ++w)
                    if (ws[w].part && ws[w].spin > 0.5f) mask |= 1 << w;
            }
            return mask;
        };

        const std::vector<MountedWeapon>& ws = g.player().weapons();
        int group0 = 0, group1 = 0;
        for (size_t w = 0; w < ws.size(); ++w) {
            if (!ws[w].part) continue;
            if (ws[w].group == 0) group0 |= 1 << w; else group1 |= 1 << w;
        }
        const int lmbMask = firedMask(true, false, 0);
        const int rmbMask = firedMask(false, true, 0);
        const int offMask = firedMask(true, false, 1);   // mount 1 toggled off
        // Toggle it back for cleanliness.
        { InputState t; t.toggleMask = 1; g.update(dt, t); }

        const bool lmbOk = lmbMask == group0;
        const bool rmbOk = rmbMask == group1;
        const bool togOk = (offMask & 1) == 0;
        std::printf("GROUPS  mounts L=%x R=%x | LMB fired %x %s, RMB fired %x %s, "
                    "toggled-off mount stayed silent: %s\n",
                    group0, group1, lmbMask, lmbOk ? "(exact)" : "(WRONG)",
                    rmbMask, rmbOk ? "(exact)" : "(WRONG)",
                    togOk ? "yes" : "NO - TOGGLE BROKEN");
    }

    // ------------------------------------------------- save/load roundtrip --
    {
        PlayerProfile a;
        a.loadout.chassis = "ch_shrike";
        a.loadout.legs = "lg_harrier";
        a.loadout.engine = "en_milspec";
        a.loadout.armor = "ar_weave";
        a.loadout.sensor = "se_wide";
        a.loadout.ensureWeaponSlots();
        if (!a.loadout.weapons.empty()) a.loadout.weapons[0] = "wp_javelin";
        a.loadout.weaponGroups.assign(a.loadout.weapons.size(), 0);
        if (!a.loadout.weaponGroups.empty()) a.loadout.weaponGroups[0] = 1;
        a.cash = 3141;
        a.level = 6;
        a.maxCleared = 5;
        a.kills = 42;
        a.deaths = 3;
        a.totalEarned = 12345;
        a.addAmmo("wp_javelin", 24);

        const char* path = "acah_save_test.txt";
        PlayerProfile b;
        const bool saved = saveProfile(a, path);
        const bool loaded = loadProfile(b, path);
        std::remove(path);
        const bool same = b.loadout.chassis == a.loadout.chassis &&
                          b.loadout.legs == a.loadout.legs &&
                          b.loadout.engine == a.loadout.engine &&
                          b.loadout.armor == a.loadout.armor &&
                          b.loadout.sensor == a.loadout.sensor &&
                          !b.loadout.weapons.empty() &&
                          b.loadout.weapons[0] == "wp_javelin" &&
                          !b.loadout.weaponGroups.empty() &&
                          b.loadout.weaponGroups[0] == 1 &&
                          b.cash == a.cash && b.level == a.level &&
                          b.maxCleared == a.maxCleared && b.kills == a.kills &&
                          b.deaths == a.deaths && b.totalEarned == a.totalEarned &&
                          b.ammoFor("wp_javelin") == 24;
        std::printf("SAVE    write %s, read %s, roundtrip %s\n",
                    saved ? "ok" : "FAILED", loaded ? "ok" : "FAILED",
                    same ? "EXACT" : "LOSSY - FIELDS DIFFER");
    }

    // ------------------------------------------------- first-person views --
    {
        Game g;
        g.init(4242u, 160, 48, 1);
        g.setCellAspect(0.5f);
        deploy(g);
        InputState idle;
        for (int i = 0; i < 30; ++i) g.update(dt, idle);
        const float orbitDist = length(g.camera().pos - g.player().position());

        InputState fpv;
        fpv.toggleFpv = true;
        g.update(dt, fpv);
        for (int i = 0; i < 10; ++i) g.update(dt, idle);
        const float fpvDist = length(g.camera().pos - g.player().position());

        InputState scope;
        scope.cycleScope = true;
        g.update(dt, scope);            // fpv off, scope stage 1
        for (int i = 0; i < 10; ++i) g.update(dt, idle);
        const float scopeFov1 = g.camera().fovY;
        g.update(dt, scope);            // stage 2
        for (int i = 0; i < 10; ++i) g.update(dt, idle);
        const float scopeFov2 = g.camera().fovY;
        g.update(dt, scope);            // off again
        for (int i = 0; i < 40; ++i) g.update(dt, idle);
        const float backDist = length(g.camera().pos - g.player().position());

        std::printf("FPV     orbit %.1f m -> driver cam %.1f m from hull "
                    "(%s), sight fov %.1f -> %.1f deg (%s), back out to %.1f m\n",
                    static_cast<double>(orbitDist), static_cast<double>(fpvDist),
                    fpvDist < 6.0f ? "ON THE MACHINE" : "STILL ORBITING - BROKEN",
                    static_cast<double>(scopeFov1 * 180.0f / PI),
                    static_cast<double>(scopeFov2 * 180.0f / PI),
                    scopeFov2 < scopeFov1 ? "magnifies" : "NO ZOOM - BROKEN",
                    static_cast<double>(backDist));
    }

    // -------------------------------------------------------------- jump ---
    {
        Game g;
        g.init(4242u, 160, 48, 1);
        g.setCellAspect(0.5f);
        deploy(g);

        const float ground = g.player().position().y;
        InputState in;
        float peak = ground;
        bool everAirborne = false;
        int chargeFrames = 0;
        for (int i = 0; i < 200; ++i) {
            in.jumpHeld = (i < 60);          // hold space for a second
            g.update(dt, in);
            peak = std::max(peak, g.player().position().y);
            if (g.player().state() == MechState::Airborne) everAirborne = true;
            if (g.player().state() == MechState::Crouching) ++chargeFrames;
            if (verbose && i % 10 == 0)
                std::printf("        t=%.2f y=%+.2f %s charge=%.2f\n",
                            static_cast<double>(i * dt),
                            static_cast<double>(g.player().position().y - ground),
                            mechStateName(g.player().state()),
                            static_cast<double>(g.player().jumpCharge()));
        }
        std::printf("JUMP    charged %.2fs, %s, apex %.2f m above the start\n",
                    static_cast<double>(chargeFrames * dt),
                    everAirborne ? "left the ground" : "NEVER LEFT THE GROUND",
                    static_cast<double>(peak - ground));
    }

    // ------------------------------------------------------------ briefing --
    // Nothing hostile may act until the player has deployed. Being shot at
    // while reading the mission brief is not tension, it is a bug.
    {
        Game g;
        g.init(4242u, 160, 48, 1);
        g.setCellAspect(0.5f);

        std::vector<Vec3> start;
        for (size_t i = 1; i < g.mission().mechs().size(); ++i)
            start.push_back(g.mission().mechs()[i].position());

        const float hp0 = g.player().health();
        int shotsSeen = 0;
        for (int i = 0; i < 60 * 8; ++i) {          // eight seconds of reading
            InputState idle;
            g.update(dt, idle);
            shotsSeen = std::max(shotsSeen, g.mission().combat().liveProjectiles());
        }
        float moved = 0.0f;
        for (size_t i = 1; i < g.mission().mechs().size(); ++i)
            moved = std::max(moved, length(g.mission().mechs()[i].position() -
                                           start[i - 1]));

        std::printf("BRIEF   8s on the briefing: %d rounds in the air, "
                    "hostiles moved %.1f m, player %.0f -> %.0f hp\n",
                    shotsSeen, static_cast<double>(moved),
                    static_cast<double>(hp0),
                    static_cast<double>(g.player().health()));

        // And they must start once deployed.
        tap(g, Key::Enter, 4);
        for (int i = 0; i < 60 * 6; ++i) {
            InputState idle;
            g.update(dt, idle);
        }
        float movedAfter = 0.0f;
        for (size_t i = 1; i < g.mission().mechs().size(); ++i)
            movedAfter = std::max(movedAfter, length(g.mission().mechs()[i].position() -
                                                     start[i - 1]));
        std::printf("        after deploying, hostiles moved %.1f m\n",
                    static_cast<double>(movedAfter));
    }

    // ----------------------------------------------------------- world fit --
    // Walk machines all over every arena, including straight at the props, and
    // check none of them ends up under the ground or falling forever. The
    // terrain is a height field, so "below the surface" is exact - and once a
    // machine is down there the foothold search casts from beneath the ground
    // into empty space and it never recovers.
    {
        int belowGround = 0, stuckAirborne = 0, checked = 0;
        float deepest = 0.0f;
        std::string worstArena;
        for (int a = 0; a < static_cast<int>(arenaCatalog().size()); ++a) {
            const ArenaDef& def = arenaByIndex(a);
            World w;
            w.generate(def, 4242u + static_cast<uint32_t>(a) * 31u);
            Rng rng(77u + static_cast<uint32_t>(a));

            for (int run = 0; run < 5; ++run) {
                Loadout l = newProfile().loadout;
                Mech m;
                const Vec3 spawn = w.findSpawnPoint(rng, Vec3(0.0f, 0.0f, 0.0f), 20.0f, 90.0f);
                m.init(w, l, spawn, rng.range(0.0f, TAU), Team::Player, 3u + run);
                std::vector<ShotRequest> shots;

                // Drive in a slowly turning arc so the machine ploughs through
                // whatever the arena scattered around, props included.
                float heading = rng.range(0.0f, TAU);
                float airFor = 0.0f;
                for (int i = 0; i < 60 * 20; ++i) {
                    heading += dt * 0.55f;
                    MechInput in;
                    in.moveWorld = Vec3(std::cos(heading), 0.0f, std::sin(heading));
                    in.throttle = 1.0f;
                    in.sprint = true;
                    in.aimPoint = m.position() + m.forward() * 30.0f;
                    m.update(dt, w, in, shots);

                    const Vec3 p = m.position();
                    const float ground = w.terrain().height(p.x, p.z);
                    if (p.y < ground - 0.5f) {
                        ++belowGround;
                        deepest = std::max(deepest, ground - p.y);
                        worstArena = def.name;
                        break;
                    }
                    airFor = (m.state() == MechState::Airborne) ? airFor + dt : 0.0f;
                    if (airFor > 4.0f) { ++stuckAirborne; worstArena = def.name; break; }
                }
                ++checked;
            }
        }
        std::printf("\nWORLD   %d runs across %d arenas: %d ended below ground, "
                    "%d fell without landing%s\n",
                    checked, static_cast<int>(arenaCatalog().size()),
                    belowGround, stuckAirborne,
                    (belowGround || stuckAirborne)
                        ? ("  <-- worst in " + worstArena).c_str() : "");
        if (deepest > 0.0f)
            std::printf("        deepest breach %.1f m below the surface\n",
                        static_cast<double>(deepest));
    }

    // ------------------------------------------------------------------ AI --
    // Three things that are invisible in a screenshot: do machines get boxed
    // into buildings, can they shoot something above them, and do a group
    // spread out or stack on one bearing.
    {
        // Spawn placement. Every arena, many draws, counting how many land
        // inside a building footprint - a hollow ruin passes insideSolid(), so
        // this used to wall machines in with no way out.
        int boxedIn = 0, total = 0;
        for (int a = 0; a < static_cast<int>(arenaCatalog().size()); ++a) {
            World w;
            w.generate(arenaByIndex(a), 90001u + static_cast<uint32_t>(a) * 17u);
            Rng rng(1234u + static_cast<uint32_t>(a));
            const Vec3 from = w.findSpawnPoint(rng, Vec3(0.0f, 0.0f, 0.0f), 0.0f, 20.0f);
            for (int i = 0; i < 60; ++i) {
                const Vec3 p = w.findSpawnPoint(rng, from, 40.0f, 80.0f);
                if (w.insideStructure(p, 0.0f)) ++boxedIn;
                ++total;
            }
        }
        std::printf("\nSPAWN   %d of %d spawn points landed inside a building "
                    "footprint\n", boxedIn, total);

        // Turret elevation: how steep an angle can a machine actually engage?
        {
            World w;
            w.generate(*arenaById("ruined_district"), 90001u);
            Loadout l = newProfile().loadout;
            Mech m;
            m.init(w, l, Vec3(0.0f, 0.0f, 0.0f), 0.0f, Team::Hostile, 5u);
            std::vector<ShotRequest> shots;
            float bestReached = 0.0f;
            for (float wantDeg = 10.0f; wantDeg <= 85.0f; wantDeg += 5.0f) {
                const float r = deg2rad(wantDeg);
                const Vec3 aim = m.position() +
                                 Vec3(std::cos(r), std::sin(r), 0.0f) * 40.0f;
                for (int i = 0; i < 120; ++i) {
                    MechInput in;
                    in.aimPoint = aim;
                    m.update(dt, w, in, shots);
                }
                const float got = std::asin(clampf(m.aimDirection().y, -1.0f, 1.0f));
                if (verbose)
                    std::printf("          want %4.0f deg -> got %4.0f deg\n",
                                static_cast<double>(wantDeg),
                                static_cast<double>(got * 180.0f / PI));
                if (std::fabs(got - r) < deg2rad(6.0f)) bestReached = wantDeg;
            }
            std::printf("        turret reaches %.0f deg of elevation "
                        "(a climber above you is unshootable below this)\n",
                        static_cast<double>(bestReached));
        }

        // Bearing spread: put four hostiles on one player and see whether they
        // surround them or stack up.
        {
            PlayerProfile prof = newProfile();
            LevelDef lvl = campaignLevels()[5];        // CORE DISTRICT, five hostiles
            Mission mission;
            mission.begin(lvl, prof);
            mission.beginCombat();
            for (int i = 0; i < 60 * 25; ++i) {
                MechInput idle;
                idle.aimPoint = mission.player().position() +
                                mission.player().forward() * 40.0f;
                mission.update(dt, idle);
            }
            std::vector<float> angles;
            const Vec3 p = mission.player().position();
            for (size_t i = 1; i < mission.mechs().size(); ++i) {
                if (!mission.mechs()[i].alive()) continue;
                const Vec3 d = flattenY(mission.mechs()[i].position() - p);
                if (lengthSq(d) < 1.0f) continue;
                angles.push_back(std::atan2(d.z, d.x));
            }
            // Mean resultant length: 1 means all on one bearing, 0 means evenly
            // spread all the way round.
            float sx = 0.0f, sy = 0.0f;
            for (float a : angles) { sx += std::cos(a); sy += std::sin(a); }
            const float clump = angles.empty() ? 0.0f
                : std::sqrt(sx * sx + sy * sy) / static_cast<float>(angles.size());
            std::printf("        %d hostiles engaging, clumping %.2f "
                        "(1 = all on one side, 0 = surrounded)\n",
                        static_cast<int>(angles.size()), static_cast<double>(clump));
        }
    }

    // ------------------------------------------------------------- climbing --
    // Drive a machine straight into a building face and see how far up it gets.
    // Run it for each leg type, because "climbing does not work" and "these
    // legs cannot grip a wall" look identical from the cockpit.
    {
        World world;
        world.generate(*arenaById("ruined_district"), 90001u);

        // Find a tall climbable face to drive at.
        const Obstacle* wall = nullptr;
        for (const Obstacle& o : world.obstacles()) {
            if (!o.climbable) continue;
            if (o.half.y * 2.0f < 8.0f) continue;
            if (std::fabs(o.center.x) > 90.0f || std::fabs(o.center.z) > 90.0f) continue;
            wall = &o;
            break;
        }
        if (!wall) {
            std::printf("\nCLIMB   no climbable structure in this arena\n");
        } else {
            const char* legIds[] = {"lg_strider", "lg_scout", "lg_harrier",
                                    "lg_grasshopper", "lg_anvil", "lg_titan"};
            std::printf("\nCLIMB   driving into a %.0f m face\n", wall->half.y * 2.0f);
            for (const char* legId : legIds) {
                Loadout l = newProfile().loadout;
                l.legs = legId;
                const MechStats st = deriveStats(l);

                // Stand off the face on the +X side and drive at it.
                const float outX = wall->half.x + 6.0f;
                const Vec3 spawn(wall->center.x + outX, 0.0f, wall->center.z);
                Mech m;
                m.init(world, l, spawn, -PI * 0.5f, Team::Player, 7u);
                m.setIndex(0);
                const float baseY = m.position().y;

                std::vector<ShotRequest> shots;
                float peak = baseY;
                float flattestUp = 1.0f;
                float worstGrip = 1.0f;
                int slips = 0;
                bool wasPlanted = true;
                float lateral = 0.0f;
                bool fellOff = false;
                bool traversing = false;
                Vec3 lastPos = m.position();

                // Phase 1: drive at the wall and climb. Phase 2: once on it,
                // traverse sideways, which walks the machine into the corner of
                // the building - the hardest thing a climber does, because for
                // a moment the limbs span two planes.
                for (int i = 0; i < 60 * 24; ++i) {
                    // Start traversing once the machine is genuinely up the
                    // face rather than at a fixed time: a fast climber was
                    // over the parapet before the clock said to turn, and the
                    // corner - the part worth testing - never got tested.
                    if (!traversing && m.onWall() &&
                        m.position().y > baseY + 7.0f) traversing = true;
                    const bool traverse = traversing && m.onWall();
                    const Vec3 up = m.up();
                    Vec3 drive;
                    if (traverse) {
                        // Horizontal, in the plane of the wall: straight along
                        // the face toward its edge.
                        drive = cross(up, Vec3(0.0f, 1.0f, 0.0f));
                        if (lengthSq(drive) < 1e-4f) drive = Vec3(0.0f, 0.0f, 1.0f);
                    } else {
                        // What the pilot actually does: hold forward at the
                        // building. No pre-projection here - the machine is
                        // what decides that driving into a face you are
                        // holding means climbing it, and this harness exists
                        // to test that it does.
                        drive = Vec3(-1.0f, 0.0f, 0.0f);
                    }
                    MechInput in;
                    in.moveWorld = (lengthSq(drive) > 1e-5f) ? normalize(drive)
                                                             : Vec3(-1.0f, 0.0f, 0.0f);
                    in.throttle = 1.0f;
                    in.aimPoint = m.position() + m.forward() * 30.0f;
                    m.update(dt, world, in, shots);

                    peak = std::max(peak, m.position().y);
                    flattestUp = std::min(flattestUp, m.up().y);
                    if (m.onWall()) worstGrip = std::min(worstGrip, m.gripQuality());
                    if (verbose && std::strcmp(legId, "lg_harrier") == 0 && i % 60 == 0) {
                        int pl = 0;
                        for (const Leg& lg : m.legs()) if (lg.planted) ++pl;
                        std::printf("            t=%4.1f y=%+6.2f up.y=%+.2f %-9s "
                                    "planted %d/6 spd %.1f grip %.2f wall %d\n",
                                    static_cast<double>(i * dt),
                                    static_cast<double>(m.position().y - baseY),
                                    static_cast<double>(m.up().y),
                                    mechStateName(m.state()), pl,
                                    static_cast<double>(m.speed()),
                                    static_cast<double>(m.gripQuality()),
                                    m.onWall() ? 1 : 0);
                    }
                    if (traverse) lateral += lengthXZ(m.position() - lastPos);
                    lastPos = m.position();

                    int planted = 0;
                    for (const Leg& lg : m.legs()) if (lg.planted) ++planted;
                    if (wasPlanted && planted <= 1) ++slips;
                    wasPlanted = planted > 1;
                    // Coming off ends the attempt. Left running, a machine that
                    // fell in the first second kept accumulating ground
                    // mileage and scored better than one that stayed up, which
                    // is the opposite of what this measures.
                    if (traversing && m.position().y < baseY + 1.5f) {
                        fellOff = true;
                        break;
                    }
                }
                (void)flattestUp;

                std::printf("        %-15s grip %.2f limit %3.0f  %-3s  "
                            "rose %5.1f m | grip %.2f, %d slips | "
                            "traverse %5.1f m %s\n",
                            legId,
                            static_cast<double>(PartCatalog::instance().find(legId)->stats.climbGrip),
                            static_cast<double>(st.climbAngle * 180.0f / PI),
                            st.canClimbWalls ? "yes" : "no",
                            static_cast<double>(peak - baseY),
                            static_cast<double>(worstGrip), slips,
                            static_cast<double>(lateral),
                            fellOff ? "FELL" : "");
            }
        }
    }

    // ------------------------------------------------------------- top-out --
    // The moment the user called out by name: cresting a wall. The front legs
    // must wrap over the parapet onto the roof and pull the hull up - not hang
    // in the air until the rear legs arrive and the whole tank flips. On a
    // clean 10 m block (a controlled shape, after this test once measured the
    // machine against the wrong building in a dense arena), a specialist must
    // end standing ON the roof, upright, without being flung airborne off it.
    {
        World world;
        world.generateTestRange(120.0f);
        const float topY = 10.0f;
        world.addTestBox(Vec3(0.0f, topY * 0.5f, 0.0f), Vec3(9.0f, topY * 0.5f, 9.0f));

        Loadout l = newProfile().loadout;
        l.legs = "lg_harrier";
        Mech m;
        m.init(world, l, Vec3(17.0f, 0.0f, 0.0f), -PI * 0.5f, Team::Player, 9u);
        m.setIndex(0);
        std::vector<ShotRequest> shots;
        bool wentAirborne = false;
        float crestedAt = -1.0f;
        float worstUpAfterCrest = 1.0f;
        float settleTime = -1.0f;
        for (int i = 0; i < 60 * 25; ++i) {
            MechInput in;
            // Drive at the wall until the machine is up; then stand down and
            // watch it settle. Driving on just walks it off the far edge,
            // which is pilot error, not a climbing bug.
            // Ground pace is now around twelve metres a second, so a full
            // second of throttle after the crest carries the machine clean
            // across an eighteen-metre block and off the far parapet - which
            // this gate would then report as a climbing failure. A third of a
            // second is enough to get the whole machine onto the deck.
            const bool doneDriving = crestedAt >= 0.0f && i * dt > crestedAt + 0.35f;
            if (!doneDriving) {
                in.moveWorld = Vec3(-1.0f, 0.0f, 0.0f);
                in.throttle = 1.0f;
            }
            in.aimPoint = m.position() + m.forward() * 30.0f;
            m.update(dt, world, in, shots);
            if (verbose && i % 30 == 0) {
                int pl = 0, roof = 0;
                for (const Leg& lg : m.legs()) {
                    if (lg.planted) ++pl;
                    if (lg.planted && lg.footNormal.y > 0.6f &&
                        lg.foot.y > m.position().y - 0.8f) ++roof;
                }
                std::printf("            t=%4.1f y=%+6.2f up.y=%+.2f %-9s "
                            "planted %d/6 roofFeet %d mantle %.2f wall %d\n",
                            static_cast<double>(i * dt),
                            static_cast<double>(m.position().y),
                            static_cast<double>(m.up().y),
                            mechStateName(m.state()), pl, roof,
                            static_cast<double>(m.mantleFraction()),
                            m.onWall() ? 1 : 0);
            }
            const bool onRoof = m.position().y > topY - 0.5f &&
                                std::fabs(m.position().x) < 9.5f &&
                                std::fabs(m.position().z) < 9.5f;
            if (crestedAt < 0.0f && onRoof && m.up().y > 0.80f)
                crestedAt = i * dt;
            if (crestedAt >= 0.0f) {
                worstUpAfterCrest = std::min(worstUpAfterCrest, m.up().y);
                if (m.state() == MechState::Airborne) wentAirborne = true;
                if (settleTime < 0.0f && m.up().y > 0.97f &&
                    m.state() == MechState::Grounded)
                    settleTime = i * dt - crestedAt;
            }
        }
        std::printf("\nTOPOUT  10 m block: %s",
                    crestedAt >= 0.0f ? "crested" : "NEVER CRESTED");
        if (crestedAt >= 0.0f)
            std::printf(" at t=%.1fs, settled upright %.1fs later, "
                        "worst up.y after crest %.2f%s",
                        static_cast<double>(crestedAt),
                        static_cast<double>(settleTime < 0.0f ? -1.0f : settleTime),
                        static_cast<double>(worstUpAfterCrest),
                        wentAirborne ? ", WENT AIRBORNE off the top" : "");
        std::printf("\n");
    }

    // ------------------------------------------------------------ workshop --
    // Drive the game to the workshop and shop the way a person would, with the
    // real keys. Reports what is actually on offer in every slot and whether a
    // purchase sticks - the questions "does the store work" and "are there any
    // alternative parts" both reduce to this.
    {
        Game g;
        g.init(4242u, 160, 48, 1);
        g.setCellAspect(0.5f);

        // The store is the subject here, not the pilot: skip straight to a
        // cleared mission and take the result screen from there with real
        // keys. Playing the whole objective chain with scripted input belongs
        // to spiderbot_sim.
        deploy(g);
        for (int i = 0; i < 30; ++i) { InputState idle; g.update(dt, idle); }
        g.debugFinishMission();
        if (g.screen() != GameScreen::MissionResult) {
            std::printf("\nSTORE   never reached the result screen\n");
            return 1;
        }
        tap(g, Key::Enter, 10);
        if (g.screen() != GameScreen::Store) {
            std::printf("\nSTORE   Enter on the result screen did not open the workshop\n");
            return 1;
        }
        std::printf("\nSTORE   opened with %d cr\n", g.profile().cash);

        // Walk every slot and count what is on offer. Entering a slot is Enter,
        // leaving it is Backspace.
        const char* names[] = {"WEAPON", "CHASSIS", "LEGS", "ENGINE", "ARMOUR", "SENSOR"};
        for (int slot = 0; slot < 6; ++slot) {
            tap(g, Key::Enter);                       // into the parts pane
            if (g.screen() != GameScreen::Store) {
                std::printf("STORE   FAILED: Enter closed the workshop "
                            "(it should only open the parts list)\n");
                return 1;
            }
            const Store& st = g.store();
            const int n = static_cast<int>(st.listing().size());
            int affordable = 0;
            for (const PartDef* p : st.listing())
                if (p->price <= g.profile().cash) ++affordable;
            std::printf("        %-8s %2d parts on offer, %d affordable",
                        names[slot], n, affordable);
            if (n > 0) {
                std::printf("  e.g. %s", st.listing()[0]->name.c_str());
                if (n > 1) std::printf(" / %s", st.listing()[n > 2 ? 2 : 1]->name.c_str());
            }
            std::printf("\n");
            tap(g, Key::Backspace);                   // back to the slot list
            tap(g, Key::Down);                        // next slot
        }

        // Now actually buy something: find the priciest affordable part in the
        // chassis slot and fit it.
        while (g.store().slot() != Slot::Chassis) tap(g, Key::Down);
        tap(g, Key::Enter);
        const std::string before = g.profile().loadout.chassis;
        const int cashBefore = g.profile().cash;
        int best = -1, bestPrice = -1;
        for (int i = 0; i < static_cast<int>(g.store().listing().size()); ++i) {
            const PartDef* p = g.store().listing()[static_cast<size_t>(i)];
            if (p->price > bestPrice && p->price <= cashBefore) { bestPrice = p->price; best = i; }
        }
        if (best >= 0) {
            while (g.store().cursor() != best) tap(g, Key::Down);
            const std::string want = g.store().selected()->id;
            tap(g, Key::Enter);
            const bool fitted = g.profile().loadout.chassis == want;
            std::printf("        buy %-18s %s (cash %d -> %d, chassis %s -> %s)\n",
                        want.c_str(), fitted ? "FITTED" : "DID NOT STICK",
                        cashBefore, g.profile().cash,
                        before.c_str(), g.profile().loadout.chassis.c_str());
        } else {
            std::printf("        nothing affordable in the chassis slot\n");
        }

        // Dump the screen as text so the layout can be eyeballed without a
        // display. The workshop is the most layout-sensitive screen there is.
        if (argFlag(argc, argv, "--screens")) {
            int cols = 0, rows = 0;
            Game::hudGridFor(1920 / 16, 1080 / 32, &cols, &rows);
            AsciiFrame hud;
            hud.resize(cols, rows);
            g.drawHudOnly(hud);
            std::printf("\n----- WORKSHOP %dx%d -----\n%s\n",
                        cols, rows, frameToText(hud).c_str());
        }

        // Tab must swap the preview, not exit.
        const bool cmp = g.store().comparing();
        tap(g, Key::Tab);
        std::printf("        Tab %s the with/without preview\n",
                    g.store().comparing() != cmp ? "toggles" : "DID NOT TOGGLE");

        // Only Space leaves.
        tap(g, Key::Space, 6);
        std::printf("        Space %s the workshop\n",
                    g.screen() != GameScreen::Store ? "deploys out of" : "DID NOT LEAVE");
    }

    return 0;
}
