// main.cpp - SDL3 + OpenGL host for the ACAH ASCII engine.
//
// Everything interesting lives in src/core, which has no dependencies at all.
// This file owns the window, the input, the frame clock, and handing the
// finished character grid to the GPU.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "../core/ascii.h"
#include "../core/font.h"
#include "../core/game.h"
#include "gl.h"
#include "audio.h"
#include "glyph_renderer.h"
#include "raw_renderer.h"

using namespace sb;

namespace {

struct Options {
    int width = 1600;
    int height = 900;
    int supersample = 2;
    int seed = 1337;
    int threads = 0;          // 0 = hardware concurrency
    bool fullscreen = true;   // ACAH defaults to fullscreen
    bool vsync = true;
    // Development aids: run a scripted pilot and/or capture the framebuffer.
    std::string screenshot;
    int exitAfterFrames = 0;
    bool demo = false;
    bool startRaw = false;
    // The defaults are the finest ramp, solid cell backgrounds and the densest
    // font. Together they turn the grid into a low-resolution image with
    // character texture on top, which is far more legible than sparse glyphs on
    // black - and legibility is the whole point of this renderer. All three are
    // still switchable at runtime (R, B and F).
    int background = static_cast<int>(Background::Solid);
    int font = 2;                     // 0 = 8x16, 1 = 6x12, 2 = 4x8
    int ramp = static_cast<int>(Ramp::Fine);
    bool dither = false;
};

// Writes the GL framebuffer to a binary PPM. Used to verify the rendering path
// on machines with no one sitting in front of them.
bool captureFramebuffer(const std::string& path, int w, int h) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3);
    gl::PixelStorei(gl::PACK_ALIGNMENT, 1);
    gl::ReadPixels(0, 0, w, h, gl::RGB, gl::UNSIGNED_BYTE, px.data());
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    // GL reports rows bottom-up; PPM wants them top-down.
    for (int y = h - 1; y >= 0; --y)
        std::fwrite(px.data() + static_cast<size_t>(y) * w * 3, 1, static_cast<size_t>(w) * 3, f);
    std::fclose(f);
    return true;
}

int argInt(int argc, char** argv, const char* key, int fallback) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], key) == 0) return SDL_atoi(argv[i + 1]);
    return fallback;
}
bool argFlag(int argc, char** argv, const char* key) {
    for (int i = 1; i < argc; ++i) if (std::strcmp(argv[i], key) == 0) return true;
    return false;
}

// ---------------------------------------------------------------- gamepad --
// One pad at a time, opened on hotplug so a controller plugged in after
// launch (or reconnected mid-mission) just works. A DualShock 4 over USB or
// Bluetooth is what SDL3's HIDAPI driver reads natively; every other pad SDL
// knows arrives through the same standard layout, so the mapping below is in
// SDL's names and the PlayStation glyphs are only in the comments.
struct Pad {
    SDL_Gamepad* dev = nullptr;
    // Edge detection for the buttons that mean "press", not "hold".
    bool wasDown[SDL_GAMEPAD_BUTTON_COUNT] = {};
    bool jumpWasDown = false;

    void open(SDL_JoystickID id) {
        if (dev) return;
        dev = SDL_OpenGamepad(id);
        if (dev) {
            std::printf("gamepad: %s\n", SDL_GetGamepadName(dev));
            // A little rumble on connect: the pad is live.
            SDL_RumbleGamepad(dev, 0x3000, 0x3000, 120);
        }
    }
    void close(SDL_JoystickID id) {
        if (dev && SDL_GetGamepadID(dev) == id) {
            SDL_CloseGamepad(dev);
            dev = nullptr;
            std::printf("gamepad: disconnected\n");
        }
    }
    float axis(SDL_GamepadAxis a) const {
        return static_cast<float>(SDL_GetGamepadAxis(dev, a)) / 32767.0f;
    }
    bool held(SDL_GamepadButton b) const { return SDL_GetGamepadButton(dev, b); }
    // True on the frame the button goes down.
    bool pressed(SDL_GamepadButton b) {
        const bool now = held(b);
        const bool edge = now && !wasDown[b];
        wasDown[b] = now;
        return edge;
    }
};

// The core stores bindings as plain integers; these pin them to SDL's enums.
static_assert(SDL_SCANCODE_W == 26 && SDL_SCANCODE_SPACE == 44 && SDL_SCANCODE_LSHIFT == 225,
              "core/bindings.cpp assumes SDL's HID scancodes");
static_assert(SDL_GAMEPAD_BUTTON_SOUTH == 0 && SDL_GAMEPAD_BUTTON_BACK == 4 &&
              SDL_GAMEPAD_BUTTON_RIGHT_STICK == 8 && SDL_GAMEPAD_BUTTON_LEFT_SHOULDER == 9 &&
              SDL_GAMEPAD_BUTTON_DPAD_UP == 11 && SDL_GAMEPAD_BUTTON_TOUCHPAD == 20,
              "core/bindings.cpp assumes SDL's gamepad button order");
