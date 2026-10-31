#pragma once

#include <vector>
#include "EngineParams.h"
#include "VizFeed.h"

namespace fv::dsp
{
/** Renders the theoretical duck/gate response to one synthetic drum hit with the
    given settings, as visualizer frames. Used for the static preview when audio
    is stopped, and by the snapshot tool. Runs the real Detector/Ducker/Gate code
    so the picture always matches what the engine does.

    hitAtMs: where the hit starts; totalMs: length of the rendered window.
    reverbRt60: approximate decay of the loaded IR, used to draw the wet tail. */
void renderEnvelopePreview (const EngineParams& p, double sampleRate, double hitAtMs, double totalMs,
                            double reverbRt60, std::vector<VizFrame>& out);
} // namespace fv::dsp
