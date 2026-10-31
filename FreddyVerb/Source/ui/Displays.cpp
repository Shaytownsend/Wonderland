#include "Displays.h"
#include "../state/Parameters.h"
#include <complex>

namespace fv::ui
{
namespace
{
    constexpr float kMinHz = 20.0f, kMaxHz = 20000.0f, kRangeDb = 18.0f;
    const char* const bandFreq[] = { pid::hpfFreq, pid::lsFreq, pid::b1Freq, pid::b2Freq, pid::hsFreq, pid::lpfFreq };
    const char* const bandGain[] = { nullptr,      pid::lsGain, pid::b1Gain, pid::b2Gain, pid::hsGain, nullptr };
    const char* const bandQ[]    = { nullptr,      pid::lsQ,    pid::b1Q,    pid::b2Q,    pid::hsQ,    nullptr };
    const char* const bandOn[]   = { pid::hpfOn,   pid::lsOn,   pid::b1On,   pid::b2On,   pid::hsOn,   pid::lpfOn };
    const char* const bandName[] = { "HPF", "LS", "B1", "B2", "HS", "LPF" };

    float val (juce::AudioProcessorValueTreeState& s, const char* id) { return s.getRawParameterValue (id)->load(); }

    using C = std::complex<double>;
    // analog prototypes evaluated at s = j*w/w0
    C hp2 (double f, double f0, double q) { C s (0, f / f0); return s * s / (s * s + s / q + 1.0); }
    C lp2 (double f, double f0, double q) { C s (0, f / f0); return 1.0 / (s * s + s / q + 1.0); }
    C bell (double f, double f0, double gDb, double q)
    {
        C s (0, f / f0); const double A = std::pow (10.0, gDb / 40.0);
        return (s * s + s * (A / q) + 1.0) / (s * s + s / (A * q) + 1.0);
    }
    C lowShelf (double f, double f0, double gDb, double q)
    {
        C s (0, f / f0); const double A = std::pow (10.0, gDb / 40.0), sa = std::sqrt (A);
        return A * (s * s + sa / q * s + A) / (A * s * s + sa / q * s + 1.0);
    }
    C highShelf (double f, double f0, double gDb, double q)
    {
        C s (0, f / f0); const double A = std::pow (10.0, gDb / 40.0), sa = std::sqrt (A);
        return A * (A * s * s + sa / q * s + 1.0) / (s * s + sa / q * s + A);
    }
    // Butterworth cascade Qs for 2/4/8 poles
    const std::vector<double>& butterQs (int slope)
    {
        static const std::vector<double> q12 { 0.70710678 };
        static const std::vector<double> q24 { 0.54119610, 1.30656296 };
        static const std::vector<double> q48 { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
        return slope == 0 ? q12 : (slope == 1 ? q24 : q48);
    }
}

EQDisplay::EQDisplay (ControlHub& h) : hub (h)
{
    for (auto* p : hub.apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->getParameterID().startsWith ("eq_"))
                hub.apvts.addParameterListener (rp->getParameterID(), this);
    setTitle ("EQ curve");
}

EQDisplay::~EQDisplay()
{
    for (auto* p : hub.apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->getParameterID().startsWith ("eq_"))
                hub.apvts.removeParameterListener (rp->getParameterID(), this);
}

float EQDisplay::magnitudeDb (juce::AudioProcessorValueTreeState& s, float hz)
{
    if (val (s, pid::eqOn) < 0.5f) return 0.0f;
    C h (1.0, 0.0);
    if (val (s, pid::hpfOn) > 0.5f)
        for (double q : butterQs ((int) val (s, pid::hpfSlope))) h *= hp2 (hz, val (s, pid::hpfFreq), q);
    if (val (s, pid::lpfOn) > 0.5f)
        for (double q : butterQs ((int) val (s, pid::lpfSlope))) h *= lp2 (hz, val (s, pid::lpfFreq), q);
    if (val (s, pid::lsOn) > 0.5f) h *= lowShelf (hz, val (s, pid::lsFreq), val (s, pid::lsGain), val (s, pid::lsQ));
    if (val (s, pid::hsOn) > 0.5f) h *= highShelf (hz, val (s, pid::hsFreq), val (s, pid::hsGain), val (s, pid::hsQ));
    if (val (s, pid::b1On) > 0.5f) h *= bell (hz, val (s, pid::b1Freq), val (s, pid::b1Gain), val (s, pid::b1Q));
    if (val (s, pid::b2On) > 0.5f) h *= bell (hz, val (s, pid::b2Freq), val (s, pid::b2Gain), val (s, pid::b2Q));
    return (float) (20.0 * std::log10 (std::max (1.0e-9, std::abs (h))));
}

float EQDisplay::xForHz (float hz) const { return plot.getX() + plot.getWidth() * std::log (hz / kMinHz) / std::log (kMaxHz / kMinHz); }
float EQDisplay::hzForX (float x) const  { return kMinHz * std::pow (kMaxHz / kMinHz, (x - plot.getX()) / plot.getWidth()); }
float EQDisplay::yForDb (float db) const { return plot.getCentreY() - db / kRangeDb * plot.getHeight() * 0.5f; }
float EQDisplay::dbForY (float y) const  { return (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * kRangeDb; }

juce::Point<float> EQDisplay::nodePos (int b) const
{
    const float f = val (hub.apvts, bandFreq[b]);
    const float g = bandGain[b] ? val (hub.apvts, bandGain[b]) : 0.0f;
    return { xForHz (f), yForDb (g) };
}

void EQDisplay::paint (juce::Graphics& g)
{
    auto* lnf = freddyLnf (*this);
    if (lnf == nullptr) return;
    const auto& c = lnf->theme().c;
    auto inner = lnf->drawDisplayFrame (g, getLocalBounds().toFloat());
    plot = inner.reduced (4.0f, 6.0f);

    g.setColour (c.lcdDim.withAlpha (0.35f));
    for (float f : { 100.0f, 1000.0f, 10000.0f }) g.fillRect (xForHz (f), plot.getY(), 1.0f, plot.getHeight());
    for (float db : { -12.0f, 0.0f, 12.0f }) g.fillRect (plot.getX(), yForDb (db), plot.getWidth(), 1.0f);

    juce::Path p;
    for (int x = 0; x <= (int) plot.getWidth(); x += 2)
    {
        const float hz = hzForX (plot.getX() + (float) x);
        const float y = yForDb (juce::jlimit (-kRangeDb * 1.1f, kRangeDb * 1.1f, magnitudeDb (hub.apvts, hz)));
        if (x == 0) p.startNewSubPath (plot.getX(), y); else p.lineTo (plot.getX() + (float) x, y);
    }
    g.setColour (c.lcdFg);
    g.strokePath (p, juce::PathStrokeType (1.8f));

    for (int b = 0; b < 6; ++b)
    {
        const bool on = val (hub.apvts, bandOn[b]) > 0.5f;
        auto pos = nodePos (b);
        pos.y = juce::jlimit (plot.getY() + 4, plot.getBottom() - 4, pos.y);
        const float r = b == selected ? 5.5f : 4.0f;
        g.setColour (on ? (b == selected ? c.accent : c.lcdFg) : c.lcdDim);
        if (on) g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (pos));
        else    g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (pos), 1.2f);
        if (b == selected)
            PixelFont::drawFitted (g, bandName[b], { pos.x - 15, pos.y - 16, 30, 7 }, juce::Justification::centred, c.lcdFg);
    }
    lnf->drawDisplayOverlay (g, inner);
}

void EQDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = -1;
    float best = 14.0f;
    for (int b = 0; b < 6; ++b)
    {
        auto pos = nodePos (b);
        pos.y = juce::jlimit (plot.getY() + 4, plot.getBottom() - 4, pos.y);
        const float d = pos.getDistanceFrom (e.position);
        if (d < best) { best = d; dragging = b; }
    }
    if (dragging >= 0)
    {
        selected = dragging;
        hub.touch (bandFreq[dragging]);
        hub.apvts.getParameter (bandFreq[dragging])->beginChangeGesture();
        if (bandGain[dragging]) hub.apvts.getParameter (bandGain[dragging])->beginChangeGesture();
        if (onBandSelected) onBandSelected (selected);
        repaint();
    }
}

void EQDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    auto* f = hub.apvts.getParameter (bandFreq[dragging]);
    f->setValueNotifyingHost (f->convertTo0to1 (juce::jlimit (kMinHz, kMaxHz, hzForX (e.position.x))));
    if (bandGain[dragging])
    {
        auto* g = hub.apvts.getParameter (bandGain[dragging]);
        g->setValueNotifyingHost (g->convertTo0to1 (juce::jlimit (-kRangeDb, kRangeDb, dbForY (e.position.y))));
    }
}

void EQDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging < 0) return;
    hub.apvts.getParameter (bandFreq[dragging])->endChangeGesture();
    if (bandGain[dragging]) hub.apvts.getParameter (bandGain[dragging])->endChangeGesture();
    dragging = -1;
}

void EQDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    // double-click a node toggles the band on/off
    mouseDown (e);
    if (dragging >= 0)
    {
        auto* on = hub.apvts.getParameter (bandOn[dragging]);
        on->beginChangeGesture();
        on->setValueNotifyingHost (on->getValue() > 0.5f ? 0.0f : 1.0f);
        on->endChangeGesture();
        mouseUp (e);
    }
}

void EQDisplay::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (bandQ[selected] == nullptr) return;
    auto* q = hub.apvts.getParameter (bandQ[selected]);
    q->beginChangeGesture();
    q->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, q->getValue() + (w.deltaY > 0 ? 0.03f : -0.03f)));
    q->endChangeGesture();
}