static_assert(SDL_GAMEPAD_AXIS_LEFT_TRIGGER == 4 && SDL_GAMEPAD_AXIS_RIGHT_TRIGGER == 5,
              "core/bindings.cpp assumes SDL's gamepad axis order");

// Names for the controls screen and the menu hints. The pad names follow
// the pad that is plugged in: a DualShock gets CROSS / CIRCLE, anything
// else A / B.
std::string keyCodeName(int code) {
    if (code < 0 || code >= sb::kMouseCodeBase) return std::string();
    const char* n = SDL_GetScancodeName(static_cast<SDL_Scancode>(code));
    return n ? std::string(n) : std::string();
}

std::string padCodeName(SDL_Gamepad* dev, int code) {
    if (code < 0) return std::string();
    if (code >= sb::kPadAxisBase) {
        const int axis = code - sb::kPadAxisBase;
        const bool ps = dev && SDL_GetGamepadType(dev) >= SDL_GAMEPAD_TYPE_PS3 &&
                        SDL_GetGamepadType(dev) <= SDL_GAMEPAD_TYPE_PS5;
        if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) return ps ? "L2" : "LT";
        if (axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) return ps ? "R2" : "RT";
        return "AXIS " + std::to_string(axis);
    }
    const SDL_GamepadButton b = static_cast<SDL_GamepadButton>(code);
    if (dev) {
        switch (SDL_GetGamepadButtonLabel(dev, b)) {
            case SDL_GAMEPAD_BUTTON_LABEL_CROSS:    return "CROSS";
            case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:   return "CIRCLE";
            case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:   return "SQUARE";
            case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return "TRIANGLE";
            default: break;
        }
    }
    const bool ps = dev && SDL_GetGamepadType(dev) >= SDL_GAMEPAD_TYPE_PS3 &&
                    SDL_GetGamepadType(dev) <= SDL_GAMEPAD_TYPE_PS5;
    switch (b) {
        case SDL_GAMEPAD_BUTTON_SOUTH:          return ps ? "CROSS" : "A";
        case SDL_GAMEPAD_BUTTON_EAST:           return ps ? "CIRCLE" : "B";
        case SDL_GAMEPAD_BUTTON_WEST:           return ps ? "SQUARE" : "X";
        case SDL_GAMEPAD_BUTTON_NORTH:          return ps ? "TRIANGLE" : "Y";
        case SDL_GAMEPAD_BUTTON_BACK:           return ps ? "SHARE" : "BACK";
        case SDL_GAMEPAD_BUTTON_GUIDE:          return "GUIDE";
        case SDL_GAMEPAD_BUTTON_START:          return ps ? "OPTIONS" : "START";
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:     return ps ? "L3" : "LS";
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:    return ps ? "R3" : "RS";
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return ps ? "L1" : "LB";
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return ps ? "R1" : "RB";
        case SDL_GAMEPAD_BUTTON_DPAD_UP:        return "D-UP";
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return "D-DOWN";
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return "D-LEFT";
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return "D-RIGHT";
        case SDL_GAMEPAD_BUTTON_TOUCHPAD:       return "TOUCHPAD";
        default: return "PAD " + std::to_string(code);
    }
}

// Radial deadzone with the remaining travel rescaled to 0..1, plus a gentle
// curve so small deflections are fine control and the last third is speed.
void stickVector(float x, float y, float* ox, float* oy, float dead, float curve) {
    const float len = std::sqrt(x * x + y * y);
    if (len < dead) { *ox = 0.0f; *oy = 0.0f; return; }
    const float scaled = std::min((len - dead) / (1.0f - dead), 1.0f);
    const float shaped = std::pow(scaled, curve);
    *ox = x / len * shaped;
    *oy = y / len * shaped;
}

