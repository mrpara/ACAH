// game.h - the whole game above the platform layer.
//
// This layer owns the run: which mission is loaded, whether you are fighting or
// shopping, where the camera is, and what the HUD says. It takes an InputState
// and a delta time and produces a finished AsciiFrame, with no knowledge of
// SDL, OpenGL or windows.
#pragma once

#include <functional>
#include <string>
#include <vector>
#include "ascii.h"
#include "audio.h"
#include "bindings.h"
#include "campaign.h"
#include "cabin.h"
#include "math3d.h"
#include "raster.h"
#include "store.h"
#include "world.h"

namespace sb {

struct InputState {
    bool forward = false, back = false, left = false, right = false;
    bool boost = false;
    // Analog drive from a gamepad stick, -1..1 each, camera-relative like the
    // keys (x right, y forward). Added to whatever the keys say; the vector's
    // length is the throttle, so a half-pushed stick is a walk.
    float moveX = 0.0f, moveY = 0.0f;
    float mouseDX = 0.0f, mouseDY = 0.0f;   // relative motion, pixels
    bool fireHeld = false;      // group 1, left mouse
    bool fire2Held = false;     // group 2, right mouse
    bool jumpHeld = false;
    bool ability[4] = {false, false, false, false};  // Q / E / R / F
    int toggleMask = 0;         // number keys 1-4, edge-triggered
    bool cycleScope = false;    // step the gunner sight: off -> 2x -> 6x -> off
    bool toggleFpv = false;     // first-person drive view

    // Edge-triggered by the platform layer, consumed once per frame.
    bool cyclePalette = false;
    bool cycleRamp = false;
    bool cycleBackground = false;
    bool toggleAscii = false;
    bool toggleHud = false;
    bool toggleDither = false;
    bool zoomIn = false;
    bool zoomOut = false;

    // Menu and store navigation, all edge-triggered.
    bool menuUp = false, menuDown = false, menuLeft = false, menuRight = false;
    bool menuConfirm = false, menuBack = false;
    bool menuNewProfile = false;  // [N] on a briefing: start over (press twice)
    bool menuToggle = false;      // the with/without comparison
    bool menuSell = false;
    bool menuNext = false;        // continue past a briefing or a result
    // Leaving the workshop for the next mission. Deliberately its own flag:
    // when Enter meant both "confirm" and "continue", pressing it in the store
    // entered the parts list and deployed on the same keystroke, so no part was
    // ever visible for long enough to buy.
    bool menuDeploy = false;
    // Opens the controls screen (from a menu, or the pause banner).
    bool openOptions = false;
    // Rebinding: the raw code that went down this frame, kCodeNone if none.
    // Only looked at while the controls screen is waiting for a press.
    int rawKey = -1;
    int rawPad = -1;
    // The pad was the last thing the pilot touched, so menus should show
    // pad buttons in their hints rather than keys.
    bool padActive = false;

    float mouseSensitivity = 0.0032f;
};

// What the player is looking at right now.
enum class GameScreen : int {
    Briefing = 0,
    Playing,
    MissionResult,
    Store
};

class Game {
public:
    void init(uint32_t seed, int cellsX, int cellsY, int supersample);
    void resize(int cellsX, int cellsY, int supersample);
    void setThreadCount(int threads) { raster_.setThreadCount(threads); }

    // Pixel width divided by pixel height of one character cell (8x16 -> 0.5).
    // The projection needs this or the whole scene comes out stretched.
    void setCellAspect(float widthOverHeight) { cellAspect_ = std::max(0.05f, widthOverHeight); }

    void update(float dt, const InputState& in);

    // Rasterize the scene and convert it to characters. The HUD is deliberately
    // NOT drawn here: it lives on its own coarser grid so its text stays
    // readable no matter how dense the scene glyphs get.
    void render(AsciiFrame& out);

    // Rasterize only, for the raw (ASCII-bypassed) view.
    void renderScene();

    // Fills `out` with nothing but HUD text, sized by the caller. Cells the HUD
    // does not touch stay blank, so the frame composites over whatever is
    // underneath - the character scene or the raw pixels.
    void drawHudOnly(AsciiFrame& out);
    void drawPixelRadar(AsciiFrame& out);

    // The character grid the HUD wants, given the space available. Returned in
    // cells of the HUD's own font, so the platform can pick a magnification.
    static void hudGridFor(int availCols, int availRows, int* colsOut, int* rowsOut);

    AsciiSettings& asciiSettings() { return ascii_; }
    RenderSettings& renderSettings() { return render_; }
    const Mech& player() const { return mission_.player(); }
    const World& world() const { return mission_.world(); }
    const Mission& mission() const { return mission_; }
    const PlayerProfile& profile() const { return profile_; }
    PlayerProfile& profile() { return profile_; }
    const Store& store() const { return store_; }
    // A frame's worth of sound requests. The platform drains this after
    // update(); if nothing does, the game is simply silent.
    AudioQueue& audio() { return audio_; }
    const AudioQueue& audio() const { return audio_; }

