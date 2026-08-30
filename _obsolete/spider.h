// spider.h - the player's walker: a parts-based model driven entirely by
// procedural animation. Nothing is keyframed; the legs are placed by an IK
// solver and a tripod gait planner that reacts to the terrain underneath.
#pragma once

#include <array>
#include <vector>
#include "math3d.h"
#include "mesh.h"
#include "raster.h"
#include "world.h"

namespace sb {

struct SpiderConfig {
    // Chassis
    float bodyWidth = 1.15f;
    float bodyLength = 1.65f;
    float bodyThickness = 0.42f;
    float standHeight = 1.85f;      // body origin above the average foot

    // Limb segment lengths
    float coxaLen = 0.55f;
    float femurLen = 1.55f;
    float tibiaLen = 2.05f;

    // Locomotion
    float maxSpeed = 7.6f;
    float minGroundClearance = 0.90f;   // hull never gets closer than this to the ground
    float acceleration = 30.0f;
    float braking = 26.0f;
    float bodyTurnRate = 4.6f;      // radians/sec the chassis yaws toward travel

    // Gait
    float stepDuration = 0.20f;     // seconds a foot spends in the air
    float stepTrigger = 0.80f;      // how far a foot may drift before stepping
    float stepHeight = 0.62f;
    // Multiples of the swing time that the landing point leads the rest
    // position by. A leg only steps once per two-swing cycle, so this has to be
    // well above 1 or the body outruns its own feet.
    float stepLead = 1.05f;
    // Fraction of total limb reach at which a planted foot must step regardless
    // of gait phase.
    float reachSafety = 0.74f;

    // Turret
    float turretYawRate = 7.0f;
    float turretPitchRate = 5.0f;
    float turretPitchMin = deg2rad(-28.0f);
    float turretPitchMax = deg2rad(52.0f);
};

struct Leg {
    Vec3 hipLocal;        // attachment on the chassis, body space
    Vec3 restLocal;       // ideal planted position, body space
    int group = 0;        // tripod group: 0 or 1

    Vec3 foot{0.0f, 0.0f, 0.0f};      // current world position
    Vec3 stepFrom{0.0f, 0.0f, 0.0f};
    Vec3 stepTo{0.0f, 0.0f, 0.0f};
    float stepT = 1.0f;
    bool stepping = false;

    // Solved joint chain, world space (filled every frame by solveIK).
    Vec3 hipWorld{0.0f, 0.0f, 0.0f};
    Vec3 coxaEnd{0.0f, 0.0f, 0.0f};
    Vec3 knee{0.0f, 0.0f, 0.0f};
    Vec3 footSolved{0.0f, 0.0f, 0.0f};   // reach-clamped; this is what is drawn
};

class SpiderBot {
public:
    void init(const World& world, const Vec3& spawnXZ, uint32_t seed = 1u);

    // `moveDir` is a world-space XZ direction (need not be normalised; its
    // length scales the throttle). `aimPoint` is where the turret should point.
    void update(float dt, const World& world, const Vec3& moveDir, const Vec3& aimPoint);

    void submit(Rasterizer& raster) const;

    // ------------------------------------------------------------- accessors
    const Vec3& position() const { return pos_; }
    const Vec3& velocity() const { return vel_; }
    float speed() const { return lengthXZ(vel_); }
    float bodyYaw() const { return bodyYaw_; }
    const SpiderConfig& config() const { return cfg_; }
    const std::array<Leg, 6>& legs() const { return legs_; }
    int legsInAir() const;

    Mat4 bodyMatrix() const { return bodyXform_; }
    Mat4 turretMatrix() const { return turretXform_; }

    // Barrel 0 is left, 1 is right. Alternating fire reads better than volleys.
    Vec3 muzzlePosition(int barrel) const;
    Vec3 aimDirection() const;

    void applyRecoil(float amount);

    float maxLegReach() const;

private:
    Vec3 localToWorldXZ(const Vec3& local) const;
    void buildMeshes();
    void updateGait(float dt, const World& world);
    void updateBodyPose(float dt, const World& world);
    void solveIK();
    void updateTurret(float dt, const Vec3& aimPoint);

    SpiderConfig cfg_;
    std::array<Leg, 6> legs_{};

    Vec3 pos_{0.0f, 0.0f, 0.0f};
    Vec3 vel_{0.0f, 0.0f, 0.0f};
    float bodyYaw_ = 0.0f;
    float bodyPitch_ = 0.0f;
    float bodyRoll_ = 0.0f;
    float bodyHeight_ = 0.0f;       // smoothed body origin height
    float bobPhase_ = 0.0f;
    float recoil_ = 0.0f;

    float turretYaw_ = 0.0f;        // relative to the chassis
    float turretPitch_ = 0.0f;

    Mat4 bodyXform_ = Mat4::identity();
    Mat4 turretXform_ = Mat4::identity();

    // Part meshes. Every visible piece of the bot is one of these, placed by
    // its own transform - that is what makes the model "parts based".
    Mesh chassisMesh_;
    Mesh deckMesh_;
    Mesh accentMesh_;
    Mesh shoulderMesh_;
    Mesh segmentMesh_;      // unit cylinder along +Y, stretched per bone
    Mesh jointMesh_;        // unit sphere
    Mesh footMesh_;
    Mesh turretBaseMesh_;
    Mesh turretHeadMesh_;
    Mesh barrelMesh_;
    Mesh eyeMesh_;
    Mesh antennaMesh_;
};

} // namespace sb