void printControls() {
    std::printf(
        "\nACAH - ASCII walker warfare\n"
        "  W A S D      move (relative to the camera)\n"
        "  (no sprint key: the machine always runs at full pace)\n"
        "  Q            use your machine's active ability\n"
        "  Space        hold to charge a jump, release to leap\n"
        "  Mouse        aim the turret / orbit the camera\n"
        "  Left click   fire\n"
        "  Wheel        camera distance\n"
        "  Enter        deploy / accept a result / fit a part\n"
        "  Arrows       navigate the workshop\n"
        "  Backspace    back out of a workshop pane\n"
        "  Tab          workshop: preview with / without the part\n"
        "  X            workshop: sell the fitted part\n"
        "  P / R        cycle palette / character ramp\n"
        "  B            cycle cell background (none / dim / solid)\n"
        "  V            toggle the ASCII filter (raw 3D view)\n"
        "  F            cycle font density (8x16 / 6x12 / 4x8)\n"
        "  - / =        glyph scale\n"
        "  1 2 3        supersampling 1x / 2x / 3x\n"
        "  E            toggle silhouette edge enhancement\n"
        "  T            toggle ordered dither\n"
        "  G            toggle CRT scanline + glow\n"
        "  H            toggle HUD\n"
        "  M            mute audio\n"
        "  , / .        volume down / up\n"
        "  Tab          release the mouse cursor\n"
        "  F11          toggle fullscreen (default on; --windowed to start windowed)\n"
        "  Esc          pause (Q on the pause screen quits)\n"
        "  O            controls screen: rebind every key and pad button (menus / pause)\n"
        "\nGamepad (DualShock 4 / any SDL pad, plug in any time):\n"
        "  Left stick   drive (deflection = pace)     Right stick  aim / look\n"
        "  L2 / R2      fire group L / R              Cross        hold to charge a jump\n"
        "  L1 / R1      legs / engine ability         Square/Circle armour / sensor ability\n"
        "  Triangle     gunner sight zoom             R3           first-person / third-person\n"
        "  D-pad        mounts 1-4 on/off             Options      continue / deploy\n"
        "  Touchpad     HUD                           Share        mute\n"
        "  Workshop: D-pad navigate, Cross fit, Circle back, Triangle compare,\n"
        "            Square sell, Options deploy\n\n");
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    opt.width = argInt(argc, argv, "--width", opt.width);
    opt.height = argInt(argc, argv, "--height", opt.height);
    opt.supersample = argInt(argc, argv, "--ss", opt.supersample);
    opt.seed = argInt(argc, argv, "--seed", opt.seed);
    opt.threads = argInt(argc, argv, "--threads", opt.threads);
    if (argFlag(argc, argv, "--windowed")) opt.fullscreen = false;
    if (argFlag(argc, argv, "--fullscreen")) opt.fullscreen = true;
    if (argFlag(argc, argv, "--no-vsync")) opt.vsync = false;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--screenshot") == 0) opt.screenshot = argv[i + 1];
    opt.exitAfterFrames = argInt(argc, argv, "--exit-after", 0);
    opt.demo = argFlag(argc, argv, "--demo");
    opt.startRaw = argFlag(argc, argv, "--raw");
    // Fall back to the struct defaults, not to zero - otherwise the
    // command line silently overrides them with the oldest settings.
    opt.background = argInt(argc, argv, "--background", opt.background);
    opt.font = argInt(argc, argv, "--font", opt.font);
    opt.ramp = argInt(argc, argv, "--ramp", opt.ramp);
    opt.dither = argFlag(argc, argv, "--dither");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (opt.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    SDL_Window* window = SDL_CreateWindow("ACAH", opt.width, opt.height, flags);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::fprintf(stderr, "OpenGL 3.3 context creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(opt.vsync ? 1 : 0);

    if (const char* missing = gl::load()) {
        std::fprintf(stderr, "Could not resolve %s - an OpenGL 3.3 driver is required.\n", missing);
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Audio is optional: a machine with no sound device still plays the game.
    // Ask for a roomier device buffer first: the default period on some
    // Windows drivers is small enough that ordinary thread scheduling
    // starves the callback and the output audibly drops out - the mix can
    // be perfect and the sound still "cuts out" at the device.
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "2048");
    AudioEngine audio;
    const char* audioError = nullptr;
    if (!audio.init(&audioError))
        std::printf("audio unavailable (%s) - running silent\n",
                    audioError ? audioError : "no device");

    RawRenderer rawView;
    GlyphRenderer glyphs;
    // A second instance with its own atlas, so the HUD keeps an 8x16 font
    // however small the scene's cells get.
    GlyphRenderer hudGlyphs;
    const char* rendererError = nullptr;
    if (!rawView.init(&rendererError) || !glyphs.init(&rendererError) ||
        !hudGlyphs.init(&rendererError)) {
        std::fprintf(stderr, "Glyph renderer init failed: %s\n",
                     rendererError ? rendererError : "unknown");
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // ---------------------------------------------------------------- state --
    int fontIndex = 0;              // 0 = 8x16, 1 = 6x12, 2 = 4x8
    int glyphScale = 1;
    // Raw mode renders one sample per screen pixel block instead of per cell.
    int rawDivisor = 3;
    float edgeStrength = 0.42f;
    int supersample = (opt.supersample < 1) ? 1 : (opt.supersample > 3 ? 3 : opt.supersample);
    bool crtEffect = true;
    bool mouseCaptured = true;
    bool fullscreen = opt.fullscreen;

    Game game;
    AsciiFrame frame;
    int gridCols = 0, gridRows = 0, originX = 0, originY = 0;
    int fbW = 0, fbH = 0;

    // The HUD gets its own grid and its own glyph pass. Drawing it into the
    // scene's grid meant HUD text inherited the scene's character size, and at
    // the 4x8 default that is four-pixel-wide lettering - unreadable. This way
    // the scene can be as dense as it likes and the text stays legible.
    AsciiFrame hudFrame;
    int hudCols = 0, hudRows = 0, hudOriginX = 0, hudOriginY = 0, hudScale = 1;

    auto currentFont = [&]() {
        return (fontIndex == 2) ? fontTiny() : (fontIndex == 1) ? fontSmall() : fontLarge();
    };

    // Always 8x16 for the HUD, magnified to whatever keeps a workshop-sized
    // grid on screen. Picking the largest magnification that still fits the
    // layout means the text grows with the window instead of shrinking.
    auto resizeHud = [&]() {
        const FontDef hf = fontLarge();
        hudScale = 1;
        for (int s2 = 4; s2 >= 1; --s2) {
            const int cols = fbW / (hf.cellW * s2);
            const int rows = fbH / (hf.cellH * s2);
            if (cols >= 96 && rows >= 28) { hudScale = s2; break; }
        }
        int wantCols = 0, wantRows = 0;
        Game::hudGridFor(fbW / (hf.cellW * hudScale), fbH / (hf.cellH * hudScale),
                         &wantCols, &wantRows);
        hudCols = wantCols;
        hudRows = wantRows;
        hudOriginX = (fbW - hudCols * hf.cellW * hudScale) / 2;
        hudOriginY = (fbH - hudRows * hf.cellH * hudScale) / 2;
        hudFrame.resize(hudCols, hudRows);
        hudGlyphs.setFont(hf);
    };

    auto resizeGrid = [&]() {
        SDL_GetWindowSizeInPixels(window, &fbW, &fbH);
        if (fbW < 1) fbW = 1;
        if (fbH < 1) fbH = 1;
        const FontDef f = currentFont();
        const int cw = f.cellW * glyphScale;
        const int ch = f.cellH * glyphScale;
        gridCols = (fbW / cw); if (gridCols < 20) gridCols = 20;
        gridRows = (fbH / ch); if (gridRows < 12) gridRows = 12;
        // Centre the grid so leftover pixels are split between both edges.
        originX = (fbW - gridCols * cw) / 2;
        originY = (fbH - gridRows * ch) / 2;
        glyphs.setFont(f);
        resizeHud();

        if (game.asciiEnabled()) {
            game.resize(gridCols, gridRows, supersample);
            game.setCellAspect(static_cast<float>(f.cellW) / static_cast<float>(f.cellH));
        } else {
            // Raw view: the rasterizer draws square pixels, not character cells,
            // so both the resolution and the projection aspect change.
            game.resize(std::max(64, fbW / rawDivisor), std::max(48, fbH / rawDivisor), 1);
            game.setCellAspect(1.0f);
        }
    };

    SDL_GetWindowSizeInPixels(window, &fbW, &fbH);
    {
        const FontDef f = currentFont();
        gridCols = std::max(20, fbW / f.cellW);
        gridRows = std::max(12, fbH / f.cellH);
    }
    fontIndex = (opt.font < 0) ? 0 : (opt.font % 3);
    game.init(static_cast<uint32_t>(opt.seed), gridCols, gridRows, supersample);
    game.loadSave();
    game.asciiSettings().background =
        static_cast<Background>(opt.background % static_cast<int>(Background::Count));
    game.asciiSettings().ramp = static_cast<Ramp>(opt.ramp % static_cast<int>(Ramp::Count));
    game.asciiSettings().dither = opt.dither;
    if (opt.threads > 0) game.setThreadCount(opt.threads);
    resizeGrid();

    if (opt.startRaw) {
        InputState toggle;
        toggle.toggleAscii = true;
        game.update(0.0f, toggle);
        resizeGrid();
    }

    SDL_SetWindowRelativeMouseMode(window, true);
    printControls();

    const Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 previous = SDL_GetPerformanceCounter();
    bool mouseDown[8] = {};
    std::vector<int> keyEdges;        // codes that went down this frame
    bool spaceWasDown = false;
    bool running = true;
    bool paused = false;
    // Which device spoke last decides whether menus show keys or buttons.
    bool padActive = false;
    // Trigger edges for pad codes bound to "press" actions.
    bool axisWasDown[SDL_GAMEPAD_AXIS_COUNT] = {};
    game.loadBindings();
    int frameIndex = 0;
    bool pendingModeChange = false;   // set when V toggles the ASCII filter
    Pad pad;
    {
        // Anything already plugged in when we start.
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        if (ids) {
            for (int i = 0; i < count && !pad.dev; ++i) pad.open(ids[i]);
            SDL_free(ids);
        }
    }
    game.setInputNamers(keyCodeName, [&pad](int code) { return padCodeName(pad.dev, code); });

    while (running) {
        InputState in;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;

                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                case SDL_EVENT_WINDOW_RESIZED:
                    resizeGrid();
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    if (mouseCaptured) {
                        in.mouseDX += event.motion.xrel;
                        in.mouseDY += event.motion.yrel;
                    }
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    padActive = false;
                    if (game.capturingInput()) {
                        in.rawKey = sb::kMouseCodeBase + event.button.button;
                        break;
                    }
                    if (event.button.button == SDL_BUTTON_LEFT && !mouseCaptured) {
                        mouseCaptured = true;
                        SDL_SetWindowRelativeMouseMode(window, true);
                    } else if (mouseCaptured && event.button.button < 8) {
                        mouseDown[event.button.button] = true;
                        // A mouse button bound to a "press" action.
                        keyEdges.push_back(sb::kMouseCodeBase + event.button.button);
                    }
                    break;

                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (event.button.button < 8) mouseDown[event.button.button] = false;
                    break;

                case SDL_EVENT_MOUSE_WHEEL:
                    if (event.wheel.y > 0) in.zoomIn = true;
                    else if (event.wheel.y < 0) in.zoomOut = true;
                    break;

                case SDL_EVENT_GAMEPAD_ADDED:
                    pad.open(event.gdevice.which);
                    break;
                case SDL_EVENT_GAMEPAD_REMOVED:
                    pad.close(event.gdevice.which);
                    break;

                case SDL_EVENT_KEY_DOWN:
                    if (event.key.repeat) break;
                    padActive = false;
                    // The controls screen waiting for a key gets it raw, and
                    // nothing else sees the press. Escape still cancels.
                    if (game.capturingInput()) {
                        if (event.key.key == SDLK_ESCAPE) game.optionsEscape();
                        else in.rawKey = static_cast<int>(event.key.scancode);
                        break;
                    }
                    // Rebindable "press" actions go by scancode; the menu
                    // keys below stay fixed so a broken binding can never
                    // lock the pilot out of the screen that fixes it.
                    keyEdges.push_back(static_cast<int>(event.key.scancode));
                    switch (event.key.key) {
                        // Esc pauses in the field (it used to quit the game
                        // outright, mid-mission, on a key next to F1). Quit
                        // is Q from the pause screen, or Esc anywhere else.
                        case SDLK_ESCAPE:
                            if (game.optionsOpen()) {
                                game.optionsEscape();
                            } else if (game.screen() == GameScreen::Playing) {
                                paused = !paused;
                                game.setPaused(paused);
                                if (paused) for (bool& b : mouseDown) b = false;
                            } else {
                                running = false;
                            }
                            break;
                        case SDLK_Q:
                            if (paused && !game.optionsOpen()) running = false;
                            break;
                        case SDLK_O:
                            // The controls screen, from any menu or the pause
                            // banner - never mid-fight, O could be bound.
                            if (game.screen() != GameScreen::Playing || paused) in.openOptions = true;
                            break;
                        // ---- menu and store navigation ----------------------
                        case SDLK_UP:     in.menuUp = true; break;
                        case SDLK_DOWN:   in.menuDown = true; break;
                        case SDLK_LEFT:   in.menuLeft = true; break;
                        case SDLK_RIGHT:  in.menuRight = true; break;
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:
                            in.menuConfirm = true;
                            in.menuNext = true;
                            break;
                        case SDLK_BACKSPACE: in.menuBack = true; break;
                        // ---- gameplay letters -------------------------------
                        // In the workshop X still sells; in the field it is the
                        // first-person drive view, which the screen decides.
                        case SDLK_X:
                            if (game.screen() == GameScreen::Store || game.optionsOpen()) in.menuSell = true;
                            break;
                        case SDLK_N: in.menuNewProfile = true; break;
                        case SDLK_COMMA:
                            audio.setSfxVolume(audio.sfxVolume() - 0.1f);
                            audio.setMusicVolume(audio.musicVolume() - 0.1f);
                            break;
                        case SDLK_PERIOD:
                            audio.setSfxVolume(audio.sfxVolume() + 0.1f);
                            audio.setMusicVolume(audio.musicVolume() + 0.1f);
                            break;
                        // ---- display, all on function keys ------------------
                        // Gameplay owns the letters now; everything cosmetic
                        // lives on F2-F10 where no firefight will fat-finger it.
                        case SDLK_F2: in.cyclePalette = true; break;
                        case SDLK_F3: in.cycleRamp = true; break;
                        case SDLK_F4: in.cycleBackground = true; break;
                        case SDLK_F5: fontIndex = (fontIndex + 1) % 3; resizeGrid(); break;
                        case SDLK_F6:
                            edgeStrength = (edgeStrength > 0.0f) ? 0.0f : 0.42f;
                            game.asciiSettings().edgeStrength = edgeStrength;
                            break;
                        case SDLK_F7: in.toggleDither = true; break;
                        case SDLK_F8: crtEffect = !crtEffect; break;
                        case SDLK_F9:
                            supersample = (supersample % 3) + 1; resizeGrid(); break;
                        case SDLK_V:
                            in.toggleAscii = true;
                            pendingModeChange = true;
                            break;
                        case SDLK_MINUS:
                            if (glyphScale > 1) { --glyphScale; resizeGrid(); }
                            break;
                        case SDLK_EQUALS:
                            if (glyphScale < 4) { ++glyphScale; resizeGrid(); }
                            break;
                        case SDLK_TAB:
                            // In the workshop this is the with/without preview
                            // toggle; everywhere else it releases the mouse.
                            if (game.screen() == GameScreen::Store || game.optionsOpen()) {
                                in.menuToggle = true;
                            } else {
                                mouseCaptured = !mouseCaptured;
                                SDL_SetWindowRelativeMouseMode(window, mouseCaptured);
                                for (bool& b : mouseDown) b = false;
                            }
                            break;
                        case SDLK_F11:
                            fullscreen = !fullscreen;
                            SDL_SetWindowFullscreen(window, fullscreen);
                            break;
                        default: break;
                    }
                    break;

                default: break;
            }
        }
        if (!running) break;

        if (opt.demo) {
            // Scripted pilot, so a capture shows the bot mid-stride and firing
            // rather than standing at the spawn point.
            // Press through the briefing first, otherwise a capture just shows
            // the mission text.
            in.menuNext = (frameIndex == 4);
            in.menuConfirm = (frameIndex == 4);
            in.forward = frameIndex > 15;
            in.boost = frameIndex > 40;
            in.mouseDX = (frameIndex > 50 && frameIndex < 90) ? 2.4f : 0.0f;
            in.fireHeld = frameIndex > 70;
        }
        // OR against the scripted values rather than assigning, so --demo input
        // survives the keyboard poll.
        const bool* keys = SDL_GetKeyboardState(nullptr);
        const Bindings& bind = game.bindings();
        // A bound code is down: a scancode from the keyboard state, a mouse
        // button from the tracked buttons (only while the cursor is captured,
        // or clicking the window to grab it would fire the guns).
        auto keyHeld = [&](Action a) {
            const int code = bind.at(a).key;
            if (code < 0) return false;
            if (code >= sb::kMouseCodeBase)
                return mouseCaptured && code - sb::kMouseCodeBase < 8 && mouseDown[code - sb::kMouseCodeBase];
            return code < SDL_SCANCODE_COUNT && keys[code];
        };
        auto keyPressed = [&](Action a) {
            const int code = bind.at(a).key;
            if (code < 0) return false;
            for (int c : keyEdges) if (c == code) return true;
            return false;
        };
        const bool inFieldKb = game.screen() == GameScreen::Playing && !paused && !game.optionsOpen();
        if (!game.optionsOpen()) {
            in.forward = in.forward || keyHeld(Action::Forward);
            in.back    = in.back    || keyHeld(Action::Back);
            in.left    = in.left    || keyHeld(Action::StrafeLeft);
            in.right   = in.right   || keyHeld(Action::StrafeRight);
            in.boost   = in.boost   || keyHeld(Action::Boost);
            in.fireHeld = in.fireHeld || keyHeld(Action::FireLeft);
            in.fire2Held = in.fire2Held || keyHeld(Action::FireRight);
            // Space is the charged jump in the field. In the workshop it is what
            // sends you back out, which is why it is read as an edge there.
            if (game.screen() == GameScreen::Store) {
                const bool spaceNow = keys[SDL_SCANCODE_SPACE];
                if (spaceNow && !spaceWasDown) in.menuDeploy = true;
                spaceWasDown = spaceNow;
            } else {
                in.jumpHeld = in.jumpHeld || keyHeld(Action::Jump);
                spaceWasDown = keys[SDL_SCANCODE_SPACE];
            }
            if (inFieldKb) {
                for (int k = 0; k < 4; ++k)
                    if (keyPressed(static_cast<Action>(static_cast<int>(Action::AbilityLegs) + k)))
                        in.ability[k] = true;
                if (keyPressed(Action::Scope)) in.cycleScope = true;
                if (keyPressed(Action::ToggleView)) in.toggleFpv = true;
                for (int k = 0; k < 4; ++k) {
                    if (keyPressed(static_cast<Action>(static_cast<int>(Action::Mount1) + k)))
                        in.toggleMask |= (1 << k);
                    if (keyPressed(static_cast<Action>(static_cast<int>(Action::Group1) + k)))
                        in.toggleMask |= (16 << k);
                }
            }
            if (keyPressed(Action::ToggleHud)) in.toggleHud = true;
            if (keyPressed(Action::Mute)) audio.toggleMute();
        }
        keyEdges.clear();

        const Uint64 now = SDL_GetPerformanceCounter();
        const float dt = static_cast<float>(static_cast<double>(now - previous) / freq);
        previous = now;

        // ---- gamepad ------------------------------------------------------
        if (pad.dev) {
            const GameScreen scr = game.screen();
            const bool inStore = scr == GameScreen::Store;
            const bool inField = scr == GameScreen::Playing && !paused;
            // Sticks. Left drives; right looks. The look stick is a RATE, so
            // it is scaled by the frame time to mean degrees per second
            // rather than degrees per frame.
            float lx, ly, rx, ry;
            stickVector(pad.axis(SDL_GAMEPAD_AXIS_LEFTX), -pad.axis(SDL_GAMEPAD_AXIS_LEFTY),
                        &lx, &ly, 0.18f, 1.3f);
            stickVector(pad.axis(SDL_GAMEPAD_AXIS_RIGHTX), pad.axis(SDL_GAMEPAD_AXIS_RIGHTY),
                        &rx, &ry, 0.14f, 1.8f);
            if (std::fabs(lx) > 0.01f || std::fabs(ly) > 0.01f || std::fabs(rx) > 0.01f ||
                std::fabs(ry) > 0.01f)
                padActive = true;
            // Edges for every button and trigger, once per frame, so a
            // binding can ask about any of them without re-arming another.
            bool btnEdge[SDL_GAMEPAD_BUTTON_COUNT] = {};
            bool axisEdge[SDL_GAMEPAD_AXIS_COUNT] = {};
            int rawPad = -1;
            for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
                btnEdge[b] = pad.pressed(static_cast<SDL_GamepadButton>(b));
                if (btnEdge[b]) { padActive = true; if (rawPad < 0) rawPad = b; }
            }
            for (int a = SDL_GAMEPAD_AXIS_LEFT_TRIGGER; a <= SDL_GAMEPAD_AXIS_RIGHT_TRIGGER; ++a) {
                const bool down = pad.axis(static_cast<SDL_GamepadAxis>(a)) > 0.35f;
                axisEdge[a] = down && !axisWasDown[a];
                axisWasDown[a] = down;
                if (axisEdge[a]) { padActive = true; if (rawPad < 0) rawPad = sb::kPadAxisBase + a; }
            }
            auto padHeld = [&](Action a) {
                const int code = bind.at(a).pad;
                if (code < 0) return false;
                if (code >= sb::kPadAxisBase)
                    return pad.axis(static_cast<SDL_GamepadAxis>(code - sb::kPadAxisBase)) > 0.35f;
                return code < SDL_GAMEPAD_BUTTON_COUNT && pad.held(static_cast<SDL_GamepadButton>(code));
            };
            auto padPressed = [&](Action a) {
                const int code = bind.at(a).pad;
                if (code < 0) return false;
                if (code >= sb::kPadAxisBase)
                    return code - sb::kPadAxisBase < SDL_GAMEPAD_AXIS_COUNT && axisEdge[code - sb::kPadAxisBase];
                return code < SDL_GAMEPAD_BUTTON_COUNT && btnEdge[code];
            };

            if (game.capturingInput()) {
                if (rawPad >= 0) in.rawPad = rawPad;
                else if (btnEdge[SDL_GAMEPAD_BUTTON_EAST]) in.menuBack = true;
            } else if (game.optionsOpen()) {
                if (btnEdge[SDL_GAMEPAD_BUTTON_DPAD_UP])    in.menuUp = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_DPAD_DOWN])  in.menuDown = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_DPAD_LEFT])  in.menuLeft = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_DPAD_RIGHT]) in.menuRight = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_SOUTH]) in.menuConfirm = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_EAST])  in.menuBack = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_NORTH]) in.menuToggle = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_WEST])  in.menuSell = true;
            } else if (inField) {
                in.moveX += lx;
                in.moveY += ly;
                // ~150 deg/s at full deflection, in the same units as the mouse.
                const float lookPixelsPerSec = 2.6f / in.mouseSensitivity;
                in.mouseDX += rx * lookPixelsPerSec * std::min(dt, 0.05f);
                in.mouseDY += ry * lookPixelsPerSec * std::min(dt, 0.05f) * 0.8f;
                // Everything below follows the bindings. Defaults: L2 / R2
                // fire group L / R, Cross jumps, L1 / R1 / Square / Circle
                // the four systems, Triangle the sight, R3 the view, D-pad
                // the mounts, touchpad the HUD, Share mute.
                if (padHeld(Action::FireLeft)) in.fireHeld = true;
                if (padHeld(Action::FireRight)) in.fire2Held = true;
                in.jumpHeld = in.jumpHeld || padHeld(Action::Jump);
                in.forward = in.forward || padHeld(Action::Forward);
                in.back = in.back || padHeld(Action::Back);
                in.left = in.left || padHeld(Action::StrafeLeft);
                in.right = in.right || padHeld(Action::StrafeRight);
                in.boost = in.boost || padHeld(Action::Boost);
                for (int k = 0; k < 4; ++k)
                    if (padPressed(static_cast<Action>(static_cast<int>(Action::AbilityLegs) + k)))
                        in.ability[k] = true;
                if (padPressed(Action::Scope)) in.cycleScope = true;
                if (padPressed(Action::ToggleView)) in.toggleFpv = true;
                for (int k = 0; k < 4; ++k) {
                    if (padPressed(static_cast<Action>(static_cast<int>(Action::Mount1) + k)))
                        in.toggleMask |= (1 << k);
                    if (padPressed(static_cast<Action>(static_cast<int>(Action::Group1) + k)))
                        in.toggleMask |= (16 << k);
                }
                if (padPressed(Action::ToggleHud)) in.toggleHud = true;
                if (padPressed(Action::Mute)) audio.toggleMute();
                // Options pauses.
                if (btnEdge[SDL_GAMEPAD_BUTTON_START]) {
                    paused = !paused;
                    game.setPaused(paused);
                }
            } else if (paused) {
                // The pause banner: Options resumes, Share opens the controls.
                if (btnEdge[SDL_GAMEPAD_BUTTON_START]) { paused = false; game.setPaused(false); }
                if (btnEdge[SDL_GAMEPAD_BUTTON_BACK]) in.openOptions = true;
            } else if (inStore) {
                if (btnEdge[SDL_GAMEPAD_BUTTON_DPAD_UP])    in.menuUp = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_DPAD_DOWN])  in.menuDown = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_DPAD_LEFT])  in.menuLeft = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_DPAD_RIGHT]) in.menuRight = true;
                // The left stick navigates too, as a repeating D-pad.
                static float stickRepeat = 0.0f;
                stickRepeat -= dt;
                if (std::fabs(lx) > 0.6f || std::fabs(ly) > 0.6f) {
                    if (stickRepeat <= 0.0f) {
                        if (ly > 0.6f) in.menuUp = true;
                        else if (ly < -0.6f) in.menuDown = true;
                        else if (lx > 0.6f) in.menuRight = true;
                        else if (lx < -0.6f) in.menuLeft = true;
                        stickRepeat = 0.22f;
                    }
                } else {
                    stickRepeat = 0.0f;
                }
                if (btnEdge[SDL_GAMEPAD_BUTTON_SOUTH]) { in.menuConfirm = true; in.menuNext = true; }
                if (btnEdge[SDL_GAMEPAD_BUTTON_EAST])  in.menuBack = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_NORTH]) in.menuToggle = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_WEST])  in.menuSell = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_START]) in.menuDeploy = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_BACK])  in.openOptions = true;
            } else {
                // Briefing / result: Cross or Options continues, Share opens
                // the controls screen.
                if (btnEdge[SDL_GAMEPAD_BUTTON_SOUTH] || btnEdge[SDL_GAMEPAD_BUTTON_START]) {
                    in.menuConfirm = true;
                    in.menuNext = true;
                }
                if (btnEdge[SDL_GAMEPAD_BUTTON_BACK]) in.openOptions = true;
                if (btnEdge[SDL_GAMEPAD_BUTTON_EAST]) in.menuBack = true;
            }
            // Hit feedback: a thump proportional to what just landed on the
            // hull, and a lighter buzz for the guns firing. SDL takes
            // milliseconds; short pulses re-armed every frame blend fine.
            const float hit = game.padRumbleHit();
            const float gun = game.padRumbleGun();
            if (hit > 0.01f || gun > 0.01f) {
                const Uint16 low = static_cast<Uint16>(std::min(1.0f, hit * 1.2f + gun * 0.25f) * 0xFFFF);
                const Uint16 high = static_cast<Uint16>(std::min(1.0f, gun * 0.9f + hit * 0.3f) * 0xFFFF);
                SDL_RumbleGamepad(pad.dev, low, high, 60);
            }
        }

        const Uint64 simStart = SDL_GetPerformanceCounter();
        // Paused: the world stands still but the frame still draws, so the
        // pilot can look at the pause banner over the last view. A pause
        // that ends after a long wait must not hand the game a huge dt.
        in.padActive = padActive;
        if (paused && !game.optionsOpen() && !in.openOptions) {
            InputState idle;
            idle.padActive = padActive;
            game.update(0.0f, idle);
        } else if (paused) {
            // The controls screen over the pause banner: it needs the menu
            // input, the world still needs to stand still.
            game.update(0.0f, in);
        } else {
            game.update(dt, in);
        }
        // Toggling the ASCII filter changes the render resolution and the
        // projection aspect, so the resize has to happen after update() has
        // consumed the key and before anything is drawn.
        if (pendingModeChange) { pendingModeChange = false; resizeGrid(); }

        if (game.asciiEnabled()) {
            game.render(frame);
        } else {
            game.renderScene();
        }
        if (hudFrame.w != hudCols || hudFrame.h != hudRows) hudFrame.resize(hudCols, hudRows);
        game.drawHudOnly(hudFrame);
        audio.submit(game.audio());
        game.setAudioStats(audio.activeVoices(), audio.limiterFloor(),
                           audio.underruns());
        const Uint64 simEnd = SDL_GetPerformanceCounter();
        game.setFrameMs(static_cast<float>(static_cast<double>(simEnd - simStart) * 1000.0 / freq));

        gl::Viewport(0, 0, fbW, fbH);
        gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl::Clear(gl::COLOR_BUFFER_BIT);
        // Raw view paints the rasterizer output as pixels; the glyph pass then
        // draws the HUD on top of it.
        if (!game.asciiEnabled()) rawView.draw(game.rasterizer(), fbW, fbH);
        if (game.asciiEnabled())
            glyphs.draw(frame, fbW, fbH, originX, originY, glyphScale,
                        crtEffect ? 0.14f : 0.0f, crtEffect ? 0.18f : 0.0f);
        // The HUD goes on last, in its own font, over everything.
        hudGlyphs.draw(hudFrame, fbW, fbH, hudOriginX, hudOriginY, hudScale,
                       0.0f, 0.0f);
        ++frameIndex;
        if (opt.exitAfterFrames > 0 && frameIndex >= opt.exitAfterFrames) {
            if (!opt.screenshot.empty()) {
                if (captureFramebuffer(opt.screenshot, fbW, fbH))
                    std::printf("wrote %s (%dx%d)\n", opt.screenshot.c_str(), fbW, fbH);
                else
                    std::fprintf(stderr, "could not write %s\n", opt.screenshot.c_str());
            }
            running = false;
        }
        SDL_GL_SwapWindow(window);
    }

    glyphs.shutdown();
    rawView.shutdown();
    audio.shutdown();
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
