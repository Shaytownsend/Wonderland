# Design decisions

Ambiguities in the spec, what was chosen, why, and how to change it. Newest at the bottom.

## Architecture
1. **Repository layout** — the plugin lives in `FreddyVerb/` inside the existing `Wonderland` repo (which already held an unrelated rack-mount project). CI runs with `working-directory: FreddyVerb`. *Change:* move the folder to its own repo; nothing depends on the parent.
2. **DSP is host-independent** — `dsp::FreddyEngine` takes a plain `EngineParams` snapshot each block and never touches the APVTS. This keeps every DSP block unit-testable and lets the static envelope preview run the real code.
3. **Linux targets** — besides the spec's VST3/AU/CLAP/AAX/Standalone, Linux builds also produce LV2 (free with JUCE). *Change:* `FV_FORMATS` in `CMakeLists.txt`.
4. **Dependencies are fetched with pinned tags** (`cmake/Dependencies.cmake`): JUCE 8.0.15, clap-juce-extensions (pinned commit), r8brain-free-src 6.5, PFFFT (marton78, pinned commit), Catch2 3.16.1.

## Convolution
5. **Partition plan (Zero Latency)** — direct-form FIR for taps 0–63, then uniform-partitioned FFT stages of 64 (from 64), 512 (from 1024), 4096 (from 8192) and 32768 (from 65536) samples. Every stage after the first starts at ≥ 2× its partition, which gives one full block of slack. Stages whose slack is larger than the host's max block size run on the background `ConvolutionWorker`; others run inline. *Change:* `planZeroLatency()` in `dsp/Convolver.cpp`.
6. **Deadline safety** — the audio thread only publishes block indices (atomics). If a background block isn't finished when its output is due, the audio thread claims and computes it itself (CAS on a per-stage claim counter). The only wait is the rare case where the worker is *mid-way* through that exact block (bounded by one block's compute). No locks, no allocation on the audio thread.
7. **Worker wake-up** — *(revised in M8)* the audio thread wakes the worker with C++20 `std::atomic::notify_one` (futex / ulock based: no lock, never blocks). The original 300 µs polling loop measured ≈ 1 % of a core in M8 profiling. This raises the macOS deployment target to 11.0.
8. **Efficient mode** — one uniform 2048-sample partition stage, latency 2048 (43 ms @ 48 kHz, compensated by the host); chosen in M8 profiling so it really costs less CPU than Zero Latency, all on the audio thread. *Change:* `Convolver::kEfficientPartition`.
9. **Double precision** — FFTs, spectra, multiply-accumulate, head FIR and history are all `double`. Engine I/O outside the convolver stays `float`.
10. **FFT backends** — vDSP double (`vDSP_fft_zripD`) on Apple, PFFFT double (AVX on x86-64) elsewhere. IPP is not implemented (see BLOCKERS #6). The spectrum layout is opaque; the convolution scale factor is measured at FFT construction, so backends cannot disagree on scaling.
11. **Offline/non-realtime renders** run every stage synchronously (deterministic, no worker).

## IR processing
12. **Mono IR upmix** — mono IR → L→L and R→R paths, no cross paths (keeps the input image; width is shaped afterwards by the SPACE section). Stereo IR → L→L = left, R→R = right. Four-channel IR → L→L, L→R, R→L, R→R. Mono input feeds both inputs of the true-stereo matrix.
13. **Shaping order** — resample (session rate × Size) → loudness normalise → Decay → Damping → Early/Late → Reverse → Start trim → Length fade. Decay/damping/early-late are computed on the forward IR so they keep their meaning when Reverse is on; Start and Length act on what you hear (after reversing).
14. **Normalisation** — measured on the resampled, *unshaped* IR (K-weighted energy per output channel, target −6 dB power gain). Switching IRs or changing Size doesn't jump in level; Decay/Length/Early/Late/Damping still audibly change the level, as users expect.
15. **Decay** — 25–200 % of the IR's own RT60 (Schroeder T30), applied as an exponential envelope, boost capped at +24 dB.
16. **Damping** — STFT (21 ms sqrt-Hann frames, 75 % overlap) with a per-frame gain `exp(-α·t·f²/(f²+fc²))`; 100 % = an extra 60 dB loss every 0.35 s far above the Damping Freq. This is time-varying, not a static filter (tested).
17. **Pre-delay** is a real-time delay line in front of the convolver (crossfaded taps, no pitch artefacts), not baked into the IR, so automating it never triggers an IR rebuild.
18. **Length** is a percentage parameter; the absolute ms value is shown on the IR display. **Start trim** gets a 1 ms raised-cosine fade-in; **Length** fades over the last 10 % of what's kept.
19. **User IR state** — absolute path + FNV-1a 64-bit content hash. Files ≤ 2 MB are also embedded (base64) in the plugin state, so a session still loads if the file moves; the UI then shows the relink option. *Change:* `IRManager::kMaxEmbedBytes`.
20. **Factory IR format** — `tools/ir_forge` renders 24-bit/96 kHz four-channel masters; the plugin embeds them as 24-bit/96 kHz FLAC to keep the binary inside the size budget.

## Dynamics, mix, UI
21. **Init preset / defaults** — every parameter default equals the spec's Init preset (Medium Plate, Follow duck −18 dB / 250 ms bloom, gate hold 150 ms / release 300 ms, Mix 35 %, EQ Post).
22. **Depth "FULL"** — Duck Depth runs −61…0 dB; anything below −60.5 dB is shown as FULL and means infinite attenuation. Gate Floor and Dry/Wet levels use the same trick for −∞.
23. **Mix + faders** — Mix is equal-power (`cos`/`sin`) and multiplies the independent Dry/Wet level faders. **Wet Solo** mutes the dry and plays the wet at its fader level (ignores Mix).
24. **Key alignment** — the key is delayed by the convolution latency only, the audio (dry and wet input) by convolution latency + lookahead, so in both quality modes the ducker sees the hit exactly `lookahead` ms before it reaches the wet output.
25. **UI scale** — each theme has a fixed logical size (flat: 1280×800 except Studio Black Rack 1360×800, Pad Station 1280×860, Floppy Era Pro 1560×760; photoreal: 1600×589 = 19"×7"). The whole panel is drawn with an affine scale (75–200 %), so every theme is vector-sharp at any size and the aspect ratio is fixed.
26. **Photoreal skins use 4U** (19"×7") rather than 3U: all ~100 controls plus the envelope, IR browser and IR displays don't fit legibly into 3U. The spec allows 4U "if the layout needs it".
27. **Photoreal labels** are baked into the rendered panel (silkscreen with wear); flat themes draw labels in code.
28. **Selected-band EQ knobs** — the four shelf/bell bands share one row of Freq/Gain/Q knobs that follow the node selected in the EQ display; HPF/LPF have permanent knobs. Every parameter stays reachable and automatable.
29. **Undo/redo** — APVTS with a `juce::UndoManager`.

## Content pipelines (M2)
30. **panel_forge pipeline** — Blender isn't installed, so the photoreal skins use the procedural Python pipeline (numpy/scipy/Pillow, no downloaded textures): height map + material per surface, rendered at 3× with one fixed top-left key light (cast shadow + AO), @2x/@1x are Lanczos downscales. Knob filmstrips rotate geometry under fixed lighting (128 frames, 270°; jog 360°). Wear is driven by a seeded "hand-time" map around the most-used controls; scratches are suppressed over text and labels always keep ≥ 55 % ink. Panels ship as JPEG, parts as PNG; each skin is 1.9–3.0 MB. *Change:* `tools/panel_forge/pf_skins.py`.
31. **Factory IR measurement** — `library.json` RT60 is T30 in the 1 kHz octave (Schroeder, −5…−35 dB). For gated/non-linear/reverse IRs that number is not a real decay time; the IR display shows the measured broadband RT60 of the *shaped* IR instead.

## Dynamics (M3)
32. **Peak detector** has a fixed 10 ms release (instant attack); a raw per-sample peak would collapse at every zero crossing and make the ducker chatter. RMS mode is an exact moving average over the RMS window.
33. **FOLLOW mode** — target reduction = (key − threshold) × Amount, capped at Depth. Attack is a one-pole towards the target; the bloom follows the chosen release curve from the level where the key *stopped holding it down*, so a long loud hit keeps the reverb ducked longer and a short one blooms sooner, and a sudden key drop never jumps.
34. **CURVE mode** — trigger when the key crosses the duck threshold, re-armed only after it falls below threshold − hysteresis and the guard time has passed. **Restart** ramps from the current level down to Depth over the Attack time, then plays the curve. **Continue** (only while a curve is running) starts a new curve from the current level with no new dip, so fast rolls don't click; a hit after full recovery always ducks fully.
35. **Curve shapes** are closed-form (`CurveShapes::evaluate`), pinned by an independent Python implementation (`tools/curve_reference/make_curve_tables.py` → `tests/data/curve_reference.csv`). Tension bends each shape with a *bounded* slope (no infinite-slope power curves). Hold-then-Ramp uses Tension for the hold fraction (10–90 %, default 50 %). Swell peaks at exactly +1.5 dB.
36. **Gate** — opens above threshold (raised-cosine attack), closes below threshold − hysteresis, holds, then releases to the floor along linear / exponential / logarithmic curves. Re-opening during hold or release continues from the current level. With the gate off the gain eases to unity in 10 ms.
37. **Static preview** runs the real Detector/Ducker/Gate on one synthetic hit (at 24 kHz for speed) after a silent pre-roll so the gate starts closed.

## Tone & space (M4)
38. **Wet EQ topology** — Simper/Zavalishin TPT SVFs (HPF/LPF as Butterworth cascades of 1/2/4 sections, shelves and bells via SVF mixing coefficients), run at 2× through JUCE's polyphase-IIR half-band oversampler, double precision. Bells additionally get **bandwidth pre-warping** (centre exact, digital Q chosen so the lower band edge lands on the analogue edge), which removes the remaining cramping: a +12 dB bell at 15 kHz on a 48 kHz session matches the analogue prototype within 0.5 dB from 8 to 19 kHz (tested).
39. **EQ placement** — two EQ instances (pre-reverb and post-gate). Toggling EQ on/off or Pre/Post crossfades over 30 ms instead of switching hard. The polyphase IIR oversampler adds a few samples of frequency-dependent group delay to the *wet* path only; it is not latency-compensated (inaudible on a reverb tail, and keeps reported latency unchanged).
40. **Width** — M/S: side × width (0–200 %). Mono-below applies a Linkwitz-Riley LR4 high-pass to the side signal only, so everything below the crossover becomes mono with the mid untouched. Balance is a linear balance law on the wet (−100 = left only).

## Content (M5)
41. **Preset format** — XML `.fvpreset` with real-unit values (`<PARAM id value>`) plus the IR reference; versioned; missing parameters fall back to defaults so presets survive future parameter additions. Bypass and Mix Lock are never stored in presets. Factory presets are generated by `tools/presets/make_presets.py` and embedded; user presets live in `~/Documents/Freddy Verb/Presets`.
42. **A/B** — two full snapshots (parameters + IR). The first switch to B copies A. "A>B" copies A into B (if you're on B, B becomes A). A/B switches and preset loads are single undo steps; every control gesture starts a new undo transaction.
43. **User IR folders** — remembered in the UI state (`irFolders`), scanned recursively (capped at 1 000 files per folder) into the browser's USER category alongside recently loaded files.
44. **Browser RT60 column** — for Non-Linear and reversed IRs the browser shows the IR length instead of RT60 (their T30 is not a meaningful decay time).

## Final UI (M7)
45. **Chosen direction: 12 — Tour-Worn Champagne** (user's pick at the M6 checkpoint). It is the default skin (`ui::defaultThemeId()`); the other 12 remain selectable under SETUP → Flat / Photoreal Rack. Saved sessions keep whatever skin they were saved with.
46. **IR browser layout** — the category *column* is replaced by a one-line category stepper (`< PLATES  5 >`, click the middle for a menu) so the list gets full width and 14–18 px rows; this keeps it legible at 75 % in the compact 4U rack skins. The list always scrolls to keep the current IR visible.
47. **Hover readout** — hovering any knob, pad or stepper shows its name and value on the readout LCD without "touching" it (no undo step; the data-entry jog wheel keeps its target).
