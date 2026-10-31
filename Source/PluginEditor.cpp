#include "PluginEditor.h"
#include <cmath>

namespace {
const juce::Colour canvas {0xff0b1015};
const juce::Colour tile {0xff151c23};
const juce::Colour stroke {0xff2c3842};
const juce::Colour text {0xffe6edf0};
const juce::Colour muted {0xff88969e};
const juce::Colour accent {0xff80d2bd};
const juce::Colour recRed {0xffff5c6c};
const juce::Colour readyGold {0xfff0b85a};

void labelStyle(juce::Label& l, float size, juce::Colour colour) {
    l.setFont(juce::Font(juce::FontOptions(size)));
    l.setColour(juce::Label::textColourId, colour);
}
} // namespace

MatrixLookAndFeel::MatrixLookAndFeel() {
    setColour(juce::ComboBox::backgroundColourId, tile);
    setColour(juce::ComboBox::outlineColourId, stroke);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::arrowColourId, accent);
    setColour(juce::PopupMenu::backgroundColourId, tile);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.darker(0.65f));
    setColour(juce::PopupMenu::highlightedTextColourId, text);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::buttonColourId, tile);
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff23363f));
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, accent);
    setColour(juce::ToggleButton::textColourId, text);
    setColour(juce::ToggleButton::tickColourId, accent);
    setColour(juce::ToggleButton::tickDisabledColourId, muted);
}

void MatrixLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float pos, float start, float end, juce::Slider&) {
    const float radius = std::min(width, height) * 0.36f;
    const auto centre = juce::Point<float>(x + width * 0.5f, y + height * 0.5f);
    const auto dial = juce::Rectangle<float>(centre.x - radius, centre.y - radius, 2 * radius, 2 * radius);
    g.setColour(juce::Colour(0xff202a31));
    g.fillEllipse(dial);
    g.setColour(stroke);
    g.drawEllipse(dial, 1.0f);
    auto track = juce::Path();
    track.addCentredArc(centre.x, centre.y, radius + 6, radius + 6, 0.0f, start, end, true);
    g.setColour(juce::Colour(0xff3a4851));
    g.strokePath(track, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto active = juce::Path();
    active.addCentredArc(centre.x, centre.y, radius + 6, radius + 6, 0.0f, start, start + pos * (end - start), true);
    g.setColour(accent);
    g.strokePath(active, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto marker = juce::Point<float>(centre.x + std::sin(start + pos * (end - start)) * radius * 0.65f,
                                    centre.y - std::cos(start + pos * (end - start)) * radius * 0.65f);
    g.setColour(text);
    g.drawLine(centre.x, centre.y, marker.x, marker.y, 2.5f);
}

MomentaryThrowPad::MomentaryThrowPad(juce::AudioProcessorValueTreeState& state)
    : parameter(state.getParameter("throwSend")) {
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
MomentaryThrowPad::~MomentaryThrowPad() { change(false); }
void MomentaryThrowPad::change(bool down) {
    if (down == held || parameter == nullptr) return;
    held = down;
    if (down) parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(down ? 100.0f : 0.0f));
    if (!down) parameter->endChangeGesture();
    repaint();
}
void MomentaryThrowPad::mouseDown(const juce::MouseEvent&) { grabKeyboardFocus(); change(true); }
void MomentaryThrowPad::mouseUp(const juce::MouseEvent&) { change(false); }
void MomentaryThrowPad::focusLost(FocusChangeType) { change(false); }
bool MomentaryThrowPad::keyPressed(const juce::KeyPress& key) {
    if (key.getKeyCode() == juce::KeyPress::spaceKey) { change(true); return true; }
    return false;
}
bool MomentaryThrowPad::keyStateChanged(bool) {
    if (held && !juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey)) change(false);
    return false;
}
void MomentaryThrowPad::refresh() { repaint(); }
void MomentaryThrowPad::paint(juce::Graphics& g) {
    const bool live = parameter && parameter->getValue() > 0.001f;
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(live ? juce::Colour(0xff193d34) : tile);
    g.fillRoundedRectangle(bounds, 15.0f);
    g.setColour(live ? accent : stroke);
    g.drawRoundedRectangle(bounds, 15.0f, live ? 2.0f : 1.0f);
    g.setColour(live ? accent : text);
    g.setFont(juce::Font(juce::FontOptions(23.0f, juce::Font::bold)));
    g.drawText(live ? "THROW ACTIVE" : "HOLD TO THROW", bounds.removeFromTop(58.0f), juce::Justification::centredBottom);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText("Hold mouse or Spacebar  |  Auto-captures wet stereo throw for Drag & Drop", bounds, juce::Justification::centredTop);
}

ThrowWaveformDragPad::ThrowWaveformDragPad(CarisThrowMatrixAudioProcessor& p)
    : processor(p) {
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}

void ThrowWaveformDragPad::refresh() { repaint(); }

void ThrowWaveformDragPad::mouseDown(const juce::MouseEvent&) {
    dragStarted = false;
}

void ThrowWaveformDragPad::mouseDrag(const juce::MouseEvent& e) {
    if (dragStarted || e.getDistanceFromDragStart() < 5) return;
    if (processor.capturedFrameCount() == 0) return;

    dragStarted = true;
    const juce::File wavFile = processor.exportCapturedWavToFile();
    if (wavFile.existsAsFile()) {
        juce::DragAndDropContainer::performExternalDragDropOfFiles(
            { wavFile.getFullPathName() }, false, this);
    }
}

void ThrowWaveformDragPad::mouseUp(const juce::MouseEvent&) {
    dragStarted = false;
}

void ThrowWaveformDragPad::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const bool recording = processor.isRecordingNow();
    const bool hasAudio = processor.capturedFrameCount() > 0;

    g.setColour(recording ? juce::Colour(0xff2c171c) : (hasAudio ? juce::Colour(0xff142529) : tile));
    g.fillRoundedRectangle(bounds, 13.0f);
    g.setColour(recording ? recRed : (hasAudio ? accent : stroke));
    g.drawRoundedRectangle(bounds, 13.0f, (recording || hasAudio) ? 1.6f : 1.0f);

    // Draw captured waveform bars
    auto waveArea = bounds.reduced(14.0f, 26.0f);
    const auto peaks = processor.getWaveformPeaks();
    const float barW = waveArea.getWidth() / static_cast<float>(peaks.size());
    const float midY = waveArea.getCentreY();
    for (std::size_t i = 0; i < peaks.size(); ++i) {
        const float norm = std::pow(peaks[i], 0.65f);
        const float halfH = std::max(1.5f, norm * (waveArea.getHeight() * 0.48f));
        g.setColour(recording ? recRed.withAlpha(0.85f)
                              : (hasAudio ? accent.withAlpha(0.85f) : stroke.withAlpha(0.45f)));
        g.fillRoundedRectangle(waveArea.getX() + i * barW, midY - halfH,
                               std::max(1.5f, barW - 1.2f), halfH * 2.0f, 1.0f);
    }

    // Status header & drag instruction
    auto topBar = bounds.reduced(12.0f, 6.0f).removeFromTop(20.0f);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    if (recording) {
        g.setColour(recRed);
        const juce::String modeStr = processor.isManualPassRecording() ? "REC PASS: " : "CAPTURING THROW + TAIL: ";
        g.drawText(modeStr + juce::String(processor.capturedDurationSeconds(), 1) + "s",
                   topBar, juce::Justification::centredLeft);
    } else if (hasAudio) {
        g.setColour(accent);
        g.drawText("DRAG WAV TO LOGIC TRACK  (" + juce::String(processor.capturedDurationSeconds(), 1) + "s Stereo)",
                   topBar, juce::Justification::centredLeft);
    } else {
        g.setColour(muted);
        g.drawText("THROW RECORDER  (Auto-records when you press Throw)",
                   topBar, juce::Justification::centredLeft);
    }

    auto bottomBar = bounds.reduced(12.0f, 5.0f).removeFromBottom(18.0f);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.setColour(hasAudio ? readyGold : muted);
    g.drawText(hasAudio ? "Click & drag this waveform directly onto an audio track in Logic Pro"
                        : "Or click REC PASS to record multiple throws in time with your vocal pass",
               bottomBar, juce::Justification::centredLeft);
}

