#include "audio.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sb {

namespace {

constexpr float kTau = 6.283185307f;

// A minor pentatonic rooted low. Cyberpunk in the cheapest possible sense:
// a flat-five away from a plain minor scale, played slowly, with everything
// else left out.

float oscillate(int wave, float phase, float* noiseSeed) {
    switch (wave) {
        case 0: return std::sin(phase * kTau);
        case 1: return (phase < 0.5f) ? 1.0f : -1.0f;
        case 2: return phase * 2.0f - 1.0f;
        case 3: return 1.0f - 4.0f * std::fabs(phase - 0.5f);
        default: {
            // A cheap deterministic noise: xorshift on a float-held integer.
            uint32_t s = static_cast<uint32_t>(*noiseSeed);
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            *noiseSeed = static_cast<float>(s ? s : 1u);
            return (static_cast<float>(s >> 8) * (1.0f / 8388608.0f)) - 1.0f;
        }
    }
}

} // namespace

float AudioEngine::frand() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return (rng_ >> 8) * (1.0f / 16777216.0f);
}

bool AudioEngine::init(const char** errorOut) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        if (errorOut) *errorOut = SDL_GetError();
        return false;
    }
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = kRate;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;

    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                        &AudioEngine::feed, this);
    if (!stream_) {
        if (errorOut) *errorOut = SDL_GetError();
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

void AudioEngine::shutdown() {
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
}

void AudioEngine::setSfxVolume(float v) { sfxGain_ = clampf(v, 0.0f, 1.0f); }
void AudioEngine::setMusicVolume(float v) { musicGain_ = clampf(v, 0.0f, 1.0f); }
void AudioEngine::toggleMute() { muted_ = !muted_; }

AudioEngine::Voice* AudioEngine::allocVoice(bool music, float incomingAmp) {
    // Music and effects live in separate blocks of the pool: a burst of
    // gunfire can never steal a pad mid-note, and a wall of pads can never
    // silence the shot you just fired. Within a block, prefer a free slot;
    // a full block only ever steals DOWN - the voice with the least audible
    // energy left goes, and a new sound quieter than everything still
    // playing is dropped rather than swapped in over something louder.
    // (Stealing the oldest was the dropout: a distant pop could cut a
    // sustained boom or a held beam mid-note.)
    const int lo = music ? 0 : kMusicVoices;
    const int hi = music ? kMusicVoices : kVoices;
    Voice* best = nullptr;
    float bestLeft = 1e9f;
    for (int i = lo; i < hi; ++i) {
        Voice& v = voices_[i];
        if (!v.active) return &v;
        const float dur = std::max(v.attack + v.decay + v.held + v.release, 0.01f);
        const float left = v.amp * clampf(1.0f - v.t / dur, 0.0f, 1.0f);
        if (left < bestLeft) { bestLeft = left; best = &v; }
    }
    // With the per-kind caps upstream the block essentially never fills, so
    // never REFUSE a sound outright - a silently swallowed gunshot IS "the
    // sound cut out" to the player. Worst case we steal the least audible
    // voice, which nobody can hear ending.
    (void)incomingAmp;
    return best;
}

namespace {
// Polyphony per effect KIND. Ten overlapping copies of the same crack add
// nothing but rail energy; past the cap a new one recycles the most spent
// voice of its own kind, so the newest instance always plays and the pool
// never floods with one sound.
int polyCapFor(Sfx k) {
    // Generous: the pool holds 54 effect voices and the headroom pass means
    // stacking is cheap. The caps only exist to stop ONE spammy kind from
    // owning the pool - they must never truncate a chain-kill's worth of
    // explosion blooms (cap 5 was 2.5 explosions: the third kill audibly
    // cut the first boom short).
    switch (k) {
        case Sfx::FireChain:   return 10;  // main + tick voices share this
        case Sfx::FireLight:   return 8;
        case Sfx::FireMedium:  return 6;
        case Sfx::FireHeavy:   return 5;
        case Sfx::ImpactMech:  return 8;
        case Sfx::ImpactWorld: return 6;
        case Sfx::Explosion:   return 10;  // main + sub pairs: five blasts
        case Sfx::Footfall:    return 8;
        case Sfx::EngineHum:   return 4;
        case Sfx::Servo:       return 3;
        // Warnings are information, not texture: two at once is a volley, six
        // at once is a wall of noise that tells you nothing.
        case Sfx::IncomingHeavy: return 2;
        case Sfx::EmpDischarge:  return 2;
        case Sfx::RepairPulse:   return 3;
        case Sfx::JamHum:        return 2;
        case Sfx::ObjectiveDone: return 1;
        default:               return 6;
    }
}
} // namespace

void AudioEngine::trigger(const SoundEvent& e) {
    Voice* v = nullptr;
    {
        // Enforce the per-kind cap before touching the shared pool.
        const int cap = polyCapFor(e.kind);
        int count = 0;
        Voice* spent = nullptr;
        float spentLeft = 1e9f;
        for (int i = kMusicVoices; i < kVoices; ++i) {
            Voice& c = voices_[i];
            if (!c.active || c.kind != static_cast<int>(e.kind)) continue;
            ++count;
            const float dur = std::max(c.attack + c.decay + c.held + c.release, 0.01f);
            const float left = c.amp * clampf(1.0f - c.t / dur, 0.0f, 1.0f);
            if (left < spentLeft) { spentLeft = left; spent = &c; }
        }
        v = (count >= cap) ? spent
                           : allocVoice(false, clampf(e.gain, 0.0f, 1.0f) * 0.5f);
    }
    if (!v) return;
    *v = Voice{};
    v->kind = static_cast<int>(e.kind);
    v->active = true;
    v->pan = clampf(e.pan, -1.0f, 1.0f);
    v->noiseSeed = static_cast<float>((rng_ = rng_ * 1664525u + 1013904223u) | 1u);
    const float p = (e.pitch > 0.01f) ? e.pitch : 1.0f;
    const float g = clampf(e.gain, 0.0f, 1.0f);

    switch (e.kind) {
        case Sfx::FireLight:
            // A dry crack: a fast square blip over a noise transient.
            v->wave = Wave::Square;
            v->freq = 420.0f * p; v->freqTarget = 130.0f * p; v->glide = 0.0025f;
            v->amp = 0.26f * g; v->attack = 0.0006f; v->decay = 0.075f;
            v->lowpass = 0.55f;
            break;
        case Sfx::FireMedium:
            v->wave = Wave::Saw;
            v->freq = 260.0f * p; v->freqTarget = 72.0f * p; v->glide = 0.0016f;
            v->amp = 0.34f * g; v->attack = 0.001f; v->decay = 0.20f;
            v->lowpass = 0.35f;
            break;
        case Sfx::FireHeavy:
            // A long descending boom with a lot of low end.
            v->wave = Wave::Triangle;
            v->freq = 180.0f * p; v->freqTarget = 34.0f * p; v->glide = 0.0007f;
            v->amp = 0.44f * g; v->attack = 0.002f; v->decay = 0.55f;
            v->lowpass = 0.22f;
            break;
        case Sfx::FireBeam: {
            // A held beam is ONE continuous voice. The game retriggers this
            // event at the weapon's rate (an arclight: eighteen a second),
            // and spawning a fresh 150 ms voice each time had them endlessly
            // recycling each other - an audible 18 Hz chop that read as "the
            // sound keeps cutting out" in every beam fight. If a beam voice
            // is already live, RE-ARM it (extend its sustain, keep its
            // phase) and release the voice we just took.
            Voice* live = nullptr;
            for (int i = kMusicVoices; i < kVoices; ++i) {
                Voice& c = voices_[i];
                if (&c != v && c.active && c.kind == static_cast<int>(Sfx::FireBeam)) {
                    live = &c;
                    break;
                }
            }
            if (live) {
                live->t = std::min(live->t, live->attack + live->decay);
                live->amp = std::max(live->amp, 0.16f * g);
                live->pan = clampf(e.pan, -1.0f, 1.0f);
                live->freqTarget = 940.0f * p;
                v->active = false;
                return;
            }
            v->wave = Wave::Square;
            v->freq = 1180.0f * p; v->freqTarget = 940.0f * p; v->glide = 0.004f;
            v->amp = 0.16f * g; v->attack = 0.004f; v->decay = 0.04f;
            v->sustain = 0.7f; v->held = 0.16f; v->release = 0.07f;
            v->lowpass = 0.7f;
            break;
        }
        case Sfx::FireChain: {
            // One round of a high-rate gun: a hard mechanical snap. The game
            // retriggers this at the gun's own rate, and ten a second of these
            // IS the chaingun burr - no loop needed.
            v->wave = Wave::Square;
            v->freq = 940.0f * p; v->freqTarget = 250.0f * p; v->glide = 0.006f;
            v->amp = 0.22f * g; v->attack = 0.0004f; v->decay = 0.042f;
            v->lowpass = 0.62f;
            // The action cycling: a tick of bright noise on top.
            if (Voice* n = allocVoice(false, g * 0.3f)) {
                *n = Voice{};
                n->kind = static_cast<int>(e.kind);
                n->active = true; n->pan = v->pan;
                n->noiseSeed = static_cast<float>((rng_ = rng_ * 1664525u + 1013904223u) | 1u);
                n->wave = Wave::Noise;
                n->freq = 1.0f; n->amp = 0.10f * g;
                n->attack = 0.0003f; n->decay = 0.018f; n->lowpass = 0.9f;
            }
            break;
        }
        case Sfx::ImpactMech: {
            // A round on armour plate: a bright transient plus an actual
            // metallic RING - a high inharmonic tone that decays like a struck
            // hull, which is what sells "you hit metal" at any distance.
            v->wave = Wave::Noise;
            v->freq = 1.0f; v->amp = 0.24f * g;
            v->attack = 0.0004f; v->decay = 0.05f; v->lowpass = 0.85f;
            if (Voice* r = allocVoice(false, g * 0.3f)) {
                *r = Voice{};
                r->kind = static_cast<int>(e.kind);
                r->active = true; r->pan = v->pan;
                r->noiseSeed = static_cast<float>((rng_ = rng_ * 1664525u + 1013904223u) | 1u);
                r->wave = Wave::Triangle;
                r->freq = 2300.0f * p; r->freqTarget = 1900.0f * p; r->glide = 0.001f;
                r->amp = 0.20f * g; r->attack = 0.0006f; r->decay = 0.16f;
                r->lowpass = 0.9f;
            }
            break;
        }
        case Sfx::ImpactWorld:
            v->wave = Wave::Noise;
            v->freq = 1.0f; v->amp = 0.20f * g;
            v->attack = 0.0008f; v->decay = 0.09f; v->lowpass = 0.28f;
            break;
        case Sfx::Explosion: {
            v->wave = Wave::Noise;
            v->freq = 1.0f; v->amp = 0.46f * g;
            v->attack = 0.002f; v->decay = 0.75f; v->lowpass = 0.16f;
            // The pressure wave under the noise: a falling sub-tone that gives
            // the blast a chest thump the noise alone never had.
            if (Voice* b = allocVoice(false, g * 0.3f)) {
                *b = Voice{};
                b->kind = static_cast<int>(e.kind);
                b->active = true; b->pan = v->pan;
                b->noiseSeed = static_cast<float>((rng_ = rng_ * 1664525u + 1013904223u) | 1u);
                b->wave = Wave::Sine;
                b->freq = 88.0f * p; b->freqTarget = 26.0f * p; b->glide = 0.0012f;
                b->amp = 0.36f * g; b->attack = 0.002f; b->decay = 0.5f;
            }
            break;
        }
        case Sfx::Servo:
            // Traverse motor: a tiny pitched blip, almost felt not heard.
            v->wave = Wave::Triangle;
            v->freq = 620.0f * p; v->freqTarget = 540.0f * p; v->glide = 0.004f;
            v->amp = 0.055f * g; v->attack = 0.004f; v->decay = 0.10f;
            v->lowpass = 0.5f;
            break;
        case Sfx::EngineHum:
            // Reactor drone: overlapping low sines make a continuous bed.
            v->wave = Wave::Sine;
            v->freq = 52.0f * p; v->freqTarget = 48.0f * p; v->glide = 0.0008f;
            v->amp = 0.10f * g; v->attack = 0.06f; v->decay = 0.34f;
            break;
        case Sfx::Footfall:
            v->wave = Wave::Triangle;
            v->freq = 92.0f * p; v->freqTarget = 46.0f * p; v->glide = 0.006f;
            v->amp = 0.16f * g; v->attack = 0.001f; v->decay = 0.10f;
            v->lowpass = 0.30f;
            break;
        case Sfx::JumpCharge:
            // A rising whine while the legs load.
            v->wave = Wave::Saw;
            v->freq = 140.0f * p; v->freqTarget = 900.0f * p; v->glide = 0.00035f;
            v->amp = 0.20f * g; v->attack = 0.02f; v->decay = 0.10f;
            v->sustain = 0.85f; v->held = 0.42f; v->release = 0.06f;
            v->lowpass = 0.6f;
            break;
        case Sfx::JumpLaunch:
            v->wave = Wave::Square;
            v->freq = 620.0f * p; v->freqTarget = 190.0f * p; v->glide = 0.0022f;
            v->amp = 0.44f * g; v->attack = 0.001f; v->decay = 0.24f;
            v->lowpass = 0.5f;
            break;
        case Sfx::Land:
            v->wave = Wave::Triangle;
            v->freq = 120.0f * p; v->freqTarget = 38.0f * p; v->glide = 0.0035f;
            v->amp = 0.42f * g; v->attack = 0.001f; v->decay = 0.26f;
            v->lowpass = 0.20f;
            break;
        case Sfx::HullHit:
            // The one the player has to notice: a hard low clang.
            v->wave = Wave::Square;
            v->freq = 210.0f * p; v->freqTarget = 62.0f * p; v->glide = 0.0035f;
            v->amp = 0.55f * g; v->attack = 0.0005f; v->decay = 0.22f;
            v->lowpass = 0.34f;
            break;
        case Sfx::Overheat:
            v->wave = Wave::Saw;
            v->freq = 780.0f; v->freqTarget = 210.0f; v->glide = 0.0009f;
            v->amp = 0.34f * g; v->attack = 0.004f; v->decay = 0.5f;
            v->lowpass = 0.45f;
            break;
        case Sfx::AbilityUse:
            v->wave = Wave::Square;
            v->freq = 300.0f * p; v->freqTarget = 1250.0f * p; v->glide = 0.0016f;
            v->amp = 0.34f * g; v->attack = 0.002f; v->decay = 0.22f;
            v->lowpass = 0.8f;
            break;
        case Sfx::Climb:
            v->wave = Wave::Noise;
            v->freq = 1.0f; v->amp = 0.13f * g;
            v->attack = 0.002f; v->decay = 0.13f; v->lowpass = 0.5f;
            break;
        case Sfx::Slip:
            v->wave = Wave::Noise;
            v->freq = 1.0f; v->amp = 0.26f * g;
            v->attack = 0.001f; v->decay = 0.24f; v->lowpass = 0.85f;
            break;
        case Sfx::Destroy:
            v->wave = Wave::Noise;
            v->freq = 1.0f; v->amp = 0.85f * g;
            v->attack = 0.003f; v->decay = 1.15f; v->lowpass = 0.13f;
            break;
        case Sfx::PickupAmmo:
            v->wave = Wave::Square;
            v->freq = 880.0f; v->freqTarget = 1760.0f; v->glide = 0.004f;
            v->amp = 0.24f * g; v->attack = 0.002f; v->decay = 0.13f;
            break;
        case Sfx::PickupRepair:
            // Warm and round where ammunition is bright and thin, so the two
            // crate types are told apart with the ears alone.
            v->wave = Wave::Sine;
            v->freq = 392.0f; v->freqTarget = 784.0f; v->glide = 0.0022f;
            v->amp = 0.30f * g; v->attack = 0.004f; v->decay = 0.42f;
            v->lowpass = 0.5f;
            break;
        case Sfx::ObjectiveDone:
            // A two-note confirm that sits above the score: the sound of the
            // contract ticking over. Nothing else in the game uses this
            // interval, which is what makes it legible under fire.
            v->wave = Wave::Square;
            v->freq = 587.0f; v->freqTarget = 1175.0f; v->glide = 0.0016f;
            v->amp = 0.34f * g; v->attack = 0.003f; v->decay = 0.55f;
            v->lowpass = 0.55f;
            break;
        case Sfx::IncomingHeavy:
            // The rocket warning. Descending, close, and dry - it has to read
            // as "this one is for you" in under half a second, because that is
            // roughly how long there is to move.
            v->wave = Wave::Saw;
            v->freq = 1250.0f; v->freqTarget = 300.0f; v->glide = 0.0035f;
            v->amp = 0.26f * g; v->attack = 0.001f; v->decay = 0.30f;
            v->lowpass = 0.7f;
            break;
        case Sfx::EmpDischarge:
            // A big downward whoop with a lot of body: eleven megawatts
            // leaving at once should sound like it cost something.
            v->wave = Wave::Saw;
            v->freq = 900.0f; v->freqTarget = 48.0f; v->glide = 0.0016f;
            v->amp = 0.52f * g; v->attack = 0.002f; v->decay = 0.85f;
            v->lowpass = 0.30f;
            break;
        case Sfx::RepairPulse:
            // Deliberately unlike anything the player's own kit makes: a
            // rising two-tone tick from the enemy side of the field. If you
            // keep hearing it, something out there is undoing your work.
            v->wave = Wave::Triangle;
            v->freq = 300.0f; v->freqTarget = 520.0f; v->glide = 0.0030f;
            v->amp = 0.20f * g; v->attack = 0.004f; v->decay = 0.30f;
            v->lowpass = 0.45f;
            break;
        case Sfx::JamHum:
            // A low unstable drone under the HUD warning. Retriggered while
            // you are inside the bubble, so it reads as a place rather than
            // an event.
            v->wave = Wave::Saw;
            v->freq = 74.0f * (0.94f + 0.12f * p); v->amp = 0.13f * g;
            v->attack = 0.15f; v->decay = 1.05f; v->lowpass = 0.12f;
            break;
        case Sfx::UiMove:
            v->wave = Wave::Square;
            v->freq = 660.0f; v->amp = 0.16f * g;
            v->attack = 0.001f; v->decay = 0.045f;
            break;
        case Sfx::UiConfirm:
            v->wave = Wave::Square;
            v->freq = 520.0f; v->freqTarget = 1040.0f; v->glide = 0.006f;
            v->amp = 0.24f * g; v->attack = 0.001f; v->decay = 0.11f;
            break;
        case Sfx::UiDeny:
            v->wave = Wave::Square;
            v->freq = 220.0f; v->freqTarget = 120.0f; v->glide = 0.006f;
            v->amp = 0.24f * g; v->attack = 0.001f; v->decay = 0.16f;
            break;
        case Sfx::MissionStart:
            v->wave = Wave::Saw;
            v->freq = 110.0f; v->freqTarget = 440.0f; v->glide = 0.0006f;
            v->amp = 0.34f * g; v->attack = 0.02f; v->decay = 0.7f;
            v->lowpass = 0.4f;
            break;
        case Sfx::MissionWin:
            v->wave = Wave::Square;
            v->freq = 440.0f; v->freqTarget = 880.0f; v->glide = 0.0012f;
            v->amp = 0.34f * g; v->attack = 0.006f; v->decay = 0.8f;
            break;
        case Sfx::MissionFail:
            v->wave = Wave::Saw;
            v->freq = 330.0f; v->freqTarget = 70.0f; v->glide = 0.0006f;
            v->amp = 0.40f * g; v->attack = 0.01f; v->decay = 1.4f;
            v->lowpass = 0.3f;
            break;
        default:
            v->active = false;
            break;
    }
}

void AudioEngine::submit(const AudioQueue& queue) {
    if (!stream_) return;
    SDL_LockAudioStream(stream_);
    for (const SoundEvent& e : queue.events()) trigger(e);
    intensityTarget_ = clampf(queue.intensity(), 0.0f, 1.0f);
    inCombat_ = queue.inCombat();
    style_ = queue.musicStyle();
    boss_ = queue.bossFight();
    SDL_UnlockAudioStream(stream_);
}

void AudioEngine::renderMusic(float dt) {
    // Slow approach so the track swells into a fight and settles out of it
    // rather than switching.
    intensity_ += (intensityTarget_ - intensity_) * clampf(dt * 0.5f, 0.0f, 1.0f);

    // Tempo is the first thing a style decides. Breakbeat runs hot; the
    // ambient flavour barely moves until the shooting starts; a boss mission
    // pushes every style a notch faster and darker.
    float stepTime;
    switch (style_) {
        case 1:  stepTime = 0.150f - 0.018f * intensity_; break;   // breakbeat
        case 2:  stepTime = 0.290f - 0.070f * intensity_; break;   // ambient
        case 3:  stepTime = 0.215f - 0.035f * intensity_; break;   // orchestral
        default: stepTime = 0.190f - 0.030f * intensity_; break;   // darksynth
    }
    if (boss_) stepTime *= 0.88f;

    // Tempo-safe step clock. The old code divided an ABSOLUTE clock by the
    // CURRENT step length; with tempo following intensity, every swell or
    // fade re-divided minutes of accumulated clock by a new denominator and
    // the step index leapt by DOZENS - bars skipped, notes dropped, the
    // score audibly cutting out whenever a fight started or ended, worse
    // the longer the mission ran. A phase accumulator advances exactly one
    // step at a time no matter what the tempo does.
    musicPhase_ += dt / stepTime;
    if (musicPhase_ < 1.0f) return;
    musicPhase_ -= std::floor(musicPhase_);   // a hitch never builds a backlog
    const int step = ++musicStep_;
    const int inBar = step % 16;
    if (inBar == 0) musicBar_ = (musicBar_ + 1) % 4;

    // One gain every music voice passes through, so a section can take the
    // whole arrangement down without four styles' worth of special cases in
    // every layer. The breakdown sets it; the breakdown's own furniture
    // restores it, because what is left in a breakdown should be at full
    // strength - that is the point of taking the rest out.
    float layerGain = 1.0f;
    auto voice = [&](Wave w, float f, float amp, float dec, float lp,
                     float pan = 0.0f) {
        Voice* v = allocVoice(true);
        if (!v) return;
        *v = Voice{};
        v->active = true;
        v->music = true;
        v->wave = w;
        v->freq = v->freqTarget = f;
        v->amp = amp * layerGain;
        v->attack = 0.008f;
        v->decay = dec;
        v->lowpass = lp;
        v->pan = pan;
        v->noiseSeed = static_cast<float>((rng_ = rng_ * 1664525u + 1013904223u) | 1u);
    };
    auto sweep = [&](Wave w, float f0, float f1, float amp, float dec, float lp) {
        Voice* v = allocVoice(true);
        if (!v) return;
        *v = Voice{};
        v->active = true;
        v->music = true;
        v->wave = w;
        v->freq = f0;
        v->freqTarget = f1;
        // Per-sample approach factor. The old value here was 3.0 - a
        // DIVERGENT filter (error times -2 every sample): the frequency hit
        // infinity three milliseconds in and every sine sweep - the kicks,
        // the TIMPANI - fed NaN into the OS audio stream, which answers
        // with glitches and dropouts. Mission 5 is the first orchestral
        // mission; its timpani are sine sweeps on every downbeat. That was
        // "sound works fine until mission 5".
        v->glide = 0.006f;
        v->amp = amp * layerGain;
        v->attack = 0.010f;
        v->decay = dec;
        v->lowpass = lp;
        v->noiseSeed = static_cast<float>((rng_ = rng_ * 1664525u + 1013904223u) | 1u);
    };

    // ------------------------------------------------------------------
    // The score. Not a pattern loop: a 32-bar song form in A minor with a
    // real chord progression, a lead line phrased in call and answer, and
    // an arrangement that builds by section and by how the fight is going.
    //
    //   bars  0-7   INTRO   bass and pads, drums breathing in
    //   bars  8-23  MAIN    full groove, melody enters at bar 8
    //   bars 24-31  LIFT    subdominant turn, melody up the octave,
    //                       counter-line underneath, everything open
    //
    // Boss missions transpose the whole form down three semitones and play
    // the LIFT arrangement from the door.
    // ------------------------------------------------------------------
    const float in2 = intensity_;
    const int bar = (step / 16) % 32;
    // The form: INTRO gathers for four bars, MAIN grooves for twelve, the
    // RISE tightens the screw for eight - climbing harmony, hats closing up,
    // a riser through the last two bars - and the LIFT is the release it was
    // all pointing at. Boss missions live in the LIFT from the door.
    const bool lift = bar >= 24;
    const bool rise = bar >= 16 && bar < 24;
    // A boss fight skips the quiet opening but still MOVES through the form.
    // Pinning it to the LIFT arrangement was why mission five looped the
    // same eight bars for the whole fight.
    const bool intro = !boss_ && bar < 4 && in2 < 0.55f;
    // The BREAKDOWN. Four bars where the arrangement falls away to bass and
    // kick before the RISE starts climbing - the piece had a build and a
    // release but nothing that made room for them, and a wall of sound that
    // never stops is a wall, not a drama. Taking everything out for four bars
    // is what makes the eight bars that follow sound like they are gathering.
    const bool drop = !boss_ && bar >= 12 && bar < 16;
    const int barPhase = bar % 8;
    const bool fillBar = (bar % 4) == 3;      // a turn every four bars now
    // How far through the RISE, 0..1: the tension knob everything reads.
    const float riseT = rise ? ((bar - 16) + inBar / 16.0f) / 8.0f : 0.0f;
    // Long-cycle variation. `pass` counts complete trips through the 32-bar
    // form, and `colour` shifts every eight bars. Together they decide
    // octave displacements, whether the answer phrase inverts, and which
    // fills fire - so the score keeps developing instead of looping.
    const int pass = (step / 16) / 32;
    const int colour = ((step / 16) / 8) & 3;
    const bool altVoicing = (colour == 1 || colour == 3);
    const bool bigFill = ((pass + colour) & 1) != 0;

    // Equal temperament off A1. Everything below speaks in semitones.
    auto hz = [](int semis) { return 55.0f * std::pow(2.0f, semis / 12.0f); };

    // Chord degrees in A minor: {root semitone, major third?}.
    struct Chord { int root; bool major; };
    static const Chord kMainProg[4][8] = {
        // darksynth: brooding i-VI-VII vamp with a III turn
        {{0,false},{0,false},{8,true},{10,true},{0,false},{0,false},{3,true},{10,true}},
        // breakbeat: driving i-VII-VI-VII, minor v on the turn
        {{0,false},{10,true},{8,true},{10,true},{0,false},{10,true},{8,true},{7,false}},
        // ambient: slow i...VI...III...VII, two bars a chord
        {{0,false},{0,false},{8,true},{8,true},{3,true},{3,true},{10,true},{10,true}},
        // orchestral: harmonic-minor drama, i-VI-iv-V
        {{0,false},{8,true},{5,false},{7,true},{0,false},{8,true},{3,true},{7,true}},
    };
    static const Chord kLiftProg[4][8] = {
        {{5,false},{5,false},{8,true},{10,true},{0,false},{10,true},{8,true},{7,true}},
        {{5,false},{3,true},{8,true},{7,true},{5,false},{3,true},{10,true},{7,true}},
        {{5,false},{5,false},{8,true},{8,true},{0,false},{0,false},{7,true},{7,true}},
        {{5,false},{0,false},{8,true},{10,true},{0,false},{7,true},{0,false},{7,true}},
    };
    const int styleRow = (style_ >= 0 && style_ < 4) ? style_ : 0;
    // The RISE walks i - III - iv - V, two bars a chord: the most literal
    // ascent to the dominant there is, resolving hard into the LIFT.
    static const Chord kRiseProg[8] = {
        {0,false},{0,false},{3,true},{3,true},{5,false},{5,false},{7,true},{7,true}};
    const Chord chord = rise ? kRiseProg[barPhase]
                             : (lift ? kLiftProg : kMainProg)[styleRow][barPhase];
    // Modulation by pass. A long fight can run the 32-bar form four or five
    // times, and the oldest trick in popular music is to shift the key up for
    // the last time round. Passes go A minor, A minor, B minor, D minor: the
    // same score, climbing, so a twenty-minute contract never settles.
    static const int kPassKey[4] = {0, 0, 2, 5};
    const int transpose = (boss_ ? -3 : 0) + kPassKey[pass & 3];
    const int cRoot = chord.root + transpose;
    const int cThird = cRoot + (chord.major ? 4 : 3);
    const int cFifth = cRoot + 7;

    // The lead line: two-bar phrases in scale degrees (-1 = rest), a call and
    // an answer, per style. Degrees index the A natural minor scale and are
    // transposed onto the current chord root so the line follows the harmony.
    static const int kMinor[7] = {0, 2, 3, 5, 7, 8, 10};
    static const int kCall[4][16] = {
        {4,-1,3,-1, 2,-1,4,5, 4,-1,3,2, 4,-1,5,6},          // darksynth
        {0,2,-1,4, 2,-1,5,4, 0,2,4,5, 6,5,4,2},             // breakbeat
        {-1,-1,4,-1, -1,3,-1,-1, 4,-1,5,-1, 3,-1,-1,-1},    // ambient
        {0,-1,2,3, 4,-1,3,2, 4,5,-1,4, 3,-1,2,-1},          // orchestral
    };
    static const int kAnswer[4][16] = {
        {2,-1,1,-1, 0,-1,2,3, 2,-1,1,0, 2,1,0,-1},
        {4,5,-1,6, 5,4,2,-1, 2,4,0,2, 4,-1,2,0},
        {5,-1,-1,4, -1,-1,3,-1, 4,-1,2,-1, 0,-1,-1,-1},
        {5,-1,4,3, 4,-1,3,2, 2,3,1,2, 0,-1,-1,-1},
    };
    // The RISE motif: one climbing figure for every style, sequenced up the
    // scale bar over bar so the line itself ratchets.
    static const int kRiseLine[16] = {0,-1,2,-1, 3,-1,4,-1, 2,-1,3,-1, 4,-1,5,-1};

    const float root = hz(cRoot);

    // ----------------------------------------------------- the layers --
    // Every style shares the same score; what differs is the instrument the
    // layer is voiced on and the rhythm it speaks in.

    // Melody realisation: pick this bar's phrase, put it on the chord.
    const int* phrase = rise ? kRiseLine : ((bar & 1) ? kAnswer : kCall)[styleRow];
    int mdeg = phrase[inBar];
    // Sequence the RISE figure upward: every two bars lifts it a step.
    if (rise && mdeg >= 0) mdeg = std::min(mdeg + (bar - 16) / 2, 6);
    const bool melodyOn = !intro && !drop &&
                          (in2 > 0.08f || lift || rise || inCombat_);
    // The LIFT takes the line up an octave; alternate colours displace it
    // again, so the same phrase returns in a different register.
    const int melOct = (lift ? 24 : 12) + (altVoicing ? 12 : 0);
    // On later passes the answer phrase inverts around the fifth, which is
    // the cheapest way to make a returning phrase sound like a development
    // of itself rather than a repeat.
    const bool invert = (pass & 1) && (bar & 1);
    if (invert && mdeg >= 0)
        mdeg = static_cast<int>(clampf(static_cast<float>(4 - mdeg), 0.0f, 6.0f));
    // A harmony voice shadows the lead a third below once things are hot.
    const bool harmonyOn = melodyOn && mdeg >= 2 && (lift || in2 > 0.45f);

    // Countermelody: the answer phrase played against the call, low and late.
    const int cdeg = ((bar & 1) ? kCall : kAnswer)[styleRow][(inBar + 8) % 16];

    // The groove floor: post-intro, every style except ambient runs a real
    // rhythm section - a four-on-the-floor kick and driving hats - and every
    // layer above it scales with the fight. Calm is thin and wide; combat is
    // a wall. The CONTRAST is the drama.
    const bool grooveOn = !intro && style_ != 2;
    float energy = clampf(in2 + riseT * 0.6f + (lift ? 0.35f : 0.0f),
                          0.0f, 1.3f);
    // Everything above the rhythm section scales with energy, so pulling it
    // down for the breakdown thins hats, stabs, arps and pads in one stroke
    // without four styles' worth of special cases.
    if (drop) energy *= 0.30f;
    // And the arrangement itself steps back a good eight decibels. A dip of a
    // decibel and a half is not a breakdown, it is a mixing error: the bass
    // and the kick were carrying the loudness and neither of them read the
    // energy knob, so the "breakdown" measured louder than the intro.
    if (drop) layerGain = 0.38f;
    // The other half of the same idea: the climb and the payoff push the whole
    // arrangement forward, not just the layers that read the energy knob. The
    // LIFT should be unmistakably the loudest thing in the piece - it is the
    // eight bars everything before it was buying.
    else if (lift) layerGain = 1.16f;
    else if (rise) layerGain = 1.0f + 0.10f * riseT;

    switch (style_) {
        // ------------------------------------------- 0: dark synthwave -----
        // A machine: four-on-the-floor kick, offbeat octave stabs, an eighth
        // bass line that walks chord tones, 16th arps opening their filter
        // with the fight, wide triad stabs, the lead doubled at the octave.
        default:
        case 0: {
            if (grooveOn && inBar % 4 == 0)
                sweep(Wave::Sine, 115.0f, 42.0f, 0.30f, 0.13f, 1.0f);
            // Offbeat octave stab - the pulse BETWEEN the kicks.
            if (grooveOn && inBar % 4 == 2)
                voice(Wave::Saw, hz(cRoot + 12), 0.11f + 0.05f * energy,
                      0.08f, 0.30f + 0.3f * energy);
            // Walking eighth bass: root, fifth, octave, seventh.
            static const int kBassPat[8] = {0, 0, 7, 0, 12, 0, 7, 10};
            if (inBar % 2 == 0) {
                const int semis = cRoot + kBassPat[(inBar / 2) % 8];
                voice(Wave::Saw, hz(semis), 0.20f + 0.07f * energy, 0.20f,
                      0.18f + 0.30f * energy);
                voice(Wave::Saw, hz(semis) * 1.007f, 0.13f, 0.20f,
                      0.18f + 0.30f * energy);
            }
            // Hats every offbeat 16th; open hat on the and-of-two.
            if (grooveOn && (inBar & 1))
                voice(Wave::Noise, 1.0f, 0.028f + 0.030f * energy, 0.022f, 0.95f,
                      (inBar % 4 == 1) ? -0.35f : 0.35f);
            if (grooveOn && inBar == 6)
                voice(Wave::Noise, 1.0f, 0.045f + 0.02f * energy, 0.09f, 0.8f);
            // Clap on the backbeat once the fight is on.
            if (in2 > 0.2f && (inBar == 4 || inBar == 12))
                voice(Wave::Noise, 1.0f, 0.10f + 0.04f * energy, 0.07f, 0.55f);
            // Triad stabs: downbeat, plus a syncopated answer at bar's end.
            if (inBar == 0 || (energy > 0.35f && inBar == 10)) {
                voice(Wave::Saw, hz(cRoot + 12), 0.11f, 0.7f, 0.38f, -0.4f);
                voice(Wave::Saw, hz(cThird + 12), 0.10f, 0.7f, 0.38f, 0.4f);
                voice(Wave::Saw, hz(cFifth + 12), 0.08f, 0.7f, 0.38f);
            }
            if (melodyOn && mdeg >= 0) {
                voice(Wave::Square, hz(cRoot + kMinor[mdeg] + melOct),
                      0.09f + 0.05f * in2, 0.30f, 0.45f + 0.3f * in2,
                      (bar & 1) ? 0.25f : -0.25f);
                // Octave double for thickness.
                voice(Wave::Square, hz(cRoot + kMinor[mdeg] + melOct + 12),
                      0.035f + 0.02f * energy, 0.22f, 0.55f,
                      (bar & 1) ? -0.15f : 0.15f);
                if (harmonyOn)
                    voice(Wave::Square, hz(cRoot + kMinor[mdeg - 2] + melOct),
                          0.050f, 0.26f, 0.4f, (bar & 1) ? -0.2f : 0.2f);
            }
            // The 16th arp: always running post-intro, filter riding energy.
            if (!intro && (inBar % 2 == 1)) {
                const int a = (inBar / 2) % 4;
                const int semis = (a == 0) ? cRoot : (a == 1) ? cThird
                                : (a == 2) ? cFifth : cRoot + 12;
                voice(Wave::Square, hz(semis + 24),
                      0.042f + 0.025f * energy, 0.09f,
                      0.45f + 0.40f * energy, (inBar % 4 < 2) ? -0.4f : 0.4f);
            }
            if (fillBar && inBar >= 12)
                sweep(Wave::Square, hz(cRoot + 24), hz(cRoot + 12),
                      0.05f, 0.18f, 0.6f);
            break;
        }
        // ----------------------------------------------- 1: breakbeat ------
        // The drums ARE the song, and they are twice as busy now: a broken
        // kick pattern with ghost snares, hats on every 16th, the bass
        // riffing eighths that go to sixteenths when the fight is real.
        case 1: {
            const bool kick = inBar == 0 || inBar == 3 || inBar == 6 ||
                              inBar == 10 || (energy > 0.5f && inBar == 13);
            if (kick) sweep(Wave::Sine, 120.0f, 45.0f, 0.32f, 0.13f, 1.0f);
            const bool snare = inBar == 4 || inBar == 12 ||
                               (energy > 0.4f && inBar == 15);
            if (snare) voice(Wave::Noise, 1.0f, 0.15f, 0.09f, 0.55f);
            // Ghost snares - the shuffle inside the beat.
            if (grooveOn && (inBar == 7 || inBar == 11))
                voice(Wave::Noise, 1.0f, 0.045f + 0.02f * energy, 0.045f, 0.6f,
                      (inBar == 7) ? 0.3f : -0.3f);
            // Hats every 16th, accented offbeats, open at 2 and 10.
            if (grooveOn)
                voice(Wave::Noise, 1.0f,
                      ((inBar & 1) ? 0.040f : 0.022f) + 0.02f * energy,
                      0.022f, 0.95f, (inBar & 2) ? 0.3f : -0.3f);
            if (grooveOn && (inBar == 2 || inBar == 10))
                voice(Wave::Noise, 1.0f, 0.05f, 0.10f, 0.8f);
            // Snare roll into the turn.
            if (fillBar && inBar >= 12)
                voice(Wave::Noise, 1.0f, 0.06f + 0.02f * (inBar - 12), 0.05f, 0.7f);
            // Bass riff: eighths, doubling to 16ths under fire.
            static const int riffTone[8] = {0, 0, 2, 0, 1, 0, 2, 1};
            const bool bassStep = (inBar % 2 == 0) || energy > 0.55f;
            if (bassStep) {
                const int t2 = riffTone[(inBar / 2) % 8];
                const int semis = (t2 == 0) ? cRoot : (t2 == 1) ? cThird : cFifth;
                voice(Wave::Square, hz(semis + ((inBar % 8 == 4) ? 0 : 12)),
                      0.13f + 0.03f * energy, 0.11f, 0.30f + 0.3f * energy);
            }
            // Stab chord on the downbeat once moving.
            if (!intro && inBar == 0) {
                voice(Wave::Saw, hz(cRoot + 12), 0.08f, 0.5f, 0.4f, -0.35f);
                voice(Wave::Saw, hz(cFifth + 12), 0.07f, 0.5f, 0.4f, 0.35f);
            }
            if (melodyOn && mdeg >= 0 && (lift || rise || bar >= 4)) {
                voice(Wave::Saw, hz(cRoot + kMinor[mdeg] + melOct),
                      0.08f + 0.04f * in2, 0.16f, 0.7f,
                      (inBar % 16 < 8) ? -0.4f : 0.4f);
                voice(Wave::Saw, hz(cRoot + kMinor[mdeg] + melOct + 12),
                      0.030f, 0.13f, 0.7f, (inBar % 16 < 8) ? 0.2f : -0.2f);
                if (harmonyOn)
                    voice(Wave::Saw, hz(cRoot + kMinor[mdeg - 2] + melOct),
                          0.045f, 0.14f, 0.6f, (inBar % 16 < 8) ? 0.3f : -0.3f);
            }
            break;
        }
        // ------------------------------------------ 2: ambient techno ------
        // The breather style - but it WAKES UP in combat: heartbeat kick,
        // shaker sixteenths, the arp doubling, shimmer over the lead.
        case 2: {
            if (inBar == 0) {
                voice(Wave::Triangle, hz(cRoot - 12), 0.20f, 3.4f, 0.12f);
                voice(Wave::Saw, hz(cThird), 0.05f + 0.04f * in2, 3.4f, 0.10f, -0.35f);
                voice(Wave::Saw, hz(cFifth + 12), 0.045f + 0.04f * in2, 3.4f, 0.10f, 0.35f);
            }
            if (inBar == 0 || inBar == 8)
                sweep(Wave::Sine, 70.0f, 41.0f, 0.16f + 0.10f * in2, 0.30f, 1.0f);
            // Combat pulse: the heartbeat becomes a beat.
            if (in2 > 0.3f && inBar % 4 == 0)
                sweep(Wave::Sine, 95.0f, 44.0f, 0.20f, 0.12f, 1.0f);
            if (in2 > 0.5f && (inBar & 1))
                voice(Wave::Noise, 1.0f, 0.022f, 0.020f, 0.9f,
                      (inBar & 2) ? 0.4f : -0.4f);
            if (melodyOn && mdeg >= 0) {
                voice(Wave::Triangle, hz(cRoot + kMinor[mdeg] + melOct),
                      0.055f + 0.03f * in2, 1.1f, 0.35f,
                      (bar & 1) ? 0.3f : -0.3f);
                // Shimmer: the same note an octave up, barely there.
                voice(Wave::Sine, hz(cRoot + kMinor[mdeg] + melOct + 12),
                      0.020f + 0.015f * in2, 1.4f, 0.5f, (bar & 1) ? -0.2f : 0.2f);
            }
            if (in2 > 0.35f && inBar % 4 == 2)
                voice(Wave::Noise, 1.0f, 0.028f * in2, 0.04f, 0.8f);
            // The arp: sparse in peace, doubled tempo in war.
            {
                const int arpEvery = (in2 > 0.45f || rise || lift) ? 2 : 4;
                if (!intro && inBar % arpEvery == 1) {
                    const int a = (inBar / 2) % 3;
                    const int semis = (a == 0) ? cRoot : (a == 1) ? cThird : cFifth;
                    voice(Wave::Triangle, hz(semis + 24), 0.042f, 0.5f, 0.4f,
                          (a == 1) ? -0.3f : 0.3f);
                }
            }
            break;
        }
        // -------------------------------------- 3: hybrid orchestral -------
        // The war film: a low staccato ostinato that NEVER stops, timpani on
        // the downbeats, brass triads answering across the bar, sustained
        // string chords behind, the horn line doubled at the octave, a noise
        // swell breathing into every phrase turn.
        case 3: {
            // The ostinato - eighth-note staccato low strings, always.
            if (!intro && inBar % 2 == 0) {
                const int semis = (inBar % 8 == 6) ? cRoot + 3
                                : (inBar % 4 == 2) ? cFifth : cRoot;
                voice(Wave::Saw, hz(semis), 0.11f + 0.05f * energy, 0.10f,
                      0.25f + 0.2f * energy, (inBar % 8 < 4) ? -0.2f : 0.2f);
            }
            // Timpani: 1 and 3, doubling under fire.
            if (inBar == 0 || inBar == 8 ||
                (energy > 0.4f && (inBar == 4 || inBar == 12))) {
                sweep(Wave::Sine, 95.0f, 38.0f, 0.28f, 0.18f, 1.0f);
                voice(Wave::Noise, 1.0f, 0.05f, 0.10f, 0.30f);
            }
            // Brass: full triad on the bar, a two-note answer mid-bar.
            if (inBar == 0) {
                voice(Wave::Saw, root, 0.13f + 0.06f * in2, 1.5f, 0.22f);
                voice(Wave::Saw, hz(cThird), 0.10f + 0.05f * in2, 1.5f, 0.22f, -0.25f);
                voice(Wave::Saw, hz(cFifth), 0.09f, 1.5f, 0.22f, 0.25f);
                voice(Wave::Saw, hz(cRoot + 12), 0.06f, 1.5f, 0.22f);
            }
            if (energy > 0.3f && inBar == 6) {
                voice(Wave::Saw, hz(cThird + 12), 0.07f, 0.4f, 0.3f, -0.3f);
                voice(Wave::Saw, hz(cFifth + 12), 0.06f, 0.4f, 0.3f, 0.3f);
            }
            if (melodyOn && mdeg >= 0) {
                voice(Wave::Saw, hz(cRoot + kMinor[mdeg] + melOct),
                      0.08f + 0.05f * in2, 0.55f, 0.35f);
                voice(Wave::Saw, hz(cRoot + kMinor[mdeg] + melOct + 12),
                      0.030f + 0.02f * energy, 0.45f, 0.4f, 0.15f);
                if (harmonyOn)
                    voice(Wave::Saw, hz(cRoot + kMinor[mdeg - 2] + melOct),
                          0.045f, 0.5f, 0.3f, 0.2f);
            }
            // The counter-line, a fourth below, in the LIFT.
            if (lift && cdeg >= 0 && inBar % 2 == 0)
                voice(Wave::Triangle, hz(cRoot + kMinor[cdeg] + 7),
                      0.055f, 0.7f, 0.3f, -0.2f);
            // A SECOND ostinato a fifth up on alternate colours: the same
            // engine, re-voiced, so a long fight keeps finding new texture.
            if (!intro && altVoicing && inBar % 2 == 1)
                voice(Wave::Saw, hz(cFifth + 12), 0.055f + 0.03f * energy, 0.09f,
                      0.30f + 0.2f * energy, (inBar % 8 < 4) ? 0.3f : -0.3f);
            // Rising brass swell across the back half of every other phrase.
            if (bigFill && inBar >= 8 && inBar % 2 == 0)
                voice(Wave::Saw, hz(cRoot + 12 + (inBar - 8)), 0.05f, 0.35f,
                      0.28f + 0.02f * inBar, -0.2f);
            // Cymbal breath into every phrase turn.
            if (fillBar && inBar >= 12)
                voice(Wave::Noise, 1.0f, 0.030f + 0.012f * (inBar - 12), 0.30f,
                      0.85f);
            // Timpani roll into the turn.
            if (fillBar && inBar >= 10 && (inBar & 1))
                sweep(Wave::Sine, 90.0f, 40.0f, 0.10f + 0.012f * inBar, 0.10f, 1.0f);
            if (boss_ && inBar == 15)
                sweep(Wave::Saw, root * 0.5f, root * 0.25f, 0.10f, 1.0f, 0.15f);
            // Boss: a low fifth drone under everything, re-fed each bar.
            if (boss_ && inBar == 0)
                voice(Wave::Saw, hz(cRoot - 12 + 7), 0.07f, 3.2f, 0.10f);
            break;
        }
    }

    // ------------------------------------------- shared tension hardware --
    // Whatever the style, the RISE closes its hats up, leans on the snare,
    // and runs a riser through its last two bars; the LIFT lands on a crash
    // and a sub drop. This is the release the eight bars were buying.
    if (rise) {
        if (inBar % 2 == 0 || riseT > 0.5f)
            voice(Wave::Noise, 1.0f, 0.020f + 0.035f * riseT, 0.022f, 0.9f,
                  (inBar & 2) ? 0.3f : -0.3f);
        if (riseT > 0.35f && inBar % 4 == 2)
            voice(Wave::Noise, 1.0f, 0.05f + 0.06f * riseT, 0.07f, 0.55f);
        if (bar >= 22) {
            if (inBar % 2 == 0)
                voice(Wave::Noise, 1.0f, 0.018f + 0.05f * riseT, 0.10f,
                      0.25f + 0.7f * riseT);
            if (inBar % 4 == 0)
                sweep(Wave::Saw, hz(cRoot + 12) * (0.8f + riseT),
                      hz(cRoot + 24), 0.035f, 0.30f, 0.5f);
        }
    }
    if (drop) {
        layerGain = 1.0f;
        // What is LEFT in a breakdown is as composed as what is taken out: a
        // sub pulse on the half-bar, a rimshot tick keeping the clock, and a
        // long filtered swell that grows across the four bars and hands over
        // to the riser at bar 16.
        if (inBar == 0 || inBar == 8)
            sweep(Wave::Sine, hz(cRoot + 12) * 0.5f, hz(cRoot) * 0.5f,
                  0.26f, 0.42f, 1.0f);
        if (inBar % 4 == 2)
            voice(Wave::Noise, 1.0f, 0.020f, 0.030f, 0.35f,
                  (inBar & 4) ? 0.45f : -0.45f);
        const float swell = ((bar - 12) + inBar / 16.0f) / 4.0f;
        if (inBar == 0)
            voice(Wave::Saw, hz(cRoot + 12), 0.030f + 0.055f * swell, 1.6f,
                  0.10f + 0.30f * swell, (bar & 1) ? 0.35f : -0.35f);
        if (bar == 15 && inBar == 12)
            sweep(Wave::Noise, 1.0f, 1.0f, 0.10f, 0.55f, 0.9f);
    }
    // Coming OUT of the breakdown. A four-bar hole in the arrangement needs a
    // door back in or the groove just reappears, and a groove that reappears
    // sounds like a mistake rather than a return.
    if (!boss_ && bar == 16 && inBar == 0) {
        voice(Wave::Noise, 1.0f, 0.13f, 0.45f, 0.5f);
        sweep(Wave::Sine, hz(cRoot + 12), hz(cRoot) * 0.5f, 0.24f, 0.35f, 1.0f);
    }
    if (!boss_ && bar == 24 && inBar == 0) {
        voice(Wave::Noise, 1.0f, 0.16f, 0.60f, 0.45f);
        sweep(Wave::Sine, 100.0f, 30.0f, 0.30f, 0.5f, 1.0f);
    }
}

void AudioEngine::mix(float* out, int frames) {
    const float dt = 1.0f / static_cast<float>(kRate);
    renderMusic(static_cast<float>(frames) * dt);

    std::memset(out, 0, sizeof(float) * static_cast<size_t>(frames) * 2);
    if (muted_) return;

    for (Voice& v : voices_) {
        if (!v.active) continue;
        const float bus = v.music ? musicGain_ : sfxGain_;
        const float lg = std::sqrt(std::max(0.0f, 0.5f * (1.0f - v.pan)));
        const float rg = std::sqrt(std::max(0.0f, 0.5f * (1.0f + v.pan)));

        for (int i = 0; i < frames; ++i) {
            // Envelope: attack, decay to sustain, hold, release. A voice with
            // no sustain is a one-shot and ends when the decay does.
            float env;
            const float holdEnd = v.attack + v.decay + v.held;
            if (v.t < v.attack) {
                env = v.t / std::max(v.attack, 1e-5f);
            } else if (v.t < v.attack + v.decay) {
                const float k = (v.t - v.attack) / std::max(v.decay, 1e-5f);
                env = 1.0f + (v.sustain - 1.0f) * k;
            } else if (v.t < holdEnd) {
                env = v.sustain;
            } else {
                const float k = (v.t - holdEnd) / std::max(v.release, 1e-5f);
                env = v.sustain * (1.0f - k);
                if (v.sustain <= 0.0001f) env = 0.0f;
            }
            if (env <= 0.0f && v.t > v.attack) { v.active = false; break; }

            // Pitch glide, used for every sweep and drop in the bank. The
            // coefficient is clamped into the stable region, whatever a
            // recipe asks for.
            if (v.glide > 0.0f)
                v.freq += (v.freqTarget - v.freq) * std::min(v.glide, 0.5f);

            float sample = oscillate(static_cast<int>(v.wave), v.phase, &v.noiseSeed);

            // A one-pole lowpass gives everything the muffled, slightly broken
            // character the ASCII look wants; without it the square waves are
            // painfully bright.
            v.lpState += (sample - v.lpState) * v.lowpass;
            sample = v.lpState;

            const float value = sample * env * v.amp * bus;
            out[i * 2 + 0] += value * lg;
            out[i * 2 + 1] += value * rg;

            v.phase += v.freq * dt;
            if (v.phase >= 1.0f) v.phase -= std::floor(v.phase);
            // Non-finite guard: one sick voice must never poison the device
            // stream. Kill it on the spot.
            if (!(v.phase > -1e9f && v.phase < 1e9f) || !std::isfinite(v.lpState)) {
                v.active = false;
                break;
            }
            v.t += dt;
        }
    }

    // Master peak limiter: instant attack, ~120 ms release, then a hard
    // safety clamp. A limiter turns the WHOLE mix down briefly and lets it
    // back up, so overlapping shots stay clean and the music bed stays
    // audible underneath. The per-sample waveshapers this replaces (first
    // x/(1+|x|), then tanh) were the real "sound cuts out when several
    // sounds play": five square waves at once drove them into saturation
    // and the mix collapsed into flat intermodulated mush.
    {
        const float kThreshold = 0.90f;
        const float kRelease = std::exp(-1.0f / (0.080f * static_cast<float>(kRate)));
        float minGain = 1.0f;
        int live = 0;
        for (const Voice& v : voices_) live += v.active ? 1 : 0;
        for (int i = 0; i < frames; ++i) {
            const float l = out[i * 2 + 0], r = out[i * 2 + 1];
            const float pk = std::max(std::fabs(l), std::fabs(r));
            limEnv_ = std::max(pk, limEnv_ * kRelease);
            const float gain = (limEnv_ > kThreshold) ? kThreshold / limEnv_ : 1.0f;
            if (gain < minGain) minGain = gain;
            out[i * 2 + 0] = clampf(l * gain, -0.98f, 0.98f);
            out[i * 2 + 1] = clampf(r * gain, -0.98f, 0.98f);
        }
        // Worst recent gain, recovering slowly so a dip is readable on the
        // HUD for a second or two.
        limFloor_ = std::min(minGain, limFloor_ + 0.02f);
        activeVoices_ = live;
    }
}

void SDLCALL AudioEngine::feed(void* userdata, SDL_AudioStream* stream,
                               int additional, int /*total*/) {
    AudioEngine* self = static_cast<AudioEngine*>(userdata);
    if (!self || additional <= 0) return;

    // Starvation watchdog: the device asks for data on its own schedule; a
    // gap of more than ~40 ms between requests means the OS starved the
    // audio thread and the output glitched no matter what the mix did.
    const uint64_t nowNs = SDL_GetTicksNS();
    if (self->lastFeedNs_ != 0 && nowNs - self->lastFeedNs_ > 40000000ull)
        ++self->underruns_;
    self->lastFeedNs_ = nowNs;

    // Render in modest blocks so the music clock advances smoothly and the
    // stack buffer stays small.
    constexpr int kBlock = 512;
    float buffer[kBlock * 2];
    int remaining = additional / static_cast<int>(sizeof(float) * 2);
    while (remaining > 0) {
        const int frames = std::min(remaining, kBlock);
        self->mix(buffer, frames);
        SDL_PutAudioStreamData(stream, buffer,
                               frames * static_cast<int>(sizeof(float) * 2));
        remaining -= frames;
    }
}

} // namespace sb
