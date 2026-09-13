#include "game.h"

#include "combat.h"

#include "led.h"

#include <cctype>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sb {

namespace {

// Camera limits. Pitch is how far the camera sits above the machine, in
// radians: positive looks down at it, negative looks up from below. It stops
// short of straight up and down because the follow camera rolls with the mech,
// and a pole crossing while rolled produces a sickening spin.
constexpr float kPitchMin = -0.55f;
constexpr float kPitchMax = 1.15f;
constexpr float kPitchStart = 0.26f;
constexpr float kCamDistMin = 7.0f;
constexpr float kCamDistMax = 26.0f;

std::string fmt(float v, int digits) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", digits, static_cast<double>(v));
    return buf;
}

std::string fmtInt(int v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", v);
    return buf;
}

// Thousands separators, because "12400cr" is harder to read at a glance than
// "12,400cr" and money is a number the player checks constantly.
std::string money(int v) {
    std::string s = fmtInt(v < 0 ? -v : v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(static_cast<size_t>(i), ",");
    return (v < 0 ? "-" : "") + s;
}

const TextStyle kDim{Vec3(0.30f, 0.55f, 0.36f), true};
const TextStyle kNorm{Vec3(0.55f, 1.00f, 0.62f), true};
const TextStyle kBright{Vec3(0.80f, 1.00f, 0.85f), true};
const TextStyle kWarn{Vec3(1.00f, 0.72f, 0.25f), true};
const TextStyle kBad{Vec3(1.00f, 0.35f, 0.28f), true};
const TextStyle kGood{Vec3(0.45f, 1.00f, 0.55f), true};

} // namespace

// ---------------------------------------------------------------- lifecycle

void Game::init(uint32_t seed, int cellsX, int cellsY, int supersample) {
    seed_ = seed ? seed : 1u;
    raster_.resize(cellsX, cellsY, supersample);
    profile_ = newProfile();
    startMission();
}

void Game::resize(int cellsX, int cellsY, int supersample) {
    raster_.resize(cellsX, cellsY, supersample);
}

void Game::startMission() {
    const std::vector<LevelDef>& levels = campaignLevels();
    level_ = (profile_.level < static_cast<int>(levels.size()))
                 ? levels[static_cast<size_t>(profile_.level)]
                 : endlessLevel(profile_.level);
    // Mixing the run seed in means two players on the same mission get the
    // same layout only if they started the same run. The DEATH COUNT is
    // deliberately NOT mixed in any more: dying now costs the whole mission,
    // so a retry has to be the same ground and the same enemies. That is
    // what makes a loss a lesson - you already know where the gun line is,
    // and this time you come at it differently.
    level_.seed ^= seed_ * 2654435761u;

    mission_.begin(level_, profile_);
    render_ = mission_.world().arena().render;
    baseRender_ = render_;
    cabinLayout_ = cabinLayoutFor(profile_.loadout);
    screen_ = GameScreen::Briefing;
    screenTimer_ = 0.0f;

    const Mech& p = mission_.player();
    camFocus_ = p.position() + Vec3(0.0f, 1.4f, 0.0f);
    // World up, not the hull's: a machine spawned on a slope would otherwise
    // hand the camera a tilted frame that it then slewed level, and the
    // transport through that slew is a few degrees of yaw for free.
    camUp_ = Vec3(0.0f, 1.0f, 0.0f);
    // Start the orbit frame looking along the machine's heading. From here on
    // the reference is carried forward independently, so camYaw_ stays a plain
    // offset from it rather than an absolute world angle.
    camRefFwd_ = normalize(flattenY(p.forward()) + Vec3(0.0f, 0.0f, 1e-4f));
    camOrbitDir_ = camRefFwd_;
    camYaw_ = 0.0f;
    camPitch_ = kPitchStart;
    camPos_ = camFocus_ - camRefFwd_ * camDist_ +
              Vec3(0.0f, std::sin(kPitchStart) * camDist_, 0.0f);
    aimPoint_ = camFocus_ + camRefFwd_ * 60.0f;
    status_ = level_.name;
}

void Game::loadSave() {
    // The save is the whole run: machine, money, mission. Its absence is a
    // fresh campaign, never an error. The platform calls this once after
    // init; the test harnesses deliberately never do, so a save on disk can
    // never change what a regression measures.
    if (loadProfile(profile_, "acah_save.txt")) startMission();
}

void Game::debugFinishMission() {
    mission_.debugComplete();
    screen_ = GameScreen::MissionResult;
    screenTimer_ = 0.0f;
}

// -------------------------------------------------------------------- input

void Game::applyDisplayToggles(const InputState& in) {
    auto cycle = [](int v, int n) { return (v + 1) % n; };
    if (in.cyclePalette)
        ascii_.palette = static_cast<Palette>(cycle(static_cast<int>(ascii_.palette),
                                                    static_cast<int>(Palette::Count)));
    if (in.cycleRamp)
        ascii_.ramp = static_cast<Ramp>(cycle(static_cast<int>(ascii_.ramp),
                                              static_cast<int>(Ramp::Count)));
    if (in.cycleBackground)
        ascii_.background = static_cast<Background>(cycle(static_cast<int>(ascii_.background),
                                                          static_cast<int>(Background::Count)));
    if (in.toggleAscii) asciiEnabled_ = !asciiEnabled_;
    // The sight cycles off -> x2.5 -> x7 -> off and hands you back the view
    // you were in when you raised it: raising the glass from the cabin and
    // lowering it used to leave you outside the machine.
    if (in.cycleScope) {
        if (scopeStage_ == 0) fpvBeforeScope_ = fpv_;
        scopeStage_ = (scopeStage_ + 1) % 3;
        fpv_ = (scopeStage_ == 0) ? fpvBeforeScope_ : false;
    }
    if (in.toggleFpv) { fpv_ = !fpv_; if (fpv_) scopeStage_ = 0; }
    if (in.toggleHud) hudVisible_ = !hudVisible_;
    if (in.toggleDither) ascii_.dither = !ascii_.dither;
    if (in.zoomIn) camDistTarget_ = clampf(camDistTarget_ - 1.5f, kCamDistMin, kCamDistMax);
    if (in.zoomOut) camDistTarget_ = clampf(camDistTarget_ + 1.5f, kCamDistMin, kCamDistMax);
}

MechInput Game::buildPlayerInput(const InputState& in) const {
    MechInput mi;

    // Movement is relative to the camera, projected onto the surface the mech
    // is standing on. That projection is what makes "forward" mean the same
    // thing on flat ground and halfway up a wall.
    const Mech& p = mission_.player();
    // On the ground the drive frame is the WORLD's horizontal, not the
    // hull's tangent plane. Building "right" as cross(camera forward
    // projected onto the hull plane, hull up) is exact on a wall and subtly
    // wrong everywhere else: the hull pitches a few degrees with every
    // slope and with its own motion, and a strafe built in that plane picks
    // up a component along the camera axis whose sign follows the pitch.
    // Measured: 0.04-0.09 of the drive vector pointed downhill in BOTH
    // strafe directions on a hillside, which is 0.5-1.0 m/s of travel the
    // pilot never asked for - the machine sidled toward the camera the whole
    // time it was dodging. Same blend the camera uses: level frame on the
    // ground, surface frame once the machine is genuinely on a face.
    const Vec3 worldUp(0.0f, 1.0f, 0.0f);
    const float onWall = clampf((p.climbFraction() - 0.10f) / 0.20f, 0.0f, 1.0f);
    Vec3 up = lerp(worldUp, p.up(), onWall);
    up = (lengthSq(up) > 1e-6f) ? normalize(up) : worldUp;

    // The ORBIT direction, not the line from the boom to the focus. The boom
    // trails the machine at a finite rate, so while strafing at 12 m/s the
    // line from camera to focus swings a few degrees toward the direction
    // of travel and back - and a strafe built from that line picks up a
    // component along the view that changes sign with the swing. Over a
    // fourteen-second dodge that summed to metres of sidle. The orbit
    // direction is what the mouse set and does not care where the boom is.
    Vec3 camFwd = camOrbitDir_;
    Vec3 fwd = camFwd - up * dot(camFwd, up);
    if (lengthSq(fwd) < 0.12f) {
        // The camera is looking nearly straight along the surface normal. That
        // is not a rare singularity - it is precisely the moment the machine
        // commits to a wall it was driving at: the pilot is still looking at
        // the building, and the building's normal has just become "up". The
        // projection then leaves almost nothing, and W used to mean whatever
        // sideways scrap survived, so the machine crabbed along the face
        // instead of going up it. Looking into the surface means up it;
        // looking out of it means back down.
        const Vec3 worldUp(0.0f, 1.0f, 0.0f);
        Vec3 uphill = worldUp - up * dot(worldUp, up);
        if (lengthSq(uphill) > 1e-5f) {
            fwd = normalize(uphill) * ((dot(camFwd, up) < 0.0f) ? 1.0f : -1.0f);
        } else {
            // Level ground with the camera straight overhead: the machine's
            // own facing is the only meaning left for "forward".
            fwd = p.forward() - up * dot(p.forward(), up);
        }
    }
    fwd = normalize(fwd);
    // cross(forward, up), matching the renderer's basis. The other order is
    // the left vector, which had D strafing left and A strafing right.
    const Vec3 rightV = normalize(cross(fwd, up));

    Vec3 move(0.0f);
    if (in.forward) move += fwd;
    if (in.back) move -= fwd;
    if (in.right) move += rightV;
    if (in.left) move -= rightV;
    // A stick adds on top of the keys. Its deflection is the throttle: the
    // keys are always full, a stick can walk.
    float stickMag = 0.0f;
    if (std::fabs(in.moveX) > 1e-3f || std::fabs(in.moveY) > 1e-3f) {
        const Vec3 stick = fwd * in.moveY + rightV * in.moveX;
        stickMag = clampf(length(stick), 0.0f, 1.0f);
        move += stick;
    }

    const float mag = length(move);
    if (mag > 1e-4f) {
        mi.moveWorld = move / mag;
        // Full throttle whenever a key is down. Walking at 0.72 meant the
        // machine never reached the speed its legs are rated for, which read
        // as "it barely moves".
        const bool anyKey = in.forward || in.back || in.left || in.right;
        mi.throttle = anyKey ? 1.0f : stickMag;
        // Scoped in: the gunner is on the glass, the driver is easing the
        // machine, not sprinting it. Magnified view plus full speed is soup.
        if (scopeStage_ > 0) mi.throttle = std::min(mi.throttle, 0.35f);
    }
    mi.sprint = in.boost;
    // Looking DOWN and driving off a lip climbs down the face instead of
    // walking off it. In the cabin a modest downward gaze is enough (the
    // pilot is looking at the edge); outside, the orbit camera already sits
    // above the machine, so it takes a deliberate steeper tilt.
    mi.wantDescend = camPitch_ > (fpv_ ? 0.32f : 0.50f);
    mi.aimPoint = aimPoint_;
    if (leadValid_) {
        mi.assistPoint = leadPoint_;
        mi.assistStrength = assistStrength_;
    }
    mi.fireHeld = in.fireHeld;
    mi.jumpHeld = in.jumpHeld;
    for (int i = 0; i < 4; ++i) mi.ability[i] = in.ability[i];
    mi.fire2Held = in.fire2Held;
    mi.toggleMask = in.toggleMask;
    return mi;
}

void Game::updateAimAssist() {
    leadValid_ = false;
    assistStrength_ = 0.0f;
    const Mech& p = mission_.player();
    if (!p.alive()) return;

    const PartDef* sensor = p.loadout().part(Slot::Sensor);
    const float range = p.stats().sensorRange;
    const int tier = sensor ? sensor->tier : 0;

    // The slowest round among the guns currently enabled decides the lead:
    // if that one arrives, the faster ones do too.
    float speed = 1e9f;
    bool anyGun = false;
    for (const MountedWeapon& mw : p.weapons()) {
        if (!mw.part || !mw.enabled) continue;
        anyGun = true;
        speed = std::min(speed, mw.part->weapon.projectileSpeed);
    }
    if (!anyGun || speed > 1e8f) return;

    // The target: whatever hostile sits nearest the aim ray, inside the
    // sensor's reach and a narrow acquisition cone. Machines first - they
    // are what long-range fire is for - then vehicles.
    const Vec3 eye = camPos_;
    const Vec3 ray = normalize(camFocus_ - camPos_);
    // From outside the machine the reticle sits on a ray that starts metres
    // behind and above the guns, so what looks lined up often is not, and
    // the target is a smaller thing on a wider screen. The acquisition cone
    // is nearly twice as wide out there (9 degrees against 5), and the pull
    // is stronger; in the cabin and on the glass the pilot is expected to
    // do the work.
    const bool outside = !fpv_ && scopeStage_ == 0;
    // 7 degrees outside (it was 9 and read as too strong), 5 inside.
    float bestScore = outside ? 0.9925f : 0.9962f;   // cos 7 deg / cos 5 deg
    Vec3 tPos, tVel;
    bool have = false;
    for (size_t i = 1; i < mission_.mechs().size(); ++i) {
        const Mech& m = mission_.mechs()[i];
        if (!m.alive() || m.team() != Team::Hostile) continue;
        const Vec3 to = m.hitCentre() - eye;
        const float d = length(to);
        if (d < 4.0f || d > range) continue;
        const float c = dot(to / d, ray);
        if (c > bestScore) { bestScore = c; tPos = m.hitCentre(); tVel = m.velocity(); have = true; }
    }
    for (const Unit& u : mission_.units()) {
        if (!u.alive() || u.team() != Team::Hostile) continue;
        const Vec3 to = u.hitCentre() - eye;
        const float d = length(to);
        if (d < 4.0f || d > range) continue;
        const float c = dot(to / d, ray);
        if (c > bestScore) { bestScore = c; tPos = u.hitCentre(); tVel = u.velocity(); have = true; }
    }
    if (!have) return;

    Vec3 aim;
    if (!leadTarget(p.hitCentre(), tPos, tVel, speed, &aim)) aim = tPos;
    leadPoint_ = aim;
    leadValid_ = true;

    // How hard the fire control pulls. Every sensor gives some; tiers give
    // more; a hard TargetLock is a firing solution, not a suggestion; the
    // Rangefinder's whole identity is this number.
    float s = 0.30f + 0.14f * static_cast<float>(tier);
    // Long optics reach: the fire control follows the sight.
    if (p.stats().trait == Trait::Spotter) s += 0.15f;
    const int rf = static_cast<int>(Ability::Rangefinder);
    if (p.stats().hasPassive[rf]) s += 0.20f * p.stats().passivePower[rf];
    if (outside) {
        s += 0.11f;
        // Shot tracking. The closer the reticle already is to the target,
        // the harder the fire control finishes the job: inside a degree the
        // round goes most of the way to the solution, at three degrees you
        // are on your own. Halved from its first version (0.85 of the way
        // to 0.92 inside 1.5 deg) - that read as an aimbot.
        const float err = std::acos(clampf(bestScore, -1.0f, 1.0f));
        const float snap = clampf(1.0f - (err - deg2rad(1.0f)) / deg2rad(2.0f), 0.0f, 1.0f);
        s = s + (0.80f - s) * snap * 0.45f;
    }
    if (p.locked()) s = 0.95f;
    // A Jammer eats the fire control. Not all of it - the gun still points
    // where you point it - but the solution the sensors were handing you goes
    // away, and it comes back the moment the jammer does. This is the whole
    // reason the thing is on the battlefield.
    const float jam = mission_.jamStrength();
    if (jam > 0.01f) s *= (1.0f - 0.85f * jam);
    assistStrength_ = clampf(s, 0.0f, 0.95f);
}

Vec3 Game::traceAimPoint() const {
    // The reticle sits where the camera ray meets the world. Aiming from the
    // camera rather than from the guns is what makes the crosshair mean
    // something; the mech's own weapons then converge on that point.
    const Vec3 dir = normalize(camFocus_ - camPos_);
    const SurfaceHit h = mission_.world().raycast(camPos_, dir, 400.0f);
    aimHitSomething_ = h.hit;
    if (h.hit) return h.point;
    return camPos_ + dir * 300.0f;
}

// -------------------------------------------------------------------- audio