CarisThrowMatrixAudioProcessorEditor::Knob::Knob(juce::AudioProcessorValueTreeState& state,
                                                 const char* id, const char* name)
    : attach(state, id, slider) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 95, 20);
    slider.setScrollWheelEnabled(false);
    const auto* param = state.getParameter(id);
    slider.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
    title.setText(name, juce::dontSendNotification);
    labelStyle(title, 12.0f, muted);
    title.setJustificationType(juce::Justification::centred);
}
void CarisThrowMatrixAudioProcessorEditor::Knob::place(juce::Component& parent, juce::Rectangle<int> area) {
    slider.setBounds(area.withTrimmedBottom(22));
    title.setBounds(area.removeFromBottom(23));
    if (slider.getParentComponent() != &parent) parent.addAndMakeVisible(slider);
    if (title.getParentComponent() != &parent) parent.addAndMakeVisible(title);
}

CarisThrowMatrixAudioProcessorEditor::CarisThrowMatrixAudioProcessorEditor(CarisThrowMatrixAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), throwPad(p.state), waveformDragPad(p) {
    setLookAndFeel(&theme);
    modeChoice.addItemList({
        "Dub Delay",
        "Space Reverb",
        "Shimmer Verb",
        "Tape PingPong",
        "Spring Reverb (Adlib)",
        "Distorted Pitch Throw",
        "Phaser Swirl Throw"
    }, 1);
    timeChoice.addItemList({"1/16", "1/8D", "1/8", "1/4D", "1/4", "1/2"}, 1);
    modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "throwMode", modeChoice);
    timeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "delayTime", timeChoice);
    sidechainAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "useSidechain", sidechainToggle);
    wetOnlyAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "wetOnly", wetOnlyToggle);

    addAndMakeVisible(modeChoice);
    addAndMakeVisible(timeChoice);
    addAndMakeVisible(sidechainToggle);
    addAndMakeVisible(wetOnlyToggle);
    addAndMakeVisible(throwPad);
    addAndMakeVisible(waveformDragPad);

    recPassButton.onClick = [this] {
        if (processor.isRecordingNow())
            processor.stopRecording();
        else
            processor.startManualPassRecording();
        waveformDragPad.refresh();
    };
    clearRecButton.onClick = [this] {
        processor.clearRecording();
        waveformDragPad.refresh();
    };
    openFolderButton.onClick = [this] {
        auto file = processor.exportCapturedWavToFile();
        if (file.existsAsFile()) {
            file.revealToUser();
        } else {
            auto dir = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                           .getChildFile("CarisThrowMatrix").getChildFile("Captures");
            dir.createDirectory();
            dir.revealToUser();
        }
    };
    addAndMakeVisible(recPassButton);
    addAndMakeVisible(clearRecButton);
    addAndMakeVisible(openFolderButton);

    labelStyle(bpmDisplay, 13.0f, accent);
    bpmDisplay.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(bpmDisplay);

    labelStyle(routingStatus, 12.0f, muted);
    routingStatus.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(routingStatus);

    constexpr const char* ids[11] {
        "feedback", "decay", "pitchSemitones", "phaserAmount", "drive", "filterCutoff",
        "damping", "stereoSpread", "duckAmount", "outputGain", "mix"
    };
    constexpr const char* names[11] {
        "FEEDBACK", "DECAY", "PITCH (ST)", "PHASER", "DRIVE / DIST", "FILTER",
        "DAMPING", "STEREO", "AUTO DUCK", "OUTPUT", "DRY / WET"
    };
    for (int i = 0; i < 11; ++i) controls[i] = std::make_unique<Knob>(p.state, ids[i], names[i]);
    setSize(940, 640);
    startTimerHz(15);
}

