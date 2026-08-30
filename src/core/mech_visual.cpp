// mech_visual.cpp - assembling a spidertank out of parts.
//
// Everything is an instance of one of a handful of unit primitives, placed by a
// transform. A fully kitted machine comes to roughly 190 of them, which is what
// gives it the layered, plated, hydraulic look of a real vehicle rather than a
// stick figure - and costs almost nothing in memory because the meshes are
// shared by every mech in the level.
//
// Parts carry an LOD tier: 0 draws at any range, 1 inside medium range, 2 only
// up close. A distant enemy is about twenty boxes; the one in your face is all
// hundred and ninety.
#include "mech.h"

namespace sb {

MechMeshLibrary::MechMeshLibrary() {
    box = makeBox(Vec3(1.0f, 1.0f, 1.0f));
    chamfer = makeChamferBox(Vec3(1.0f, 1.0f, 1.0f), 0.34f);
    cyl4 = makeCylinder(1.0f, 1.0f, 1.0f, 4, true, true);
    cyl6 = makeCylinder(1.0f, 1.0f, 1.0f, 6, true, true);
    cyl8 = makeCylinder(1.0f, 1.0f, 1.0f, 8, true, true);
    cyl12 = makeCylinder(1.0f, 1.0f, 1.0f, 12, true, true);
    cone6 = makeCylinder(1.0f, 0.12f, 1.0f, 6, true, true);
    sphere = makeSphere(1.0f, 5, 7);
    wedge = makeSlopedBox(Vec3(1.0f, 0.0f, 1.0f), Vec3(0.55f, 0.0f, 0.75f), 1.0f);
    taper = makeSlopedBox(Vec3(1.0f, 0.0f, 1.0f), Vec3(0.55f, 0.0f, 0.55f), 1.0f);
    pod = makeCylinder(1.0f, 0.5f, 1.0f, 8, true, true);
}

const MechMeshLibrary& MechMeshLibrary::instance() {
    static const MechMeshLibrary lib;
    return lib;
}

namespace {

// Materials. Kept deliberately close in value so the machine reads as one
// object, with the deck lightest so it is legible from above and a single warm
// accent for identity.
struct Palette {
    Vec3 deck, hull, dark, limb, joint, accent, barrel, glow;
};

Palette paletteFor(const PartDef* chassis, const PartDef* legs, Team team) {
    Palette p;
    const Vec3 base = chassis ? chassis->tint : Vec3(0.55f, 0.57f, 0.58f);
    const Vec3 limbBase = legs ? legs->tint : Vec3(0.55f, 0.56f, 0.56f);

    p.deck = base * 0.86f;
    p.hull = base * 0.58f;
    p.dark = base * 0.34f;
    p.limb = limbBase * 0.60f;
    p.joint = limbBase * 0.88f;
    p.barrel = Vec3(0.34f, 0.36f, 0.40f);

    if (team == Team::Hostile) {
        // Hostiles run a red-brown warpaint so they read instantly, even in the
        // monochrome palettes where only the value difference survives.
        p.deck = Vec3(0.52f, 0.34f, 0.28f);
        p.hull = Vec3(0.30f, 0.19f, 0.16f);
        p.dark = Vec3(0.18f, 0.12f, 0.11f);
        p.limb = Vec3(0.34f, 0.26f, 0.24f);
        p.joint = Vec3(0.50f, 0.38f, 0.34f);
        p.accent = Vec3(0.88f, 0.22f, 0.14f);
        p.glow = Vec3(1.0f, 0.35f, 0.22f);
    } else {
        p.accent = Vec3(0.85f, 0.45f, 0.10f);
        p.glow = Vec3(0.40f, 1.00f, 0.60f);
    }
    return p;
}

} // namespace

void Mech::buildVisual() {
    const MechMeshLibrary& L = MechMeshLibrary::instance();
    visual_.clear();
    visual_.reserve(220);

    const PartDef* chassis = loadout_.part(Slot::Chassis);
    const PartDef* legsPart = loadout_.part(Slot::Legs);
    const PartDef* enginePart = loadout_.part(Slot::Engine);
    const PartDef* armorPart = loadout_.part(Slot::Armor);
    const PartDef* sensorPart = loadout_.part(Slot::Sensor);
    const Palette pal = paletteFor(chassis, legsPart, team_);
    limbTint_ = pal.limb;
    // Leg armour wears the hull's own deck tone: the machine is plated in ONE
    // armour system, hull and limbs together, which is most of the "tank on
    // legs" read. The dark struts underneath provide the contrast.
    // Brighter than the deck on purpose: leg plates face SIDEWAYS, where
    // the key light and the sky ambient both arrive at a grazing angle, so
    // an honest deck tone shades down to mud. Overdriving the albedo is what
    // makes the plating read as the same pale armour as the hull.
    platePal_ = pal.deck * 1.05f;
    // Fitted armour plates the LIMBS too: the same purchase that hangs slabs
    // on the hull laps extra scales down the legs, in the armour's own tint.
    armorLegT_ = (armorPart && armorPart->stats.mass > 0.1f)
                     ? clampf(armorPart->stats.mass / 9.0f, 0.15f, 1.0f)
                     : 0.0f;
    armorTint_ = armorPart ? armorPart->tint : pal.deck;
    legStyle_ = legsPart ? legsPart->style : 0;
    // Heavier limbs are visibly thicker. Mass is the honest driver: a Titan set
    // masses three times a Scout set and should look it.
    legGirth_ = legsPart ? clampf(0.72f + legsPart->stats.mass / 7.0f, 0.72f, 1.75f) : 1.0f;
    jointTint_ = pal.joint;
    darkTint_ = pal.dark;
    accentTint_ = pal.accent;
    glowTint_ = pal.glow;

    Rng rng(seed_ * 2654435761u + 17u);

    // Every piece is stamped with the part that put it there, so the store can
    // isolate one part's geometry later. `section` is set at the top of each
    // block below and every add() inherits it.
    Slot section = Slot::Chassis;

    auto add = [&](const Mesh* mesh, const Mat4& m, const Vec3& tint, uint8_t lod,
                   uint8_t frame = 0, float emissive = 0.0f) {
        VisualPart vp;
        vp.mesh = mesh;
        vp.local = m;
        vp.tint = tint;
        vp.lod = lod;
        vp.frame = frame;
        vp.emissive = emissive;
        vp.origin = section;
        visual_.push_back(vp);
    };
    auto slab = [&](const Vec3& c, const Vec3& h, const Vec3& tint, uint8_t lod,
                    uint8_t frame = 0, float yaw = 0.0f, float pitch = 0.0f) {
        add(&L.box, Mat4::translation(c) * Mat4::rotationY(yaw) * Mat4::rotationX(pitch) *
                    Mat4::scaling(h), tint, lod, frame);
    };
    auto chamferSlab = [&](const Vec3& c, const Vec3& h, const Vec3& tint, uint8_t lod,
                           uint8_t frame = 0, float yaw = 0.0f) {
        add(&L.chamfer, Mat4::translation(c) * Mat4::rotationY(yaw) * Mat4::scaling(h),
            tint, lod, frame);
    };

    // Chassis proportions scale with the hull's mass class, so a siege deck is
    // visibly a bigger machine than a scout frame.
    const float bulk = chassis ? clampf(chassis->stats.mass / 5.0f, 0.75f, 1.9f) : 1.0f;
    bulk_ = bulk;
    const HullFamily fam = chassis ? chassis->family : HullFamily::Turreted;
    // The family changes the silhouette before any detail does: a low-profile
    // hull is a flattened, widened wedge; everything else keeps the block.
    const float flat = (fam == HullFamily::LowProfile) ? 0.62f : 1.0f;
    // Per-style PROPORTIONS: the same tonnage stacked as a tall pod, a long
    // hull, a wide shell or a box is four different machines before a single
    // panel goes on. These multipliers are the body plan.
    float pw = 1.0f, pl = 1.0f, pt = 1.0f;
    const int hsProp = chassis ? chassis->style : 0;
    switch (hsProp) {
        case 0: pw = 0.82f; pl = 0.72f; pt = 1.75f; break;   // round pod
        case 2: pw = 0.85f; pl = 1.20f; pt = 0.85f; break;   // dart
        case 3: pw = 1.10f; pl = 0.82f; pt = 1.45f; break;   // upright torso
        case 4: pw = 0.92f; pl = 1.30f; pt = 1.00f; break;   // long gun hull
        case 6: pw = 1.45f; pl = 1.00f; pt = 0.70f; break;   // flat crab shell
        default: break;
    }
    const float hw = 1.42f * bulk * (fam == HullFamily::LowProfile ? 1.14f : 1.0f) * pw;
    const float hl = 1.78f * bulk * pl; // half length
    const float ht = 0.60f * bulk * flat * pt; // half thickness of the core hull

    // Where the turret ring sits, by body plan.
    switch (chassis ? chassis->style : 0) {
        case 0:  turretMountY_ = ht * 1.28f; break;   // atop the pod dome
        case 3:  turretMountY_ = ht * 1.66f; break;   // the turret IS the head
        case 6:  turretMountY_ = ht * 0.95f; break;   // on the shell crown
        default: turretMountY_ = -1.0f; break;
    }

    // ======================================================= CORE HULL ======
    // Each chassis style is a different VEHICLE, not a rescaled brick. The
    // style picks the silhouette recipe; the family already picked the rules.
    // Every recipe ends with the same tank furniture - hip fenders over the
    // leg roots, integral skirt plates with visible bolt seams, tow points -
    // because that shared vocabulary is what makes them all read as armour.
    section = Slot::Chassis;
    const int hs = chassis ? chassis->style : 0;

    // Bolted plate: a slab plus a seam line and corner bolts, the basic unit
    // of "this machine is made of armour plate" at every scale.
    auto plate = [&](const Vec3& c, const Vec3& h, const Vec3& tint, uint8_t lod,
                     float yaw = 0.0f) {
        chamferSlab(c, h, tint, lod, 0, yaw);
        const float bx = h.x * 0.78f, bz = h.z * 0.78f;
        for (int cx = -1; cx <= 1; cx += 2)
            for (int cz = -1; cz <= 1; cz += 2)
                add(&L.cyl6, Mat4::translation(c + Vec3(std::cos(yaw) * cx * bx +
                                                        std::sin(yaw) * cz * bz,
                                                        h.y + 0.012f * bulk,
                                                        -std::sin(yaw) * cx * bx +
                                                        std::cos(yaw) * cz * bz)) *
                             Mat4::scaling(Vec3(0.028f * bulk, 0.018f * bulk, 0.028f * bulk)),
                    tint * 1.28f, 2);
    };

    switch (hs) {
        case 0: {
            // WASP / DRONE - the POD. A rounded faceted drum with legs
            // sprouting from its equator: the Tachikoma plan. Bottom taper,
            // fat equator drum, top taper, dome - no box anywhere.
            add(&L.pod, Mat4::translation(Vec3(0.0f, -ht * 0.15f, 0.0f)) *
                        Mat4::rotationX(PI) * Mat4::rotationY(PI / 8.0f) *
                        Mat4::scaling(Vec3(hw * 0.94f, ht * 0.85f, hl * 0.60f)),
                pal.hull, 0);
            add(&L.cyl8, Mat4::translation(Vec3(0.0f, -ht * 0.18f, 0.0f)) *
                         Mat4::rotationY(PI / 8.0f) *
                         Mat4::scaling(Vec3(hw * 0.96f, ht * 0.55f, hl * 0.62f)),
                pal.hull, 0);
            add(&L.pod, Mat4::translation(Vec3(0.0f, ht * 0.35f, 0.0f)) *
                        Mat4::rotationY(PI / 8.0f) *
                        Mat4::scaling(Vec3(hw * 0.95f, ht * 0.72f, hl * 0.61f)),
                pal.deck, 0);
            add(&L.sphere, Mat4::translation(Vec3(0.0f, ht * 0.95f, 0.0f)) *
                           Mat4::scaling(Vec3(hw * 0.52f, ht * 0.42f, hl * 0.36f)),
                pal.deck, 0);
            // One seam ring at the equator - a panel line, not clutter.
            add(&L.cyl8, Mat4::translation(Vec3(0.0f, -ht * 0.02f, 0.0f)) *
                         Mat4::rotationY(PI / 8.0f) *
                         Mat4::scaling(Vec3(hw * 0.985f, 0.02f * bulk, hl * 0.635f)),
                pal.dark, 1);
            // Sensor eye on the bow.
            add(&L.sphere, Mat4::translation(Vec3(0.0f, ht * 0.1f, hl * 0.62f)) *
                           Mat4::scaling(Vec3(0.10f * bulk)), pal.glow, 1, 0, 0.8f);
            break;
        }
        case 1: {
            // MULE - the freight hull. Tall vertical slab sides, a squared
            // bow, cargo stowage strapped along the flanks: a container truck
            // conscripted into a war.
            slab(Vec3(0.0f, ht * 0.10f, -hl * 0.06f),
                 Vec3(hw * 0.95f, ht * 1.18f, hl * 0.88f), pal.hull, 0);
            slab(Vec3(0.0f, ht * 0.05f, hl * 0.82f),
                 Vec3(hw * 0.88f, ht * 0.95f, hl * 0.16f), pal.hull, 0);
            add(&L.wedge, Mat4::translation(Vec3(0.0f, ht * 0.9f, hl * 0.78f)) *
                          Mat4::scaling(Vec3(hw * 0.84f, ht * 0.55f, hl * 0.24f)), pal.deck, 0);
            chamferSlab(Vec3(0.0f, ht * 1.18f, -0.10f * bulk),
                        Vec3(hw * 0.86f, 0.14f * bulk, hl * 0.78f), pal.deck, 0);
            break;
        }
        case 2: {
            // SHRIKE - the raider. A long raked arrowhead: needle prow, swept
            // side panels, a pinched waist, twin tail fins. Fast even parked.
            chamferSlab(Vec3(0.0f, 0.0f, -hl * 0.18f),
                        Vec3(hw * 0.78f, ht * 0.92f, hl * 0.66f), pal.hull, 0);
            add(&L.wedge, Mat4::translation(Vec3(0.0f, ht * 0.05f, hl * 0.62f)) *
                          Mat4::scaling(Vec3(hw * 0.62f, ht * 1.0f, hl * 0.58f)),
                pal.deck * 0.92f, 0);
            add(&L.wedge, Mat4::translation(Vec3(0.0f, ht * 0.35f, hl * 0.30f)) *
                          Mat4::scaling(Vec3(hw * 0.40f, ht * 0.72f, hl * 0.52f)), pal.deck, 0);
            // Swept side panels, canted in plan like folded wings.
            for (int s = -1; s <= 1; s += 2) {
                chamferSlab(Vec3(s * hw * 0.88f, 0.02f * bulk, -hl * 0.02f),
                            Vec3(0.10f * bulk, ht * 0.75f, hl * 0.52f), pal.deck, 0, 0,
                            s * -0.22f);
                // Tail fin.
                add(&L.wedge, Mat4::translation(Vec3(s * hw * 0.55f, ht + 0.30f * bulk,
                                                     -hl * 0.78f)) *
                              Mat4::rotationY(PI) * Mat4::rotationZ(s * deg2rad(14.0f)) *
                              Mat4::scaling(Vec3(0.05f * bulk, 0.45f * bulk, hl * 0.26f)),
                    pal.deck, 1);
            }
            chamferSlab(Vec3(0.0f, ht + 0.06f * bulk, -0.12f * bulk),
                        Vec3(hw * 0.58f, 0.10f * bulk, hl * 0.56f), pal.deck, 0);
            break;
        }
        case 3: {
            // REVENANT - the upright TORSO of the camo reference: a tall
            // chest standing over the leg ring, head high, shoulders wide.
            // Nothing else in the catalogue stands this upright.
            chamferSlab(Vec3(0.0f, ht * 0.35f, -hl * 0.05f),
                        Vec3(hw * 0.72f, ht * 1.15f, hl * 0.62f), pal.hull, 0);
            // Pelvis block: fills chest-bottom to belly so the machine is one
            // solid mass from any angle, not a shell over a dark cavity.
            chamferSlab(Vec3(0.0f, -ht * 0.55f, -hl * 0.05f),
                        Vec3(hw * 0.80f, ht * 0.55f, hl * 0.70f), pal.hull, 0);
            add(&L.wedge, Mat4::translation(Vec3(0.0f, ht * 0.10f, hl * 0.58f)) *
                          Mat4::scaling(Vec3(hw * 0.66f, ht * 1.35f, hl * 0.42f)),
                pal.deck * 0.92f, 0);
            // No separate head: the TURRET is the head, riding clear above
            // the shoulder line. The eye sits on the chest bow.
            add(&L.sphere, Mat4::translation(Vec3(0.0f, ht * 1.30f, hl * 0.72f)) *
                           Mat4::scaling(Vec3(0.09f * bulk)), pal.glow, 1, 0, 0.85f);
            break;
        }
        case 4: {
            // BASTION - the LONG GUN HULL of the field-painting reference: a
            // stretched tank body riding high over under-slung legs, with a
            // real prow, deck line and stern. The longest plan there is.
            chamferSlab(Vec3(0.0f, 0.0f, -hl * 0.08f),
                        Vec3(hw * 0.95f, ht * 0.9f, hl * 0.82f), pal.hull, 0);
            add(&L.wedge, Mat4::translation(Vec3(0.0f, ht * 0.1f, hl * 0.74f)) *
                          Mat4::scaling(Vec3(hw * 0.85f, ht * 1.15f, hl * 0.45f)),
                pal.deck * 0.92f, 0);
            add(&L.taper, Mat4::translation(Vec3(0.0f, -ht * 0.2f, hl * 0.95f)) *
                          Mat4::rotationX(deg2rad(-78.0f)) *
                          Mat4::scaling(Vec3(hw * 0.6f, ht * 1.0f, ht * 0.5f)),
                pal.deck, 0);
            chamferSlab(Vec3(0.0f, ht * 0.85f, -0.04f * bulk),
                        Vec3(hw * 1.02f, 0.16f * bulk, hl * 1.02f), pal.deck, 0);
            add(&L.wedge, Mat4::translation(Vec3(0.0f, ht * 0.2f, -hl * 0.86f)) *
                          Mat4::rotationY(PI) *
                          Mat4::scaling(Vec3(hw * 0.8f, ht * 0.8f, hl * 0.22f)), pal.hull, 0);
            // Deck edge galleries and their struts.
            for (int s = -1; s <= 1; s += 2) {
                slab(Vec3(s * hw * 1.0f, ht * 0.55f, 0.0f),
                     Vec3(0.06f * bulk, 0.10f * bulk, hl * 0.90f), pal.dark, 1);
            }
            break;
        }
        case 5: {
            // FERRUM / JUDICATOR - the casemate hulls. Short, tall, and all
            // bow: the hull proper is a plinth for the armoured superstructure
            // the family block builds above. Interlocked plate rows make the
            // glacis read as forged, not cast.
            chamferSlab(Vec3(0.0f, 0.0f, -hl * 0.08f),
                        Vec3(hw, ht * 1.1f, hl * 0.86f), pal.hull, 0);
            add(&L.wedge, Mat4::translation(Vec3(0.0f, ht * 0.1f, hl * 0.70f)) *
                          Mat4::scaling(Vec3(hw * 0.98f, ht * 1.6f, hl * 0.44f)),
                pal.deck * 0.92f, 0);
            // One heavy bow plate.
            chamferSlab(Vec3(0.0f, ht * 0.15f, hl * 0.88f),
                        Vec3(hw * 0.78f, ht * 0.55f, 0.06f * bulk), pal.deck, 1);
            chamferSlab(Vec3(0.0f, ht + 0.12f * bulk, -hl * 0.34f),
                        Vec3(hw * 0.80f, 0.13f * bulk, hl * 0.44f), pal.deck, 0);
            break;
        }
        case 6: {
            // VIPER - the CRAB SHELL: one wide flattened dome of a carapace
            // with a skirt ring, the legs sprawling from under its rim. The
            // widest, lowest plan in the catalogue.
            add(&L.pod, Mat4::translation(Vec3(0.0f, -ht * 0.3f, 0.0f)) *
                        Mat4::rotationY(PI / 8.0f) *
                        Mat4::scaling(Vec3(hw * 0.98f, ht * 1.5f, hl * 0.85f)),
                pal.hull, 0);
            add(&L.cyl8, Mat4::translation(Vec3(0.0f, -ht * 0.45f, 0.0f)) *
                         Mat4::rotationY(PI / 8.0f) *
                         Mat4::scaling(Vec3(hw * 1.0f, ht * 0.35f, hl * 0.88f)),
                pal.hull, 0);
            // Shell segments: three lapped tapered bands running nose to tail.
            for (int i = 0; i <= 0; ++i)
                add(&L.taper, Mat4::translation(Vec3(0.0f, ht * (0.45f - 0.12f * std::fabs((float)i)),
                                                     i * hl * 0.42f)) *
                              Mat4::rotationX(i < 0 ? PI : 0.0f) *
                              Mat4::scaling(Vec3(hw * (0.80f - 0.10f * std::fabs((float)i)),
                                                 hl * 0.34f, ht * 0.55f)),
                    i == 0 ? pal.deck : pal.deck * 0.92f, 0);
            // Skirt ring under the rim.
            add(&L.cyl8, Mat4::translation(Vec3(0.0f, -ht * 0.75f, 0.0f)) *
                         Mat4::rotationY(PI / 8.0f) *
                         Mat4::scaling(Vec3(hw * 0.90f, ht * 0.22f, hl * 0.80f)),
                pal.dark, 1);
            // Eye slit on the bow rim.
            slab(Vec3(0.0f, -ht * 0.05f, hl * 0.86f),
                 Vec3(hw * 0.26f, 0.03f * bulk, 0.03f * bulk), pal.glow, 1);
            break;
        }
        default: {
            // LONGBOW - the fire platform. A short heavy hull that exists to
            // anchor the cradle: rail bed running the full length, a massive
            // rear counterweight block, an open lattice mast forward.
            chamferSlab(Vec3(0.0f, 0.0f, hl * 0.10f),
                        Vec3(hw * 0.92f, ht, hl * 0.72f), pal.hull, 0);
            plate(Vec3(0.0f, -ht * 0.2f, -hl * 0.74f),
                  Vec3(hw * 0.98f, ht * 0.95f, hl * 0.28f), pal.hull, 0);
            // The counterweight: darkest, densest thing on the machine.
            slab(Vec3(0.0f, ht * 0.55f, -hl * 0.80f),
                 Vec3(hw * 0.72f, ht * 0.70f, hl * 0.20f), pal.dark, 0);
            // Rail bed.
            for (int s = -1; s <= 1; s += 2)
                slab(Vec3(s * hw * 0.34f, ht + 0.10f * bulk, -0.06f * bulk),
                     Vec3(0.07f * bulk, 0.09f * bulk, hl * 0.94f), pal.joint, 0);
            add(&L.wedge, Mat4::translation(Vec3(0.0f, ht * 0.15f, hl * 0.86f)) *
                          Mat4::scaling(Vec3(hw * 0.72f, ht * 0.95f, hl * 0.26f)),
                pal.deck * 0.92f, 0);
            break;
        }
    }

    // Belly - the boxy hulls get a full closing plate AND a keel below it,
    // so the underside is a solid volume rather than a dark hole between
    // legs. The round plans (pod, crab) already close themselves and a flat
    // slab under a drum reads as a mistake, not armour.
    if (hs != 0 && hs != 6) {
        slab(Vec3(0.0f, -ht - 0.09f * bulk, 0.0f),
             Vec3(hw * 0.95f, 0.10f * bulk, hl * 0.94f), pal.hull * 0.8f, 0);
        chamferSlab(Vec3(0.0f, -ht - 0.22f * bulk, 0.0f),
                    Vec3(hw * 0.55f, 0.12f * bulk, hl * 0.70f), pal.dark, 1);
    }

    // Raised turret mounts get a pedestal drum closing the gap between the
    // hull top and the ring - a floating turret is the openest look there is.
    if (turretMountY_ > 0.0f && turretMountY_ > ht * 1.05f)
        add(&L.cyl8, Mat4::translation(Vec3(0.0f, ht * 0.80f, 0.0f)) *
                     Mat4::scaling(Vec3(0.46f * bulk, turretMountY_ - ht * 0.80f,
                                        0.46f * bulk)),
            pal.hull, 0);

    // ---------------------------------------------- shared tank furniture --
    // Integral skirt plates: deep flank slabs running most of the hull, which
    // close the open gap between the hull bottom and the leg tops.
    // Like the leg plates, they face SIDEWAYS into grazing light, so they
    // get the same overdriven tint and shadow lift or they render as pits.
    if (hs != 0 && hs != 6)
        for (int s = -1; s <= 1; s += 2)
            add(&L.chamfer,
                Mat4::translation(Vec3(s * (hw + 0.09f * bulk), -0.16f * bulk, 0.0f)) *
                    Mat4::scaling(Vec3(0.08f * bulk, ht * 0.95f, hl * 0.92f)),
                pal.deck * 1.02f, 1, 0, 0.10f);

    // Hazard stripes: the only saturated colour on the machine.
    if (hs != 0 && hs != 6)
    for (int s = -1; s <= 1; s += 2) {
        slab(Vec3(s * hw * 0.52f, ht + 0.335f * bulk, hl * 0.10f),
             Vec3(0.10f * bulk, 0.022f * bulk, hl * 0.34f), pal.accent, 1);
    }

    // ======================================================= ENGINE =========
    section = Slot::Engine;
    if (enginePart) {
        const Vec3 eTint = enginePart->tint;
        const float er = 0.30f * bulk;
        const int es = enginePart->style;

        // Core barrel. A fusion core is a fat drum, a cheap cell is a thin can.
        const float coreR = er * (0.85f + 0.16f * es);
        add(&L.cyl8, Mat4::translation(Vec3(0.0f, ht + 0.12f * bulk, -hl * 0.62f)) *
                     Mat4::rotationX(deg2rad(90.0f)) *
                     Mat4::scaling(Vec3(coreR, hl * 0.34f, coreR)), eTint, 1);

        // Exhaust stacks. Arrangement is per-core: paired, a fan, or a raked
        // cluster canted outward.
        const int stacks = (enginePart->stats.powerOutput > 6.0f) ? 4 :
                           (enginePart->stats.powerOutput > 3.5f) ? 3 : 2;
        for (int i = 0; i < stacks; ++i) {
            const float t = (stacks > 1) ? (i / static_cast<float>(stacks - 1)) - 0.5f : 0.0f;
            const float x = t * 0.52f * bulk;
            const float cant = (es >= 3) ? t * deg2rad(26.0f) : 0.0f;
            const float len = 0.42f * bulk * (0.8f + 0.16f * es);
            add(&L.cyl6, Mat4::translation(Vec3(x, ht + 0.30f * bulk, -hl * 0.86f)) *
                         Mat4::rotationZ(cant) *
                         Mat4::scaling(Vec3(0.075f * bulk, len, 0.075f * bulk)),
                pal.dark, 1);
            add(&L.cyl6, Mat4::translation(Vec3(x + std::sin(cant) * len,
                                                ht + 0.30f * bulk + len, -hl * 0.86f)) *
                         Mat4::rotationZ(cant) *
                         Mat4::scaling(Vec3(0.092f * bulk, 0.07f * bulk, 0.092f * bulk)),
                pal.dark * 0.7f, 2);
        }
    }

    // ======================================================= ARMOUR =========
    section = Slot::Armor;
    // Bolt-on plating. Heavier armour parts add more and thicker plates, so the
    // machine visibly grows a shell as you spend money on it.
    if (armorPart && armorPart->stats.mass > 0.1f) {
        const float t = clampf(armorPart->stats.mass / 9.0f, 0.15f, 1.0f);
        const float pt = 0.055f * bulk + 0.075f * bulk * t;      // plate thickness
        const Vec3 plateTint = armorPart->tint;
        const int rows = 2 + static_cast<int>(t * 2.5f);

        for (int s = -1; s <= 1; s += 2) {
            for (int i = 0; i < rows; ++i) {
                const float z = hl * (0.72f - 1.44f * i / std::max(1, rows - 1));
                chamferSlab(Vec3(s * (hw + 0.20f * bulk + pt), 0.02f * bulk, z),
                            Vec3(pt, ht * 0.92f, hl * (0.62f / rows)), plateTint, 0);
                // Bolt heads.
                for (int b = -1; b <= 1; b += 2)
                    add(&L.cyl6, Mat4::translation(Vec3(s * (hw + 0.20f * bulk + pt * 2.0f),
                                                        b * ht * 0.5f, z)) *
                                 Mat4::rotationZ(deg2rad(90.0f)) *
                                 Mat4::scaling(Vec3(0.035f * bulk, 0.02f * bulk, 0.035f * bulk)),
                        plateTint * 1.3f, 2);
            }
        }
        // Frontal glacis plate and roof plates.
        chamferSlab(Vec3(0.0f, ht * 0.15f, hl + pt * 1.2f),
                    Vec3(hw * 0.86f, ht * 0.95f, pt), plateTint, 0);
        for (int i = 0; i < 2; ++i)
            chamferSlab(Vec3(0.0f, ht + 0.33f * bulk + pt, hl * (0.42f - 0.84f * i)),
                        Vec3(hw * 0.66f, pt, hl * 0.34f), plateTint * 1.06f, 1);
        // Each plating type wears differently, so you can tell at a glance what
        // a machine is carrying rather than only that it is carrying more.
        switch (armorPart->style) {
            case 1: {
                // Spall liner: a thin skirt hanging below the hull line.
                for (int s = -1; s <= 1; s += 2)
                    for (int i = 0; i < 4; ++i)
                        slab(Vec3(s * (hw + 0.16f * bulk), -ht - 0.16f * bulk,
                                  hl * (0.66f - 0.44f * i)),
                             Vec3(0.03f * bulk, 0.13f * bulk, hl * 0.18f),
                             plateTint * 0.85f, 2);
                break;
            }
            case 2: {
                // Layered ceramic: overlapping angled tiles up the flanks.
                for (int s = -1; s <= 1; s += 2)
                    for (int i = 0; i < 5; ++i)
                        add(&L.wedge,
                            Mat4::translation(Vec3(s * (hw + 0.26f * bulk),
                                                   -ht * 0.4f + i * 0.20f * bulk,
                                                   hl * 0.05f)) *
                            Mat4::rotationZ(deg2rad(s * 74.0f)) *
                            Mat4::scaling(Vec3(0.16f * bulk, 0.05f * bulk, hl * 0.62f)),
                            plateTint * (0.92f + 0.03f * i), 1);
                break;
            }
            case 3: {
                // Reactive slabs: chunky boxes with a bright detonation seam.
                for (int s = -1; s <= 1; s += 2)
                    for (int i = 0; i < 4; ++i) {
                        const float z = hl * (0.66f - 0.44f * i);
                        slab(Vec3(s * (hw + 0.30f * bulk), ht * 0.2f, z),
                             Vec3(0.13f * bulk, 0.19f * bulk, hl * 0.17f), plateTint, 1);
                        slab(Vec3(s * (hw + 0.44f * bulk), ht * 0.2f, z),
                             Vec3(0.012f * bulk, 0.16f * bulk, hl * 0.14f),
                             pal.accent, 2);
                    }
                for (int s = -1; s <= 1; s += 2)
                    for (int i = 0; i < 3; ++i)
                        slab(Vec3(s * hw * 0.78f, ht + 0.42f * bulk, hl * (0.5f - 0.5f * i)),
                             Vec3(0.16f * bulk, 0.10f * bulk, 0.20f * bulk), plateTint * 0.9f, 2);
                break;
            }
            case 4: {
                // Ablative hex: a shell of staggered hexagonal pads over
                // everything, which is what a top-tier machine should look like.
                for (int s = -1; s <= 1; s += 2)
                    for (int i = 0; i < 6; ++i)
                        for (int k = 0; k < 2; ++k)
                            add(&L.cyl6,
                                Mat4::translation(Vec3(s * (hw + 0.30f * bulk),
                                                       -ht * 0.5f + k * 0.55f * bulk,
                                                       hl * (0.74f - 0.30f * i))) *
                                Mat4::rotationZ(deg2rad(90.0f)) *
                                Mat4::scaling(Vec3(0.17f * bulk, 0.07f * bulk, 0.17f * bulk)),
                                plateTint * (k ? 1.05f : 0.95f), 1);
                for (int i = 0; i < 4; ++i)
                    add(&L.cyl6, Mat4::translation(Vec3((i - 1.5f) * 0.42f * bulk,
                                                        ht + 0.44f * bulk, hl * 0.30f)) *
                                 Mat4::scaling(Vec3(0.19f * bulk, 0.05f * bulk, 0.19f * bulk)),
                        plateTint * 1.1f, 2);
                break;
            }
            default: break;
        }
    }

    // ======================================================= SENSORS ========
    section = Slot::Sensor;
    if (sensorPart) {
        const Vec3 sTint = sensorPart->tint;
        const int ss = sensorPart->style;
        const float headLift = (hs == 0) ? ht * 0.75f
                             : (hs == 3) ? ht * 1.0f
                             : (hs == 6) ? ht * 0.30f : 0.0f;
        const Vec3 head(0.30f * bulk, ht + 0.42f * bulk + headLift, hl * 0.52f);

        if (ss == 0) {
            // Optical pod: a squat dome with a lens.
            add(&L.sphere, Mat4::translation(head) * Mat4::scaling(Vec3(0.15f * bulk)),
                sTint, 1);
            add(&L.cyl8, Mat4::translation(head + Vec3(0.0f, 0.0f, 0.13f * bulk)) *
                         Mat4::rotationX(deg2rad(90.0f)) *
                         Mat4::scaling(Vec3(0.07f * bulk, 0.05f * bulk, 0.07f * bulk)),
                pal.glow, 2, 0, 0.7f);
        } else if (ss == 1) {
            // Wide-angle: a bar of three lenses on a short pylon.
            add(&L.box, Mat4::translation(head) *
                        Mat4::scaling(Vec3(0.30f * bulk, 0.07f * bulk, 0.09f * bulk)),
                sTint, 1);
            for (int i = 0; i <= 0; ++i)
                add(&L.cyl8, Mat4::translation(head + Vec3(i * 0.19f * bulk, 0.0f,
                                                           0.09f * bulk)) *
                             Mat4::rotationX(deg2rad(90.0f)) *
                             Mat4::scaling(Vec3(0.05f * bulk, 0.04f * bulk, 0.05f * bulk)),
                    pal.glow, 2, 0, 0.65f);
        } else if (ss == 2) {
            // Tracker array: a rotating flat panel on a mast.
            add(&L.cyl6, Mat4::translation(head) *
                         Mat4::scaling(Vec3(0.05f * bulk, 0.46f * bulk, 0.05f * bulk)),
                pal.dark, 1);
            add(&L.box, Mat4::translation(head + Vec3(0.0f, 0.50f * bulk, 0.0f)) *
                        Mat4::rotationY(deg2rad(28.0f)) *
                        Mat4::scaling(Vec3(0.30f * bulk, 0.025f * bulk, 0.16f * bulk)),
                sTint, 1);
            for (int i = 0; i < 3; ++i)
                add(&L.box, Mat4::translation(head + Vec3(0.0f, 0.53f * bulk,
                                                          (i - 1) * 0.09f * bulk)) *
                            Mat4::rotationY(deg2rad(28.0f)) *
                            Mat4::scaling(Vec3(0.28f * bulk, 0.012f * bulk, 0.012f * bulk)),
                    pal.glow * 0.8f, 2, 0, 0.5f);
        } else {
            // Lidar mast: a tall spine with a spinning drum and a dish.
            add(&L.cyl6, Mat4::translation(head) *
                         Mat4::scaling(Vec3(0.055f * bulk, 0.78f * bulk, 0.055f * bulk)),
                pal.dark, 0);
            add(&L.cyl12, Mat4::translation(head + Vec3(0.0f, 0.80f * bulk, 0.0f)) *
                          Mat4::scaling(Vec3(0.17f * bulk, 0.13f * bulk, 0.17f * bulk)),
                sTint, 0);
            add(&L.cyl12, Mat4::translation(head + Vec3(0.0f, 0.94f * bulk, 0.0f)) *
                          Mat4::scaling(Vec3(0.19f * bulk, 0.02f * bulk, 0.19f * bulk)),
                pal.glow, 1, 0, 0.8f);
            add(&L.cone6, Mat4::translation(head + Vec3(0.16f * bulk, 0.58f * bulk, 0.0f)) *
                          Mat4::rotationZ(deg2rad(-62.0f)) *
                          Mat4::scaling(Vec3(0.20f * bulk, 0.16f * bulk, 0.20f * bulk)),
                sTint * 1.1f, 1);
        }
    }

    // One antenna.
    for (int i = 0; i < 1; ++i) {
        const float x = -0.30f * hw;
        add(&L.cyl4, Mat4::translation(Vec3(x, ht + 0.34f * bulk, -hl * 0.66f)) *
                     Mat4::rotationZ(deg2rad(i ? 6.0f : -8.0f)) *
                     Mat4::scaling(Vec3(0.022f * bulk, 1.05f * bulk, 0.022f * bulk)),
            pal.joint, 1);
    }

    // ======================================================= TURRET =========
    section = Slot::Chassis;

    // Family superstructure, fixed to the hull (frame 0), built before the
    // rotating parts so the silhouette differences are structural.
    if (fam == HullFamily::Casemate) {
        // The armoured bow: a tall fixed casemate over the front half of the
        // hull, all glacis. The "turret" that follows is only a sight head.
        add(&L.wedge, Mat4::translation(Vec3(0.0f, ht + 0.42f * bulk, hl * 0.28f)) *
                      Mat4::scaling(Vec3(hw * 0.80f, 0.52f * bulk, hl * 0.62f)),
            pal.hull, 0);
        chamferSlab(Vec3(0.0f, ht + 0.58f * bulk, -hl * 0.28f),
                    Vec3(hw * 0.66f, 0.30f * bulk, hl * 0.36f), pal.deck, 0);
        // Heavy applique plates on the bow cheeks.
        for (int sgn = -1; sgn <= 1; sgn += 2)
            add(&L.wedge, Mat4::translation(Vec3(sgn * hw * 0.62f, ht + 0.25f * bulk,
                                                 hl * 0.40f)) *
                          Mat4::rotationY(sgn * 0.35f) *
                          Mat4::scaling(Vec3(hw * 0.22f, 0.30f * bulk, hl * 0.30f)),
                pal.deck, 1);
    } else if (fam == HullFamily::Artillery) {
        // The open cradle: rails running aft, and recoil spades at the rear
        // corners that read as "this thing digs in to shoot".
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            slab(Vec3(sgn * hw * 0.42f, ht + 0.40f * bulk, -hl * 0.10f),
                 Vec3(0.09f * bulk, 0.26f * bulk, hl * 0.66f), pal.dark, 0);
            add(&L.wedge, Mat4::translation(Vec3(sgn * hw * 0.78f, -ht * 0.4f,
                                                 -hl * 0.92f)) *
                          Mat4::rotationY(PI) *
                          Mat4::scaling(Vec3(0.16f * bulk, 0.42f * bulk, 0.34f * bulk)),
                pal.deck, 1);
        }
        // The crew blast shield: a raked plate forward of the mount, with a
        // sighting slit. It is what makes the platform read as ARTILLERY
        // rather than a flatbed with a gun dropped on it.
        add(&L.wedge, Mat4::translation(Vec3(0.0f, ht + 0.55f * bulk, hl * 0.34f)) *
                      Mat4::scaling(Vec3(hw * 0.55f, 0.42f * bulk, 0.16f * bulk)),
            pal.deck, 0);
        slab(Vec3(0.0f, ht + 0.62f * bulk, hl * 0.37f),
             Vec3(hw * 0.30f, 0.035f * bulk, 0.03f * bulk), pal.dark, 1);
    }

