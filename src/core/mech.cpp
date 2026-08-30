#include "mech.h"

namespace sb {

namespace {

constexpr float kGravity = -24.0f;
constexpr float kJumpChargeTime = 0.5f;      // the deliberate wind-up before a leap
constexpr float kFallDamageSpeed = 19.0f;    // impacts below this are free
constexpr float kFallDamageScale = 3.2f;
// Sprint was a button you held forever - a tax on the hand, not a choice.
// The multiplier is now BAKED IN: the machine always travels at what used
// to be sprint speed, and Shift does nothing.
constexpr float kBaseSpeedScale = 1.45f;
constexpr float kClimbGrace = 0.40f;         // seconds of lost contact before letting go

// Six limbs, three a side. The tripods are {front-left, mid-right, back-left}
// and its mirror, which is the gait every real hexapod uses because three feet
// are always down and their centroid always contains the body.
const Vec3 kLegOutward[6] = {
    {-0.72f, 0.0f,  0.69f}, { 0.72f, 0.0f,  0.69f},
    {-1.00f, 0.0f,  0.00f}, { 1.00f, 0.0f,  0.00f},
    {-0.72f, 0.0f, -0.69f}, { 0.72f, 0.0f, -0.69f},
};
const int kLegGroup[6] = {0, 1, 1, 0, 0, 1};

} // namespace

const char* mechStateName(MechState s) {
    switch (s) {
        case MechState::Grounded:  return "WALKING";
        case MechState::Crouching: return "CHARGING";
        case MechState::Airborne:  return "AIRBORNE";
        case MechState::Landing:   return "LANDING";
        case MechState::Destroyed: return "DESTROYED";
        default:                   return "?";
    }
}

// ------------------------------------------------------------------- setup

void Mech::setLoadout(const Loadout& loadout) {
    loadout_ = loadout;
    loadout_.ensureWeaponSlots();
    stats_ = deriveStats(loadout_);

    const PartCatalog& cat = PartCatalog::instance();
    const PartDef* chassis = loadout_.part(Slot::Chassis);

    weapons_.clear();
    if (chassis) {
        for (size_t i = 0; i < chassis->hardpoints.size(); ++i) {
            MountedWeapon mw;
            mw.mount = chassis->hardpoints[i];
            const std::string& id = (i < loadout_.weapons.size()) ? loadout_.weapons[i] : std::string();
            const PartDef* wp = cat.find(id);
            // A weapon only fits a mount of its own size class or larger.
            if (wp && static_cast<int>(wp->size) <= static_cast<int>(mw.mount.size)) {
                mw.part = wp;
                if (wp->weapon.ammo == AmmoKind::Limited) {
                    mw.rounds = wp->weapon.magazine;
                    // A line hull is built around its racks: heavy weapons on
                    // this frame deploy with spares nobody else gets, which
                    // is what lets a limited-ammo build last a whole contract.
                    if (stats_.trait == Trait::Bulk) mw.reserve += 2;
                }
            }
            mw.group = (i < loadout_.weaponGroups.size() && loadout_.weaponGroups[i] >= 0)
                           ? (loadout_.weaponGroups[i] != 0 ? 1 : 0)
                           : ((wp && wp->size != SizeClass::Light) ? 1 : 0);
            weapons_.push_back(mw);
        }
    }

    const float bulk = chassis ? clampf(chassis->stats.mass / 5.0f, 0.75f, 1.9f) : 1.0f;
    hitRadius_ = 1.55f * bulk;

    layoutLegs();
    buildVisual();
}

void Mech::poseStatic(const Loadout& loadout, float yaw) {
    setLoadout(loadout);
    team_ = Team::Player;
    health_ = stats_.maxHealth;
    poseAt(Vec3(0.0f, stats_.standHeight, 0.0f), yaw);
}

void Mech::poseAt(const Vec3& position, float yaw) {
    state_ = MechState::Grounded;
    heat_ = 0.0f;
    overheated_ = false;
    crouch_ = 0.0f;
    recoil_ = 0.0f;
    damageFlash_ = 0.0f;
    turretYaw_ = 0.0f;
    turretPitch_ = 0.0f;

    up_ = Vec3(0.0f, 1.0f, 0.0f);
    forward_ = normalize(Vec3(std::sin(yaw), 0.0f, std::cos(yaw)));
    vel_ = Vec3(0.0f);
    pos_ = position;
    rideHeight_ = stats_.standHeight;
    supportPoint_ = pos_ - up_ * stats_.standHeight;
    supportNormal_ = up_;

    bodyXform_ = Mat4::translation(pos_) *
                 Mat4::basis(normalize(cross(up_, forward_)), up_, forward_);

    for (Leg& leg : legs_) {
        leg.foot = transformPoint(bodyXform_, leg.restLocal);
        leg.footNormal = up_;
        leg.planted = true;
        leg.stepping = false;
        leg.stepT = 1.0f;
        leg.stepFrom = leg.stepTo = leg.foot;
        leg.stepToNormal = up_;
    }
    solveIK();
    turretXform_ = bodyXform_;
    updateTurret(1.0f, pos_ + forward_ * 40.0f + Vec3(0.0f, 1.0f, 0.0f));
}

void Mech::layoutLegs() {
    const PartDef* chassis = loadout_.part(Slot::Chassis);
    const float bulk = chassis ? clampf(chassis->stats.mass / 5.0f, 0.75f, 1.9f) : 1.0f;
    const int hs = chassis ? chassis->style : 0;

    const float stance = stats_.standHeight;
    const float reach = stats_.legReach;

    // The BODY PLAN. Where the legs meet the hull is most of a walker's
    // silhouette: a pod with legs sprouting high off its equator reads as a
    // spider; a long hull with legs tucked underneath reads as a walking
    // tank; a flat shell with wide-set paddles reads as a crab. Each chassis
    // style gets its own plan rather than one shared ring.
    float hipX = 1.34f, hipY = -0.02f, hipZ = 1.62f, splay = 1.0f;
    // Per-pair anchoring: {front, mid, rear} multipliers on the ring, plus a
    // fore/aft shift, so legs can cluster, fan, rake or trail per plan -
    // the same six limbs bolted to genuinely different skeletons.
    float fx[3] = {1.0f, 1.0f, 1.0f};      // pair width
    float fz[3] = {1.0f, 1.0f, 1.0f};      // pair length spread
    float zs[3] = {0.0f, 0.0f, 0.0f};      // pair fore/aft shift (bulk units)
    // Limb articulation: how the reach splits into femur and tibia, and how
    // far the knee bends outward instead of straight up.
    femurFrac_ = 0.42f; tibiaFrac_ = 0.45f; kneeOut_ = 0.0f;
    hullStyle_ = hs;
    switch (hs) {
        case 0:  // pod: high sockets on the equator, tall steep arches
            hipX = 1.00f; hipY = 0.55f; hipZ = 1.10f; splay = 1.42f;
            femurFrac_ = 0.40f; tibiaFrac_ = 0.47f; kneeOut_ = 0.14f;
            break;
        case 2:  // dart: raked - front pair swept ahead, mid tucked aft
            hipX = 1.15f; hipY = -0.10f; hipZ = 2.05f; splay = 0.85f;
            fz[0] = 1.18f; zs[1] = -0.42f; fx[1] = 0.92f;
            femurFrac_ = 0.44f; tibiaFrac_ = 0.43f;
            break;
        case 3:  // torso: all six radiate from one compact shoulder ring
            hipX = 1.60f; hipY = 0.22f; hipZ = 1.30f; splay = 1.22f;
            fz[0] = 0.72f; fz[2] = 0.72f; fx[1] = 1.12f;
            femurFrac_ = 0.45f; tibiaFrac_ = 0.42f; kneeOut_ = 0.10f;
            break;
        case 4:  // long hull: struts in a narrow line under the keel
            hipX = 1.15f; hipY = -0.38f; hipZ = 2.30f; splay = 0.76f;
            fx[0] = 0.82f; fx[1] = 0.78f; fx[2] = 0.82f;
            femurFrac_ = 0.43f; tibiaFrac_ = 0.44f;
            break;
        case 5:  // casemate: low, planted, rear pair braced wide
            hipX = 1.30f; hipY = -0.12f; hipZ = 1.55f; splay = 0.95f;
            fx[2] = 1.14f;
            break;
        case 6:  // crab shell: a fan - mid pair widest, flat outward knees
            hipX = 2.00f; hipY = 0.02f; hipZ = 1.50f; splay = 1.50f;
            fx[1] = 1.22f; fz[0] = 0.85f; fz[2] = 0.85f;
            femurFrac_ = 0.47f; tibiaFrac_ = 0.40f; kneeOut_ = 0.55f;
            break;
        case 7:  // cradle: rear pair trails wide - the recoil stance
            hipX = 1.35f; hipY = -0.18f; hipZ = 1.80f; splay = 0.92f;
            fx[2] = 1.20f; fz[2] = 1.12f; zs[1] = 0.18f;
            femurFrac_ = 0.44f; tibiaFrac_ = 0.43f;
            break;
        default: break;                    // box: the baseline ring
    }

    // Plant the feet at roughly 62% of full reach, stretched or tucked by the
    // plan's splay. High-hip plans get more lateral reach, which is what
    // raises the knee arch above the body - the spider profile.
    const float diag = 0.62f * reach;
    const float lateral =
        std::sqrt(std::max(diag * diag - stance * stance, 0.6f)) * splay;

    for (int i = 0; i < 6; ++i) {
        Leg& leg = legs_[static_cast<size_t>(i)];
        const Vec3 out = kLegOutward[i];
        const int p = i / 2;               // pair: 0 front, 1 mid, 2 rear
        leg.hipLocal = Vec3(out.x * hipX * fx[p] * bulk, hipY * bulk,
                            (out.z * hipZ * fz[p] + zs[p]) * bulk);
        leg.restLocal = leg.hipLocal + Vec3(out.x * lateral, 0.0f, out.z * lateral);
        leg.restLocal.y = -stance;
        leg.group = kLegGroup[i];
        leg.phaseOffset = (kLegGroup[i] == 0) ? 0.0f : 0.5f;
    }
}

void Mech::init(const World& world, const Loadout& loadout, const Vec3& spawnPos,
                float headingYaw, Team team, uint32_t seed) {
    team_ = team;
    seed_ = seed ? seed : 1u;
    setLoadout(loadout);

    pos_ = spawnPos;
    vel_ = Vec3(0.0f);
    up_ = Vec3(0.0f, 1.0f, 0.0f);
    forward_ = normalize(Vec3(std::sin(headingYaw), 0.0f, std::cos(headingYaw)));
    rideHeight_ = stats_.standHeight;
    health_ = stats_.maxHealth;
    state_ = MechState::Grounded;

    // Drop onto the ground rather than trusting the caller's Y.
    const SurfaceHit ground = world.findFoothold(pos_, up_, 40.0f, 90.0f);
    if (ground.hit) {
        pos_ = ground.point + up_ * stats_.standHeight;
        supportPoint_ = ground.point;
        supportNormal_ = ground.normal;
    }

    bodyXform_ = Mat4::translation(pos_) *
                 Mat4::basis(normalize(cross(up_, forward_)), up_, forward_);

    for (int i = 0; i < 6; ++i) {
        Leg& leg = legs_[static_cast<size_t>(i)];
        const Vec3 restWorld = localToWorld(leg.restLocal);
        const SurfaceHit h = world.findFoothold(restWorld, up_, 3.0f, 8.0f);
        leg.foot = h.hit ? h.point : restWorld;
        leg.footNormal = h.hit ? h.normal : up_;
        leg.planted = h.hit;
        leg.stepFrom = leg.stepTo = leg.foot;
        leg.stepToNormal = leg.footNormal;
        leg.stepT = 1.0f;
        leg.stepping = false;
    }
    solveIK();
}

Vec3 Mech::localToWorld(const Vec3& local) const {
    return transformPoint(bodyXform_, local);
}

bool Mech::gripAllowed(const Vec3& normal) const {
    // How far this surface tilts from level, against what the limbs can hold.
    const float tilt = std::acos(clampf(normal.y, -1.0f, 1.0f));
    return tilt <= stats_.climbAngle + 1e-3f;
}

float Mech::gripQuality() const {
    // How much margin the limbs have on the surface they are holding, knocked
    // down while rounding a corner. This is the single number that decides
    // whether a machine walks up a wall or scrabbles at it.
    const float tilt = std::acos(clampf(up_.y, -1.0f, 1.0f));
    const float margin = stats_.climbAngle - tilt;
    const float base = clampf(0.45f + margin / deg2rad(40.0f) * 0.55f, 0.0f, 1.0f);
    // A corner costs a machine with headroom almost nothing and a machine at
    // its limit almost everything. Applying the same penalty to both made the
    // specialist legs fall off corners too, which removes the reason to buy
    // them.
    const float penalty = 0.78f * (1.0f - 0.62f * base);
    const float q = base * (1.0f - penalty * clampf(cornerStress_, 0.0f, 1.0f));
    return clampf(q, 0.0f, 1.0f);
}

float Mech::climbFraction() const {
    return clampf(1.0f - clampf(up_.y, -1.0f, 1.0f), 0.0f, 2.0f) * 0.5f;
}

int Mech::legsInAir() const {
    int n = 0;
    for (const Leg& l : legs_) if (l.stepping || !l.planted) ++n;
    return n;
}

// ------------------------------------------------------------- orientation

void Mech::updateOrientation(float dt, const World& world, const MechInput& in) {
    // The body is oriented by whatever its feet are holding. Averaging the
    // contact normals means the transition from ground to wall happens by
    // itself: as the leading legs find the wall, the average tips, the body
    // rolls, and the trailing legs follow onto the same surface next step.
    Vec3 normalSum(0.0f);
    Vec3 pointSum(0.0f);
    int contacts = 0;
    for (const Leg& leg : legs_) {
        if (!leg.planted) continue;
        normalSum += leg.footNormal;
        pointSum += leg.foot;
        ++contacts;
    }

    Vec3 desiredUp = up_;
    if (contacts > 0) {
        supportNormal_ = normalize(normalSum);
        supportPoint_ = pointSum * (1.0f / static_cast<float>(contacts));
        desiredUp = supportNormal_;
    } else if (state_ == MechState::Airborne) {
        desiredUp = Vec3(0.0f, 1.0f, 0.0f);
    }

    // Wall commitment. Pushing into a climbable face aims the body at it, and
    // once committed the body takes that face's normal outright instead of
    // averaging it with whatever the trailing legs are still standing on.
    //
    // The averaging is what used to make climbing fail. During a transition
    // three legs are on the ground and three on the wall, so the mean normal
    // sits at 45 degrees - a diagonal neither surface holds - and the machine
    // wobbled between the two until it slid off. Committing to the face lets
    // the trailing legs find the wall on their next step, which is what makes
    // the transition finish.
    bool onFace = false;
    if (!in.releaseGrip && state_ != MechState::Airborne) {
        // While climbing, probe straight into the face being held so the ray
        // follows the body around; otherwise probe where the pilot is driving.
        Vec3 dir = climbing_ ? -climbNormal_
                             : (lengthSq(in.moveWorld) > 1e-4f ? normalize(in.moveWorld)
                                                               : forward_);
        // Mid-crest the wall below the lip is off limits: re-gripping the
        // face while the body pours onto the roof yanked the hull sideways
        // halfway through every top-out.
        const bool wantsIt = (climbing_ || in.throttle > 0.25f) && crestEase_ <= 0.3f;
        if (wantsIt && lengthSq(dir) > 1e-6f) {
            dir = normalize(dir);
            const Vec3 origin = pos_ - up_ * (rideHeight_ * 0.35f);
            // A longer reach once committed, so a step away from the face or a
            // bump in the wall does not drop the machine off it.
            const float probeLen = hitRadius_ + (climbing_ ? 3.2f : 1.6f);
            const SurfaceHit probe = world.raycast(origin, dir, probeLen);
            if (probe.hit && probe.obstacle >= 0) {
                const Obstacle& o = world.obstacles()[static_cast<size_t>(probe.obstacle)];
                const float tilt = std::acos(clampf(probe.normal.y, -1.0f, 1.0f));
                // A LOW obstacle is not a wall. A jersey barrier, a rubble
                // pile, a crate stack: the machine steps OVER those, and
                // committing to them as climbing faces - taking their normal
                // as body-up, running the wall gait - is what made low cover
                // feel like a puzzle instead of a kerb. Anything whose top
                // is within a stride of the hull gets a step-up assist and
                // no climb state at all.
                const float obTop = o.center.y + o.half.y;
                // "Low" is a property of the OBSTACLE, measured from the
                // ground it stands on - not of where the hull happens to be.
                // (Measuring against the hull made the top of a ten-metre
                // wall count as low cover the moment the machine climbed
                // near it, which cancelled the whole top-out.)
                const float obBase = world.terrain().height(o.center.x, o.center.z);
                const float obHeight = obTop - obBase;
                const bool lowStep = o.climbable && !climbing_ &&
                                     obHeight < stats_.standHeight * 1.15f &&
                                     obTop < pos_ .y - rideHeight_ +
                                                 stats_.standHeight * 1.05f;
                if (lowStep) {
                    // Lift over it: a gentle boost while pushing into the
                    // face, which the gait then plants feet on top of.
                    const float need = obTop - (pos_.y - rideHeight_);
                    if (need > 0.15f && dot(vel_, dir) > -0.5f) {
                        const Vec3 worldUp(0.0f, 1.0f, 0.0f);
                        if (dot(vel_, worldUp) < 3.2f)
                            vel_ += worldUp * (7.5f * dt * clampf(need, 0.0f, 2.0f));
                        vel_ += dir * (2.5f * dt);
                    }
                }
                if (!lowStep && o.climbable && tilt > deg2rad(50.0f) &&
                    gripAllowed(probe.normal)) {
                    // Catching a wall costs momentum. Arriving at ten metres a
                    // second and expecting six limbs to find purchase on a
                    // vertical face is not reasonable - the machine used to get
                    // one foot on, fail the two-contact test and drop straight
                    // off again. Shedding most of the speed on contact is both
                    // what would really happen and what lets the gait catch up.
                    if (!climbing_) {
                        const Vec3 intoFace = climbNormal_ * dot(vel_, probe.normal);
                        vel_ = (vel_ - intoFace) * 0.35f;
                        // Where this climb began. Everything below the start is
                        // the ground the machine walked in on, not roof.
                        climbBaseY_ = pos_.y - rideHeight_;
                    }
                    climbNormal_ = probe.normal;
                    climbing_ = true;
                    onFace = true;
                }
            }
        }
    }
    // Rounding a corner. When the committed face runs out - the machine has
    // walked to the edge of the wall - look for an adjacent climbable face
    // before giving up on the building. Probing along the direction of travel
    // and around the old normal finds the returning wall of an outside corner
    // and the far wall of an inside one.
    if (climbing_ && !onFace) {
        const Vec3 travel = flattenY(vel_);
        const Vec3 side = (lengthSq(travel) > 0.5f) ? normalize(travel)
                                                    : normalize(cross(up_, forward_));
        const Vec3 tries[5] = {
            // An outside corner is the common case and the one that used to
            // end every traverse. Walking off the edge of face A leaves the
            // hull diagonally out from the corner post, touching neither face:
            // A is behind, B is to the side, and both are a hull radius away.
            // The only ray that finds anything from there is the one aimed
            // back at the corner itself, and it was the one direction the
            // search did not try.
            normalize(-(side + climbNormal_)),           // back at the corner
            side,                                        // straight on round it
            normalize(side * 0.7f - climbNormal_ * 0.7f),// tucking inward
            normalize(side * 0.7f + climbNormal_ * 0.7f),// wrapping outward
            -climbNormal_,                               // still the old face
        };
        const Vec3 origin = pos_ - up_ * (rideHeight_ * 0.25f);
        for (const Vec3& d : tries) {
            if (lengthSq(d) < 1e-6f) continue;
            const SurfaceHit probe = world.raycast(origin, normalize(d),
                                                   hitRadius_ + 3.6f);
            if (!probe.hit || probe.obstacle < 0) continue;
            const Obstacle& o = world.obstacles()[static_cast<size_t>(probe.obstacle)];
            const float tilt = std::acos(clampf(probe.normal.y, -1.0f, 1.0f));
            if (!o.climbable || tilt <= deg2rad(50.0f)) continue;
            if (!gripAllowed(probe.normal)) continue;
            // Found another face. How violent the change of plane is decides
            // how much it costs: a gentle wrap is nearly free, a right-angle
            // corner is where an ill-suited machine comes off.
            const float turn = 1.0f - clampf(dot(probe.normal, climbNormal_), -1.0f, 1.0f);
            if (turn > 0.02f) cornerStress_ = clampf(cornerStress_ + turn * 0.9f, 0.0f, 1.4f);
            climbNormal_ = probe.normal;
            onFace = true;
            break;
        }
    }
    cornerStress_ = std::max(0.0f, cornerStress_ - dt * 0.55f);

    // A short grace period before letting go. Arriving at a wall at ten metres
    // a second, the probe misses for a frame or two while the body swings onto
    // the face, and dropping the commitment on the first miss meant the machine
    // bounced off its first contact every time and only stuck on a later,
    // slower approach.
    if (onFace) climbGrace_ = kClimbGrace;
    else climbGrace_ = std::max(0.0f, climbGrace_ - dt);
    if (!onFace && climbGrace_ <= 0.0f) {
        // The wall ran out. This is the OTHER way a climb ends - not a
        // recognised top-out, just a face that stopped being there - and it
        // used to end the climb silently, leaving the machine with four to
        // six metres a second of ascent and nothing to spend it on. It sailed
        // straight up into open sky above the roof and fell back down. A
        // climb that ends without a lip gets the same treatment a top-out
        // gets: shed the ascent, let go of the face, hand it to ground
        // handling. Whatever the geometry did, the machine steps over rather
        // than launching.
        if (climbing_) {
            const Vec3 worldUpG(0.0f, 1.0f, 0.0f);
            const float upVel = dot(vel_, worldUpG);
            if (upVel > 1.6f) vel_ -= worldUpG * (upVel - 1.6f);
            crestEase_ = std::max(crestEase_, 0.8f);
            for (Leg& leg : legs_) {
                if (leg.planted && dot(leg.footNormal, climbNormal_) > 0.70f) {
                    leg.planted = false;
                    leg.stepping = false;
                }
            }
        }
        climbing_ = false;
        cornerStress_ = 0.0f;
    }

    // Topping out. The hull is at the lip when a level surface sits just past
    // the edge at body height. This must be detected and *acted on*, because
    // the face probe still sees wall below and would happily keep the machine
    // committed to it - which is exactly the old failure: the hull hung flat
    // on the face until the rear legs reached the top, then flipped over all
    // at once. Releasing the wall here and shoving up-and-over is the pull-up.
    mantle_ = 0.0f;
    if (climbing_) {
        int wallFeet = 0, roofFeet = 0;
        Vec3 roofN(0.0f);
        float lowWallFoot = 1e9f;
        for (const Leg& leg : legs_) {
            if (!leg.planted) continue;
            if (dot(leg.footNormal, climbNormal_) > 0.70f) {
                ++wallFeet;
                lowWallFoot = std::min(lowWallFoot, leg.foot.y);
            }
        }
        for (const Leg& leg : legs_) {
            if (!leg.planted) continue;
            if (dot(leg.footNormal, climbNormal_) > 0.70f) continue;
            // A "roof" foot grips a level surface up the FACE - above the
            // lowest wall-gripping foot, within two stand-heights of the
            // body. Two subtleties, both learned the hard way: at the base
            // of a wall the trailing legs stand on level GROUND (below every
            // wall foot - excluded by the lowWallFoot test), and when a
            // climb overshoots the lip the roof drops MORE than a third of
            // a stand-height below the body - the old tight height gate
            // stopped counting those feet, mantle and release both switched
            // off, and the climb drive walked the machine seven metres a
            // second into open sky.
            // With wall feet holding the face, any level foot above the
            // lowest of them is roof; with NO wall feet (start of a climb,
            // ground everywhere) only the old tight near-the-hull gate
            // counts, or the base of every wall fires the mantle.
            // And the hard floor under all of it: nothing at or below the
            // height the climb STARTED from is roof. At the base of a wall the
            // trailing legs stand on flat ground beside the lowest wall foot,
            // which satisfied every relative test here and counted four "roof"
            // feet on first contact - the release fired at the base, the
            // machine dropped off and crested the building twice.
            if (leg.foot.y < climbBaseY_ + 0.75f) continue;
            const bool roofHold =
                (wallFeet > 0)
                    ? (leg.foot.y > lowWallFoot - 0.2f &&
                       leg.foot.y > pos_.y - rideHeight_ * 1.9f)
                    : (leg.foot.y > pos_.y - rideHeight_ * 0.30f);
            if (leg.footNormal.y > 0.60f && roofHold) {
                ++roofFeet;
                roofN += leg.footNormal;
            }
        }
        const Vec3 worldUp(0.0f, 1.0f, 0.0f);
        Vec3 uphill = worldUp - climbNormal_ * dot(worldUp, climbNormal_);
        uphill = (lengthSq(uphill) > 1e-5f) ? normalize(uphill) : worldUp;

        if (roofFeet > 0) {
            // Front feet over the edge, gripping the roof. The body follows
            // the feet - that is the whole orientation rule of this machine -
            // so the hull starts pitching over the lip as soon as the first
            // foot wraps it, instead of waiting for all six.
            mantle_ = static_cast<float>(roofFeet) /
                      static_cast<float>(std::max(1, roofFeet + wallFeet));
            desiredUp = normalize(lerp(climbNormal_, normalize(roofN),
                                       mantle_ * 0.85f));
        } else {
            desiredUp = climbNormal_;
        }

        // The release: a level surface just past the edge, close to hull
        // height. Let go of the face, keep the momentum, and hand the machine
        // to ordinary ground handling - which the roof-planted feet make
        // seamless, because they are already standing on the thing it lands on.
        // The probe must look past the wall plane: the hull rides a full
        // stand-height OUTSIDE the face, and casting down from out there sees
        // only the drop. -climbNormal_ points through the face; above the lip
        // that is over the roof.
        // The search has to reach a LONG way down, because it is not only
        // finding the lip on the way up - it is the backstop that catches a
        // machine which has already climbed past the roof. At two stand
        // heights the probe stopped reaching the deck about four metres above
        // it, so a fast climber blew through the whole release window in a
        // few frames and kept going into open sky. Four stand heights covers
        // any ascent rate the caps allow.
        const SurfaceHit lip = world.findFoothold(
            pos_ + uphill * (hitRadius_ * 0.6f) - climbNormal_ * (rideHeight_ + 0.8f),
            worldUp, 1.2f, rideHeight_ * 2.0f);
        // The release window is measured against the FEET, not the hull
        // centre. Two cases qualify: the roof is level with where the legs
        // are standing (a normal top-out), or the machine has overshot and
        // the roof is already below the feet (release immediately - holding
        // the wall there is what pins it in the air past the lip).
        // Measuring against the hull with a generous margin let the release
        // fire a full metre BELOW the parapet - and once the upper limbs
        // started hooking the lip early, that fired on every climb, dropped
        // the machine back onto the face and made it crest twice.
        const float feetY = pos_.y - rideHeight_;
        const bool levelWithFeet = lip.point.y > feetY - rideHeight_ * 0.55f &&
                                   lip.point.y < feetY + rideHeight_ * 0.65f;
        const bool overshot = feetY > lip.point.y + 0.1f;
        // The surface the probe found has to be higher than the ground this
        // climb started on, or the "roof" it is offering is the pavement at
        // the foot of the wall.
        const bool aboveStart = lip.point.y > climbBaseY_ + 0.75f;
        // Two ways to qualify, and the second one matters more than it looks.
        //
        // The tidy case is roof-planted feet: limbs already over the parapet,
        // hull level with them, hand it to ground handling.
        //
        // The other is that at the exact moment the deck comes level with the
        // legs, the limbs frequently have NOTHING to hold - the old anchors
        // are below on the face, the new ones are not reachable yet, and the
        // machine passes through a frame or two at nought planted feet. The
        // roof-feet rule cannot fire during that gap, so the climb drive kept
        // pushing at a wall that had already ended and threw the machine five
        // metres into the sky above the roof. If the feet are at deck height
        // there is no wall left to climb, whatever the limbs are doing.
        // Measured against the HULL, not the feet. When the front limbs first
        // wrap the parapet the body is still most of a stand-height below the
        // deck - that is what a mantle looks like - so a feet-level window
        // never opened, the machine climbed straight past the roof and threw
        // itself five metres into the sky. What makes a hull-level window safe
        // now (it was not, before) is `aboveStart`: the pavement at the bottom
        // of the wall can no longer masquerade as a roof.
        const bool hullNearDeck = pos_.y > lip.point.y - rideHeight_ * 0.55f;
        // And the hard backstop. At the exact frame the deck comes level the
        // limbs often hold nothing at all - old anchors below on the face, new
        // ones not yet in reach - so the roof-feet rule cannot fire. Once the
        // body is above the deck there is no wall left to climb, whatever the
        // legs are doing.
        const bool hullPastDeck = pos_.y > lip.point.y + rideHeight_ * 0.20f;
        // The absolute guard, and the only one that cannot be argued with by
        // geometry: cast straight DOWN from the hull. If there is a level
        // surface within a stand-height and a half underneath, the machine is
        // standing over a roof, and whatever the face probes think they are
        // still holding is a phantom. Everything above this line reasons about
        // where the lip is; this line asks whether there is a floor, which is
        // the question that actually settles it. Without it a fast climber
        // could keep a grip on nothing six metres above a building and walk
        // into open sky.
        // Cast down from over the BUILDING, not from the hull. A machine on a
        // wall rides a hull-radius clear of the face, so its centre is
        // outside the footprint and a plumb line from it goes past the roof
        // and all the way to the pavement - which is why this test kept
        // reporting no floor two metres under a deck. Stepping inward along
        // the face normal first puts the cast where the roof actually is.
        const SurfaceHit under = world.findFoothold(
            pos_ - climbNormal_ * (hitRadius_ + 1.2f),
            worldUp, 0.4f, rideHeight_ * 2.4f);
        const bool overRoof = under.hit && under.normal.y > 0.70f &&
                              under.point.y > climbBaseY_ + 1.2f;
        const bool qualified = (hullNearDeck && roofFeet >= 2) ||
                               hullPastDeck || overshot;
        (void)levelWithFeet;
        // Two independent ways to be done climbing, and the second one does
        // NOT go through the lip probe - which was the whole problem. That
        // probe looks for a deck by casting inside the building from the
        // hull, and on some approaches it simply never finds one: it returned
        // no hit for the entire ascent while the machine climbed twenty-three
        // metres up a ten-metre block. A downward cast asking "is there a
        // floor under me" cannot fail that way, so it is checked separately
        // and it is the backstop that makes climbing past a roof impossible
        // rather than merely unlikely.
        const bool byLip = lip.hit && lip.normal.y > 0.72f && aboveStart && qualified;
        if (byLip || overRoof) {
            if (!byLip) {
                // Released by the floor under the hull rather than by a lip.
                supportPoint_ = under.point;
                supportNormal_ = under.normal;
            }
            climbing_ = false;
            climbGrace_ = 0.0f;
            cornerStress_ = 0.0f;
            if (byLip) {
                supportPoint_ = lip.point;
                supportNormal_ = lip.normal;
            }
            desiredUp = byLip ? lip.normal : under.normal;
            // The pull-up itself: a STEP onto the roof, not a launch. The
            // climb arrives carrying metres per second of upward speed; cap
            // it and push the hull OVER the lip instead - the follow-through
            // carries the rest.
            const float upVel = dot(vel_, worldUp);
            if (upVel > 2.0f) vel_ -= worldUp * (upVel - 2.0f);
            vel_ += -climbNormal_ * 2.6f + worldUp * 0.8f;
            // Onto the roof, not off it: the horizontal carry is capped to a
            // walking pace so a fast machine does not launch itself over the
            // far parapet the instant it tops out.
            {
                Vec3 flat = vel_ - worldUp * dot(vel_, worldUp);
                const float fs = length(flat);
                const float cap = std::min(stats_.maxSpeed * 0.55f, 5.5f);
                if (fs > cap) vel_ -= flat * ((fs - cap) / fs);
            }
            crestEase_ = 1.0f;
            // The rear limbs LET GO of the face. A foot still gripping the
            // wall kept feeding the wall's normal into the support average,
            // and the machine went on "walking" up the phantom slope of the
            // two mixed planes - seven metres a second into open sky above
            // the roof. Unplanted, they swing over the lip to roof anchors
            // on their next step: the back half of the wrap-around.
            for (Leg& leg : legs_) {
                if (leg.planted && dot(leg.footNormal, climbNormal_) > 0.70f) {
                    leg.planted = false;
                    leg.stepping = false;
                }
            }
        }
    }

    if (in.releaseGrip) {
        desiredUp = Vec3(0.0f, 1.0f, 0.0f);
        climbing_ = false;
        climbGrace_ = 0.0f;
    }
    if (state_ == MechState::Airborne) { climbing_ = false; climbGrace_ = 0.0f; }

    // Cresting, the up vector settles FAST: the old rate let the hull hang
    // tilted forty degrees for half a second after topping out, which read
    // as a stumble rather than a mantle.
    const float rate = (state_ == MechState::Airborne) ? 3.0f
                     : (crestEase_ > 0.0f) ? 12.0f : 7.0f;
    up_ = normalize(lerp(up_, desiredUp, 1.0f - std::exp(-rate * dt)));

    // Keep heading perpendicular to up, and turn it toward where we are going.
    Vec3 wish = in.moveWorld - up_ * dot(in.moveWorld, up_);
    if (lengthSq(wish) > 1e-5f && in.throttle > 0.1f) {
        wish = normalize(wish);
        const float maxTurn = stats_.turnRate * dt;
        const float cosang = clampf(dot(forward_, wish), -1.0f, 1.0f);
        const float ang = std::acos(cosang);
        if (ang > 1e-4f) {
            const float t = std::min(1.0f, maxTurn / ang);
            forward_ = normalize(lerp(forward_, wish, t));
        }
    }
    // A casemate aims with its hull: whenever the sight line falls outside
    // the gun's sliver of traverse, the whole machine walks its feet around
    // to bear. Full rate standing, half rate on the move - laying the hull
    // IS the weapon handling on this family, for the player and the AI both.
    if (stats_.turretYawLimit < PI) {
        Vec3 aimFlat = in.aimPoint - pos_;
        aimFlat = aimFlat - up_ * dot(aimFlat, up_);
        if (lengthSq(aimFlat) > 1.0f) {
            aimFlat = normalize(aimFlat);
            const float cosang = clampf(dot(forward_, aimFlat), -1.0f, 1.0f);
            const float ang = std::acos(cosang);
            if (ang > stats_.turretYawLimit * 0.5f) {
                const bool moving2 = in.throttle > 0.1f && lengthSq(in.moveWorld) > 1e-4f;
                const float rate = stats_.turnRate * (moving2 ? 0.5f : 1.15f) * dt;
                const float t2 = std::min(1.0f, rate / ang);
                forward_ = normalize(lerp(forward_, aimFlat, t2));
            }
        }
    }

    forward_ = forward_ - up_ * dot(forward_, up_);
    if (lengthSq(forward_) < 1e-6f) {
        // Degenerate only if heading and up align; pick any perpendicular.
        forward_ = normalize(cross(up_, Vec3(1.0f, 0.0f, 0.0f)));
    }
    forward_ = normalize(forward_);
}

// -------------------------------------------------------------- locomotion

void Mech::updateLocomotion(float dt, const World& world, const MechInput& in) {
    const float climb = climbFraction();
    float speedScale = lerpf(1.0f, stats_.climbSpeedFactor, clampf(climb * 1.6f, 0.0f, 1.0f));

    // Water. A spidertank does not swim. Leg-deep it wades, slowed and unable
    // to jump; past hull depth it floods, and flooding is not a damage type
    // you tank through - it is a countdown. This is what makes rivers soft
    // barriers, deep channels hard ones, and a causeway worth fighting over.
    wading_ = 0.0f;
    if (world.hasWater()) {
        const float feetY = pos_.y - rideHeight_;
        const float depth = world.waterLevel() - feetY;
        if (depth > 0.3f) {
            wading_ = clampf(depth / std::max(rideHeight_, 0.5f), 0.0f, 1.5f);
            speedScale *= lerpf(1.0f, 0.45f, clampf(wading_, 0.0f, 1.0f));
            if (world.waterLevel() > pos_.y + 0.4f) {
                // Hull under. Electronics, reactor, crew compartment: gone.
                floodTimer_ += dt;
                applyDamage((22.0f + stats_.maxHealth * 0.06f) * dt,
                            Vec3(0.0f, 1.0f, 0.0f));
            } else {
                floodTimer_ = 0.0f;
            }
        } else {
            floodTimer_ = 0.0f;
        }
    }
    const float crouchScale = lerpf(1.0f, 0.15f, crouch_);
    // Sprint is a genuine overdrive rather than "the throttle you were not
    // using": walking is full throttle, and holding shift pushes past it.
    // Always on - but on the GROUND only. Climbing is limb work, not a
    // run: feeding the run multiplier into a wall ascent sent the machine
    // rocketing four metres past the lip of every roof.
    // Ground pace is the derived stat itself now (see deriveStats). Climbing
    // is limb work, not a run: the wall ascent runs at the un-multiplied rate,
    // or the machine rockets metres past the lip of every roof.
    const float sprintScale = climbing_ ? (1.0f / kBaseSpeedScale) : 1.0f;
    // Brace plants the machine: no travel at all, in exchange for the steadiest
    // guns in the game. Overdrive is the opposite trade.
    const bool bracing = braced();
    const float overdrive = engagedKind(Ability::Overdrive)
                          ? engagedPower(Ability::Overdrive) : 1.0f;
    const float maxSpeed = bracing ? 0.0f
        : stats_.maxSpeed * speedScale * crouchScale * sprintScale * overdrive;

    // Desired travel, always in the surface's tangent plane.
    //
    // On a wall the naive projection is wrong, and it is why climbing used to
    // feel broken. The pilot pushes the stick at the building; that vector
    // points straight into the face, so projecting it onto the face leaves
    // almost nothing - whatever sideways residue the approach angle happened to
    // give - and the machine crabs along the wall instead of going up it. What
    // the pilot means by "into the surface I am holding" is "up it", so the
    // into-face component is turned into uphill travel rather than discarded.
    Vec3 intent = in.moveWorld;
    // While the face is momentarily lost (the grace window at the lip or over
    // a bump), pouring on more uphill speed is what used to fling the machine
    // ballistically off the top of every building. Climb only what is there.
    const bool faceInHand = climbing_ && climbGrace_ >= kClimbGrace * 0.99f;
    if (faceInHand && state_ != MechState::Airborne) {
        const float into = -dot(intent, up_);       // >0 when pushing at the face
        if (into > 0.0f) {
            const Vec3 worldUp(0.0f, 1.0f, 0.0f);
            Vec3 uphill = worldUp - up_ * dot(worldUp, up_);
            if (lengthSq(uphill) > 1e-4f) intent += normalize(uphill) * into;
        }
    }
    Vec3 wish = intent - up_ * dot(intent, up_);
    const float wishLen = lengthSq(wish) > 1e-6f ? 1.0f : 0.0f;
    if (wishLen > 0.0f) wish = normalize(wish);
    // In the grace window the face is GONE: pouring tangent speed up the
    // remembered plane is how the machine walked seven metres a second into
    // open sky above the roof. Lateral drive stays; ascent goes.
    // The same clamp applies through the CREST: the wall is released but up_
    // has not settled yet, so the tangent plane still points up the face -
    // and driving along it at full ground speed rocketed the machine four
    // metres past the lip. (Ground pace got much faster; this leak only
    // became visible then.)
    if (crestEase_ > 0.0f && state_ != MechState::Airborne && wishLen > 0.0f) {
        const Vec3 worldUp(0.0f, 1.0f, 0.0f);
        const float upComp = dot(wish, worldUp);
        if (upComp > 0.15f) {
            wish -= worldUp * (upComp - 0.15f);
            if (lengthSq(wish) > 1e-6f) wish = normalize(wish);
        }
    }
    if (climbing_ && !faceInHand && state_ != MechState::Airborne && wishLen > 0.0f) {
        const Vec3 worldUp(0.0f, 1.0f, 0.0f);
        const float upComp = dot(wish, worldUp);
        if (upComp > 0.2f) {
            wish -= worldUp * (upComp - 0.2f);
            if (lengthSq(wish) > 1e-6f) wish = normalize(wish);
        }
    }

    if (state_ != MechState::Airborne) {
        const Vec3 tangentVel = vel_ - up_ * dot(vel_, up_);
        const Vec3 target = wish * (maxSpeed * clampf(in.throttle, 0.0f, 1.0f) * wishLen);
        const Vec3 delta = target - tangentVel;
        const float maxDelta = stats_.acceleration * dt * (wishLen > 0.0f ? 1.0f : 1.6f);
        const float dl = length(delta);
        vel_ += (dl > maxDelta && dl > 1e-5f) ? delta * (maxDelta / dl) : delta;
    }

    // Gravity. The component along the surface normal is cancelled further
    // down - that is what stops a gripping machine falling off a wall. The
    // component *along* the surface has to be carried by the limbs, and until
    // it was, a machine clinging to a wall still accelerated down it at the
    // full 24 m/s^2 as though the wall were greased: it would climb two body
    // lengths, lose its footing and drop. How much of that load the legs take
    // scales with the margin between the surface they are holding and the
    // steepest they could hold, so a marginal build sags its way up a wall and
    // a specialist walks up it.
    Vec3 gravity(0.0f, kGravity, 0.0f);
    if (state_ != MechState::Airborne) {
        const float hold = gripQuality();
        const Vec3 alongNormal = up_ * dot(gravity, up_);
        const Vec3 alongSurface = gravity - alongNormal;
        gravity = alongNormal + alongSurface * (1.0f - hold);
    }
    vel_ += gravity * dt;

    // The wall ran out from under the probe - the lip, a window, a bump. All
    // the climb speed in the machine is pointed straight past the edge, and
    // carrying it through the grace window is what used to fire the hull off
    // every rooftop like a mortar round. Bleed the vertical rate down hard;
    // the legs will take it from here.
    if (climbing_ && !faceInHand && state_ != MechState::Airborne) {
        const Vec3 worldUp(0.0f, 1.0f, 0.0f);
        const float vUp = dot(vel_, worldUp);
        if (vUp > 1.1f)
            vel_ -= worldUp * ((vUp - 1.1f) * std::min(1.0f, 9.0f * dt));
    }
    // An absolute ceiling on how fast limbs can haul the hull up a face.
    // Chassis speed buys ground pace, never ascent rate - otherwise a fast
    // machine overshoots every parapet it climbs.
    if (climbing_ && state_ != MechState::Airborne) {
        const Vec3 worldUp(0.0f, 1.0f, 0.0f);
        const float vUp = dot(vel_, worldUp);
        if (vUp > 6.0f) vel_ -= worldUp * (vUp - 6.0f);
    }

    // The pull-up. Front feet gripping the roof haul the hull toward the lip;
    // without this the machine hangs at the crest with its centre of mass
    // still below the edge, which reads as stalling. Strength follows how many
    // feet have the roof, so one tentative claw tugs and three feet heave.
    if (climbing_ && mantle_ > 0.0f && state_ != MechState::Airborne) {
        const Vec3 worldUp(0.0f, 1.0f, 0.0f);
        Vec3 uphill = worldUp - climbNormal_ * dot(worldUp, climbNormal_);
        if (lengthSq(uphill) > 1e-5f) {
            // The pull ROTATES as the feet come over: an early mantle hauls
            // up the face, a committed one hauls the hull ACROSS the lip -
            // the body pours onto the roof instead of climbing straight past
            // it into the air and dropping back down, which was the top-out
            // jank (filmed: 4 m of overshoot, then a 4 m fall).
            const Vec3 over = normalize(lerp(normalize(uphill), -climbNormal_,
                                             clampf(mantle_ * 1.4f, 0.0f, 0.9f)));
            vel_ += (over * 6.5f + worldUp * 2.0f) * (mantle_ * dt);
        }
        // A pull, not a launch: the vertical rate cap tightens the more feet
        // own the roof.
        const float vyCap = lerpf(3.6f, 1.6f, clampf(mantle_ * 1.3f, 0.0f, 1.0f));
        const float vy = dot(vel_, worldUp);
        if (vy > vyCap) vel_ -= worldUp * (vy - vyCap);
        // Nothing about cresting is ballistic: total speed stays at a brisk
        // walk, so the machine STEPS onto the roof instead of skating
        // metres across it off accumulated mantle pulls.
        const float sp = length(vel_);
        const float spCap = stats_.maxSpeed * 1.2f;
        if (sp > spCap) vel_ *= spCap / sp;
    }

    // Scrabbling. A machine holding a surface near the limit of what its limbs
    // can take does not fail all at once - it loses a foot, slips, catches
    // itself, loses another. Below a comfortable margin it starts shedding
    // grip, which both looks like struggling and makes a marginal climb a
    // genuine gamble rather than a slower certainty.
    if (climbing_ && state_ != MechState::Airborne) {
        const float q = gripQuality();
        if (q < 0.62f) {
            slipTimer_ -= dt;
            if (slipTimer_ <= 0.0f) {
                Rng slipRng(seed_ * 2246822519u +
                            static_cast<uint32_t>(bobPhase_ * 1024.0f) + 13u);
                // Worse grip means slips come faster and cost more.
                const float severity = 1.0f - q / 0.62f;
                slipTimer_ = lerpf(1.4f, 0.25f, severity);
                // A jolt down the surface: the machine drops a little before it
                // catches itself. Deliberately velocity only - unplanting feet
                // here left limbs dangling with no foothold to step to, and the
                // gait has no path back from that, so the legs trailed the body
                // and stretched to twice their reach.
                const Vec3 downhill = Vec3(0.0f, -1.0f, 0.0f) -
                                      up_ * dot(Vec3(0.0f, -1.0f, 0.0f), up_);
                if (lengthSq(downhill) > 1e-5f)
                    vel_ += normalize(downhill) * (2.0f + 5.5f * severity);
                (void)slipRng;
            }
        } else {
            slipTimer_ = 0.6f;
        }
    }

    Vec3 desired = pos_ + vel_ * dt;

    Vec3 contactNormal(0.0f, 1.0f, 0.0f);
    ObstacleKind contactKind = ObstacleKind::Terrain;
    Vec3 corrected = world.resolveCollision(desired, hitRadius_, &contactNormal, &contactKind);

    // Cap how far collision may move the mech in one frame. Resolving a deep
    // overlap instantly reads as a teleport and drags planted feet past reach.
    {
        Vec3 push = corrected - desired;
        const float pushLen = length(push);
        const float maxPush = 0.35f;
        if (pushLen > maxPush) corrected = desired + push * (maxPush / pushLen);
        if (pushLen > 1e-5f) {
            const Vec3 n = normalize(push);
            const float into = dot(vel_, n);
            if (into < 0.0f) vel_ -= n * into;
        }
    }
    pos_ = corrected;

    // ---- support sampling ------------------------------------------------
    // Foot-level probes at each rest position tell us both what to stand on and
    // how high to ride. Sampling the surface (rather than averaging the planted
    // feet) keeps body height continuous: the feet land in discrete steps, but
    // the surface underneath does not.
    Vec3 pointSum(0.0f), normalSum(0.0f);
    int found = 0;
    for (const Leg& leg : legs_) {
        const Vec3 restWorld = localToWorld(leg.restLocal + Vec3(0.0f, rideHeight_ * 0.5f, 0.0f));
        const SurfaceHit h = world.findFoothold(restWorld, up_, rideHeight_ * 0.9f,
                                                stats_.legReach * 0.95f);
        if (!h.hit || !gripAllowed(h.normal)) continue;
        pointSum += h.point;
        normalSum += h.normal;
        ++found;
    }

    const bool supported = found >= 2;
    if (supported) {
        supportPoint_ = pointSum * (1.0f / static_cast<float>(found));
        supportNormal_ = normalize(normalSum);
    }

    if (state_ == MechState::Airborne) {
        airTime_ += dt;
        peakFallSpeed_ = std::max(peakFallSpeed_, length(vel_));
        apexY_ = std::max(apexY_, pos_.y);

        // Touching down needs more than "there is a surface within leg reach".
        // The foothold search looks a full limb length down, so on the way up
        // from a jump it still finds the ground the machine just left - which
        // used to land the mech about a metre into its own leap and made
        // jumping look like it did nothing at all. Require that the machine is
        // actually falling toward the surface and is close enough to stand on
        // it.
        const float aboveSupport = dot(pos_ - supportPoint_, supportNormal_);
        const bool closing = dot(vel_, supportNormal_) < 0.5f;
        if (supported && airTime_ > 0.10f && closing &&
            aboveSupport <= rideHeight_ * 1.20f) {
            // Landing. Fall damage is a height ledger, not a speed check: the
            // height a jump gained is free, because a machine that leapt 15 m
            // and landed where it started took a fall it chose and its legs
            // were built for. Only the height *beyond* what the jump earned -
            // or any real fall, walked off a roof with nothing earned - costs
            // structure.
            const float impact = std::max(0.0f, -dot(vel_, supportNormal_));
            const float fallH = std::max(0.0f, apexY_ - pos_.y);
            const float earnedH = std::max(0.0f, apexY_ - jumpLaunchY_);
            const float freeH = std::max(7.5f, earnedH * 1.10f + 2.0f);
            if (fallH > freeH) {
                const float legFactor = 1.0f / (1.0f + stats_.armor * 0.04f);
                applyDamage((fallH - freeH) * 4.0f * legFactor, -supportNormal_);
            }
            (void)impact;
            state_ = MechState::Landing;
            landTimer_ = clampf(impact / 30.0f, 0.10f, 0.45f);
            vel_ -= supportNormal_ * dot(vel_, supportNormal_);
            airTime_ = 0.0f;
            peakFallSpeed_ = 0.0f;
            // Feet down FIRST. Plant every limb on whatever is under it at
            // the moment of contact, so the absorb dip that follows is the
            // body sinking between planted feet - the legs taking the hit -
            // rather than the hull hitting dirt and legs arriving later.
            for (Leg& leg : legs_) {
                const Vec3 restWorld = localToWorld(leg.restLocal);
                const SurfaceHit h = world.findFoothold(
                    restWorld + up_ * (rideHeight_ * 0.5f), up_,
                    rideHeight_ * 0.9f, stats_.legReach * 0.9f);
                if (h.hit && gripAllowed(h.normal)) {
                    leg.foot = h.point;
                    leg.footNormal = h.normal;
                    leg.planted = true;
                    leg.stepping = false;
                    leg.stepFrom = leg.stepTo = h.point;
                    leg.stepT = 1.0f;
                }
            }
        }
    } else {
        if (!supported) {
            gripLost_ += dt;
            // Rounding a corner, the rest-position probes genuinely find
            // nothing for a few frames: the body is out past the edge with the
            // old face behind it and the new one not yet under it. A sixth of
            // a second was not enough to survive that, so every traverse ended
            // at the first corner. A machine that still holds a face gets
            // longer - which is also where the corner is allowed to be the
            // thing that kills an ill-suited build, through grip, not through
            // an arbitrary timer.
            const float patience = climbing_ ? 0.42f : 0.16f;
            // A crouched machine is pressing itself INTO the ground; it does
            // not lose grip. Letting this trip while charging flicked the
            // state out of Crouching on every terrain bump, which zeroed the
            // charge on re-entry - the bug reported as "fully charged jumps
            // don't work": five held seconds never banked more than half a
            // charge on rough ground.
            if (gripLost_ > patience && state_ != MechState::Crouching) {
                state_ = MechState::Airborne;
                airTime_ = 0.0f;
                // Nothing was earned: a fall that starts here is all real.
                jumpLaunchY_ = pos_.y;
                apexY_ = pos_.y;
            }
        } else {
            gripLost_ = 0.0f;
            // Ride at the right height above the support surface, and cancel
            // any velocity into or out of it.
            const float along = dot(pos_ - supportPoint_, up_);
            const float corr = rideHeight_ - along;
            pos_ += up_ * (corr * (1.0f - std::exp(-13.0f * dt)));
            const float vn = dot(vel_, up_);
            vel_ -= up_ * vn;
            // A little adhesion, so convex corners and overhangs do not fling
            // the machine off the moment its centre passes the edge.
            if (climb > 0.05f) vel_ -= up_ * (2.5f * dt * climb);
        }
    }

    if (state_ == MechState::Landing) {
        landTimer_ -= dt;
        if (landTimer_ <= 0.0f) state_ = MechState::Grounded;
    }

    // Hard floor. The terrain is a height field, so "am I under the ground"
    // is an exact question with a cheap answer - and it has to be asked,
    // because once a machine is below the surface it is unrecoverable by any
    // other means: findFoothold casts downward from the body, so from under
    // the ground it searches into empty space, never finds support, and the
    // machine falls out of the world forever. Nothing else in the simulation
    // can catch that, which is why this guard is unconditional rather than
    // part of the collision response.
    {
        // The floor sits at the surface itself, not a body-radius above it. A
        // machine rolling onto the foot of a wall legitimately dips its hull
        // close to the ground, and a guard set higher than the terrain fires
        // during every wall transition - which cancelled the climb before it
        // started and made climbing look broken all over again.
        const float ground = world.terrain().height(pos_.x, pos_.z);
        if (pos_.y < ground) {
            const float breach = ground - pos_.y;
            pos_.y = ground;
            if (vel_.y < 0.0f) vel_.y = 0.0f;
            supportPoint_ = Vec3(pos_.x, ground, pos_.z);
            supportNormal_ = world.terrain().normal(pos_.x, pos_.z);
            if (state_ == MechState::Airborne) {
                state_ = MechState::Landing;
                landTimer_ = 0.15f;
                airTime_ = 0.0f;
                peakFallSpeed_ = 0.0f;
            }
            // Only give up a wall for a real breach. Brushing the surface while
            // transitioning onto a face is not falling through the world.
            if (breach > 0.6f) {
                climbing_ = false;
                climbGrace_ = 0.0f;
            }
        }

        // A charging machine gets a higher floor: the hull's own clearance.
        // Deep in a crouch the support probes can start from under the
        // surface and find nothing, so the spring that normally holds ride
        // height lets go and the hard floor above catches the hull only when
        // its CENTRE reaches the dirt - the reported "the spidertank dips
        // into the ground while charging a jump". The belly never goes below
        // what the hull needs, however deep the crouch.
        if (state_ == MechState::Crouching) {
            const float minY = ground + hitRadius_ * 0.58f + 0.12f;
            if (pos_.y < minY) {
                pos_.y += (minY - pos_.y) * (1.0f - std::exp(-14.0f * dt));
                if (vel_.y < 0.0f) vel_.y = 0.0f;
            }
        }
    }

    // Unstick. Collision resolution is capped at 0.35 m per frame so a deep
    // overlap does not read as a teleport, but that cap means a machine wedged
    // between two boxes - or one that spawned inside a rubble pile - can push
    // against them forever without ever getting out. If a machine has been
    // trying to move and has not moved, walk it out along the contact normal at
    // a rate the cap cannot swallow.
    const float wanted = length(flattenY(vel_)) * dt;
    const float actual = length(pos_ - lastPos_);
    // Buried: the hull centre is actually inside something solid. That happens
    // when a machine drops through a gap in a roof, is shoved into a wall by an
    // explosion, or walks into the mouth of a ruin and the shell closes behind
    // it. It counts as stuck even if the pilot has let go of the stick, because
    // there is nothing the pilot can press that would help.
    const bool buried = world.insideSolid(pos_, hitRadius_ * 0.35f);
    const bool trying = in.throttle > 0.15f && wanted > 0.01f;
    if (buried || (trying && actual < wanted * 0.12f)) {
        wedged_ += dt;
    } else {
        wedged_ = std::max(0.0f, wedged_ - dt * 2.0f);
    }

    // Confinement is the other trap, and the one players actually report.
    // The machine is not wedged at all - it walks around perfectly well - it
    // simply walks around INSIDE something: a courtyard with no door wide
    // enough, a light well, the shell of a ruin that closed behind it. Nothing
    // above notices, because everything above asks "did it move", and it did.
    // The question that catches this one is "did it get anywhere".
    if (in.throttle > 0.15f && state_ != MechState::Airborne) {
        confineTimer_ += dt;
        // 25 m, not a few: inside a courtyard the machine can happily walk
        // twenty metres from one wall to the other, and a short reset distance
        // meant crossing its own prison counted as progress and cleared the
        // timer every time. In the open, a machine covers 25 m in two seconds
        // and the timer never gets anywhere near firing.
        if (lengthXZ(pos_ - confineRef_) > 25.0f) {
            confineRef_ = pos_;
            confineTimer_ = 0.0f;
        }
    } else {
        confineRef_ = pos_;
        confineTimer_ = std::max(0.0f, confineTimer_ - dt * 2.0f);
    }
    const bool confined = confineTimer_ > 6.0f;

    if (wedged_ > 0.45f || confined) {
        // Find the way out rather than guessing at one. The old rule pushed
        // along the deepest contact normal, which is exactly the direction that
        // cancels when a machine is pinched between two walls - it would sit
        // there shoving at both of them forever. Instead, march outward along
        // sixteen bearings; the bearing that stays clear the longest is the
        // alley mouth, the doorway, or the gap the machine fell through.
        //
        // Re-scanned four times a second rather than every frame: it is a lot
        // of solid queries, and the geometry it is reading does not move.
        escapeScan_ -= dt;
        if (escapeScan_ <= 0.0f) {
            escapeScan_ = 0.25f;
            const int kBearings = 16;
            const float kReach = 26.0f;      // far enough to see out of a yard
            float best = -1.0f;
            escapeOpen_ = 0.0f;
            // Bias towards where the pilot is asking to go, so an escape with
            // two ways out picks the one that was wanted. A preference, never
            // an override: an open bearing always beats a blocked one.
            const Vec3 want = flattenY(in.moveWorld);
            const Vec3 wantDir = (lengthSq(want) > 1e-4f) ? normalize(want) : Vec3(0.0f);
            for (int i = 0; i < kBearings; ++i) {
                const float a = (static_cast<float>(i) / static_cast<float>(kBearings)) * TAU;
                const Vec3 dir(std::cos(a), 0.0f, std::sin(a));
                float clear = 0.0f;
                for (float d = 0.8f; d <= kReach; d += 0.8f) {
                    if (world.insideSolid(pos_ + dir * d, hitRadius_ * 0.55f)) break;
                    clear = d;
                }
                escapeOpen_ = std::max(escapeOpen_, clear);
                const float score = clear + 1.5f * std::max(0.0f, dot(dir, wantDir));
                if (score > best) { best = score; escapeDir_ = dir; }
            }
        }

        // A bearing that runs most of the way to the scan limit leads OUT.
        // One that runs a few metres and stops is just the far wall of the
        // same box - taking it walks the machine across its prison.
        const bool wayOut = escapeOpen_ > 18.0f;
        if (wayOut || (wedged_ > 0.45f && escapeOpen_ > 1.5f)) {
            // Shoulder it free, fast enough that the 0.35 m per-frame
            // collision cap cannot swallow the whole correction, slow enough
            // to read as the machine forcing its way out rather than a
            // teleport.
            pos_ += escapeDir_ * (3.6f * dt);
            vel_ = flattenY(vel_) * 0.4f + escapeDir_ * 1.2f;
            vel_.y = std::max(vel_.y, 0.0f);
        } else if (wedged_ > 1.2f || confined) {
            // Boxed in on every bearing: the only way out is up, and this is a
            // machine that walks up buildings, so clambering out of a light
            // well is in character. It is also the backstop that means no
            // geometry anywhere in the game can hold a pilot permanently -
            // and it cannot be abused to levitate, because a machine merely
            // shoving at one wall in the open has fifteen other bearings
            // reading wide open and never reaches this branch.
            pos_.y += 3.4f * dt;
            vel_.y = std::max(vel_.y, 0.0f);
            climbing_ = false;
            // Keep shouldering along the widest bearing while it rises. Under
            // a covered gallery the lift alone gets nowhere - there is a slab
            // overhead - and the pair together walk the machine along the bay
            // until either the roof runs out or a doorway comes into the sweep.
            pos_ += escapeDir_ * (2.2f * dt);
        }
    } else {
        escapeScan_ = 0.0f;
    }
    lastPos_ = pos_;

    pos_ = world.clampToWorld(pos_, hitRadius_ + 1.0f);
}

// -------------------------------------------------------------------- jump

void Mech::updateJump(float dt, const World& world, const MechInput& in) {
    (void)world;
    const bool canJump = (state_ == MechState::Grounded || state_ == MechState::Landing) &&
                         stats_.jumpImpulse > 0.5f && wading_ < 0.4f;

    if (state_ == MechState::Crouching) {
        // Winding up. The legs fold, the hull settles toward the surface, and
        // the pilot decides how much to store: releasing early gives a hop,
        // holding to the stop gives everything the legs have. The charge is
        // the height control - there is no other one.
        jumpCharge_ = std::min(1.0f, jumpCharge_ + dt / kJumpChargeTime);
        crouch_ = smoothstep01(jumpCharge_);
        if (!in.jumpHeld) {
            const float commit = lerpf(0.35f, 1.0f, jumpCharge_);
            const Vec3 tangent = forward_ * clampf(in.throttle, 0.0f, 1.0f);
            vel_ += up_ * (stats_.jumpImpulse * commit) +
                    tangent * (stats_.jumpImpulse * commit * 0.42f);
            state_ = MechState::Airborne;
            airTime_ = 0.0f;
            jumpCharge_ = 0.0f;
            crouch_ = 0.0f;
            jumpLatched_ = true;
            // The ledger for fall damage: height gained by this jump is free.
            jumpLaunchY_ = pos_.y;
            apexY_ = pos_.y;
            // Feet let go so the limbs tuck rather than dragging.
            for (Leg& leg : legs_) leg.planted = false;
        }
        return;
    }

    if (in.jumpHeld && canJump && !jumpLatched_) {
        state_ = MechState::Crouching;
        jumpCharge_ = 0.0f;
    }
    if (!in.jumpHeld) jumpLatched_ = false;

    // Crouch relaxes when not charging; landing gives a brief absorb dip.
    float target = 0.0f;
    // The absorb: the body dips below stance in proportion to the impact and
    // recovers as the timer runs out. This is the "settle" half of feet-first.
    if (state_ == MechState::Landing) target = 0.72f * clampf(landTimer_ / 0.3f, 0.0f, 1.0f);
    // Follows the same continuous descent slider as the limbs: the hull
    // un-crouches as the legs unfold, instead of stepping at a threshold.
    else if (state_ == MechState::Airborne) target = lerpf(0.30f, 0.10f, airPose_);
    crouch_ = damp(crouch_, target, 9.0f, dt);
}

// -------------------------------------------------------------------- gait

void Mech::updateGait(float dt, const World& world) {
    // Smoothed velocity for step planning. Raw velocity fluctuates frame to
    // frame; using it directly to place footfalls is what made the limbs
    // visibly twitch even when the machine was walking in a straight line.
    gaitVel_ = lerp(gaitVel_, vel_, 1.0f - std::exp(-8.0f * dt));
    const Vec3 tangentVel = gaitVel_ - up_ * dot(gaitVel_, up_);
    const float speed = length(tangentVel);
    const float speedFrac = clampf(speed / std::max(stats_.maxSpeed, 0.1f), 0.0f, 1.0f);

    if (state_ == MechState::Airborne || state_ == MechState::Crouching) {
        // Two airborne postures, and the difference is the whole look of a
        // jump. Rising, the limbs tuck and ride with the body. FALLING, they
        // do what a real jumper's legs do: reach down and splay toward the
        // ground, so the feet arrive first and the body settles onto them -
        // instead of the hull bellying into the dirt with the legs still in
        // the air, which read as a crash every single time.
        // The tuck and the reach are ONE pose on a slider, not two poses with
        // a switch between them. The old code flipped at vel.y < -2 m/s: the
        // target pose jumped, the ease rate jumped with it, and the limbs
        // visibly SNAPPED open partway down - the discontinuity in the jump.
        // `descent` eases 0 (rising, tucked) to 1 (falling hard, reaching)
        // over a wide band, and every term below is a blend across it.
        const float vDown = -dot(vel_, Vec3(0.0f, 1.0f, 0.0f));
        const float descentWant = (state_ == MechState::Airborne)
                                      ? smoothstep01(clampf((vDown + 3.0f) / 9.0f,
                                                            0.0f, 1.0f))
                                      : 0.0f;
        // Time-smoothed as well as value-smoothed, so even a sudden velocity
        // change (a mid-air clip, a shove) unfolds the legs rather than
        // popping them.
        airPose_ = damp(airPose_, descentWant, 6.5f, dt);
        const float d = airPose_;

        for (Leg& leg : legs_) {
            // TUCK pose: feet drawn up under the hull, per body plan.
            const float tuck = clampf(0.55f + kneeOut_ * 0.30f, 0.55f, 0.75f);
            const Vec3 tuckLocal = leg.restLocal *
                                       lerpf(1.0f, tuck,
                                             crouch_ > 0.1f ? crouch_ : 0.8f) +
                                   Vec3(0.0f, rideHeight_ * 0.35f, 0.0f);
            // REACH pose: below stance and splayed outward, feet leading.
            const float splay2 = 0.30f + kneeOut_ * 0.35f;
            const Vec3 reachLocal = leg.restLocal +
                                    Vec3(leg.restLocal.x * splay2,
                                         -rideHeight_ * 0.40f,
                                         leg.restLocal.z * splay2);
            Vec3 target = localToWorld(lerp(tuckLocal, reachLocal, d));
            Vec3 wantNormal = up_;
            // Well into the descent, aim each foot at its actual landing spot
            // so touchdown starts from contact rather than a snap - blended
            // in by the same slider, never switched on.
            if (d > 0.15f) {
                const SurfaceHit h = world.findFoothold(
                    target + Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f),
                    1.5f, rideHeight_ * 1.6f);
                if (h.hit) {
                    const float k = clampf((d - 0.15f) / 0.45f, 0.0f, 1.0f);
                    target = lerp(target, h.point, k);
                    wantNormal = normalize(lerp(up_, h.normal, k));
                }
            }
            leg.footNormal = normalize(lerp(leg.footNormal, wantNormal,
                                            1.0f - std::exp(-9.0f * dt)));
            const float ease = lerpf(9.0f, 13.0f, d);
            leg.foot = lerp(leg.foot, target, 1.0f - std::exp(-ease * dt));
            leg.stepping = false;
            leg.planted = false;
        }
        return;
    }

