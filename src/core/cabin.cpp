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
            // close, a low roof, a full console.
            lay.pillarXf = 0.84f;
            lay.roofY = 0.56f;
            lay.dashYf = -0.74f;
            break;
        case HullFamily::Casemate:
            // The gun lives in here with you. Wide, low glass over the
            // mantlet, the breech filling the right of the cabin.
            lay.pillarXf = 0.92f;
            lay.roofY = 0.50f;
            lay.dashYf = -0.70f;
            lay.breech = true;
            break;
        case HullFamily::LowProfile:
            // A flat hull has no headroom and needs none: a wide bubble on
            // thin struts, the console a shelf at your knees.
            lay.pillarXf = 0.97f;
            lay.pillarR = 0.032f;
            lay.roofY = 0.66f;
            lay.dashYf = -0.80f;
            lay.dashZ = 0.90f;
            lay.bubble = true;
            break;
        case HullFamily::Artillery:
            // An open cradle: rails, no roof, hydraulic columns at the far
            // sides, a periscope frame overhead.
            lay.pillarXf = 0.94f;
            lay.pillarR = 0.07f;
            lay.roofY = 0.80f;
            lay.dashYf = -0.74f;
            lay.openTop = true;
            break;
    }
    switch (style) {
        case 0: lay.roundPillars = true; break;             // pod: tubular frame
        case 2: lay.rake = 0.28f; break;                    // dart: raked glass
        case 3: lay.pillarXf *= 0.88f; lay.roofY += 0.08f; break;  // torso: tall, narrow
        case 4: lay.mullion = true; lay.pillarR *= 1.4f; break;    // long hull: heavy
        default: break;
    }
    if (armour) lay.plate = 0.85f + 0.22f * static_cast<float>(armour->tier);
    lay.sensorTier = sensor ? sensor->tier : 0;
    lay.pillarR *= lay.plate;
    if (lay.breech) {
        // The breech takes the right of the console: the gun group moves in.
        lay.gunU = -0.02f;
        lay.objU = 0.30f;
        lay.threatU = -0.20f;
        lay.scopeU = -0.50f;
    }
    return lay;
}

