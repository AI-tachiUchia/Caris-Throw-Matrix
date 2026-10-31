#include "ThrowDSP.h"
#include <algorithm>
#include <cmath>

namespace throwfx {
namespace {
constexpr float pi = 3.14159265358979323846f;
float limit(float v, float lo, float hi) noexcept { return std::clamp(v, lo, hi); }
float lowpassAmount(float hz, double rate) noexcept {
    return 1.0f - std::exp(-2.0f * pi * hz / static_cast<float>(rate));
}
float interpolate(const std::vector<float>& samples, float index) noexcept {
    const float sz = static_cast<float>(samples.size());
    while (index < 0.0f) index += sz;
    while (index >= sz) index -= sz;
    const auto i = static_cast<std::size_t>(index);
    const auto j = (i + 1) % samples.size();
    const float t = index - static_cast<float>(i);
    return samples[i] + t * (samples[j] - samples[i]);
}
float saturateDrive(float x, float drivePercent, bool extraGrit) noexcept {
    const float normDrive = limit(drivePercent, 0.0f, 100.0f) * 0.01f;
    const float preGain = 1.0f + normDrive * (extraGrit ? 6.5f : 3.8f);
    const float asym = extraGrit ? 0.28f * normDrive : 0.10f * normDrive;
    const float driven = x * preGain;
    // Zero-preserving asymmetric waveshaper: f(0) == 0 exactly
    const float shaped = std::tanh(driven + asym) - std::tanh(asym);
    const float norm = std::max(0.30f, std::tanh(preGain));
    return shaped / norm;
}
} // namespace

void DelayLine::prepare(std::size_t maxSamples) {
    samples.assign(std::max<std::size_t>(4, maxSamples), 0.0f);
    cursor = 0;
}
void DelayLine::clear() noexcept { std::fill(samples.begin(), samples.end(), 0.0f); cursor = 0; }
float DelayLine::read(float delaySamples) const noexcept {
    if (samples.empty()) return 0.0f;
    delaySamples = limit(delaySamples, 1.0f, static_cast<float>(samples.size() - 2));
    return interpolate(samples, static_cast<float>(cursor) - delaySamples);
}
void DelayLine::write(float value) noexcept {
    if (samples.empty()) return;
    samples[cursor] = std::isfinite(value) ? limit(value, -4.0f, 4.0f) : 0.0f;
    cursor = (cursor + 1) % samples.size();
}

float ReverbTank::Comb::run(float input, float feedback, float damp) noexcept {
    const float delayed = data[cursor];
    filter += (delayed - filter) * (1.0f - damp);
    data[cursor] = input + feedback * filter;
    cursor = (cursor + 1) % data.size();
    return delayed;
}
void ReverbTank::Comb::clear() noexcept {
    std::fill(data.begin(), data.end(), 0.0f); cursor = 0; filter = 0.0f;
}
float ReverbTank::Allpass::run(float input) noexcept {
    const float buffered = data[cursor];
    const float y = -input + buffered;
    data[cursor] = input + 0.5f * buffered;
    cursor = (cursor + 1) % data.size();
    return y;
}
void ReverbTank::Allpass::clear() noexcept {
    std::fill(data.begin(), data.end(), 0.0f); cursor = 0;
}
void ReverbTank::prepare(double sampleRate) {
    rate = sampleRate;
    constexpr int combLengths[4] { 1116, 1188, 1277, 1356 };
    constexpr int allpassLengths[2] { 225, 341 };
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 4; ++i)
            combs[ch][i].data.assign(static_cast<std::size_t>(std::max(16, static_cast<int>(std::lround((combLengths[i] + ch * 31) * rate / 44100.0)))), 0.0f);
        for (int i = 0; i < 2; ++i)
            diffusion[ch][i].data.assign(static_cast<std::size_t>(std::max(8, static_cast<int>(std::lround((allpassLengths[i] + ch * 17) * rate / 44100.0)))), 0.0f);
    }
    clear();
}
void ReverbTank::clear() noexcept {
    cachedDecay = -1.0f;
    for (auto& ch : combs) for (auto& c : ch) c.clear();
    for (auto& ch : diffusion) for (auto& a : ch) a.clear();
}
std::array<float, 2> ReverbTank::process(float left, float right, float decay, float damping) noexcept {
    if (decay != cachedDecay) {
        cachedDecay = decay;
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 4; ++i) {
                const float seconds = static_cast<float>(combs[channel][i].data.size() / rate);
                feedbackCoefficients[channel][i] = limit(std::pow(0.001f, seconds / std::max(0.5f, decay)), 0.0f, 0.985f);
            }
    }
    const float in[2] {left * 0.20f, right * 0.20f};
    std::array<float, 2> result{};
    for (int ch = 0; ch < 2; ++ch) {
        float sum = 0.0f;
        for (int i = 0; i < 4; ++i)
            sum += combs[ch][i].run(in[ch], feedbackCoefficients[ch][i],
                                     0.08f + 0.80f * limit(damping, 0.0f, 1.0f));
        float value = sum * 0.25f;
        for (auto& allpass : diffusion[ch]) value = allpass.run(value);
        result[ch] = value;
    }
    return result;
}

