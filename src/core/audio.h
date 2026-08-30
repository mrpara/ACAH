// audio.h - what the game asks to be heard.
//
// The core never makes a sound itself; it appends events to a queue and the
// platform layer synthesises them. Same split as everything else here: this
// header knows nothing about SDL, so the whole game still runs and can be
// tested with no audio device at all.
//
// Nothing is sampled. Every sound is generated from oscillators and noise at
// run time, which suits a game drawn in characters - and means there are no
// audio assets to ship alongside a single self-contained executable.
#pragma once

#include <cstdint>
#include <vector>

namespace sb {

enum class Sfx : int {
    FireLight = 0,   // autocannon, carbine: a short dry crack
    FireChain,       // high-rate chaingun: a hard metallic snap per round -
                     // retriggered at the gun's own rate it becomes the buzz
    FireMedium,      // lance, flak: a heavier thump with a tail
    FireHeavy,       // railgun, siege: a long descending boom
    FireBeam,        // arclight: a sustained buzz, retriggered while held
    ImpactMech,      // a round landing on a machine
    ImpactWorld,     // a round landing on scenery
    Explosion,
    Footfall,
    Servo,           // turret traverse: a quiet motor blip while slewing
    EngineHum,       // reactor under load: a low layered drone while moving
    JumpCharge,
    JumpLaunch,
    Land,
    HullHit,         // you took damage
    Overheat,
    AbilityUse,
    Climb,           // limbs biting into a wall
    Slip,            // grip failing
    Destroy,         // a machine coming apart
    PickupAmmo,
    PickupRepair,    // amber salvage: field repair, and it should not sound
                     // like ammunition - it is the only healing in a mission
    ObjectiveDone,   // a segment closed out: the clearest "that worked" the
                     // game has, and it had no sound at all
    IncomingHeavy,   // a rocket or tank round launched at YOU: the warning
                     // that makes dodging a skill rather than a lottery
    EmpDischarge,    // your reactor dumping itself into the air
    RepairPulse,     // a Warden putting armour back on something you are
                     // trying to kill - the sound that says "wrong target"
    JamHum,          // the bubble you are standing in, so the degraded HUD
                     // has something audible behind it
    UiMove,
    UiConfirm,
    UiDeny,
    MissionStart,
    MissionWin,
    MissionFail,
    Count
};

struct SoundEvent {
    Sfx kind = Sfx::UiMove;
    float gain = 1.0f;      // 0..1, usually distance-attenuated by the caller
    float pitch = 1.0f;     // multiplier on the sound's base frequency
    float pan = 0.0f;       // -1 left, +1 right
};

// A frame's worth of sound requests. The game fills it; the platform drains it.
class AudioQueue {
public:
    void push(Sfx kind, float gain = 1.0f, float pitch = 1.0f, float pan = 0.0f) {
        if (events_.size() >= 64) return;      // a frame never needs more
        SoundEvent e;
        e.kind = kind;
        e.gain = gain;
        e.pitch = pitch;
        e.pan = pan;
        events_.push_back(e);
    }
    const std::vector<SoundEvent>& events() const { return events_; }
    void clear() { events_.clear(); }

    // Music intent, updated each frame. The platform reads it to decide what
    // the soundtrack should be doing.
    void setIntensity(float v) { intensity_ = v; }
    float intensity() const { return intensity_; }
    void setInCombat(bool v) { inCombat_ = v; }
    bool inCombat() const { return inCombat_; }
    // Which flavour scores this mission: 0 dark synthwave / industrial,
    // 1 breakbeat, 2 brooding ambient techno, 3 hybrid orchestral. Boss
    // missions push the same style harder and darker.
    void setMusicStyle(int style) { musicStyle_ = style; }
    int musicStyle() const { return musicStyle_; }
    void setBossFight(bool v) { bossFight_ = v; }
    bool bossFight() const { return bossFight_; }

private:
    std::vector<SoundEvent> events_;
    float intensity_ = 0.0f;   // 0 calm, 1 heaviest fighting
    bool inCombat_ = false;
    int musicStyle_ = 0;
    bool bossFight_ = false;
};

} // namespace sb
