#include "bindings.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace sb {

namespace {

// USB HID scancodes, as SDL numbers them.
constexpr int kScanA = 4, kScanD = 7, kScanE = 8, kScanF = 9, kScanH = 11, kScanM = 16,
              kScanQ = 20, kScanR = 21, kScanS = 22, kScanW = 26, kScanX = 27, kScanZ = 29,
              kScan1 = 30, kScan2 = 31, kScan3 = 32, kScan4 = 33, kScan5 = 34, kScan6 = 35,
              kScan7 = 36, kScan8 = 37, kScanSpace = 44, kScanLShift = 225;
// SDL_GamepadButton indices.
constexpr int kPadSouth = 0, kPadEast = 1, kPadWest = 2, kPadNorth = 3, kPadBack = 4,
              kPadRightStick = 8, kPadLeftShoulder = 9, kPadRightShoulder = 10,
              kPadDpadUp = 11, kPadDpadDown = 12, kPadDpadLeft = 13, kPadDpadRight = 14,
              kPadTouchpad = 20;
constexpr int kAxisLeftTrigger = 4, kAxisRightTrigger = 5;

const char* const kNames[kActionCount] = {
    "FORWARD", "BACK", "STRAFE LEFT", "STRAFE RIGHT", "BOOST", "JUMP",
    "FIRE GROUP L", "FIRE GROUP R",
    "LEGS SYSTEM", "ENGINE SYSTEM", "ARMOUR SYSTEM", "SENSOR SYSTEM",
    "GUNSIGHT", "CABIN / EXTERNAL VIEW",
    "TOGGLE GUN 1", "TOGGLE GUN 2", "TOGGLE GUN 3", "TOGGLE GUN 4",
    "GUN 1 GROUP L/R", "GUN 2 GROUP L/R", "GUN 3 GROUP L/R", "GUN 4 GROUP L/R",
    "HUD ON/OFF", "MUTE",
};

} // namespace

bool actionIsHeld(Action a) {
    switch (a) {
        case Action::Forward: case Action::Back: case Action::StrafeLeft:
        case Action::StrafeRight: case Action::Boost: case Action::Jump:
        case Action::FireLeft: case Action::FireRight:
            return true;
        default:
            return false;
    }
}

const char* actionName(Action a) {
    const int i = static_cast<int>(a);
    return (i >= 0 && i < kActionCount) ? kNames[i] : "?";
}

std::string defaultKeyName(int code) {
    if (code == kCodeNone) return "--";
    if (code >= kMouseCodeBase) {
        const int b = code - kMouseCodeBase;
        return b == 1 ? "LMB" : b == 2 ? "MMB" : b == 3 ? "RMB" : "MB" + std::to_string(b);
    }
    // USB HID usage codes, which is what SDL numbers scancodes by.
    if (code >= 4 && code <= 29) return std::string(1, static_cast<char>('A' + code - 4));
    if (code >= 30 && code <= 38) return std::string(1, static_cast<char>('1' + code - 30));
    if (code == 39) return "0";
    if (code >= 58 && code <= 69) return "F" + std::to_string(code - 57);
    switch (code) {
        case 40: return "ENTER";
        case 41: return "ESC";
        case 42: return "BKSP";
        case 43: return "TAB";
        case 44: return "SPACE";
        case 45: return "-";
        case 46: return "=";
        case 47: return "[";
        case 48: return "]";
        case 49: return "\\";
        case 51: return ";";
        case 52: return "'";
        case 53: return "`";
        case 54: return ",";
        case 55: return ".";
        case 56: return "/";
        case 57: return "CAPS";
        case 74: return "HOME";
        case 75: return "PGUP";
        case 76: return "DEL";
        case 77: return "END";
        case 78: return "PGDN";
        case 79: return "RIGHT";
        case 80: return "LEFT";
        case 81: return "DOWN";
        case 82: return "UP";
        case 224: return "LCTRL";
        case 225: return "SHIFT";
        case 226: return "ALT";
        case 227: return "LGUI";
        case 228: return "RCTRL";
        case 229: return "RSHIFT";
        case 230: return "RALT";
        default: break;
    }
    return "K" + std::to_string(code);
}