    // Frame 1: everything from here rotates with the turret.
    const float tr = 0.52f * bulk * (fam == HullFamily::Casemate ? 0.55f : 1.0f);
    add(&L.cyl12, Mat4::translation(Vec3(0.0f, -0.14f * bulk, 0.0f)) *
                  Mat4::scaling(Vec3(tr * 1.05f, 0.10f * bulk, tr * 1.05f)), pal.dark, 0, 1);
    add(&L.cyl8, Mat4::translation(Vec3(0.0f, -0.06f * bulk, 0.0f)) *
                 Mat4::scaling(Vec3(tr, 0.16f * bulk, tr)), pal.hull, 0, 1);
    // The turret is deliberately tall. In the reference machines the fighting
    // compartment is a block that sits proud of the hull, not a lid flush with
    // it - from the front three-quarter view that block is most of what reads
    // as "tank" rather than "table with legs".
    {
        // The fighting compartment. Turreted hulls carry the full block; a
        // casemate mounts only a sight head (the guns live in the bow); a
        // low-profile hull runs a flattened, wider one.
        const float bh = (fam == HullFamily::Casemate) ? 0.14f
                       : (fam == HullFamily::LowProfile) ? 0.20f : 0.34f;
        const float bw = (fam == HullFamily::LowProfile) ? 0.62f : 0.50f;
        chamferSlab(Vec3(0.0f, (fam == HullFamily::Casemate ? 0.10f : 0.30f) * bulk,
                         -0.02f * bulk),
                    Vec3(bw * bulk, bh * bulk, 0.58f * bulk), pal.deck, 0, 1);
    }
    // Sloped cheeks either side, so the block is not a plain brick.
    for (int i = 0; i < 2; ++i) {
        const float s = i ? 1.0f : -1.0f;
        add(&L.wedge, Mat4::translation(Vec3(s * 0.50f * bulk, 0.06f * bulk, 0.10f * bulk)) *
                      Mat4::rotationZ(deg2rad(s * 90.0f)) *
                      Mat4::scaling(Vec3(0.26f * bulk, 0.16f * bulk, 0.44f * bulk)),
            pal.hull, 1, 1);
    }
    // Mantlet: a wide armoured collar with cheek blocks either side of the
    // gun slot - the face of the turret, so it gets the most metal.
    add(&L.wedge, Mat4::translation(Vec3(0.0f, 0.04f * bulk, 0.46f * bulk)) *
                  Mat4::scaling(Vec3(0.44f * bulk, 0.46f * bulk, 0.28f * bulk)),
        pal.hull, 0, 1);
    // Gunner's cupola: the tallest thing on a manned turret. A low-profile
    // hull runs an unmanned turret - no cupola, a sensor stub instead.
    if (fam != HullFamily::LowProfile) {
        add(&L.cyl8, Mat4::translation(Vec3(-0.06f * bulk, 0.62f * bulk, -0.14f * bulk)) *
                     Mat4::scaling(Vec3(0.24f * bulk, 0.22f * bulk, 0.24f * bulk)), pal.hull, 0, 1);
        add(&L.cyl12, Mat4::translation(Vec3(-0.06f * bulk, 0.83f * bulk, -0.14f * bulk)) *
                      Mat4::scaling(Vec3(0.26f * bulk, 0.05f * bulk, 0.26f * bulk)), pal.deck, 1, 1);
    } else {
        add(&L.cyl6, Mat4::translation(Vec3(-0.06f * bulk, 0.42f * bulk, -0.14f * bulk)) *
                     Mat4::scaling(Vec3(0.06f * bulk, 0.16f * bulk, 0.06f * bulk)),
            pal.dark, 1, 1);
        add(&L.sphere, Mat4::translation(Vec3(-0.06f * bulk, 0.58f * bulk, -0.14f * bulk)) *
                       Mat4::scaling(Vec3(0.07f * bulk)), pal.glow, 1, 1, 0.7f);
    }
    // Accent band across the turret roof.
    slab(Vec3(0.0f, 0.645f * bulk, 0.22f * bulk),
         Vec3(0.34f * bulk, 0.018f * bulk, 0.10f * bulk), pal.accent, 1, 1);
    // Stowage bin on the bustle.
    chamferSlab(Vec3(0.0f, 0.24f * bulk, -0.66f * bulk),
                Vec3(0.38f * bulk, 0.20f * bulk, 0.15f * bulk), pal.dark, 1, 1);

