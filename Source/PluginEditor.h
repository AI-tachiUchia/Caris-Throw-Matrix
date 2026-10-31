#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>

class MatrixLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    MatrixLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float rotaryStart, float rotaryEnd, juce::Slider&) override;
};

class MomentaryThrowPad final : public juce::Component {
public:
    explicit MomentaryThrowPad(juce::AudioProcessorValueTreeState&);
    ~MomentaryThrowPad() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool) override;
    void focusLost(FocusChangeType) override;
    void refresh();
private:
    void change(bool down);
    juce::RangedAudioParameter* parameter = nullptr;
    bool held = false;
};

class ThrowWaveformDragPad final : public juce::Component {
public:
    explicit ThrowWaveformDragPad(CarisThrowMatrixAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void refresh();
private:
    CarisThrowMatrixAudioProcessor& processor;
    bool dragStarted = false;
};

class CarisThrowMatrixAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                    public juce::DragAndDropContainer,
                                                    private juce::Timer {
public:
    explicit CarisThrowMatrixAudioProcessorEditor(CarisThrowMatrixAudioProcessor&);
    ~CarisThrowMatrixAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct Knob {
        juce::Slider slider;
        juce::Label title;
        juce::AudioProcessorValueTreeState::SliderAttachment attach;
        Knob(juce::AudioProcessorValueTreeState& state, const char* id, const char* label);
        void place(juce::Component& parent, juce::Rectangle<int> region);
    };
    void timerCallback() override;
    CarisThrowMatrixAudioProcessor& processor;
    MatrixLookAndFeel theme;
    juce::ComboBox modeChoice, timeChoice;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach, timeAttach;
    juce::ToggleButton sidechainToggle {"SIDECHAIN IN"};
    juce::ToggleButton wetOnlyToggle {"WET ONLY (KILL DRY)"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sidechainAttach, wetOnlyAttach;

    MomentaryThrowPad throwPad;
    ThrowWaveformDragPad waveformDragPad;
    juce::TextButton recPassButton {"REC PASS"};
    juce::TextButton clearRecButton {"CLEAR"};
    juce::TextButton openFolderButton {"FOLDER"};

    juce::Label bpmDisplay;
    juce::Label routingStatus;
    std::array<std::unique_ptr<Knob>, 11> controls;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarisThrowMatrixAudioProcessorEditor)
};
