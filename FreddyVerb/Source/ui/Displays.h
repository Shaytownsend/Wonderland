#pragma once

#include "Controls.h"

namespace fv::ui
{
/** Small EQ curve display with draggable nodes (wet EQ). */
class EQDisplay : public juce::Component, private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit EQDisplay (ControlHub&);
    ~EQDisplay() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Band selected by clicking a node: 0 HPF, 1 LS, 2 B1, 3 B2, 4 HS, 5 LPF. */
    std::function<void (int band)> onBandSelected;
    int getSelectedBand() const { return selected; }

    /** Analog-prototype magnitude (dB) of the whole EQ at f. Public for tests/UI. */
    static float magnitudeDb (juce::AudioProcessorValueTreeState&, float hz);

private:
    void parameterChanged (const juce::String&, float) override { triggerRepaint(); }
    void triggerRepaint() { juce::MessageManager::callAsync ([s = juce::Component::SafePointer<EQDisplay> (this)] { if (s) s->repaint(); }); }
    juce::Point<float> nodePos (int band) const;
    float xForHz (float hz) const;
    float hzForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    juce::Rectangle<float> plot;
    ControlHub& hub;
    int selected = 2, dragging = -1;
};

/** Waveform + decay view of the current IR with draggable start / length /
    pre-delay handles. The IR data comes from the IR manager (milestone 2). */
class IRDisplay : public juce::Component
{
public:
    explicit IRDisplay (ControlHub&);
    /** Thumbnail data: per-pixel peak of the shaped IR (0..1) and an optional
        decay spectrogram (rows = bands, cols = time, values 0..1). */
    void setThumbnail (std::vector<float> peaks, double lengthSeconds, juce::String name, double rt60);
    void setSpectrogram (std::vector<std::vector<float>> bands) { spectro = std::move (bands); repaint(); }
    void setMissing (bool m, juce::String path) { missing = m; missingPath = std::move (path); repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    std::function<void()> onRelinkRequested;
    bool showSpectrogram = false;

private:
    float xForMs (float ms) const;
    ControlHub& hub;
    std::vector<float> peaks;
    std::vector<std::vector<float>> spectro;
    double lengthSec = 1.0, rt60 = 1.0;
    juce::String irName { "MEDIUM PLATE" };
    juce::String missingPath;
    bool missing = false;
    juce::Rectangle<float> plot;
    int drag = -1, hover = -1;
    float dragStartX = 0, dragStartVal = 0;
};
} // namespace fv::ui