    // ======================================================= WEAPONS ========
    section = Slot::Weapon;
    for (size_t wi = 0; wi < weapons_.size(); ++wi) {
        const MountedWeapon& mw = weapons_[wi];
        if (!mw.part) continue;
        const uint8_t frame = static_cast<uint8_t>(2 + wi);
        const Vec3 wt = mw.part->tint;
        const float sc = bulk * (mw.part->size == SizeClass::Heavy ? 1.25f :
                                 mw.part->size == SizeClass::Medium ? 1.0f : 0.8f);
        const int style = mw.part->style;

        // Common mount: a fat yoke, a trunnion block and a cradle plate under
        // the receiver, at LOD 0 - a gun visibly BOLTED on from any range,
        // never floating beside the turret.
        add(&L.cyl6, Mat4::rotationZ(deg2rad(90.0f)) *
                     Mat4::scaling(Vec3(0.12f * sc, 0.30f * sc, 0.12f * sc)), pal.joint, 0, frame);
        chamferSlab(Vec3(0.0f, 0.0f, -0.12f * sc), Vec3(0.19f * sc, 0.17f * sc, 0.20f * sc),
                    pal.dark, 0, frame);
        chamferSlab(Vec3(0.0f, -0.10f * sc, 0.10f * sc),
                    Vec3(0.15f * sc, 0.05f * sc, 0.30f * sc), pal.hull, 1, frame);

        switch (style) {
            case 0:   // PD-9 autocannon: single barrel, boxy receiver
            case 1: { // Twin-14 chaingun: two barrels and a feed
                const int barrels = (style == 1) ? 2 : 1;
                chamferSlab(Vec3(0.0f, 0.0f, 0.22f * sc), Vec3(0.15f * sc, 0.13f * sc, 0.26f * sc),
                            wt, 0, frame);
                for (int b = 0; b < barrels; ++b) {
                    const float x = (barrels == 1) ? 0.0f : (b ? 0.09f : -0.09f) * sc;
                    add(&L.cyl6, Mat4::translation(Vec3(x, 0.0f, 0.44f * sc)) *
                                 Mat4::rotationX(deg2rad(90.0f)) *
                                 Mat4::scaling(Vec3(0.048f * sc, 0.62f * sc, 0.048f * sc)),
                        pal.barrel, 0, frame);
                    add(&L.cyl6, Mat4::translation(Vec3(x, 0.0f, 1.02f * sc)) *
                                 Mat4::rotationX(deg2rad(90.0f)) *
                                 Mat4::scaling(Vec3(0.065f * sc, 0.10f * sc, 0.065f * sc)),
                        pal.dark, 1, frame);
                }
                // Ammunition feed and ejection port.
                chamferSlab(Vec3(-0.17f * sc, -0.04f * sc, 0.12f * sc),
                            Vec3(0.07f * sc, 0.09f * sc, 0.18f * sc), pal.dark, 2, frame);
                for (int i = 0; i < 3; ++i)
                    slab(Vec3(0.0f, 0.14f * sc, 0.05f * sc + i * 0.10f * sc),
                         Vec3(0.11f * sc, 0.02f * sc, 0.03f * sc), pal.dark, 2, frame);
                break;
            }
            case 2: { // Needle gauss: long thin rail with coil rings
                chamferSlab(Vec3(0.0f, 0.0f, 0.18f * sc), Vec3(0.12f * sc, 0.11f * sc, 0.22f * sc),
                            wt, 0, frame);
                add(&L.cyl6, Mat4::translation(Vec3(0.0f, 0.0f, 0.42f * sc)) *
                             Mat4::rotationX(deg2rad(90.0f)) *
                             Mat4::scaling(Vec3(0.035f * sc, 0.95f * sc, 0.035f * sc)),
                    pal.barrel, 0, frame);
                for (int i = 0; i < 5; ++i)
                    add(&L.cyl8, Mat4::translation(Vec3(0.0f, 0.0f, 0.52f * sc + i * 0.16f * sc)) *
                                 Mat4::rotationX(deg2rad(90.0f)) *
                                 Mat4::scaling(Vec3(0.075f * sc, 0.028f * sc, 0.075f * sc)),
                        wt * 1.25f, 2, frame);
                break;
            }
            case 3: { // Lance railgun: heavy twin rails and a capacitor block
                chamferSlab(Vec3(0.0f, 0.02f * sc, 0.10f * sc),
                            Vec3(0.19f * sc, 0.16f * sc, 0.30f * sc), wt, 0, frame);
                for (int r = -1; r <= 1; r += 2)
                    slab(Vec3(r * 0.09f * sc, 0.02f * sc, 0.72f * sc),
                         Vec3(0.035f * sc, 0.045f * sc, 0.62f * sc), pal.barrel, 0, frame);
                for (int i = 0; i < 4; ++i)
                    slab(Vec3(0.0f, 0.02f * sc, 0.30f * sc + i * 0.28f * sc),
                         Vec3(0.14f * sc, 0.09f * sc, 0.05f * sc), wt * 1.2f, 1, frame);
                chamferSlab(Vec3(0.0f, 0.20f * sc, -0.18f * sc),
                            Vec3(0.15f * sc, 0.10f * sc, 0.16f * sc), pal.dark, 2, frame);
                break;
            }
            case 4: { // Solaris plasma: bulbous emitter with a glowing throat
                chamferSlab(Vec3(0.0f, 0.0f, 0.14f * sc), Vec3(0.18f * sc, 0.17f * sc, 0.24f * sc),
                            wt, 0, frame);
                add(&L.cyl8, Mat4::translation(Vec3(0.0f, 0.0f, 0.40f * sc)) *
                             Mat4::rotationX(deg2rad(90.0f)) *
                             Mat4::scaling(Vec3(0.14f * sc, 0.40f * sc, 0.14f * sc)),
                    pal.barrel, 0, frame);
                add(&L.cyl8, Mat4::translation(Vec3(0.0f, 0.0f, 0.80f * sc)) *
                             Mat4::rotationX(deg2rad(90.0f)) *
                             Mat4::scaling(Vec3(0.19f * sc, 0.12f * sc, 0.19f * sc)),
                    wt * 1.3f, 1, frame);
                add(&L.sphere, Mat4::translation(Vec3(0.0f, 0.0f, 0.88f * sc)) *
                               Mat4::scaling(Vec3(0.10f * sc)), Vec3(1.0f, 0.62f, 0.28f),
                    1, frame, 0.85f);
                for (int i = 0; i < 3; ++i)
                    add(&L.cyl6, Mat4::translation(Vec3(0.0f, 0.16f * sc, 0.10f * sc + i * 0.14f * sc)) *
                                 Mat4::scaling(Vec3(0.05f * sc, 0.09f * sc, 0.05f * sc)),
                        pal.dark, 2, frame);
                break;
            }
            case 5: { // Scatter flak: short wide muzzle and a drum
                chamferSlab(Vec3(0.0f, 0.0f, 0.16f * sc), Vec3(0.17f * sc, 0.15f * sc, 0.22f * sc),
                            wt, 0, frame);
                add(&L.cyl8, Mat4::translation(Vec3(0.0f, 0.0f, 0.36f * sc)) *
                             Mat4::rotationX(deg2rad(90.0f)) *
                             Mat4::scaling(Vec3(0.09f * sc, 0.34f * sc, 0.09f * sc)),
                    pal.barrel, 0, frame);
                add(&L.cone6, Mat4::translation(Vec3(0.0f, 0.0f, 0.70f * sc)) *
                              Mat4::rotationX(deg2rad(-90.0f)) *
                              Mat4::scaling(Vec3(0.19f * sc, 0.22f * sc, 0.19f * sc)),
                    wt * 1.2f, 0, frame);
                add(&L.cyl12, Mat4::translation(Vec3(-0.20f * sc, 0.0f, 0.10f * sc)) *
                              Mat4::rotationZ(deg2rad(90.0f)) *
                              Mat4::scaling(Vec3(0.15f * sc, 0.08f * sc, 0.15f * sc)),
                    pal.dark, 1, frame);
                break;
            }
            case 6: { // Thumper: stubby high-angle tube
                chamferSlab(Vec3(0.0f, 0.0f, 0.12f * sc), Vec3(0.16f * sc, 0.15f * sc, 0.20f * sc),
                            wt, 0, frame);
                add(&L.cyl8, Mat4::translation(Vec3(0.0f, 0.06f * sc, 0.34f * sc)) *
                             Mat4::rotationX(deg2rad(78.0f)) *
                             Mat4::scaling(Vec3(0.11f * sc, 0.46f * sc, 0.11f * sc)),
                    pal.barrel, 0, frame);
                for (int i = 0; i < 4; ++i)
                    add(&L.sphere, Mat4::translation(Vec3(-0.20f * sc, -0.06f * sc,
                                                          -0.02f * sc + i * 0.09f * sc)) *
                                   Mat4::scaling(Vec3(0.045f * sc)), pal.accent, 2, frame);
                break;
            }
            case 7: { // Swarm rack: a box of missile cells
                chamferSlab(Vec3(0.0f, 0.06f * sc, 0.0f), Vec3(0.26f * sc, 0.22f * sc, 0.26f * sc),
                            wt, 0, frame);
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 3; ++c) {
                        add(&L.cyl6, Mat4::translation(Vec3((c - 1) * 0.15f * sc,
                                                            0.06f * sc + (r - 1) * 0.14f * sc,
                                                            0.28f * sc)) *
                                     Mat4::rotationX(deg2rad(90.0f)) *
                                     Mat4::scaling(Vec3(0.055f * sc, 0.10f * sc, 0.055f * sc)),
                            pal.dark, 1, frame);
                    }
                }
                slab(Vec3(0.0f, 0.30f * sc, 0.0f), Vec3(0.27f * sc, 0.03f * sc, 0.26f * sc),
                     pal.accent, 2, frame);
                break;
            }
            case 8: { // Hammerfall: one enormous barrel with a muzzle brake
                chamferSlab(Vec3(0.0f, 0.02f * sc, 0.08f * sc),
                            Vec3(0.22f * sc, 0.20f * sc, 0.32f * sc), wt, 0, frame);
                add(&L.cyl8, Mat4::translation(Vec3(0.0f, 0.02f * sc, 0.40f * sc)) *
                             Mat4::rotationX(deg2rad(90.0f)) *
                             Mat4::scaling(Vec3(0.10f * sc, 1.05f * sc, 0.10f * sc)),
                    pal.barrel, 0, frame);
                for (int i = 0; i < 3; ++i)
                    add(&L.cyl8, Mat4::translation(Vec3(0.0f, 0.02f * sc, 1.16f * sc + i * 0.12f * sc)) *
                                 Mat4::rotationX(deg2rad(90.0f)) *
                                 Mat4::scaling(Vec3(0.15f * sc, 0.035f * sc, 0.15f * sc)),
                        pal.dark, 1, frame);
                chamferSlab(Vec3(0.0f, 0.24f * sc, -0.24f * sc),
                            Vec3(0.17f * sc, 0.10f * sc, 0.20f * sc), pal.dark, 2, frame);
                break;
            }
            default: { // Obelisk mortar: near-vertical tube on a heavy base
                chamferSlab(Vec3(0.0f, 0.02f * sc, 0.0f), Vec3(0.28f * sc, 0.16f * sc, 0.28f * sc),
                            wt, 0, frame);
                add(&L.cyl8, Mat4::translation(Vec3(0.0f, 0.14f * sc, 0.10f * sc)) *
                             Mat4::rotationX(deg2rad(40.0f)) *
                             Mat4::scaling(Vec3(0.15f * sc, 0.95f * sc, 0.15f * sc)),
                    pal.barrel, 0, frame);
                for (int i = 0; i < 3; ++i)
                    add(&L.cyl8, Mat4::translation(Vec3(0.0f, 0.14f * sc + i * 0.22f * sc,
                                                        0.10f * sc + i * 0.19f * sc)) *
                                 Mat4::rotationX(deg2rad(40.0f)) *
                                 Mat4::scaling(Vec3(0.19f * sc, 0.03f * sc, 0.19f * sc)),
                        wt * 1.2f, 2, frame);
                for (int s = -1; s <= 1; s += 2)
                    slab(Vec3(s * 0.26f * sc, -0.10f * sc, -0.14f * sc),
                         Vec3(0.05f * sc, 0.12f * sc, 0.22f * sc), pal.dark, 2, frame);
                break;
            }
        }
    }
}