    const Rasterizer& rasterizer() const { return raster_; }
    const Camera& camera() const { return cam_; }
    GameScreen screen() const { return screen_; }
    // Gamepad haptics, 0..1, decaying; the platform layer turns these into
    // motor strengths. Core knows nothing about controllers.
    float padRumbleHit() const { return rumbleHit_; }
    float padRumbleGun() const { return rumbleGun_; }

    // Starts the mission the profile's level index points at.
    void startMission();
    // Loads acah_save.txt if present and restarts on the saved mission.
    // Called by the platform only - never by the test harnesses.
    void loadSave();
    // Test scaffolding: jump to the result screen as a cleared mission.
    void debugFinishMission();

    // Debug hook: overrides the follow camera with a fixed orbit around the bot.
    void setDebugOrbit(bool on, float yaw = 0.0f, float pitch = -0.35f, float dist = 12.0f) {
        debugOrbit_ = on; camYaw_ = yaw; camPitch_ = pitch;
        camDist_ = camDistTarget_ = dist;
    }

    void setStatusLine(const std::string& s) { status_ = s; }
    void setFrameMs(float ms) { frameMs_ = damp(frameMs_, ms, 6.0f, 0.05f); }
    // The platform pauses by not calling update(); this only tells the HUD.
    void setPaused(bool p) { paused_ = p; }

    // ---- controls ---------------------------------------------------------
    Bindings& bindings() { return bindings_; }
    const Bindings& bindings() const { return bindings_; }
    // Loads acah_controls.txt if present. Platform only.
    void loadBindings();
    // The controls screen: an overlay over whatever screen is up. While it is
    // open update() handles only its navigation and the rest of the game
    // stands still.
    bool optionsOpen() const { return optionsOpen_; }
    void openOptions() { optionsOpen_ = true; optCursor_ = 0; optCapture_ = 0; }
    // True while the screen is waiting for a key or button to bind: the
    // platform then feeds raw codes and swallows everything else.
    bool capturingInput() const { return optionsOpen_ && optCapture_ != 0; }
    // Escape: cancels a capture, otherwise closes the screen.
    void optionsEscape();
    // The platform names codes (it owns SDL); the core only formats them.
    void setInputNamers(std::function<std::string(int)> key,
                        std::function<std::string(int)> pad) {
        keyNamer_ = std::move(key);
        padNamer_ = std::move(pad);
    }
    // The label for an action on whichever device the pilot is using.
    std::string actionLabel(Action a) const;
    // The short form, for engraving on a cockpit panel rather than listing
    // in a table: "LMB" where the options screen says "MOUSE L".
    std::string actionLabelShort(Action a) const;
    std::string keyLabel(int code) const;
    std::string keyLabelShort(int code) const;
    std::string padLabel(int code) const;
    bool paused() const { return paused_; }
    // Audio health from the platform layer, drawn on the HUD status line so
    // a "sound keeps cutting out" report comes back with numbers attached:
    // voices in flight, worst recent limiter gain, device underrun count.
    void setAudioStats(int voices, float limFloor, int underruns) {
        audVoices_ = voices; audLimFloor_ = limFloor; audUnderruns_ = underruns;
    }
    bool hudVisible() const { return hudVisible_; }
    bool asciiEnabled() const { return asciiEnabled_; }

private:
    void updateCamera(float dt, const InputState& in);
public:
    int scopeStage() const { return scopeStage_; }
    bool firstPerson() const { return fpv_; }
    bool inCabin() const { return fpv_ && scopeStage_ == 0 && screen_ == GameScreen::Playing; }
private:
    void applyDisplayToggles(const InputState& in);
    void updateOptions(const InputState& in);
    void drawOptions(AsciiFrame& frame);
    // A menu hint: "[ENTER] FIT" on the keyboard, "[CROSS] FIT" on a pad.
    std::string hk(const char* key, int padCode) const;
    MechInput buildPlayerInput(const InputState& in) const;
    Vec3 traceAimPoint() const;
    // Sensor fire control: finds the target nearest the aim ray, solves the
    // intercept for the player's guns, and drives both the red lead marker
    // and the shot-direction assist. Better sensors reach further and pull
    // harder; a hard TargetLock is near-perfect.
    void updateAimAssist();

    void drawHud(AsciiFrame& frame);
    void drawBriefing(AsciiFrame& frame);
    void drawResult(AsciiFrame& frame);
    void drawStore(AsciiFrame& frame);
    void drawCombatHud(AsciiFrame& frame);
    // The brow's two plates - controls and contract - are text on the HUD
    // grid, inside the projected panels.
    void drawCabinBrow(AsciiFrame& frame);
    // The console's legends are lamps on the SCENE grid, placed by
    // projecting the instrument faces. See the comment on the definition.
    void drawCabinLamps(AsciiFrame& out);

    void emitAudio(float dt, const MechInput& mi);

