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

---

## M3 — Dynamics  ·  [UI previews](ui_previews/m3_dynamics/index.html)

**Built**
- `Detector`: key HPF/LPF (TPT SVF), Peak (10 ms release) or exact moving-window RMS, stereo key = max(L, R), filtered key for Key Listen. Lookahead (M1) keeps the key ahead of the wet by exactly the lookahead time.
- `Ducker` FOLLOW: threshold / amount / depth (or FULL) / attack / bloom with linear, exponential, logarithmic or S release; program-dependent.
- `Ducker` CURVE: transient trigger with guard time and hysteresis; Linear Ramp, Exponential, Logarithmic, S-Curve, Hold-then-Ramp, Pump, Swell (+1.5 dB); Depth, Duration (or tempo sync), Tension; Restart / Continue retrigger.
- `Gate`: threshold, hysteresis, attack, hold (or tempo sync), release with linear / exponential / log curves, floor (−∞…0 dB), Gate Off.
- Engine: wet × duck × gate per sample; visualizer frames now carry the real key level, duck and gate gains; static preview uses the real dynamics code.

**Tests** — 35 test cases / 2 363 assertions green. New: curve shapes vs independent reference table (max error < 1e-9), shape endpoints + Swell peak = +1.5 dB, detector peak/RMS levels and key HPF rejection, FOLLOW program dependence and depth, CURVE retrigger guard, gate hold/release timing, **bounded-derivative click tests** under step, swept and fast-roll inputs for every mode × release curve/shape × tension × retrigger mode × attack 0.1–50 ms × release 5–2000 ms with full-depth ducking, plus absolute click thresholds at default settings.
**pluginval** — VST3 strictness 10: SUCCESS.
**CPU** (48 kHz / 128, 3 s true-stereo IR, audio thread): Zero Latency 1.55 %, all-inline 2.40 %, Efficient 3.20 %.
**Decisions** — DECISIONS.md #32–37.

---

## M4 — Tone & space  ·  [UI previews](ui_previews/m4_tone_space/index.html)

**Built**
- `WetEQ`: HPF 20 Hz–2 kHz and LPF 1–20 kHz at 12/24/48 dB/oct, low & high shelf (freq, ±18 dB, Q), two bells (freq, ±18 dB, Q); TPT SVF, 2× oversampled, decramped bells, smoothed parameters (coefficients every 8 samples); Pre/Post placement with crossfade.
- `WidthStage`: width 0–200 % (M/S), mono-below 20–500 Hz (LR4 on the side), wet balance.
- Mix section, peak + RMS metering (input / wet post-gate / output), correlation meter and click-free latency-compensated bypass were completed in M1 and now see the full wet chain.

**Tests** — 40 test cases / 2 387 assertions green. New: measured EQ response vs analogue bell / shelf prototypes (incl. a 15 kHz bell at 48 kHz within 0.5 dB), Butterworth −3 dB points and slopes for 12/24/48 dB/oct, worst-case automation (bell frequency jumping every 64 samples) without zipper, width 0/100/200 %, balance, mono-below (50 Hz side < −30 dB, 2 kHz side untouched).
**pluginval** — VST3 strictness 10: SUCCESS.
**CPU** (48 kHz / 128, 3 s true-stereo IR, EQ on with default HPF 24 + LPF 12, audio thread): Zero Latency 2.55 %, all-inline 3.85 %, Efficient 4.17 %. The oversampled EQ costs ≈ 1 %; it is the first optimisation target for M8.
**Decisions** — DECISIONS.md #38–40.

---

## M5 — Content  ·  [UI previews](ui_previews/m5_content/index.html)

