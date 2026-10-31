#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace {
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using Range = juce::NormalisableRange<float>;
void addFloat(Layout& layout, const char* id, const char* name,
              float lo, float hi, float step, float defaultValue) {
    Range range {lo, hi, step};
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{id, 1}, name, range, defaultValue));
}
} // namespace

Layout CarisThrowMatrixAudioProcessor::makeParameterLayout() {
    Layout layout;
    // Stable parameter IDs; host automation survives plugin rebuilds.
    addFloat(layout, "throwSend", "Throw Send", 0.0f, 100.0f, 0.1f, 0.0f);
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"throwMode", 1}, "Throw Mode",
        juce::StringArray{
            "Dub Delay",
            "Space Reverb",
            "Shimmer Verb",
            "Tape PingPong",
            "Spring Reverb (Adlib)",
            "Distorted Pitch Throw",
            "Phaser Swirl Throw"
        }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"delayTime", 1}, "Delay Time",
        juce::StringArray{"1/16", "1/8D", "1/8", "1/4D", "1/4", "1/2"}, 4));
    addFloat(layout, "feedback", "Feedback", 0.0f, 100.0f, 0.1f, 65.0f);
    addFloat(layout, "decay", "Decay", 0.5f, 12.0f, 0.01f, 4.5f);
    addFloat(layout, "filterCutoff", "Filter Cutoff", 200.0f, 18000.0f, 1.0f, 4500.0f);
    addFloat(layout, "damping", "Damping", 0.0f, 100.0f, 0.1f, 40.0f);
    addFloat(layout, "drive", "Drive", 0.0f, 100.0f, 0.1f, 25.0f);
    addFloat(layout, "stereoSpread", "Stereo Spread", 0.0f, 100.0f, 0.1f, 75.0f);
    addFloat(layout, "duckAmount", "Duck Amount", 0.0f, 100.0f, 0.1f, 50.0f);
    addFloat(layout, "outputGain", "Output Gain", -60.0f, 12.0f, 0.1f, 0.0f);
    addFloat(layout, "mix", "Mix", 0.0f, 100.0f, 0.1f, 50.0f);
    addFloat(layout, "pitchSemitones", "Pitch Shift", -12.0f, 12.0f, 0.1f, 0.0f);
    addFloat(layout, "phaserAmount", "Phaser Swirl", 0.0f, 100.0f, 0.1f, 0.0f);
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"useSidechain", 1}, "Sidechain Input", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"wetOnly", 1}, "Wet Only (Kill Dry)", false));
    return layout;
}

CarisThrowMatrixAudioProcessor::CarisThrowMatrixAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "CarisThrowMatrixState", makeParameterLayout()) {}

