// cabin.cpp - see cabin.h.
#include "cabin.h"

#include <cmath>
#include "mech.h"

namespace sb {

namespace {

// Materials. Everything in the cabin is SELF-LIT at a fixed value: the
// scene's sun comes from one side, and lit-vs-shadowed cabin faces made one
// pillar read bright and the other vanish. A cabin interior is in shadow
// anyway; what matters is that it is uniformly darker than the world so
// the world reads THROUGH it, with the console a shade lighter and the
// gauges bright.
// (Self-lit surfaces render at 2.2x their tint, and the ASCII levels
// stretch maps roughly 0.10-0.32 of gamma-corrected luminance across the
// whole ramp, so these numbers are small: 0.04 is a faint glyph, 0.03 is
// black, 0.07 is a clearly drawn edge.)
const Vec3 kFrame(0.040f, 0.043f, 0.046f);
const Vec3 kFrameLit(0.056f, 0.059f, 0.062f);
const Vec3 kConsole(0.046f, 0.049f, 0.049f);
const Vec3 kBezel(0.026f, 0.029f, 0.029f);
const Vec3 kGlassRim(0.072f, 0.078f, 0.080f);
// An instrument screen: darker than the panelling around it, so a face with
// nothing on it still reads as a screen, and the readout painted over it by
// the HUD pass has something to sit against.
const Vec3 kScreen(0.021f, 0.026f, 0.024f);

struct Painter {
    Rasterizer& raster;
    const MechMeshLibrary& L;
    Mat4 frame;   // camera space -> world

    // Frame pieces carry a little self-light: a shadowed pillar must still
    // read as a pillar, not as a gap in the glass. Gauges pass their own.
    void box(const Vec3& centre, const Vec3& half, const Vec3& colour,
             float emissive = 1.0f, const Mat4& local = Mat4::identity()) {
        DrawItem it;
        it.mesh = &L.box;
        it.model = frame * Mat4::translation(centre) * local * Mat4::scaling(half);
        it.tint = colour;
        it.emissive = emissive;
        it.rim = 0.0f;
        it.twoSided = true;
        raster.submit(it);
    }
    void chamfer(const Vec3& centre, const Vec3& half, const Vec3& colour,
                 const Mat4& local = Mat4::identity()) {
        DrawItem it;
        it.mesh = &L.chamfer;
        it.model = frame * Mat4::translation(centre) * local * Mat4::scaling(half);
        it.tint = colour;
        it.emissive = 1.0f;
        it.rim = 0.0f;
        it.twoSided = true;
        raster.submit(it);
    }
    // A cylinder along local +Y from `base`, radius r, length len.
    void cyl(const Vec3& base, const Vec3& axis, float r, float len, const Vec3& colour) {
        const Vec3 y = normalize(axis);
        Vec3 x = cross(y, Vec3(0.0f, 0.0f, 1.0f));
        if (lengthSq(x) < 1e-4f) x = cross(y, Vec3(1.0f, 0.0f, 0.0f));
        x = normalize(x);
        const Vec3 z = cross(x, y);
        DrawItem it;
        it.mesh = &L.cyl8;
        it.model = frame * Mat4::translation(base) * Mat4::basis(x, y, z) *
                   Mat4::scaling(Vec3(r, len, r));
        it.tint = colour;
        it.emissive = 1.0f;
        it.rim = 0.0f;
        it.twoSided = true;
        raster.submit(it);
    }
    // A lit bar of length `frac * len` from `at` along +x, with a dark track
    // behind it so an empty gauge still shows where the gauge is.
    void bar(const Vec3& at, float len, float frac, const Vec3& lit, float thick = 0.018f) {
        box(at + Vec3(len * 0.5f, -0.004f, 0.0f), Vec3(len * 0.5f + 0.012f, thick * 0.7f, thick * 1.6f),
            kBezel, 0.15f);
        const float f = clampf(frac, 0.0f, 1.0f);
        if (f > 0.01f)
            box(at + Vec3(len * f * 0.5f, 0.006f, 0.0f), Vec3(len * f * 0.5f, thick, thick),
                lit, 1.0f);
    }
    void lamp(const Vec3& at, const Vec3& lit, float emissive, float size = 0.026f) {
        box(at, Vec3(size * 1.3f, size * 0.5f, size * 1.3f), kBezel, 0.15f);
        box(at + Vec3(0.0f, 0.008f, 0.0f), Vec3(size, size * 0.6f, size), lit, emissive);
    }
    // An instrument face: a dark screen sunk into the panelling behind a lit
    // bezel, with a bracket above and below so it reads as a fitted unit
    // rather than a hole. The HUD pass paints the readout over the screen.
    void screen(const CabinPanel& p, const Vec3& rim, float thick) {
        const Mat4 tilt = Mat4::rotationX(-p.tilt);
        const Vec3 u = p.up();
        // Bezel: a frame of four bars around the face, so the screen's own
        // dark rectangle is never outlined by a bigger dark rectangle.
        const float bw = thick, bh = thick * 0.8f;
        box(p.centre + u * (p.halfH + bh) + Vec3(0.0f, 0.0f, 0.004f),
            Vec3(p.halfW + bw * 2.0f, bh, 0.006f), rim, 1.0f, tilt);
        box(p.centre - u * (p.halfH + bh) + Vec3(0.0f, 0.0f, 0.004f),
            Vec3(p.halfW + bw * 2.0f, bh, 0.006f), rim, 1.0f, tilt);
        for (int s = -1; s <= 1; s += 2)
            box(p.centre + Vec3(static_cast<float>(s) * (p.halfW + bw), 0.0f, 0.004f),
                Vec3(bw, p.halfH + bh * 2.0f, 0.006f), rim * 0.8f, 1.0f, tilt);
        // The screen itself: darker than the panelling it sits in.
        box(p.centre + Vec3(0.0f, 0.0f, 0.008f), Vec3(p.halfW, p.halfH, 0.004f),
            kScreen, 1.0f, tilt);
    }

