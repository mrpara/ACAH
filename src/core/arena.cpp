#include "arena.h"

namespace sb {

namespace {

// Base lighting. Each arena tweaks this rather than restating all of it.
RenderSettings baseRender() {
    RenderSettings r;
    return r;
}

// A low sun is deliberate everywhere: at a steep angle every patch of ground
// returns nearly the same value and the character ramp has nothing to work with.
RenderSettings mood(const Vec3& sun, const Vec3& sunColor, const Vec3& fog,
                    const Vec3& skyHorizon, const Vec3& skyTop, float fogDensity,
                    const Vec3& sky, const Vec3& ground) {
    RenderSettings r = baseRender();
    r.lightDir = normalize(sun);
    r.lightColor = sunColor;
    r.fogColor = fog;
    r.skyHorizon = skyHorizon;
    r.skyTop = skyTop;
    r.fogDensity = fogDensity;
    r.skyAmbient = sky;
    r.groundAmbient = ground;
    return r;
}

} // namespace

const std::vector<ArenaDef>& arenaCatalog() {
    static const std::vector<ArenaDef> arenas = [] {
        std::vector<ArenaDef> a;

        // ------------------------------------------------ 1. RUINED DISTRICT
        {
            ArenaDef d;
            d.id = "ruined_district";
            d.name = "SECTOR 7";
            d.subtitle = "RUINED DISTRICT";
            d.terrain.hillAmp = 20.0f; d.terrain.midAmp = 4.4f;
            d.terrain.groundColor = Vec3(0.095f, 0.150f, 0.100f);
            d.terrain.rockColor = Vec3(0.200f, 0.198f, 0.190f);
            d.render = mood(Vec3(0.68f, 0.42f, 0.38f), Vec3(1.25f, 1.25f, 1.22f),
                            Vec3(0.018f, 0.036f, 0.031f), Vec3(0.028f, 0.062f, 0.058f),
                            Vec3(0.004f, 0.010f, 0.013f), 0.0068f,
                            Vec3(0.13f, 0.15f, 0.18f), Vec3(0.035f, 0.035f, 0.040f));
            d.layout = DistrictLayout::Cluster;
            d.ruinCount = 14; d.districtRadius = 95.0f;
            d.floorsMin = 2; d.floorsMax = 5;
            d.treeDensity = 0.35f; d.rockDensity = 0.5f;
            d.containerCount = 10; d.barrierCount = 14; d.rubbleCount = 12;
            d.verticality = "MEDIUM";
            a.push_back(d);
        }

        // ------------------------------------------------ 2. IRONWOOD FLATS
        {
            ArenaDef d;
            d.id = "forest";
            d.name = "IRONWOOD FLATS";
            d.subtitle = "OVERGROWN LOWLAND";
            d.terrain.hillAmp = 22.0f; d.terrain.midAmp = 5.2f; d.terrain.fineAmp = 1.6f;
            d.terrain.groundColor = Vec3(0.085f, 0.160f, 0.095f);
            d.render = mood(Vec3(0.62f, 0.44f, 0.40f), Vec3(1.20f, 1.24f, 1.18f),
                            Vec3(0.016f, 0.038f, 0.030f), Vec3(0.026f, 0.066f, 0.056f),
                            Vec3(0.004f, 0.011f, 0.012f), 0.0072f,
                            Vec3(0.12f, 0.16f, 0.17f), Vec3(0.030f, 0.036f, 0.032f));
            d.layout = DistrictLayout::Scattered;
            d.ruinCount = 5; d.districtRadius = 130.0f;
            d.floorsMin = 1; d.floorsMax = 3; d.ruinDamage = 0.75f;
            d.treeDensity = 1.05f; d.rockDensity = 0.7f;
            d.barrierCount = 4; d.rubbleCount = 6;
            d.verticality = "LOW";
            a.push_back(d);
        }

        // ------------------------------------------------ 3. FOUNDRY YARD
        {
            ArenaDef d;
            d.id = "industrial";
            d.name = "FOUNDRY YARD";
            d.subtitle = "INDUSTRIAL FLATS";
            d.terrain.hillAmp = 6.0f; d.terrain.midAmp = 1.8f; d.terrain.fineAmp = 0.6f;
            d.terrain.plainsFloor = 0.35f;
            d.terrain.flatRadius = 70.0f; d.terrain.flatFalloff = 60.0f;
            d.terrain.groundColor = Vec3(0.115f, 0.115f, 0.110f);
            d.terrain.rockColor = Vec3(0.170f, 0.165f, 0.155f);
            d.terrain.mottle = 0.20f;
            d.render = mood(Vec3(0.70f, 0.40f, 0.30f), Vec3(1.28f, 1.20f, 1.08f),
                            Vec3(0.030f, 0.030f, 0.026f), Vec3(0.060f, 0.058f, 0.048f),
                            Vec3(0.010f, 0.010f, 0.012f), 0.0060f,
                            Vec3(0.16f, 0.15f, 0.14f), Vec3(0.040f, 0.038f, 0.034f));
            d.layout = DistrictLayout::Grid;
            d.ruinCount = 8; d.districtRadius = 100.0f;
            d.floorsMin = 1; d.floorsMax = 3; d.ruinScale = 1.35f;
            d.concreteTint = Vec3(0.32f, 0.31f, 0.29f);
            d.containerCount = 34; d.pipeRackCount = 10; d.barrierCount = 16;
            d.mastCount = 4; d.rubbleCount = 6; d.rockDensity = 0.15f;
            d.verticality = "MEDIUM";
            a.push_back(d);
        }

        // ------------------------------------------------ 4. RED CHANNEL
        {
            ArenaDef d;
            d.id = "canyon";
            d.name = "RED CHANNEL";
            d.subtitle = "CANYON NETWORK";
            d.terrain.hillAmp = 16.0f; d.terrain.midAmp = 3.0f;
            d.terrain.canyonDepth = 26.0f; d.terrain.canyonFreq = 0.0048f;
            d.terrain.canyonWidth = 0.20f;
            d.terrain.groundColor = Vec3(0.150f, 0.100f, 0.070f);
            d.terrain.rockColor = Vec3(0.185f, 0.130f, 0.095f);
            d.terrain.slopeRockiness = 2.4f;
            d.render = mood(Vec3(0.74f, 0.36f, 0.30f), Vec3(1.32f, 1.14f, 0.96f),
                            Vec3(0.040f, 0.026f, 0.018f), Vec3(0.085f, 0.055f, 0.038f),
                            Vec3(0.012f, 0.008f, 0.010f), 0.0058f,
                            Vec3(0.17f, 0.14f, 0.13f), Vec3(0.045f, 0.034f, 0.028f));
            d.layout = DistrictLayout::Line;
            d.ruinCount = 6; d.districtRadius = 120.0f;
            d.floorsMin = 1; d.floorsMax = 3; d.ruinDamage = 0.8f;
            d.rockDensity = 2.2f; d.rubbleCount = 18; d.barrierCount = 6;
            d.verticality = "HIGH";
            a.push_back(d);
        }

        // ------------------------------------------------ 5. CORE DISTRICT
        {
            ArenaDef d;
            d.id = "downtown";
            d.name = "CORE DISTRICT";
            d.subtitle = "HIGH-RISE RUINS";
            d.terrain.hillAmp = 4.0f; d.terrain.midAmp = 1.2f; d.terrain.fineAmp = 0.4f;
            d.terrain.flatRadius = 105.0f; d.terrain.flatFalloff = 55.0f;
            d.terrain.groundColor = Vec3(0.105f, 0.105f, 0.108f);
            d.terrain.rockColor = Vec3(0.160f, 0.160f, 0.165f);
            d.render = mood(Vec3(0.60f, 0.46f, 0.50f), Vec3(1.18f, 1.18f, 1.24f),
                            Vec3(0.026f, 0.028f, 0.036f), Vec3(0.052f, 0.056f, 0.070f),
                            Vec3(0.008f, 0.010f, 0.016f), 0.0064f,
                            Vec3(0.15f, 0.16f, 0.20f), Vec3(0.036f, 0.036f, 0.044f));
            d.layout = DistrictLayout::Grid;
            d.ruinCount = 26; d.districtRadius = 105.0f;
            d.floorsMin = 4; d.floorsMax = 11; d.ruinScale = 1.1f; d.ruinDamage = 0.45f;
            d.concreteTint = Vec3(0.33f, 0.33f, 0.35f);
            d.barrierCount = 22; d.containerCount = 8; d.rubbleCount = 20; d.mastCount = 5;
            d.rockDensity = 0.0f;
            d.verticality = "EXTREME";
            a.push_back(d);
        }

        // ------------------------------------------------ 6. TERRACE FIELDS
        {
            ArenaDef d;
            d.id = "mesa";
            d.name = "TERRACE FIELDS";
            d.subtitle = "STEPPED MESA";
            d.terrain.hillAmp = 30.0f; d.terrain.midAmp = 4.0f;
            d.terrain.terrace = 7.0f; d.terrain.terraceBlend = 0.78f;
            d.terrain.groundColor = Vec3(0.130f, 0.115f, 0.085f);
            d.terrain.rockColor = Vec3(0.190f, 0.170f, 0.140f);
            d.render = mood(Vec3(0.72f, 0.40f, 0.34f), Vec3(1.30f, 1.20f, 1.04f),
                            Vec3(0.032f, 0.028f, 0.022f), Vec3(0.070f, 0.060f, 0.046f),
                            Vec3(0.010f, 0.010f, 0.012f), 0.0056f,
                            Vec3(0.16f, 0.15f, 0.15f), Vec3(0.042f, 0.038f, 0.032f));
            d.layout = DistrictLayout::Ring;
            d.ruinCount = 9; d.districtRadius = 125.0f;
            d.floorsMin = 1; d.floorsMax = 4;
            d.rockDensity = 1.4f; d.bunkerCount = 6; d.rubbleCount = 10;
            d.verticality = "HIGH";
            a.push_back(d);
        }

        // ------------------------------------------------ 7. ASH DUNES
        {
            ArenaDef d;
            d.id = "dunes";
            d.name = "ASH DUNES";
            d.subtitle = "DRIFT FIELDS";
            d.terrain.hillAmp = 17.0f; d.terrain.hillFreq = 0.0092f;
            d.terrain.midAmp = 6.5f; d.terrain.midFreq = 0.017f;
            d.terrain.fineAmp = 0.9f; d.terrain.microAmp = 0.12f;
            d.terrain.plainsFloor = 0.55f;
            d.terrain.groundColor = Vec3(0.150f, 0.140f, 0.115f);
            d.terrain.rockColor = Vec3(0.175f, 0.165f, 0.140f);
            d.terrain.slopeRockiness = 5.0f; d.terrain.mottle = 0.06f;
            d.render = mood(Vec3(0.76f, 0.38f, 0.26f), Vec3(1.34f, 1.22f, 1.00f),
                            Vec3(0.046f, 0.040f, 0.030f), Vec3(0.095f, 0.082f, 0.060f),
                            Vec3(0.014f, 0.013f, 0.014f), 0.0052f,
                            Vec3(0.18f, 0.17f, 0.15f), Vec3(0.050f, 0.046f, 0.038f));
            d.layout = DistrictLayout::Scattered;
            d.ruinCount = 5; d.districtRadius = 140.0f;
            d.floorsMin = 1; d.floorsMax = 3; d.ruinDamage = 0.85f;
            d.rockDensity = 0.35f; d.rubbleCount = 8; d.mastCount = 3;
            d.verticality = "LOW";
            a.push_back(d);
        }

        // ------------------------------------------------ 8. IMPACT FIELD
        {
            ArenaDef d;
            d.id = "craters";
            d.name = "IMPACT FIELD";
            d.subtitle = "SHELLED GROUND";
            d.terrain.hillAmp = 12.0f;
            d.terrain.midAmp = 7.5f; d.terrain.midFreq = 0.030f;
            d.terrain.fineAmp = 2.6f; d.terrain.fineFreq = 0.075f;
            d.terrain.ridged = 0.55f;
            d.terrain.groundColor = Vec3(0.100f, 0.098f, 0.088f);
            d.terrain.rockColor = Vec3(0.155f, 0.150f, 0.140f);
            d.render = mood(Vec3(0.66f, 0.40f, 0.42f), Vec3(1.22f, 1.16f, 1.12f),
                            Vec3(0.026f, 0.026f, 0.026f), Vec3(0.055f, 0.055f, 0.055f),
                            Vec3(0.009f, 0.009f, 0.011f), 0.0070f,
                            Vec3(0.14f, 0.14f, 0.16f), Vec3(0.034f, 0.034f, 0.036f));
            d.layout = DistrictLayout::Scattered;
            d.ruinCount = 10; d.districtRadius = 120.0f;
            d.floorsMin = 1; d.floorsMax = 3; d.ruinDamage = 0.9f;
            d.rockDensity = 1.1f; d.rubbleCount = 26; d.barrierCount = 10; d.bunkerCount = 4;
            d.verticality = "MEDIUM";
            a.push_back(d);
        }

        // ------------------------------------------------ 9. STORM HIGHLANDS
        {
            ArenaDef d;
            d.id = "highlands";
            d.name = "STORM HIGHLANDS";
            d.subtitle = "RIDGE COUNTRY";
            d.terrain.hillAmp = 38.0f; d.terrain.hillFreq = 0.0055f;
            d.terrain.ridged = 0.85f;
            d.terrain.midAmp = 6.0f; d.terrain.plainsFloor = 0.30f;
            d.terrain.groundColor = Vec3(0.080f, 0.115f, 0.105f);
            d.terrain.rockColor = Vec3(0.165f, 0.175f, 0.185f);
            d.terrain.slopeRockiness = 2.2f;
            d.render = mood(Vec3(0.58f, 0.44f, 0.56f), Vec3(1.10f, 1.16f, 1.30f),
                            Vec3(0.020f, 0.028f, 0.040f), Vec3(0.042f, 0.056f, 0.078f),
                            Vec3(0.006f, 0.010f, 0.018f), 0.0080f,
                            Vec3(0.13f, 0.16f, 0.22f), Vec3(0.030f, 0.034f, 0.044f));
            d.layout = DistrictLayout::Ring;
            d.ruinCount = 7; d.districtRadius = 130.0f;
            d.floorsMin = 2; d.floorsMax = 5;
            d.treeDensity = 0.5f; d.rockDensity = 2.0f; d.bunkerCount = 5; d.mastCount = 4;
            d.verticality = "HIGH";
            a.push_back(d);
        }

        // ------------------------------------------------ 10. SPILLWAY
        {
            ArenaDef d;
            d.id = "spillway";
            d.name = "SPILLWAY";
            d.subtitle = "DRAINAGE TRENCHES";
            d.terrain.hillAmp = 8.0f; d.terrain.midAmp = 2.2f;
            d.terrain.canyonDepth = 14.0f; d.terrain.canyonFreq = 0.0125f;
            d.terrain.canyonWidth = 0.09f;
            d.terrain.groundColor = Vec3(0.105f, 0.112f, 0.108f);
            d.terrain.rockColor = Vec3(0.150f, 0.155f, 0.155f);
            d.render = mood(Vec3(0.64f, 0.42f, 0.44f), Vec3(1.20f, 1.20f, 1.20f),
                            Vec3(0.022f, 0.030f, 0.032f), Vec3(0.048f, 0.060f, 0.062f),
                            Vec3(0.008f, 0.012f, 0.014f), 0.0066f,
                            Vec3(0.14f, 0.16f, 0.18f), Vec3(0.034f, 0.038f, 0.040f));
            d.layout = DistrictLayout::Line;
            d.ruinCount = 11; d.districtRadius = 110.0f;
            d.floorsMin = 1; d.floorsMax = 4; d.ruinScale = 1.2f;
            d.containerCount = 14; d.barrierCount = 24; d.pipeRackCount = 8; d.rubbleCount = 10;
            d.rockDensity = 0.3f;
            d.verticality = "MEDIUM";
            a.push_back(d);
        }

        // ------------------------------------------------ 11. OUTPOST RIDGE
        {
            ArenaDef d;
            d.id = "outpost";
            d.name = "OUTPOST RIDGE";
            d.subtitle = "FORTIFIED LINE";
            d.terrain.hillAmp = 26.0f; d.terrain.hillFreq = 0.0048f;
            d.terrain.ridged = 0.45f; d.terrain.midAmp = 4.5f;
            d.terrain.terrace = 4.5f; d.terrain.terraceBlend = 0.35f;
            d.terrain.groundColor = Vec3(0.098f, 0.130f, 0.098f);
            d.terrain.rockColor = Vec3(0.175f, 0.175f, 0.165f);
            d.render = mood(Vec3(0.70f, 0.40f, 0.36f), Vec3(1.26f, 1.22f, 1.12f),
                            Vec3(0.024f, 0.032f, 0.028f), Vec3(0.050f, 0.064f, 0.056f),
                            Vec3(0.008f, 0.011f, 0.013f), 0.0062f,
                            Vec3(0.15f, 0.16f, 0.17f), Vec3(0.036f, 0.038f, 0.036f));
            d.layout = DistrictLayout::Line;
            d.ruinCount = 8; d.districtRadius = 115.0f;
            d.floorsMin = 1; d.floorsMax = 3;
            d.bunkerCount = 12; d.barrierCount = 28; d.containerCount = 10;
            d.mastCount = 6; d.rockDensity = 0.9f; d.treeDensity = 0.25f;
            d.verticality = "MEDIUM";
            a.push_back(d);
        }

        // ------------------------------------------------ 13. THE CAUSEWAY
        // Islands and the bridges between them. The sea is a hard rule: a
        // spidertank cannot swim, so the map IS its chokepoints, and knocking
        // something off a deck is as good as shooting it.
        {
            ArenaDef d;
            d.id = "causeway";
            d.name = "GREY SOUND";
            d.subtitle = "ISLAND CAUSEWAYS";
            d.terrain.hillAmp = 34.0f; d.terrain.hillFreq = 0.0130f;
            d.terrain.plainsBias = 0.6f; d.terrain.plainsFloor = 0.05f;
            d.terrain.midAmp = 4.0f;
            d.terrain.groundColor = Vec3(0.085f, 0.120f, 0.095f);
            d.terrain.rockColor = Vec3(0.150f, 0.152f, 0.150f);
            d.render = mood(Vec3(0.52f, 0.44f, 0.50f), Vec3(1.05f, 1.10f, 1.20f),
                            Vec3(0.014f, 0.026f, 0.034f), Vec3(0.030f, 0.052f, 0.066f),
                            Vec3(0.006f, 0.010f, 0.016f), 0.0060f,
                            Vec3(0.12f, 0.14f, 0.19f), Vec3(0.030f, 0.034f, 0.042f));
            d.waterLevel = 1.6f;
            d.causewayCount = 16;
            d.layout = DistrictLayout::Scattered;
            d.ruinCount = 7; d.districtRadius = 120.0f;
            d.floorsMin = 1; d.floorsMax = 3;
            d.containerCount = 10; d.barrierCount = 10; d.rubbleCount = 6;
            d.mastCount = 3; d.rockDensity = 0.8f;
            d.extent = 230.0f;
            d.verticality = "LOW";
            a.push_back(d);
        }

        // ------------------------------------------------ 14. THE UNDERWORKS
        // Enclosed fighting: covered galleries, dense high blocks, narrow
        // lanes. The roof over your head is cover from drones and a ceiling
        // on your jumps, both at once.
        {
            ArenaDef d;
            d.id = "underworks";
            d.name = "THE UNDERWORKS";
            d.subtitle = "SERVICE GALLERIES";
            d.terrain.hillAmp = 6.0f; d.terrain.midAmp = 1.8f;
            d.terrain.groundColor = Vec3(0.088f, 0.095f, 0.100f);
            d.terrain.rockColor = Vec3(0.140f, 0.142f, 0.148f);
            d.render = mood(Vec3(0.46f, 0.38f, 0.52f), Vec3(0.95f, 0.98f, 1.12f),
                            Vec3(0.016f, 0.020f, 0.030f), Vec3(0.034f, 0.042f, 0.060f),
                            Vec3(0.007f, 0.009f, 0.015f), 0.0080f,
                            Vec3(0.11f, 0.12f, 0.17f), Vec3(0.028f, 0.030f, 0.040f));
            d.galleryCount = 7;
            d.layout = DistrictLayout::Grid;
            d.ruinCount = 12; d.districtRadius = 108.0f;
            d.floorsMin = 2; d.floorsMax = 5;
            d.containerCount = 16; d.barrierCount = 18;
            d.pipeRackCount = 10; d.rubbleCount = 8;
            d.extent = 190.0f;
            d.verticality = "HIGH";
            a.push_back(d);
        }

        // ------------------------------------------------ 12. ARCOLOGY SHELL
        {
            ArenaDef d;
            d.id = "arcology";
            d.name = "ARCOLOGY SHELL";
            d.subtitle = "VERTICAL RUIN";
            d.terrain.hillAmp = 5.0f; d.terrain.midAmp = 1.4f;
            d.terrain.flatRadius = 120.0f; d.terrain.flatFalloff = 50.0f;
            d.terrain.groundColor = Vec3(0.092f, 0.098f, 0.105f);
            d.terrain.rockColor = Vec3(0.145f, 0.150f, 0.160f);
            d.render = mood(Vec3(0.55f, 0.48f, 0.58f), Vec3(1.16f, 1.16f, 1.26f),
                            Vec3(0.020f, 0.024f, 0.034f), Vec3(0.044f, 0.050f, 0.068f),
                            Vec3(0.006f, 0.008f, 0.015f), 0.0060f,
                            Vec3(0.15f, 0.16f, 0.21f), Vec3(0.034f, 0.036f, 0.046f));
            d.layout = DistrictLayout::Cluster;
            d.ruinCount = 22; d.districtRadius = 90.0f;
            d.floorsMin = 6; d.floorsMax = 14; d.ruinScale = 1.25f; d.ruinDamage = 0.35f;
            d.concreteTint = Vec3(0.31f, 0.32f, 0.36f);
            d.barrierCount = 16; d.rubbleCount = 18; d.mastCount = 6; d.containerCount = 6;
            d.rockDensity = 0.0f;
            d.verticality = "EXTREME";
            a.push_back(d);
        }

        // ------------------------------------------------ 15. IRONWORKS ROW
        // A refinery avenue: pipe galleries, cooling stacks and fuel farms
        // strung down the lane under sodium haze. Everything here burns.
        {
            ArenaDef d;
            d.id = "refinery";
            d.name = "IRONWORKS ROW";
            d.subtitle = "REFINERY AVENUE";
            d.terrain.hillAmp = 4.5f; d.terrain.midAmp = 1.6f;
            d.terrain.groundColor = Vec3(0.105f, 0.095f, 0.080f);
            d.terrain.rockColor = Vec3(0.155f, 0.140f, 0.120f);
            d.render = mood(Vec3(0.78f, 0.52f, 0.30f), Vec3(1.28f, 1.10f, 0.88f),
                            Vec3(0.034f, 0.024f, 0.014f), Vec3(0.070f, 0.050f, 0.030f),
                            Vec3(0.012f, 0.008f, 0.006f), 0.0072f,
                            Vec3(0.18f, 0.13f, 0.09f), Vec3(0.044f, 0.032f, 0.022f));
            d.layout = DistrictLayout::Line;
            d.ruinCount = 8; d.districtRadius = 115.0f;
            d.floorsMin = 1; d.floorsMax = 4; d.ruinDamage = 0.35f;
            d.concreteTint = Vec3(0.33f, 0.30f, 0.26f);
            d.containerCount = 22; d.pipeRackCount = 22; d.barrierCount = 12;
            d.mastCount = 6; d.rubbleCount = 6; d.bunkerCount = 3;
            d.verticality = "MEDIUM";
            a.push_back(d);
        }

        // ------------------------------------------------ 16. WHITE PAN
        // A dead salt flat: hard light, kilometre sightlines, almost nothing
        // to hide behind. The long-range arena - bring reach or bring speed.
        {
            ArenaDef d;
            d.id = "saltflats";
            d.name = "WHITE PAN";
            d.subtitle = "SALT FLAT";
            d.terrain.hillAmp = 3.0f; d.terrain.midAmp = 0.9f;
            d.terrain.fineAmp = 0.5f; d.terrain.plainsBias = 2.6f;
            d.terrain.plainsFloor = 0.08f;
            d.terrain.groundColor = Vec3(0.230f, 0.225f, 0.210f);
            d.terrain.rockColor = Vec3(0.200f, 0.195f, 0.185f);
            d.terrain.slopeRockiness = 1.2f;
            d.render = mood(Vec3(0.95f, 0.90f, 0.80f), Vec3(1.45f, 1.42f, 1.35f),
                            Vec3(0.052f, 0.050f, 0.044f), Vec3(0.105f, 0.100f, 0.090f),
                            Vec3(0.016f, 0.015f, 0.014f), 0.0032f,
                            Vec3(0.25f, 0.24f, 0.22f), Vec3(0.055f, 0.052f, 0.048f));
            d.layout = DistrictLayout::Scattered;
            d.ruinCount = 4; d.districtRadius = 135.0f;
            d.floorsMin = 1; d.floorsMax = 2; d.ruinDamage = 0.75f;
            d.containerCount = 8; d.barrierCount = 6; d.rockDensity = 0.25f;
            d.rubbleCount = 4; d.mastCount = 4;
            d.verticality = "LOW";
            a.push_back(d);
        }

        // The size pass. Missions are linear routes down the structural axis
        // now, so every arena grows to give the route somewhere to go; the
        // avenue stretch in placeStructures lines the longer lane with city.
        for (ArenaDef& d : a) d.extent *= 1.45f;
        // The LONG WAR pass: ten-to-fifteen-minute contracts need somewhere
        // to happen. Land arenas grow again - structure counts scale with
        // the stretch automatically in placeStructures, and the extra open
        // ground IS the long-range game. Water arenas keep their size: the
        // causeway chains are counted by hand and must reach the far shore.
        // (Was 1.30. The route used to be a serpentine that crossed the
        // district three times to make a long march out of a short arena;
        // the user asked for LINEAR missions about twice as long, so the
        // ground itself is now long - a mile and a half of it - and the
        // lane is one gentle sweep down the middle. Generation is cheap,
        // and everything that draws or queries is distance-culled.)
        for (ArenaDef& d : a)
            if (d.waterLevel <= 0.0f) d.extent *= 4.6f;

        return a;
    }();
    return arenas;
}

const ArenaDef& arenaByIndex(int index) {
    const std::vector<ArenaDef>& all = arenaCatalog();
    const int n = static_cast<int>(all.size());
    int i = index % n;
    if (i < 0) i += n;
    return all[static_cast<size_t>(i)];
}

const ArenaDef* arenaById(const std::string& id) {
    for (const ArenaDef& a : arenaCatalog()) if (a.id == id) return &a;
    return nullptr;
}

} // namespace sb