std::string defaultPadName(int code) {
    if (code == kCodeNone) return "--";
    if (code >= kPadAxisBase) {
        const int axis = code - kPadAxisBase;
        return axis == 4 ? "LT" : axis == 5 ? "RT" : "AX" + std::to_string(axis);
    }
    switch (code) {
        case 0: return "A";
        case 1: return "B";
        case 2: return "X";
        case 3: return "Y";
        case 4: return "BACK";
        case 5: return "GUIDE";
        case 6: return "START";
        case 7: return "LS";
        case 8: return "RS";
        case 9: return "LB";
        case 10: return "RB";
        case 11: return "D-UP";
        case 12: return "D-DN";
        case 13: return "D-LF";
        case 14: return "D-RT";
        case 20: return "TOUCH";
        default: break;
    }
    return "P" + std::to_string(code);
}

void Bindings::reset() {
    for (ActionBinding& b : b_) b = ActionBinding{};
    at(Action::Forward)       = {kScanW, kCodeNone};
    at(Action::Back)          = {kScanS, kCodeNone};
    at(Action::StrafeLeft)    = {kScanA, kCodeNone};
    at(Action::StrafeRight)   = {kScanD, kCodeNone};
    at(Action::Boost)         = {kScanLShift, kCodeNone};
    at(Action::Jump)          = {kScanSpace, kPadSouth};
    at(Action::FireLeft)      = {kMouseCodeBase + 1, kPadAxisBase + kAxisLeftTrigger};
    at(Action::FireRight)     = {kMouseCodeBase + 3, kPadAxisBase + kAxisRightTrigger};
    at(Action::AbilityLegs)   = {kScanQ, kPadLeftShoulder};
    at(Action::AbilityEngine) = {kScanE, kPadRightShoulder};
    at(Action::AbilityArmor)  = {kScanR, kPadWest};
    at(Action::AbilitySensor) = {kScanF, kPadEast};
    at(Action::Scope)         = {kScanZ, kPadNorth};
    at(Action::ToggleView)    = {kScanX, kPadRightStick};
    at(Action::Mount1)        = {kScan1, kPadDpadUp};
    at(Action::Mount2)        = {kScan2, kPadDpadRight};
    at(Action::Mount3)        = {kScan3, kPadDpadDown};
    at(Action::Mount4)        = {kScan4, kPadDpadLeft};
    at(Action::Group1)        = {kScan5, kCodeNone};
    at(Action::Group2)        = {kScan6, kCodeNone};
    at(Action::Group3)        = {kScan7, kCodeNone};
    at(Action::Group4)        = {kScan8, kCodeNone};
    at(Action::ToggleHud)     = {kScanH, kPadTouchpad};
    at(Action::Mute)          = {kScanM, kPadBack};
}

void Bindings::setKey(Action a, int code) {
    if (code != kCodeNone)
        for (ActionBinding& b : b_) if (b.key == code) b.key = kCodeNone;
    at(a).key = code;
}

void Bindings::setPad(Action a, int code) {
    if (code != kCodeNone)
        for (ActionBinding& b : b_) if (b.pad == code) b.pad = kCodeNone;
    at(a).pad = code;
}

bool Bindings::save(const char* path) const {
    FILE* f = std::fopen(path, "w");
    if (!f) return false;
    std::fprintf(f, "# ACAH controls. One line per action: index key pad (-1 = unbound).\n");
    for (int i = 0; i < kActionCount; ++i)
        std::fprintf(f, "%d %d %d # %s\n", i, b_[i].key, b_[i].pad, kNames[i]);
    std::fclose(f);
    return true;
}

bool Bindings::load(const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) return false;
    char line[256];
    int loaded = 0;
    while (std::fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        int i = 0, key = 0, pad = 0;
        if (std::sscanf(line, "%d %d %d", &i, &key, &pad) == 3 && i >= 0 && i < kActionCount) {
            b_[i].key = key;
            b_[i].pad = pad;
            ++loaded;
        }
    }
    std::fclose(f);
    return loaded > 0;
}

} // namespace sb