    // A gait clock rather than opportunistic triggering. Each tripod owns half
    // the cycle, so footfalls land in a regular rhythm; before this the legs
    // stepped whenever they happened to drift far enough, which read as jitter.
    const float cadence = std::max(stats_.stepCadence, 0.2f);
    // Climbing runs the cycle quicker. On a wall the body is dragging itself
    // along by its feet rather than falling forward onto them, so a limb that
    // takes a leisurely quarter-second to place is a limb the hull has already
    // climbed past - which is what left the machine overreached and stepping
    // with everything at once.
    const float wallHaste = climbing_ ? 0.68f : 1.0f;
    const float cycleTime = clampf(0.95f * wallHaste /
                                   (cadence * (0.55f + 1.15f * speedFrac)), 0.24f, 1.30f);
    gaitPhase_ += dt / cycleTime;
    if (gaitPhase_ >= 1.0f) gaitPhase_ -= std::floor(gaitPhase_);

    const float swingTime = cycleTime * 0.42f;
    const float lead = speed * swingTime * 1.05f;     // land half a stride ahead
    const float trigger = 0.72f * lerpf(1.0f, 0.62f, speedFrac);
    const float reachLimit = stats_.legReach * 0.76f;
    float stepHeight = 0.30f + 0.34f * stats_.standHeight * (0.5f + 0.5f * speedFrac);
    // The gait carries the plan's character: arched plans (pod, torso) step
    // high and dainty, the crab sprawl skims its feet low over the ground.
    if (hullStyle_ == 0 || hullStyle_ == 3) stepHeight *= 1.22f;
    else if (hullStyle_ == 6) stepHeight *= 0.72f;