CarisThrowMatrixAudioProcessorEditor::~CarisThrowMatrixAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void CarisThrowMatrixAudioProcessorEditor::timerCallback() {
    bpmDisplay.setText(juce::String(processor.currentBpm(), 1) + " BPM  |  MONO->STEREO & SIDECHAIN READY",
                       juce::dontSendNotification);
    const bool usingSc = processor.isUsingSidechain();
    const float scLvl = processor.currentSidechainLevel();
    const float inLvl = processor.currentInputLevel();
    const juce::String srcText = usingSc
        ? ("SOURCE: SIDECHAIN ACTIVE (lvl " + juce::String(scLvl, 2) + ")")
        : ("SOURCE: TRACK INPUT (lvl " + juce::String(inLvl, 2) + ")");
    routingStatus.setText(srcText, juce::dontSendNotification);
    routingStatus.setColour(juce::Label::textColourId, usingSc ? readyGold : muted);

    recPassButton.setButtonText(processor.isRecordingNow() ? "STOP REC" : "REC PASS");
    recPassButton.setColour(juce::TextButton::buttonColourId,
                            processor.isRecordingNow() ? recRed.darker(0.35f) : tile);

    throwPad.refresh();
    waveformDragPad.refresh();
}

void CarisThrowMatrixAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(canvas);
    g.setColour(accent);
    g.fillRoundedRectangle(31.0f, 25.0f, 4.0f, 43.0f, 2.0f);
    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
    g.drawText("THROW / MATRIX", 47, 20, 380, 34, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText("CARIS  /  SPRING, PITCH & PHASER THROWS  /  DRAG-TO-DAW RECORDER  /  v0.2",
               49, 55, 520, 18, juce::Justification::centredLeft);
    g.setColour(stroke);
    g.drawLine(31.0f, 84.0f, 909.0f, 84.0f, 1.0f);

    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("EFFECT MODE", 34, 96, 260, 18, juce::Justification::centredLeft);
    g.drawText("BEAT DIVISION", 316, 96, 160, 18, juce::Justification::centredLeft);
    g.drawText("ROUTING & SIDECHAIN", 495, 96, 300, 18, juce::Justification::centredLeft);

    g.setColour(tile);
    g.fillRoundedRectangle(28.0f, 326.0f, 884.0f, 278.0f, 13.0f);
    g.setColour(stroke);
    g.drawRoundedRectangle(28.0f, 326.0f, 884.0f, 278.0f, 13.0f, 1.0f);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.drawText("THROW CHARACTER: TIME, PITCH SHIFT, PHASER SWIRL, DISTORTION & FILTER",
               42, 336, 520, 16, juce::Justification::centredLeft);
    g.drawText("TAIL, DUCKING & MIX", 640, 336, 255, 16, juce::Justification::centredRight);
    g.drawText("Tip: Select a Vocal in Logic's top-right 'Side Chain' menu or use directly on a Mono/Stereo track. Drag recorded throws straight onto a track.",
               30, 612, 880, 18, juce::Justification::centredLeft);
}

void CarisThrowMatrixAudioProcessorEditor::resized() {
    bpmDisplay.setBounds(520, 22, 388, 24);
    routingStatus.setBounds(520, 48, 388, 22);

    modeChoice.setBounds(34, 118, 270, 36);
    timeChoice.setBounds(316, 118, 160, 36);
    sidechainToggle.setBounds(495, 118, 165, 36);
    wetOnlyToggle.setBounds(670, 118, 235, 36);

    throwPad.setBounds(28, 170, 420, 140);
    waveformDragPad.setBounds(462, 170, 362, 140);
    recPassButton.setBounds(832, 170, 80, 42);
    clearRecButton.setBounds(832, 219, 80, 42);
    openFolderButton.setBounds(832, 268, 80, 42);

    constexpr int knobWidth = 136;
    // Top row: 6 knobs (Feedback, Decay, Pitch, Phaser, Drive, Filter)
    for (int i = 0; i < 6; ++i)
        controls[i]->place(*this, juce::Rectangle<int>(40 + i * 144, 360, knobWidth, 108));
    // Bottom row: 5 knobs (Damping, Stereo, Auto Duck, Output, Dry/Wet)
    for (int i = 0; i < 5; ++i)
        controls[6 + i]->place(*this, juce::Rectangle<int>(95 + i * 156, 482, knobWidth, 108));
}
