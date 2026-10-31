#include "EnvelopeVisualizer.h"
#include "../dsp/EnvelopePreview.h"
#include "../dsp/DspCommon.h"
#include "../state/Parameters.h"
#include <array>

namespace fv::ui
{
namespace
{
    constexpr float kTopDb = 6.0f, kBottomDb = -60.0f;
    constexpr int kHistory = 1 << 14;
    inline float toDb (float g) { return g > 1.0e-6f ? 20.0f * std::log10 (g) : -120.0f; }
}

EnvelopeVisualizer::EnvelopeVisualizer (ControlHub& h, dsp::VizFeed& f) : hub (h), feed (f)
{
    history.assign (kHistory, {});
    popBuf.resize (dsp::VizFeed::kCapacity);
    setTitle ("Envelope display");
    setDescription ("Shows dry input, wet signal, duck gain, gate gain and the bloom window");
}

void EnvelopeVisualizer::setTimeScaleMs (int ms)
{
    timeScaleMs = juce::jlimit (250, 4000, ms);
    repaint();
}

void EnvelopeVisualizer::injectFrames (const std::vector<dsp::VizFrame>& frames, double fr)
{
    frameRate = fr;
    for (auto& fr2 : frames) { history[(size_t) writePos] = fr2; writePos = (writePos + 1) & (kHistory - 1); }
    injected = true;
    previewMode = false;
    repaint();
}

void EnvelopeVisualizer::pull (bool audioRunning, const dsp::EngineParams& params, double sampleRate, double rt60)
{
    const int n = feed.pop (popBuf.data(), (int) popBuf.size());
    frameRate = feed.frameRate.load();
    if (! frozen)
        for (int i = 0; i < n; ++i) { history[(size_t) writePos] = popBuf[(size_t) i]; writePos = (writePos + 1) & (kHistory - 1); }

    const bool wantPreview = ! audioRunning && ! injected;
    auto key = [] (const dsp::EngineParams& e)
    {
        return std::array<float, 26> { (float) e.duckOn, (float) e.duckMode, e.duckDepthDb, e.duckThreshDb, e.duckAttackMs,
            e.duckReleaseMs, e.duckAmount, (float) e.duckRelCurve, (float) e.curveShape, e.curveDurMs, e.curveTension,
            (float) e.curveRetrig, (float) e.gateOn, e.gateThreshDb, e.gateHystDb, e.gateAttackMs, e.gateHoldMs,
            e.gateReleaseMs, e.gateFloorDb, (float) e.gateRelCurve, e.keyHpfHz, e.keyLpfHz, (float) e.detMode, e.rmsMs,
            e.curveGuardMs, e.curveHystDb };
    };
    const bool paramsChanged = key (params) != key (lastParams);
    if (wantPreview && (paramsChanged || ! previewMode || preview.empty()))
    {
        previewHitMs = timeScaleMs * 0.12;
        dsp::renderEnvelopePreview (params, sampleRate, previewHitMs, timeScaleMs, rt60, preview);
        frameRate = sampleRate / dsp::kVizDecimation;
    }
    previewMode = wantPreview;
    lastParams = params;
    if (n > 0 || paramsChanged || previewMode) repaint();
}

void EnvelopeVisualizer::resized()
{
    persistence = {};
}

float EnvelopeVisualizer::yForDb (float db) const
{
    const float x = (juce::jlimit (kBottomDb, kTopDb, db) - kBottomDb) / (kTopDb - kBottomDb);
    return plotArea.getBottom() - x * plotArea.getHeight();
}

float EnvelopeVisualizer::dbForY (float y) const
{
    const float x = (plotArea.getBottom() - y) / plotArea.getHeight();
    return kBottomDb + x * (kTopDb - kBottomDb);
}

void EnvelopeVisualizer::computeBloomSpans (const std::vector<dsp::VizFrame>& v, std::vector<BloomSpan>& spans, int& lastOnset) const
{
    spans.clear();
    lastOnset = -1;
    const float thr = lastParams.gateThreshDb;
    bool above = false;
    int onset = -1, bloomStart = -1;
    float minDuck = 2.0f;
    for (int i = 0; i < (int) v.size(); ++i)
    {
        const auto& f = v[(size_t) i];
        const bool nowAbove = f.keyDb > thr;
        if (nowAbove && ! above)
        {
            if (bloomStart >= 0) spans.push_back ({ bloomStart, i });
            onset = i; lastOnset = i; bloomStart = -1; minDuck = 2.0f;
        }
        above = nowAbove;
        if (onset < 0) continue;
        minDuck = std::min (minDuck, f.duckGain);
        // bloom starts when the duck gain begins to recover from its minimum
        if (bloomStart < 0 && minDuck < 0.97f && f.duckGain > minDuck * 1.12f + 0.005f && ! nowAbove)
            bloomStart = i;
        if (! lastParams.duckOn && bloomStart < 0 && ! nowAbove) bloomStart = i;
        // ... and ends when the gate has closed (or the duck never released)
        if (bloomStart >= 0 && lastParams.gateOn && f.gateGain < 0.5f)
        {
            spans.push_back ({ bloomStart, i });
            bloomStart = -1; onset = -1;
        }
    }
    if (bloomStart >= 0) spans.push_back ({ bloomStart, (int) v.size() - 1 });
}

void EnvelopeVisualizer::drawTraces (juce::Graphics& g, juce::Rectangle<float> plot, const std::vector<dsp::VizFrame>& v, float glow)
{
    auto* lnf = freddyLnf (*this);
    const auto& c = lnf->theme().c;
    const int nFrames = (int) v.size();
    if (nFrames < 2) return;
    const float pxPerFrame = plot.getWidth() / (float) (nFrames - 1);

    // --- dry + wet filled envelopes (column max) -------------------------
    auto fillEnvelope = [&] (auto getter, juce::Colour col)
    {
        juce::Path p;
        p.startNewSubPath (plot.getX(), plot.getBottom());
        const int cols = std::max (2, (int) plot.getWidth());
        for (int x = 0; x < cols; ++x)
        {
            const int i0 = (int) ((float) x / (float) cols * (float) nFrames);
            const int i1 = std::max (i0 + 1, (int) ((float) (x + 1) / (float) cols * (float) nFrames));
            float m = 0.0f;
            for (int i = i0; i < std::min (i1, nFrames); ++i) m = std::max (m, getter (v[(size_t) i]));
            p.lineTo (plot.getX() + (float) x, yForDb (toDb (m)));
        }
        p.lineTo (plot.getRight(), plot.getBottom());
        p.closeSubPath();
        g.setColour (col);
        g.fillPath (p);
    };
    fillEnvelope ([] (const dsp::VizFrame& f) { return f.dryPeak; }, c.vizDry);
    fillEnvelope ([] (const dsp::VizFrame& f) { return f.wetPeak; }, c.vizWet.withMultipliedAlpha (0.75f));

    // --- gain curves -----------------------------------------------------
    auto curve = [&] (auto getter)
    {
        juce::Path p;
        const int step = std::max (1, (int) (1.0f / std::max (0.25f, pxPerFrame)));
        for (int i = 0; i < nFrames; i += step)
        {
            const float x = plot.getX() + (float) i * pxPerFrame;
            const float y = yForDb (toDb (getter (v[(size_t) i])));
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };
    auto stroke = [&] (const juce::Path& p, juce::Colour col, float w)
    {
        if (glow > 0.0f)
        {
            g.setColour (col.withMultipliedAlpha (0.18f * glow));
            g.strokePath (p, juce::PathStrokeType (w * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col.withMultipliedAlpha (0.35f * glow));
            g.strokePath (p, juce::PathStrokeType (w * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    const auto duckPath = curve ([] (const dsp::VizFrame& f) { return f.duckGain; });
    const auto gatePath = curve ([] (const dsp::VizFrame& f) { return f.gateGain; });
    const auto combPath = curve ([] (const dsp::VizFrame& f) { return f.duckGain * f.gateGain; });

    // duck: solid, gate: dashed (shape cue in addition to colour)
    stroke (duckPath, c.vizDuck, 1.6f);
    {
        juce::Path dashed;
        const float dashes[] = { 6.0f, 3.0f };
        juce::PathStrokeType (1.6f).createDashedStroke (dashed, gatePath, dashes, 2);
        g.setColour (c.vizGate);
        g.fillPath (dashed);
        if (glow > 0.0f) { g.setColour (c.vizGate.withMultipliedAlpha (0.25f * glow)); g.strokePath (gatePath, juce::PathStrokeType (4.0f)); }
    }
    stroke (combPath, c.vizCombined, 2.6f);
}

void EnvelopeVisualizer::paint (juce::Graphics& g)
{
    auto* lnf = freddyLnf (*this);
    if (lnf == nullptr) return;
    const auto& th = lnf->theme();
    const auto& c = th.c;

    inner = lnf->drawDisplayFrame (g, getLocalBounds().toFloat());
    auto area = inner.reduced (6.0f, 4.0f);
    header = area.removeFromTop (12.0f);
    auto legend = area.removeFromBottom (11.0f);
    plotArea = area.reduced (0, 4.0f);

    // --- build the visible window -----------------------------------------
    const int frames = std::max (2, (int) (timeScaleMs * 0.001 * frameRate));
    if (previewMode)
        view = preview;
    else
    {
        view.resize ((size_t) frames);
        int rp = (writePos - frames) & (kHistory - 1);
        for (int i = 0; i < frames; ++i) { view[(size_t) i] = history[(size_t) rp]; rp = (rp + 1) & (kHistory - 1); }
    }
    if (view.size() < 2) view.assign (2, {});

    // --- grid ---------------------------------------------------------------
    g.setColour (c.lcdDim.withAlpha (0.35f));
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f })
        g.fillRect (plotArea.getX(), yForDb (db), plotArea.getWidth(), 1.0f);
    const int divisions = timeScaleMs <= 500 ? 5 : (timeScaleMs <= 1000 ? 4 : (int) (timeScaleMs / 500));
    for (int i = 1; i < divisions; ++i)
        g.fillRect (plotArea.getX() + plotArea.getWidth() * (float) i / (float) divisions, plotArea.getY(), 1.0f, plotArea.getHeight());

    // --- bloom window shading -------------------------------------------------
    std::vector<BloomSpan> spans;
    int lastOnset = -1;
    computeBloomSpans (view, spans, lastOnset);
    const float pxPerFrame = plotArea.getWidth() / (float) (view.size() - 1);
    const double msPerFrame = 1000.0 / frameRate;
    for (auto& s : spans)
    {
        auto r = juce::Rectangle<float> (plotArea.getX() + (float) s.start * pxPerFrame, plotArea.getY(),
                                         (float) (s.end - s.start) * pxPerFrame, plotArea.getHeight());
        g.setColour (c.vizBloom);
        g.fillRect (r);
        // hatched edge so the window reads without colour
        g.setColour (c.vizCombined.withAlpha (0.5f));
        g.fillRect (r.getX(), r.getY(), 1.0f, r.getHeight());
        g.fillRect (r.getRight() - 1.0f, r.getY(), 1.0f, r.getHeight());
        const auto label = "BLOOM " + juce::String (juce::roundToInt ((double) (s.end - s.start) * msPerFrame)) + "MS";
        if (r.getWidth() > PixelFont::textWidth (label, 1.2f) + 6.0f)
            lnf->drawDisplayText (g, label, r.withHeight (12.0f).translated (0, 3.0f), juce::Justification::centred);
    }

    // --- threshold lines ----------------------------------------------------
    auto threshLine = [&] (float db, juce::Colour col, const juce::String& txt)
    {
        const float y = yForDb (db);
        juce::Path p; p.startNewSubPath (plotArea.getX(), y); p.lineTo (plotArea.getRight(), y);
        juce::Path d; const float dash[] = { 2.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (d, p, dash, 2);
        g.setColour (col); g.fillPath (d);
        PixelFont::drawFitted (g, txt, { plotArea.getRight() - 60.0f, y - 9.0f, 58.0f, 7.0f }, juce::Justification::right, col);
    };
    if (lastParams.duckOn) threshLine (lastParams.duckThreshDb, c.vizThreshDuck, "DUCK THR");
    if (lastParams.gateOn) threshLine (lastParams.gateThreshDb, c.vizThreshGate, "GATE THR");

    // --- traces (with optional CRT persistence) --------------------------------
    const float glow = lnf->traceGlow();
    if (th.lcdStyle == LcdStyle::crtGreen)
    {
        const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        const int w = juce::roundToInt (plotArea.getWidth() * scale), h = juce::roundToInt (plotArea.getHeight() * scale);
        if (persistence.getWidth() != w || persistence.getHeight() != h)
            persistence = juce::Image (juce::Image::ARGB, std::max (1, w), std::max (1, h), true);
        {
            juce::Graphics pg (persistence);
            // fade the old frame (phosphor decay)
            pg.setColour (juce::Colours::black.withAlpha (0.0f));
            persistence.multiplyAllAlphas (0.72f);
            pg.addTransform (juce::AffineTransform::translation (-plotArea.getX(), -plotArea.getY()).scaled (scale));
            drawTraces (pg, plotArea, view, glow);
        }
        g.drawImage (persistence, plotArea);
    }
    else
        drawTraces (g, plotArea, view, glow);

    // --- handles --------------------------------------------------------------
    rebuildHandles();
    for (int i = 0; i < (int) handles.size(); ++i)
    {
        auto& hd = handles[(size_t) i];
        const bool hot = i == hoverHandle || i == dragHandle;
        const float r = hot ? 6.0f : 4.5f;
        auto col = hd.paramID.startsWith ("gate") ? c.vizGate : c.vizDuck;
        g.setColour (c.lcdBg);
        g.fillRect (juce::Rectangle<float> (r * 2 + 2, r * 2 + 2).withCentre (hd.pos));
        g.setColour (col);
        if (hd.horizontal)
        {
            juce::Path p; p.addTriangle (hd.pos.x - r, hd.pos.y - r, hd.pos.x + r, hd.pos.y - r, hd.pos.x, hd.pos.y + r);
            g.fillPath (p);
        }
        else
            g.fillRect (juce::Rectangle<float> (r * 2, r * 2).withCentre (hd.pos));
        if (hot)
            PixelFont::drawFitted (g, hd.label, { hd.pos.x - 50, hd.pos.y - 20, 100, 8 }, juce::Justification::centred, col);
    }

    // --- header readout + legend ----------------------------------------------
    auto& apvts = hub.apvts;
    auto txt = [&] (const char* id) { return apvts.getParameter (id)->getCurrentValueAsText().removeCharacters (" ").toUpperCase(); };
    juce::String head = "DUCK " + (lastParams.duckOn ? txt (pid::duckDepth) : juce::String ("OFF"))
                      + "   BLOOM " + txt (pid::duckRelease)
                      + "   HOLD " + (lastParams.gateOn ? txt (pid::gateHold) : juce::String ("OFF"))
                      + "   REL " + txt (pid::gateRelease);
    lnf->drawDisplayText (g, head, header.withTrimmedRight (110.0f), juce::Justification::centredLeft);
    juce::String mode = previewMode ? "PREVIEW" : (frozen ? "FREEZE" : "LIVE");
    lnf->drawDisplayText (g, mode + " " + juce::String (timeScaleMs) + "MS", header.removeFromRight (104.0f), juce::Justification::centredRight, true);

    // legend: colour chip + text for each trace
    struct L { const char* t; juce::Colour col; };
    const L items[] = { { "DRY", c.vizDry.withAlpha (1.0f) }, { "WET", c.vizWet }, { "DUCK", c.vizDuck },
                        { "GATE", c.vizGate }, { "DUCKxGATE", c.vizCombined }, { "BLOOM", c.vizBloom.withAlpha (0.9f) } };
    float lx = legend.getX();
    for (auto& it : items)
    {
        g.setColour (it.col);
        g.fillRect (juce::Rectangle<float> (lx, legend.getCentreY() - 2.0f, 10.0f, 4.0f));
        lx += 13.0f;
        const float px = std::max (1.0f, legend.getHeight() / 8.0f);
        lx += PixelFont::drawText (g, it.t, lx, legend.getCentreY() - 3.5f * px, px, c.lcdFg.withAlpha (0.85f)) + 10.0f;
    }

    lnf->drawDisplayOverlay (g, inner);
}

void EnvelopeVisualizer::rebuildHandles()
{
    handles.clear();
    if (view.size() < 2 || plotArea.isEmpty()) return;
    const float pxPerMs = plotArea.getWidth() / (float) timeScaleMs;

    std::vector<BloomSpan> spans;
    int onset = -1;
    computeBloomSpans (view, spans, onset);
    const float pxPerFrame = plotArea.getWidth() / (float) (view.size() - 1);
    float onsetX = onset >= 0 ? plotArea.getX() + (float) onset * pxPerFrame
                              : plotArea.getX() + (float) previewHitMs * pxPerMs;
    float bloomX = onsetX + 40.0f * pxPerMs;
    float holdX = onsetX + (lastParams.gateHoldMs + 60.0f) * pxPerMs;
    if (! spans.empty())
    {
        auto& s = spans.back();
        bloomX = plotArea.getX() + (float) s.start * pxPerFrame;
        holdX  = plotArea.getX() + (float) s.end * pxPerFrame;
    }
    const float depthDb = lastParams.duckDepthDb <= kDuckFullDb ? kBottomDb : lastParams.duckDepthDb;
    if (lastParams.duckOn)
    {
        handles.push_back ({ pid::duckDepth, { onsetX + 12.0f * pxPerMs + 8.0f, yForDb (depthDb) }, false, "DEPTH" });
        handles.push_back ({ pid::duckRelease, { std::min (plotArea.getRight() - 4, bloomX + lastParams.duckReleaseMs * pxPerMs), plotArea.getY() + 8.0f }, true, "BLOOM" });
    }
    if (lastParams.gateOn)
    {
        handles.push_back ({ pid::gateHold, { std::min (plotArea.getRight() - 4, holdX), plotArea.getY() + 22.0f }, true, "HOLD" });
        handles.push_back ({ pid::gateRelease, { std::min (plotArea.getRight() - 4, holdX + lastParams.gateReleaseMs * pxPerMs), plotArea.getY() + 36.0f }, true, "RELEASE" });
    }
}

int EnvelopeVisualizer::hitTestHandle (juce::Point<float> p) const
{
    for (int i = 0; i < (int) handles.size(); ++i)
        if (handles[(size_t) i].pos.getDistanceFrom (p) < 10.0f) return i;
    return -1;
}

void EnvelopeVisualizer::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitTestHandle (e.position);
    if (h != hoverHandle) { hoverHandle = h; repaint(); }
    setMouseCursor (h >= 0 ? (handles[(size_t) h].horizontal ? juce::MouseCursor::LeftRightResizeCursor
                                                              : juce::MouseCursor::UpDownResizeCursor)
                           : juce::MouseCursor::NormalCursor);
}

void EnvelopeVisualizer::mouseDown (const juce::MouseEvent& e)
{
    dragHandle = hitTestHandle (e.position);
    dragLast = e.position;
    if (dragHandle >= 0)
    {
        const auto& id = handles[(size_t) dragHandle].paramID;
        hub.touch (id);
        hub.apvts.getParameter (id)->beginChangeGesture();
    }
    else if (header.contains (e.position) && e.position.x > header.getCentreX())
    {
        // click on the right of the header: cycle the time scale
        int idx = 0;
        for (int i = 0; i < 5; ++i) if (kTimeScales[i] == timeScaleMs) idx = i;
        setTimeScaleMs (kTimeScales[(idx + 1) % 5]);
    }
}

void EnvelopeVisualizer::mouseDrag (const juce::MouseEvent& e)
{
    if (dragHandle < 0) return;
    const auto& hd = handles[(size_t) dragHandle];
    auto* p = hub.apvts.getParameter (hd.paramID);
    const auto range = p->getNormalisableRange();
    const float cur = p->convertFrom0to1 (p->getValue());
    const bool fine = e.mods.isCommandDown() || e.mods.isCtrlDown();
    float next = cur;
    if (hd.horizontal)
    {
        const float msPerPx = (float) timeScaleMs / plotArea.getWidth() * (fine ? 0.2f : 1.0f);
        next = cur + (e.position.x - dragLast.x) * msPerPx;
    }
    else
        next = juce::jlimit (range.start, range.end, dbForY (e.position.y));
    dragLast = e.position;
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (range.start, range.end, next)));
    repaint();
}

void EnvelopeVisualizer::mouseUp (const juce::MouseEvent&)
{
    if (dragHandle >= 0)
        hub.apvts.getParameter (handles[(size_t) dragHandle].paramID)->endChangeGesture();
    dragHandle = -1;
}
} // namespace fv::ui