bool CarisThrowMatrixAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    // Support Stereo->Stereo, Mono->Stereo (crucial for Logic Pro mono vocal tracks!), and Mono->Mono.
    const bool mainValid =
        (input == juce::AudioChannelSet::stereo() && output == juce::AudioChannelSet::stereo()) ||
        (input == juce::AudioChannelSet::mono()   && output == juce::AudioChannelSet::stereo()) ||
        (input == juce::AudioChannelSet::mono()   && output == juce::AudioChannelSet::mono());
    if (!mainValid) return false;

    if (layouts.inputBuses.size() > 1) {
        const auto sc = layouts.getChannelSet(true, 1);
        if (!sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void CarisThrowMatrixAudioProcessor::prepareToPlay(double sampleRate, int) {
    currentSampleRate = std::max(8000.0, sampleRate);
    engine.prepare(currentSampleRate);
    // Pre-allocate 60 seconds of stereo capture buffer so the audio thread never allocates.
    const auto maxCaptureFrames = static_cast<std::size_t>(currentSampleRate * 60.0);
    captureL.assign(maxCaptureFrames, 0.0f);
    captureR.assign(maxCaptureFrames, 0.0f);
    capturedFrames.store(0, std::memory_order_relaxed);
    recordingState.store(0, std::memory_order_relaxed);
    silenceFramesAfterRelease = 0;
    framesSinceRelease = 0;
    wasThrowing = false;
}

void CarisThrowMatrixAudioProcessor::releaseResources() { engine.reset(); }

void CarisThrowMatrixAudioProcessor::startManualPassRecording() noexcept {
    clearRequested.store(true, std::memory_order_relaxed);
    recordingState.store(2, std::memory_order_relaxed);
}

void CarisThrowMatrixAudioProcessor::stopRecording() noexcept {
    recordingState.store(0, std::memory_order_relaxed);
}

void CarisThrowMatrixAudioProcessor::clearRecording() noexcept {
    recordingState.store(0, std::memory_order_relaxed);
    clearRequested.store(true, std::memory_order_relaxed);
    capturedFrames.store(0, std::memory_order_relaxed);
}

double CarisThrowMatrixAudioProcessor::capturedDurationSeconds() const noexcept {
    return static_cast<double>(capturedFrames.load(std::memory_order_relaxed)) / std::max(8000.0, currentSampleRate);
}

std::array<float, 96> CarisThrowMatrixAudioProcessor::getWaveformPeaks() const noexcept {
    std::array<float, 96> peaks{};
    const auto total = std::min(capturedFrames.load(std::memory_order_relaxed), captureL.size());
    if (total == 0) return peaks;
    for (std::size_t bin = 0; bin < peaks.size(); ++bin) {
        const std::size_t start = (bin * total) / peaks.size();
        const std::size_t end = std::max(start + 1, ((bin + 1) * total) / peaks.size());
        float peak = 0.0f;
        for (std::size_t i = start; i < end && i < total; ++i) {
            const float v = std::max(std::abs(captureL[i]), std::abs(captureR[i]));
            if (v > peak) peak = v;
        }
        peaks[bin] = std::min(1.0f, peak);
    }
    return peaks;
}

juce::File CarisThrowMatrixAudioProcessor::exportCapturedWavToFile() {
    // If an auto-capture tail is still running when the user grabs the drag handle, finalize it cleanly.
    if (recordingState.load(std::memory_order_relaxed) == 1)
        recordingState.store(0, std::memory_order_relaxed);

    const auto frames = std::min(capturedFrames.load(std::memory_order_relaxed), captureL.size());
    if (frames == 0) return {};

    if (lastExportedFile.existsAsFile() && lastExportedFrameCount == frames)
        return lastExportedFile;

    juce::File dir = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                         .getChildFile("CarisThrowMatrix")
                         .getChildFile("Captures");
    if (!dir.createDirectory())
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("CarisThrowMatrixCaptures");
    dir.createDirectory();

    const auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    juce::File outFile = dir.getNonexistentChildFile("CarisThrow_" + timestamp, ".wav", false);

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> fileStream(outFile.createOutputStream());
    if (!fileStream) return {};

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(fileStream.get(), currentSampleRate, 2, 24, {}, 0));
    if (!writer) return {};
    fileStream.release(); // writer now owns fileStream

    juce::AudioBuffer<float> tempBuffer(2, static_cast<int>(frames));
    std::copy_n(captureL.data(), frames, tempBuffer.getWritePointer(0));
    std::copy_n(captureR.data(), frames, tempBuffer.getWritePointer(1));
    writer->writeFromAudioSampleBuffer(tempBuffer, 0, static_cast<int>(frames));
    writer->flush();

    lastExportedFile = outFile;
    lastExportedFrameCount = frames;
    return outFile;
}

void CarisThrowMatrixAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    auto load = [this](const char* id) noexcept {
        return state.getRawParameterValue(id)->load(std::memory_order_relaxed);
    };
    throwfx::Parameters p;
    p.throwSend = load("throwSend");
    p.mode = static_cast<int>(std::lround(load("throwMode")));
    p.delayTime = static_cast<int>(std::lround(load("delayTime")));
    p.feedback = load("feedback");
    p.decay = load("decay");
    p.cutoff = load("filterCutoff");
    p.damping = load("damping");
    p.drive = load("drive");
    p.spread = load("stereoSpread");
    p.duck = load("duckAmount");
    p.outputDb = load("outputGain");
    p.mix = load("mix");
    p.pitchSemitones = load("pitchSemitones");
    p.phaserAmount = load("phaserAmount");
    p.wetOnly = load("wetOnly") > 0.5f;
    const bool forceSidechain = load("useSidechain") > 0.5f;

    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (const auto info = playHead->getPosition())
            if (auto hostBpm = info->getBpm(); hostBpm && std::isfinite(*hostBpm) && *hostBpm > 0.0)
                bpm = *hostBpm;
    displayedBpm.store(bpm, std::memory_order_relaxed);

    auto mainInBus = getBusBuffer(buffer, true, 0);
    auto mainOutBus = getBusBuffer(buffer, false, 0);
    const bool hasScBus = (getBusCount(true) > 1) && (getBus(true, 1)->isEnabled());
    auto scBus = hasScBus ? getBusBuffer(buffer, true, 1) : juce::AudioBuffer<float>();

    const int numSamples = buffer.getNumSamples();
    const int mainInCh = mainInBus.getNumChannels();
    const int mainOutCh = mainOutBus.getNumChannels();
    const int scCh = scBus.getNumChannels();

    const float* inL = mainInCh > 0 ? mainInBus.getReadPointer(0) : nullptr;
    const float* inR = mainInCh > 1 ? mainInBus.getReadPointer(1) : inL;
    const float* scL = scCh > 0 ? scBus.getReadPointer(0) : nullptr;
    const float* scR = scCh > 1 ? scBus.getReadPointer(1) : scL;

    float* outL = mainOutCh > 0 ? mainOutBus.getWritePointer(0) : nullptr;
    float* outR = mainOutCh > 1 ? mainOutBus.getWritePointer(1) : nullptr;

    // Measure peak levels on main input and sidechain input
    float mainPeak = 0.0f;
    float scPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        if (inL) mainPeak = std::max(mainPeak, std::max(std::abs(inL[i]), std::abs(inR ? inR[i] : inL[i])));
        if (scL) scPeak = std::max(scPeak, std::max(std::abs(scL[i]), std::abs(scR ? scR[i] : scL[i])));
    }
    inputMeter.store(mainPeak, std::memory_order_relaxed);
    sidechainMeter.store(scPeak, std::memory_order_relaxed);

    // Use sidechain if explicitly toggled OR if sidechain has signal while main track input is silent
    const bool useSc = (scL != nullptr) && (forceSidechain || (scPeak > 0.0002f && mainPeak < 0.00005f));
    sidechainActive.store(useSc, std::memory_order_relaxed);

    if (clearRequested.exchange(false, std::memory_order_relaxed)) {
        capturedFrames.store(0, std::memory_order_relaxed);
        silenceFramesAfterRelease = 0;
        framesSinceRelease = 0;
    }

    const bool throwingNow = (p.throwSend > 0.5f);
    int recState = recordingState.load(std::memory_order_relaxed);

    // Auto-capture management when not in manual pass mode
    if (recState != 2 && autoCapture.load(std::memory_order_relaxed)) {
        if (throwingNow && !wasThrowing) {
            if (recState == 0) {
                // Begin a fresh throw capture
                capturedFrames.store(0, std::memory_order_relaxed);
                recState = 1;
                recordingState.store(1, std::memory_order_relaxed);
            }
            silenceFramesAfterRelease = 0;
            framesSinceRelease = 0;
        }
    }
    wasThrowing = throwingNow;

    std::size_t writePos = capturedFrames.load(std::memory_order_relaxed);
    const std::size_t maxFrames = captureL.size();
    const std::size_t minTailFrames = static_cast<std::size_t>(currentSampleRate * 0.9);
    const std::size_t silenceStopFrames = static_cast<std::size_t>(currentSampleRate * 0.45);

    for (int i = 0; i < numSamples; ++i) {
        // Read inputs BEFORE writing outputs (safe even when Mono->Stereo + Sidechain share buffer channels)
        const float dryL = inL ? inL[i] : 0.0f;
        const float dryR = inR ? inR[i] : dryL;
        const float sideL = scL ? scL[i] : 0.0f;
        const float sideR = scR ? scR[i] : sideL;

        const float srcL = useSc ? sideL : dryL;
        const float srcR = useSc ? sideR : dryR;

        // When feeding from sidechain, output 100% wet throw so the sidechain dry vocal never bleeds through
        throwfx::Parameters frameParams = p;
        if (useSc)
            frameParams.wetOnly = true;

        const auto result = engine.process(srcL, srcR, frameParams, bpm);
        const auto wetFrame = engine.lastWetFrame();

        if (outL && outR) {
            outL[i] = result[0];
            outR[i] = result[1];
        } else if (outL) {
            outL[i] = (result[0] + result[1]) * 0.5f;
        }

        // Record stereo 100% wet throw into capture buffer
        if (recState != 0 && writePos < maxFrames) {
            captureL[writePos] = wetFrame[0];
            captureR[writePos] = wetFrame[1];
            ++writePos;

            if (recState == 1) {
                if (throwingNow) {
                    silenceFramesAfterRelease = 0;
                    framesSinceRelease = 0;
                } else {
                    ++framesSinceRelease;
                    const float wetMag = std::max(std::abs(wetFrame[0]), std::abs(wetFrame[1]));
                    if (wetMag < 0.0004f)
                        ++silenceFramesAfterRelease;
                    else
                        silenceFramesAfterRelease = 0;

                    if (framesSinceRelease > minTailFrames && silenceFramesAfterRelease > silenceStopFrames) {
                        recState = 0;
                        recordingState.store(0, std::memory_order_relaxed);
                    }
                }
            }
        } else if (recState != 0 && writePos >= maxFrames) {
            recState = 0;
            recordingState.store(0, std::memory_order_relaxed);
        }
    }

    capturedFrames.store(writePos, std::memory_order_relaxed);

    // Clear any extra output channels beyond stereo
    for (int c = mainOutCh; c < getTotalNumOutputChannels(); ++c)
        buffer.clear(c, 0, numSamples);
}

void CarisThrowMatrixAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void CarisThrowMatrixAudioProcessor::setStateInformation(const void* src, int bytes) {
    if (auto xml = getXmlFromBinary(src, bytes))
        if (xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* CarisThrowMatrixAudioProcessor::createEditor() {
    return new CarisThrowMatrixAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new CarisThrowMatrixAudioProcessor();
}
