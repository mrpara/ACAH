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

#include <cmath>
#include <string>
#include <vector>
#include "math3d.h"
#include "parts.h"
#include "raster.h"

namespace sb {

class Mech;

// One instrument panel: a recessed face set into the cabin's panelling,
// with a bezel around it. EVERYTHING the pilot reads lives on one of these -
// the HUD pass projects the face and paints the readout inside it - so no
// text floats over the glass. There are five: two in the brow over the
// windscreen (controls, contract) and three in the console under it
// (machine, radar, drive and guns).
struct CabinPanel {
    Vec3 centre{0.0f, 0.0f, 1.0f};   // camera space
    float halfW = 0.20f, halfH = 0.10f;
    float tilt = 0.0f;               // radians; positive leans the top away

    // The face's own up vector in camera space (its right is always +x).
    Vec3 up() const { return Vec3(0.0f, std::cos(tilt), -std::sin(tilt)); }
    // A point ON the face, in face coordinates: u and v run -1 to +1 from
    // edge to edge. The geometry pass puts gauges here and the text pass
    // puts their labels at the same coordinates, which is the only reason
    // an etched label and the lit bar under it stay together while the
    // cabin sways.
    Vec3 at(float u, float v) const {
        return centre + Vec3(u * halfW, 0.0f, 0.0f) + up() * (v * halfH);
    }
    // Corners, clockwise from the top left.
    void corners(Vec3* out) const {
        const Vec3 u = up();
        out[0] = centre - Vec3(halfW, 0.0f, 0.0f) + u * halfH;
        out[1] = centre + Vec3(halfW, 0.0f, 0.0f) + u * halfH;
        out[2] = centre + Vec3(halfW, 0.0f, 0.0f) - u * halfH;
        out[3] = centre - Vec3(halfW, 0.0f, 0.0f) - u * halfH;
    }
};

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
    float roofYf = 0.72f;       // brow underside as a fraction of the half-height there
    float roofY = 0.58f;        // resolved: brow underside at the pillar plane (metres)
    float dashYf = -0.34f;      // console top as a fraction of the visible half-height at its depth
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

    // ---- panel placement, all as SCREEN fractions at the panel's depth ----
    float browZ = 0.92f;        // the brow screens sit here
    float browGap = 0.035f;     // bezel between the two brow screens
    // The control plate needs four columns of key-and-job, the contract
    // two lines of orders, so the brow does not divide down the middle.
    float browSplit = 0.20f;    // where the brow divides, across the view
    float panelZ = 0.62f;       // the console screens sit here
    // The console's top edge, as a screen fraction at panelZ. This is where
    // the windscreen ends, so it is the number that decides how much of the
    // world the pilot can see: the console is a near-vertical instrument
    // face rather than a deep shelf precisely so the glass keeps its share.
    float panelTop = -0.53f;
    float panelBot = -0.96f;    // and the screens stop just inside the view
    float cutL = -0.30f;        // console divisions, across the view
    float cutR = 0.17f;
    float panelGap = 0.030f;

    // Resolves every metre-valued field below from the fractions above.
    void resolve(const Camera& cam);

    // The five instrument faces, resolved.
    CabinPanel browL;   // control bindings
    CabinPanel browR;   // the contract, and what is shooting at you
    CabinPanel dashL;   // structure, heat, fitted systems
    CabinPanel dashC;   // the radar screen
    CabinPanel dashR;   // drive state and the guns
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

// ---------------------------------------------------------------- legends
// The cabin's lettering is not text. Every legend and every digit in here
// is a little grid of emissive squares - an LED matrix module bolted to an
// instrument face - so it is projected, tilted, occluded and swayed with
// the panel it is screwed to. Character-grid text could not do that: it
// snaps to whole HUD cells while the cabin moves continuously, which is
// exactly what made the old readouts look like they were floating in front
// of the machine instead of being part of it.
enum class CabinFace { BrowL, BrowR, DashL, DashC, DashR };

struct CabinLabel {
    CabinFace face = CabinFace::DashL;
    float u = -1.0f, v = 0.0f;   // face coordinates of the anchor
    int align = -1;              // -1 anchor is the left edge, 0 centre, +1 right
    float scale = 1.0f;          // dot pitch, in scene character cells
    Vec3 colour{0.45f, 1.00f, 0.60f};
    float glow = 1.0f;           // brightness multiplier; < 1 reads as unlit
    std::string text;
};

// Everything the pilot reads, as lamps. Built by the game each frame and
// handed to submitCabin, which turns it into one emissive mesh.
struct CabinReadout {
    std::vector<CabinLabel> labels;
    void add(CabinFace f, float u, float v, int align, const std::string& t,
             const Vec3& colour, float scale = 1.0f, float glow = 1.0f) {
        if (t.empty()) return;
        CabinLabel l;
        l.face = f; l.u = u; l.v = v; l.align = align;
        l.text = t; l.colour = colour; l.scale = scale; l.glow = glow;
        labels.push_back(l);
    }
};

// How a console panel divides into instrument rows, and where row `i` of
// `n` sits on the face. The geometry pass and the text pass both ask these,
// so a lit gauge and the label etched beside it cannot disagree about which
// row they are on.
inline int cabinRowCount(int needed) { return (needed < 4) ? 4 : needed; }
inline float cabinRowV(int i, int n) {
    return (n <= 0) ? 0.0f
                    : 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
}

CabinLayout cabinLayoutFor(const Loadout& loadout);
CabinState cabinStateFor(const Mech& player, float time, bool threat, bool jammed);

// Draws the cabin into the scene for this camera. `sway` is the camera-space
// offset the body's motion adds (bob, lean); the caller decides how much.
void submitCabin(Rasterizer& raster, const Camera& cam, const CabinLayout& lay,
                 const CabinState& st, const Vec3& sway);

} // namespace sb