float SpringTank::DispersionAllpass::run(float input) noexcept {
    const float delayed = data[cursor];
    const float w = input + coeff * delayed;
    const float y = -coeff * w + delayed;
    data[cursor] = w;
    cursor = (cursor + 1) % data.size();
    return y;
}
void SpringTank::DispersionAllpass::clear() noexcept {
    std::fill(data.begin(), data.end(), 0.0f);
    cursor = 0;
}
void SpringTank::prepare(double sampleRate) {
    rate = std::max(8000.0, sampleRate);
    // Prime-numbered short allpass lengths create the classic metallic "drip/chirp" spring transient
    constexpr int chirpLengths[8] { 5, 7, 11, 13, 17, 19, 23, 29 };
    constexpr double springMs[2][2] { {37.3, 53.7}, {41.1, 58.9} };
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 8; ++i) {
            const auto len = static_cast<std::size_t>(std::max(3, static_cast<int>(std::lround((chirpLengths[i] + ch * 2) * rate / 44100.0))));
            chirp[ch][i].data.assign(len, 0.0f);
            chirp[ch][i].coeff = (i % 2 == 0) ? 0.74f : -0.68f;
        }
        for (int s = 0; s < 2; ++s) {
            const float samples = static_cast<float>(springMs[ch][s] * 0.001 * rate);
            springs[ch][s].baseSamples = samples;
            springs[ch][s].delay.prepare(static_cast<std::size_t>(samples + rate * 0.02 + 16));
        }
    }
    clear();
}
void SpringTank::clear() noexcept {
    for (auto& ch : chirp) for (auto& a : ch) a.clear();
    for (auto& ch : springs) for (auto& s : ch) s.clear();
    modPhase = 0.0f;
}
std::array<float, 2> SpringTank::process(float left, float right, float decay, float damping) noexcept {
    modPhase += 2.3f / static_cast<float>(rate);
    if (modPhase >= 1.0f) modPhase -= 1.0f;

    const float dampCoeff = lowpassAmount(4800.0f - limit(damping, 0.0f, 1.0f) * 3100.0f, rate);
    const float hpCoeff = lowpassAmount(165.0f, rate);
    const float safeDecay = limit(decay, 0.4f, 10.0f);
    const float fb = limit(std::pow(0.001f, 0.048f / safeDecay), 0.25f, 0.94f);

    float in[2] { left * 0.38f, right * 0.38f };
    // Pass incoming transient through the 6-stage dispersion chirp cascade
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 6; ++i)
            in[ch] = chirp[ch][i].run(in[ch]);
    }

    std::array<float, 2> out {0.0f, 0.0f};
    float tankCross[2] {0.0f, 0.0f};
    for (int ch = 0; ch < 2; ++ch) {
        const float wobble = std::sin(2.0f * pi * modPhase + ch * 1.3f) * static_cast<float>(rate) * 0.00028f;
        const float d0 = springs[ch][0].delay.read(springs[ch][0].baseSamples + wobble);
        const float d1 = springs[ch][1].delay.read(springs[ch][1].baseSamples - wobble * 0.7f);
        out[ch] = (d0 * 0.62f - d1 * 0.48f);
        tankCross[ch] = (d0 + d1) * 0.5f;
    }

    for (int ch = 0; ch < 2; ++ch) {
        const int other = 1 - ch;
        for (int s = 0; s < 2; ++s) {
            auto& line = springs[ch][s];
            const float raw = in[ch] + fb * (s == 0 ? tankCross[ch] : (0.75f * tankCross[ch] - 0.25f * tankCross[other]));
            line.lpState += dampCoeff * (raw - line.lpState);
            line.hpState += hpCoeff * (line.lpState - line.hpState);
            const float bandpassed = line.lpState - line.hpState;
            // Dedicated dispersion allpass per spring line (stages 6 and 7, clocked once per sample)
            const float dispersed = chirp[ch][6 + s].run(bandpassed);
            line.delay.write(std::tanh(dispersed));
        }
        // Blend initial chirp transient with resonant spring tank
        out[ch] = out[ch] * 1.35f + in[ch] * 0.45f;
    }
    return out;
}