// ---------------------------------------------------------------- drawing

void Mech::submit(Rasterizer& raster, const Vec3& viewPos) const {
    const MechMeshLibrary& L = MechMeshLibrary::instance();

    // Level of detail. A mech across the arena is a couple of dozen boxes; the
    // one in your face is the full assembly. Without this, six fully detailed
    // machines would cost more triangles than the entire landscape.
    const float dist = length(pos_ - viewPos);
    const int lodBudget = (dist < 42.0f) ? 2 : (dist < 105.0f) ? 1 : 0;

    DrawItem item;
    // Machines get by far the strongest rim in the scene. Against a landscape
    // of concrete at almost the same value, a mech's grey hull dissolves into
    // the buildings behind it once the image is quantised to a dozen glyphs;
    // the grazing-angle brightening is what draws the outline back in.
    item.rim = 1.7f;

    // Damage tint: a hit flashes the whole hull, and a wrecked mech goes sooty.
    const float flash = damageFlash_;
    const bool dead = (state_ == MechState::Destroyed);
    auto shade = [&](const Vec3& c) {
        Vec3 out = dead ? c * 0.42f : c;
        if (flash > 0.01f) out = lerp(out, Vec3(1.0f, 0.5f, 0.35f), flash * 0.55f);
        return out;
    };

    // ---- hull, turret and weapons ---------------------------------------
    for (const VisualPart& vp : visual_) {
        if (vp.lod > lodBudget) continue;
        Mat4 frame;
        if (vp.frame == 0) {
            frame = bodyXform_;
        } else if (vp.frame == 1) {
            frame = turretXform_;
        } else {
            const size_t wi = static_cast<size_t>(vp.frame - 2);
            if (wi >= weapons_.size()) continue;
            const MountedWeapon& mw = weapons_[wi];
            // Recoil kicks the weapon back along its own axis, and a gun that
            // has to spool ROLLS as it winds up - so a rotary visibly comes
            // to life a beat before it starts shooting and keeps turning for
            // a beat after you let go. Without this the spin-up is a delay
            // with no cause on screen, which reads as the gun being broken.
            const float kick = -mw.spin * 0.16f;
            const bool rotary = mw.part && mw.part->weapon.spinUp > 0.0f;
            frame = (mw.mount.onTurret ? turretXform_ : bodyXform_) *
                    Mat4::translation(mw.mount.offset + Vec3(0.0f, 0.0f, kick));
            if (rotary && mw.spool > 0.01f)
                frame = frame * Mat4::rotationZ(spinPhase_ * mw.spool);
        }
        item.mesh = vp.mesh;
        item.model = frame * vp.local;
        item.tint = shade(vp.tint);
        // A small emissive floor on every live part: faces turned away from
        // the key light stay legible dark metal instead of falling to pure
        // black, which read as HOLES in the hull. The legs already do this.
        item.emissive = dead ? 0.0f : std::max(vp.emissive, 0.12f);
        raster.submit(item);
    }
    item.emissive = 0.0f;

    // ---- muzzle flashes --------------------------------------------------
    if (!dead) {
        for (size_t i = 0; i < weapons_.size(); ++i) {
            const MountedWeapon& mw = weapons_[i];
            if (mw.spin < 0.15f || !mw.part) continue;
            item.mesh = &L.sphere;
            item.model = Mat4::translation(muzzlePosition(static_cast<int>(i))) *
                         Mat4::scaling(Vec3(0.10f + 0.30f * mw.spin));
            item.tint = mw.part->weapon.tracerColor;
            item.emissive = 1.0f;
            raster.submit(item);
        }
        item.emissive = 0.0f;
    }

    // ---- legs ------------------------------------------------------------
    // Rebuilt every frame from the solved chain. Each limb is a shoulder
    // housing, an armoured coxa, a femur with side plates and a hydraulic ram,
    // a knee, an armoured tibia, an ankle and a clawed foot pad - about twenty
    // pieces, times six.
    const float s = bulk_;
    const Vec3 joint = shade(jointTint_);
    const Vec3 dark = shade(darkTint_);

    for (const Leg& leg : legs_) {
        const Vec3 hip = leg.hipWorld;
        const Vec3 coxa = leg.coxaEnd;
        const Vec3 knee = leg.knee;
        const Vec3 foot = leg.footSolved;
        const float g = legGirth_;

        const Vec3 femurDir = normalize(knee - coxa + Vec3(1e-5f, 0.0f, 0.0f));
        const Vec3 tibiaDir = normalize(foot - knee + Vec3(1e-5f, 0.0f, 0.0f));


        Vec3 outb = hip - pos_;
        outb -= up_ * dot(outb, up_);
        outb = (lengthSq(outb) > 1e-5f) ? normalize(outb) : Vec3(1.0f, 0.0f, 0.0f);
        const Vec3 plate = shade(platePal_);
        const Vec3 plateLit = shade(platePal_ * 1.12f);

        auto draw1 = [&](const Mesh* mesh, const Mat4& model, const Vec3& tint,
                         float glow = 0.10f) {
            item.mesh = mesh;
            item.model = model;
            item.tint = tint;
            item.emissive = glow;
            raster.submit(item);
            item.emissive = 0.0f;
        };
        // A limb segment as ONE tapered armoured pod: fat at the upper
        // joint, necking down into the lower one - the sectioning every
        // reference leg shares, insect femur and armoured casing at once.
        auto podSeg = [&](const Vec3& a, const Vec3& b, float rx, float rz,
                          const Vec3& tint) {
            const Vec3 d2 = normalize(b - a + Vec3(1e-5f, 0.0f, 0.0f));
            const float len = length(b - a);
            draw1(&L.pod, Mat4::translation(a - d2 * (len * 0.06f)) * alignYTo(d2) *
                              Mat4::scaling(Vec3(rx, len * 1.10f, rz)),
                  tint);
        };
        // The tapered top plate: a thin frustum slab riding the upper face
        // of a pod - the "tapered armour plate" of the brief, distinct from
        // the pod it shields.
        auto topPlate = [&](const Vec3& a, const Vec3& b, float w, const Vec3& tint) {
            const Vec3 d2 = normalize(b - a + Vec3(1e-5f, 0.0f, 0.0f));
            const float len = length(b - a);
            const Vec3 n = normalize(outb + up_ * 0.85f);
            const Vec3 lift = n - d2 * dot(n, d2);
            const Vec3 nl = (lengthSq(lift) > 1e-6f) ? normalize(lift) : up_;
            Vec3 axisR = normalize(cross(d2, nl));
            draw1(&L.taper,
                  Mat4::translation(lerp(a, b, 0.06f) + nl * (w * 0.34f)) *
                      Mat4::basis(axisR, d2, cross(axisR, d2)) *
                      Mat4::scaling(Vec3(w, len * 0.92f, w * 0.22f)),
                  tint);
        };
        // A limb segment as a BLADE: a flat tapered knife of armour, thin
        // side-to-side and deep front-to-back - the CAD walker's leg, and
        // nothing like a pod.
        auto bladeSeg = [&](const Vec3& a, const Vec3& b, float w, const Vec3& tint) {
            const Vec3 d2 = normalize(b - a + Vec3(1e-5f, 0.0f, 0.0f));
            const float len = length(b - a);
            const Vec3 n = normalize(outb + up_ * 0.55f);
            const Vec3 lift = n - d2 * dot(n, d2);
            const Vec3 nl = (lengthSq(lift) > 1e-6f) ? normalize(lift) : up_;
            const Vec3 axisR = normalize(cross(d2, nl));
            draw1(&L.taper,
                  Mat4::translation(a - d2 * (len * 0.04f)) *
                      Mat4::basis(axisR, d2, cross(axisR, d2)) *
                      Mat4::scaling(Vec3(w * 0.38f, len * 1.08f, w * 1.55f)),
                  tint);
        };
        // Plate CONTOURS, one per leg language. The same job - armour riding
        // the outer face of a limb segment - drawn as four different shapes:
        // a tapered trapezoid, a rounded blister, a triangular ridge, a flat
        // rectangular slab. The contour is what tells the sets apart up close.
        auto plateFrame = [&](const Vec3& a, const Vec3& b, Vec3& d2, float& len,
                              Vec3& nl, Vec3& axisR) {
            d2 = normalize(b - a + Vec3(1e-5f, 0.0f, 0.0f));
            len = length(b - a);
            const Vec3 n = normalize(outb + up_ * 0.85f);
            const Vec3 lift = n - d2 * dot(n, d2);
            nl = (lengthSq(lift) > 1e-6f) ? normalize(lift) : up_;
            axisR = normalize(cross(d2, nl));
        };
        // Rounded shell blister: convex, lapped, the AMU shell-band look.
        auto shellPlate = [&](const Vec3& a, const Vec3& b, float w, const Vec3& tint) {
            Vec3 d2, nl, axisR; float len;
            plateFrame(a, b, d2, len, nl, axisR);
            draw1(&L.pod,
                  Mat4::translation(lerp(a, b, 0.10f) + nl * (w * 0.22f)) *
                      Mat4::basis(axisR, d2, cross(axisR, d2)) *
                      Mat4::scaling(Vec3(w, len * 0.86f, w * 0.42f)),
                  tint);
        };
        // Triangular ridge plate: a peaked spine facing outward.
        auto ridgePlate = [&](const Vec3& a, const Vec3& b, float w, const Vec3& tint) {
            Vec3 d2, nl, axisR; float len;
            plateFrame(a, b, d2, len, nl, axisR);
            draw1(&L.wedge,
                  Mat4::translation(lerp(a, b, 0.5f) + nl * (w * 0.18f)) *
                      Mat4::basis(axisR, nl, d2) *
                      Mat4::scaling(Vec3(w * 0.85f, w * 0.55f, len * 0.88f)),
                  tint);
        };
        // Flat rectangular slab, chamfered edges: the fortress plate.
        auto slabPlate = [&](const Vec3& a, const Vec3& b, float w, const Vec3& tint) {
            Vec3 d2, nl, axisR; float len;
            plateFrame(a, b, d2, len, nl, axisR);
            draw1(&L.chamfer,
                  Mat4::translation(lerp(a, b, 0.5f) + nl * (w * 0.32f)) *
                      Mat4::basis(axisR, d2, cross(axisR, d2)) *
                      Mat4::scaling(Vec3(w, len * 0.46f, w * 0.16f)),
                  tint);
        };
        // The knee flap: one plate lapped over the joint from above.
        auto kneeFlap = [&](float w) {
            const Vec3 bis = normalize(femurDir + tibiaDir + Vec3(1e-5f, 0.0f, 0.0f));
            const Vec3 n = normalize(outb + up_ * 0.9f);
            Vec3 axisR = normalize(cross(bis, n) + Vec3(1e-5f, 0.0f, 0.0f));
            draw1(&L.chamfer,
                  Mat4::translation(knee + n * (w * 0.5f)) *
                      Mat4::basis(axisR, bis, cross(axisR, bis)) *
                      Mat4::scaling(Vec3(w * 0.9f, w * 1.1f, w * 0.30f)),
                  plateLit);
        };

        const float r0 = 0.20f * s * g;

        // The ANCHOR. How a limb meets the hull is per body plan - a socket
        // on a pod's equator, a shoulder boom off a torso, a drop strut under
        // a long hull, a faired rim notch on a crab - so the plans differ at
        // the mounting, not just in the hull behind it.
        switch (hullStyle_) {
            case 0:   // pod: a raised socket collar around the ball
                draw1(&L.cyl8, Mat4::translation(hip) * alignYTo(outb) *
                          Mat4::scaling(Vec3(0.26f * s * g, 0.11f * s, 0.26f * s * g)),
                      plateLit);
                break;
            case 3: { // torso: a shoulder boom reaching out from the chest
                const Vec3 inner = hip - outb * (0.66f * s) + up_ * (0.12f * s);
                podSeg(inner, hip, r0 * 1.55f, r0 * 1.40f, plate);
                break;
            }
            case 4: { // long hull: a vertical drop strut from the keel line
                const Vec3 keel = hip + up_ * (0.60f * s);
                podSeg(keel, hip, r0 * 1.30f, r0 * 1.30f, dark);
                break;
            }
            case 6: { // crab: a flat fairing wedge lapped over the rim socket
                const Vec3 axisT = normalize(cross(up_, outb) + Vec3(1e-5f, 0.0f, 0.0f));
                const Vec3 slope = normalize(outb + up_ * 0.35f);
                draw1(&L.chamfer,
                      Mat4::translation(hip + up_ * (0.20f * s) + outb * (0.06f * s)) *
                          Mat4::basis(axisT, slope, cross(axisT, slope)) *
                          Mat4::scaling(Vec3(0.34f * s * g, 0.46f * s * g, 0.10f * s)),
                      plateLit);
                break;
            }
            default: break;
        }

        // Hip: joint ball under a lapped cover band.
        draw1(&L.sphere, Mat4::translation(hip) * Mat4::scaling(Vec3(0.30f * s * g)),
              joint, 0.0f);
        podSeg(hip, coxa, r0 * 1.25f, r0 * 1.15f, plate);

        switch (legStyle_) {
            case 1: {
                // SCOUT/HARRIER - BLADE legs: flat tapered knives of armour,
                // thin edge-on, deep in profile, a small exposed knee ball.
                // The fastest-looking silhouette in the catalogue.
                bladeSeg(coxa, knee, r0 * 1.9f, plate);
                bladeSeg(knee, foot, r0 * 1.45f, plate);
                draw1(&L.sphere, Mat4::translation(knee) *
                          Mat4::scaling(Vec3(0.17f * s * g)), joint, 0.0f);
                break;
            }
            case 2: {
                // ANVIL - the heavy pods under ROUNDED shell blisters, lapped
                // like beetle bands, and a skirt swallowing the ankle.
                podSeg(coxa, knee, r0 * 2.0f, r0 * 1.8f, plate);
                shellPlate(coxa, lerp(coxa, knee, 0.60f), r0 * 1.8f, plateLit);
                shellPlate(lerp(coxa, knee, 0.45f), knee, r0 * 1.6f, plate);
                podSeg(knee, foot, r0 * 1.6f, r0 * 1.45f, plate);
                shellPlate(knee, foot, r0 * 1.35f, plateLit);
                kneeFlap(0.30f * s * g);
                draw1(&L.taper, Mat4::translation(foot + leg.footNormal * (0.5f * s * g)) *
                          alignYTo(leg.footNormal * -1.0f) *
                          Mat4::scaling(Vec3(0.30f * s * g, 0.5f * s * g, 0.27f * s * g)),
                      plate);
                break;
            }
            case 3: {
                // GRASSHOPPER - slim pods under a peaked RIDGE plate, the
                // coil spring riding the tibia in the open, spike feet.
                podSeg(coxa, knee, r0 * 1.15f, r0 * 1.0f, plate);
                ridgePlate(coxa, knee, r0 * 1.25f, plateLit);
                podSeg(knee, foot, r0 * 0.75f, r0 * 0.7f, dark);
                for (int c2 = 0; c2 < 6; ++c2)
                    draw1(&L.cyl8,
                          Mat4::translation(lerp(knee, foot, 0.10f + c2 * 0.12f)) *
                              alignYTo(tibiaDir) *
                              Mat4::scaling(Vec3(r0 * 1.05f, 0.028f * s, r0 * 1.05f)),
                          shade(accentTint_) * 0.9f, 0.0f);
                kneeFlap(0.20f * s * g);
                break;
            }
            case 4: {
                // TITAN - fattest pods under flat RECTANGULAR slabs, lapped
                // in pairs down each segment: the walking bunker.
                podSeg(coxa, knee, r0 * 1.85f, r0 * 1.65f, plate);
                slabPlate(coxa, lerp(coxa, knee, 0.56f), r0 * 1.6f, plateLit);
                slabPlate(lerp(coxa, knee, 0.44f), knee, r0 * 1.6f, plate);
                podSeg(knee, foot, r0 * 1.45f, r0 * 1.3f, plate);
                slabPlate(knee, lerp(knee, foot, 0.7f), r0 * 1.3f, plateLit);
                kneeFlap(0.32f * s * g);
                break;
            }
            default: {
                // STRIDER - the line pod with a single tapered top plate on
                // each segment. Clean, military, tapering.
                podSeg(coxa, knee, r0 * 1.55f, r0 * 1.35f, plate);
                topPlate(coxa, knee, r0 * 1.35f, plateLit);
                podSeg(knee, foot, r0 * 1.20f, r0 * 1.05f, plate);
                kneeFlap(0.26f * s * g);
                break;
            }
        }

        // Fitted armour spills onto the limbs: lapped scales down the femur
        // (and, heavy enough, a shin guard) in the armour part's own tint -
        // buy plate, and the LEGS visibly wear it too.
        if (armorLegT_ > 0.01f && lodBudget >= 1) {
            const Vec3 aTint = shade(armorTint_ * 1.04f);
            const int scales = 1 + static_cast<int>(armorLegT_ * 2.01f);
            for (int k = 0; k < scales; ++k) {
                const float t0 = 0.04f + 0.90f * k / scales;
                const float t1 = std::min(t0 + 1.12f * 0.90f / scales, 0.99f);
                topPlate(lerp(coxa, knee, t0), lerp(coxa, knee, t1),
                         r0 * (1.9f + 0.5f * armorLegT_), aTint);
            }
            if (armorLegT_ > 0.55f)
                topPlate(lerp(knee, foot, 0.10f), lerp(knee, foot, 0.78f),
                         r0 * 1.5f, aTint);
        }

        // Hip cover band, lapped over the pod root.
        {
            const Vec3 d2 = normalize(coxa - hip + Vec3(1e-5f, 0.0f, 0.0f));
            const Vec3 n = normalize(outb + up_ * 0.8f);
            Vec3 axisR = normalize(cross(d2, n) + Vec3(1e-5f, 0.0f, 0.0f));
            draw1(&L.chamfer,
                  Mat4::translation(lerp(hip, coxa, 0.4f) + n * (0.20f * s * g)) *
                      Mat4::basis(axisR, d2, cross(axisR, d2)) *
                      Mat4::scaling(Vec3(0.28f * s * g, 0.16f * s * g, 0.09f * s * g)),
                  plateLit);
        }

        // Feet.
        const Vec3 n = leg.footNormal;
        Vec3 fwdG = tibiaDir - n * dot(tibiaDir, n);
        fwdG = (lengthSq(fwdG) > 1e-5f) ? normalize(fwdG) : outb;
        const Vec3 sideG = cross(n, fwdG);
        if (legStyle_ == 1 || legStyle_ == 3) {
            for (int c2 = 0; c2 < 3; ++c2) {
                const Vec3 dirs[3] = {normalize(fwdG + sideG * 0.7f),
                                      normalize(fwdG - sideG * 0.7f),
                                      normalize(fwdG * -0.9f)};
                const Vec3 tip = foot + dirs[c2] * (0.40f * s * g) - n * (0.10f * s);
                draw1(&L.cone6, segmentTransform(lerp(knee, foot, 0.93f), tip, 0.05f * s),
                      dark, 0.0f);
            }
        } else if (legStyle_ != 2) {
            draw1(&L.chamfer, Mat4::translation(foot + n * (0.06f * s)) * alignYTo(n) *
                      Mat4::scaling(Vec3(0.24f * s * g, 0.09f * s, 0.28f * s * g)),
                  plate * 0.92f, 0.06f);
            if (lodBudget >= 1)
                for (int c2 = -1; c2 <= 1; c2 += 2) {
                    const Vec3 dirT = normalize(fwdG + sideG * (0.5f * c2));
                    draw1(&L.wedge,
                          Mat4::translation(foot + dirT * (0.22f * s * g)) *
                              Mat4::basis(cross(n, dirT), dirT, n) *
                              Mat4::scaling(Vec3(0.08f * s * g, 0.15f * s * g, 0.055f * s)),
                          plate, 0.06f);
                }
        }
    }
}

