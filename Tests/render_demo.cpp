// Optional demo: generates synthetic plucked audio through all four modes.
#include "ThrowDSP.h"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

static void u16(std::ofstream& f, uint16_t n) { f.put(static_cast<char>(n)); f.put(static_cast<char>(n >> 8)); }
static void u32(std::ofstream& f, uint32_t n) { u16(f, static_cast<uint16_t>(n)); u16(f, static_cast<uint16_t>(n >> 16)); }
int main(int argc, char** argv) {
    const std::string root = (argc > 1) ? argv[1] : ".";
    constexpr int sampleRate = 48000, seconds = 6;
    constexpr uint32_t frames = sampleRate * seconds;
    for (int mode = 0; mode < throwfx::Mode::NumModes; ++mode) {
        const std::string filename = root + "/demo_mode_" + std::to_string(mode) + ".wav";
        std::ofstream f(filename, std::ios::binary);
        if (!f) { std::cerr << "Cannot open " << filename << '\n'; return 1; }
        f.write("RIFF", 4); u32(f, 36 + frames * 4); f.write("WAVEfmt ", 8);
        u32(f, 16); u16(f, 1); u16(f, 2); u32(f, sampleRate); u32(f, sampleRate * 4); u16(f, 4); u16(f, 16);
        f.write("data", 4); u32(f, frames * 4);
        throwfx::Engine engine; engine.prepare(sampleRate);
        throwfx::Parameters p; p.mode = mode; p.mix = 70; p.throwSend = 100; p.decay = 3.5;
        p.delayTime = 2; p.feedback = 68;
        for (uint32_t i = 0; i < frames; ++i) {
            const double t = static_cast<double>(i) / sampleRate;
            const int note = static_cast<int>(t / 0.5);
            const double age = std::fmod(t, 0.5);
            const double freq = (note % 4 == 0 ? 220.0 : note % 4 == 1 ? 261.63 : note % 4 == 2 ? 329.63 : 392.0);
            const float sound = t < 1.55 ? static_cast<float>(0.48 * std::exp(-age * 19.0) *
                 (std::sin(2 * 3.141592653589793 * t * freq) + 0.24 * std::sin(2 * 3.141592653589793 * t * freq * 2))) : 0.0f;
            // Capture the first 1.55 s, then release the throw and hear the tail.
            if (t >= 1.55) p.throwSend = 0;
            auto output = engine.process(sound, sound * 0.90f, p, 120.0);
            for (float v : output) {
                const float safe = std::max(-1.0f, std::min(1.0f, v));
                u16(f, static_cast<uint16_t>(static_cast<int16_t>(std::lrint(safe * 32767.0f))));
            }
        }
        std::cout << "Wrote " << filename << '\n';
    }
}
