#include "ThrowDSP.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace throwfx;
    assert(std::abs(Engine::beatsForDivision(0) - 0.25f) < 1e-6f);
    assert(std::abs(Engine::beatsForDivision(1) - 0.75f) < 1e-6f);
    assert(std::abs(Engine::beatsForDivision(4) - 1.00f) < 1e-6f);

    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        Engine engine;
        engine.prepare(sampleRate);
        Parameters p;
        p.mode = 0;
        p.mix = 0;
        for (int i = 0; i < 100; ++i) {
            float input = i == 0 ? 0.6f : 0.0f;
            const auto out = engine.process(input, input, p, 122);
            assert(std::abs(out[0] - input) < 1e-6f);
            assert(std::abs(out[1] - input) < 1e-6f);
        }
        // Every mode (0..6) must create finite, non-zero wet output once enabled.
        for (int mode = 0; mode < Mode::NumModes; ++mode) {
            engine.reset();
            p.mode = mode;
            p.mix = 100;
            p.throwSend = 100;
            p.delayTime = 0;
            p.decay = 1.8f;
            p.pitchSemitones = (mode == Mode::DistortedPitch) ? -7.0f : 0.0f;
            p.phaserAmount = (mode == Mode::PhaserSwirl) ? 80.0f : 25.0f;
            double wetEnergy = 0;
            for (int i = 0; i < static_cast<int>(sampleRate * 0.95); ++i) {
                const float dryL = (i < 64) ? std::sin(0.2f * static_cast<float>(i)) : 0.0f;
                const float dryR = (i < 64) ? std::cos(0.2f * static_cast<float>(i)) * 0.5f : 0.0f;
                const auto out = engine.process(dryL, dryR, p, 140);
                const auto wet = engine.lastWetFrame();
                assert(std::isfinite(out[0]) && std::isfinite(out[1]));
                assert(std::isfinite(wet[0]) && std::isfinite(wet[1]));
                assert(std::abs(out[0]) < 10.0f && std::abs(out[1]) < 10.0f);
                wetEnergy += std::abs(out[0]) + std::abs(out[1]);
            }
            if (wetEnergy < 0.002) {
                std::cerr << "FAIL mode=" << mode << " rate=" << sampleRate << " energy=" << wetEnergy << "\n";
                return 1;
            }
            std::cout << "PASS rate=" << sampleRate << " mode=" << mode << " wet-energy=" << wetEnergy << "\n";
        }
        engine.reset();
        p.mode = Mode::SpringReverb;
        p.throwSend = 100;
        p.mix = 100;
        p.delayTime = 0;
        // Tail must continue when the throw gate is released.
        double tail = 0;
        for (int i = 0; i < static_cast<int>(sampleRate * 0.4); ++i) {
            if (i == static_cast<int>(sampleRate * 0.1)) p.throwSend = 0;
            auto out = engine.process(i == 0 ? 1.0f : 0, i == 0 ? 1.0f : 0, p, 120);
            if (i > static_cast<int>(sampleRate * 0.2)) tail += std::abs(out[0]) + std::abs(out[1]);
        }
        assert(tail > 0.01);
        std::cout << "PASS throw-tail rate=" << sampleRate << " energy=" << tail << "\n";
    }
    std::cout << "All native DSP tests passed.\n";
}