void PitchShifter::prepare(double sampleRate) {
    const auto size = static_cast<std::size_t>(std::max(1024.0, sampleRate * 0.09));
    grainSize = static_cast<float>(size);
    for (auto& voice : voices) {
        voice.samples.assign(size + 16, 0.0f);
        voice.cursor = 0;
        voice.phase = 0.0f;
    }
}
void PitchShifter::clear() noexcept {
    for (auto& v : voices) {
        std::fill(v.samples.begin(), v.samples.end(), 0.0f);
        v.cursor = 0;
        v.phase = 0.0f;
    }
}
float PitchShifter::process(int channel, float input, float semitones) noexcept {
    auto& v = voices[static_cast<std::size_t>(channel & 1)];
    if (v.samples.empty()) return input;
    v.samples[v.cursor] = input;
    if (std::abs(semitones) < 0.01f) {
        v.cursor = (v.cursor + 1) % v.samples.size();
        return input;
    }
    const float ratio = std::pow(2.0f, limit(semitones, -24.0f, 24.0f) / 12.0f);
    const float drift = (1.0f - ratio) / grainSize;
    auto readGrain = [&](float phase) {
        const float delay = 6.0f + phase * (grainSize - 12.0f);
        const float weight = 0.5f * (1.0f - std::cos(2.0f * pi * phase));
        return weight * interpolate(v.samples, static_cast<float>(v.cursor) - delay);
    };
    float other = v.phase + 0.5f;
    if (other >= 1.0f) other -= 1.0f;
    const float out = readGrain(v.phase) + readGrain(other);
    v.phase += drift;
    while (v.phase >= 1.0f) v.phase -= 1.0f;
    while (v.phase < 0.0f) v.phase += 1.0f;
    v.cursor = (v.cursor + 1) % v.samples.size();
    return out;
}

void StereoPhaser::prepare(double sampleRate) {
    rate = std::max(8000.0, sampleRate);
    clear();
}
void StereoPhaser::clear() noexcept {
    for (auto& ch : stages) for (auto& s : ch) s.z1 = 0.0f;
    feedbackState = {0.0f, 0.0f};
    lfoPhase = 0.0f;
}
std::array<float, 2> StereoPhaser::process(float left, float right, float amount) noexcept {
    const float mix = limit(amount, 0.0f, 1.0f);
    if (mix <= 0.0005f) return {left, right};

    lfoPhase += 0.42f / static_cast<float>(rate);
    if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

    const float in[2] {left, right};
    std::array<float, 2> out {left, right};
    for (int ch = 0; ch < 2; ++ch) {
        const float lfo = 0.5f + 0.5f * std::sin(2.0f * pi * (lfoPhase + ch * 0.25f));
        const float centerHz = 260.0f * std::pow(9.5f, lfo);
        const float w = std::tan(pi * limit(centerHz, 120.0f, static_cast<float>(rate * 0.42)) / static_cast<float>(rate));
        const float coeff = (1.0f - w) / (1.0f + w);

        float y = std::tanh(in[ch] + feedbackState[ch] * 0.48f);
        for (auto& stage : stages[ch])
            y = stage.run(y, coeff);
        feedbackState[ch] = y;
        out[ch] = in[ch] * (1.0f - 0.55f * mix) + y * (0.75f * mix);
    }
    return out;
}