    // Standing still: a deadzone stops the feet shuffling in place.
    const bool moving = speed > 0.35f;

    // How many limbs are already off the surface. A hexapod is only ever
    // stable because half its legs are down, and the overreach safety valve
    // below was allowed to ignore that: climbing, every limb went urgent at
    // once, all six swung together, and the machine had a single foot on the
    // wall when it needed two to count as supported. It then dropped off,
    // which is exactly the "climbing doesn't work" the wall gait was showing.
    // Three at a time, and never the last leg of a tripod.
    int swinging = 0;
    for (const Leg& l : legs_) if (l.stepping) ++swinging;
    const int kMaxSwing = 3;

    for (Leg& leg : legs_) {
        // Where this foot should be planted right now, on whatever surface is
        // under it: the rest position, led by the smoothed travel velocity.
        const Vec3 restWorld = localToWorld(leg.restLocal);
        Vec3 anchorSearch = restWorld + tangentVel * (lead / std::max(speed, 1e-3f)) *
                            (moving ? 1.0f : 0.0f);
        // Climbing limbs reach UPHILL of their rest slot - a climber grabs
        // above itself and pulls, it does not step where it already is. The
        // bias is what stretches the front legs up the face ahead of the body.
        if (climbing_) {
            const Vec3 worldUp(0.0f, 1.0f, 0.0f);
            Vec3 uphill = worldUp - climbNormal_ * dot(worldUp, climbNormal_);
            if (lengthSq(uphill) > 1e-4f)
                anchorSearch += normalize(uphill) * (stats_.legReach * 0.13f);
        }
        SurfaceHit anchor = world.findFoothold(anchorSearch + up_ * (rideHeight_ * 0.4f),
                                               up_, rideHeight_ * 0.8f,
                                               stats_.legReach * 0.9f);

        // Over the parapet. Climbing, the search above casts into the wall
        // plane - and a limb that has risen past the top of the face finds
        // nothing there, because there is no more wall. What a real climber
        // does with that limb is reach over and grip the ROOF: the foot wraps
        // the edge, and those front feet are what pull the body up and over.
        // So when the wall search comes up empty, look straight down from just
        // past the edge for a level surface within reach.
        if (climbing_) {
            const Vec3 worldUp(0.0f, 1.0f, 0.0f);
            // Which limbs go hunting for the roof: any limb whose wall hold
            // failed - and ALSO the UPPER limbs even while the wall would
            // still take them. A climber hooks the lip the moment a hand can
            // reach it; it does not climb until its head hits the parapet
            // and then discover the roof all at once. This early hook is
            // what makes the limbs visibly WRAP AROUND the edge while the
            // body is still on the face.
            const bool upperLimb =
                dot(restWorld - pos_, worldUp) > rideHeight_ * 0.15f;
            if (!anchor.hit || !gripAllowed(anchor.normal) || upperLimb) {
                // Reach INWARD past the face. The rest position sits on the
                // wall plane, and casting straight down from there only
                // grazes the slab's top edge; the roof the foot wants is
                // beyond the parapet, a body-width in. -up_ points through
                // the wall, which above the lip means over the roof.
                const Vec3 overEdge = restWorld - up_ * 1.3f + worldUp * 0.4f;
                const SurfaceHit over = world.findFoothold(overEdge, worldUp, 1.2f,
                                                           stats_.legReach * 0.85f);
                const bool nearLip = std::fabs(over.point.y - restWorld.y) <
                                     stats_.legReach * 0.9f;
                // The eager upper-limb hook must be a LIP hold, above the
                // body centre - otherwise a mid-wall limb happily grabs an
                // interior floor slab THROUGH the wall of a hollow ruin.
                const bool lipHold = over.point.y > pos_.y + rideHeight_ * 0.05f;
                if (over.hit && over.normal.y > 0.70f && nearLip &&
                    (lipHold || !anchor.hit || !gripAllowed(anchor.normal))) {
                    anchor = over;
                }
            }
        }

        if (leg.stepping) {
            // Keep chasing the target while airborne. Freezing the landing spot
            // at lift-off is fine walking straight, but a hard turn rotates the
            // stance out from under it and the limb lands overextended.
            if (anchor.hit && gripAllowed(anchor.normal)) {
                Vec3 moving2 = anchor.point;
                const Vec3 fromHip = moving2 - leg.hipWorld;
                const float d = length(fromHip);
                const float limit = stats_.legReach * 0.78f;
                if (d > limit) moving2 = leg.hipWorld + fromHip * (limit / d);
                leg.stepTo = moving2;
                leg.stepToNormal = anchor.normal;
            }

            leg.stepT += dt / std::max(swingTime, 1e-3f);
            if (leg.stepT >= 1.0f) {
                leg.stepT = 1.0f;
                leg.stepping = false;
                leg.foot = leg.stepTo;
                leg.footNormal = leg.stepToNormal;
                leg.planted = true;
            } else {
                const float s = smoothstep01(leg.stepT);
                Vec3 p = lerp(leg.stepFrom, leg.stepTo, s);
                // The arc lifts along the surface normal, not world up, so a
                // foot on a wall swings out from the wall.
                const Vec3 lift = normalize(lerp(leg.footNormal, leg.stepToNormal, s));
                p += lift * (std::sin(leg.stepT * PI) * stepHeight);
                leg.foot = p;
                leg.planted = false;
            }
            continue;
        }

        if (!anchor.hit || !gripAllowed(anchor.normal)) {
            // Nothing to stand on here. Draw the limb back toward where it
            // wants to be rather than leaving it pinned in world space: on a
            // wall, several legs lose their footing at once and a foot left
            // behind while the body climbs away stretches to two or three
            // times its reach, which is both ugly and most of the reason a
            // climb comes apart partway up.
            const Vec3 home = localToWorld(leg.restLocal);
            leg.foot = lerp(leg.foot, home, 1.0f - std::exp(-7.0f * dt));
            leg.footNormal = up_;
            leg.planted = false;
            continue;
        }

        const float drift = length(leg.foot - anchor.point);
        const float overreach = length(leg.foot - leg.hipWorld);

        // The limb's window in the gait cycle: each tripod gets half.
        const float phase = gaitPhase_ - leg.phaseOffset;
        const float legPhase = phase - std::floor(phase);
        const bool windowOpen = legPhase < 0.46f;

        const bool wants = drift > trigger || std::fabs(dot(leg.foot - anchor.point, up_)) > 0.45f;
        // Safety valve: a planted foot near the limb's limit steps immediately,
        // whatever the gait would prefer. A visibly stretched leg is never worth
        // holding tripod discipline for.
        const bool urgent = overreach > reachLimit;

        if (!urgent) {
            if (!moving && drift < trigger * 1.6f) continue;
            if (!wants || !windowOpen) continue;
        }
        // The stability cap outranks urgency - up to a point. A stretched
        // limb is ugly; a machine with no feet on the wall falls off it; but a
        // limb at the very end of its physical reach tears free regardless,
        // because holding tripod discipline with a leg past full extension is
        // how walking used to leave limbs pinned and rubber-banding.
        const bool tearing = overreach > stats_.legReach * 0.92f;
        if (swinging >= kMaxSwing && !tearing) continue;

        Vec3 target = anchor.point;
        const Vec3 fromHip = target - leg.hipWorld;
        const float planned = length(fromHip);
        const float plannedLimit = stats_.legReach * 0.74f;
        if (planned > plannedLimit) target = leg.hipWorld + fromHip * (plannedLimit / planned);

        leg.stepFrom = leg.foot;
        leg.stepTo = target;
        leg.stepToNormal = anchor.normal;
        leg.stepT = 0.0f;
        leg.stepping = true;
        leg.planted = false;
        ++swinging;
    }

