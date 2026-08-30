// Standalone visual probe for the mech and the world. Renders raw colour frames
// so the model, the gait and the climbing can be checked without the game layer.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/core/arena.h"
#include "../src/core/mech.h"
#include "../src/core/raster.h"
#include "../src/core/world.h"

using namespace sb;

static bool writePPM(const std::string& path, const std::vector<uint8_t>& rgb, int w, int h) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    std::fwrite(rgb.data(), 1, rgb.size(), f);
    std::fclose(f);
    return true;
}

static void dump(const std::string& path, const Rasterizer& r) {
    const int w = r.sampleWidth(), h = r.sampleHeight();
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 3);
    const std::vector<Vec3>& src = r.colorBuffer();
    for (size_t i = 0; i < src.size(); ++i) {
        const float c[3] = {src[i].x, src[i].y, src[i].z};
        for (int k = 0; k < 3; ++k)
            px[i * 3 + static_cast<size_t>(k)] =
                static_cast<uint8_t>(clampf(std::pow(clampf(c[k], 0.f, 1.f), 1.f/2.2f), 0.f, 1.f) * 255.f);
    }
    writePPM(path, px, w, h);
}

static int argInt(int argc, char** argv, const char* k, int d) {
    for (int i = 1; i + 1 < argc; ++i) if (!std::strcmp(argv[i], k)) return std::atoi(argv[i+1]);
    return d;
}
static std::string argStr(int argc, char** argv, const char* k, const std::string& d) {
    for (int i = 1; i + 1 < argc; ++i) if (!std::strcmp(argv[i], k)) return argv[i+1];
    return d;
}

int main(int argc, char** argv) {
    const int arenaIdx = argInt(argc, argv, "--arena", 0);
    const int frames = argInt(argc, argv, "--frames", 120);
    const int W = argInt(argc, argv, "--w", 640), H = argInt(argc, argv, "--h", 360);
    const std::string out = argStr(argc, argv, "--out", "out/probe.ppm");
    const std::string chassis = argStr(argc, argv, "--chassis", "ch_wasp");
    const std::string legs = argStr(argc, argv, "--legs", "lg_strider");
    const std::string armor = argStr(argc, argv, "--armor", "ar_none");
    const std::string engine = argStr(argc, argv, "--engine", "en_civic");
    const std::string wpn = argStr(argc, argv, "--weapon", "wp_pd9");
    const float camDist = argInt(argc, argv, "--dist", 900) / 100.0f;
    const float camPitch = argInt(argc, argv, "--pitch", -25) / 100.0f;
    const float camYaw = argInt(argc, argv, "--yaw", 60) / 100.0f;
    const int wallMode = argInt(argc, argv, "--wall", 0);

    World world;
    world.generate(arenaByIndex(arenaIdx), 1337);

    Loadout lo;
    lo.chassis = chassis; lo.legs = legs; lo.engine = engine;
    lo.armor = armor; lo.sensor = "se_basic";
    lo.ensureWeaponSlots();
    for (auto& w : lo.weapons) w = wpn;

    Mech mech;
    Rng rng(99);
    Vec3 spawn = world.findSpawnPoint(rng, Vec3(0,0,0), 4.0f, 12.0f);
    mech.init(world, lo, spawn, 0.0f, Team::Player, 7);

    Rasterizer raster;
    raster.resize(W, H, 1);
    RenderSettings rs = world.arena().render;

    std::vector<ShotRequest> shots;
    Vec3 target = spawn;
    if (wallMode) {
        // Walk at the nearest tall obstacle so the climb can be observed.
        float best = 1e9f;
        for (const Obstacle& o : world.obstacles()) {
            if (o.half.y < 4.0f || !o.climbable) continue;
            const float d = lengthXZ(o.center - spawn);
            if (d < best) { best = d; target = o.center; }
        }
    }

    for (int i = 0; i < frames; ++i) {
        MechInput in;
        if (wallMode) {
            Vec3 d = flattenY(target - mech.position());
            in.moveWorld = normalize(d);
            in.throttle = 1.0f;
        } else {
            in.moveWorld = Vec3(std::sin(i * 0.01f), 0, std::cos(i * 0.01f));
            in.throttle = (i > 20) ? 1.0f : 0.0f;
        }
        in.aimPoint = mech.position() + mech.forward() * 40.0f;
        if (argInt(argc, argv, "--jump", 0) && i > 40 && i < 46) in.jumpHeld = true;
        mech.update(1.0f / 60.0f, world, in, shots);
        shots.clear();
    }

    // Frame the mech, with the camera rolled so the machine stays upright.
    const Vec3 focus = mech.position() + mech.up() * 0.8f;
    const Vec3 look(std::sin(camYaw) * std::cos(camPitch), std::sin(camPitch),
                    std::cos(camYaw) * std::cos(camPitch));
    Camera cam;
    cam.set(focus - look * camDist, focus, deg2rad(58.0f),
            static_cast<float>(W) / H, 0.12f, 340.0f, mech.up());

    raster.beginFrame(cam, rs);
    world.submit(raster, cam.pos, 220.0f);
    mech.submit(raster, cam.pos);
    raster.endFrame();
    dump(out, raster);
    {
        int planted = 0;
        float maxExt = 0.0f, minFootY = 1e9f;
        for (const Leg& lg : mech.legs()) {
            if (lg.planted) ++planted;
            maxExt = std::max(maxExt, lg.extension);
            minFootY = std::min(minFootY, lg.footSolved.y);
        }
        const float groundUnder = world.terrain().height(mech.position().x, mech.position().z);
        std::printf("planted %d/6 | maxExt %.2f | body-above-ground %.2f | foot-vs-ground %.2f | ride %.2f\n",
                    planted, maxExt, mech.position().y - groundUnder, minFootY - groundUnder,
                    mech.stats().standHeight);
    }
    std::printf("arena %s | pos %.1f %.1f %.1f | up %.2f %.2f %.2f | state %s | tris %d\n",
                world.arena().name.c_str(), mech.position().x, mech.position().y, mech.position().z,
                mech.up().x, mech.up().y, mech.up().z,
                mechStateName(mech.state()), raster.lastTriangleCount());
    return 0;
}