**Built**
- `tools/ir_forge` and the 37-IR factory library landed in M2 (see there).
- `PresetManager`: factory + user presets, categories, Init, Save As / Delete / Reveal folder, prev/next stepping, dirty flag, Mix Lock, A/B compare with copy, undo/redo integration (one transaction per gesture / preset load).
- **35 factory presets** (`tools/presets/make_presets.py`) across Snare, Kick, Toms, Perc, Vintage, Non-Linear, FX and Utility — including all eight examples from the brief (Snare Bloom Plate, Kick Room Pump, Tom Swell Hall, Tight Gated '80s Snare, Rim Chamber Flick, Clap Reverse Bloom, Perc Spring Pop, Hat Air Room). Each pairs a factory IR with its own duck/gate/tone settings.
- IR browser: remembered user folders (add/remove), recent files, favourites, search, category column, drag-and-drop, prev/next.
- Preset LCD shows A/B slot, name and modified marker; preset menu with category sub-menus.

**Tests** — 46 test cases / 3 280 assertions green. New: ≥ 30 factory presets, every preset's parameter IDs and ranges valid and its IR present, the brief's eight named presets exist, load applies values and defaults the rest + loads the IR, dirty flag, Mix Lock, A/B round trip and copy, user save/rescan/load/delete, undo/redo.
**pluginval** — strictness 10: VST3 SUCCESS, LV2 SUCCESS.
**CPU** — unchanged from M4.
**Decisions** — DECISIONS.md #41–44.

---

## M6 — UI checkpoint  ·  [UI previews — all 13 options](ui_previews/m6_ui_checkpoint/index.html)

**Built**
- **8 flat options at full detail.** Section layouts are now balanced grids (no orphan knobs; neighbouring sections share one knob size). Per-theme hardware detail, all flat vector: Putty Classic (moulding seam, corner screws, vents), Studio Black Rack (rack handles, louvres, 2U plate), Cream Workstation (pinstripes, badge, spacious), **Pad Station** (new full-width 12-pad PAD BANK: enable, duck mode and the 7 curve shapes with big icons, colour-coded groups), Sunbleached '89 (sun-fade patches, pastel function-coded pads, masking-tape label), Night Session (minimal, status LEDs, 7-segment readouts), Service Manual (line art, printed REVERB → DUCK → GATE → TONE → SPACE → MIX arrows, drawing title block), **Floppy Era Pro** (re-balanced to 1560×920, numbered colour stage stripes, large clickable disk-drive IR slot).
- **5 photoreal options at final detail** (`panel_forge` M6 pass): curve icons on the shape keys, broadcast knurled knobs + VU faces, sculpted chicken-heads with rotating shadows, hammered paint, readable toggles and illuminated rockers, procedural handwriting on tape notes, fixed readout glow, gentler glass overlay. 1.8–3.2 MB per skin.
- WCAG AA contrast for every label/readout in all 13 themes; envelope duck/gate/combined/bloom colours distinguishable in all 13 (both enforced by tests). 7-segment decimal point fixed.
- Steppers abbreviate long names like hardware LCDs only when needed; everything readable at 75 %, sharp at 200 %.
- **Fix found by pluginval's randomised mode:** bool/choice parameters holding fractional host values were not re-synced by `setStateInformation` (APVTS only notifies changed tree values). Now every saved value is pushed explicitly; unit test added.
- EQ coefficient recomputation now skipped when its parameters are settled.
- Installer scripts: macOS `.pkg` (`packaging/macos/build_pkg.sh`) and Windows Inno Setup (`packaging/windows/FreddyVerb.iss`), built by CI.

**Tests** — 50 test cases / 3 467 assertions green (new: theme count/order, WCAG AA contrast, visualizer colour separation, state re-sync).
**pluginval** — VST3 and LV2 strictness 10: SUCCESS (standard order, repeated). Randomised order: see BLOCKERS #7 (JUCE VST3 cache artefact on a 2-option choice) and #8 (one unreproduced crash; investigate with `gdb -batch -ex run -ex bt --args pluginval --strictness-level 10 --repeat 8 --randomise <plugin>`).
**CPU** (48 kHz / 128, 3 s true-stereo IR, EQ on, quiet machine): Zero Latency 2.04 % (audio thread), all-inline 3.41 %, Efficient 3.86 %.

**⏸ Waiting for the UI choice** — pick one of the 13 options (or a combination) to polish to release quality in M7. The others stay selectable as alternate skins unless you say otherwise.

---

## M7 — Final UI: Tour-Worn Champagne  ·  [UI previews](ui_previews/m7_final_ui/index.html) · [Champagne @ 2×](ui_previews/m7_final_ui/champagne_2x/12_tour_worn_champagne.png)

**Your choice:** 12 — Tour-Worn Champagne, now the default skin. The other 12 stay available as alternate skins (SETUP menu).

**Built**
- Tour-Worn Champagne as the release default; editor opens on it, scales 75–200 % (fixed 19"×7" aspect) using the closest 1×/2×/3× asset set with high-quality downscaling.
- IR browser redesigned for legibility at 75 %: category stepper instead of a column, full-width list, bigger rows, current IR kept in view.
- Hover → value readout on the LCD for every knob, pad and stepper (in addition to tooltips; Cmd/Ctrl fine-drag and double-click reset unchanged).
- Envelope LCD: the measured BLOOM label sits on its own backing plate at the bottom of the bloom window so it no longer collides with the header or the waveform.
- Tests for the release default and for editor behaviour (opens on the default skin, 75–200 % with fixed aspect, clamped scale, live switching through all 13 skins).

**Tests** — 52 test cases / 3 494 assertions green.
**pluginval** — VST3 strictness 10: SUCCESS.
**CPU** — unchanged from M6 (UI only).
**Decisions** — DECISIONS.md #45–47.
