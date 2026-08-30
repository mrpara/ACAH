// headless.cpp - runs the game loop with no window, driving a scripted input
// sequence, and writes out both text dumps and PPM images of chosen frames.
//
//   spiderbot_headless [--cols N] [--rows N] [--ss N] [--frames N] [--out DIR]
//
// Used to verify the renderer, the gait and the weapons without a display, and
// to benchmark the rasterizer.
#include <chrono>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "../src/core/ascii.h"
#include "../src/core/font.h"
#include "../src/core/game.h"

using namespace sb;

namespace {

bool writePPM(const std::string& path, const std::vector<uint8_t>& rgb, int w, int h) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    std::fwrite(rgb.data(), 1, rgb.size(), f);
    std::fclose(f);
    return true;
}

bool writeText(const std::string& path, const std::string& text) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
    return true;
}

// Dumps the rasterizer's colour buffer straight to an image, bypassing the
// ASCII stage. Any geometry, culling or shading bug shows up here first.
bool writeRawBuffer(const std::string& path, const Rasterizer& raster) {
    const int w = raster.sampleWidth(), h = raster.sampleHeight();
    std::vector<uint8_t> rgb(static_cast<size_t>(w) * h * 3);
    const std::vector<Vec3>& src = raster.colorBuffer();
    for (size_t i = 0; i < src.size(); ++i) {
        for (int c = 0; c < 3; ++c) {
            const float v = (c == 0) ? src[i].x : (c == 1) ? src[i].y : src[i].z;
            const float s = std::pow(clampf(v, 0.0f, 1.0f), 1.0f / 2.2f);
            rgb[i * 3 + c] = static_cast<uint8_t>(clampf(s, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    }
    return writePPM(path, rgb, w, h);
}

bool argFlag(int argc, char** argv, const char* key) {
    for (int i = 1; i < argc; ++i) if (std::strcmp(argv[i], key) == 0) return true;
    return false;
}

int argInt(int argc, char** argv, const char* key, int fallback) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], key) == 0) return std::atoi(argv[i + 1]);
    return fallback;
}

std::string argStr(int argc, char** argv, const char* key, const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], key) == 0) return argv[i + 1];
    return fallback;
}

} // namespace

