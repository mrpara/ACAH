// audio.h - the synthesiser.
//
// Every sound in the game is generated here from oscillators, noise and
// envelopes. Nothing is loaded from disk. That keeps the executable
// self-contained, and a bank of square waves, detuned saws and filtered noise
// is the right instrument for a game drawn in characters anyway.
//
// A voice is a short recipe rather than a buffer: it holds phase, an envelope
// and a few parameters, and is mixed a sample at a time. Sixteen of them run at
// once, which is far more than a frame ever needs.
#pragma once

#include <SDL3/SDL.h>

#include <cstdint>

#include "../core/audio.h"
#include "../core/math3d.h"   // clampf, and nothing else

namespace sb {

class AudioEngine {
public:
    bool init(const char** errorOut);
    void shutdown();
    bool running() const { return stream_ != nullptr; }

    // Drains a frame of events and updates the music state.
    void submit(const AudioQueue& queue);

    void setSfxVolume(float v);
    void setMusicVolume(float v);
    float sfxVolume() const { return sfxGain_; }
    float musicVolume() const { return musicGain_; }
    void toggleMute();
    bool muted() const { return muted_; }

    // Offline render, for verifying the bank without an audio device. Fills
    // `out` with interleaved stereo float. Nothing in the game calls this.
    void renderForTest(float* out, int frames) { mix(out, frames); }
    void triggerForTest(const SoundEvent& e) { trigger(e); }
    void setIntensityForTest(float v) { intensityTarget_ = clampf(v, 0.0f, 1.0f); }
    int musicStepForTest() const { return musicStep_; }
    void setMusicStateForTest(int style, bool boss, bool combat) {
        style_ = style; boss_ = boss; inCombat_ = combat;
    }
    int activeMusicVoicesForTest() const {
        int n = 0;
        for (int i = 0; i < kMusicVoices; ++i) n += voices_[i].active ? 1 : 0;
        return n;
    }

    // Live diagnostics, readable from the game thread without locking (the
    // values are monotonic or smoothed; a torn read is harmless). These are
    // how "the sound keeps cutting out" stops being a guessing game: if
    // underruns() climbs, the DEVICE is starving (scheduling, buffer size);
    // if limiterFloor() dives, the MIX is clipping its own headroom.
    int activeVoices() const { return activeVoices_; }
    float limiterFloor() const { return limFloor_; }   // worst recent gain, 1 = clean
    int underruns() const { return underruns_; }       // feed gaps > 40 ms

private:
    // --------------------------------------------------------------- voices --
    enum class Wave : int { Sine = 0, Square, Saw, Triangle, Noise };

    struct Voice {
        bool active = false;
        Wave wave = Wave::Square;
        float phase = 0.0f;
        float freq = 220.0f;
        float freqTarget = 220.0f;   // glide destination, for sweeps
        float glide = 0.0f;          // per-sample approach rate
        float amp = 0.0f;
        float attack = 0.001f;       // seconds
        float decay = 0.15f;
        float sustain = 0.0f;        // level, 0..1
        float release = 0.05f;
        float t = 0.0f;              // time since trigger
        float held = 0.0f;           // how long the sustain lasts
        float pan = 0.0f;
        float lowpass = 1.0f;        // 1 = open, smaller = duller
        float lpState = 0.0f;
        float noiseSeed = 1.0f;
        bool music = false;          // mixed under the music bus
        int kind = -1;               // Sfx enum value, -1 for music voices
    };

    Voice* allocVoice(bool music = false, float incomingAmp = 1e9f);
    void trigger(const SoundEvent& e);
    void renderMusic(float dt);
    void mix(float* out, int frames);

    static void SDLCALL feed(void* userdata, SDL_AudioStream* stream,
                             int additional, int total);

    SDL_AudioStream* stream_ = nullptr;
    static constexpr int kRate = 44100;
    // Partitioned pool: the first block is reserved for the score, the rest
    // for effects. A firefight's worth of per-shot sfx used to steal the
    // music's sustained pads mid-note - the reported "sound keeps cutting
    // out" - because allocation stole the OLDEST voice, and nothing lives
    // longer than a pad.
    static constexpr int kMusicVoices = 26;
    static constexpr int kVoices = 80;
    Voice voices_[kVoices];

    // Gain staging: the score sits UP with the effects now - "no music
    // during missions" was the music bus 25 dB under a firefight. The sfx
    // bus and the per-recipe amps are set with real HEADROOM: a busy
    // exchange of fire must sum BELOW the limiter threshold, because a
    // limiter that engages on every chaingun snap modulates the entire mix
    // at the gun's rate - which is indistinguishable from "the sound keeps
    // cutting out whenever several things play".
    // Nudged up with the score: music went from 0.86 to 0.92 and the guns were
    // starting to sit underneath it. The point of a louder score is drama, not
    // burying the thing the player is actually doing.
    float sfxGain_ = 0.43f;
    // Louder, on request. Peak measured 0.50 against a limiter that does not
    // start working until 0.90, so this is spending headroom that was simply
    // sitting there - not squeezing the mix.
    float musicGain_ = 0.92f;
    bool muted_ = false;
    // Master peak limiter state: instant attack, slow release, and meant to
    // be a rare SAFETY, not a compressor that lives on the signal.
    float limEnv_ = 0.0f;
    // Diagnostics (see accessors above).
    int activeVoices_ = 0;
    float limFloor_ = 1.0f;
    int underruns_ = 0;
    uint64_t lastFeedNs_ = 0;

    // --------------------------------------------------------------- music --
    // A slow four-bar loop: a bass pulse on the root, a sparse arpeggio above
    // it, and a hat that only shows up once the shooting starts.
    float musicPhase_ = 0.0f;   // fractional progress into the next step
    int musicStep_ = -1;
    int musicBar_ = 0;
    float intensity_ = 0.0f;
    float intensityTarget_ = 0.0f;
    bool inCombat_ = false;
    int style_ = 0;             // which flavour is scoring the mission
    bool boss_ = false;
    uint32_t rng_ = 0x1337u;

    float frand();
};

} // namespace sb
