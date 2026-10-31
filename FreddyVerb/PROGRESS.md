# Freddy Verb — progress report

One entry per milestone. Each links to that milestone's 13-option UI contact sheet.

---

## M1 — Foundation  ·  [UI previews](ui_previews/m1_foundation/index.html)

**Built**
- CMake project on JUCE 8.0.15 with VST3 / AU (macOS) / CLAP (clap-juce-extensions) / LV2 (Linux) / Standalone; AAX auto-enabled when `FV_AAX_SDK_PATH` is given (skipped here, see BLOCKERS #1). ccache, pinned FetchContent deps.
- All 84 parameters in the APVTS (`state/Parameters.cpp`), grouped, with units, skews, text conversion, tooltips; defaults = the spec's Init preset. Undo manager attached.
- Host-independent `dsp::FreddyEngine`: latency-compensated dry / key / wet branches (`LatencyDelay` with crossfaded tap changes), equal-power mix + dry/wet faders, wet solo, key listen, output gain, click-free latency-compensated bypass (host bypass parameter too), peak/RMS meters, correlation, and the lock-free SPSC visualizer feed.
- Versioned state (`FreddyVerbState` v1: parameters + UI state).
- UI framework: `FreddyTheme` + `FreddyLookAndFeel` per theme, one shared `MainPanel` with every control (knobs with fine-drag/double-click reset/wheel/tooltips/accessibility, LED pads, choice steppers, data-entry jog wheel editing the last-touched parameter, LCD readout, meters), the envelope visualizer (live + static preview, bloom shading, draggable handles, time scales, freeze), EQ curve display with draggable nodes, IR display with draggable start/length/pre-delay handles, IR browser shell. Vector 5×7 dot-matrix + 7-segment renderers for every LCD/VFD/LED readout. Fixed-aspect scaling 75–200 %.
- Theme switcher (SETUP button → Flat / Photoreal Rack) works live.
- 13 themes registered: 8 flat (rough first pass), 5 photoreal (image-skin LookAndFeel; procedural placeholder until panel_forge art lands).
- `FreddyVerb_Snapshots` offscreen renderer + contact sheet, and `--manifest` export for panel_forge.
- Catch2 harness; GitHub Actions CI for macOS Universal / Windows x64 / Linux with tests + pluginval 10.
- Also already in this commit (M2 work started in parallel): partitioned convolver, IR processor, IR manager — see M2.

**Tests** — 24 test cases / 720 assertions green (Linux): parameter table, Init defaults, state round-trip + future-version load, bus layouts, latency reporting in every quality/lookahead mode at 44.1–192 kHz, 0 %-wet null test (fixed and random block sizes), bypass null test.
**pluginval** — VST3, strictness 10: SUCCESS (Linux, built from source).
**CPU** — pass-through engine: negligible (see M2 for convolution numbers).
**Decisions** — DECISIONS.md #1–4, #21–29.
**Notes** — the photoreal previews in this milestone are placeholders: `tools/panel_forge` was started in parallel and its first-pass art lands with M2.

---

## M2 — Reverb core  ·  [UI previews](ui_previews/m2_reverb_core/index.html)

**Built**
- `dsp::RealFFT`: double-precision FFT with opaque spectra — Apple vDSP on macOS, PFFFT (AVX) elsewhere; scale self-calibrated.
- `dsp::Convolver`: zero-latency non-uniform partitioned convolution (64-tap direct head; 64/512/4096/32768 FFT partitions, Garcia-style with ≥ 2P start offsets), Efficient mode (uniform 1024 partitions, reports 1024), true stereo (LL/LR/RL/RR, inactive paths skipped), any block size 1–8192.
- `ConvolutionWorker`: background thread for tail partitions; lock-free posting, audio-thread take-over if a block is late (no locks, no allocation, no dropouts).
- `IRProcessor`: r8brain 24-bit resampling to the session rate (Size = resample to a scaled rate), loudness normalisation (K-weighted), Decay reshaping, time-varying STFT Damping, Early/Late split, Reverse, Start trim, Length fade, RT60 estimate, thumbnail + decay spectrogram.
- `IRManager`: background rebuilds with 30 ms debounce, lock-free hand-off, 50 ms equal-power crossfade in the engine, off-thread deletion; user IRs (WAV/AIFF/FLAC, 1/2/4 ch, ≤ 20 s) with path + content hash in state, small files embedded, missing-file → relink prompt on the IR display; real-time pre-delay line (crossfaded).
- `IRLibrary` + **`tools/ir_forge`**: 37 original true-stereo 24-bit/96 kHz factory IRs (Rooms ×9, Plates ×5, Chambers ×3, Halls ×4, Springs ×3, Vintage ×4, Non-Linear ×5, FX ×4; 18.9 MB FLAC) from our own FDN (16/32 lines, modulated, per-band absorption), modal plate, dispersive spring, non-linear/gated, vintage-digital and FX generators; `analyze.py` checks RT60/echo density/flutter/correlation/clipping — all 37 pass.
- **`tools/panel_forge`**: procedural renderer for the 5 photoreal 4U skins (panel, filmstrips, buttons, LEDs, glass overlay, VU needle, layout JSON); all 102 controls placed; 1.9–3.0 MB per skin.
- UI: IR browser lists the factory library + recent user IRs (favourites, search, categories, drag-and-drop, file chooser); IR display shows the shaped IR, RT60, spectrogram toggle and relink state.

**Tests** — 27 test cases / 879 assertions green. New: FFT round-trip; convolution vs long-double brute force **< −120 dB** for IR lengths 1–150 000 × quality {ZL, Efficient} × {mono, stereo, 4-ch} × {inline, async worker} with random block sizes; block sizes 1–8192 + reset; reported latency; 100 %-wet unit-IR null vs delayed dry (both qualities, with lookahead); RT60 estimate; r8brain resampling (< −90 dB vs ideal sine); upmix rules; normalisation; Decay/Size/Start/Length/Reverse/Early-Late/Damping behaviour; click-free IR swap; every factory IR decodes; 7-segment decimal point.
**pluginval** — VST3 strictness 10: SUCCESS.
**CPU** (48 kHz / 128, 3 s true-stereo IR, Linux x86-64, audio thread): Zero Latency 1.5 % (+ worker), Zero Latency all-inline 2.6 %, Efficient 3.3 %. Target < 3 % met in the default mode; Efficient is revisited in M8.
**Decisions** — DECISIONS.md #5–20, #30–31.