void CabinLayout::resolve(const Camera& cam) {
    const float tanHalf = std::tan(cam.fovY * 0.5f);
    auto halfW = [&](float z) { return tanHalf * cam.aspect * z; };
    pillarX = pillarXf * halfW(1.0f);
    dashHalfW = dashHalfWf * halfW(dashZ);
    const float dz = dashZ - 0.06f;
    const float hw = halfW(dz);
    dashY = dashYf * tanHalf * dz;
    barLen = barLenF * hw;
    lampPitch = lampPitchF * hw;
    const float y = dashY + 0.02f;
    hullBar = Vec3(hullU * hw, y, dz);
    heatBar = Vec3(hullU * hw, y, dz - 0.07f);
    abilityLamps = Vec3(hullU * hw + lampPitch * 0.5f, y, dz - 0.15f);
    gunLamps = Vec3(gunU * hw + lampPitch * 0.5f, y, dz);
    jumpBar = Vec3(gunU * hw, y, dz - 0.07f);
    objectiveText = Vec3(objU * hw, y, dz);
    threatLamp = Vec3(threatU * hw, y, dz + 0.04f);
    scopeSize = (0.070f + 0.012f * static_cast<float>(sensorTier)) * hw;
    scope = Vec3(scopeU * hw, dashY + 0.01f, dz - 0.10f);
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
    const std::vector<MountedWeapon>& ws = p.weapons();
    st.gunCount = static_cast<int>(std::min<size_t>(ws.size(), 6));
    for (int i = 0; i < st.gunCount; ++i) {
        const MountedWeapon& w = ws[static_cast<size_t>(i)];
        if (!w.part) { st.gunEnabled[i] = false; st.gunEmpty[i] = true; continue; }
        const WeaponDef& def = w.part->weapon;
        st.gunEnabled[i] = w.enabled;
        st.gunEmpty[i] = def.ammo == AmmoKind::Limited && w.rounds == 0 && w.reserve == 0;
        st.gunReady[i] = w.enabled && w.cooldown <= 0.01f &&
                         (def.ammo != AmmoKind::Limited || w.rounds > 0);
        st.gunHeat[i] = (def.spinUp > 0.0f) ? w.spool : clampf(w.bloom / 2.2f, 0.0f, 1.0f);
    }
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
        // Side walls / door frames, back beside the seat.
        if (!lay.bubble) {
            P.box(Vec3(x + static_cast<float>(side) * (pr + 0.10f), (top + bottom) * 0.5f, zP - 0.45f),
                  Vec3(0.07f, (top - bottom) * 0.5f, 0.55f), kFrame * 0.9f);
        } else {
            // A bubble has a low sill and glass above it.
            P.box(Vec3(x + static_cast<float>(side) * (pr + 0.10f), bottom + 0.08f, zP - 0.45f),
                  Vec3(0.07f, 0.10f, 0.55f), kFrame * 0.9f);
        }
    }

    // ---- roof --------------------------------------------------------------
    if (!lay.openTop) {
        // The beam over the glass. Thickness follows the frame but is capped:
        // a heavy hull gets a heavier bar, not a quarter of the view.
        const float beamH = std::min(pr * 0.8f, 0.045f);
        P.box(Vec3(0.0f, top, zP + 0.04f), Vec3(px + pr, beamH, 0.14f), kFrame);
        // The roof plate seen from below, just behind the beam: a thin band,
        // enough to say "enclosed" without hiding the sky.
        P.box(Vec3(0.0f, top + beamH + 0.03f, zP - 0.02f), Vec3(px + pr + 0.1f, 0.025f, 0.18f),
              kFrame * 0.7f);
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
        // Open cradle: a periscope frame overhead and hydraulic columns.
        P.box(Vec3(0.0f, top - 0.02f, zP - 0.15f), Vec3(0.22f, 0.05f, 0.14f), kFrame);
        P.box(Vec3(0.0f, top - 0.16f, zP - 0.15f), Vec3(0.05f, 0.10f, 0.05f), kFrame * 0.8f);
        for (int side = -1; side <= 1; side += 2)
            P.cyl(Vec3(static_cast<float>(side) * (px + 0.32f), bottom, zP - 0.30f),
                  Vec3(0.0f, 1.0f, 0.0f), 0.09f, top - bottom + 0.2f, kFrameLit * 0.8f);
    }

    // ---- the console -------------------------------------------------------
    {
        const float dz = lay.dashZ;
        // Top slab, tilted a little toward the pilot.
        P.chamfer(Vec3(0.0f, lay.dashY - 0.03f, dz), Vec3(lay.dashHalfW, 0.035f, 0.22f), kConsole,
                  Mat4::rotationX(-0.16f));
        // The face below it, down to the floor.
        P.box(Vec3(0.0f, lay.dashY - 0.22f, dz + 0.20f), Vec3(lay.dashHalfW, 0.20f, 0.12f), kFrame * 0.8f);
        // A lit lip at the far edge: the line where the glass starts.
        P.box(Vec3(0.0f, lay.dashY + 0.005f, dz + 0.27f), Vec3(lay.dashHalfW, 0.008f, 0.012f), kGlassRim, 0.1f);
        // Console radar scope: a dark bezel on the console where the radar
        // (painted in the ASCII pass) sits; the hood grows with the sensor.
        const float scope = lay.scopeSize;
        P.box(lay.scope, Vec3(scope * 3.0f, 0.006f, scope * 1.5f), kBezel, 0.2f);
        P.box(lay.scope + Vec3(0.0f, 0.03f, scope * 1.4f), Vec3(scope * 3.1f, 0.03f, 0.02f),
              st.jammed ? Vec3(0.9f, 0.25f, 0.2f) * (0.4f + 0.6f * std::fabs(std::sin(st.time * 13.0f)))
                        : kFrame,
              st.jammed ? 0.9f : 0.0f);
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

    // ---- instruments -------------------------------------------------------
    const Vec3 good(0.40f, 1.00f, 0.50f), warn(1.00f, 0.72f, 0.25f), bad(1.00f, 0.32f, 0.25f);
    const Vec3 cool(0.35f, 0.65f, 1.00f);
    // Hull: green to red, and the whole bar flashes on a hit.
    {
        Vec3 c = gaugeColour(st.hull, good, warn, bad);
        if (st.damageFlash > 0.0f) c = lerp(c, Vec3(1.0f, 1.0f, 1.0f), st.damageFlash * 0.8f);
        P.bar(lay.hullBar, lay.barLen, st.hull, c);
    }
    // Heat: blue, warming through amber to red; strobes when overheated.
    {
        Vec3 c = lerp(cool, warn, clampf(st.heat / 0.7f, 0.0f, 1.0f));
        if (st.heat > 0.7f) c = lerp(warn, bad, (st.heat - 0.7f) / 0.3f);
        if (st.overheated) c = bad * (0.5f + 0.5f * std::fabs(std::sin(st.time * 16.0f)));
        P.bar(lay.heatBar, lay.barLen, st.heat, c);
    }
    // Ability lamps, one per fitted slot: dim while cooling, green when ready,
    // amber while engaged.
    {
        int k = 0;
        for (int i = 0; i < 4; ++i) {
            if (!st.abilityFitted[i]) continue;
            const Vec3 at = lay.abilityLamps + Vec3(lay.lampPitch * static_cast<float>(k++), 0.0f, 0.0f);
            Vec3 c;
            float e;
            if (st.abilityEngaged[i]) { c = warn; e = 1.0f; }
            else if (st.abilityReady[i]) { c = good; e = 0.85f; }
            else { c = lerp(good * 0.25f, good * 0.6f, 1.0f - st.abilityCool[i]); e = 0.4f; }
            P.lamp(at, c, e);
        }
    }
    // Gun lamps: one per mount. Off = dark, ready = green, cycling = dim,
    // dry = red, and a rotary spooling or a group blooming warms to amber.
    for (int i = 0; i < st.gunCount; ++i) {
        const Vec3 at = lay.gunLamps + Vec3(lay.lampPitch * static_cast<float>(i), 0.0f, 0.0f);
        Vec3 c; float e;
        if (!st.gunEnabled[i]) { c = Vec3(0.12f, 0.14f, 0.13f); e = 0.2f; }
        else if (st.gunEmpty[i]) { c = bad; e = 0.6f; }
        else if (st.gunReady[i]) { c = lerp(good, warn, st.gunHeat[i]); e = 0.9f; }
        else { c = good * 0.35f; e = 0.35f; }
        P.lamp(at, c, e);
    }
    // Jump charge (or grip, while climbing).
    if (st.climbing) P.bar(lay.jumpBar, lay.barLen, 1.0f, Vec3(0.75f, 0.95f, 0.85f) * 0.8f);
    else P.bar(lay.jumpBar, lay.barLen, st.jump, warn);
    // Threat lamp in the centre: lit red when something hostile can see you.
    {
        const bool on = st.threat && std::fmod(st.time, 0.6f) < 0.4f;
        P.lamp(lay.threatLamp, on ? bad : Vec3(0.18f, 0.08f, 0.07f), on ? 1.0f : 0.25f, 0.030f);
    }
}

} // namespace sb