    // ---- instruments mounted ON a panel ----------------------------------
    // All three take face coordinates (u, v in -1..1), so the text pass can
    // put a label at the same coordinates and know it will land on it.

    // A raised plate: what a label is etched into. Slightly proud of the
    // screen and a shade lighter, so the lettering reads as engraved metal
    // rather than as something printed on the glass.
    void plate(const CabinPanel& p, float u0, float u1, float v, float vh,
               const Vec3& colour) {
        const Mat4 tilt = Mat4::rotationX(-p.tilt);
        const Vec3 c = p.at((u0 + u1) * 0.5f, v);
        box(c, Vec3(std::fabs(u1 - u0) * 0.5f * p.halfW, vh * p.halfH, 0.004f),
            colour, 1.0f, tilt);
    }

    // A lit gauge lying on the face: a sunk track with a lit bar filling it.
    // This is the instrument itself - geometry, not characters - so it
    // brightens and colours with what it is measuring.
    void gauge(const CabinPanel& p, float u0, float u1, float v, float frac,
               const Vec3& lit, float vh) {
        const Mat4 tilt = Mat4::rotationX(-p.tilt);
        const float halfU = std::fabs(u1 - u0) * 0.5f;
        box(p.at((u0 + u1) * 0.5f, v), Vec3(halfU * p.halfW + 0.004f, vh * p.halfH, 0.003f),
            kBezel * 1.5f, 0.9f, tilt);
        const float f = clampf(frac, 0.0f, 1.0f);
        if (f > 0.012f) {
            const float uEnd = u0 + (u1 - u0) * f;
            box(p.at((u0 + uEnd) * 0.5f, v) - Vec3(0.0f, 0.0f, 0.004f),
                Vec3(std::fabs(uEnd - u0) * 0.5f * p.halfW, vh * p.halfH * 0.74f, 0.003f),
                lit, 1.0f, tilt);
        }
    }

