#pragma once
#include <JuceHeader.h>
#include "ThrowDSP.h"
#include <array>
#include <atomic>
#include <mutex>
#include <vector>

class CarisThrowMatrixAudioProcessor final : public juce::AudioProcessor {
public:
    CarisThrowMatrixAudioProcessor();
    ~CarisThrowMatrixAudioProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 24.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState state;
    double currentBpm() const noexcept { return displayedBpm.load(std::memory_order_relaxed); }
    float currentInputLevel() const noexcept { return inputMeter.load(std::memory_order_relaxed); }
    float currentSidechainLevel() const noexcept { return sidechainMeter.load(std::memory_order_relaxed); }
    bool isUsingSidechain() const noexcept { return sidechainActive.load(std::memory_order_relaxed); }

    // Internal Throw Recorder & Drag-and-Drop WAV export API
    void startManualPassRecording() noexcept;
    void stopRecording() noexcept;
    void clearRecording() noexcept;
    void setAutoCaptureEnabled(bool enabled) noexcept { autoCapture.store(enabled, std::memory_order_relaxed); }
    bool isAutoCaptureEnabled() const noexcept { return autoCapture.load(std::memory_order_relaxed); }
    bool isRecordingNow() const noexcept { return recordingState.load(std::memory_order_relaxed) != 0; }
    bool isManualPassRecording() const noexcept { return recordingState.load(std::memory_order_relaxed) == 2; }
    double capturedDurationSeconds() const noexcept;
    std::size_t capturedFrameCount() const noexcept { return capturedFrames.load(std::memory_order_relaxed); }
    std::array<float, 96> getWaveformPeaks() const noexcept;
    juce::File exportCapturedWavToFile();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout();
    throwfx::Engine engine;
    std::atomic<double> displayedBpm {120.0};
    std::atomic<float> inputMeter {0.0f};
    std::atomic<float> sidechainMeter {0.0f};
    std::atomic<bool> sidechainActive {false};

    // Lock-free preallocated stereo recorder state (0 = idle, 1 = auto-throw+tail, 2 = manual pass)
    double currentSampleRate = 48000.0;
    std::vector<float> captureL, captureR;
    std::atomic<std::size_t> capturedFrames {0};
    std::atomic<int> recordingState {0};
    std::atomic<bool> autoCapture {true};
    std::atomic<bool> clearRequested {false};
    std::size_t silenceFramesAfterRelease = 0;
    std::size_t framesSinceRelease = 0;
    bool wasThrowing = false;
    juce::File lastExportedFile;
    std::size_t lastExportedFrameCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarisThrowMatrixAudioProcessor)
};