int main(int argc, char** argv) {
    const int cols = argInt(argc, argv, "--cols", 200);
    const int rows = argInt(argc, argv, "--rows", 56);
    const int ss = argInt(argc, argv, "--ss", 2);
    const int frames = argInt(argc, argv, "--frames", 240);
    const int seed = argInt(argc, argv, "--seed", 1337);
    const std::string outDir = argStr(argc, argv, "--out", "out");
    const int paletteArg = argInt(argc, argv, "--palette", 0);
    const bool dumpRaw = argFlag(argc, argv, "--raw");
    const bool orbit = argFlag(argc, argv, "--orbit");
    const int threads = argInt(argc, argv, "--threads", 0);
    // Wander drives the bot across far more terrain than the scripted demo,
    // which is what finds the worst-case leg extension and hull clearance.
    const bool wander = argFlag(argc, argv, "--wander");

    Game game;
    game.init(static_cast<uint32_t>(seed), cols, rows, ss);
    game.setCellAspect(static_cast<float>(kFont8x16Width) / kFont8x16Height);
    game.asciiSettings().palette = static_cast<Palette>(paletteArg % static_cast<int>(Palette::Count));
    if (threads > 0) game.setThreadCount(threads);
    {
        AsciiSettings& a = game.asciiSettings();
        const int black = argInt(argc, argv, "--black", -1);
        const int white = argInt(argc, argv, "--white", -1);
        const int contrast = argInt(argc, argv, "--contrast", -1);
        const int gamma = argInt(argc, argv, "--gamma", -1);
        if (black >= 0) a.blackPoint = black / 1000.0f;
        if (white >= 0) a.whitePoint = white / 1000.0f;
        if (contrast >= 0) a.contrast = contrast / 1000.0f;
        if (gamma >= 0) a.gamma = gamma / 1000.0f;
        if (argFlag(argc, argv, "--dither")) a.dither = true;
    }
    if (orbit) {
        const float oy = argInt(argc, argv, "--orbit-yaw", 60) / 100.0f;
        const float op = argInt(argc, argv, "--orbit-pitch", -30) / 100.0f;
        const float od = argInt(argc, argv, "--orbit-dist", 750) / 100.0f;
        game.setDebugOrbit(true, oy, op, od);
    }

    AsciiFrame frame;
    const FontDef font = fontLarge();

    // Frames worth capturing: startup pose, walking, turning, firing.
    const int captureAt[] = {1, 30, 75, 120, 165, 210, 239};
    const float dt = 1.0f / 60.0f;

    double totalMs = 0.0;
    double worstMs = 0.0;

    // Gait/pose regression metrics. A limb over 1.0 is stretched past its reach;
    // a negative hull clearance means the chassis is inside the terrain. Also
    // tracks the largest single-frame body height jump, which is what a
    // discontinuous pose adjustment looks like numerically.
    float worstStretch = 0.0f;
    float worstHeightJump = 0.0f;
    float prevBodyY = 0.0f;
    bool havePrevY = false;
    int climbFrames = 0;
    int embeddedFrames = 0;
    int worstStretchFrame = -1, worstJumpFrame = -1;
    Vec3 worstStretchPos, worstJumpPos;
    float worstJumpSpeed = 0.0f;

    for (int i = 0; i < frames; ++i) {
        InputState in;
        const float t = i * dt;

        if (wander) {
            // Deterministic pseudo-random pilot: sprint constantly, swinging the
            // view so the bot covers ground in every direction.
            Rng rng(static_cast<uint32_t>(i / 45) * 2654435761u + 11u);
            const float turn = rng.range(-9.0f, 9.0f);
            in.menuNext = (i == 0);
            in.forward = true;
            in.boost = true;
            in.left = rng.unit() < 0.18f;
            in.right = rng.unit() < 0.18f;
            in.mouseDX = turn;
            in.mouseDY = rng.range(-1.0f, 1.0f);
            game.update(dt, in);
            game.render(frame);
            {
                const Mech& bot = game.player();
                const float reach = bot.maxLegReach();
                for (const Leg& leg : bot.legs()) {
                    const float used = length(leg.foot - leg.hipWorld) / reach;
                    if (used > worstStretch) { worstStretch = used; worstStretchFrame = i;
                                               worstStretchPos = bot.position(); }
                }
                const Vec3 bp = bot.position();
                // Hull clearance and body-height continuity are measured
                // against the terrain height field, which says nothing useful
                // once the machine is on the side of a building. Skip those two
                // while climbing rather than reporting a false failure; limb
                // extension is pure geometry and stays valid everywhere.
                // "Is the hull inside something solid" is the check that
                // actually matters and it is valid everywhere. Measuring
                // clearance against the terrain height field said nothing
                // useful the moment the machine stood on rubble, let alone
                // climbed a building.
                if (game.world().insideSolid(bp, 0.0f)) ++embeddedFrames;
                const bool settled = bot.state() == MechState::Grounded && !bot.onWall();
                if (bot.onWall()) ++climbFrames;
                if (settled) {
                    if (havePrevY && i > 10) {
                        const float jump = std::fabs(bp.y - prevBodyY);
                        if (jump > worstHeightJump) { worstHeightJump = jump; worstJumpFrame = i;
                                                      worstJumpPos = bp; worstJumpSpeed = bot.speed(); }
                    }
                    prevBodyY = bp.y;
                    havePrevY = true;
                } else {
                    havePrevY = false;
                }
            }
            continue;
        }

        // Scripted pilot: walk forward, sweep the view, strafe, then open fire.
        // The first frame also presses through the mission briefing, otherwise
        // the pilot spends the whole run reading it.
        in.menuNext = (i == 0);
        in.forward = (i > 20 && i < 150) || (i > 180);
        in.right = (i > 90 && i < 130);
        in.left = (i > 200 && i < 230);
        in.boost = (i > 40 && i < 120);
        in.mouseDX = (i > 60 && i < 110) ? 3.2f : ((i > 150 && i < 175) ? -4.0f : 0.0f);
        in.mouseDY = (i > 130 && i < 145) ? 1.4f : 0.0f;
        in.fireHeld = (i > 100 && i < 135) || (i > 195 && i < 235);
        (void)t;

        const auto t0 = std::chrono::high_resolution_clock::now();
        game.update(dt, in);
        game.render(frame);
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        game.setFrameMs(static_cast<float>(ms));
        if (i > 5) { totalMs += ms; worstMs = (ms > worstMs) ? ms : worstMs; }

        {
            const Mech& bot = game.player();
            const float reach = bot.maxLegReach();
            for (const Leg& leg : bot.legs()) {
                const float used = length(leg.foot - leg.hipWorld) / reach;
                if (used > worstStretch) worstStretch = used;
            }
            const Vec3 bp = bot.position();
            if (game.world().insideSolid(bp, 0.0f)) ++embeddedFrames;
            const bool settled = bot.state() == MechState::Grounded && !bot.onWall();
            if (bot.onWall()) ++climbFrames;
            if (settled) {
                if (havePrevY && i > 10) {
                    const float jump = std::fabs(bp.y - prevBodyY);
                    if (jump > worstHeightJump) worstHeightJump = jump;
                }
                prevBodyY = bp.y;
                havePrevY = true;
            } else {
                havePrevY = false;
            }
        }

        for (int c : captureAt) {
            if (c != i) continue;
            const std::string base = outDir + "/frame_" + std::to_string(i);
            writeText(base + ".txt", frameToText(frame));
            std::vector<uint8_t> rgb;
            int w = 0, h = 0;
            blitFrameToRGB(frame, font, rgb, w, h);
            writePPM(base + ".ppm", rgb, w, h);
            if (dumpRaw) writeRawBuffer(base + "_raw.ppm", game.rasterizer());
            std::printf("captured frame %d -> %s.ppm (%dx%d px)\n", i, base.c_str(), w, h);
        }
    }

    if (argFlag(argc, argv, "--hist")) {
        int counts[128] = {};
        for (const Cell& c : frame.cells) {
            const int i = static_cast<unsigned char>(c.ch);
            if (i < 128) ++counts[i];
        }
        const char* ramp = rampChars(game.asciiSettings().ramp);
        std::printf("glyph histogram (final frame):\n");
        for (const char* p2 = ramp; *p2; ++p2) {
            const int i = static_cast<unsigned char>(*p2);
            std::printf("  '%c' %5.1f%%\n", *p2,
                        100.0 * counts[i] / static_cast<double>(frame.cells.size()));
        }
    }

    std::printf("\ngait check over %d frames (%d on a structure, "
                "terrain-relative checks skipped there):\n", frames, climbFrames);
    std::printf("  worst limb extension   %.3f of reach %s\n", worstStretch,
                worstStretch > 1.0f ? "  <-- STRETCHED" : "");
    std::printf("  hull inside geometry   %d frames %s\n", embeddedFrames,
                embeddedFrames > 0 ? "  <-- CLIPPING" : "");
    std::printf("  worst body height jump %.4f m/frame %s\n", worstHeightJump,
                worstHeightJump > 0.10f ? "  <-- DISCONTINUOUS" : "");
    std::printf("  stretch at frame %d pos %.1f,%.1f | jump at frame %d pos %.1f,%.1f spd %.1f\n",
                worstStretchFrame, worstStretchPos.x, worstStretchPos.z,
                worstJumpFrame, worstJumpPos.x, worstJumpPos.z, worstJumpSpeed);

    const double avg = totalMs / static_cast<double>(frames - 6);
    std::printf("\ngrid %dx%d  supersample %d  samples %dx%d\n", cols, rows, ss, cols * ss, rows * ss);
    std::printf("avg frame %.2f ms (%.1f fps)   worst %.2f ms\n", avg, 1000.0 / avg, worstMs);
    return 0;
}