    bobPhase_ += dt * (3.0f + speed * 1.4f);
    if (bobPhase_ > TAU) bobPhase_ -= TAU;
}

// ---------------------------------------------------------------------- IK

void Mech::solveIK() {
    const PartDef* legPart = loadout_.part(Slot::Legs);
    const float reach = stats_.legReach;
    // The reach split is the chassis' to decide: a pod runs long steep
    // tibiae (the tall arch), a crab runs long flat femurs. The total is
    // always the same 87% of reach, so gameplay range never changes.
    const float coxa = reach * 0.13f;
    const float femur = reach * femurFrac_;
    const float tibia = reach * tibiaFrac_;
    (void)legPart;

    const Vec3 bodyUp = normalize(transformDir(bodyXform_, Vec3(0.0f, 1.0f, 0.0f)));

    for (Leg& leg : legs_) {
        leg.hipWorld = transformPoint(bodyXform_, leg.hipLocal);

        // The coxa is a short outward yaw joint, level with the hull.
        Vec3 outward = leg.foot - leg.hipWorld;
        const Vec3 planar = outward - bodyUp * dot(outward, bodyUp);
        const Vec3 fallback = transformDir(bodyXform_, Vec3(leg.restLocal.x, 0.0f, leg.restLocal.z));
        outward = (lengthSq(planar) > 1e-6f) ? normalize(planar) : normalize(fallback);
        leg.coxaEnd = leg.hipWorld + outward * coxa;

        // Two-bone analytic solve. The drawn foot is clamped to the limb's real
        // reach: if the gait and the geometry ever disagree, the leg bends short
        // rather than stretching.
        Vec3 toFoot = leg.foot - leg.coxaEnd;
        float dist = length(toFoot);
        const float minReach = std::fabs(femur - tibia) + 0.02f;
        const float maxReach = femur + tibia - 0.02f;
        const Vec3 dir = (dist > 1e-5f) ? toFoot * (1.0f / dist) : -bodyUp;
        leg.extension = clampf(dist / (coxa + femur + tibia), 0.0f, 1.5f);
        dist = clampf(dist, minReach, maxReach);
        leg.footSolved = leg.coxaEnd + dir * dist;

        // The knee bends away from the surface, giving the arachnid profile.
        // Plans with kneeOut_ bias the bend outward as well as up, which is
        // what flattens a crab's arch and spreads a pod's high knees.
        Vec3 bendRef = (kneeOut_ > 0.001f) ? normalize(bodyUp + outward * kneeOut_)
                                           : bodyUp;
        Vec3 perp = bendRef - dir * dot(bendRef, dir);
        if (lengthSq(perp) < 1e-6f) {
            bendRef = normalize(cross(dir, Vec3(0.0f, 0.0f, 1.0f)));
            perp = bendRef - dir * dot(bendRef, dir);
        }
        perp = normalize(perp);

        const float a = (dist * dist + femur * femur - tibia * tibia) / (2.0f * dist);
        const float hSq = femur * femur - a * a;
        leg.knee = leg.coxaEnd + dir * a + perp * ((hSq > 0.0f) ? std::sqrt(hSq) : 0.0f);
    }
}