// Draws only what one part contributes. Legs are a special case: they are not
// in the visual list at all, they are rebuilt every frame from the solved
// chain, so isolating them means drawing the limbs and nothing else.
void Mech::submitSlotOnly(Rasterizer& raster, Slot slot, int mount) const {
    const MechMeshLibrary& L = MechMeshLibrary::instance();

    DrawItem item;
    item.rim = 0.85f;

    if (slot != Slot::Legs) {
        for (const VisualPart& vp : visual_) {
            if (vp.origin != slot) continue;
            if (slot == Slot::Weapon && mount >= 0 &&
                vp.frame != static_cast<uint8_t>(2 + mount)) continue;

            Mat4 frame;
            if (vp.frame == 0) {
                frame = bodyXform_;
            } else if (vp.frame == 1) {
                frame = turretXform_;
            } else {
                const size_t wi = static_cast<size_t>(vp.frame - 2);
                if (wi >= weapons_.size()) continue;
                const MountedWeapon& mw = weapons_[wi];
                frame = (mw.mount.onTurret ? turretXform_ : bodyXform_) *
                        Mat4::translation(mw.mount.offset);
            }
            item.mesh = vp.mesh;
            item.model = frame * vp.local;
            item.tint = vp.tint;
            item.emissive = vp.emissive;
            raster.submit(item);
        }
        return;
    }

    // Legs on their own: one limb chain, drawn simply. The inspector wants a
    // readable shape, not the full twenty-piece assembly.
    const float s = bulk_;
    item.emissive = 0.0f;
    for (const Leg& leg : legs_) {
        const Vec3 hip = leg.hipWorld;
        const Vec3 coxa = leg.coxaEnd;
        const Vec3 knee = leg.knee;
        const Vec3 foot = leg.footSolved;

        auto segment = [&](const Vec3& a, const Vec3& b, float radius, const Vec3& tint) {
            const Vec3 d = b - a;
            const float len = length(d);
            if (len < 1e-4f) return;
            const Vec3 dir = d / len;
            Vec3 up(0.0f, 1.0f, 0.0f);
            if (std::fabs(dot(dir, up)) > 0.98f) up = Vec3(1.0f, 0.0f, 0.0f);
            const Vec3 right = normalize(cross(up, dir));
            const Vec3 realUp = cross(dir, right);
            item.mesh = &L.cyl8;
            // The unit cylinder runs along +Y with its base at the origin, so
            // the basis puts its axis on the segment direction.
            item.model = Mat4::translation(a) *
                         Mat4::basis(right, dir, realUp) *
                         Mat4::scaling(Vec3(radius, len, radius));
            item.tint = tint;
            raster.submit(item);
        };

        segment(hip, coxa, 0.245f * s, jointTint_);
        segment(coxa, knee, 0.225f * s, limbTint_);
        segment(knee, foot, 0.175f * s, limbTint_);

        item.mesh = &L.sphere;
        item.model = Mat4::translation(knee) * Mat4::scaling(Vec3(0.24f * s));
        item.tint = jointTint_;
        raster.submit(item);

        item.mesh = &L.box;
        item.model = Mat4::translation(foot) *
                     Mat4::scaling(Vec3(0.30f * s, 0.10f * s, 0.30f * s));
        item.tint = darkTint_;
        raster.submit(item);
    }
}

} // namespace sb