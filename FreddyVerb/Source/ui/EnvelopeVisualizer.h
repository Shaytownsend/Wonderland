#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Controls.h"
#include "../dsp/VizFeed.h"
#include "../dsp/EngineParams.h"

namespace fv::ui
{
/** Scrolling real-time display of the duck -> bloom -> gate relationship.
    Data path: audio thread -> VizFeed (lock-free SPSC) -> pull() on the UI timer.
    When audio is stopped it shows the theoretical response to one synthetic hit
    (static preview), computed with the real dynamics code. */
class EnvelopeVisualizer : public juce::Component
{
public:
    EnvelopeVisualizer (ControlHub&, dsp::VizFeed&);

    /** Called by the editor's 60 fps timer. */
    void pull (bool audioRunning, const dsp::EngineParams& params, double sampleRate, double reverbRt60);

    void setTimeScaleMs (int ms);
    int  getTimeScaleMs() const noexcept { return timeScaleMs; }
    void setFrozen (bool f) { frozen = f; repaint(); }
    bool isFrozen() const noexcept { return frozen; }
    bool isShowingPreview() const noexcept { return previewMode; }

    /** For the snapshot tool: push frames directly and stop treating audio as stopped. */
    void injectFrames (const std::vector<dsp::VizFrame>& frames, double frameRate);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverHandle = -1; repaint(); }

    static constexpr int kTimeScales[5] = { 250, 500, 1000, 2000, 4000 };

private:
    struct Handle { juce::String paramID; juce::Point<float> pos; bool horizontal; juce::String label; };
    struct BloomSpan { int start, end; };

    void rebuildHandles();
    int  hitTestHandle (juce::Point<float>) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    float xForFrame (int frameFromRight) const;
    void computeBloomSpans (const std::vector<dsp::VizFrame>& v, std::vector<BloomSpan>& spans, int& lastOnset) const;
    void drawTraces (juce::Graphics&, juce::Rectangle<float> plot, const std::vector<dsp::VizFrame>& v, float glow);

    ControlHub& hub;
    dsp::VizFeed& feed;
    std::vector<dsp::VizFrame> history;   // ring buffer
    int writePos = 0;
    std::vector<dsp::VizFrame> view;      // linearised visible window (oldest -> newest)
    std::vector<dsp::VizFrame> preview;
    std::vector<dsp::VizFrame> popBuf;
    double frameRate = 750.0;
    int timeScaleMs = 1000;
    bool frozen = false, previewMode = true;
    bool injected = false;
    dsp::EngineParams lastParams;
    double previewHitMs = 0.0;

    juce::Rectangle<float> plotArea, inner, header;
    std::vector<Handle> handles;
    int hoverHandle = -1, dragHandle = -1;
    juce::Point<float> dragLast;
    juce::Image persistence;   // CRT phosphor persistence buffer
};
} // namespace fv::ui