void Game::emitAudio(float dt, const MechInput& mi) {
    const Mech& p = mission_.player();
    // Haptics decay like the shake does; the platform reads them each frame.
    rumbleHit_ *= std::exp(-9.0f * dt);
    rumbleGun_ *= std::exp(-14.0f * dt);

    // Panning and attenuation are done here rather than in the synthesiser so
    // the platform layer stays a dumb instrument: it is handed gain, pitch and
    // a stereo position, and knows nothing about the world.
    const Vec3 camRight = normalize(cross(normalize(camFocus_ - camPos_), camUp_));
    auto place = [&](const Vec3& at, float range) {
        struct Placed { float gain, pan; };
        const Vec3 rel = at - camPos_;
        const float d = length(rel);
        const float gain = clampf(1.0f - d / range, 0.0f, 1.0f);
        const float pan = (d > 0.1f) ? clampf(dot(rel / d, camRight), -1.0f, 1.0f) : 0.0f;
        return Placed{gain * gain, pan};   // squared: distance should bite
    };

    // ---- every gun that spoke this frame --------------------------------
    // The combat layer reports each shot with its position and weapon, player
    // and enemy alike, so the soundscape is the actual battle: your own report
    // up close, theirs cracking off at distance, all of it positioned. A
    // per-frame budget keeps a massed volley from eating every voice.
    const CombatEvents& cev = mission_.lastCombatEvents();
    {
        int budget = 7;
        for (const FireEvent& f : cev.fired) {
            if (!f.weapon || budget <= 0) break;
            const bool own = (f.shooter == 0);
            const auto pl = place(f.pos, own ? 400.0f : 300.0f);
            if (!own && pl.gain < 0.06f) continue;
            --budget;
            const WeaponDef& w = *f.weapon;
            const bool beam = w.projectileSpeed > 700.0f && w.fireInterval < 0.1f;
            // A chaingun is its rate of fire: each round is a short metallic
            // crack, and twelve of them a second IS the buzz.
            const bool chain = !beam && w.fireInterval <= 0.12f;
            const Sfx kind = beam ? Sfx::FireBeam
                           : chain ? Sfx::FireChain
                           : (w.damage >= 55.0f || w.blastRadius >= 3.5f) ? Sfx::FireHeavy
                           : (w.damage >= 20.0f) ? Sfx::FireMedium
                                                 : Sfx::FireLight;
            const float jitter = 0.94f + 0.12f * std::sin(elapsed_ * 47.0f + f.pos.x);
            audio_.push(kind, own ? 0.9f : pl.gain * 0.8f, jitter, own ? 0.0f : pl.pan);
            if (own && w.damage >= 30.0f)
                shake_ = std::min(1.0f, shake_ + 0.10f + w.damage * 0.003f);
            if (own) rumbleGun_ = std::min(1.0f, rumbleGun_ + (chain ? 0.10f : 0.04f + w.damage * 0.006f));

            // The rocket warning. A hostile warhead in the air is the single
            // most dangerous thing in this game and, until now, it announced
            // itself with the same distant crack as a rifle. Every launch you
            // do not hear is a hit you had no chance to dodge, which is the
            // difference between a hard game and an unfair one - and building
            // for evasion cannot be a real choice if the cue to evade is
            // inaudible. It rides ON TOP of the launch report rather than
            // replacing it, so the direction still comes from the report.
            // 28, not 35: the marksman rifle added last wave does 34, which
            // sat just under the old threshold - so the single longest-ranged
            // thing on the battlefield was also the only heavy weapon that
            // shot at you in silence, from beyond the range you could see it.
            if (!own && (w.blastRadius >= 3.0f || w.damage >= 28.0f) &&
                warnCooldown_ <= 0.0f) {
                const float d = length(f.pos - p.position());
                if (d < 240.0f) {
                    // At most one warning every three quarters of a second.
                    // Four rocket teams firing on the same volley would
                    // otherwise turn an alert into a texture, and a warning
                    // you hear constantly is a warning you stop hearing.
                    warnCooldown_ = 0.75f;
                    const float near2 = 1.0f - clampf(d / 240.0f, 0.0f, 1.0f);
                    audio_.push(Sfx::IncomingHeavy, 0.35f + 0.55f * near2,
                                1.0f, pl.pan * 0.7f);
                }
            }
        }
    }

    // ---- every round that landed ----------------------------------------
    // Metal answers with a ping, dirt with a thud, warheads with the blast.
    {
        int budget = 6;
        for (const ImpactEvent& ie : cev.impacts) {
            if (budget <= 0) break;
            const auto pl = place(ie.pos, 260.0f);
            if (pl.gain < 0.06f) continue;
            --budget;
            const float jitter = 0.88f + 0.24f * std::fabs(std::sin(ie.pos.x * 12.9f +
                                                                    ie.pos.z * 7.7f));
            if (ie.surface == 2) {
                audio_.push(Sfx::Explosion, pl.gain * clampf(ie.energy, 0.45f, 1.2f),
                            clampf(1.15f - ie.energy * 0.25f, 0.6f, 1.1f), pl.pan);
                const float near2 = 1.0f - clampf(length(ie.pos - p.position()) / 55.0f,
                                                  0.0f, 1.0f);
                shake_ = std::min(1.0f, shake_ + near2 * ie.energy * 0.45f);
            } else if (ie.surface == 1) {
                audio_.push(Sfx::ImpactMech, pl.gain * clampf(ie.energy, 0.3f, 1.0f),
                            jitter, pl.pan);
            } else {
                audio_.push(Sfx::ImpactWorld, pl.gain * 0.55f * clampf(ie.energy, 0.3f, 1.0f),
                            jitter, pl.pan);
            }
        }
    }

    warnCooldown_ = std::max(0.0f, warnCooldown_ - dt);

    // ---- the new battlefield, made audible -------------------------------
    // Three mechanics added last wave that a player could meet without ever
    // being told they existed. A jammer takes your instruments; a Warden
    // undoes your damage; your own reactor discharge kills drones. All three
    // were silent, and a silent mechanic is one the player concludes is a bug.
    if (mission_.takeEmpFlash()) {
        audio_.push(Sfx::EmpDischarge, 1.0f);
        shake_ = std::min(1.0f, shake_ + 0.45f);
    }
    for (const Vec3& at : mission_.repairPulses()) {
        const auto pl = place(at, 220.0f);
        if (pl.gain > 0.05f) audio_.push(Sfx::RepairPulse, pl.gain * 0.85f, 1.0f, pl.pan);
    }
    {
        const float jam = mission_.jamStrength();
        jamHum_ = std::max(0.0f, jamHum_ - dt);
        if (jam > 0.05f && jamHum_ <= 0.0f) {
            audio_.push(Sfx::JamHum, 0.35f + 0.55f * jam, 0.85f + 0.3f * jam);
            jamHum_ = 0.9f;
        }
    }

    // ---- the contract ticking over --------------------------------------
    // A segment closing out was, until now, silent: the panel in the corner
    // changed and nothing else happened. In a firefight nobody is reading the
    // corner of the screen, so the single most encouraging event in the game
    // went unnoticed until the next one started.
    {
        const int done = mission_.ledger().objectivesDone;
        if (done > lastObjectivesDone_) {
            if (lastObjectivesDone_ >= 0) audio_.push(Sfx::ObjectiveDone, 1.0f);
            lastObjectivesDone_ = done;
        } else if (done < lastObjectivesDone_) {
            lastObjectivesDone_ = done;      // a new mission: re-arm quietly
        }
    }

    // ---- ammunition scavenged -------------------------------------------
    for (int i = 0; i < cev.pickupsCollected; ++i)
        audio_.push(Sfx::PickupAmmo, 0.8f, 1.0f + 0.08f * i);
    // Field repair gets its own voice. It is the only healing there is inside
    // a mission, and a player who cannot tell by ear which crate they just ran
    // over has to check the hull bar to find out whether it mattered.
    for (int i = 0; i < cev.repairsCollected; ++i)
        audio_.push(Sfx::PickupRepair, 0.9f, 1.0f + 0.05f * i);

    // ---- the small war dying --------------------------------------------
    for (int idx : cev.unitsKilled) {
        if (idx < 0 || idx >= static_cast<int>(mission_.units().size())) continue;
        const auto pl = place(mission_.units()[static_cast<size_t>(idx)].position(), 260.0f);
        if (pl.gain > 0.03f)
            audio_.push(Sfx::Explosion, pl.gain * 0.8f, 1.25f, pl.pan);
    }
    for (int idx : cev.propsKilled) {
        if (idx < 0 || idx >= static_cast<int>(mission_.props().size())) continue;
        const auto pl = place(mission_.props()[static_cast<size_t>(idx)].pos, 300.0f);
        if (pl.gain > 0.03f)
            audio_.push(Sfx::Explosion, pl.gain, 0.85f, pl.pan);
    }

    // ---- taking damage ---------------------------------------------------
    if (prevHealth_ >= 0.0f && p.health() < prevHealth_ - 0.5f) {
        const float bite = clampf((prevHealth_ - p.health()) / 40.0f, 0.25f, 1.0f);
        audio_.push(Sfx::HullHit, bite, 1.0f - 0.25f * bite, 0.0f);
        shake_ = std::min(1.0f, shake_ + bite * 0.35f);
        rumbleHit_ = std::min(1.0f, rumbleHit_ + bite);
    }
    prevHealth_ = p.health();

    // ---- something died --------------------------------------------------
    const int alive = mission_.enemiesAlive();
    if (prevEnemies_ >= 0 && alive < prevEnemies_) {
        for (size_t i = 1; i < mission_.mechs().size(); ++i) {
            const Mech& m = mission_.mechs()[i];
            if (m.alive()) continue;
            const auto pl = place(m.position(), 220.0f);
            audio_.push(Sfx::Destroy, pl.gain, 0.9f + 0.2f * (i % 3), pl.pan);
            break;
        }
    }
    prevEnemies_ = alive;

    // ---- movement --------------------------------------------------------
    // Footfalls come from the actual gait now: the frame a leg finishes its
    // step and takes weight, that foot thuds - panned to its side, pitched by
    // the machine's mass. Six legs walking IS the rhythm; no clock fakes it.
    {
        const std::array<Leg, 6>& legs = p.legs();
        const float weight = clampf(p.stats().mass / 26.0f, 0.35f, 1.0f);
        for (int i = 0; i < 6; ++i) {
            const bool s = legs[static_cast<size_t>(i)].stepping;
            if (prevStepping_[i] && !s &&
                (p.state() == MechState::Grounded || p.state() == MechState::Landing)) {
                const auto pl = place(legs[static_cast<size_t>(i)].footSolved, 90.0f);
                audio_.push(Sfx::Footfall, 0.30f + 0.30f * weight,
                            1.05f - 0.35f * weight +
                                0.06f * static_cast<float>(i % 3),
                            pl.pan * 0.6f);
            }
            prevStepping_[i] = s;
        }
        // A nearby enemy machine's tread carries: the heaviest thing on the
        // field should be audible before it is visible.
        if (const Mech* e = mission_.nearestEnemy()) {
            const float d = length(e->position() - p.position());
            if (d < 80.0f && e->state() == MechState::Grounded && e->speed() > 1.0f) {
                footClock_ += dt * clampf(e->speed() / 4.0f, 0.4f, 2.6f);
                if (footClock_ >= 1.0f) {
                    footClock_ -= 1.0f;
                    const auto pl = place(e->position(), 90.0f);
                    audio_.push(Sfx::Footfall, pl.gain * 0.5f, 0.62f, pl.pan);
                }
            }
        }
    }

    // ---- machine noise ---------------------------------------------------
    // The turret traverse motor, heard while the gun is actually slewing.
    {
        float dRaw = p.turretYaw() - prevTurretYaw_;
        while (dRaw > PI) dRaw -= TAU;
        while (dRaw < -PI) dRaw += TAU;
        const float dYaw = std::fabs(dRaw);
        prevTurretYaw_ = p.turretYaw();
        servoClock_ -= dt;
        if (dYaw / std::max(dt, 1e-4f) > 0.55f && servoClock_ <= 0.0f) {
            servoClock_ = 0.11f;
            audio_.push(Sfx::Servo, 0.30f,
                        0.85f + clampf(dYaw / std::max(dt, 1e-4f), 0.0f, 4.0f) * 0.08f);
        }
    }
    // The reactor under load: a low drone layered every quarter second while
    // the machine moves, gain following throttle.
    {
        humClock_ -= dt;
        const float load = clampf(p.speed() / std::max(p.stats().maxSpeed, 1.0f),
                                  0.0f, 1.0f);
        if (load > 0.12f && humClock_ <= 0.0f) {
            humClock_ = 0.24f;
            audio_.push(Sfx::EngineHum, 0.18f + 0.22f * load, 0.9f + 0.25f * load);
        }
    }

    const bool charging = p.state() == MechState::Crouching;
    if (charging && !prevCharging_) audio_.push(Sfx::JumpCharge, 0.8f);
    const bool airborne = p.state() == MechState::Airborne;
    if (airborne && !prevAirborne_ && prevCharging_) audio_.push(Sfx::JumpLaunch, 0.9f);
    if (!airborne && prevAirborne_) {
        audio_.push(Sfx::Land, clampf(p.speed() / 18.0f, 0.35f, 1.0f));
        // A grey dust bloom under the feet: forty tonnes arriving should
        // move some ground.
        mission_.combat().explosion(p.position() - p.up() * (p.stats().standHeight * 0.8f),
                                    0.5f + clampf(p.speed() / 14.0f, 0.0f, 1.0f) * 1.6f,
                                    Vec3(0.38f, 0.36f, 0.32f), dustRng_);
    }
    prevCharging_ = charging;
    prevAirborne_ = airborne;

    // ---- climbing --------------------------------------------------------
    // The mantle: the moment the machine hauls itself over a roof lip gets
    // its own voice - a servo strain into a settling thud.
    if (prevCrest_ <= 0.01f && p.crestEase() > 0.01f) {
        audio_.push(Sfx::Servo, 0.7f, 0.7f);
        audio_.push(Sfx::Land, 0.55f, 1.25f);
    }
    prevCrest_ = p.crestEase();
    const bool onWall = p.onWall();
    if (onWall) {
        slipClock_ += dt;
        const float q = p.gripQuality();
        // A machine holding well bites the wall rhythmically; one at its limit
        // scrabbles, and you hear the difference before you see it.
        const float interval = (q < 0.5f) ? 0.22f : 0.55f;
        if (slipClock_ > interval) {
            slipClock_ = 0.0f;
            audio_.push(q < 0.5f ? Sfx::Slip : Sfx::Climb,
                        q < 0.5f ? 0.7f : 0.45f, 0.85f + 0.3f * q);
        }
    } else {
        slipClock_ = 0.0f;
    }
    prevOnWall_ = onWall;

    // ---- systems ---------------------------------------------------------
    if (p.overheated() && !prevOverheat_) audio_.push(Sfx::Overheat, 0.85f);
    prevOverheat_ = p.overheated();
    {
        bool used = false;
        for (int i = 0; i < 4; ++i) used = used || (mi.ability[i] && p.abilityEngaged(i));
        if (used) {
            audio_.push(Sfx::AbilityUse, 0.8f);
            // The kick: engaging anything physical rocks the camera a touch.
            shake_ = std::min(1.0f, shake_ + 0.22f);
        }
    }

    // ---- music intent ----------------------------------------------------
    // The workshop and the debrief get their own quiet theme; the briefing
    // previews the mission's score.
    const bool hangar = screen_ == GameScreen::Store || screen_ == GameScreen::MissionResult;
    audio_.setMusicStyle(hangar ? 7 : level_.musicStyle);
    audio_.setBossFight(level_.bossMission && !hangar);
    // The soundtrack follows the shape of the fight: how many hostiles are up,
    // how close the nearest one is, and how hurt you are.
    float heat = clampf(static_cast<float>(alive) / 4.0f, 0.0f, 1.0f);
    if (const Mech* e = mission_.nearestEnemy()) {
        const float d = length(e->position() - p.position());
        heat = std::max(heat, clampf(1.0f - d / 90.0f, 0.0f, 1.0f));
    }
    heat = std::max(heat, 1.0f - p.healthFraction());
    audio_.setIntensity(screen_ == GameScreen::Playing ? heat : 0.0f);
    audio_.setInCombat(screen_ == GameScreen::Playing && alive > 0);
}

// ------------------------------------------------------------------- update

void Game::update(float dt, const InputState& in) {
    elapsed_ += dt;
    hitMarker_ = std::max(0.0f, hitMarker_ - dt * 2.5f);
    audio_.clear();
    applyDisplayToggles(in);

    // Menu clicks, wherever they came from.
    if (in.menuUp || in.menuDown || in.menuLeft || in.menuRight)
        audio_.push(Sfx::UiMove, 0.7f);
    if (in.menuConfirm || in.menuDeploy) audio_.push(Sfx::UiConfirm, 0.8f);

    padActive_ = in.padActive;
    if (in.openOptions && !optionsOpen_) openOptions();
    if (optionsOpen_) {
        // The controls screen owns the input; the world stands still.
        updateOptions(in);
        return;
    }

    switch (screen_) {
        case GameScreen::Briefing: {
            screenTimer_ += dt;
            // The world keeps ticking behind the briefing so the transition
            // into the fight has no seam - but the mission is still in its
            // Briefing phase, so the hostiles only stand and animate.
            MechInput idle;
            idle.aimPoint = aimPoint_;
            mission_.update(dt * 0.35f, idle);
            updateCamera(dt, in);
            // Mission replay: cleared contracts stay open for money. The
            // arrows page through everything unlocked so far.
            if (in.menuLeft || in.menuRight) {
                const int maxLevel = profile_.maxCleared + 1;
                int want = profile_.level + (in.menuRight ? 1 : -1);
                if (want < 0) want = 0;
                if (want > maxLevel) want = maxLevel;
                if (want != profile_.level) {
                    profile_.level = want;
                    startMission();
                    break;
                }
            }
            wipeArmed_ = std::max(0.0f, wipeArmed_ - dt);
            if (in.menuNewProfile) {
                if (wipeArmed_ > 0.0f) {
                    // The confirmed wipe: a fresh ledger, a bare machine, and
                    // mission one. The old save file is overwritten on the
                    // next settle, and immediately on deploy.
                    profile_ = newProfile();
                    saveProfile(profile_, "acah_save.txt");
                    startMission();
                    wipeArmed_ = 0.0f;
                    audio_.push(Sfx::UiConfirm, 0.9f);
                    break;
                }
                wipeArmed_ = 2.5f;
                audio_.push(Sfx::UiDeny, 0.7f);
            }
            // The workshop is one key away from any briefing, so a player who
            // quit mid-mission, failed, or is replaying an old contract can
            // always refit before walking into it again.
            if (in.menuBack) {
                screen_ = GameScreen::Store;
                store_.open(profile_);
                audio_.push(Sfx::UiConfirm, 0.8f);
                break;
            }
            if (in.menuNext || in.menuConfirm || screenTimer_ > 60.0f) {
                screen_ = GameScreen::Playing;
                mission_.beginCombat();
                audio_.push(Sfx::MissionStart, 0.9f);
            }
            break;
        }

        case GameScreen::Playing: {
            aimPoint_ = traceAimPoint();
            updateAimAssist();
            const MechInput mi = buildPlayerInput(in);
            const int deathsBefore = mission_.ledger().checkpointDeaths;
            respawnBanner_ = std::max(0.0f, respawnBanner_ - dt);
            mission_.update(dt, mi);
            if (mission_.ledger().checkpointDeaths > deathsBefore)
                respawnBanner_ = 3.2f;
            if (mission_.damageDealtThisFrame() > 0.0f) {
                hitMarker_ = 1.0f;
                audio_.push(Sfx::ImpactMech, 0.55f, 1.1f);
            }
            updateCamera(dt, in);
            emitAudio(dt, mi);

            if (mission_.phase() == MissionPhase::Cleared ||
                mission_.phase() == MissionPhase::Failed) {
                screenTimer_ += dt;
                // A beat before the result, so the last explosion lands.
                if (screenTimer_ > 2.2f) {
                    screen_ = GameScreen::MissionResult;
                    screenTimer_ = 0.0f;
                    audio_.push(mission_.phase() == MissionPhase::Cleared
                                    ? Sfx::MissionWin : Sfx::MissionFail, 1.0f);
                }
            }
            break;
        }

        case GameScreen::MissionResult: {
            screenTimer_ += dt;
            MechInput idle;
            idle.aimPoint = aimPoint_;
            mission_.update(dt * 0.4f, idle);
            updateCamera(dt, in);
            if (in.menuNext || in.menuConfirm) {
                // Win or lose, the workshop comes next. Sending a losing player
                // straight back into the same fight with the same machine and
                // the salvage they just earned sitting unspent is how a run
                // becomes unwinnable without ever saying so.
                mission_.settle(profile_);
                saveProfile(profile_, "acah_save.txt");
                screen_ = GameScreen::Store;
                store_.open(profile_);
            }
            break;
        }

        case GameScreen::Store: {
            store_.update(dt);
            if (in.menuUp) store_.moveCursor(-1);
            if (in.menuDown) store_.moveCursor(1);
            if (in.menuLeft) store_.nextSlot(-1);
            if (in.menuRight) store_.nextSlot(1);
            if (in.menuConfirm) store_.confirm();
            if (in.menuBack) store_.back();
            if (in.menuToggle) {
                if (store_.pane() == StorePane::Slots && store_.slot() == Slot::Weapon)
                    store_.flipGroup();
                else
                    store_.toggleCompare();
            }
            if (in.menuSell) store_.sellSelected();
            if (in.menuDeploy) {
                store_.close();
                saveProfile(profile_, "acah_save.txt");
                startMission();
            }
            break;
        }
    }
}