// ------------------------------------------------------------------ turret

void Mech::updateTurret(float dt, const Vec3& aimPoint) {
    const Mat4 invBody = invertRigid(bodyXform_);
    const Vec3 local = transformPoint(invBody, aimPoint);
    const Vec3 dir = normalize(local - Vec3(0.0f, 0.55f, 0.0f));

    const float wantYaw = std::atan2(dir.x, dir.z);
    // Elevation has to reach a machine clinging to the wall above you. At 62
    // degrees anything steeper than a 2:1 slant was simply unshootable, so a
    // player who climbed was untouchable rather than exposed.
    const float wantPitch = clampf(std::asin(clampf(dir.y, -1.0f, 1.0f)),
                                   deg2rad(-38.0f), deg2rad(82.0f));

    // A hard lock, or a braced hull, swings the turret faster and holds it
    // steadier - the mechanical reason to spend either.
    float steady = (braced() ? 1.9f : 1.0f) * (locked() ? 1.5f : 1.0f);
    // The gun-handling hull: a ring built for slewing, not for carrying.
    if (stats_.trait == Trait::Gunnery) steady *= 1.45f;
    const float yawStep = 4.4f * steady * dt;
    turretYaw_ += clampf(angleDelta(turretYaw_, wantYaw), -yawStep, yawStep);
    // A casemate has no turret ring: the gun lays within a sliver of arc and
    // the rest of the aiming is the machine turning on its legs.
    if (stats_.turretYawLimit < PI)
        turretYaw_ = clampf(turretYaw_, -stats_.turretYawLimit, stats_.turretYawLimit);
    const float pitchStep = 3.4f * steady * dt;
    turretPitch_ += clampf(wantPitch - turretPitch_, -pitchStep, pitchStep);

    const PartDef* chassis = loadout_.part(Slot::Chassis);
    const float bulk = chassis ? clampf(chassis->stats.mass / 5.0f, 0.75f, 1.9f) : 1.0f;
    // Negated: Mat4::rotationX(a) sends +Z to (0, -sin a, cos a), so a positive
    // pitch aims the barrel DOWN. The turret was elevating backwards - asking
    // for 85 degrees up produced 78 down. Shots still flew true, because
    // updateWeapons aims from muzzle to aim point rather than through this
    // transform, but `aimDirection()` did not, and the AI gates its trigger on
    // that: against anything much above it the turret's reported facing was
    // twice the elevation away from the target and the machine simply never
    // fired. That is most of "enemies cannot shoot you when you climb".
    // The ring height comes from the body plan: a pod or a torso carries its
    // turret far higher than a flat shell does.
    const float mountY = (turretMountY_ > 0.0f) ? turretMountY_ : 0.50f * bulk;
    turretXform_ = bodyXform_
                 * Mat4::translation(Vec3(0.0f, mountY, 0.0f))
                 * Mat4::rotationY(turretYaw_)
                 * Mat4::rotationX(-turretPitch_);
}

