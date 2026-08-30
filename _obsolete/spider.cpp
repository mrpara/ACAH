#include "spider.h"

namespace sb {

namespace {

// Palette. Tuned for the full-colour mode as much as the phosphor one: the top
// plates are the lightest surfaces so the bot reads from above, the limbs sit
// mid-range so they stop blowing out to white from below, and a single warm
// accent gives the machine an identity against an otherwise green world.
const Vec3 kHullDeck (0.46f, 0.52f, 0.60f);   // upper plates - brightest
const Vec3 kHullBody (0.24f, 0.28f, 0.35f);   // chassis flanks
const Vec3 kHullDark (0.19f, 0.21f, 0.24f);   // recesses, turret ring
const Vec3 kLimb     (0.33f, 0.35f, 0.39f);   // leg segments
const Vec3 kJoint    (0.47f, 0.50f, 0.55f);   // shoulders, knees
const Vec3 kAccent   (0.82f, 0.42f, 0.10f);   // warning amber
const Vec3 kBarrel   (0.25f, 0.27f, 0.30f);
const Vec3 kEye      (0.40f, 1.00f, 0.60f);

// A flat hexagonal slab, which reads as a machined plate from any angle.
Mesh makePlate(float halfWidth, float thickness, float halfLength, const Vec3& color) {
    Mesh m = makeCylinder(1.0f, 0.82f, 1.0f, 6, true, true, color);
    // Rotate so a flat edge faces forward rather than a vertex.
    transformMesh(m, Mat4::rotationY(deg2rad(30.0f)));
    transformMesh(m, Mat4::scaling(Vec3(halfWidth, thickness, halfLength)));
    m.computeBounds();
    return m;
}

} // namespace

void SpiderBot::buildMeshes() {
    chassisMesh_ = makePlate(1.0f, 1.0f, 1.0f, kHullBody);
    deckMesh_ = makePlate(1.0f, 1.0f, 1.0f, kHullDeck);
    accentMesh_ = makeBox(Vec3(1.0f, 1.0f, 1.0f), kAccent);

    shoulderMesh_ = makeSphere(1.0f, 4, 6, kJoint);
    segmentMesh_ = makeCylinder(1.0f, 0.66f, 1.0f, 6, true, true, kLimb);
    jointMesh_ = makeSphere(1.0f, 4, 6, kJoint);
    footMesh_ = makeCylinder(0.55f, 1.0f, 1.0f, 5, true, true, kHullDark);

    turretBaseMesh_ = makeCylinder(1.0f, 0.86f, 1.0f, 8, true, true, kHullDark);
    turretHeadMesh_ = makeBox(Vec3(1.0f, 1.0f, 1.0f), kHullBody);
    barrelMesh_ = makeCylinder(1.0f, 0.78f, 1.0f, 6, true, true, kBarrel);
    eyeMesh_ = makeSphere(1.0f, 4, 6, kEye);
    antennaMesh_ = makeCylinder(1.0f, 0.2f, 1.0f, 4, false, false, kJoint);
}

void SpiderBot::init(const World& world, const Vec3& spawnXZ, uint32_t seed) {
    (void)seed;
    buildMeshes();

    const Terrain& t = world.terrain();
    pos_ = Vec3(spawnXZ.x, t.height(spawnXZ.x, spawnXZ.z) + cfg_.standHeight, spawnXZ.z);
    bodyHeight_ = pos_.y;
    vel_ = Vec3(0.0f);
    bodyYaw_ = 0.0f;

    // Layout: three legs a side, splayed outward. Groups alternate so that
    // {front-left, mid-right, back-left} and its mirror form the two tripods.
    const float h = cfg_.standHeight;
    const Vec3 hips[6] = {
        {-0.75f, 0.0f,  1.05f}, { 0.75f, 0.0f,  1.05f},
        {-0.85f, 0.0f,  0.00f}, { 0.85f, 0.0f,  0.00f},
        {-0.75f, 0.0f, -1.05f}, { 0.75f, 0.0f, -1.05f},
    };
    const Vec3 rests[6] = {
        {-2.20f, -h,  1.85f}, { 2.20f, -h,  1.85f},
        {-2.65f, -h,  0.00f}, { 2.65f, -h,  0.00f},
        {-2.20f, -h, -1.85f}, { 2.20f, -h, -1.85f},
    };
    const int groups[6] = {0, 1, 1, 0, 0, 1};

    for (int i = 0; i < 6; ++i) {
        Leg& leg = legs_[static_cast<size_t>(i)];
        leg.hipLocal = hips[i];
        leg.restLocal = rests[i];
        leg.group = groups[i];
        const Vec3 world0(pos_.x + rests[i].x, 0.0f, pos_.z + rests[i].z);
        leg.foot = Vec3(world0.x, t.height(world0.x, world0.z), world0.z);
        leg.stepFrom = leg.stepTo = leg.foot;
        leg.stepT = 1.0f;
        leg.stepping = false;
    }

    bodyXform_ = Mat4::translation(pos_) * Mat4::rotationY(bodyYaw_);
    updateBodyPose(0.0f, world);
    solveIK();
}

int SpiderBot::legsInAir() const {
    int n = 0;
    for (const Leg& l : legs_) if (l.stepping) ++n;
    return n;
}

void SpiderBot::update(float dt, const World& world, const Vec3& moveDir, const Vec3& aimPoint) {
    if (dt <= 0.0f) return;

    // ---------------------------------------------------------- locomotion --
    const Vec3 wish = flattenY(moveDir);
    const float wishLen = std::min(lengthXZ(wish), 1.0f);
    if (wishLen > 0.001f) {
        const Vec3 target = normalize(wish) * (cfg_.maxSpeed * wishLen);
        const Vec3 delta = target - vel_;
        const float maxDelta = cfg_.acceleration * dt;
        const float dl = lengthXZ(delta);
        vel_ += (dl > maxDelta) ? normalize(delta) * maxDelta : delta;
    } else {
        const float sp = lengthXZ(vel_);
        const float drop = cfg_.braking * dt;
        vel_ = (sp <= drop) ? Vec3(0.0f) : vel_ * ((sp - drop) / sp);
    }

    Vec3 desired = pos_ + vel_ * dt;
    desired.y = pos_.y;
    Vec3 corrected = world.resolveCollision(desired, 1.35f);

    // Cap how far collision may move the bot in one frame. Resolving a deep
    // overlap instantly reads as a teleport: the body jumps to new ground height
    // and drags the planted feet past their reach. Bleeding it out over a few
    // frames is invisible and keeps the pose continuous.
    {
        Vec3 push = corrected - desired;
        const float pushLen = lengthXZ(push);
        const float maxPush = 0.30f;
        if (pushLen > maxPush) corrected = desired + push * (maxPush / pushLen);
    }
    // If collision moved us, bleed off the velocity component into the obstacle
    // so the bot slides along it instead of grinding to a halt.
    const Vec3 slide = corrected - desired;
    if (lengthSq(slide) > 1e-6f) {
        const Vec3 n = normalize(Vec3(slide.x, 0.0f, slide.z));
        const float into = dot(vel_, n);
        if (into < 0.0f) vel_ -= n * into;
    }
    pos_.x = corrected.x;
    pos_.z = corrected.z;

    // The chassis yaws to face the direction of travel; the turret is free.
    if (lengthXZ(vel_) > 0.35f) {
        const float travelYaw = std::atan2(vel_.x, vel_.z);
        const float d = angleDelta(bodyYaw_, travelYaw);
        const float maxTurn = cfg_.bodyTurnRate * dt;
        bodyYaw_ += clampf(d, -maxTurn, maxTurn);
    }

    recoil_ = damp(recoil_, 0.0f, 9.0f, dt);

    updateGait(dt, world);
    updateBodyPose(dt, world);
    updateTurret(dt, aimPoint);
    solveIK();
}

Vec3 SpiderBot::localToWorldXZ(const Vec3& local) const {
    const float cosY = std::cos(bodyYaw_), sinY = std::sin(bodyYaw_);
    return Vec3(pos_.x + local.x * cosY + local.z * sinY, 0.0f,
                pos_.z - local.x * sinY + local.z * cosY);
}

float SpiderBot::maxLegReach() const {
    return cfg_.coxaLen + cfg_.femurLen + cfg_.tibiaLen;
}

void SpiderBot::updateGait(float dt, const World& world) {
    const Terrain& t = world.terrain();

    // A tripod may only lift once the opposite tripod is fully planted.
    bool groupStepping[2] = {false, false};
    for (const Leg& leg : legs_) if (leg.stepping) groupStepping[leg.group] = true;

    const float speed = lengthXZ(vel_);
    const float speedFrac = clampf(speed / cfg_.maxSpeed, 0.0f, 1.0f);

    // Faster movement shortens the swing.
    const float stepTime = cfg_.stepDuration * lerpf(1.30f, 0.55f, speedFrac);

    // Where a freshly planted foot should be: ahead of the rest position by half
    // a stride, so the foot sweeps symmetrically around rest instead of spending
    // the whole cycle trailing behind it. A gait cycle is two swings, so half a
    // stride is one swing time of travel - hence stepLead near 1.
    //
    // Getting this wrong is what made limbs stretch during sustained walking:
    // the body outran its own feet and the planted foot ended up past the limb's
    // reach. All drift is now measured against this anchor rather than against
    // the bare rest position, which keeps the stride centred at any speed.
    const float lead = cfg_.stepLead * stepTime;

    // At speed the trigger tightens, so the cycle turns over faster.
    const float trigger = cfg_.stepTrigger * lerpf(1.0f, 0.60f, speedFrac);
    const float reachLimit = maxLegReach() * cfg_.reachSafety;

    for (Leg& leg : legs_) {
        const Vec3 restXZ = localToWorldXZ(leg.restLocal);
        const Vec3 anchorXZ(restXZ.x + vel_.x * lead, 0.0f, restXZ.z + vel_.z * lead);
        const Vec3 anchor(anchorXZ.x, t.height(anchorXZ.x, anchorXZ.z), anchorXZ.z);

        if (leg.stepping) {
            // Keep chasing the anchor while the foot is in the air. Freezing the
            // landing spot at lift-off is fine walking in a straight line, but a
            // hard turn rotates the whole stance out from under it - the target
            // goes stale mid-swing and the leg lands already overextended.
            {
                Vec3 moving = anchor;
                const Vec3 hipNow = transformPoint(bodyXform_, leg.hipLocal);
                const Vec3 fromHip = moving - hipNow;
                const float d = length(fromHip);
                const float limit = maxLegReach() * 0.80f;
                if (d > limit) moving = hipNow + fromHip * (limit / d);
                moving.y = t.height(moving.x, moving.z);
                leg.stepTo = moving;
            }

            leg.stepT += dt / std::max(stepTime, 1e-3f);
            if (leg.stepT >= 1.0f) {
                leg.stepT = 1.0f;
                leg.stepping = false;
                leg.foot = leg.stepTo;
                leg.foot.y = t.height(leg.foot.x, leg.foot.z);
            } else {
                const float s = smoothstep01(leg.stepT);
                Vec3 p = lerp(leg.stepFrom, leg.stepTo, s);
                // Parabolic lift, plus a guarantee the foot never sinks into
                // ground that rises under it mid-swing.
                p.y += std::sin(leg.stepT * PI) * cfg_.stepHeight * (0.7f + 0.5f * speedFrac);
                const float ground = t.height(p.x, p.z);
                p.y = std::max(p.y, ground + 0.04f);
                leg.foot = p;
            }
            continue;
        }

        const float drift = lengthXZ(leg.foot - anchor);
        const float verticalDrift = std::fabs(leg.foot.y - anchor.y);
        const bool wantsStep = drift > trigger || verticalDrift > 0.5f;

        // Safety valve: a planted foot approaching the limb's reach must step
        // now, whatever the gait would prefer. Holding tripod discipline at the
        // cost of a visibly stretched leg is never the right trade.
        const Vec3 hipWorld = transformPoint(bodyXform_, leg.hipLocal);
        const bool overreaching = length(leg.foot - hipWorld) > reachLimit;

        if (!wantsStep && !overreaching) continue;
        if (!overreaching && groupStepping[1 - leg.group]) continue;

        Vec3 target = anchor;
        // Never plan a landing the leg cannot actually reach.
        const Vec3 fromHip = target - hipWorld;
        const float planned = length(fromHip);
        const float plannedLimit = maxLegReach() * 0.80f;
        if (planned > plannedLimit) target = hipWorld + fromHip * (plannedLimit / planned);

        // Nudge the landing spot away from anything solid.
        target = world.resolveCollision(target, 0.35f);
        target.y = t.height(target.x, target.z);

        leg.stepFrom = leg.foot;
        leg.stepTo = target;
        leg.stepT = 0.0f;
        leg.stepping = true;
        groupStepping[leg.group] = true;
    }
}

void SpiderBot::updateBodyPose(float dt, const World& world) {
    const Terrain& t = world.terrain();

    // Height and attitude are driven by sampling the terrain under the bot's own
    // footprint rather than by averaging the planted feet.
    //
    // The foot average is piecewise constant: it only changes when a foot lands,
    // so the body height moved in visible steps and the whole chassis twitched
    // on every touchdown. Sampling the height field at the six rest positions is
    // a continuous function of position and heading, so the body now rises and
    // tilts smoothly as it walks, and the legs simply follow.
    float supportSum = 0.0f;
    float supportMax = -1e9f;
    float frontY = 0.0f, backY = 0.0f, leftY = 0.0f, rightY = 0.0f;
    for (int i = 0; i < 6; ++i) {
        const Leg& leg = legs_[static_cast<size_t>(i)];
        const Vec3 xz = localToWorldXZ(leg.restLocal);
        const float h = t.height(xz.x, xz.z);
        supportSum += h;
        supportMax = std::max(supportMax, h);
        if (leg.restLocal.z > 0.5f) frontY += h * 0.5f;
        if (leg.restLocal.z < -0.5f) backY += h * 0.5f;
        if (leg.restLocal.x < 0.0f) leftY += h * (1.0f / 3.0f);
        else rightY += h * (1.0f / 3.0f);
    }
    const float supportAvg = supportSum / 6.0f;

    // Stand on the mean ground level, but never let the ground directly beneath
    // the chassis come through the hull - that is what stopped the bot burying
    // itself in a hillside when walking uphill.
    //
    // The clearance term deliberately uses only the sample under the body, not
    // the highest sample across the footprint. A max() over six rotating samples
    // is continuous in value but not in rate: which sample wins flips as the bot
    // turns, and the body height visibly surged on every switch.
    const float underBody = t.height(pos_.x, pos_.z);
    const float targetY = std::max(supportAvg + cfg_.standHeight,
                                   underBody + cfg_.minGroundClearance);

    bodyHeight_ = (dt > 0.0f) ? damp(bodyHeight_, targetY, 14.0f, dt) : targetY;
    (void)supportMax;

    // Pitch and roll follow the same continuous samples. Positive pitch = nose up.
    const float fbDist = std::max(std::fabs(legs_[0].restLocal.z - legs_[4].restLocal.z), 0.4f);
    const float lrDist = std::max(std::fabs(legs_[3].restLocal.x - legs_[2].restLocal.x), 0.4f);
    const float pitchTarget = std::atan2(frontY - backY, fbDist) * 0.85f;
    const float rollTarget = std::atan2(rightY - leftY, lrDist) * 0.85f;
    if (dt > 0.0f) {
        bodyPitch_ = damp(bodyPitch_, pitchTarget, 10.0f, dt);
        bodyRoll_ = damp(bodyRoll_, rollTarget, 10.0f, dt);
    } else {
        bodyPitch_ = pitchTarget;
        bodyRoll_ = rollTarget;
    }

    // A subtle gait bob so the walk does not look like it is on rails.
    const float speed = lengthXZ(vel_);
    bobPhase_ += dt * (4.0f + speed * 1.6f);
    if (bobPhase_ > TAU) bobPhase_ -= TAU;
    const float bob = std::sin(bobPhase_ * 2.0f) * 0.035f * clampf(speed / cfg_.maxSpeed, 0.0f, 1.0f);

    // Hard safety net, set below the damped target so it almost never engages
    // and cannot fight the damping when it does.
    pos_.y = std::max(bodyHeight_ + bob - recoil_ * 0.16f,
                      underBody + cfg_.minGroundClearance * 0.8f);

    bodyXform_ = Mat4::translation(pos_)
               * Mat4::rotationY(bodyYaw_)
               * Mat4::rotationX(-bodyPitch_ - recoil_ * 0.10f)
               * Mat4::rotationZ(-bodyRoll_);
}

void SpiderBot::solveIK() {
    const Vec3 bodyUp = normalize(transformDir(bodyXform_, Vec3(0.0f, 1.0f, 0.0f)));
    const float L1 = cfg_.femurLen;
    const float L2 = cfg_.tibiaLen;

    for (Leg& leg : legs_) {
        leg.hipWorld = transformPoint(bodyXform_, leg.hipLocal);

        // The coxa is a short outward segment; it points at the foot but stays
        // roughly level with the chassis, like a real hip yaw joint.
        Vec3 outward = leg.foot - leg.hipWorld;
        Vec3 outFlat = transformDir(bodyXform_, Vec3(leg.restLocal.x, 0.0f, leg.restLocal.z));
        const Vec3 planar = outward - bodyUp * dot(outward, bodyUp);
        outward = (lengthSq(planar) > 1e-6f) ? normalize(planar) : normalize(outFlat);
        leg.coxaEnd = leg.hipWorld + outward * cfg_.coxaLen;

        // Two-bone analytic IK from the coxa tip to the foot. Note that the
        // solved foot position is clamped to the limb's actual reach and is what
        // gets drawn - `leg.foot` is where the gait wants the foot to be, and if
        // the two ever disagree the leg must bend short, not stretch.
        Vec3 toFoot = leg.foot - leg.coxaEnd;
        float dist = length(toFoot);
        const float minReach = std::fabs(L1 - L2) + 0.02f;
        const float maxReach = L1 + L2 - 0.02f;
        const Vec3 dir = (dist > 1e-5f) ? toFoot * (1.0f / dist) : Vec3(0.0f, -1.0f, 0.0f);
        dist = clampf(dist, minReach, maxReach);
        leg.footSolved = leg.coxaEnd + dir * dist;

        // The knee bends away from the ground, giving the arachnid profile.
        Vec3 bendRef = bodyUp;
        Vec3 perp = bendRef - dir * dot(bendRef, dir);
        if (lengthSq(perp) < 1e-6f) {
            bendRef = normalize(cross(dir, Vec3(0.0f, 0.0f, 1.0f)));
            perp = bendRef - dir * dot(bendRef, dir);
        }
        perp = normalize(perp);

        const float a = (dist * dist + L1 * L1 - L2 * L2) / (2.0f * dist);
        const float hSq = L1 * L1 - a * a;
        const float hh = (hSq > 0.0f) ? std::sqrt(hSq) : 0.0f;
        leg.knee = leg.coxaEnd + dir * a + perp * hh;
    }
}

void SpiderBot::updateTurret(float dt, const Vec3& aimPoint) {
    // Convert the world-space aim point into chassis space, so the turret angles
    // stay relative to a body that is itself pitching and rolling over terrain.
    const Mat4 invBody = invertRigid(bodyXform_);
    const Vec3 local = transformPoint(invBody, aimPoint);
    const Vec3 dir = normalize(local - Vec3(0.0f, 0.55f, 0.0f));

    const float wantYaw = std::atan2(dir.x, dir.z);
    const float wantPitch = clampf(std::asin(clampf(dir.y, -1.0f, 1.0f)),
                                   cfg_.turretPitchMin, cfg_.turretPitchMax);

    const float yawStep = cfg_.turretYawRate * dt;
    turretYaw_ += clampf(angleDelta(turretYaw_, wantYaw), -yawStep, yawStep);
    const float pitchStep = cfg_.turretPitchRate * dt;
    turretPitch_ += clampf(wantPitch - turretPitch_, -pitchStep, pitchStep);

    turretXform_ = bodyXform_
                 * Mat4::translation(Vec3(0.0f, 0.34f, 0.0f))
                 * Mat4::rotationY(turretYaw_)
                 * Mat4::translation(Vec3(0.0f, 0.26f, 0.0f))
                 * Mat4::rotationX(turretPitch_);
}

Vec3 SpiderBot::muzzlePosition(int barrel) const {
    const float side = (barrel & 1) ? 0.24f : -0.24f;
    return transformPoint(turretXform_, Vec3(side, 0.06f, 1.62f - recoil_ * 0.25f));
}

Vec3 SpiderBot::aimDirection() const {
    return normalize(transformDir(turretXform_, Vec3(0.0f, 0.0f, 1.0f)));
}

void SpiderBot::applyRecoil(float amount) {
    recoil_ = std::min(recoil_ + amount, 1.0f);
}

void SpiderBot::submit(Rasterizer& raster) const {
    DrawItem item;

    // ------------------------------------------------------------- chassis --
    item.mesh = &chassisMesh_;
    item.model = bodyXform_ * Mat4::translation(Vec3(0.0f, -cfg_.bodyThickness * 0.5f, 0.0f))
                            * Mat4::scaling(Vec3(cfg_.bodyWidth, cfg_.bodyThickness, cfg_.bodyLength));
    raster.submit(item);

    item.mesh = &deckMesh_;
    item.model = bodyXform_ * Mat4::translation(Vec3(0.0f, cfg_.bodyThickness * 0.5f, 0.0f))
                            * Mat4::scaling(Vec3(cfg_.bodyWidth * 0.78f, cfg_.bodyThickness * 0.62f,
                                                 cfg_.bodyLength * 0.80f));
    raster.submit(item);

    // Amber hazard stripes across the deck: the only saturated colour on the
    // machine, and what makes it identifiable at a glance in full-colour mode.
    item.mesh = &accentMesh_;
    for (int i = -1; i <= 1; i += 2) {
        item.model = bodyXform_
                   * Mat4::translation(Vec3(i * cfg_.bodyWidth * 0.50f,
                                            cfg_.bodyThickness * 0.78f, 0.0f))
                   * Mat4::scaling(Vec3(cfg_.bodyWidth * 0.17f, 0.038f, cfg_.bodyLength * 0.58f));
        raster.submit(item);
    }

    // Forward sensor cluster.
    item.mesh = &eyeMesh_;
    item.model = bodyXform_ * Mat4::translation(Vec3(0.0f, 0.30f, cfg_.bodyLength * 0.86f))
                            * Mat4::scaling(Vec3(0.16f, 0.10f, 0.10f));
    item.emissive = 0.9f;
    raster.submit(item);
    item.emissive = 0.0f;

    item.mesh = &antennaMesh_;
    item.model = bodyXform_ * Mat4::translation(Vec3(-0.35f, 0.34f, -cfg_.bodyLength * 0.6f))
                            * Mat4::scaling(Vec3(0.045f, 1.05f, 0.045f));
    raster.submit(item);

    // ---------------------------------------------------------------- legs --
    for (const Leg& leg : legs_) {
        item.mesh = &shoulderMesh_;
        item.model = Mat4::translation(leg.hipWorld) * Mat4::scaling(Vec3(0.24f));
        raster.submit(item);

        item.mesh = &segmentMesh_;
        item.model = segmentTransform(leg.hipWorld, leg.coxaEnd, 0.17f);
        raster.submit(item);

        item.model = segmentTransform(leg.coxaEnd, leg.knee, 0.155f);
        raster.submit(item);

        item.mesh = &jointMesh_;
        item.model = Mat4::translation(leg.knee) * Mat4::scaling(Vec3(0.17f));
        raster.submit(item);

        item.mesh = &segmentMesh_;
        item.model = segmentTransform(leg.knee, leg.footSolved, 0.115f);
        raster.submit(item);

        item.mesh = &footMesh_;
        item.model = Mat4::translation(leg.footSolved - Vec3(0.0f, 0.05f, 0.0f))
                   * Mat4::scaling(Vec3(0.17f, 0.11f, 0.17f));
        raster.submit(item);
    }

    // -------------------------------------------------------------- turret --
    item.mesh = &turretBaseMesh_;
    item.model = bodyXform_ * Mat4::translation(Vec3(0.0f, cfg_.bodyThickness * 0.72f, 0.0f))
                            * Mat4::rotationY(turretYaw_)
                            * Mat4::scaling(Vec3(0.52f, 0.24f, 0.52f));
    raster.submit(item);

    item.mesh = &turretHeadMesh_;
    item.model = turretXform_ * Mat4::scaling(Vec3(0.40f, 0.26f, 0.52f));
    raster.submit(item);

    item.mesh = &accentMesh_;
    item.model = turretXform_ * Mat4::translation(Vec3(0.0f, 0.27f, 0.0f))
                              * Mat4::scaling(Vec3(0.41f, 0.045f, 0.16f));
    raster.submit(item);

    item.mesh = &barrelMesh_;
    for (int b = 0; b < 2; ++b) {
        const float side = (b == 1) ? 0.24f : -0.24f;
        const Vec3 from = transformPoint(turretXform_, Vec3(side, 0.06f, 0.30f - recoil_ * 0.25f));
        const Vec3 to = transformPoint(turretXform_, Vec3(side, 0.06f, 1.62f - recoil_ * 0.25f));
        item.model = segmentTransform(from, to, 0.085f);
        raster.submit(item);
    }

    // Muzzle glow while the recoil is still settling.
    if (recoil_ > 0.02f) {
        item.mesh = &eyeMesh_;
        item.emissive = 1.0f;
        for (int b = 0; b < 2; ++b) {
            item.model = Mat4::translation(muzzlePosition(b))
                       * Mat4::scaling(Vec3(0.10f + 0.22f * recoil_));
            raster.submit(item);
        }
        item.emissive = 0.0f;
    }

    // Turret status light.
    item.mesh = &eyeMesh_;
    item.model = turretXform_ * Mat4::translation(Vec3(0.0f, 0.24f, -0.20f))
                              * Mat4::scaling(Vec3(0.09f));
    item.emissive = 0.85f;
    raster.submit(item);
    item.emissive = 0.0f;
}

} // namespace sb
