#pragma once
// JUCE-independent real-time DSP core for Caris Throw Matrix.
// Reimplementation based on .fmx parameter metadata + extended v0.2 modes (Spring, Pitch, Phaser, Distortion).
#include <array>
#include <cstddef>
#include <vector>

namespace throwfx {

enum Mode : int {
    DubDelay = 0,
    SpaceReverb = 1,
    ShimmerVerb = 2,
    TapePingPong = 3,
    SpringReverb = 4,
    DistortedPitch = 5,
    PhaserSwirl = 6,
    NumModes = 7
};

struct Parameters {
    float throwSend = 0.0f;      // 0..100%: momentary/automated throw send
    int mode = 0;                // 0..6 (see Mode enum)
    int delayTime = 4;           // 0: 1/16, 1: 1/8D, 2: 1/8, 3: 1/4D, 4: 1/4, 5: 1/2
    float feedback = 65.0f;      // 0..100%
    float decay = 4.5f;          // 0.5..12.0 s
    float cutoff = 4500.0f;      // 200..18000 Hz
    float damping = 40.0f;       // 0..100%
    float drive = 25.0f;         // 0..100%
    float spread = 75.0f;        // 0..100%
    float duck = 50.0f;          // 0..100%
    float outputDb = 0.0f;       // -60..+12 dB
    float mix = 50.0f;           // 0..100%
    float pitchSemitones = 0.0f; // -12..+12 semitones
    float phaserAmount = 0.0f;   // 0..100%
    bool wetOnly = false;        // true = 100% wet (kill dry) for bus/sidechain/capture workflows
};

class DelayLine {
public:
    void prepare(std::size_t maxSamples);
    void clear() noexcept;
    float read(float delaySamples) const noexcept;
    void write(float value) noexcept;
private:
    std::vector<float> samples;
    std::size_t cursor = 0;
};

class ReverbTank {
public:
    void prepare(double sampleRate);
    void clear() noexcept;
    std::array<float, 2> process(float left, float right, float decay, float damping) noexcept;
private:
    struct Comb {
        std::vector<float> data;
        std::size_t cursor = 0;
        float filter = 0.0f;
        float run(float input, float feedback, float damp) noexcept;
        void clear() noexcept;
    };
    struct Allpass {
        std::vector<float> data;
        std::size_t cursor = 0;
        float run(float input) noexcept;
        void clear() noexcept;
    };
    std::array<std::array<Comb, 4>, 2> combs;
    std::array<std::array<Allpass, 2>, 2> diffusion;
    double rate = 48000.0;
    float cachedDecay = -1.0f;
    std::array<std::array<float, 4>, 2> feedbackCoefficients{};
};

// Physical-style Spring Reverb tank with chirp dispersion allpass chain for vocal adlibs/throws.
class SpringTank {
public:
    void prepare(double sampleRate);
    void clear() noexcept;
    std::array<float, 2> process(float left, float right, float decay, float damping) noexcept;
private:
    struct DispersionAllpass {
        std::vector<float> data;
        std::size_t cursor = 0;
        float coeff = 0.72f;
        float run(float input) noexcept;
        void clear() noexcept;
    };
    struct SpringLine {
        DelayLine delay;
        float baseSamples = 1600.0f;
        float lpState = 0.0f;
        float hpState = 0.0f;
        void clear() noexcept { delay.clear(); lpState = 0.0f; hpState = 0.0f; }
    };
    std::array<std::array<DispersionAllpass, 8>, 2> chirp;
    std::array<std::array<SpringLine, 2>, 2> springs;
    double rate = 48000.0;
    float modPhase = 0.0f;
};

class PitchShifter {
public:
    void prepare(double sampleRate);
    void clear() noexcept;
    float process(int channel, float input, float semitones = 12.0f) noexcept;
private:
    struct Voice {
        std::vector<float> samples;
        std::size_t cursor = 0;
        float phase = 0.0f;
    };
    std::array<Voice, 2> voices;
    float grainSize = 2048.0f;
};

class StereoPhaser {
public:
    void prepare(double sampleRate);
    void clear() noexcept;
    std::array<float, 2> process(float left, float right, float amount) noexcept;
private:
    struct FirstOrderAllpass {
        float z1 = 0.0f;
        float run(float x, float coeff) noexcept {
            const float y = coeff * x + z1;
            z1 = x - coeff * y;
            return y;
        }
    };
    std::array<std::array<FirstOrderAllpass, 6>, 2> stages{};
    std::array<float, 2> feedbackState {0.0f, 0.0f};
    double rate = 48000.0;
    float lfoPhase = 0.0f;
};

class Engine {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    // Process one stereo frame. No allocations and no locks on the audio thread.
    std::array<float, 2> process(float inputL, float inputR,
                                 const Parameters& p, double bpm) noexcept;
    // Last 100%-wet stereo frame (post-duck, post-spread, post-output-gain) for internal throw recording.
    std::array<float, 2> lastWetFrame() const noexcept { return lastWet; }
    static float beatsForDivision(int index) noexcept;
private:
    double rate = 48000.0;
    std::array<DelayLine, 2> echoes;
    ReverbTank reverb;
    SpringTank spring;
    PitchShifter pitch;
    PitchShifter subPitch;
    StereoPhaser phaser;
    std::array<float, 2> inputLowpass {0.0f, 0.0f};
    std::array<float, 2> feedbackLowpass {0.0f, 0.0f};
    std::array<float, 2> lastWet {0.0f, 0.0f};
    float duckEnvelope = 0.0f;
    float wowPhase = 0.0f;
    float smoothedSend = 0.0f;
    float previousCutoff = -1.0f, previousDamping = -1.0f, previousOutputDb = -99.0f;
    float inputCoefficient = 0.5f, feedbackCoefficient = 0.5f, outputGain = 1.0f;
};

} // namespace throwfx