Vec3 Mech::aimDirection() const {
    return normalize(transformDir(turretXform_, Vec3(0.0f, 0.0f, 1.0f)));
}

Vec3 Mech::muzzlePosition(int weaponIndex) const {
    if (weaponIndex < 0 || weaponIndex >= static_cast<int>(weapons_.size()))
        return transformPoint(turretXform_, Vec3(0.0f, 0.0f, 1.2f));
    const MountedWeapon& mw = weapons_[static_cast<size_t>(weaponIndex)];
    const float sc = mw.part ? (mw.part->size == SizeClass::Heavy ? 1.25f :
                                mw.part->size == SizeClass::Medium ? 1.0f : 0.8f) : 1.0f;
    const Mat4 frame = mw.mount.onTurret
        ? turretXform_ * Mat4::translation(mw.mount.offset)
        : bodyXform_ * Mat4::translation(mw.mount.offset);
    return transformPoint(frame, Vec3(0.0f, 0.0f, 1.05f * sc));
}

// ----------------------------------------------------------------- weapons

void Mech::updateAbility(float dt, const MechInput& in) {
    for (int i = 0; i < 4; ++i) {
        abilityCooldown_[i] = std::max(0.0f, abilityCooldown_[i] - dt);
        abilityTimer_[i] = std::max(0.0f, abilityTimer_[i] - dt);
    }
    repairPause_ += dt;

    // Passive: slow structure repair once nothing has hit you for a while. It
    // rewards breaking contact rather than making you unkillable in a fight.
    const int regen = static_cast<int>(Ability::Regenerator);
    if (stats_.hasPassive[regen] && repairPause_ > 5.0f && health_ > 0.0f)
        health_ = std::min(stats_.maxHealth,
                           health_ + stats_.passivePower[regen] * dt);

    // Overdrive leaves a heat debt: the surge is free at the time and costs you
    // afterwards, so it is a decision rather than a button you hold.
    if (overdriveDebt_ > 0.0f) {
        const float bleed = std::min(overdriveDebt_, dt * 0.35f);
        heat_ = std::min(stats_.heatCapacity * 1.05f, heat_ + bleed);
        overdriveDebt_ -= bleed;
    }

    // Each slot has its own key, its own cooldown, its own timer. Pressing Q
    // never touches what the engine or the armour is doing.
    for (int slot = 0; slot < 4; ++slot) {
        const MechStats::ActiveAbility& ab = stats_.actives[slot];
        if (!in.ability[slot] || abilityCooldown_[slot] > 0.0f ||
            ab.kind == Ability::None) continue;

        switch (ab.kind) {
            case Ability::Dash: {
                // A hard shove along the way the pilot is driving, or the
                // heading if they are stationary. Works on a wall too, which
                // is how a climbing machine crosses a gap between buildings.
                Vec3 dir = in.moveWorld - up_ * dot(in.moveWorld, up_);
                if (lengthSq(dir) < 1e-4f) dir = forward_;
                vel_ += normalize(dir) * ab.power;
                break;
            }
            case Ability::Brace:
                // Handled in locomotion and aiming; the timer is the state.
                break;
            case Ability::Overdrive:
                heat_ = std::max(0.0f, heat_ - stats_.heatCapacity * 0.35f);
                overdriveDebt_ += stats_.heatCapacity * 0.55f;
                break;
            case Ability::VentHeat:
                heat_ = 0.0f;
                overheated_ = false;
                break;
            case Ability::Siphon: {
                // The reactor poured into the frame. Real structure back, in
                // the middle of a fight, paid for in heat you now have to
                // manage - which is the trade that makes it a decision and
                // not a free heal.
                const float back = stats_.maxHealth * ab.power;
                if (health_ > 0.0f)
                    health_ = std::min(health_ + back, stats_.maxHealth);
                heat_ = std::min(stats_.heatCapacity * 1.02f,
                                 heat_ + stats_.heatCapacity * 0.60f);
                siphonFlash_ = 1.0f;
                break;
            }
            case Ability::EmpSurge:
                // The blast itself is resolved by the mission, which is the
                // only thing that can see the units. All the machine does is
                // announce that it happened, and pay for it.
                empPulse_ = 1.0f;
                heat_ = std::min(stats_.heatCapacity * 1.02f,
                                 heat_ + stats_.heatCapacity * 0.45f);
                break;
            case Ability::Capacitor: {
                // Every cooldown gun comes back at once and every limited rack
                // gets a magazine. An alpha strike you have to earn the moment
                // for, rather than a sustained increase.
                for (MountedWeapon& mw : weapons_) {
                    if (!mw.part) continue;
                    mw.cooldown = 0.0f;
                    if (mw.part->weapon.ammo == AmmoKind::Limited) {
                        mw.rounds = std::max(mw.rounds, mw.part->weapon.magazine);
                        mw.reserve += 1;
                    }
                }
                heat_ = std::min(stats_.heatCapacity * 1.02f,
                                 heat_ + stats_.heatCapacity * 0.30f);
                break;
            }
            case Ability::SilentRun:
                // Handled by the timer: while it runs, the machine's
                // signature is a fraction of normal and hostiles lose it.
                break;
            case Ability::Bulwark:
            case Ability::TargetLock:
                break;
            default:
                continue;
        }
        abilityTimer_[slot] = ab.duration;
        abilityCooldown_[slot] = ab.cooldown;
    }
}

