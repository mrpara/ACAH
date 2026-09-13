// Rebindable controls. The core knows actions; the platform knows keys and
// pad buttons. Codes are stored as plain integers so the core stays free of
// SDL: a keyboard code is the USB HID scancode SDL uses (stable across
// platforms - W is 26 everywhere), a mouse button is 1000 + its SDL index,
// a pad code is the SDL_GamepadButton index, or 100 + the SDL_GamepadAxis
// index for a trigger. The platform static_asserts these against SDL's
// enums so the two cannot drift apart.
#pragma once

#include <string>

namespace sb {

enum class Action : int {
    Forward = 0,
    Back,
    StrafeLeft,
    StrafeRight,
    Boost,
    Jump,
    FireLeft,
    FireRight,
    AbilityLegs,
    AbilityEngine,
    AbilityArmor,
    AbilitySensor,
    Scope,
    ToggleView,
    Mount1,
    Mount2,
    Mount3,
    Mount4,
    Group1,
    Group2,
    Group3,
    Group4,
    ToggleHud,
    Mute,
    Count
};

constexpr int kActionCount = static_cast<int>(Action::Count);

// Code conventions shared with the platform layer.
constexpr int kCodeNone = -1;
constexpr int kMouseCodeBase = 1000;    // 1000 + SDL mouse button (1 = left)
constexpr int kPadAxisBase = 100;       // 100 + SDL_GamepadAxis (a trigger)

struct ActionBinding {
    int key = kCodeNone;     // keyboard scancode or mouse code
    int pad = kCodeNone;     // pad button or trigger code
};

// Which of these are "held" (moving, firing) versus "pressed" (a toggle).
bool actionIsHeld(Action a);
const char* actionName(Action a);

// Names for a code without asking the platform. The platform can do better
// (it knows the keyboard layout and which pad is plugged in), but the core
// has to be able to letter its own cockpit plate with no platform attached -
// a cabin that reads "KEY 26 KEY 4 KEY 22 KEY 7" is no use to anybody.
std::string defaultKeyName(int code);
std::string defaultPadName(int code);

class Bindings {
public:
    Bindings() { reset(); }
    void reset();

    ActionBinding& at(Action a) { return b_[static_cast<int>(a)]; }
    const ActionBinding& at(Action a) const { return b_[static_cast<int>(a)]; }

    // A plain text file, one action per line: NAME key pad.
    bool load(const char* path);
    bool save(const char* path) const;

    // Assigning a code clears it from any other action on the same device,
    // so two actions never share a key by accident.
    void setKey(Action a, int code);
    void setPad(Action a, int code);

private:
    ActionBinding b_[kActionCount];
};

} // namespace sb
