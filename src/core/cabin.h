// cabin.h - the pilot's cabin, seen from the driver's seat.
//
// The first-person view is framed by real geometry: pillars, a roof beam, a
// dashboard, the breech of a casemate gun, the open cradle of a fire
// platform. The instruments are ON the dashboard as lit shapes - a hull bar,
// a heat bar, ability lamps, a lamp per gun - so the readouts are part of the
// machine rather than text floating over the world. What the cabin looks like
// is decided by the parts: the chassis family sets the shape, the chassis
// style its details, the armour how thick the frame is, and the sensor what
// sits on the console.
//
// Everything is built in CAMERA space (x right, y up, z forward, metres),
// so the cabin frames the view wherever the pilot is looking. A little body
// sway is added on top so it does not read as a picture frame.
#pragma once

#include <vector>
#include "math3d.h"
#include "parts.h"
#include "raster.h"

namespace sb {

class Mech;

// Where the instruments sit, in camera space. The HUD's text pass projects
// these through the camera to put labels beside the lit gauges.
struct CabinLayout {
    // Horizontal positions are FRACTIONS of the visible half-width at their
    // depth (-1 = left edge of the view, +1 = right), so the cabin frames
    // the view at any window aspect; resolve() turns them into metres for
    // the current camera. Heights are metres (the vertical field of view is
    // fixed).
    float pillarXf = 0.86f;     // A-pillar position as a fraction of the view's half-width
    float dashHalfWf = 1.30f;   // console half-width, same units
    float pillarX = 0.95f;      // resolved: half-distance between the A-pillars
    float pillarR = 0.05f;      // pillar half-thickness
    float roofY = 0.58f;        // roof beam height (open cradle: none)
    float dashYf = -0.62f;      // console top as a fraction of the visible half-height at its depth
    float dashY = -0.50f;       // resolved: dashboard top surface height (metres)
    float dashZ = 0.80f;        // dashboard distance
    float dashHalfW = 1.45f;
    bool openTop = false;       // artillery cradle: no roof
    bool mullion = false;       // heavy hull: a centre bar in the windscreen
    bool breech = false;        // casemate: the gun's breech in the cabin
    bool bubble = false;        // low profile: thin struts, wide glass
    bool roundPillars = false;  // pod: cylinders instead of bars
    float rake = 0.0f;          // pillar lean, radians (dart)
    float plate = 1.0f;         // armour tier multiplier on frame thickness
    int sensorTier = 0;         // console scope size

    // Instrument anchor fractions (u across the view at the console depth).
    float hullU = -0.92f, gunU = 0.12f, objU = 0.52f, threatU = -0.12f, scopeU = -0.42f;
    float barLenF = 0.42f;      // bar length as a fraction of the half-width
    float lampPitchF = 0.07f;

    // Resolves every metre-valued field below from the fractions above.
    void resolve(const Camera& cam);

    // Instrument anchors (camera space, resolved): left end of each bar /
    // first lamp.
    Vec3 hullBar{-1.15f, -0.44f, 0.72f};
    Vec3 heatBar{-1.15f, -0.50f, 0.66f};
    Vec3 abilityLamps{-1.15f, -0.56f, 0.60f};
    Vec3 gunLamps{0.45f, -0.44f, 0.72f};
    Vec3 jumpBar{0.45f, -0.50f, 0.66f};
    Vec3 objectiveText{0.45f, -0.56f, 0.60f};
    Vec3 threatLamp{0.00f, -0.42f, 0.74f};
    Vec3 scope{-0.40f, -0.46f, 0.70f};
    float scopeSize = 0.06f;
    float barLen = 0.55f;
    float lampPitch = 0.09f;
};

// The live numbers the gauges show.
struct CabinState {
    float hull = 1.0f;          // 0..1
    float heat = 0.0f;          // 0..1
    bool overheated = false;
    float jump = 0.0f;          // 0..1 charge
    float damageFlash = 0.0f;   // 0..1, recent hit
    float speedFrac = 0.0f;     // 0..1, for the sway
    float time = 0.0f;          // seconds, for the sway and blink
    int abilityCount = 0;
    bool abilityFitted[4] = {false, false, false, false};
    bool abilityReady[4] = {false, false, false, false};
    bool abilityEngaged[4] = {false, false, false, false};
    float abilityCool[4] = {0.0f, 0.0f, 0.0f, 0.0f};   // 0..1 remaining
    int gunCount = 0;
    bool gunReady[6] = {false, false, false, false, false, false};
    bool gunEnabled[6] = {true, true, true, true, true, true};
    bool gunEmpty[6] = {false, false, false, false, false, false};
    float gunHeat[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};   // spool or bloom, 0..1
    bool threat = false;        // a hostile has line of sight
    bool jammed = false;
    bool climbing = false;
};

CabinLayout cabinLayoutFor(const Loadout& loadout);
CabinState cabinStateFor(const Mech& player, float time, bool threat, bool jammed);

// Draws the cabin into the scene for this camera. `sway` is the camera-space
// offset the body's motion adds (bob, lean); the caller decides how much.
void submitCabin(Rasterizer& raster, const Camera& cam, const CabinLayout& lay,
                 const CabinState& st, const Vec3& sway);

} // namespace sb
