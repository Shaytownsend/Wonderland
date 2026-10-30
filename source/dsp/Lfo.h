#pragma once

#include <cmath>

namespace queen
{
// Global triangle LFO with a delay fade-in.
// auto: free running; the fade restarts when a note starts with no other keys held.
// man:  runs only while the LFO TRIG button is down (phase + fade restart on press).
class Lfo
{
public:
    void prepare (double sr) { sampleRate = sr; }

    void retrigger() noexcept { fade = 0.0f; }

    // Advances n samples and returns the bipolar output (-1..1) scaled by the fade.
    float advance (int n, float rateHz, float delaySeconds, bool manualMode, bool triggerDown) noexcept
    {
        if (manualMode)
        {
            if (triggerDown && ! wasDown)
            {
                phase = 0.0;
                fade = 0.0f;
            }
            wasDown = triggerDown;
            if (! triggerDown)
                return 0.0f;
        }
        phase += (double) rateHz * n / sampleRate;
        phase -= std::floor (phase);
        if (delaySeconds <= 0.0f)
            fade = 1.0f;
        else
            fade = std::fmin (1.0f, fade + (float) (n / (delaySeconds * sampleRate)));
        return value() * fade;
    }

    float value() const noexcept
    {
        // triangle starting at 0 and rising: 0 -> 1 -> 0 -> -1 -> 0
        const float p = (float) phase;
        return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
    }

    float currentFade() const noexcept { return fade; }

private:
    double sampleRate = 44100.0, phase = 0.0;
    float fade = 1.0f;
    bool wasDown = false;
};
} // namespace queen