void Game::updateCamera(float dt, const InputState& in) {
    const Mech& p = mission_.player();

    // Under magnification the same wrist movement must mean a smaller angle,
    // or a 7x sight is unusable: sensitivity scales with the field of view.
    const float fovScale = (scopeStage_ == 2) ? 0.13f
                         : (scopeStage_ == 1) ? 0.40f : 1.0f;
    if (!debugOrbit_) {
        // `camPitch_` is how far the camera sits *above* the machine, so
        // pushing the mouse down (positive dy) raises the camera and tips the
        // view downward. Subtracting here is what made aiming feel inverted.
        // Reticle friction: while the fire control has a target under the
        // reticle from the outside view, the view turns a third slower, so
        // the aim stays on the machine that is jinking under it rather than
        // skating past. Only outside - in the cabin the sight is yours.
        const float stick = (!fpv_ && scopeStage_ == 0 && leadValid_) ? 0.82f : 1.0f;
        camYaw_ += in.mouseDX * in.mouseSensitivity * fovScale * stick;
        camPitch_ = clampf(camPitch_ + in.mouseDY * in.mouseSensitivity * fovScale * stick,
                           kPitchMin, kPitchMax);
    }

    camDist_ = damp(camDist_, camDistTarget_, 6.0f, dt);

    // The camera's up vector chases the mech's. Following it instantly makes
    // the transition onto a wall lurch; following it slowly is what turns the
    // same event into the world rotating around the machine.
    // On the GROUND the camera's up is the world's, whatever the body is
    // doing: a walker crossing rough terrain tips its hull with every
    // slope and rut, and letting the camera frame follow that - even
    // smoothly - accumulated yaw through the parallel transport below
    // (transport around a wobbling axis is not a no-op; it is a slow
    // rotation). Measured: 10 degrees of view swing in a second of strafing
    // across a hillside with the mouse untouched, which then turned every
    // strafe into an arc. Only a genuine wall hands the frame to the
    // surface, where following it is what keeps the machine upright on
    // screen.
    const Vec3 worldUp(0.0f, 1.0f, 0.0f);
    const float onWall = clampf((p.climbFraction() - 0.10f) / 0.20f, 0.0f, 1.0f);
    Vec3 wantUp = lerp(worldUp, p.up(), onWall);
    wantUp = (lengthSq(wantUp) > 1e-6f) ? normalize(wantUp) : worldUp;
    camUp_ = normalize(lerp(camUp_, wantUp, clampf(dt * 3.2f, 0.0f, 1.0f)));

    // The orbit reference frame is carried across frames and only re-fitted to
    // `camUp_`, never rebuilt from the machine's heading.
    //
    // This is the whole fix for the camera fighting you. The machine turns to
    // face wherever you are driving it, and you drive it relative to the
    // camera. So if the camera's frame is derived from the machine's heading,
    // the two chase each other: you press forward, the body turns, the camera
    // swings, "forward" now means somewhere else, the body turns again. The
    // machine ends up crabbing sideways and the view never settles.
    //
    // Parallel transport instead: keep the reference direction, and each frame
    // just project out any component along the new `camUp_`. On level ground
    // that is a no-op and the camera is rock steady no matter what the body
    // does. On a wall it rotates exactly as much as the surface did, which is
    // what keeps the machine upright on screen while climbing.
    Vec3 refFwd = camRefFwd_ - camUp_ * dot(camRefFwd_, camUp_);
    if (lengthSq(refFwd) < 1e-6f) {
        // Only degenerate if the stored reference has become parallel to up,
        // which a 90-degree surface change can do in one step. Any consistent
        // perpendicular restores a usable frame.
        refFwd = cross(camUp_, Vec3(1.0f, 0.0f, 0.0f));
        if (lengthSq(refFwd) < 1e-6f) refFwd = cross(camUp_, Vec3(0.0f, 0.0f, 1.0f));
    }
    refFwd = normalize(refFwd);
    camRefFwd_ = refFwd;
    // Right is cross(forward, up) - the same convention Camera::set uses when
    // it builds the view basis. cross(up, forward) is the *left* vector, and
    // using it here (and for the strafe keys) is why A and D were reversed.
    const Vec3 refRight = normalize(cross(refFwd, camUp_));

    const float cy = std::cos(camYaw_), sy = std::sin(camYaw_);
    const float cp = std::cos(camPitch_), sp = std::sin(camPitch_);
    // Direction from the camera toward the focus. Positive pitch looks down.
    const Vec3 offsetDir = normalize(refFwd * (cy * cp) + refRight * (sy * cp) -
                                     camUp_ * sp);
    camOrbitDir_ = offsetDir;

    const Vec3 focus = p.position() + camUp_ * 1.4f;
    // Follow the machine closely. A slow focus lerp reads as the camera
    // drifting under you, which is the other half of "the view moves all the
    // time" - the position boom is what wants smoothing, not the aim point.
    camFocus_ = lerp(camFocus_, focus, clampf(dt * 26.0f, 0.0f, 1.0f));

    const float aspect = (raster_.cellsY() > 0)
        ? (static_cast<float>(raster_.cellsX()) * cellAspect_) /
          static_cast<float>(raster_.cellsY())
        : 1.6f;

    // First-person modes bypass the boom entirely: the eye is ON the machine,
    // looking along the same orbit direction the mouse has been steering all
    // along, so entering and leaving them never re-aims the guns.
    if (scopeStage_ > 0 || fpv_) {
        const Mech& pm = mission_.player();
        Vec3 eye;
        float fov;
        if (scopeStage_ > 0) {
            // The gunner's glass: up on the turret roof, ahead of the mantlet
            // so the hull never clips into the view.
            eye = transformPoint(pm.turretMatrix(), Vec3(0.0f, 0.85f, 1.3f));
            fov = (scopeStage_ == 2) ? deg2rad(8.5f) : deg2rad(25.0f);
        } else {
            // The driver's slit at the bow.
            eye = transformPoint(pm.bodyMatrix(), Vec3(0.0f, 0.75f, 1.5f));
            fov = deg2rad(66.0f);
        }
        // Never put the eye inside a wall. At a corner or a lip the bow can
        // be buried in the face for a few frames, and from inside the slab
        // the view is black. Pull the eye back along the line from the hull
        // centre until it is clear.
        {
            const Vec3 centre = pm.position();
            const Vec3 to = eye - centre;
            const float len = length(to);
            if (len > 0.05f) {
                const SurfaceHit h = mission_.world().raycast(centre, to / len, len + 0.25f);
                if (h.hit && h.distance < len + 0.25f)
                    eye = centre + (to / len) * std::max(0.2f, h.distance - 0.25f);
            }
        }
        camPos_ = eye;
        camFocus_ = eye + offsetDir * 60.0f;
        if (shake_ > 0.001f) {
            shake_ *= std::exp(-7.0f * dt);
            const float t2 = elapsed_ * 43.0f;
            camPos_ += (cam_.right * std::sin(t2 * 1.13f) +
                        cam_.up * std::sin(t2 * 0.79f + 1.7f)) * (shake_ * 0.10f);
        }
        cam_.set(camPos_, camFocus_, fov, aspect, 0.15f, 900.0f, camUp_);
        return;
    }

    Vec3 want = camFocus_ - offsetDir * camDist_;

    // Keep the camera out of solid geometry: pull it in along the boom if
    // something is between it and the mech.
    const Vec3 boom = want - camFocus_;
    const float boomLen = length(boom);
    if (boomLen > 1e-3f) {
        const SurfaceHit h = mission_.world().raycast(camFocus_, boom / boomLen, boomLen + 0.6f);
        if (h.hit) want = camFocus_ + (boom / boomLen) * std::max(2.2f, h.distance - 0.6f);
    }
    camPos_ = lerp(camPos_, want, clampf(dt * 14.0f, 0.0f, 1.0f));

    Vec3 eyeJit = camPos_;
    if (shake_ > 0.001f) {
        shake_ *= std::exp(-7.0f * dt);
        const float t2 = elapsed_ * 43.0f;
        eyeJit += (cam_.right * std::sin(t2 * 1.13f) +
                   cam_.up * std::sin(t2 * 0.79f + 1.7f)) * (shake_ * 0.32f);
    }
    cam_.set(eyeJit, camFocus_, deg2rad(62.0f), aspect, 0.15f, 520.0f, camUp_);
}

// ------------------------------------------------------------------- render

void Game::renderScene() {
    // The blackout: the sun goes, the fill goes, the fog closes in. What is
    // left is the machine's own lamps (the ambient floor) and the radar.
    {
        const float k = clampf(mission_.blackout(), 0.0f, 1.0f);
        if (k > 0.001f) {
            const float lit = 1.0f - 0.88f * k;
            render_.lightColor = baseRender_.lightColor * lit;
            render_.fillColor = baseRender_.fillColor * lit;
            render_.skyAmbient = baseRender_.skyAmbient * (1.0f - 0.65f * k);
            render_.groundAmbient = baseRender_.groundAmbient * (1.0f + 0.5f * k);
            render_.fogDensity = baseRender_.fogDensity * (1.0f + 1.6f * k);
            render_.fogColor = baseRender_.fogColor * (1.0f - 0.7f * k);
        } else {
            render_.lightColor = baseRender_.lightColor;
            render_.fillColor = baseRender_.fillColor;
            render_.skyAmbient = baseRender_.skyAmbient;
            render_.groundAmbient = baseRender_.groundAmbient;
            render_.fogDensity = baseRender_.fogDensity;
            render_.fogColor = baseRender_.fogColor;
        }
    }
    raster_.beginFrame(cam_, render_);
    if (screen_ == GameScreen::Store) {
        // The store renders its own little scene: two turntables parked far
        // above the battlefield, lit by the same settings.
        store_.submitMechPreview(raster_, camPos_);
        store_.submitPartPreview(raster_, kPartPreviewCentre, 1.0f);
    } else {
        // From inside the machine, the player's own fire is kept out of the
        // pilot's face for the first few metres (see Combat::submit).
        mission_.submit(raster_, camPos_, viewDistance_, scopeStage_ > 0 || fpv_,
                        (scopeStage_ > 0 || fpv_) ? 9.0f : 0.0f);
        if (inCabin()) {
            const Mech& p = mission_.player();
            bool threat = false;
            if (const Mech* e = mission_.nearestEnemy())
                threat = e->alive() && length(e->position() - p.position()) < 220.0f &&
                         mission_.world().lineOfSight(e->hitCentre(), p.hitCentre());
            const CabinState st = cabinStateFor(p, elapsed_, threat, mission_.jamStrength() > 0.3f);
            // Body sway: a walk rocks the cabin a little, more at pace; a
            // landing dips it. Tiny numbers - it is the difference between a
            // cabin and a picture frame.
            const float sf = st.speedFrac;
            const float w = elapsed_ * (5.0f + 4.5f * sf);
            Vec3 sway(std::cos(w * 0.5f) * 0.010f * sf,
                      std::sin(w) * 0.014f * sf - (p.state() == MechState::Landing ? 0.05f : 0.0f),
                      0.0f);
            // Kept for the text pass: the readouts are painted over the
            // panels' projections, so they have to ride the same sway or
            // they drift off their own instruments at walking pace.
            cabinSway_ = sway;
            submitCabin(raster_, cam_, cabinLayout_, st, sway);
        }
    }
    raster_.endFrame();
}

void Game::render(AsciiFrame& out) {
    if (screen_ == GameScreen::Store) {
        // Park the camera on the showroom turntable.
        const float aspect = (raster_.cellsY() > 0)
            ? (static_cast<float>(raster_.cellsX()) * cellAspect_) /
              static_cast<float>(raster_.cellsY())
            : 1.6f;
        const Vec3 eye = kMechPreviewCentre +
                         Vec3(std::sin(0.6f) * 13.0f, 4.5f, std::cos(0.6f) * 13.0f);
        cam_.set(eye, kMechPreviewCentre, deg2rad(48.0f), aspect, 0.15f, 520.0f,
                 Vec3(0.0f, 1.0f, 0.0f));
        camPos_ = eye;
    }
    renderScene();
    convertToAscii(raster_, ascii_, out);
    // The radar lives on the SCENE grid, not the coarse HUD grid: its cells
    // are painted as solid background-colour pixels, which is what makes it
    // read as an instrument screen rather than a box of letters.
    if (screen_ == GameScreen::Playing && hudVisible_) {
        drawPixelRadar(out);
        // The console's legends live on the same grid as the radar, for the
        // same reason: a whole scene cell per lamp is crisp, and the face's
        // projection is what puts it where the instrument is.
        if (inCabin()) drawCabinLamps(out);
    }
}