// ===========================================================================
// IRDisplay
// ===========================================================================
IRDisplay::IRDisplay (ControlHub& h) : hub (h)
{
    setTitle ("Impulse response display");
    // placeholder thumbnail until an IR is loaded
    std::vector<float> pk (400);
    for (size_t i = 0; i < pk.size(); ++i)
    {
        const float t = (float) i / (float) pk.size();
        pk[i] = std::exp (-5.0f * t) * (0.65f + 0.35f * std::abs (std::sin ((float) i * 1.7f))) * std::min (1.0f, t * 60.0f);
    }
    peaks = std::move (pk);
    lengthSec = 1.6;
}

void IRDisplay::setThumbnail (std::vector<float> p, double len, juce::String name, double rt)
{
    peaks = std::move (p); lengthSec = std::max (0.01, len); irName = std::move (name); rt60 = rt;
    repaint();
}

float IRDisplay::xForMs (float ms) const
{
    return plot.getX() + plot.getWidth() * juce::jlimit (0.0f, 1.0f, ms / (float) (lengthSec * 1000.0));
}

void IRDisplay::paint (juce::Graphics& g)
{
    auto* lnf = freddyLnf (*this);
    if (lnf == nullptr) return;
    const auto& c = lnf->theme().c;
    auto inner = lnf->drawDisplayFrame (g, getLocalBounds().toFloat());
    auto area = inner.reduced (5.0f, 3.0f);
    auto head = area.removeFromTop (std::max (10.0f, area.getHeight() * 0.12f));
    plot = area.reduced (0, 3.0f);

    lnf->drawDisplayText (g, irName, head, juce::Justification::centredLeft);
    lnf->drawDisplayText (g, "RT60 " + juce::String (rt60, 2) + "S", head, juce::Justification::centredRight, true);

    if (missing)
    {
        lnf->drawDisplayText (g, "IR FILE MISSING", plot.withHeight (plot.getHeight() * 0.4f), juce::Justification::centred);
        lnf->drawDisplayText (g, "CLICK TO RELINK", plot.withTrimmedTop (plot.getHeight() * 0.5f).withHeight (plot.getHeight() * 0.25f), juce::Justification::centred, true);
        lnf->drawDisplayOverlay (g, inner);
        return;
    }

    if (showSpectrogram && ! spectro.empty())
    {
        const int rows = (int) spectro.size();
        const float rh = plot.getHeight() / (float) rows;
        for (int r = 0; r < rows; ++r)
        {
            const auto& row = spectro[(size_t) r];
            const float cw = plot.getWidth() / (float) std::max<size_t> (1, row.size());
            for (size_t i = 0; i < row.size(); ++i)
            {
                g.setColour (c.lcdFg.withAlpha (juce::jlimit (0.0f, 1.0f, row[i])));
                g.fillRect (plot.getX() + (float) i * cw, plot.getBottom() - (float) (r + 1) * rh, cw + 0.5f, rh + 0.5f);
            }
        }
    }
    else
    {
        // mirrored waveform
        juce::Path p;
        const int n = (int) peaks.size();
        const float mid = plot.getCentreY();
        p.startNewSubPath (plot.getX(), mid);
        for (int i = 0; i < n; ++i)
            p.lineTo (plot.getX() + plot.getWidth() * (float) i / (float) std::max (1, n - 1), mid - peaks[(size_t) i] * plot.getHeight() * 0.48f);
        for (int i = n - 1; i >= 0; --i)
            p.lineTo (plot.getX() + plot.getWidth() * (float) i / (float) std::max (1, n - 1), mid + peaks[(size_t) i] * plot.getHeight() * 0.48f);
        p.closeSubPath();
        g.setColour (c.lcdFg.withAlpha (0.85f));
        g.fillPath (p);
    }

    // handles: pre-delay (P), start (S), length (L)
    auto& s = hub.apvts;
    const float pre = s.getRawParameterValue (pid::predelay)->load();
    const float start = s.getRawParameterValue (pid::irStart)->load();
    const float lenPct = s.getRawParameterValue (pid::irLength)->load();
    const float xs[] = { xForMs (pre), xForMs (start), plot.getX() + plot.getWidth() * lenPct * 0.01f };
    const char* tags[] = { "P", "S", "L" };
    for (int i = 0; i < 3; ++i)
    {
        const bool hot = hover == i || drag == i;
        g.setColour (i == 2 ? c.accent : c.lcdFg);
        g.fillRect (xs[i] - (hot ? 1.0f : 0.5f), plot.getY(), hot ? 2.0f : 1.0f, plot.getHeight());
        auto tag = juce::Rectangle<float> (xs[i] - 5.0f, plot.getBottom() - 10.0f, 10.0f, 10.0f);
        g.fillRect (tag);
        PixelFont::drawFitted (g, tags[i], tag.reduced (2.0f), juce::Justification::centred, c.lcdBg);
    }
    // dim truncated region
    g.setColour (c.lcdBg.withAlpha (0.6f));
    g.fillRect (plot.withLeft (xs[2]));
    lnf->drawDisplayOverlay (g, inner);
}