void Engine::prepare(double sampleRate) {
    rate = std::max(8000.0, sampleRate);
    for (auto& delay : echoes) delay.prepare(static_cast<std::size_t>(rate * 8.0));
    reverb.prepare(rate);
    spring.prepare(rate);
    pitch.prepare(rate);
    subPitch.prepare(rate);
    phaser.prepare(rate);
    reset();
}
void Engine::reset() noexcept {
    for (auto& delay : echoes) delay.clear();
    reverb.clear();
    spring.clear();
    pitch.clear();
    subPitch.clear();
    phaser.clear();
    inputLowpass.fill(0.0f);
    feedbackLowpass.fill(0.0f);
    lastWet.fill(0.0f);
    duckEnvelope = 0.0f;
    wowPhase = 0.0f;
    smoothedSend = 0.0f;
    previousCutoff = -1.0f;
    previousDamping = -1.0f;
    previousOutputDb = -99.0f;
}
float Engine::beatsForDivision(int index) noexcept {
    switch (index) {
        case 0: return 0.25f;   // sixteenth
        case 1: return 0.75f;   // dotted eighth
        case 2: return 0.50f;   // eighth
        case 3: return 1.50f;   // dotted quarter
        case 4: return 1.00f;   // quarter
        case 5: return 2.00f;   // half
        default: return 1.00f;
    }
}
std::array<float, 2> Engine::process(float inputL, float inputR,
                                     const Parameters& p, double bpm) noexcept {
    const float safeBpm = static_cast<float>(limit(static_cast<float>(bpm), 20.0f, 300.0f));
    const float division = beatsForDivision(p.delayTime);
    const float delaySamples = limit(static_cast<float>(rate) * 60.0f / safeBpm * division,
                                     1.0f, static_cast<float>(rate * 7.8));
    const float targetSend = limit(p.throwSend * 0.01f, 0.0f, 1.0f);
    const float sendCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(rate) * 0.003f));
    smoothedSend += (targetSend - smoothedSend) * sendCoeff;
    const float send = smoothedSend;
    const float feedback = limit(p.feedback * 0.01f, 0.0f, 0.95f);
    const float damping = limit(p.damping * 0.01f, 0.0f, 1.0f);
    const float cutoff = limit(p.cutoff, 200.0f, 18000.0f);
    if (cutoff != previousCutoff || damping != previousDamping) {
        inputCoefficient = lowpassAmount(cutoff, rate);
        feedbackCoefficient = lowpassAmount(cutoff * (1.0f - damping * 0.70f), rate);
        previousCutoff = cutoff;
        previousDamping = damping;
    }
    const float lpf = inputCoefficient;
    const float tailLpf = feedbackCoefficient;
    const bool isDistortedMode = (p.mode == Mode::DistortedPitch);
    const float effectiveDrive = isDistortedMode ? std::max(55.0f, p.drive) : p.drive;

    const float source[2] { inputL, inputR };
    float injected[2] {};
    for (int ch = 0; ch < 2; ++ch) {
        inputLowpass[ch] += lpf * (source[ch] - inputLowpass[ch]);
        injected[ch] = saturateDrive(inputLowpass[ch], effectiveDrive, isDistortedMode) * send;
    }

    // Apply optional user pitch shift to the injected throw signal (when pitchSemitones != 0 and not handled specially)
    const float userPitch = limit(p.pitchSemitones, -12.0f, 12.0f);
    if (std::abs(userPitch) > 0.05f && p.mode != Mode::ShimmerVerb && p.mode != Mode::DistortedPitch) {
        injected[0] = pitch.process(0, injected[0], userPitch);
        injected[1] = pitch.process(1, injected[1], userPitch);
    }

    float delayTap[2] {};
    if (p.mode == Mode::TapePingPong || p.mode == Mode::PhaserSwirl || p.mode == Mode::DistortedPitch) {
        wowPhase += 0.45f / static_cast<float>(rate);
        if (wowPhase >= 1.0f) wowPhase -= 1.0f;
    }
    for (int ch = 0; ch < 2; ++ch) {
        const float wow = (p.mode == Mode::TapePingPong || p.mode == Mode::PhaserSwirl || p.mode == Mode::DistortedPitch)
            ? std::sin(2.0f * pi * wowPhase + ch * 0.4f) * static_cast<float>(rate) * 0.0015f
            : 0.0f;
        delayTap[ch] = echoes[ch].read(delaySamples + wow);
        feedbackLowpass[ch] += tailLpf * (delayTap[ch] - feedbackLowpass[ch]);
    }

    float echoInput[2] { injected[0], injected[1] };
    if (p.mode == Mode::TapePingPong || p.mode == Mode::PhaserSwirl) {
        echoInput[0] += feedback * feedbackLowpass[1];
        echoInput[1] += feedback * feedbackLowpass[0];
    } else {
        echoInput[0] += feedback * feedbackLowpass[0];
        echoInput[1] += feedback * feedbackLowpass[1];
    }

    float wet[2] {};
    switch (p.mode) {
        case Mode::DubDelay:
        case Mode::TapePingPong: {
            wet[0] = delayTap[0];
            wet[1] = delayTap[1];
            break;
        }
        case Mode::SpaceReverb:
        case Mode::ShimmerVerb: {
            const auto verb = reverb.process(injected[0] + delayTap[0] * 0.32f,
                                             injected[1] + delayTap[1] * 0.32f,
                                             limit(p.decay, 0.5f, 12.0f), damping);
            wet[0] = verb[0] * 2.8f + delayTap[0] * 0.12f;
            wet[1] = verb[1] * 2.8f + delayTap[1] * 0.12f;
            if (p.mode == Mode::ShimmerVerb) {
                const float shiftSemi = (std::abs(userPitch) > 0.05f) ? userPitch : 12.0f;
                const float shiftedL = pitch.process(0, verb[0], shiftSemi);
                const float shiftedR = pitch.process(1, verb[1], shiftSemi);
                wet[0] += shiftedL * 1.3f;
                wet[1] += shiftedR * 1.3f;
                echoInput[0] += shiftedL * 0.25f;
                echoInput[1] += shiftedR * 0.25f;
            }
            break;
        }
        case Mode::SpringReverb: {
            // Adlib Spring Reverb: chirp dispersion + metallic dual spring tank + rhythmic pre-delay tap
            const auto spr = spring.process(injected[0] + delayTap[0] * 0.38f,
                                            injected[1] + delayTap[1] * 0.38f,
                                            limit(p.decay, 0.5f, 12.0f), damping);
            wet[0] = spr[0] * 1.65f + delayTap[0] * 0.28f;
            wet[1] = spr[1] * 1.65f + delayTap[1] * 0.28f;
            echoInput[0] += spr[0] * 0.18f;
            echoInput[1] += spr[1] * 0.18f;
            break;
        }
        case Mode::DistortedPitch: {
            // Distorted Pitch Throw: pitch-shifted (default -12st sub + +12st octave, or custom userPitch) + gritty saturation + lush reverb
            const float mainShift = (std::abs(userPitch) > 0.05f) ? userPitch : -12.0f;
            const float pL = pitch.process(0, injected[0] + delayTap[0] * 0.45f, mainShift);
            const float pR = pitch.process(1, injected[1] + delayTap[1] * 0.45f, mainShift);
            const float octL = subPitch.process(0, injected[0] + delayTap[0] * 0.35f, 12.0f);
            const float octR = subPitch.process(1, injected[1] + delayTap[1] * 0.35f, 12.0f);
            const float dirtyL = saturateDrive(pL * 0.70f + octL * 0.35f + delayTap[0] * 0.35f, effectiveDrive, true);
            const float dirtyR = saturateDrive(pR * 0.70f + octR * 0.35f + delayTap[1] * 0.35f, effectiveDrive, true);
            const auto verb = reverb.process(dirtyL * 0.65f, dirtyR * 0.65f, limit(p.decay, 0.5f, 12.0f), damping);
            wet[0] = dirtyL * 0.55f + verb[0] * 2.1f;
            wet[1] = dirtyR * 0.55f + verb[1] * 2.1f;
            echoInput[0] += pL * 0.18f * feedback;
            echoInput[1] += pR * 0.18f * feedback;
            break;
        }
        case Mode::PhaserSwirl:
        default: {
            // Psychedelic Phaser Swirl: ping-pong delay + diffused space tank + deep stereo phaser modulation
            const auto verb = reverb.process(injected[0] + delayTap[0] * 0.55f,
                                             injected[1] + delayTap[1] * 0.55f,
                                             limit(p.decay, 0.5f, 12.0f), damping);
            wet[0] = delayTap[0] * 0.75f + verb[0] * 2.1f;
            wet[1] = delayTap[1] * 0.75f + verb[1] * 2.1f;
            break;
        }
    }

    // Apply stereo phaser (always active in PhaserSwirl mode, or scaled by phaserAmount in any mode)
    const float phaserDepth = (p.mode == Mode::PhaserSwirl)
        ? std::max(0.75f, limit(p.phaserAmount * 0.01f, 0.0f, 1.0f))
        : limit(p.phaserAmount * 0.01f, 0.0f, 1.0f);
    if (phaserDepth > 0.001f) {
        const auto phased = phaser.process(wet[0], wet[1], phaserDepth);
        wet[0] = phased[0];
        wet[1] = phased[1];
        // Also feed a touch of phased movement back into the delay loop in PhaserSwirl mode
        if (p.mode == Mode::PhaserSwirl) {
            echoInput[0] = 0.75f * echoInput[0] + 0.25f * phased[0] * feedback;
            echoInput[1] = 0.75f * echoInput[1] + 0.25f * phased[1] * feedback;
        }
    }

    echoes[0].write(std::tanh(echoInput[0]));
    echoes[1].write(std::tanh(echoInput[1]));

    // Envelope-following auto-duck while the dry instrument/vocal is sounding.
    const float level = std::max(std::abs(inputL), std::abs(inputR));
    const float smoothing = std::exp(-1.0f / (static_cast<float>(rate) * ((level > duckEnvelope) ? 0.008f : 0.22f)));
    duckEnvelope = smoothing * duckEnvelope + (1.0f - smoothing) * level;
    const float duckGain = 1.0f / (1.0f + limit(p.duck, 0.0f, 100.0f) * 0.01f * duckEnvelope * 8.0f);

    const float middle = (wet[0] + wet[1]) * 0.5f;
    const float side = (wet[0] - wet[1]) * 0.5f;
    const float stereo = 0.20f + limit(p.spread, 0.0f, 100.0f) * 0.018f;
    wet[0] = (middle + side * stereo) * duckGain;
    wet[1] = (middle - side * stereo) * duckGain;
    const float mix = p.wetOnly ? 1.0f : limit(p.mix * 0.01f, 0.0f, 1.0f);
    if (p.outputDb != previousOutputDb) {
        outputGain = std::pow(10.0f, limit(p.outputDb, -60.0f, 12.0f) * 0.05f);
        previousOutputDb = p.outputDb;
    }
    const float output = outputGain;
    lastWet[0] = std::isfinite(wet[0] * output) ? wet[0] * output : 0.0f;
    lastWet[1] = std::isfinite(wet[1] * output) ? wet[1] * output : 0.0f;

    const float dryGain = p.wetOnly ? 0.0f : (1.0f - mix);
    std::array<float, 2> result {
        (dryGain * inputL + mix * wet[0]) * output,
        (dryGain * inputR + mix * wet[1]) * output
    };
    for (auto& v : result) if (!std::isfinite(v)) v = 0.0f;
    return result;
}
} // namespace throwfx