void Mech::updateWeapons(float dt, const MechInput& in, std::vector<ShotRequest>& shotsOut) {
    heat_ = std::max(0.0f, heat_ - stats_.coolRate * dt);
    if (overheated_ && heat_ < stats_.heatCapacity * 0.35f) overheated_ = false;
    if (heat_ >= stats_.heatCapacity) overheated_ = true;
    recoil_ = damp(recoil_, 0.0f, 8.0f, dt);

    Rng rng(seed_ * 40503u + static_cast<uint32_t>(bobPhase_ * 4096.0f) + 7u);

    for (size_t i = 0; i < weapons_.size(); ++i) {
        MountedWeapon& mw = weapons_[i];
        mw.cooldown = std::max(0.0f, mw.cooldown - dt);
        mw.spin = damp(mw.spin, 0.0f, 7.0f, dt);
        if (!mw.part) continue;
        const WeaponDef& w = mw.part->weapon;

        // ---- handling state -------------------------------------------
        // A rotary spools up while its trigger is down and winds back down
        // when it is not, so opening fire costs you a moment and letting go
        // costs you that moment again. The cone opens as rounds go out and
        // closes when they stop, which is what turns "hold the trigger" into
        // a decision instead of the only thing you ever do.
        {
            const bool held = (mw.group == 0) ? in.fireHeld : in.fire2Held;
            if (w.spinUp > 0.0f) {
                const float rate = 1.0f / std::max(w.spinUp, 0.05f);
                mw.spool = clampf(mw.spool + (held ? rate : -rate * 0.8f) * dt,
                                  0.0f, 1.0f);
            } else {
                mw.spool = 1.0f;
            }
            if (!held) {
                mw.burstLeft = 0;
                mw.bloom = std::max(0.0f, mw.bloom - w.bloomRecover * 1.6f * dt);
            } else {
                mw.bloom = std::max(0.0f, mw.bloom - w.bloomRecover * dt);
            }
        }

        // Number keys 1-4 flip a mount in and out of its trigger group;
        // 5-8 move it between the left and right triggers. The reassignment
        // writes through to the loadout so the workshop and the save keep it.
        if (in.toggleMask & (1 << i)) mw.enabled = !mw.enabled;
        if (in.toggleMask & (1 << (i + 4))) {
            mw.group ^= 1;
            if (i < loadout_.weaponGroups.size())
                loadout_.weaponGroups[i] = mw.group;
        }

        const bool trigger = (mw.group == 0) ? in.fireHeld : in.fire2Held;
        if (!mw.enabled || !trigger || overheated_ || mw.cooldown > 0.0f) continue;

        // Artillery only speaks planted. Light guns still work on the move -
        // self-defense is allowed - but the rack that justifies the hull
        // holds fire until the legs are still or braced.
        if (stats_.plantToFire && mw.part->size != SizeClass::Light &&
            speed() > 1.0f && !braced()) continue;

        if (w.ammo == AmmoKind::Limited && mw.rounds <= 0) {
            // Empty: pull a magazine off the rack if one was bought or salvaged.
            // The reload costs a beat, which is the price of running heavy guns.
            if (mw.reserve <= 0) continue;
            const int take = std::min(mw.reserve, w.magazine);
            mw.reserve -= take;
            mw.rounds = take;
            mw.cooldown = 1.4f;
            continue;
        }

        const Vec3 muzzle = muzzlePosition(static_cast<int>(i));
        Vec3 dir = in.aimPoint - muzzle;
        dir = (lengthSq(dir) > 1e-4f) ? normalize(dir) : aimDirection();
        // Fire control: pull the shot toward the sensor's firing solution,
        // but only when the pilot is already aiming close to it. The assist
        // finishes an aim the pilot started; it never acquires on its own.
        if (in.assistStrength > 0.01f) {
            Vec3 want = in.assistPoint - muzzle;
            if (lengthSq(want) > 1e-4f) {
                want = normalize(want);
                const float cosang = dot(want, dir);
                if (cosang > 0.990f) {   // within ~8 degrees
                    const float w2 = clampf(in.assistStrength, 0.0f, 0.95f);
                    dir = normalize(lerp(dir, want, w2));
                }
            }
        }
        // Bracing, a hard lock and a rangefinder all tighten the cone the
        // combat layer will scatter this shot into.
        float spreadScale = 1.0f;
        if (braced()) spreadScale *= 0.35f;
        if (locked()) spreadScale *= (1.0f - 0.6f * engagedPower(Ability::TargetLock));
        const int rf = static_cast<int>(Ability::Rangefinder);
        if (stats_.hasPassive[rf]) spreadScale *= (1.0f - stats_.passivePower[rf]);
        // Two hulls that answer the same question - "what does moving cost my
        // gunnery" - in opposite directions. A siege deck is a firing
        // PLATFORM: parked, it groups like a tripod. A raider is built to
        // shoot on the run and pays nothing for it. Everything else sits
        // between the two, losing accuracy with speed the ordinary way.
        {
            const float sp = length(flattenY(vel_));
            const float moving = clampf(sp / std::max(stats_.maxSpeed, 1.0f),
                                        0.0f, 1.0f);
            if (stats_.trait == Trait::Platform)
                spreadScale *= lerpf(0.55f, 1.15f, moving);
            else if (stats_.trait == Trait::Sprinter)
                spreadScale *= 1.0f;
            else
                spreadScale *= lerpf(1.0f, 1.25f, moving);
        }

        // One request per trigger pull. Pellet count and cone spread are the
        // combat layer's business - doing it here as well would apply the
        // scatter twice and turn every shotgun into a fog.
        ShotRequest s;
        s.origin = muzzle;
        s.direction = dir;
        s.weapon = &w;
        s.team = team_;
        s.shooter = index_;
        // Bloom rides on TOP of everything else and is the only term allowed
        // above one: bracing a hosed-out chaingun should still be worse than
        // bracing a rifle.
        spreadScale = clampf(spreadScale, 0.05f, 1.0f) * (1.0f + mw.bloom);
        s.spreadScale = clampf(spreadScale, 0.05f, 6.0f);
        // An assault gun's warheads are simply bigger. It applies to the
        // machine's own rounds only, so the trait travels with the hull
        // rather than with the weapon.
        if (stats_.trait == Trait::Breacher) s.blastScale = 1.35f;
        // The fire platform's real identity. Long optics were giving it a
        // better firing solution and a longer sensor reach, but its rounds
        // still expired at exactly the range everything else's did - so a
        // sniper build had nowhere it could shoot from that its targets could
        // not shoot back at, and the second tier of enemies (marksmen at 330
        // metres, mortars at 250) took that ground away entirely. A third
        // more reach is a niche: engage from outside what answers.
        if (stats_.trait == Trait::Spotter) s.rangeScale = 1.35f;
        // Seeker guidance steers capable missiles; guns are beneath it.
        const int homing = static_cast<int>(Ability::Homing);
        if (w.homingCapable && stats_.hasPassive[homing])
            s.homing = 0.9f * stats_.passivePower[homing];
        shotsOut.push_back(s);

        // Spin-up stretches the interval: at a dead stop a rotary fires at a
        // third of its rate and climbs to full over its spool time.
        float interval = w.fireInterval;
        if (w.spinUp > 0.0f) interval /= lerpf(0.34f, 1.0f, mw.spool);
        mw.bloom = std::min(mw.bloom + w.bloomPerShot,
                            (w.bloomMax > 0.0f) ? w.bloomMax : 0.0f);
        if (w.burstCount > 0) {
            if (mw.burstLeft <= 0) mw.burstLeft = w.burstCount;
            --mw.burstLeft;
        }
        mw.cooldown = (w.ammo == AmmoKind::Cooldown)
                          ? w.cooldownTime
                          : ((w.burstCount > 0 && mw.burstLeft <= 0)
                                 ? w.burstGap
                                 : interval);
        mw.spin = 1.0f;
        heat_ = std::min(stats_.heatCapacity * 1.05f, heat_ + w.heatPerShot);
        recoil_ = std::min(1.0f, recoil_ + w.recoil * 0.4f *
                                     (stats_.trait == Trait::Gunnery ? 0.45f : 1.0f));
        if (w.ammo == AmmoKind::Limited) --mw.rounds;
    }
}