void Game::drawPixelRadar(AsciiFrame& out) {
    // The radar is painted on the SCENE grid rather than the coarse HUD
    // grid: its cells are solid background colour, which is what makes it
    // read as a lit screen instead of a box of letters. In the cabin it
    // fills the console's centre panel exactly - the panel's bezel is the
    // instrument's frame, so the display has no chrome of its own.
    const bool cabin = inCabin();
    int rx, ry, rw, rh;
    if (cabin) {
        CabinLayout lay = cabinLayout_;
        lay.resolve(cam_);
        const Vec3 sway = cabinSway_;
        auto proj = [&](const Vec3& local, float* sx, float* sy) {
            const Vec3 world = cam_.pos + cam_.right * (local.x + sway.x) +
                               cam_.up * (local.y + sway.y) +
                               cam_.forward * (local.z + sway.z);
            const Vec4 clip = transform(cam_.viewProj, Vec4(world, 1.0f));
            if (clip.w < 0.02f) return false;
            *sx = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(out.w);
            *sy = (0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(out.h);
            return true;
        };
        Vec3 c[4];
        lay.dashC.corners(c);
        float sx[4], sy[4];
        for (int i = 0; i < 4; ++i)
            if (!proj(c[i], &sx[i], &sy[i])) return;
        const float x0 = std::max(sx[0], sx[3]), x1 = std::min(sx[1], sx[2]);
        const float y0 = std::max(sy[0], sy[1]), y1 = std::min(sy[2], sy[3]);
        rx = std::max(0, static_cast<int>(std::ceil(x0)));
        ry = std::max(0, static_cast<int>(std::ceil(y0)));
        rw = std::min(out.w - rx, static_cast<int>(x1) - rx);
        rh = std::min(out.h - ry, static_cast<int>(y1) - ry);
        if (rw < 12 || rh < 8) return;
    } else {
        rh = 32;                                    // 2:1 cells -> square map
        while ((out.w < rh * 2 + 4 || out.h < rh + 4) && rh > 12) rh -= 2;
        rw = rh * 2;
        if (out.w < rw + 4 || out.h < rh + 4) return;
        rx = out.w - rw - 2;
        ry = out.h - rh - 2;
    }

    const float range = 150.0f;                     // metres to the nearer edge
    const Mech& p = mission_.player();
    const Vec3 fwd = normalize(flattenY(camFocus_ - camPos_) + Vec3(0.0f, 0.0f, 1e-4f));
    // Screen right, same convention as lookAt: cross(forward, up).
    const Vec3 right = Vec3(-fwd.z, 0.0f, fwd.x);

    // Cells per metre. The grid's cells are twice as tall as they are wide,
    // so the two axes need different cell scales to put a circle on screen
    // as a circle: pick the one that fits `range` in both directions and let
    // the longer axis simply show more ground.
    const float cx = static_cast<float>(rw) * 0.5f, cyf = static_cast<float>(rh) * 0.5f;
    const float aspect = std::max(0.1f, cellAspect_);
    const float sX = std::min(cx / range, (cyf / aspect) / range);
    const float sY = sX * aspect;

    auto pix = [&](int x, int y, const Vec3& c) {
        if (x < 0 || y < 0 || x >= rw || y >= rh) return;
        Cell& cell = out.at(rx + x, ry + y);
        cell.ch = ' ';
        cell.r = cell.g = cell.b = 0;
        cell.br = static_cast<uint8_t>(clampf(c.x, 0.0f, 1.0f) * 255.0f);
        cell.bg = static_cast<uint8_t>(clampf(c.y, 0.0f, 1.0f) * 255.0f);
        cell.bb = static_cast<uint8_t>(clampf(c.z, 0.0f, 1.0f) * 255.0f);
        if (cell.br == 0 && cell.bg == 0 && cell.bb == 0) cell.bg = 8;
    };
    // Brighten whatever is already there, for the sweep's afterglow.
    auto glow = [&](int x, int y, float k) {
        if (x < 0 || y < 0 || x >= rw || y >= rh) return;
        Cell& cell = out.at(rx + x, ry + y);
        auto lift = [&](uint8_t v, float add) {
            return static_cast<uint8_t>(clampf(static_cast<float>(v) + add, 0.0f, 255.0f));
        };
        cell.br = lift(cell.br, 26.0f * k);
        cell.bg = lift(cell.bg, 64.0f * k);
        cell.bb = lift(cell.bb, 30.0f * k);
    };
    // World position -> cell. Returns false when it lies off the screen and
    // is not to be clamped to the rim.
    auto plot = [&](const Vec3& at, bool clampEdge, int* ox, int* oy) {
        const Vec3 rel = flattenY(at - p.position());
        float fx = dot(rel, right) * sX;
        float fz = dot(rel, fwd) * sY;
        const float mx = cx - 1.0f, my = cyf - 1.0f;
        if (std::fabs(fx) > mx || std::fabs(fz) > my) {
            if (!clampEdge) return false;
            const float k = std::min(mx / std::max(std::fabs(fx), 1e-3f),
                                     my / std::max(std::fabs(fz), 1e-3f));
            fx *= k;
            fz *= k;
        }
        *ox = static_cast<int>(cx + fx);
        *oy = static_cast<int>(cyf - fz);
        return true;
    };
    auto blip = [&](const Vec3& at, const Vec3& c, bool clampEdge) {
        int x, y;
        if (plot(at, clampEdge, &x, &y)) pix(x, y, c);
    };

    // ---- the screen itself, refreshed a few times a second ---------------
    // Terrain under a graticule: the ground as a green height ramp, water
    // dark blue, anything built a flat grey, with range rings every fifty
    // metres and cross-hairs on the machine's own axes. All of it is static
    // in screen space, so it is baked into the same cache as the terrain.
    if (radarBg_.size() != static_cast<size_t>(rw * rh)) {
        radarBg_.assign(static_cast<size_t>(rw * rh), Vec3(0.0f));
        radarTick_ = 0;
    }
    if (radarTick_-- <= 0) {
        radarTick_ = 8;
        const World& w = mission_.world();
        const float py = p.position().y;
        // The ground first, then the buildings STAMPED over it. Asking the
        // world "is this point built on" once per radar cell meant a grid
        // query and a walk of every major prop for each of a few thousand
        // cells, which cost the best part of a tenth of a second every time
        // the plot refreshed - a hitch every eight frames, and the worse the
        // arena was built up the worse it got. Drawing the boxes onto the
        // map instead is the same picture for the cost of the boxes.
        for (int y = 0; y < rh; ++y)
            for (int x = 0; x < rw; ++x) {
                const float mx = (static_cast<float>(x) + 0.5f - cx) / sX;
                const float mz = (cyf - static_cast<float>(y) - 0.5f) / sY;
                const Vec3 at = p.position() + right * mx + fwd * mz;
                Vec3 c;
                const float hgt = w.terrain().height(at.x, at.z);
                if (w.hasWater() && hgt < w.waterLevel()) {
                    c = Vec3(0.04f, 0.07f, 0.13f);              // sea
                } else {
                    // Height relative to the machine, as a green ramp.
                    const float rel = clampf((hgt - py) / 40.0f + 0.5f, 0.0f, 1.0f);
                    c = lerp(Vec3(0.030f, 0.090f, 0.052f),
                             Vec3(0.17f, 0.29f, 0.14f), rel);
                }
                // The graticule, etched over the ground: rings at fifty-metre
                // steps and the two axes through the machine.
                const float d = std::sqrt(mx * mx + mz * mz);
                const float ringPitch = 50.0f;
                const float ring = std::fabs(d - std::round(d / ringPitch) * ringPitch);
                const float cellM = 1.0f / std::max(sX, 1e-3f);
                if (d > ringPitch * 0.5f && ring < cellM * 0.55f)
                    c = lerp(c, Vec3(0.10f, 0.30f, 0.16f), 0.55f);
                if (std::fabs(mx) < cellM * 0.6f || std::fabs(mz) < cellM * 0.6f)
                    c = lerp(c, Vec3(0.07f, 0.22f, 0.12f), 0.45f);
                radarBg_[static_cast<size_t>(y * rw + x)] = c;
            }
        // ---- the built-up ground, stamped from the boxes themselves ------
        const Vec3 built(0.20f, 0.22f, 0.19f);
        const float reach = range * 1.7f;
        // World XZ -> map cell. The inverse of the loop above, so a box lands
        // exactly on the ground it stands on.
        auto cellOf = [&](const Vec3& at, float* fx, float* fy) {
            const Vec3 rel = flattenY(at - p.position());
            *fx = cx + dot(rel, right) * sX;
            *fy = cyf - dot(rel, fwd) * sY;
        };
        auto stampBox = [&](const Obstacle& ob) {
            // Only what stands ON the ground: a roof deck twenty metres up
            // is not something the plot should paint as a wall.
            const float base = mission_.world().terrain().height(ob.center.x, ob.center.z);
            if (ob.center.y - ob.half.y > base + 3.0f) return;
            if (ob.center.y + ob.half.y < base + 0.6f) return;
            const float r = std::sqrt(ob.half.x * ob.half.x + ob.half.z * ob.half.z);
            float fx, fy;
            cellOf(ob.center, &fx, &fy);
            const int spanX = static_cast<int>(r * sX) + 1;
            const int spanY = static_cast<int>(r * sY) + 1;
            const int x0 = std::max(0, static_cast<int>(fx) - spanX);
            const int x1 = std::min(rw - 1, static_cast<int>(fx) + spanX);
            const int y0 = std::max(0, static_cast<int>(fy) - spanY);
            const int y1 = std::min(rh - 1, static_cast<int>(fy) + spanY);
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    const float mx = (static_cast<float>(x) + 0.5f - cx) / sX;
                    const float mz = (cyf - static_cast<float>(y) - 0.5f) / sY;
                    const Vec3 at = p.position() + right * mx + fwd * mz;
                    const Vec3 l = ob.toLocal(Vec3(at.x, ob.center.y, at.z));
                    if (std::fabs(l.x) > ob.half.x + 0.4f ||
                        std::fabs(l.z) > ob.half.z + 0.4f) continue;
                    radarBg_[static_cast<size_t>(y * rw + x)] = built;
                }
        };
        for (const Obstacle& ob : w.obstacles()) {
            if (!ob.active) continue;
            if (std::fabs(ob.center.x - p.position().x) > reach ||
                std::fabs(ob.center.z - p.position().z) > reach) continue;
            stampBox(ob);
        }
        // A building's footprint, so a hollow ruin reads as a block rather
        // than as four thin walls.
        for (const PropInstance& prop : w.propInstances()) {
            if (!prop.major || prop.hidden) continue;
            if (std::fabs(prop.pos.x - p.position().x) > reach ||
                std::fabs(prop.pos.z - p.position().z) > reach) continue;
            float fx, fy;
            cellOf(prop.pos, &fx, &fy);
            const int spanX = static_cast<int>(prop.radius * sX) + 1;
            const int spanY = static_cast<int>(prop.radius * sY) + 1;
            const float rr = prop.radius * prop.radius;
            for (int y = std::max(0, static_cast<int>(fy) - spanY);
                 y <= std::min(rh - 1, static_cast<int>(fy) + spanY); ++y)
                for (int x = std::max(0, static_cast<int>(fx) - spanX);
                     x <= std::min(rw - 1, static_cast<int>(fx) + spanX); ++x) {
                    const float mx = (static_cast<float>(x) + 0.5f - cx) / sX;
                    const float mz = (cyf - static_cast<float>(y) - 0.5f) / sY;
                    const Vec3 at = p.position() + right * mx + fwd * mz;
                    const float dx = at.x - prop.pos.x, dz = at.z - prop.pos.z;
                    if (dx * dx + dz * dz > rr) continue;
                    radarBg_[static_cast<size_t>(y * rw + x)] = built;
                }
        }
    }
    for (int y = 0; y < rh; ++y)
        for (int x = 0; x < rw; ++x)
            pix(x, y, radarBg_[static_cast<size_t>(y * rw + x)]);

    // ---- the sweep --------------------------------------------------------
    // A bar of light going round the screen with a tail behind it. It tells
    // the pilot at a glance that the set is live - a static plot reads as a
    // picture, a sweeping one reads as an instrument listening.
    {
        const float sweep = std::fmod(elapsed_ * 1.9f, TAU);
        const int tail = 22;
        const float reach = std::sqrt(cx * cx + cyf * cyf);
        for (int t = 0; t < tail; ++t) {
            const float a = sweep - static_cast<float>(t) * 0.055f;
            const float k = (1.0f - static_cast<float>(t) / tail);
            const float sa = std::sin(a), ca = std::cos(a);
            for (float d = 1.0f; d < reach; d += 0.7f) {
                const int x = static_cast<int>(cx + sa * d);
                const int y = static_cast<int>(cyf - ca * d * aspect);
                glow(x, y, k * k * 0.55f);
            }
        }
    }

    // ---- the route --------------------------------------------------------
    // A dotted spur toward whatever the contract wants next, and the mark
    // itself blinking at the end of it.
    Vec3 goal = mission_.objectiveZone();
    float bestD = 1e18f;
    for (int idx : mission_.markedProps()) {
        const Destructible& d = mission_.props()[static_cast<size_t>(idx)];
        if (!d.alive) continue;
        const float dd = lengthSq(d.pos - p.position());
        if (dd < bestD) { bestD = dd; goal = d.pos; }
    }
    for (int idx : mission_.markedUnits()) {
        const Unit& u = mission_.units()[static_cast<size_t>(idx)];
        if (!u.alive()) continue;
        const float dd = lengthSq(u.position() - p.position());
        if (dd < bestD) { bestD = dd; goal = u.position(); }
    }
    {
        const Vec3 rel = flattenY(goal - p.position());
        const float rl = length(rel);
        if (rl > 10.0f) {
            const Vec3 dir = rel / rl;
            for (int stp = 1; stp <= 7; ++stp)
                blip(p.position() + dir * (range * 0.12f * stp),
                     Vec3(0.20f, 0.30f, 0.15f), false);
        }
        if (std::fmod(elapsed_, 0.8f) < 0.55f) {
            int gx, gy;
            if (plot(goal, true, &gx, &gy)) {
                const Vec3 amber(1.0f, 0.75f, 0.25f);
                pix(gx, gy, amber);
                pix(gx - 1, gy, amber * 0.5f);
                pix(gx + 1, gy, amber * 0.5f);
            }
        }
    }

    // ---- contacts ---------------------------------------------------------
    // Jamming eats the plot. Contacts drop out at random in proportion to how
    // hard you are being jammed, so the display flickers and thins rather than
    // switching off - a radar that is LYING to you reads very differently from
    // one that is merely absent, and it makes finding the jammer urgent.
    const float jam = mission_.jamStrength();
    for (const Unit& u : mission_.units()) {
        if (!u.alive()) continue;
        if (u.team() == Team::Player) { blip(u.position(), Vec3(0.3f, 0.9f, 0.4f), true); continue; }
        Vec3 c(0.85f, 0.45f, 0.25f);
        if (u.kind() == UnitKind::Tank) c = Vec3(1.0f, 0.35f, 0.2f);
        if (u.kind() == UnitKind::Drone || u.kind() == UnitKind::Gunship) c = Vec3(0.45f, 0.7f, 1.0f);
        if (u.kind() == UnitKind::Warden) c = Vec3(0.45f, 1.0f, 0.55f);
        if (u.kind() == UnitKind::Jammer) c = Vec3(0.75f, 0.55f, 1.0f);
        if (u.kind() == UnitKind::ShieldPylon) c = Vec3(0.55f, 0.85f, 1.0f);
        // The jammer itself never hides: it is loud, and you are meant to be
        // able to go and find it.
        if (jam > 0.02f && u.kind() != UnitKind::Jammer) {
            const float h = std::fabs(std::sin(u.position().x * 3.1f +
                                               u.position().z * 1.7f +
                                               elapsed_ * 2.3f));
            if (h < jam * 0.85f) continue;
        }
        blip(u.position(), c, false);
    }
    // Hostile machines are the only contact worth two cells: they are the
    // thing that kills you, and one cell at this scale is a speck.
    for (size_t i = 1; i < mission_.mechs().size(); ++i) {
        const Mech& m = mission_.mechs()[i];
        if (!m.alive()) continue;
        int x, y;
        if (!plot(m.position(), true, &x, &y)) continue;
        const Vec3 red(1.0f, 0.15f, 0.12f);
        pix(x, y, red);
        pix(x + 1, y, red);
    }

    // ---- the machine, dead centre -----------------------------------------
    // A chevron pointing up the screen (the plot is always heading-up) with a
    // short bore line ahead of it, so "which way am I looking" is answered by
    // the instrument and not by memory.
    {
        const int mx = static_cast<int>(cx), my = static_cast<int>(cyf);
        const Vec3 lit(0.85f, 1.0f, 0.88f);
        for (int i = 1; i <= std::max(2, rh / 8); ++i)
            pix(mx, my - i, Vec3(0.30f, 0.55f, 0.34f));
        pix(mx, my, lit);
        pix(mx - 1, my + 1, lit * 0.7f);
        pix(mx + 1, my + 1, lit * 0.7f);
    }

    // Frame and label. In the cabin the frame is the panel's own bezel and
    // the range is etched at the bottom of the screen; outside, the radar is
    // a floating box and needs its own.
    if (!cabin) {
        drawRect(out, rx - 1, ry - 1, rw + 2, rh + 2, kDim);
        drawText(out, rx, ry - 1, "RADAR 150m", kDim);
    } else if (rw > 22) {
        const TextStyle etch{Vec3(0.22f, 0.45f, 0.26f), true};
        drawText(out, rx + 1, ry + rh - 1, fmtInt(static_cast<int>(range)) + "m", etch);
    }
}

void Game::drawHudOnly(AsciiFrame& out) {
    // The caller owns the size. Clearing rather than resizing keeps the frame's
    // buffers allocated across frames.
    for (Cell& c : out.cells) c = Cell{};
    if (hudVisible_) drawHud(out);
    if (optionsOpen_) {
        drawOptions(out);
        return;
    }
    if (paused_ && out.w >= 40 && out.h >= 10) {
        const int cx = out.w / 2, cy = out.h / 2;
        fillPanel(out, cx - 21, cy - 2, 42, 5, Vec3(0.008f, 0.018f, 0.013f));
        drawText(out, cx - 3, cy - 1, "PAUSED", kBright);
        drawText(out, cx - 19, cy + 1,
                 padActive_ ? "OPTIONS RESUME   SHARE CONTROLS"
                            : "ESC RESUME   O CONTROLS   Q QUIT", kDim);
    }
}

// ------------------------------------------------------------- controls

void Game::loadBindings() {
    bindings_.load("acah_controls.txt");
}

void Game::optionsEscape() {
    if (!optionsOpen_) return;
    if (optCapture_ != 0) optCapture_ = 0;
    else optionsOpen_ = false;
}

std::string Game::keyLabel(int code) const {
    if (code == kCodeNone) return "---";
    if (code >= kMouseCodeBase) {
        const int b = code - kMouseCodeBase;
        return b == 1 ? "MOUSE L" : b == 2 ? "MOUSE M" : b == 3 ? "MOUSE R"
                                  : "MOUSE " + fmtInt(b);
    }
    // The platform knows the keyboard layout and can do better; without one
    // attached the core still letters its own cockpit plate.
    if (keyNamer_) {
        std::string n = keyNamer_(code);
        for (char& c : n) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (!n.empty()) return n;
    }
    return defaultKeyName(code);
}

// The short form, for the places a binding is engraved on a panel rather
// than listed in a table: "LMB" where the options screen says "MOUSE L".
std::string Game::keyLabelShort(int code) const {
    if (code >= kMouseCodeBase) return defaultKeyName(code);
    const std::string full = keyLabel(code);
    return full.size() > 5 ? defaultKeyName(code) : full;
}

std::string Game::padLabel(int code) const {
    if (code == kCodeNone) return "---";
    if (padNamer_) {
        const std::string n = padNamer_(code);
        if (!n.empty()) return n;
    }
    return defaultPadName(code);
}

std::string Game::actionLabel(Action a) const {
    const ActionBinding& b = bindings_.at(a);
    return padActive_ ? padLabel(b.pad) : keyLabel(b.key);
}

std::string Game::actionLabelShort(Action a) const {
    const ActionBinding& b = bindings_.at(a);
    return padActive_ ? padLabel(b.pad) : keyLabelShort(b.key);
}

std::string Game::hk(const char* key, int padCode) const {
    return "[" + (padActive_ ? padLabel(padCode) : std::string(key)) + "]";
}

void Game::updateOptions(const InputState& in) {
    const int rowsTotal = kActionCount + 2;      // + RESET, BACK
    if (optCapture_ != 0) {
        const Action a = static_cast<Action>(optCursor_);
        if (in.menuBack) { optCapture_ = 0; return; }
        if (optCapture_ == 1 && in.rawKey != kCodeNone) {
            bindings_.setKey(a, in.rawKey);
            bindings_.save("acah_controls.txt");
            optCapture_ = 0;
            audio_.push(Sfx::UiConfirm, 0.8f);
        } else if (optCapture_ == 2 && in.rawPad != kCodeNone) {
            bindings_.setPad(a, in.rawPad);
            bindings_.save("acah_controls.txt");
            optCapture_ = 0;
            audio_.push(Sfx::UiConfirm, 0.8f);
        }
        return;
    }
    if (in.menuUp) optCursor_ = (optCursor_ + rowsTotal - 1) % rowsTotal;
    if (in.menuDown) optCursor_ = (optCursor_ + 1) % rowsTotal;
    // Left/right hop between the two columns of the table.
    const int perCol = (kActionCount + 1) / 2;
    if ((in.menuLeft || in.menuRight) && optCursor_ < kActionCount) {
        const int other = (optCursor_ < perCol) ? optCursor_ + perCol : optCursor_ - perCol;
        if (other < kActionCount) optCursor_ = other;
    }
    if (in.menuBack) { optionsOpen_ = false; return; }
    if (optCursor_ == kActionCount) {              // RESET DEFAULTS
        if (in.menuConfirm) {
            bindings_.reset();
            bindings_.save("acah_controls.txt");
            audio_.push(Sfx::UiConfirm, 0.8f);
        }
        return;
    }
    if (optCursor_ == kActionCount + 1) {          // BACK
        if (in.menuConfirm) optionsOpen_ = false;
        return;
    }
    const Action a = static_cast<Action>(optCursor_);
    // Confirm rebinds on the device the confirm came from; the toggle key
    // rebinds the other one, so a keyboard user can still set pad buttons.
    if (in.menuConfirm) optCapture_ = padActive_ ? 2 : 1;
    else if (in.menuToggle) optCapture_ = padActive_ ? 1 : 2;
    else if (in.menuSell) {
        if (padActive_) bindings_.setPad(a, kCodeNone); else bindings_.setKey(a, kCodeNone);
        bindings_.save("acah_controls.txt");
    }
}

void Game::drawOptions(AsciiFrame& frame) {
    const int W = frame.w, H = frame.h;
    if (W < 60 || H < 20) return;
    for (Cell& c : frame.cells) c = Cell{};
    fillPanel(frame, 0, 0, W, H, Vec3(0.006f, 0.014f, 0.010f));
    drawText(frame, 2, 1, "CONTROLS", kBright);
    drawTextRight(frame, W - 2, 1, padActive_ ? "PAD" : "KEYBOARD", kDim);

    // Two columns of actions: name, key, pad.
    const int perCol = (kActionCount + 1) / 2;
    const int colW = std::min(48, (W - 4) / 2);
    const int nameW = std::max(12, colW - 24);
    drawText(frame, 2 + nameW, 3, "KEY", kDim);
    drawText(frame, 2 + nameW + 12, 3, "PAD", kDim);
    if (2 + colW + nameW + 12 < W) {
        drawText(frame, 2 + colW + nameW, 3, "KEY", kDim);
        drawText(frame, 2 + colW + nameW + 12, 3, "PAD", kDim);
    }
    for (int i = 0; i < kActionCount; ++i) {
        const int col = i / perCol;
        const int x = 2 + col * colW;
        const int y = 4 + (i % perCol);
        if (y >= H - 4) break;
        const bool cur = (i == optCursor_);
        const Action a = static_cast<Action>(i);
        const ActionBinding& b = bindings_.at(a);
        std::string name = actionName(a);
        if (static_cast<int>(name.size()) > nameW - 3) name = name.substr(0, static_cast<size_t>(nameW - 3));
        drawText(frame, x, y, (cur ? "> " : "  ") + name, cur ? kBright : kNorm);
        const bool capK = cur && optCapture_ == 1, capP = cur && optCapture_ == 2;
        drawText(frame, x + nameW, y, capK ? "PRESS..." : keyLabel(b.key).substr(0, 11),
                 capK ? kWarn : (cur ? kBright : kDim));
        drawText(frame, x + nameW + 12, y, capP ? "PRESS..." : padLabel(b.pad).substr(0, 11),
                 capP ? kWarn : (cur ? kBright : kDim));
    }
    const int ry = 4 + perCol + 1;
    drawText(frame, 2, ry, std::string(optCursor_ == kActionCount ? "> " : "  ") + "RESET DEFAULTS",
             optCursor_ == kActionCount ? kBright : kNorm);
    drawText(frame, 2, ry + 1, std::string(optCursor_ == kActionCount + 1 ? "> " : "  ") + "BACK",
             optCursor_ == kActionCount + 1 ? kBright : kNorm);

    if (optCapture_ != 0) {
        drawText(frame, 2, H - 2,
                 optCapture_ == 1 ? "PRESS A KEY OR MOUSE BUTTON   [BKSP] CANCEL"
                                  : "PRESS A PAD BUTTON OR TRIGGER   " + hk("BKSP", 1) + " CANCEL",
                 kWarn);
    } else {
        drawText(frame, 2, H - 2,
                 hk("ENTER", 0) + " REBIND  " + hk("TAB", 3) + " REBIND OTHER DEVICE  " +
                     hk("X", 2) + " UNBIND  " + hk("BKSP", 1) + " BACK",
                 kDim);
    }
    drawText(frame, 2, H - 1, "SAVED TO acah_controls.txt   MENUS ALWAYS USE ARROWS / ENTER / D-PAD", kDim);
}