    Mission mission_;
    AudioQueue audio_;
    // Edge detection for the sounds that are states rather than events.
    float prevHealth_ = -1.0f;
    int prevEnemies_ = -1;
    bool prevOverheat_ = false;
    bool prevCharging_ = false;
    bool prevAirborne_ = false;
    bool prevOnWall_ = false;
    float prevCrest_ = 0.0f;
    int prevProjectiles_ = 0;
    float footClock_ = 0.0f;
    float slipClock_ = 0.0f;
    PlayerProfile profile_;
    Store store_;
    LevelDef level_;

    Rasterizer raster_;
    Camera cam_;
    RenderSettings render_;
    RenderSettings baseRender_;   // the arena's lighting before any set-piece darkens it
    AsciiSettings ascii_;

    GameScreen screen_ = GameScreen::Briefing;
    float screenTimer_ = 0.0f;

    float camYaw_ = 0.0f;
    float camPitch_ = 0.26f;
    float camDist_ = 12.0f;
    // Gunner sight and first-person drive. The scope is stages of magnified
    // first-person from the turret; FPV is an unmagnified view from the hull.
    int scopeStage_ = 0;        // 0 off, 1 low power, 2 high power
    // First person is the DEFAULT view: the pilot sits in the cabin and the
    // instruments are on the console (see cabin.h). X steps outside.
    bool fpv_ = true;
    bool fpvBeforeScope_ = true;   // the view to return to when the sight comes down
    CabinLayout cabinLayout_;   // rebuilt whenever the loadout changes
    float camDistTarget_ = 12.0f;
    Vec3 camPos_{0.0f, 0.0f, 0.0f};
    Vec3 camFocus_{0.0f, 0.0f, 0.0f};
    // The orbit direction the mouse has steered - where the view is HEADED,
    // as opposed to camFocus_-camPos_, which is where the trailing boom
    // happens to be looking this frame. Drive input is built from this.
    Vec3 camOrbitDir_{0.0f, 0.0f, 1.0f};
    // The camera's own up vector, smoothed toward the mech's. This is what
    // keeps the machine upright on screen while it walks up a wall: the world
    // rolls around the tank instead of the tank rolling out of frame.
    Vec3 camUp_{0.0f, 1.0f, 0.0f};
    // The orbit's reference forward, carried across frames and only re-fitted
    // to camUp_. Deriving it from the machine's heading instead makes the
    // camera and the body chase each other every time you press a key.
    Vec3 camRefFwd_{0.0f, 0.0f, 1.0f};
    Vec3 aimPoint_{0.0f, 0.0f, 0.0f};
    mutable bool aimHitSomething_ = false;
    Vec3 leadPoint_{0.0f, 0.0f, 0.0f};   // where the sensor says to shoot
    bool leadValid_ = false;
    float assistStrength_ = 0.0f;
    float cellAspect_ = 0.5f;

    uint32_t seed_ = 1u;
    bool hudVisible_ = true;
    bool asciiEnabled_ = true;
    bool debugOrbit_ = false;
    float viewDistance_ = 250.0f;
    float frameMs_ = 16.0f;
    bool paused_ = false;
    int audVoices_ = 0;
    float audLimFloor_ = 1.0f;
    int audUnderruns_ = 0;
    float elapsed_ = 0.0f;
    // Edge-detect for the objective-complete cue: -1 until the first mission
    // frame, so starting a contract does not fire a completion chime.
    int lastObjectivesDone_ = -1;
    // Rate limit on the incoming-rocket warning.
    float warnCooldown_ = 0.0f;
    float jamHum_ = 0.0f;      // retrigger clock for the jammer drone
    float hitMarker_ = 0.0f;
    // Camera shake: fed by heavy fire, nearby blasts and taking hits, decays
    // fast. Applied to the eye only, never the aim, so it reads as recoil
    // without costing accuracy.
    float shake_ = 0.0f;
    float rumbleHit_ = 0.0f;   // gamepad haptics: hull hits (low motor)
    float rumbleGun_ = 0.0f;   // gamepad haptics: own guns (high motor)
    // Plant-driven footfall state and the machine-noise clocks.
    bool prevStepping_[6] = {};
    float prevTurretYaw_ = 0.0f;
    float servoClock_ = 0.0f;
    float humClock_ = 0.0f;
    float respawnBanner_ = 0.0f;   // seconds left on the checkpoint banner
    float wipeArmed_ = 0.0f;       // [N] pressed once: confirm window
    std::vector<Vec3> radarBg_;    // cached terrain sweep for the pixel radar
    int radarTick_ = 0;
    Rng dustRng_{20260829u};
    std::string status_ = "SYSTEMS NOMINAL";

    // The cabin's body sway this frame, so the HUD pass can project the
    // instrument panels exactly where the geometry pass drew them.
    Vec3 cabinSway_{0.0f, 0.0f, 0.0f};

    Bindings bindings_;
    bool optionsOpen_ = false;
    int optCursor_ = 0;
    int optCapture_ = 0;         // 0 idle, 1 waiting for a key, 2 for a pad button
    bool padActive_ = false;
    std::function<std::string(int)> keyNamer_, padNamer_;
};

} // namespace sb
