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