void Mech::scaleHealth(float factor) {
    const float f = std::max(0.05f, factor);
    const float frac = (stats_.maxHealth > 0.0f) ? (health_ / stats_.maxHealth) : 1.0f;
    stats_.maxHealth *= f;
    health_ = stats_.maxHealth * frac;
}

void Mech::refillAmmo() {
    for (MountedWeapon& mw : weapons_) {
        if (!mw.part) continue;
        if (mw.part->weapon.ammo == AmmoKind::Limited)
            mw.rounds = mw.part->weapon.magazine;
    }
}

// ------------------------------------------------------------------ damage

void Mech::applyDamage(float amount, const Vec3& fromDirection) {
    if (health_ <= 0.0f) return;
    repairPause_ = 0.0f;
    lastHitDir_ = normalize(fromDirection + Vec3(1e-5f, 0.0f, 0.0f));
    lastHitAge_ = 0.0f;

    // Directional armour. The round arrived travelling along `fromDirection`;
    // compare it against the hull's facing to decide which plate it met.
    // Front-on fire meets the glacis and loses a third of its bite; anything
    // that worked its way behind you finds the engine deck. This is what makes
    // facing, flanking and cover geometry decisions rather than scenery - and
    // it applies to every machine, so circling a casemate is how you kill one.
    {
        const Vec3 incoming = normalize(fromDirection - up_ * dot(fromDirection, up_) +
                                        Vec3(1e-5f, 0.0f, 0.0f));
        const float along = dot(incoming, forward_);   // -1 = head-on, +1 = from behind
        float mult;
        if (along < -0.42f)      mult = stats_.frontDamageMult;
        else if (along > 0.42f)  mult = stats_.rearDamageMult;
        else                     mult = 1.0f;
        amount *= mult;
        lastHitFromRear_ = along > 0.42f;
    }

    // Reactive plating spends itself on the first heavy hit of a wave. It is
    // what makes a big incoming shell survivable once, not a general shield.
    const int reactive = static_cast<int>(Ability::ReactivePlate);
    if (stats_.hasPassive[reactive] && !reactiveSpent_ &&
        amount > stats_.maxHealth * 0.10f) {
        amount *= (1.0f - stats_.passivePower[reactive]);
        reactiveSpent_ = true;
    }
    // Bulwark is the active version: angle the plating and eat everything for
    // a few seconds.
    if (engagedKind(Ability::Bulwark))
        amount *= (1.0f - engagedPower(Ability::Bulwark));

    // Armour works two ways, and it needs both to be a real build choice.
    // FLAT subtraction is what makes a heavy machine ignore small arms
    // entirely. PROPORTIONAL reduction - which flat mitigation never gave -
    // is what lets that same machine survive a tank shell, so "plate up and
    // walk into it" is a viable answer to a heavy gun instead of a way to
    // die slightly slower. Diminishing, capped at 45%, and nothing is ever
    // immune: a floor of 10% always gets through.
    // A sloped glacis only works if you are FACING the thing shooting at you,
    // which is the whole character of a casemate hull: it is enormously tough
    // from the front and ordinary from anywhere else, so driving it well means
    // keeping your nose pointed at the trouble.
    float traitScale = 1.0f;
    if (stats_.trait == Trait::Frontal && lengthSq(fromDirection) > 1e-6f) {
        const float facing = dot(normalize(flattenY(forward_)),
                                 normalize(flattenY(-fromDirection) +
                                           Vec3(0.0f, 0.0f, 1e-5f)));
        if (facing > 0.55f) traitScale = lerpf(1.0f, 0.62f,
                                               clampf((facing - 0.55f) / 0.45f,
                                                      0.0f, 1.0f));
    }
    amount *= traitScale;

    const float prop = 0.45f * (stats_.armor / (stats_.armor + 9.0f));
    const float afterProp = amount * (1.0f - prop);
    const float mitigated = std::max(afterProp - stats_.armor, amount * 0.10f);
    health_ -= mitigated;
    damageFlash_ = 1.0f;
    if (health_ <= 0.0f) {
        health_ = 0.0f;
        state_ = MechState::Destroyed;
    }
}

// ------------------------------------------------------------------ update

void Mech::update(float dt, const World& world, const MechInput& in,
                  std::vector<ShotRequest>& shotsOut) {
    if (dt <= 0.0f) return;
    dt = std::min(dt, 0.05f);
    damageFlash_ = damp(damageFlash_, 0.0f, 6.0f, dt);
    siphonFlash_ = damp(siphonFlash_, 0.0f, 2.2f, dt);
    {
        float fastest = 0.0f;
        for (const MountedWeapon& mw : weapons_)
            if (mw.part && mw.part->weapon.spinUp > 0.0f)
                fastest = std::max(fastest, mw.spool);
        spinPhase_ += dt * 26.0f * fastest;
        if (spinPhase_ > TAU * 128.0f) spinPhase_ -= TAU * 128.0f;
    }

    if (state_ == MechState::Destroyed) {
        // Dead weight: fall, settle, stop responding.
        vel_ += Vec3(0.0f, kGravity, 0.0f) * dt;
        pos_ += vel_ * dt;
        const SurfaceHit h = world.findFoothold(pos_, Vec3(0.0f, 1.0f, 0.0f), 4.0f, 60.0f);
        if (h.hit && dot(pos_ - h.point, Vec3(0.0f, 1.0f, 0.0f)) < hitRadius_ * 0.6f) {
            pos_.y = h.point.y + hitRadius_ * 0.6f;
            vel_ = vel_ * 0.4f;
        }
        rideHeight_ = damp(rideHeight_, stats_.standHeight * 0.25f, 3.0f, dt);
        bodyXform_ = Mat4::translation(pos_) *
                     Mat4::basis(normalize(cross(up_, forward_)), up_, forward_);
        solveIK();
        return;
    }

    // Ride height follows the crouch, which the jump and landing drive - but
    // never below what the hull physically needs. A fully charged crouch on a
    // heavy chassis used to press the belly plate straight through the ground;
    // the machine squats to its clearance and no further.
    const float hullClearance = hitRadius_ * 0.58f + 0.12f;
    const float wantRide = std::max(stats_.standHeight * lerpf(1.0f, 0.42f, crouch_),
                                    hullClearance);
    rideHeight_ = damp(rideHeight_, wantRide, 12.0f, dt);

    lastHitAge_ += dt;
    updateAbility(dt, in);
    updateJump(dt, world, in);
    updateOrientation(dt, world, in);

    // Body lean. The hull frame tips with smoothed acceleration - nose up
    // under a surge, banked into a hard turn, dipped when braking - which is
    // most of what makes forty tonnes on legs FEEL like forty tonnes.
    {
        const Vec3 accel = (vel_ - prevFrameVel_) * (1.0f / std::max(dt, 1e-4f));
        prevFrameVel_ = vel_;
        leanAccel_ = lerp(leanAccel_, accel, 1.0f - std::exp(-5.0f * dt));
    }
    const float levelness = clampf(up_.y, 0.0f, 1.0f);
    const Vec3 rightAxis = normalize(cross(up_, forward_));
    const float leanPitch = clampf(dot(leanAccel_, forward_) * -0.0045f,
                                   -0.085f, 0.085f) * levelness;
    const float leanRoll = clampf(dot(leanAccel_, rightAxis) * 0.0040f,
                                  -0.070f, 0.070f) * levelness;
    Mat4 leanM = Mat4::rotationX(leanPitch) * Mat4::rotationZ(leanRoll);

    // The pull-up. On a wall the body does not glide, it surges: each tripod's
    // grip hauls the hull a hand-width up the face with a nod into the wall,
    // eased in and out so grabbing or leaving the wall never pops. Purely a
    // body-frame motion - pos_ and the physics never see it.
    climbPose_ = damp(climbPose_, climbing_ ? 1.0f : 0.0f, 5.0f, dt);
    Vec3 heaveOff(0.0f, 0.0f, 0.0f);
    if (climbPose_ > 0.01f) {
        const float ph = gaitPhase_ * TAU * 2.0f;    // one surge per tripod
        heaveOff = up_ * (std::sin(ph) * 0.085f * bulk_ * climbPose_);
        leanM = leanM * Mat4::rotationX(std::sin(ph + 0.9f) * 0.038f * climbPose_);
    }
    // The crest follow-through. For half a second after releasing the wall
    // at the top: the nose dips over the lip and comes back up (the body
    // pouring onto the roof), and a gentle forward pull walks the mass over
    // the edge instead of trusting one impulse to carry it.
    if (crestEase_ > 0.0f) {
        crestEase_ = std::max(0.0f, crestEase_ - dt / 0.55f);
        const float arc = std::sin(crestEase_ * PI);        // 0 -> 1 -> 0
        leanM = leanM * Mat4::rotationX(arc * 0.085f);      // nose-over
        if (state_ != MechState::Airborne) {
            Vec3 fwdFlat = forward_ - up_ * dot(forward_, up_);
            if (lengthSq(fwdFlat) > 1e-5f)
                vel_ += normalize(fwdFlat) * (dt * 5.0f * arc);
        }
        // Absolute caps through the crest - not chassis-relative, or a fast
        // machine simply flies off the roof it just climbed.
        const Vec3 worldUp2(0.0f, 1.0f, 0.0f);
        const float vUp2 = dot(vel_, worldUp2);
        if (vUp2 > 2.5f) vel_ -= worldUp2 * (vUp2 - 2.5f);
        const float sp = length(vel_);
        const float spCap = std::min(stats_.maxSpeed * 0.9f, 7.0f);
        if (sp > spCap) vel_ *= spCap / sp;
    }

    bodyXform_ = Mat4::translation(pos_ + heaveOff) *
                 Mat4::basis(rightAxis, up_, forward_) * leanM;

    updateLocomotion(dt, world, in);

    // Rebuild the frame after locomotion so the gait and IK see this frame's
    // position rather than last frame's.
    bodyXform_ = Mat4::translation(pos_ + heaveOff) *
                 Mat4::basis(normalize(cross(up_, forward_)), up_, forward_) * leanM;

    updateGait(dt, world);
    updateTurret(dt, in.aimPoint);
    updateWeapons(dt, in, shotsOut);
    solveIK();
}

} // namespace sb