void Game::hudGridFor(int availCols, int availRows, int* colsOut, int* rowsOut) {
    // The workshop is the widest screen: it needs a slot list on the left, a
    // comparison table on the right, and a controls line underneath.
    if (colsOut) *colsOut = clampf(static_cast<float>(availCols), 64.0f, 190.0f);
    if (rowsOut) *rowsOut = clampf(static_cast<float>(availRows), 24.0f, 60.0f);
}

void Game::drawHud(AsciiFrame& frame) {
    switch (screen_) {
        case GameScreen::Briefing:      drawBriefing(frame); break;
        case GameScreen::Playing:       drawCombatHud(frame); break;
        case GameScreen::MissionResult: drawResult(frame); break;
        case GameScreen::Store:         drawStore(frame); break;
    }
}

// --------------------------------------------------------------- combat HUD

void Game::drawCombatHud(AsciiFrame& frame) {
    const int W = frame.w, H = frame.h;
    if (W < 30 || H < 12) return;
    const Mech& p = mission_.player();

    // ---- reticle ---------------------------------------------------------
    const int cx = W / 2, cy = H / 2;
    const TextStyle& ret = p.overheated() ? kBad : (aimHitSomething_ ? kNorm : kDim);
    // The reticle OPENS with the cone. Sustained fire from a rotary walks its
    // group out to three times its resting spread, and a fixed crosshair
    // hides that completely - the player just experiences their gun quietly
    // getting worse for no visible reason and has no way to learn that
    // letting go of the trigger fixes it. The marks step outward as the bloom
    // builds and close again as it recovers, so trigger discipline is
    // something you can SEE working.
    float cone = 0.0f;
    for (const MountedWeapon& mw : p.weapons())
        if (mw.part && mw.enabled) cone = std::max(cone, mw.bloom);
    const int open = static_cast<int>(clampf(cone, 0.0f, 2.4f) * 1.7f);
    const int retX = 2 + open, retY = 1 + (open + 1) / 2;
    const Vec3 hot = lerp(ret.color, Vec3(1.0f, 0.62f, 0.30f),
                          clampf(cone / 1.6f, 0.0f, 1.0f));
    putCell(frame, cx - retX, cy, '-', hot);
    putCell(frame, cx + retX, cy, '-', hot);
    putCell(frame, cx, cy - retY, '|', hot);
    putCell(frame, cx, cy + retY, '|', hot);
    // A faint inner mark stays put so the aim point itself never moves.
    if (open > 0) {
        putCell(frame, cx - 2, cy, '.', ret.color * 0.55f);
        putCell(frame, cx + 2, cy, '.', ret.color * 0.55f);
    }
    if (hitMarker_ > 0.0f) {
        const Vec3 c = lerp(Vec3(0.4f, 0.6f, 0.45f), Vec3(1.0f, 0.9f, 0.4f), hitMarker_);
        putCell(frame, cx - 1, cy - 1, '\\', c);
        putCell(frame, cx + 1, cy - 1, '/', c);
        putCell(frame, cx - 1, cy + 1, '/', c);
        putCell(frame, cx + 1, cy + 1, '\\', c);
    }

    // ---- the fall of shot ------------------------------------------------
    // Rounds drop, and how far they drop is what decides a gun's useful
    // reach. The sight says where they will land: a mark under the reticle
    // for the SLOWEST gun currently switched on (if that one arrives, the
    // faster ones do), stepping further down the glass the further out the
    // pilot is looking. Lay the mark on the target rather than the cross and
    // the shot goes home - which is the whole skill a slow gun asks for.
    {
        float slow = 1e9f;
        float grav = kShotGravity;
        int slowIdx = -1;
        for (size_t i = 0; i < p.weapons().size(); ++i) {
            const MountedWeapon& mw = p.weapons()[i];
            if (!mw.part || !mw.enabled) continue;
            if (mw.part->weapon.projectileSpeed < slow) {
                slow = mw.part->weapon.projectileSpeed;
                grav = shotGravity(mw.part->weapon);
                slowIdx = static_cast<int>(i);
            }
        }
        const float dist = (slowIdx >= 0)
                               ? length(aimPoint_ - p.muzzlePosition(slowIdx))
                               : 0.0f;
        if (slow < 1e8f && dist > 20.0f) {
            const float drop = ballisticDrop(dist, slow, grav);
            const Vec4 a = transform(cam_.viewProj, Vec4(aimPoint_, 1.0f));
            const Vec4 b = transform(cam_.viewProj,
                                     Vec4(aimPoint_ - Vec3(0.0f, drop, 0.0f), 1.0f));
            if (a.w > 0.05f && b.w > 0.05f) {
                const float ay = (0.5f - a.y / a.w * 0.5f) * static_cast<float>(H);
                const float by = (0.5f - b.y / b.w * 0.5f) * static_cast<float>(H);
                const int dy = static_cast<int>(by - ay + 0.5f);
                if (dy >= 2 && cy + dy < H - 1) {
                    const Vec3 amber(0.95f, 0.72f, 0.30f);
                    putCell(frame, cx, cy + dy, '=', amber);
                    putCell(frame, cx - 1, cy + dy, '.', amber * 0.6f);
                    putCell(frame, cx + 1, cy + dy, '.', amber * 0.6f);
                }
            }
        }
    }

    // ---- fire-control lead marker ---------------------------------------
    // The sensor's answer to "where do I actually put the rounds": a red
    // cross at the intercept point for the current target. Put your reticle
    // on the red and the assist walks the shots the rest of the way.
    if (leadValid_) {
        const Vec4 clip = transform(cam_.viewProj, Vec4(leadPoint_, 1.0f));
        if (clip.w > 0.05f) {
            const int lx = static_cast<int>((clip.x / clip.w * 0.5f + 0.5f) *
                                            static_cast<float>(W));
            const int ly = static_cast<int>((0.5f - clip.y / clip.w * 0.5f) *
                                            static_cast<float>(H));
            if (lx > 1 && lx < W - 2 && ly > 1 && ly < H - 2) {
                const Vec3 red(1.0f, 0.28f, 0.20f);
                putCell(frame, lx, ly, 'x', red);
                putCell(frame, lx - 1, ly, '[', red);
                putCell(frame, lx + 1, ly, ']', red);
                // Locked solutions get the full bracket.
                if (assistStrength_ > 0.9f) {
                    putCell(frame, lx, ly - 1, '-', red);
                    putCell(frame, lx, ly + 1, '-', red);
                }
            }
        }
    }

    // ---- machine lost ----------------------------------------------------
    if (respawnBanner_ > 0.0f) {
        const bool blinkOn = std::fmod(elapsed_, 0.5f) < 0.34f;
        if (blinkOn)
            drawText(frame, cx - 13, cy + 6, "MACHINE DESTROYED", kBad);
        drawText(frame, cx - 13, cy + 7, "THE CONTRACT IS LOST", kDim);
    }
    // ---- low structure warning -------------------------------------------
    // With no respawn, knowing you are one shell from losing the contract is
    // the most important thing the HUD can tell you. It also points at the
    // way out: amber salvage crates are the only repair there is.
    if (p.healthFraction() < 0.30f && p.alive() && !inCabin()) {
        const bool blinkOn = std::fmod(elapsed_, 0.7f) < 0.45f;
        if (blinkOn) drawText(frame, cx - 8, cy + 5, "STRUCTURE CRITICAL", kBad);
        drawText(frame, cx - 14, cy + 6, "FIND REPAIR SALVAGE (AMBER CRATES)", kDim);
    }

    // ---- hit direction -----------------------------------------------------
    // Where that came from: a red chevron on a ring around the reticle,
    // pointing at the shooter, fading over a second. The single most useful
    // thing a HUD can tell a machine with a thin back plate.
    if (p.alive() && p.lastHitAge() < 1.1f) {
        const Vec3 src = p.lastHitDirection() * -1.0f;   // toward the shooter
        const Vec3 fwdF = normalize(flattenY(camFocus_ - camPos_) +
                                    Vec3(0.0f, 0.0f, 1e-4f));
        const Vec3 rightF = Vec3(fwdF.z, 0.0f, -fwdF.x);
        const float ang = std::atan2(dot(flattenY(src), rightF),
                                     dot(flattenY(src), fwdF));
        const int px = cx + static_cast<int>(std::round(std::sin(ang) * 10.0f));
        const int py = cy - static_cast<int>(std::round(std::cos(ang) * 5.0f));
        const float fade = 1.0f - p.lastHitAge() / 1.1f;
        const Vec3 col = Vec3(1.0f, 0.25f, 0.18f) * (0.4f + 0.6f * fade);
        const float a = std::fmod(ang + TAU + PI / 8.0f, TAU);
        static const char kChev[8] = {'^', '/', '>', '\\', 'v', '/', '<', '\\'};
        const char ch = kChev[static_cast<int>(a / (PI / 4.0f)) & 7];
        putCell(frame, px, py, ch, col);
        putCell(frame, px + ((ch == '<') ? 1 : (ch == '>') ? -1 : 0),
                py + ((ch == '^') ? 1 : (ch == 'v') ? -1 : 0), '*', col * 0.7f);
    }

    // ---- in-world objective markers --------------------------------------
    // The mission drawn on the world itself: a blinking diamond over the
    // current objective with the range under it, and a tick over every marked
    // target. "Where do I go" should never require reading a sentence.
    {
        auto project = [&](const Vec3& at, int* px, int* py) {
            const Vec4 clip = transform(cam_.viewProj, Vec4(at, 1.0f));
            if (clip.w < 0.05f) return false;
            *px = static_cast<int>((clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(W));
            *py = static_cast<int>((0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(H));
            return *px > 1 && *px < W - 6 && *py > 1 && *py < H - 2;
        };
        const Vec3 amber(1.0f, 0.72f, 0.25f);
        auto marker = [&](const Vec3& at, bool primary) {
            int px, py;
            if (!project(at + Vec3(0.0f, 6.0f, 0.0f), &px, &py)) return;
            if (primary) {
                if (std::fmod(elapsed_, 0.9f) < 0.62f) {
                    putCell(frame, px, py, 'V', amber);
                    putCell(frame, px - 1, py - 1, '/', amber);
                    putCell(frame, px + 1, py - 1, '\\', amber);
                }
                // The range in metres is a cockpit reading, not a sticker on
                // the world: from inside the machine it belongs on the brow
                // panel with the rest of the contract, and the diamond alone
                // marks the spot.
                if (!inCabin()) {
                    const int dist = static_cast<int>(length(at - p.position()));
                    drawText(frame, px - 2, py - 2, fmtInt(dist) + "m",
                             TextStyle{amber, false});
                }
            } else {
                putCell(frame, px, py, 'v', amber * 0.8f);
            }
        };
        if (const ObjectiveSpec* obj = mission_.currentObjective()) {
            // Every objective that HAS a place gets a diamond over it - the
            // march segments emphatically included. They are the commonest
            // objective in the game and they used to be the only ones with no
            // marker and no bearing anywhere on the HUD, which meant the
            // instruction "PUSH THE GUN LINE" came with no indication of which
            // way the gun line was. Worse, a march advances the moment you
            // ARRIVE - so the one thing the player needed to know was the one
            // thing nothing on screen told them.
            const bool zoneKind = obj->kind != ObjectiveKind::Convoy &&
                                  obj->kind != ObjectiveKind::Rampage &&
                                  obj->kind != ObjectiveKind::DestroyMarked &&
                                  obj->kind != ObjectiveKind::KillTarget &&
                                  obj->kind != ObjectiveKind::Blackout;
            if (zoneKind) marker(mission_.objectiveZone(), true);

            // And when the mark is behind you or off the side of the screen,
            // a chevron on the edge of the view pointing at it. Without this,
            // "which way" is answered only by a clock reading in a corner
            // panel, which is a sentence to read in the middle of a firefight.
            // A chevron on the rim of the view for anything the mission wants
            // you to find that is not currently on screen. Written once and
            // used for the objective zone AND for every marked target, because
            // "hunt the spotter" is unplayable if the spotter is a diamond
            // that only appears once you are already looking at it.
            // A Warden at work, drawn on the world: a green cross over whatever
            // it is repairing. Between this and the repair tick, "why is that
            // tank not dying" becomes a question with a visible answer
            // standing forty metres behind it.
            for (const Vec3& at : mission_.repairPulses()) {
                int px, py;
                if (!project(at + Vec3(0.0f, 3.2f, 0.0f), &px, &py)) continue;
                const Vec3 mend(0.42f, 1.0f, 0.55f);
                putCell(frame, px, py, '+', mend);
                putCell(frame, px - 1, py, '-', mend * 0.7f);
                putCell(frame, px + 1, py, '-', mend * 0.7f);
            }

            auto edgePointer = [&](const Vec3& at) {
                int mx, my;
                if (project(at + Vec3(0.0f, 6.0f, 0.0f), &mx, &my)) return;
                {
                    const Vec3 to = flattenY(at - p.position());
                    const Vec3 fwdC = normalize(flattenY(camFocus_ - camPos_) +
                                                Vec3(0.0f, 0.0f, 1e-4f));
                    const Vec3 rightC = normalize(cross(fwdC, Vec3(0.0f, 1.0f, 0.0f)));
                    const Vec3 t = normalize(to + Vec3(0.0f, 0.0f, 1e-4f));
                    const float fx = dot(t, rightC), fz = dot(t, fwdC);
                    // Place the chevron on the rim of an ellipse inscribed in
                    // the view, in the direction of the mark.
                    const float cx = static_cast<float>(W) * 0.5f;
                    const float cy = static_cast<float>(H) * 0.5f;
                    const float ang = std::atan2(fx, fz);
                    const int ex = static_cast<int>(cx + std::sin(ang) * (cx - 6.0f));
                    const int ey = static_cast<int>(cy - std::cos(ang) * (cy - 4.0f));
                    const char* g = (std::fabs(fx) > std::fabs(fz))
                                        ? (fx > 0.0f ? ">" : "<")
                                        : (fz > 0.0f ? "^" : "v");
                    const int dist = static_cast<int>(length(to));
                    drawText(frame, std::max(1, std::min(W - 8, ex - 1)),
                             std::max(0, std::min(H - 1, ey)),
                             inCabin() ? std::string(g)
                                       : std::string(g) + fmtInt(dist) + "m",
                             TextStyle{amber, false});
                }
            };
            if (zoneKind) edgePointer(mission_.objectiveZone());
            for (int idx : mission_.markedProps()) {
                const Destructible& d = mission_.props()[static_cast<size_t>(idx)];
                if (d.alive) { marker(d.hitCentre(), true); edgePointer(d.hitCentre()); }
            }
            for (int idx : mission_.markedUnits()) {
                const Unit& u = mission_.units()[static_cast<size_t>(idx)];
                if (u.alive()) { marker(u.hitCentre(), true); edgePointer(u.hitCentre()); }
            }
            for (int idx : mission_.markedMechs()) {
                if (idx <= 0 || idx >= static_cast<int>(mission_.mechs().size())) continue;
                const Mech& m2 = mission_.mechs()[static_cast<size_t>(idx)];
                if (m2.alive()) { marker(m2.hitCentre(), true); edgePointer(m2.hitCentre()); }
            }
            if (mission_.escortIndex() >= 0) {
                const Unit& esc = mission_.units()[static_cast<size_t>(mission_.escortIndex())];
                if (esc.alive()) marker(esc.position(), false);
            }
        }
    }

    // Gunner sight: rangefinder crosshairs and the magnification, drawn wide
    // so the scoped view reads as glass rather than a zoomed-in field.
    if (scopeStage_ > 0) {
        const Vec3 sc = kNorm.color;
        for (int i = 4; i < std::min(W / 2 - 2, 16); i += 2) {
            putCell(frame, cx - i, cy, '-', sc);
            putCell(frame, cx + i, cy, '-', sc);
        }
        for (int i = 2; i < std::min(H / 2 - 1, 6); ++i) {
            putCell(frame, cx, cy - i, '|', sc);
            putCell(frame, cx, cy + i, '|', sc);
        }
        drawText(frame, cx - 5, cy + std::min(H / 2 - 1, 6) + 1,
                 scopeStage_ == 2 ? "SIGHT x7" : "SIGHT x2.5", kBright);
        const SurfaceHit rh = mission_.world().raycast(camPos_,
                                                       normalize(camFocus_ - camPos_),
                                                       600.0f);
        if (rh.hit)
            drawText(frame, cx + 3, cy - 2,
                     fmtInt(static_cast<int>(rh.distance)) + "m", kNorm);
    }

    // Water: the one warning that ends runs.
    if (p.wading() > 0.05f && !inCabin()) {
        const bool deep = p.wading() > 0.8f;
        drawText(frame, cx - 6, cy + 4, deep ? "!! FLOODING !!" : "WADING",
                 deep ? kBad : kWarn);
    }

    // In the cabin the console's readouts are its own instruments - lit
    // gauges with lamp legends, drawn elsewhere - so all the HUD pass has
    // left to do is the two plates in the brow. No side panels: the glass
    // is for looking through.
    if (inCabin()) { drawCabinBrow(frame); return; }

    // ---- left column: machine status -------------------------------------
    // The whole column sits on one dark panel. Per-glyph backing keeps text
    // legible, but a column of readouts over a busy skyline still shimmered;
    // a solid card behind it is what finally makes the HUD read as a HUD.
    {
        int abilityRows = 0;
        for (int slot = 0; slot < 4; ++slot)
            if (p.slotAbility(slot) != Ability::None) ++abilityRows;
        const int rows = 9 + abilityRows + static_cast<int>(p.weapons().size());
        fillPanel(frame, 1, 0, 36, std::min(rows, H - 3),
                  Vec3(0.008f, 0.018f, 0.013f));
    }
    int y = 1;
    drawText(frame, 2, y++, "ACAH // " + level_.name, kBright);
    drawText(frame, 2, y++, mission_.world().arena().name, kDim);
    ++y;

    const float hpFrac = p.healthFraction();
    const TextStyle& hpStyle = hpFrac < 0.25f ? kBad : (hpFrac < 0.55f ? kWarn : kGood);
    drawText(frame, 2, y, "HULL", kDim);
    drawHorizontalBar(frame, 8, y, 18, hpFrac, hpStyle, kDim);
    drawText(frame, 27, y++, fmtInt(static_cast<int>(p.health())), hpStyle);

    const float heatFrac = clampf(p.heat() / std::max(p.stats().heatCapacity, 0.01f), 0.0f, 1.0f);
    const TextStyle& heatStyle = p.overheated() ? kBad : (heatFrac > 0.7f ? kWarn : kNorm);
    drawText(frame, 2, y, "HEAT", kDim);
    drawHorizontalBar(frame, 8, y, 18, heatFrac, heatStyle, kDim);
    if (p.overheated()) drawText(frame, 27, y, "OVERHEAT", kBad);
    ++y;

    // One ability per slot, each on its own key. Only fitted ones get a row,
    // so a bare build costs no screen space.
    {
        for (int slot = 0; slot < 4; ++slot) {
            if (p.slotAbility(slot) == Ability::None) continue;
            const bool engaged = p.abilityEngaged(slot);
            const bool ready = p.abilityReady(slot);
            const std::string keyLab = "[" +
                actionLabel(static_cast<Action>(static_cast<int>(Action::AbilityLegs) + slot)).substr(0, 3) + "]";
            drawText(frame, 2, y, keyLab, ready ? kBright : kDim);
            drawHorizontalBar(frame, 8, y, 18,
                              engaged ? 1.0f : 1.0f - p.abilityCooldownFraction(slot),
                              engaged ? kWarn : (ready ? kGood : kDim), kDim);
            drawText(frame, 27, y++, abilityName(p.slotAbility(slot)),
                     engaged ? kWarn : (ready ? kNorm : kDim));
        }
    }

    // Climb readout, shown only when it matters.
    const float climb = p.climbFraction();
    if (climb > 0.05f) {
        drawText(frame, 2, y, "GRIP", kDim);
        drawHorizontalBar(frame, 8, y, 18, clampf(climb, 0.0f, 1.0f), kBright, kDim);
        drawText(frame, 27, y++, "CLIMBING", kBright);
    } else if (p.jumpCharge() > 0.01f) {
        drawText(frame, 2, y, "JUMP", kDim);
        drawHorizontalBar(frame, 8, y, 18, clampf(p.jumpCharge(), 0.0f, 1.0f), kWarn, kDim);
        drawText(frame, 27, y++, "CHARGING", kWarn);
    } else {
        ++y;
    }
    ++y;

    // ---- weapons ---------------------------------------------------------
    drawText(frame, 2, y++, "-- ARMAMENT --", kDim);
    const std::vector<MountedWeapon>& ws = p.weapons();
    for (size_t i = 0; i < ws.size() && y < H - 6; ++i) {
        const MountedWeapon& w = ws[i];
        if (!w.part) { drawText(frame, 2, y++, "  [ EMPTY MOUNT ]", kDim); continue; }
        const WeaponDef& def = w.part->weapon;

        // [1L] = mount 1 on the left trigger; lowercase means switched off.
        std::string tag = "[";
        tag += static_cast<char>('1' + static_cast<int>(i));
        tag += w.enabled ? (w.group == 0 ? 'L' : 'R') : '-';
        tag += "] ";
        std::string line = tag + w.part->name;
        if (line.size() > 26) line = line.substr(0, 26);
        const bool ready = w.enabled && w.cooldown <= 0.01f &&
                           (def.ammo != AmmoKind::Limited || w.rounds > 0);
        drawText(frame, 2, y, line, !w.enabled ? kDim : (ready ? kNorm : kDim));

        std::string ammo;
        switch (def.ammo) {
            case AmmoKind::Unlimited: ammo = "INF"; break;
            case AmmoKind::Cooldown:
                ammo = w.cooldown > 0.01f ? fmt(w.cooldown, 1) + "s" : "RDY";
                break;
            case AmmoKind::Limited:
                ammo = fmtInt(w.rounds) + "+" + fmtInt(w.reserve);
                break;
        }
        const TextStyle& as = (def.ammo == AmmoKind::Limited && w.rounds == 0 && w.reserve == 0)
                                  ? kBad : (ready ? kNorm : kDim);
        drawText(frame, 30, y, ammo, as);
        // Handling, in three characters at the end of the line. A gun that
        // has to spool shows how far up it is; a gun whose group is walking
        // shows how far out. Both are live state the player is expected to
        // manage and neither had any representation at all.
        if (def.spinUp > 0.0f && w.spool > 0.02f) {
            const int lit = static_cast<int>(w.spool * 3.0f + 0.5f);
            for (int k = 0; k < 3; ++k)
                putCell(frame, 26 + k, y, k < lit ? '>' : '-',
                        (w.spool > 0.95f ? kGood : kWarn).color);
        } else if (w.bloom > 0.05f) {
            const int lit = static_cast<int>(clampf(w.bloom / 2.2f, 0.0f, 1.0f) * 3.0f + 0.5f);
            for (int k = 0; k < 3; ++k)
                putCell(frame, 26 + k, y, k < lit ? '^' : '-',
                        (w.bloom > 1.2f ? kBad : kWarn).color);
        }
        ++y;
    }

    // (The radar is drawn as pixels on the scene grid - see drawPixelRadar.)

    // ---- right column: mission state -------------------------------------
    fillPanel(frame, W - 29, 0, 28, 13, Vec3(0.008f, 0.018f, 0.013f));
    int ry = 1;
    drawTextRight(frame, W - 2, ry++, money(profile_.cash + mission_.cashEarned()) + " cr", kBright);

    // The objective is the mission: what to do, how far along it is, how
    // long is left, and which way the marked position lies.
    if (const ObjectiveSpec* obj = mission_.currentObjective()) {
        drawTextRight(frame, W - 2, ry++,
                      "OBJECTIVE " + fmtInt(mission_.objectiveIndex() + 1) + "/" +
                          fmtInt(mission_.objectiveCount()), kNorm);
        drawTextRight(frame, W - 2, ry++, obj->label, kBright);
        // What the label MEANS. Mission labels are written as orders a
        // commander would give - "SILENCE THE HIGH GUNS" - which reads well
        // and tells a first-time player nothing about what ends the segment.
        // One plain line underneath says what actually finishes it.
        {
            const char* how = "";
            switch (obj->kind) {
                case ObjectiveKind::ClearHostiles:
                    how = obj->advanceOnReach ? "advance to the mark or break the guns"
                                              : "destroy the armour holding this ground";
                    break;
                case ObjectiveKind::DestroyMarked: how = "wreck the marked structures"; break;
                case ObjectiveKind::ReachZone:     how = "get to the mark"; break;
                case ObjectiveKind::Escort:        how = "keep the crawler alive"; break;
                case ObjectiveKind::HoldZone:      how = "stand on the mark until the clock runs out"; break;
                case ObjectiveKind::Rampage:       how = "destroy everything you can, fast"; break;
                case ObjectiveKind::KillTarget:    how = "kill the marked machine"; break;
                case ObjectiveKind::Convoy:        how = "stop the column before it escapes"; break;
                case ObjectiveKind::Blackout:      how = "drop the marked masts"; break;
                case ObjectiveKind::Breakthrough:  how = "cross the line to the mark"; break;
                case ObjectiveKind::KillUnits:
                    how = obj->killKind == UnitKind::ShieldPylon ? "destroy the shield pylons - nothing near them can be hurt"
                                                                 : "destroy the marked emplacements";
                    break;
                case ObjectiveKind::Outrun:        how = "the barrage is walking up behind you - beat it to the mark"; break;
            }
            if (*how) drawTextRight(frame, W - 2, ry++, how, kDim);
            if (obj->gateHealth > 0.0f)
                drawTextRight(frame, W - 2, ry++, "the gate is shielded while a pylon stands", kDim);
            const std::string cs = mission_.eliteCallsign();
            if (!cs.empty()) drawTextRight(frame, W - 2, ry++, "TARGET: " + cs, kWarn);
            if (mission_.collapseCountdown() >= 0.0f)
                drawTextRight(frame, W - 2, ry++,
                              "CHARGES ARMED  T-" + fmt(mission_.collapseCountdown(), 0) + "  KEEP MOVING",
                              kBad);
            if (obj->kind == ObjectiveKind::Outrun) {
                const float lead = dot(p.position(), mission_.missionAxisDir()) - mission_.barrageAlong();
                drawTextRight(frame, W - 2, ry++,
                              lead > 0.0f ? "BARRAGE " + fmtInt(static_cast<int>(lead)) + "m BEHIND"
                                          : std::string("UNDER THE BARRAGE"),
                              lead > 40.0f ? kWarn : kBad);
            }
            if (mission_.blackout() > 0.5f)
                drawTextRight(frame, W - 2, ry++, "SECTOR DARK - RADAR ONLY", kDim);
        }
        if (mission_.objectiveTarget() > 0)
            drawTextRight(frame, W - 2, ry++,
                          fmtInt(mission_.objectiveProgress()) + " / " +
                              fmtInt(mission_.objectiveTarget()), kWarn);
        if (obj->timer > 0.0f)
            drawTextRight(frame, W - 2, ry++,
                          "T-" + fmt(std::max(0.0f, mission_.objectiveTimer()), 1),
                          mission_.objectiveTimer() < 20.0f ? kBad : kWarn);
        // Bearing to the zone for the go-there objectives.
        // The bearing, for everything with a place to be - which, now that a
        // march segment advances on arrival, is nearly everything.
        if (obj->kind != ObjectiveKind::Convoy &&
            obj->kind != ObjectiveKind::Rampage &&
            obj->kind != ObjectiveKind::DestroyMarked &&
            obj->kind != ObjectiveKind::KillTarget &&
            obj->kind != ObjectiveKind::Blackout) {
            const Vec3 to = mission_.objectiveZone() - p.position();
            const float dist = length(flattenY(to));
            const Vec3 fwdC = normalize(camFocus_ - camPos_);
            const Vec3 flatF = normalize(flattenY(fwdC) + Vec3(0.0f, 0.0f, 1e-4f));
            const Vec3 flatT = normalize(flattenY(to) + Vec3(0.0f, 0.0f, 1e-4f));
            const float ang = std::atan2(cross(flatF, flatT).y, dot(flatF, flatT));
            int clock = static_cast<int>(std::round(ang / (PI / 6.0f)));
            clock = ((clock % 12) + 12) % 12;
            if (clock == 0) clock = 12;
            drawTextRight(frame, W - 2, ry++,
                          "MARK " + fmtInt(clock) + " O'CLOCK " +
                              fmtInt(static_cast<int>(dist)) + "m", kNorm);
        }
        // Jamming has to announce itself or it reads as the game being broken:
        // the reticle stops helping and the radar starts lying, and a player
        // with no explanation concludes the HUD is buggy rather than that
        // there is a vehicle out there doing it to them.
        if (mission_.jamStrength() > 0.02f)
            drawTextRight(frame, W - 2, ry++,
                          mission_.jamStrength() > 0.55f
                              ? "! JAMMED - KILL THE JAMMER"
                              : "! SIGNAL DEGRADED",
                          kBad);
        if (mission_.alarmLevel() > 0.05f)
            drawTextRight(frame, W - 2, ry++,
                          std::string("ALARM ") +
                              (mission_.alarmLevel() > 0.6f ? "HIGH" : "RISING"),
                          mission_.alarmLevel() > 0.6f ? kBad : kWarn);
    }
    {
        int hostiles = mission_.enemiesAlive();
        for (const Unit& u : mission_.units())
            if (u.alive() && u.team() == Team::Hostile) ++hostiles;
        drawTextRight(frame, W - 2, ry++, "HOSTILES " + fmtInt(hostiles),
                      hostiles > 0 ? kWarn : kGood);
    }
    ++ry;

    // Threat bearing: which way the nearest enemy is, as a clock direction
    // relative to where you are looking.
    if (const Mech* e = mission_.nearestEnemy()) {
        const Vec3 to = e->position() - p.position();
        const float dist = length(to);
        const Vec3 fwd = normalize(camFocus_ - camPos_);
        const Vec3 flatF = normalize(flattenY(fwd) + Vec3(0.0f, 0.0f, 1e-4f));
        const Vec3 flatT = normalize(flattenY(to) + Vec3(0.0f, 0.0f, 1e-4f));
        const float ang = std::atan2(cross(flatF, flatT).y, dot(flatF, flatT));
        int clock = static_cast<int>(std::round(ang / (PI / 6.0f)));
        clock = ((clock % 12) + 12) % 12;
        if (clock == 0) clock = 12;
        drawTextRight(frame, W - 2, ry++,
                      "THREAT " + fmtInt(clock) + " O'CLOCK", kWarn);
        drawTextRight(frame, W - 2, ry++, fmtInt(static_cast<int>(dist)) + "m", kDim);
        if (e->onWall()) drawTextRight(frame, W - 2, ry++, "* ON STRUCTURE *", kBad);
    }

    // ---- bottom bar ------------------------------------------------------
    const std::string state = mechStateName(p.state());
    drawText(frame, 2, H - 2, state + "  " + fmt(p.speed(), 1) + " m/s", kDim);
    drawText(frame, 2, H - 1,
             "[" + actionLabel(Action::Scope) + "] SIGHT [" + actionLabel(Action::ToggleView) +
                 "] DRIVE [" + actionLabel(Action::AbilityLegs) + "/" + actionLabel(Action::AbilityEngine) +
                 "/" + actionLabel(Action::AbilityArmor) + "/" + actionLabel(Action::AbilitySensor) +
                 "] SYS [" + actionLabel(Action::Mount1) + "-" + actionLabel(Action::Mount4) + "] GUNS",
             kDim);
    drawTextRight(frame, W - 2, H - 2,
                  std::string(paletteName(ascii_.palette)) + " | " +
                      rampName(ascii_.ramp) + " | " +
                      backgroundName(ascii_.background) + " | " +
                      fmt(frameMs_ > 0.0f ? 1000.0f / frameMs_ : 0.0f, 0) + " FPS",
                  kDim);
    // Audio health: v = voices in flight, g = worst recent limiter gain
    // (1.00 means the limiter never touched the mix), u = device underruns
    // since launch. If the sound "cuts out" and u climbs, the OS is starving
    // the device; if g dives instead, the mix is eating its own headroom.
    drawTextRight(frame, W - 2, H - 1,
                  "AUD v" + std::to_string(audVoices_) + " g" +
                      fmt(audLimFloor_, 2) + " u" + std::to_string(audUnderruns_),
                  kDim);
}

// ------------------------------------------------------------------- cabin

// The brow over the windscreen carries the two readouts that are about the
// job rather than about the machine - the control plate and the contract -
// and those stay as text on the HUD grid, painted inside the projected
// panels. It is the CONSOLE, under the glass, that had to stop being text:
// see drawCabinLamps.
void Game::drawCabinBrow(AsciiFrame& frame) {
    const int W = frame.w, H = frame.h;
    const Mech& p = mission_.player();
    CabinLayout lay = cabinLayout_;
    lay.resolve(cam_);

    // Camera space - the cabin's own frame, body sway included - to a HUD
    // cell, so the plates ride with the panelling.
    const Vec3 sway = cabinSway_;
    auto project = [&](const Vec3& local, float* sx, float* sy) {
        const Vec3 world = cam_.pos + cam_.right * (local.x + sway.x) +
                           cam_.up * (local.y + sway.y) +
                           cam_.forward * (local.z + sway.z);
        const Vec4 clip = transform(cam_.viewProj, Vec4(world, 1.0f));
        if (clip.w < 0.02f) return false;
        *sx = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(W);
        *sy = (0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(H);
        return true;
    };
    // A panel's usable face, in cells: the INNER bounds of its projected
    // quad, inset by a cell so text never rides up onto the bezel.
    struct Face { int x = 0, y = 0, w = 0, h = 0; bool ok = false; };
    auto faceOf = [&](const CabinPanel& panel) {
        Vec3 c[4];
        panel.corners(c);
        float sx[4], sy[4];
        Face f;
        for (int i = 0; i < 4; ++i)
            if (!project(c[i], &sx[i], &sy[i])) return f;
        const float x0 = std::max(sx[0], sx[3]), x1 = std::min(sx[1], sx[2]);
        const float y0 = std::max(sy[0], sy[1]), y1 = std::min(sy[2], sy[3]);
        f.x = std::max(0, static_cast<int>(std::ceil(x0)) + 1);
        f.y = std::max(0, static_cast<int>(std::ceil(y0)));
        f.w = std::min(W - f.x, static_cast<int>(x1) - f.x);
        f.h = std::min(H - f.y, static_cast<int>(y1) - f.y);
        f.ok = f.w >= 10 && f.h >= 1;
        return f;
    };
    auto line = [&](const Face& f, int row, int col, const std::string& s,
                    const TextStyle& style) {
        if (!f.ok || row < 0 || row >= f.h || col < 0 || col >= f.w) return;
        const size_t room = static_cast<size_t>(f.w - col);
        drawText(frame, f.x + col, f.y + row,
                 s.size() > room ? s.substr(0, room) : s, style);
    };
    auto lineRight = [&](const Face& f, int row, const std::string& s,
                         const TextStyle& style) {
        if (!f.ok || row < 0 || row >= f.h) return;
        const int col = f.w - static_cast<int>(s.size());
        if (col < 0) return;
        drawText(frame, f.x + col, f.y + row, s, style);
    };
    // Which way something is, as a clock face from where the pilot is looking.
    auto clockTo = [&](const Vec3& to) {
        const Vec3 flatF = normalize(flattenY(camOrbitDir_) + Vec3(0.0f, 0.0f, 1e-4f));
        const Vec3 flatT = normalize(flattenY(to) + Vec3(0.0f, 0.0f, 1e-4f));
        const float ang = std::atan2(cross(flatF, flatT).y, dot(flatF, flatT));
        int c = static_cast<int>(std::round(ang / (PI / 6.0f)));
        c = ((c % 12) + 12) % 12;
        return c == 0 ? 12 : c;
    };

    // ---- brow left: the control plate -------------------------------------
    // Read by looking for the KEY you are about to press, not for the word,
    // so the key comes first on every entry. Built from the live bindings, so
    // a rebound control relabels the plate the pilot is looking at.
    {
        const Face f = faceOf(lay.browL);
        auto k = [&](Action a) { return actionLabelShort(a); };
        const std::pair<std::string, std::string> entries[] = {
            {k(Action::Forward) + k(Action::StrafeLeft) + k(Action::Back) +
                 k(Action::StrafeRight), "DRIVE"},
            {k(Action::FireLeft) + "/" + k(Action::FireRight), "GUNS"},
            {k(Action::AbilityLegs) + k(Action::AbilityEngine) + k(Action::AbilityArmor) +
                 k(Action::AbilitySensor), "SYS"},
            {k(Action::Mount1) + "-" + k(Action::Mount4), "MOUNTS"},
            {k(Action::Jump), "JUMP"},
            {k(Action::Boost), "BOOST"},
            {k(Action::Scope), "SIGHT"},
            {k(Action::ToggleView), "VIEW"},
            {padActive_ ? padLabel(4) : std::string("O"), "CONTROLS"},
        };
        const int n = static_cast<int>(sizeof(entries) / sizeof(entries[0]));
        size_t keyW = 0, labW = 0;
        for (const auto& e : entries) {
            keyW = std::max(keyW, e.first.size());
            labW = std::max(labW, e.second.size());
        }
        const int colW = static_cast<int>(keyW + labW) + 3;
        if (f.ok && colW > 0) {
            const int cols = std::max(1, std::min(4, f.w / colW));
            const int rows = std::max(1, std::min(f.h, (n + cols - 1) / cols));
            for (int i = 0; i < n; ++i) {
                const int col = i / rows, row = i % rows;
                if (col >= cols) break;
                const int x = col * colW;
                std::string key = entries[i].first;
                key.resize(keyW, ' ');
                line(f, row, x, key, kBright);
                line(f, row, x + static_cast<int>(keyW) + 1, entries[i].second, kDim);
            }
        }
    }

    // ---- brow right: the contract, and what is shooting at you ------------
    {
        const Face f = faceOf(lay.browR);
        line(f, 0, 0, level_.name, kBright);
        lineRight(f, 0, money(profile_.cash + mission_.cashEarned()) + " CR", kDim);
        if (const ObjectiveSpec* obj = mission_.currentObjective()) {
            line(f, 1, 0, fmtInt(mission_.objectiveIndex() + 1) + "/" +
                          fmtInt(mission_.objectiveCount()) + " " + obj->label, kWarn);
            std::string right;
            if (mission_.objectiveTarget() > 0)
                right += fmtInt(mission_.objectiveProgress()) + "/" +
                         fmtInt(mission_.objectiveTarget());
            if (obj->timer > 0.0f)
                right += (right.empty() ? "" : "  ") + std::string("T-") +
                         fmt(std::max(0.0f, mission_.objectiveTimer()), 0);
            const std::string cs = mission_.eliteCallsign();
            if (!cs.empty()) right = cs + (right.empty() ? "" : "  ") + right;
            if (!right.empty()) lineRight(f, 1, right, kNorm);
            const bool zoneKind = obj->kind != ObjectiveKind::Convoy &&
                                  obj->kind != ObjectiveKind::Rampage &&
                                  obj->kind != ObjectiveKind::DestroyMarked &&
                                  obj->kind != ObjectiveKind::KillTarget &&
                                  obj->kind != ObjectiveKind::Blackout;
            if (zoneKind) {
                const Vec3 to = mission_.objectiveZone() - p.position();
                line(f, 2, 0, "MARK " + fmtInt(clockTo(to)) + " O'C " +
                              fmtInt(static_cast<int>(length(flattenY(to)))) + "m", kNorm);
            }
        }
        // Contacts: how many, and where the nearest machine is.
        {
            int hostiles = mission_.enemiesAlive();
            for (const Unit& u : mission_.units())
                if (u.alive() && u.team() == Team::Hostile) ++hostiles;
            std::string t = fmtInt(hostiles) + " HOSTILE";
            if (const Mech* e = mission_.nearestEnemy()) {
                const Vec3 to = e->position() - p.position();
                t += "  MECH " + fmtInt(clockTo(to)) + " O'C " +
                     fmtInt(static_cast<int>(length(to))) + "m";
                if (e->onWall()) t += " UP";
            }
            lineRight(f, f.h > 2 ? 2 : 1, t, hostiles > 0 ? kWarn : kGood);
        }
        // Whatever is currently going wrong takes the bottom row.
        {
            std::string w;
            const TextStyle* ws = &kWarn;
            if (mission_.collapseCountdown() >= 0.0f) {
                w = "CHARGES ARMED T-" + fmt(mission_.collapseCountdown(), 0) + " KEEP MOVING";
                ws = &kBad;
            } else if (const ObjectiveSpec* o2 = mission_.currentObjective()) {
                if (o2->kind == ObjectiveKind::Outrun) {
                    const float lead = dot(p.position(), mission_.missionAxisDir()) -
                                       mission_.barrageAlong();
                    w = lead > 0.0f ? "BARRAGE " + fmtInt(static_cast<int>(lead)) + "m BEHIND"
                                    : std::string("UNDER THE BARRAGE");
                    ws = lead > 40.0f ? &kWarn : &kBad;
                }
            }
            if (w.empty() && p.wading() > 0.05f) {
                w = p.wading() > 0.8f ? "!! FLOODING !!" : "WADING";
                ws = p.wading() > 0.8f ? &kBad : &kWarn;
            }
            if (w.empty() && p.healthFraction() < 0.30f && p.alive()) {
                w = "STRUCTURE CRITICAL - FIND AMBER SALVAGE";
                ws = &kBad;
            }
            if (w.empty() && mission_.jamStrength() > 0.02f) {
                w = mission_.jamStrength() > 0.55f ? "JAMMED" : "SIGNAL DEGRADED";
                ws = &kBad;
            }
            if (w.empty() && mission_.blackout() > 0.5f) {
                w = "SECTOR DARK - RADAR ONLY";
                ws = &kDim;
            }
            if (w.empty() && mission_.alarmLevel() > 0.05f) {
                w = mission_.alarmLevel() > 0.6f ? "ALARM HIGH" : "ALARM RISING";
                ws = mission_.alarmLevel() > 0.6f ? &kBad : &kWarn;
            }
            if (!w.empty()) line(f, f.h > 3 ? 3 : 2, 0, w, *ws);
        }
    }
}

// ---------------------------------------------------------------- the lamps
//
// The console's legends, drawn as LAMPS on the SCENE grid - the same trick
// the radar uses. Each dot of a 3x5 matrix character is a whole scene cell
// painted solid, so the lettering is exactly as crisp as HUD text is, while
// its POSITION comes from projecting the instrument face: it leans, slides
// and sways with the console it is bolted to instead of snapping around on
// the coarse HUD grid a third of the way across the screen.
void Game::drawCabinLamps(AsciiFrame& out) {
    const Mech& p = mission_.player();
    CabinLayout lay = cabinLayout_;
    lay.resolve(cam_);
    const Vec3 sway = cabinSway_;
    const int W = out.w, H = out.h;
    auto project = [&](const Vec3& local, float* sx, float* sy) {
        const Vec3 world = cam_.pos + cam_.right * (local.x + sway.x) +
                           cam_.up * (local.y + sway.y) +
                           cam_.forward * (local.z + sway.z);
        const Vec4 clip = transform(cam_.viewProj, Vec4(world, 1.0f));
        if (clip.w < 0.02f) return false;
        *sx = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(W);
        *sy = (0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(H);
        return true;
    };
    // A face, in scene cells: the inner bounds of its projected quad.
    struct Face {
        float x0 = 0.0f, x1 = 0.0f, yTop = 0.0f, yBot = 0.0f;
        bool ok = false;
        float x(float u) const { return x0 + (u + 1.0f) * 0.5f * (x1 - x0); }
        float y(float v) const { return yBot - (v + 1.0f) * 0.5f * (yBot - yTop); }
        float w() const { return x1 - x0; }
        float h() const { return yBot - yTop; }
    };
    auto faceOf = [&](const CabinPanel& panel) {
        Vec3 c[4];
        panel.corners(c);
        float sx[4], sy[4];
        Face f;
        for (int i = 0; i < 4; ++i)
            if (!project(c[i], &sx[i], &sy[i])) return f;
        f.x0 = std::max(sx[0], sx[3]);
        f.x1 = std::min(sx[1], sx[2]);
        f.yTop = std::max(sy[0], sy[1]);
        f.yBot = std::min(sy[2], sy[3]);
        f.ok = f.w() > 24.0f && f.h() > 10.0f;
        return f;
    };
    // One lit bulb: a block of whole cells, painted as background colour the
    // way the radar paints its screen. Whole cells are the entire point - a
    // lamp that lands across a cell boundary splits its light between two
    // characters and the legend turns to mud.
    auto bulb = [&](int px, int py, int dw, int dh, const Vec3& c) {
        for (int yy = 0; yy < dh; ++yy)
            for (int xx = 0; xx < dw; ++xx) {
                const int x = px + xx, y = py + yy;
                if (x < 0 || y < 0 || x >= W || y >= H) continue;
                Cell& cell = out.at(x, y);
                cell.ch = ' ';
                cell.r = cell.g = cell.b = 0;
                cell.br = static_cast<uint8_t>(clampf(c.x, 0.0f, 1.0f) * 255.0f);
                cell.bg = static_cast<uint8_t>(clampf(c.y, 0.0f, 1.0f) * 255.0f);
                cell.bb = static_cast<uint8_t>(clampf(c.z, 0.0f, 1.0f) * 255.0f);
                if (!(cell.br | cell.bg | cell.bb)) cell.bg = 8;
            }
    };

    // Lamp colours. These are what a lit bulb looks like, not palette
    // entries: the console is a dark place and a legend is meant to glow.
    const Vec3 cLabel(0.34f, 0.62f, 0.48f);
    const Vec3 cNorm(0.52f, 0.96f, 0.66f);
    const Vec3 cBright(0.80f, 1.00f, 0.88f);
    const Vec3 cGood(0.46f, 1.00f, 0.56f);
    const Vec3 cWarn(1.00f, 0.76f, 0.30f);
    const Vec3 cBad(1.00f, 0.40f, 0.32f);
    const Vec3 cCool(0.50f, 0.80f, 1.00f);

    // How big a bulb is on a given face: as large as the face's height will
    // allow `rows` lines of five bulbs and a gap, never smaller than one
    // cell. A bulb is twice as wide as it is tall in CELLS, which is square
    // in pixels, so the letterforms keep their proportions.
    struct Grid { int dw = 2, dh = 1, adv = 7; };
    auto gridFor = [](const Face& f, int rows) {
        Grid g;
        const int unit = std::max(1, static_cast<int>(f.h()) /
                                         std::max(1, rows * (kLedGlyphH + 1)));
        g.dh = unit;
        g.dw = unit * 2;
        g.adv = g.dw * kLedGlyphW + unit;    // one unit of daylight between
        return g;
    };
    // A legend, laid down from a face coordinate. `align` is -1 for the left
    // edge at u, +1 for the right edge at u. Anything that would not fit
    // between `uMin` and `uMax` is simply not shown: a console legend never
    // spills over the instrument next to it.
    auto lamps = [&](const Face& f, const Grid& g, float u, float v, int align,
                     const std::string& text, const Vec3& colour,
                     float uMin, float uMax) {
        if (!f.ok || text.empty()) return;
        const int len = static_cast<int>(text.size());
        const float wide = static_cast<float>(len * g.adv - g.dh);
        float x = f.x(u);
        if (align > 0) x -= wide;
        const float lo = f.x(uMin), hi = f.x(uMax);
        if (x < lo) x = lo;
        if (x + wide > hi) return;
        const int px = static_cast<int>(x + 0.5f);
        const int py = static_cast<int>(f.y(v) + 0.5f) -
                       (kLedGlyphH * g.dh) / 2;
        for (int i = 0; i < len; ++i) {
            const uint8_t* gl = ledGlyph(text[static_cast<size_t>(i)]);
            for (int gy = 0; gy < kLedGlyphH; ++gy) {
                if (gl[gy] == 0) continue;
                for (int gx = 0; gx < kLedGlyphW; ++gx) {
                    if (((gl[gy] >> (kLedGlyphW - 1 - gx)) & 1) == 0) continue;
                    bulb(px + i * g.adv + gx * g.dw, py + gy * g.dh,
                         g.dw, g.dh, colour);
                }
            }
        }
    };
    // How many characters fit between two face coordinates.
    auto room = [](const Face& f, const Grid& g, float u0, float u1) {
        const float span = (u1 - u0) * 0.5f * f.w();
        return std::max(0, static_cast<int>((span + static_cast<float>(g.dh)) /
                                            static_cast<float>(g.adv)));
    };
    auto fit = [](std::string s, int chars) {
        if (chars <= 0) return std::string();
        if (static_cast<int>(s.size()) > chars) s.resize(static_cast<size_t>(chars));
        return s;
    };

    // The face coordinates the console's fittings are laid out on. These
    // match cabin.cpp's instrument pass exactly; if one moves the other has
    // to move with it, which is why they are named the same thing in both.
    const float uLab0 = -0.97f, uLab1 = -0.50f;
    const float uNum0 = 0.42f, uNum1 = 0.97f;

    // ---- console left: structure, heat, and the systems fitted -----------
    {
        const Face f = faceOf(lay.dashL);
        int abilities = 0;
        for (int slot = 0; slot < 4; ++slot)
            if (p.slotAbility(slot) != Ability::None) ++abilities;
        const int n = cabinRowCount(2 + abilities);
        const Grid g = gridFor(f, n);
        const int labW = room(f, g, uLab0, uLab1);
        const int numW = room(f, g, uNum0, uNum1);

        const float hpFrac = p.healthFraction();
        const Vec3 hpc = hpFrac < 0.25f ? cBad : (hpFrac < 0.55f ? cWarn : cGood);
        float v = cabinRowV(0, n);
        lamps(f, g, uLab0, v, -1, fit("HULL", labW), cLabel, uLab0, uLab1);
        lamps(f, g, uNum1, v, 1, fit(fmtInt(static_cast<int>(p.health())), numW),
              hpc, uNum0, uNum1);

        const float heatFrac = clampf(p.heat() / std::max(p.stats().heatCapacity, 0.01f),
                                      0.0f, 1.0f);
        const Vec3 htc = p.overheated() ? cBad : (heatFrac > 0.7f ? cWarn : cCool);
        v = cabinRowV(1, n);
        lamps(f, g, uLab0, v, -1, fit("HEAT", labW), cLabel, uLab0, uLab1);
        lamps(f, g, uNum1, v, 1,
              fit(p.overheated() ? std::string("OVR")
                                 : fmtInt(static_cast<int>(heatFrac * 100.0f + 0.5f)),
                  numW),
              htc, uNum0, uNum1);

        // One line per system this machine carries, against its own lamp.
        // The key it answers to leads the line, because a system nobody can
        // find the button for is not fitted in any useful sense.
        int k = 0;
        for (int slot = 0; slot < 4 && 2 + k < n; ++slot) {
            if (p.slotAbility(slot) == Ability::None) continue;
            v = cabinRowV(2 + k, n);
            const bool on = p.abilityEngaged(slot), ready = p.abilityReady(slot);
            const Vec3 c = on ? cWarn : (ready ? cBright : cLabel);
            const std::string key = actionLabelShort(
                static_cast<Action>(static_cast<int>(Action::AbilityLegs) + slot));
            const std::string status =
                on ? std::string("ON") : ready ? std::string("RDY")
                                               : fmt(p.abilityCooldownSeconds(slot), 0);
            lamps(f, g, uNum1, v, 1, fit(status, numW), c, uNum0, uNum1);
            lamps(f, g, -0.78f, v, -1,
                  fit(key + " " + abilityName(p.slotAbility(slot)),
                      room(f, g, -0.78f, uNum0 - 0.04f)),
                  c, -0.78f, uNum0 - 0.04f);
            ++k;
        }
    }

    // ---- console right: the drive and the guns ---------------------------
    {
        const Face f = faceOf(lay.dashR);
        const std::vector<MountedWeapon>& ws = p.weapons();
        std::vector<int> fitted;
        for (size_t i = 0; i < ws.size() && fitted.size() < 6; ++i)
            if (ws[i].part) fitted.push_back(static_cast<int>(i));
        const int guns = static_cast<int>(fitted.size());
        const int n = cabinRowCount(2 + guns);
        const Grid g = gridFor(f, n);
        const int labW = room(f, g, uLab0, uLab1);
        const int numW = room(f, g, uNum0, uNum1);

        // The speedometer, which is a fitting like everything else: a lit
        // dial on the panel with its own digits in a sunk window.
        float v = cabinRowV(0, n);
        lamps(f, g, uLab0, v, -1, fit("SPD", labW), cLabel, uLab0, uLab1);
        lamps(f, g, uNum1, v, 1, fit(fmtInt(static_cast<int>(p.speed() + 0.5f)), numW),
              cBright, uNum0, uNum1);

        const bool climbing = p.climbFraction() > 0.05f;
        v = cabinRowV(1, n);
        lamps(f, g, uLab0, v, -1, fit(climbing ? "GRIP" : "JUMP", labW), cLabel,
              uLab0, uLab1);
        lamps(f, g, uNum1, v, 1,
              fit(fmtInt(static_cast<int>((climbing ? p.climbFraction()
                                                    : p.jumpCharge()) * 100.0f)),
                  numW),
              climbing ? cBright : (p.jumpCharge() > 0.99f ? cGood : cWarn),
              uNum0, uNum1);

        // One line per mount, against its own lamp: which trigger it is on,
        // what is bolted there, and what it has left.
        for (int i = 0; i < guns && 2 + i < n; ++i) {
            v = cabinRowV(2 + i, n);
            const int mount = fitted[static_cast<size_t>(i)];
            const MountedWeapon& w = ws[static_cast<size_t>(mount)];
            const WeaponDef& def = w.part->weapon;
            std::string ammo;
            switch (def.ammo) {
                case AmmoKind::Unlimited: ammo = "INF"; break;
                case AmmoKind::Cooldown:
                    ammo = w.cooldown > 0.01f ? fmt(w.cooldown, 1) : std::string("RDY");
                    break;
                case AmmoKind::Limited: ammo = fmtInt(w.rounds); break;
            }
            if (def.spinUp > 0.0f && w.spool > 0.02f)
                ammo = fmtInt(static_cast<int>(w.spool * 100.0f));
            const bool dry = def.ammo == AmmoKind::Limited && w.rounds == 0 && w.reserve == 0;
            const Vec3 c = !w.enabled ? cLabel : (dry ? cBad : cNorm);
            const std::string tag = fmtInt(mount + 1) + (w.group == 0 ? "L" : "R");
            // A gun row has no gauge across the middle of it, so its name
            // gets the whole panel between the lamp and the ammo window -
            // which is the difference between a name and an abbreviation.
            lamps(f, g, uNum1, v, 1, fit(ammo, 4), c, 0.60f, uNum1);
            lamps(f, g, -0.86f, v, -1,
                  fit(tag + " " + w.part->name, room(f, g, -0.86f, 0.56f)),
                  c, -0.86f, 0.56f);
        }
    }
}

// ---------------------------------------------------------------- briefing

void Game::drawBriefing(AsciiFrame& frame) {
    const int W = frame.w, H = frame.h;
    if (W < 40 || H < 14) return;

    const int boxW = std::min(W - 8, 68);
    const int wrap = boxW - 6;

    // Lay the content out first, THEN size the box around it. The old fixed
    // sixteen-row box printed the objective list at a fixed row while the
    // wrapped briefing was still using it, and the two braided into garbage.
    std::vector<std::pair<std::string, int>> lines;   // text, style: 0 dim 1 norm 2 bright 3 warn
    lines.push_back({"MISSION " + fmtInt(profile_.level + 1), 0});
    lines.push_back({level_.name, 2});
    lines.push_back({"", 0});
    lines.push_back({mission_.world().arena().name + "  -  " +
                     mission_.world().arena().subtitle, 1});
    lines.push_back({std::string("VERTICALITY ") +
                     mission_.world().arena().verticality, 0});
    // What the job is worth, before you walk into it. Without this the only
    // way to plan a purchase was to finish a contract and find out - which
    // makes saving up for a part a guess rather than a decision, and saving
    // up is the whole shape of the campaign now that missions pay less.
    {
        const bool replay = profile_.level <= profile_.maxCleared;
        std::string fee = "CONTRACT FEE " + money(level_.completionBonus) +
                          " cr PLUS SALVAGE";
        if (replay) fee += "  (RERUN: QUARTER RATE)";
        lines.push_back({fee, replay ? 3 : 1});
    }
    lines.push_back({"", 0});

    std::string rest = level_.briefing;
    while (!rest.empty()) {
        size_t cut = rest.size();
        if (static_cast<int>(cut) > wrap) {
            cut = static_cast<size_t>(wrap);
            while (cut > 0 && rest[cut] != ' ') --cut;
            if (cut == 0) cut = static_cast<size_t>(wrap);
        }
        lines.push_back({rest.substr(0, cut), 1});
        rest = (cut < rest.size()) ? rest.substr(cut + 1) : std::string();
    }
    lines.push_back({"", 0});
    for (int i2 = 0; i2 < mission_.objectiveCount(); ++i2) {
        const ObjectiveSpec& o = level_.objectives[static_cast<size_t>(i2)];
        lines.push_back({fmtInt(i2 + 1) + ". " + o.label, i2 == 0 ? 3 : 0});
    }
    lines.push_back({"", 0});

    const int boxH = std::min(H - 2, static_cast<int>(lines.size()) + 4);
    const int x = (W - boxW) / 2;
    const int y = std::max(1, (H - boxH) / 2);

    fillPanel(frame, x, y, boxW, boxH);
    drawRect(frame, x, y, boxW, boxH, kDim);

    int ly = y + 1;
    for (const auto& ln : lines) {
        if (ly >= y + boxH - 3) break;
        static const TextStyle* styles[4] = {&kDim, &kNorm, &kBright, &kWarn};
        if (!ln.first.empty())
            drawText(frame, x + 3, ly, ln.first, *styles[ln.second]);
        ++ly;
    }

    const bool blink = std::fmod(elapsed_, 1.2f) < 0.7f;
    drawText(frame, x + 3, y + boxH - 2,
             blink ? hk("ENTER", 0) + " DEPLOY   " + hk("BKSP", 1) + " WORKSHOP" : std::string(), kBright);
    if (profile_.maxCleared >= 0)
        drawTextRight(frame, x + boxW - 3, y + boxH - 2, "< > REPLAY CONTRACTS", kDim);
    // The views nobody finds by accident, and the fresh start.
    drawText(frame, x + 3, y + boxH - 1,
             "[" + actionLabel(Action::Scope) + "] GUNSIGHT  [" + actionLabel(Action::ToggleView) +
                 "] EXTERNAL VIEW  " + hk("O", 4) + " CONTROLS",
             kDim);
    drawTextRight(frame, x + boxW - 3, y + boxH - 1,
                  wipeArmed_ > 0.0f ? "[N] AGAIN TO WIPE SAVE!" : "[N] NEW PROFILE",
                  wipeArmed_ > 0.0f ? kBad : kDim);
}

// ------------------------------------------------------------------ result

void Game::drawResult(AsciiFrame& frame) {
    const int W = frame.w, H = frame.h;
    if (W < 40 || H < 14) return;
    const bool won = mission_.phase() == MissionPhase::Cleared;
    const MissionLedger& led = mission_.ledger();

    const int boxW = std::min(W - 6, 62);
    const int boxH = std::min(H - 2, 22);
    const int x = (W - boxW) / 2;
    const int y = (H - boxH) / 2;
    fillPanel(frame, x, y, boxW, boxH);
    drawRect(frame, x, y, boxW, boxH, won ? kGood : kBad);

    int ly = y + 1;
    drawText(frame, x + 3, ly++, "AFTER-ACTION REPORT // " + level_.name,
             won ? kGood : kBad);
    drawText(frame, x + 3, ly++,
             won ? "CONTRACT FULFILLED" : "CONTRACT FAILED", won ? kBright : kBad);
    ++ly;

    auto row = [&](const char* label, const std::string& val, const TextStyle& st) {
        if (ly >= y + boxH - 2) return;
        drawText(frame, x + 3, ly, label, kDim);
        drawTextRight(frame, x + boxW - 3, ly++, val, st);
    };

    // The destruction ledger. Cold numbers; the numbers do the celebrating.
    int unitTotal = 0;
    const int nKinds = std::min(kUnitKindCount, 16);
    for (int i = 0; i < nKinds; ++i) unitTotal += led.unitKills[i];
    row("HOSTILE MACHINES DESTROYED", fmtInt(led.mechKills), kNorm);
    if (unitTotal > 0) {
        row("UNITS DESTROYED", fmtInt(unitTotal), kNorm);
        for (int i = 0; i < nKinds; ++i)
            if (led.unitKills[i] > 0)
                row(("  " + std::string(unitKindName(static_cast<UnitKind>(i))) + "S").c_str(),
                    fmtInt(led.unitKills[i]), kDim);
    }
    if (led.propsDestroyed > 0)
        row("STRUCTURES / MATERIEL", fmtInt(led.propsDestroyed), kNorm);
    row("OBJECTIVES COMPLETED",
        fmtInt(led.objectivesDone) + " / " + fmtInt(mission_.objectiveCount()), kNorm);
    if (led.checkpointDeaths > 0)
        row("MACHINE LOST", fmtInt(led.checkpointDeaths), kBad);
    ++ly;
    row("KILL BOUNTIES", money(led.cashKills) + " cr", kNorm);
    row("DESTRUCTION LEDGER", money(led.cashDestruction) + " cr", kNorm);
    row("OBJECTIVE FEES", money(led.cashObjectives) + " cr", kNorm);
    row("SECTORS CLEARED", fmtInt(led.sectorsCleared) + " / " +
                               fmtInt(mission_.objectiveCount()) + "   " +
                               money(led.cashClearance) + " cr",
        led.cashClearance > 0 ? kGood : kDim);
    if (won) {
        row("COMPLETION BONUS", money(level_.completionBonus) + " cr", kNorm);
        row("TIME BONUS", money(led.cashTimeBonus) + " cr",
            led.cashTimeBonus > 0 ? kGood : kDim);
        row("MISSION TIME", fmt(mission_.missionTime(), 1) + "s", kDim);
    }
    ++ly;
    if (won) {
        int take = mission_.cashEarned();
        // A contract already on the books pays a quarter when replayed.
        const bool replay = profile_.level <= profile_.maxCleared;
        if (replay) take = take / 4;
        row("TOTAL PAYOUT", money(take) + " cr", kBright);
        if (replay) row("  (REPEAT CONTRACT - QUARTER RATE)", "", kDim);
        row("BALANCE", money(profile_.cash + take) + " cr", kBright);
    } else {
        row("SALVAGE RECOVERY (PARTIAL)", money(static_cast<int>(
            mission_.cashEarned() * 0.35f) + 140 +
            mission_.level().power * 45) + " cr", kDim);
        row("THE CONTRACT IS LOST - RUN IT AGAIN", "", kBad);
    }
    drawText(frame, x + 3, y + boxH - 2,
             hk("ENTER", 0) + (won ? " PROCEED TO WORKSHOP" : " REFIT AND RETRY"),
             kBright);
}

// ------------------------------------------------------------------- store

void Game::drawStore(AsciiFrame& frame) {
    const int W = frame.w, H = frame.h;
    if (W < 60 || H < 20) return;

    // Header.
    drawText(frame, 2, 1, "WORKSHOP", kBright);
    drawText(frame, 2, 2, "NEXT: MISSION " + fmtInt(profile_.level + 1), kDim);
    drawTextRight(frame, W - 2, 1, money(store_.cash()) + " cr", kBright);

    // ---- left: slot list and parts ---------------------------------------
    int y = 4;
    const std::vector<Slot>& ss = storeSlots();
    for (Slot s : ss) {
        const bool cur = (s == store_.slot());
        std::string label = slotName(s);
        if (s == Slot::Weapon) {
            label += " " + fmtInt(store_.weaponMount() + 1);
            const Loadout& cl = store_.currentLoadout();
            const size_t m = static_cast<size_t>(store_.weaponMount());
            const int grp = (m < cl.weaponGroups.size()) ? cl.weaponGroups[m] : 0;
            label += grp ? " [R]" : " [L]";
            if (m < cl.weapons.size() && !cl.weapons[m].empty())
                if (const PartDef* wp2 = PartCatalog::instance().find(cl.weapons[m]))
                    label += " " + wp2->name.substr(0, 12);
        }
        drawText(frame, 2, y++, (cur ? "> " : "  ") + label, cur ? kBright : kDim);
    }

    y += 1;
    if (store_.pane() != StorePane::Slots) {
        const std::vector<const PartDef*>& list = store_.listing();
        // Scroll the listing so the cursor stays visible in a short window.
        const int rows = std::min(static_cast<int>(list.size()), H - y - 4);
        int first = 0;
        if (rows > 0 && store_.cursor() >= rows) first = store_.cursor() - rows + 1;
        for (int i = 0; i < rows; ++i) {
            const int idx = first + i;
            if (idx >= static_cast<int>(list.size())) break;
            const PartDef* p = list[static_cast<size_t>(idx)];
            const bool cur = (idx == store_.cursor());
            const bool blocked = !store_.fittable(p);
            std::string line = (cur ? "> " : "  ") + p->name;
            if (line.size() > 30) line = line.substr(0, 30);
            // A part the mount cannot take is dimmed rather than hidden, and
            // its price is replaced by what it would need, so the catalogue
            // doubles as a reason to want a bigger chassis.
            const TextStyle& ns = blocked ? kDim : (cur ? kBright : kNorm);
            drawText(frame, 2, y, line, ns);
            if (blocked) {
                drawText(frame, 33, y++, "LOCKED", kDim);
            } else {
                drawText(frame, 33, y++,
                         profile_.hasUnlocked(p->id) ? std::string("SALVAGE")
                         : p->price > 0 ? money(p->price) : std::string("--"),
                         profile_.hasUnlocked(p->id) ? kGood : (cur ? kBright : kDim));
            }
        }
    } else {
        drawText(frame, 2, y++, hk("ENTER", 0) + " BROWSE", kDim);
        drawText(frame, 2, y++, (padActive_ ? "[D-PAD L/R]" : "[LEFT/RIGHT]") + std::string(" MOUNT"), kDim);
        if (store_.slot() == Slot::Weapon)
            drawText(frame, 2, y++, hk("TAB", 3) + " TRIGGER GROUP L/R", kDim);
        drawText(frame, 2, y++, hk("O", 4) + " CONTROLS", kDim);
    }

    // ---- right: the comparison table -------------------------------------
    // The stat table starts on row 15 and stops five rows short of the
    // bottom. On a thirty-row grid (a 1440p window at the magnification the
    // HUD picks there) that is ten rows for up to sixteen lines, and the
    // rest ran off the bottom of the screen. When they will not fit and
    // the grid is wide enough, the table goes two columns; when it is not,
    // the lines that show no change are the ones dropped.
    const int statCount = static_cast<int>(store_.statLines().size());
    const int statBottom = H - 5;
    const bool twoCol = statCount > (statBottom - 15) && W >= 110;
    const int rx = W - (twoCol ? 66 : 40);
    if (rx >= 40 && store_.pane() != StorePane::Slots) {
        int ry = 4;
        if (const PartDef* sel = store_.selected()) {
            // The description wraps to the width the panel ACTUALLY has, and
            // the stat table starts wherever the description ends. Both used
            // to be pinned to numbers typed in when this panel was a fixed
            // forty columns and the grid a fixed thirty rows, which is how a
            // blurb ended up cut off mid-sentence with the table drawn over
            // the rest of it.
            const int descW = std::max(16, W - 2 - rx);
            std::vector<std::pair<std::string, const TextStyle*>> desc;
            auto wrapInto = [&](const std::string& text, const TextStyle* st) {
                std::string rest = text;
                while (!rest.empty()) {
                    size_t cut = rest.size();
                    if (static_cast<int>(cut) > descW) {
                        cut = static_cast<size_t>(descW);
                        while (cut > 0 && rest[cut] != ' ') --cut;
                        if (cut == 0) cut = static_cast<size_t>(descW);
                    }
                    desc.push_back({rest.substr(0, cut), st});
                    rest = (cut < rest.size()) ? rest.substr(cut + 1) : std::string();
                }
            };
            desc.push_back({sel->name, &kBright});
            desc.push_back({sel->maker, &kDim});
            desc.push_back({std::string(), &kDim});
            wrapInto(sel->blurb, &kNorm);
            // A chassis trait is the most decisive thing about a hull and it
            // appears on no stat row, so it goes above the ability block in
            // the same place the eye is already looking.
            if (sel->stats.trait != Trait::None) {
                desc.push_back({std::string(), &kDim});
                desc.push_back({std::string("TRAIT  ") + traitName(sel->stats.trait), &kGood});
                wrapInto(traitBlurb(sel->stats.trait), &kDim);
            }
            // What this part lets you *do*. More decisive than any stat row.
            if (sel->stats.ability != Ability::None) {
                desc.push_back({std::string(), &kDim});
                desc.push_back({std::string(abilityIsActive(sel->stats.ability)
                                    ? "[" + actionLabel(Action::AbilityLegs).substr(0, 3) + "] "
                                    : "PASSIVE ") + abilityName(sel->stats.ability), &kWarn});
                wrapInto(abilityBlurb(sel->stats.ability), &kDim);
            }
            // Share the column out: the table gets the rows it needs, the
            // description keeps the rest, and whichever has to give way says
            // so with an ellipsis instead of stopping in the middle of a word.
            // The table can always fall back to showing only the lines that
            // actually CHANGED, so it yields to the description first: what
            // a part does is more decisive than a column of numbers that
            // read the same either way.
            int changed = 0;
            for (const StatLine& l : store_.statLines())
                if (std::fabs(l.candidate - l.current) >= 1e-3f) ++changed;
            int statRows = twoCol ? (statCount + 1) / 2 : statCount;
            const int room = std::max(4, statBottom - ry);
            int descRows = static_cast<int>(desc.size());
            if (descRows + 1 + statRows > room) {
                statRows = std::max(6, std::min(statRows, changed));
                descRows = std::max(3, room - statRows - 1);
            }
            descRows = std::min(descRows, static_cast<int>(desc.size()));
            for (int i = 0; i < descRows; ++i) {
                std::string t = desc[static_cast<size_t>(i)].first;
                // A block that had to be cut short says so - and the marker
                // has to fit the column too, or the ellipsis is the thing
                // that runs off the edge.
                if (i == descRows - 1 && descRows < static_cast<int>(desc.size())) {
                    if (static_cast<int>(t.size()) + 3 > descW)
                        t.resize(static_cast<size_t>(std::max(0, descW - 3)));
                    t += "...";
                }
                drawText(frame, rx, ry++, t, *desc[static_cast<size_t>(i)].second);
            }
            if (ry < statBottom) ++ry;

            // Which lines to show: all of them when they fit (in one column
            // or two); otherwise the changed ones first, unchanged ones only
            // while there is room.
            std::vector<const StatLine*> shown;
            {
                const int avail = std::max(1, statBottom - ry);
                const int room = twoCol ? avail * 2 : avail;
                for (const StatLine& l : store_.statLines()) {
                    const bool same = std::fabs(l.candidate - l.current) < 1e-3f;
                    if (!same) shown.push_back(&l);
                }
                if (statCount <= room) {
                    shown.clear();
                    for (const StatLine& l : store_.statLines()) shown.push_back(&l);
                } else {
                    for (const StatLine& l : store_.statLines()) {
                        if (static_cast<int>(shown.size()) >= room) break;
                        const bool same = std::fabs(l.candidate - l.current) < 1e-3f;
                        if (same) shown.push_back(&l);
                    }
                    // Keep the catalogue order.
                    std::vector<const StatLine*> ordered;
                    for (const StatLine& l : store_.statLines())
                        for (const StatLine* q : shown) if (q == &l) ordered.push_back(q);
                    shown = ordered;
                }
            }
            const int perCol = twoCol ? (static_cast<int>(shown.size()) + 1) / 2
                                      : static_cast<int>(shown.size());
            for (size_t si = 0; si < shown.size(); ++si) {
                const StatLine& l = *shown[si];
                const int col = (twoCol && static_cast<int>(si) >= perCol) ? 1 : 0;
                const int cx0 = rx + col * 33;
                const int cy = ry + static_cast<int>(si) - col * perCol;
                if (cy >= statBottom) break;
                // A yes/no stat reads as "1" in a numeric column, which tells
                // nobody anything.
                if (l.label == "WALL CAPABLE") {
                    const bool now = l.current > 0.5f, then = l.candidate > 0.5f;
                    drawText(frame, cx0, cy, l.label, kDim);
                    drawText(frame, cx0 + 14, cy, now ? "YES" : "NO", now ? kGood : kDim);
                    if (now != then) {
                        drawText(frame, cx0 + 22, cy, "->", kDim);
                        drawText(frame, cx0 + 25, cy, then ? "YES" : "NO",
                                 then ? kGood : kBad);
                    }
                    continue;
                }
                // Only show a delta when the numbers actually differ; a table of
                // identical values is noise that hides the two lines that matter.
                const bool same = std::fabs(l.candidate - l.current) < 1e-3f;
                drawText(frame, cx0, cy, l.label, kDim);
                drawText(frame, cx0 + 14, cy, fmt(l.current, l.digits), kNorm);
                if (!same) {
                    const bool better = l.higherIsBetter ? (l.candidate > l.current)
                                                         : (l.candidate < l.current);
                    drawText(frame, cx0 + 22, cy, "->", kDim);
                    drawText(frame, cx0 + 25, cy, fmt(l.candidate, l.digits),
                             better ? kGood : kBad);
                }
            }
        }
    }

    // ---- bottom: price and controls --------------------------------------
    const int by = H - 3;
    // Overdraw is the one way to make the machine strictly worse by spending
    // money, so it gets a warning rather than a table row nobody reads.
    if (store_.previewStats().overdrawn) {
        drawText(frame, 2, by - 1,
                 "! REACTOR OVERDRAWN - " +
                     fmt(store_.previewStats().powerDraw, 2) + " MW DRAWN, " +
                     fmt(store_.previewStats().powerOutput, 2) + " MW AVAILABLE",
                 kBad);
        drawText(frame, 2, by - 0 - 1 + 1, "", kDim);
    }
    if (store_.pane() != StorePane::Slots && store_.selected()) {
        const std::string blocked = store_.blockedReason(store_.selected());
        const int price = store_.priceOfSelected();
        if (!blocked.empty()) {
            drawText(frame, 2, by, blocked + " - " + money(price) + " cr", kWarn);
        } else if (store_.alreadyFitted()) {
            drawText(frame, 2, by, "FITTED", kGood);
        } else {
            drawText(frame, 2, by, "COST " + money(price) + " cr",
                     store_.canAfford() ? kNorm : kBad);
        }
    }
    drawText(frame, 2, by + 1,
             hk("ENTER", 0) + " FIT  " + hk("BKSP", 1) + " BACK  " + hk("TAB", 3) + " " +
                 std::string(store_.comparing() ? "SHOW CURRENT" : "SHOW FITTED") +
                 "  " + hk("X", 2) + " SELL  " + hk("SPACE", 6) + " DEPLOY", kDim);

    if (store_.messageAge() < 2.5f && !store_.message().empty())
        drawTextRight(frame, W - 2, by, store_.message(), kWarn);
}

} // namespace sb