void IRDisplay::mouseMove (const juce::MouseEvent& e)
{
    auto& s = hub.apvts;
    const float xs[] = { xForMs (s.getRawParameterValue (pid::predelay)->load()),
                         xForMs (s.getRawParameterValue (pid::irStart)->load()),
                         plot.getX() + plot.getWidth() * s.getRawParameterValue (pid::irLength)->load() * 0.01f };
    int h = -1;
    for (int i = 2; i >= 0; --i) if (std::abs (e.position.x - xs[i]) < 6.0f) { h = i; break; }
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void IRDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (missing) { if (onRelinkRequested) onRelinkRequested(); return; }
    if (e.mods.isPopupMenu() || (hover < 0 && e.getNumberOfClicks() == 1 && e.position.y < plot.getY()))
    {
        showSpectrogram = ! showSpectrogram;
        repaint();
        return;
    }
    drag = hover;
    if (drag < 0) return;
    const char* ids[] = { pid::predelay, pid::irStart, pid::irLength };
    auto* p = hub.apvts.getParameter (ids[drag]);
    hub.touch (ids[drag]);
    p->beginChangeGesture();
    dragStartX = e.position.x;
    dragStartVal = p->convertFrom0to1 (p->getValue());
}

void IRDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (drag < 0) return;
    const char* ids[] = { pid::predelay, pid::irStart, pid::irLength };
    auto* p = hub.apvts.getParameter (ids[drag]);
    const float dx = e.position.x - dragStartX;
    float v = dragStartVal;
    if (drag == 2) v += dx / plot.getWidth() * 100.0f;
    else           v += dx / plot.getWidth() * (float) (lengthSec * 1000.0);
    const auto r = p->getNormalisableRange();
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (r.start, r.end, v)));
    repaint();
}

void IRDisplay::mouseUp (const juce::MouseEvent&)
{
    if (drag < 0) return;
    const char* ids[] = { pid::predelay, pid::irStart, pid::irLength };
    hub.apvts.getParameter (ids[drag])->endChangeGesture();
    drag = -1;
}
} // namespace fv::ui