    // An annunciator on the face: a lit pip in a dark surround.
    void pip(const CabinPanel& p, float u, float v, const Vec3& lit, float emissive,
             float size) {
        const Mat4 tilt = Mat4::rotationX(-p.tilt);
        const Vec3 c = p.at(u, v);
        box(c, Vec3(size * 1.5f * p.halfW, size * 2.2f * p.halfH, 0.003f), kBezel, 0.5f, tilt);
        box(c - Vec3(0.0f, 0.0f, 0.004f),
            Vec3(size * p.halfW, size * 1.5f * p.halfH, 0.003f), lit, emissive, tilt);
    }
};

Vec3 gaugeColour(float frac, const Vec3& good, const Vec3& warn, const Vec3& bad) {
    if (frac < 0.25f) return bad;
    if (frac < 0.55f) return warn;
    return good;
}

} // namespace

CabinLayout cabinLayoutFor(const Loadout& loadout) {
    CabinLayout lay;
    const PartDef* chassis = loadout.part(Slot::Chassis);
    const PartDef* armour = loadout.part(Slot::Armor);
    const PartDef* sensor = loadout.part(Slot::Sensor);
    const HullFamily family = chassis ? chassis->family : HullFamily::Turreted;
    const int style = chassis ? chassis->style : 1;

    switch (family) {
        case HullFamily::Turreted:
            // An enclosed crew compartment with a vision slot: pillars fairly
            // close, a deep brow of panelling, a full console.
            lay.pillarXf = 0.96f;
            lay.roofYf = 0.780f;
            lay.dashYf = -0.34f;
            break;
        case HullFamily::Casemate:
            // The gun lives in here with you. Wide, low glass over the
            // mantlet, the breech filling the right of the cabin.
            lay.pillarXf = 1.00f;
            lay.roofYf = 0.770f;
            lay.dashYf = -0.32f;
            lay.breech = true;
            break;
        case HullFamily::LowProfile:
            // A flat hull has no headroom and needs none: a wide bubble on
            // thin struts, the console a shelf at your knees.
            lay.pillarXf = 1.05f;
            lay.pillarR = 0.032f;
            lay.roofYf = 0.790f;
            lay.dashYf = -0.38f;
            lay.dashZ = 0.90f;
            lay.bubble = true;
            break;
        case HullFamily::Artillery:
            // An open cradle: rails, no roof, hydraulic columns at the far
            // sides, and the two brow screens hung off a roll frame.
            lay.pillarXf = 1.01f;
            lay.pillarR = 0.07f;
            lay.roofYf = 0.785f;
            lay.dashYf = -0.36f;
            lay.openTop = true;
            break;
    }
    switch (style) {
        case 0: lay.roundPillars = true; break;             // pod: tubular frame
        case 2: lay.rake = 0.28f; break;                    // dart: raked glass
        case 3: lay.pillarXf *= 0.88f; lay.roofYf += 0.01f; break;  // torso: tall, narrow
        case 4: lay.mullion = true; lay.pillarR *= 1.4f; break;     // long hull: heavy
        default: break;
    }
    if (armour) lay.plate = 0.85f + 0.22f * static_cast<float>(armour->tier);
    lay.sensorTier = sensor ? sensor->tier : 0;
    lay.pillarR *= lay.plate;
    if (lay.breech) {
        // The breech takes the right of the cabin, so the console's three
        // screens all shift left and the gun panel loses a little width.
        lay.cutL = -0.44f;
        lay.cutR = -0.02f;
    }
    return lay;
}

void CabinLayout::resolve(const Camera& cam) {
    const float tanHalf = std::tan(cam.fovY * 0.5f);
    auto halfW = [&](float z) { return tanHalf * cam.aspect * z; };
    auto halfH = [&](float z) { return tanHalf * z; };
    pillarX = pillarXf * halfW(1.0f);
    dashHalfW = dashHalfWf * halfW(dashZ);
    // The brow's underside and the console's top surface are quoted as
    // fractions of the half-height AT THEIR OWN DEPTH, so the cockpit frames
    // the same share of the view at any window aspect.
    roofY = roofYf * halfH(1.0f);
    dashY = panelTop * halfH(panelZ);
    (void)dashYf;

    // A screen spanning a range of screen fractions at one depth. `u` runs
    // across the view, `v` up it; both are fractions of the half-extent at
    // that depth, so the panel lands where the pilot sees it regardless of
    // how wide the window is. The bezel is taken out of the face itself.
    auto face = [&](float z, float u0, float u1, float v0, float v1, float tilt) {
        const float hw = halfW(z), hh = halfH(z);
        CabinPanel p;
        p.centre = Vec3((u0 + u1) * 0.5f * hw, (v0 + v1) * 0.5f * hh, z);
        // The bezel is taken out of the face proportionally: a fixed inset
        // is a rounding error on a big panel and a whole row of text on a
        // shallow one, and the brow is shallow on every light hull.
        const float halfU = (u1 - u0) * 0.5f * hw, halfV = (v1 - v0) * 0.5f * hh;
        p.halfW = std::max(0.01f, halfU - std::min(0.012f, halfU * 0.035f));
        p.halfH = std::max(0.01f, halfV - std::min(0.010f, halfV * 0.035f));
        p.tilt = tilt;
        return p;
    };

    // ---- the brow ---------------------------------------------------------
    // Two screens under the roof panelling, tipped down toward the pilot.
    // The band runs off the top of the view so the panelling has no edge.
    // The FACE stops at the top of the view even though the panelling behind
    // it runs off: every line of lamps the brow carries has to be somewhere
    // the pilot can see, and a face that ran to 1.34 put two thirds of its
    // own height above the glass.
    {
        const float v0 = roofYf + 0.008f;
        const float v1 = 1.02f;
        browL = face(browZ, -0.985f, browSplit - browGap, v0, v1, -0.22f);
        browR = face(browZ, browSplit + browGap, 0.985f, v0, v1, -0.22f);
    }

    // ---- the console ------------------------------------------------------
    // Three screens in a near-vertical instrument face. The face starts at
    // panelTop, which IS the bottom of the windscreen: a deep shelf would
    // rise up the view as it receded and cost the pilot the ground they are
    // walking onto, so the console is a wall of instruments instead.
    {
        const float v0 = panelBot, v1 = panelTop;
        dashL = face(panelZ, -0.985f, cutL - panelGap, v0, v1, 0.12f);
        // The radar sits a little lower than its neighbours: the strip of
        // console above it carries the caution lamps, which have to be
        // somewhere the radar's own picture will not paint over them.
        dashC = face(panelZ, cutL + panelGap, cutR - panelGap, v0, v1 - 0.075f, 0.12f);
        dashR = face(panelZ, cutR + panelGap, breech ? 0.72f : 0.985f, v0, v1, 0.12f);
    }
}

CabinState cabinStateFor(const Mech& p, float time, bool threat, bool jammed) {
    CabinState st;
    st.hull = p.healthFraction();
    st.heat = clampf(p.heat() / std::max(p.stats().heatCapacity, 0.01f), 0.0f, 1.0f);
    st.overheated = p.overheated();
    st.jump = clampf(p.jumpCharge(), 0.0f, 1.0f);
    st.damageFlash = clampf(1.0f - p.lastHitAge() / 0.6f, 0.0f, 1.0f);
    st.speedFrac = clampf(p.speed() / std::max(p.stats().maxSpeed, 0.1f), 0.0f, 1.0f);
    st.time = time;
    for (int i = 0; i < 4; ++i) {
        st.abilityFitted[i] = p.slotAbility(i) != Ability::None;
        if (st.abilityFitted[i]) ++st.abilityCount;
        st.abilityReady[i] = p.abilityReady(i);
        st.abilityEngaged[i] = p.abilityEngaged(i);
        st.abilityCool[i] = p.abilityCooldownFraction(i);
    }
    // Only mounts that carry something get a lamp. An empty hardpoint is a
    // workshop problem, not a cockpit one, and a row of dashes in the middle
    // of the gun panel is exactly the clutter the console is meant to avoid.
    const std::vector<MountedWeapon>& ws = p.weapons();
    int g = 0;
    for (size_t i = 0; i < ws.size() && g < 6; ++i) {
        const MountedWeapon& w = ws[i];
        if (!w.part) continue;
        const WeaponDef& def = w.part->weapon;
        st.gunEnabled[g] = w.enabled;
        st.gunEmpty[g] = def.ammo == AmmoKind::Limited && w.rounds == 0 && w.reserve == 0;
        st.gunReady[g] = w.enabled && w.cooldown <= 0.01f &&
                         (def.ammo != AmmoKind::Limited || w.rounds > 0);
        st.gunHeat[g] = (def.spinUp > 0.0f) ? w.spool : clampf(w.bloom / 2.2f, 0.0f, 1.0f);
        ++g;
    }
    st.gunCount = g;
    st.threat = threat;
    st.jammed = jammed;
    st.climbing = p.climbFraction() > 0.05f;
    return st;
}

void submitCabin(Rasterizer& raster, const Camera& cam, const CabinLayout& layIn,
                 const CabinState& st, const Vec3& sway) {
    CabinLayout lay = layIn;
    lay.resolve(cam);
    Painter P{raster, MechMeshLibrary::instance(),
              Mat4::translation(cam.pos + cam.right * sway.x + cam.up * sway.y +
                                cam.forward * sway.z) *
                  Mat4::basis(cam.right, cam.up, cam.forward)};

    const float pr = lay.pillarR;
    const float px = lay.pillarX;
    const float zP = 1.0f;                   // pillar plane
    const float top = lay.openTop ? lay.roofY + 0.25f : lay.roofY;
    const float bottom = lay.dashY - 0.35f;

    // ---- pillars -----------------------------------------------------------
    for (int side = -1; side <= 1; side += 2) {
        const float x = px * static_cast<float>(side);
        const Vec3 base(x, bottom, zP + 0.10f);
        const Vec3 tip(x + std::sin(lay.rake) * 0.0f, top, zP + 0.10f - std::sin(lay.rake) * 0.35f);
        const Vec3 axis = tip - base;
        const float len = length(axis);
        if (lay.roundPillars) {
            P.cyl(base, axis, pr * 1.25f, len, kFrame);
        } else {
            const Vec3 mid = (base + tip) * 0.5f;
            const Vec3 y = normalize(axis);
            const Vec3 xAxis(1.0f, 0.0f, 0.0f);
            const Vec3 z = normalize(cross(xAxis, y));
            P.box(mid, Vec3(pr, len * 0.5f, pr * 1.6f), kFrame, 0.0f, Mat4::basis(xAxis, y, z));
            // A lit inner edge so the pillar reads as a bar, not a hole.
            P.box(mid + Vec3(-static_cast<float>(side) * pr * 0.9f, 0.0f, -pr * 0.6f),
                  Vec3(pr * 0.12f, len * 0.5f, pr * 0.8f), kGlassRim, 0.0f,
                  Mat4::basis(xAxis, y, z));
        }
        // Side walls / door frames, back beside the seat. They sit BEHIND
        // the pillar plane: a wall that reaches forward of it sweeps in
        // across the glass as it approaches the camera and quietly costs the
        // pilot the outer sixth of the view on each side.
        if (!lay.bubble) {
            P.box(Vec3(x + static_cast<float>(side) * (pr + 0.06f), (top + bottom) * 0.5f, zP - 0.30f),
                  Vec3(0.05f, (top - bottom) * 0.5f, 0.42f), kFrame * 0.9f);
        } else {
            // A bubble has a low sill and glass above it.
            P.box(Vec3(x + static_cast<float>(side) * (pr + 0.06f), bottom + 0.08f, zP - 0.30f),
                  Vec3(0.05f, 0.10f, 0.42f), kFrame * 0.9f);
        }
    }

    // Screen fractions to camera-space metres, at a chosen depth: the whole
    // cabin is laid out this way so it frames the same share of the view at
    // any window aspect.
    const float tanHalf = std::tan(cam.fovY * 0.5f);
    auto hwAt = [&](float z) { return tanHalf * cam.aspect * z; };
    auto hhAt = [&](float z) { return tanHalf * z; };
    // A slab of panelling spanning a rectangle of the view at depth z.
    auto slab = [&](float z, float u0, float u1, float v0, float v1,
                    const Vec3& colour, float thick = 0.012f) {
        const float hw = hwAt(z), hh = hhAt(z);
        P.box(Vec3((u0 + u1) * 0.5f * hw, (v0 + v1) * 0.5f * hh, z),
              Vec3(std::fabs(u1 - u0) * 0.5f * hw, std::fabs(v1 - v0) * 0.5f * hh, thick),
              colour, 1.0f);
    };

    // ---- the brow ----------------------------------------------------------
    // A band of panelling over the glass carrying the two overhead screens.
    // It is the deepest part of the cabin frame because it is where the
    // pilot's eye goes between shots: the contract on the right, the
    // controls on the left, both inside the machine rather than over it.
    const float bz = lay.browZ;
    if (!lay.openTop) {
        const float beamH = std::min(pr * 0.8f, 0.045f);
        // The lit lip along the underside, where the panelling meets glass.
        // It sits back at the pillar plane rather than under the brow: it
        // used to reach forward to z = 0.90, which is in FRONT of the brow
        // screens, and quietly ate the bottom half of everything lit on
        // them.
        P.box(Vec3(0.0f, top, zP + 0.16f), Vec3(px + pr, beamH, 0.14f), kFrame);
        slab(bz + 0.04f, -1.30f, 1.30f, lay.roofYf + 0.010f, 1.60f, kFrame * 0.92f, 0.020f);
        slab(bz + 0.02f, -1.30f, 1.30f, lay.roofYf + 0.010f, lay.roofYf + 0.030f,
             kGlassRim * 0.55f, 0.006f);
        if (lay.mullion)
            P.box(Vec3(0.0f, (top + lay.dashY) * 0.5f, zP + 0.06f),
                  Vec3(pr * 0.9f, (top - lay.dashY) * 0.5f, pr * 1.2f), kFrame);
        if (lay.bubble) {
            // Two thin diagonal struts across the glass.
            for (int side = -1; side <= 1; side += 2) {
                const Vec3 a(static_cast<float>(side) * px, top, zP + 0.05f);
                const Vec3 b(static_cast<float>(side) * px * 0.45f, top + 0.16f, zP - 0.20f);
                P.cyl(a, b - a, pr * 0.7f, length(b - a), kFrame);
            }
        }
    } else {
        // Open cradle: no roof to sink screens into, so they hang off a roll
        // frame - two mounts and a cross-tube above the pilot's head.
        slab(bz + 0.05f, -1.30f, 1.30f, lay.roofYf + 0.055f, lay.roofYf + 0.105f,
             kFrameLit * 0.8f, 0.030f);
        for (int side = -1; side <= 1; side += 2)
            P.cyl(Vec3(static_cast<float>(side) * (px + 0.32f), bottom, zP - 0.30f),
                  Vec3(0.0f, 1.0f, 0.0f), 0.09f, top - bottom + 0.2f, kFrameLit * 0.8f);
        for (int side = -1; side <= 1; side += 2)
            P.box(Vec3(static_cast<float>(side) * hwAt(bz) * 0.55f,
                       hhAt(bz) * (lay.roofYf + 0.30f), bz + 0.06f),
                  Vec3(0.020f, hhAt(bz) * 0.26f, 0.020f), kFrame);
    }
    // The two brow screens, and the panelling immediately around them.
    slab(bz + 0.03f, -1.30f, 1.30f, lay.roofYf + 0.025f, 1.60f, kFrame, 0.014f);
    P.screen(lay.browL, kFrameLit, 0.010f * lay.plate);
    P.screen(lay.browR, kFrameLit, 0.010f * lay.plate);

    // ---- the console -------------------------------------------------------
    {
        // A wall of instruments standing between the pilot's knees and the
        // glass. It runs off the bottom of the view, so nothing of the world
        // shows underneath it, and a lit lip along the top is the line where
        // the windscreen ends.
        slab(lay.panelZ + 0.030f, -1.40f, 1.40f, lay.panelBot - 1.2f, lay.panelTop,
             kConsole, 0.016f);
        slab(lay.panelZ + 0.016f, -1.40f, 1.40f, lay.panelTop - 0.020f, lay.panelTop,
             kGlassRim * 0.5f, 0.006f);
        // A shallow coaming over the top of the panel: the console has depth,
        // but it is depth going AWAY from the pilot rather than a shelf
        // rising up the view.
        P.chamfer(Vec3(0.0f, lay.dashY + 0.012f, lay.panelZ + 0.16f),
                  Vec3(lay.dashHalfW, 0.014f, 0.16f), kFrame, Mat4::rotationX(-0.06f));
        // Divider ribs between the screens, so the console reads as three
        // fitted units rather than one painted board.
        for (float u : {lay.cutL, lay.cutR})
            slab(lay.panelZ + 0.014f, u - 0.007f, u + 0.007f, lay.panelBot - 0.3f,
                 lay.panelTop - 0.02f, kFrame * 0.85f, 0.008f);
    }
    // Damage flashes the machine panel; heat warms the gun panel; the radar's
    // bezel is what goes red under a jammer.
    {
        const Vec3 flash = lerp(kFrameLit, Vec3(1.0f, 0.45f, 0.35f), st.damageFlash);
        const Vec3 hot = st.overheated
            ? Vec3(1.0f, 0.45f, 0.2f) * (0.4f + 0.5f * std::fabs(std::sin(st.time * 16.0f)))
            : kFrameLit;
        const Vec3 jam = st.jammed
            ? Vec3(0.9f, 0.25f, 0.2f) * (0.3f + 0.5f * std::fabs(std::sin(st.time * 13.0f)))
            : kFrameLit;
        P.screen(lay.dashL, flash, 0.010f * lay.plate);
        P.screen(lay.dashC, jam, 0.012f * lay.plate);
        P.screen(lay.dashR, hot, 0.010f * lay.plate);
    }

    // ---- casemate breech ---------------------------------------------------
    if (lay.breech) {
        // The block, the recoil slide, and the barrel disappearing through
        // the mantlet ahead. Left of the pilot's shoulder is the loader's side.
        P.chamfer(Vec3(0.78f, lay.dashY + 0.18f, lay.dashZ + 0.35f), Vec3(0.30f, 0.24f, 0.36f), kFrame * 1.1f);
        P.box(Vec3(0.78f, lay.dashY + 0.18f, lay.dashZ + 0.92f), Vec3(0.20f, 0.18f, 0.24f), kFrame * 0.9f);
        P.cyl(Vec3(0.78f, lay.dashY + 0.18f, lay.dashZ + 1.10f), Vec3(0.0f, 0.0f, 1.0f), 0.10f, 1.2f,
              kFrameLit * 0.8f);
        // Recuperator cylinders along the top.
        P.cyl(Vec3(0.66f, lay.dashY + 0.46f, lay.dashZ + 0.05f), Vec3(0.0f, 0.0f, 1.0f), 0.05f, 0.9f, kFrameLit);
        P.cyl(Vec3(0.90f, lay.dashY + 0.46f, lay.dashZ + 0.05f), Vec3(0.0f, 0.0f, 1.0f), 0.05f, 0.9f, kFrameLit);
    }

    // ---- the instruments ---------------------------------------------------
    // Every reading the console gives is a physical fitting on one of its
    // faces: an engraved label plate, a lit gauge, an annunciator. The text
    // pass only fills in the digits, at the same face coordinates, so a
    // number always sits in its own sunk window and never over the world.
    const Vec3 good(0.40f, 1.00f, 0.50f), warn(1.00f, 0.72f, 0.25f), bad(1.00f, 0.32f, 0.25f);
    const Vec3 cool(0.35f, 0.65f, 1.00f);
    const Vec3 dead(0.10f, 0.12f, 0.11f);
    // Where a row's three fittings live across a face, left to right: the
    // engraved name, the gauge, the digit window.
    const float uLab0 = -0.97f, uLab1 = -0.50f;
    const float uBar0 = -0.44f, uBar1 = 0.36f;
    const float uNum0 = 0.42f, uNum1 = 0.97f;

    // ---- left face: structure, heat, and the systems fitted ---------------
    {
        const CabinPanel& f = lay.dashL;
        const int n = cabinRowCount(2 + st.abilityCount);
        const float vh = 0.78f / static_cast<float>(n);
        // Structure: green through amber to red, and the whole bar flashes
        // white on a hit, which is the cabin's own damage indicator.
        {
            const float v = cabinRowV(0, n);
            Vec3 c = gaugeColour(st.hull, good, warn, bad);
            if (st.damageFlash > 0.0f) c = lerp(c, Vec3(1.0f, 1.0f, 1.0f), st.damageFlash * 0.8f);
            P.plate(f, uLab0, uLab1, v, vh, kFrameLit);
            P.gauge(f, uBar0, uBar1, v, st.hull, c, vh);
            P.plate(f, uNum0, uNum1, v, vh, kBezel * 0.8f);
        }
        // Heat: blue, warming through amber to red, strobing when the sink
        // is full and the guns have stopped answering.
        {
            const float v = cabinRowV(1, n);
            Vec3 c = lerp(cool, warn, clampf(st.heat / 0.7f, 0.0f, 1.0f));
            if (st.heat > 0.7f) c = lerp(warn, bad, (st.heat - 0.7f) / 0.3f);
            if (st.overheated) c = bad * (0.5f + 0.5f * std::fabs(std::sin(st.time * 16.0f)));
            P.plate(f, uLab0, uLab1, v, vh, kFrameLit);
            P.gauge(f, uBar0, uBar1, v, st.heat, c, vh);
            P.plate(f, uNum0, uNum1, v, vh, kBezel * 0.8f);
        }
        // One annunciator per system the machine actually carries: dim while
        // it recharges, green when it will fire, amber while it is running.
        int k = 0;
        for (int i = 0; i < 4 && 2 + k < n; ++i) {
            if (!st.abilityFitted[i]) continue;
            const float v = cabinRowV(2 + k, n);
            Vec3 c; float e;
            if (st.abilityEngaged[i]) { c = warn; e = 1.0f; }
            else if (st.abilityReady[i]) { c = good; e = 0.9f; }
            else { c = lerp(good * 0.22f, good * 0.55f, 1.0f - st.abilityCool[i]); e = 0.4f; }
            P.pip(f, -0.90f, v, c, e, 0.045f);
            P.plate(f, uNum0, uNum1, v, vh * 0.85f, kBezel * 0.8f);
            ++k;
        }
    }

    // ---- right face: the drive, and the guns ------------------------------
    {
        const CabinPanel& f = lay.dashR;
        const int n = cabinRowCount(2 + st.gunCount);
        const float vh = 0.78f / static_cast<float>(n);
        // Road speed, as a fitting rather than a number over the world: a
        // lit bar that runs out along the panel, with tick marks behind it
        // so the pilot can read a pace off it without reading the digits.
        {
            const float v = cabinRowV(0, n);
            P.plate(f, uLab0, uLab1, v, vh, kFrameLit);
            const Vec3 dial = lerp(Vec3(0.35f, 0.72f, 1.00f), Vec3(0.55f, 1.00f, 0.85f),
                                   st.speedFrac);
            P.gauge(f, uBar0, uBar1, v, st.speedFrac, dial, vh);
            // Quarter marks along the top of the track.
            for (int t = 1; t < 4; ++t) {
                const float u = uBar0 + (uBar1 - uBar0) * 0.25f * static_cast<float>(t);
                P.pip(f, u, v + vh * 1.15f, kGlassRim * 3.0f, 0.8f, 0.012f);
            }
            P.plate(f, uNum0, uNum1, v, vh, kBezel * 0.8f);
        }
        // Jump charge, or the grip the limbs have while the machine is on a
        // wall - the same gauge, because they are the same question: how
        // much have the legs got stored.
        {
            const float v = cabinRowV(1, n);
            P.plate(f, uLab0, uLab1, v, vh, kFrameLit);
            if (st.climbing) P.gauge(f, uBar0, uBar1, v, 1.0f, Vec3(0.75f, 0.95f, 0.85f), vh);
            else P.gauge(f, uBar0, uBar1, v, st.jump, warn, vh);
            P.plate(f, uNum0, uNum1, v, vh, kBezel * 0.8f);
        }
        // One lamp per mount: dark when switched off, green when it will
        // fire, dim while it cycles, red when it is dry - and warming to
        // amber as a rotary spools or a group's group blooms open.
        for (int i = 0; i < st.gunCount && 2 + i < n; ++i) {
            const float v = cabinRowV(2 + i, n);
            Vec3 c; float e;
            if (!st.gunEnabled[i]) { c = dead; e = 0.2f; }
            else if (st.gunEmpty[i]) { c = bad; e = 0.7f; }
            else if (st.gunReady[i]) { c = lerp(good, warn, st.gunHeat[i]); e = 0.95f; }
            else { c = good * 0.35f; e = 0.35f; }
            P.pip(f, -0.92f, v, c, e, 0.042f);
            P.plate(f, uNum0, uNum1, v, vh * 0.85f, kBezel * 0.8f);
        }
    }

    // ---- the caution row ---------------------------------------------------
    // Four annunciators on the coaming over the radar. The faces carry the
    // numbers; these are the four things worth catching out of the corner of
    // the eye while looking through the glass.
    {
        const bool blink = std::fmod(st.time, 0.6f) < 0.4f;
        struct Ann { bool on; Vec3 lit; };
        const Ann anns[4] = {
            {st.hull < 0.30f && blink, bad},                       // structure
            {st.overheated || st.heat > 0.85f, warn},              // heat
            {st.threat && blink, bad},                             // seen
            {st.jammed && blink, Vec3(0.75f, 0.55f, 1.0f)},        // jammed
        };
        const float pitch = 0.048f * hwAt(lay.panelZ);
        const float y = (lay.panelTop - 0.038f) * hhAt(lay.panelZ);
        for (int i = 0; i < 4; ++i) {
            const float x = lay.dashC.centre.x + (static_cast<float>(i) - 1.5f) * pitch;
            const Vec3 at(x, y, lay.panelZ - 0.004f);
            P.box(at, Vec3(pitch * 0.40f, 0.008f, 0.003f), kBezel, 0.5f);
            P.box(at - Vec3(0.0f, 0.0f, 0.004f), Vec3(pitch * 0.28f, 0.005f, 0.003f),
                  anns[i].on ? anns[i].lit : anns[i].lit * 0.10f, anns[i].on ? 1.0f : 0.22f);
        }
    }

}

} // namespace sb
