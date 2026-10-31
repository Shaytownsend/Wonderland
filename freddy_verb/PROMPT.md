# Build Prompt: FREDDY VERB — Ducked, Blooming, Gated Drum Reverb Plugin

> Paste everything below this line into a fresh coding session.

---

## Role and goal

You are a senior audio DSP engineer and plugin developer. Build **Freddy Verb**, a professional-grade drum reverb plugin, in **C++ with JUCE 8** (CMake build). Targets: **VST3, AU, AAX (if SDK present), and Standalone**, on macOS (Universal Binary) and Windows x64.

Freddy Verb is an **insert effect for individual drum tracks** (kick, snare, toms, percussion), not a send/return reverb. It recreates in one plugin an Ableton rack I use:

```
Track input ─┬─► WET: IR Reverb ─► Ducker (keyed from dry) ─► Gate (keyed from dry) ─► Wet EQ ─► Width ─┐
             │                                                                                      ├─► Sum ─► Output
             └─► DRY: untouched drum (latency-compensated) ─────────────────────────────────────────┘
```

**The sound:** the reverb gets out of the way while the drum hits (ducked), **blooms** in as the hit dies away (the ducker releases), and is then **cut off on purpose** by a gate keyed from the dry drum, which sets when and how fast the tail fades. The bloom is the window between the duck releasing and the gate closing. That window is the identity of this plugin, so every design decision should make it easy to shape, hear and see.

Both the ducker and the gate are driven by the **plugin's own dry input** (internal sidechain). The user never sets up sidechain routing.

---

## 1. Reverb engine — must be world-class

The convolution quality has to match the best convolution reverbs on the market. This is not a "good enough" reverb.

### 1.1 Convolution core
- **Zero-latency, non-uniform partitioned convolution.** The head is direct-form or small-block FFT, the tail uses progressively larger FFT partitions (Gardner / Garcia-style scheduling). Provide a **Quality** switch:
  - *Zero Latency* (default; reports 0 samples from convolution)
  - *Efficient* (uniform partitions, reports the latency it uses)
- Run the tail partitions on a **background thread** with a deadline-safe design. The audio thread must never block, allocate, or lock.
- **64-bit double precision** accumulation internally. Denormal protection everywhere.
- **True-stereo convolution**: support mono, stereo, and 4-channel true-stereo IRs (L→L, L→R, R→L, R→R). Mono IRs are upmixed sensibly.
- Mono-in/stereo-out and stereo-in/stereo-out bus layouts.
- **IR swaps without clicks**: load and prepare the new IR on a background thread, then **crossfade** (about 50 ms equal-power) between the old and new convolvers.
- **High-quality resampling** of IRs to the session sample rate (windowed-sinc or r8brain-class quality, not linear). Support 44.1 kHz to 192 kHz.

### 1.2 IR shaping (non-destructive, recomputed off the audio thread)
- **Pre-delay**: 0–250 ms, with optional tempo sync (1/64 to 1/4, dotted and triplet).
- **Start / trim**: skip the first 0–100 ms of the IR.
- **Length**: truncate the IR with a smooth fade (10%–100%, or absolute ms).
- **Decay shaping**: multiply the IR by an envelope to shorten or lengthen the *apparent* decay without truncation artifacts.
- **Size / stretch**: time-stretch by resampling (50%–200%).
- **Early / Late balance**: split the IR at a user-set crossover time (default 80 ms) and give each part its own gain.
- **Damping**: frequency-dependent decay. High frequencies decay faster over time. Implement it as time-varying filtering of the IR during preparation, not as a static filter.
- **Reverse** toggle.
- **Normalize**: loudness-normalize every IR so switching IRs does not jump in level.

### 1.3 IR sources — both factory and user
- **User IRs**: drag-and-drop or browse WAV/AIFF/FLAC files, any length up to 20 s, any channel count 1, 2 or 4. Remember user folders, and save the IR path plus a hash in the plugin state. If the file is missing on reload, show a clear relink prompt. Optionally embed small IRs in the state.
- **Factory library**: ship a curated, categorized drum-focused set:
  - Rooms (tight, wood, live, dead)
  - Plates
  - Chambers
  - Halls
  - Springs
  - Vintage hardware-style digital
  - Non-linear / gated classics
  - FX (reversed, bloom, metallic)

  Each IR has metadata: name, category, RT60, a description, and a thumbnail or illustration.
- **Factory IR sourcing:** use only IRs that are legally redistributable, such as properly licensed CC0/CC-BY libraries with attribution recorded in `CREDITS.md`, or IRs **rendered by our own offline generator**. Build a separate offline tool, `tools/ir_forge`, that renders high-quality IRs from:
  - a dense FDN reverb (16+ lines, modulated, frequency-dependent decay);
  - a plate model;
  - a spring model;
  - a non-linear/gated model.

  It exports 24-bit/96 kHz true-stereo WAVs. Do not use IRs taken from commercial libraries.

### 1.4 IR browser
- Use a browser in the style of a high-end convolution reverb:
  - category column, IR list, search, favorites;
  - a large IR display showing the waveform and a decay/spectrogram view, with the trim, length and pre-delay handles drawn on it and draggable;
  - previous/next arrows for fast auditioning.

---

## 2. Ducker (the "duck & bloom" stage)

The ducker comes **after** the reverb and attenuates the wet signal while the dry drum is hitting. Its key signal is the dry input.

### 2.1 Detector (shared by the ducker and the gate)
- The key is taken from the dry input, before the reverb.
- **Key filter**: HPF (20 Hz–2 kHz) and LPF (200 Hz–20 kHz), so you can, say, key a snare without kick bleed.
- **Detection mode**: Peak or RMS (RMS window 1–50 ms). Stereo key = max(L, R).
- **Lookahead**: 0–10 ms, applied as a delay on the audio path and reported as latency. Default 0.
- **Key Listen** button: solo the filtered key signal.
- The ducker and gate each have **their own** threshold and sensitivity; only the filter and detection settings are shared.

### 2.2 Duck mode A — FOLLOW (tracks the drum's real envelope)
- Gain reduction follows the key's real envelope.
- Controls:
  - **Depth** (0 to −60 dB, or full duck);
  - **Threshold**;
  - **Attack** (0.1–50 ms);
  - **Release / Bloom** (5 ms–2 s): how fast the reverb comes back up;
  - **Ratio / Amount**: how strongly key level maps to reduction;
  - **Release Curve**: linear, exponential, logarithmic, S-curve.
- Program-dependent: a long, loud hit keeps the reverb ducked longer; a short hit blooms sooner.

### 2.3 Duck mode B — CURVE (triggered fixed shapes)
- A **transient detector** watches the key. It uses a threshold plus a re-trigger guard time and hysteresis.
- Each trigger fires a fixed gain curve over a set **Duration** (10 ms–2 s, or tempo-synced to 1/64 through 1 bar).
- Factory curve shapes:
  - **Linear Ramp**
  - **Exponential**: fast recovery, then easing in
  - **Logarithmic**: slow start, then a fast bloom
  - **S-Curve**
  - **Hold-then-Ramp**: stays fully ducked for X%, then ramps
  - **Pump**: classic sidechain-pump shape
  - **Swell**: overshoots slightly, about +1.5 dB, before settling, for an exaggerated bloom
- Every curve has **Depth**, **Duration** and **Curve Tension** (bends the shape).
- **Retrigger behavior**: Restart vs. Continue-from-current-level, so fast rolls don't click.
- Curves are evaluated from lookup tables or closed-form math at sample rate, with no zipper noise.

---

## 3. Gate (tail cutoff stage)

The gate comes **after** the ducker. It is keyed from the dry input through the shared detector, **not** from the reverb. It decides when the tail ends and how fast it fades.

- Controls:
  - **Threshold**;
  - **Hysteresis** (0–12 dB, prevents chatter);
  - **Attack** (0.05–50 ms);
  - **Hold** (0–2 s, or tempo-synced): how long the bloom lives after the drum drops below threshold;
  - **Release** (5 ms–5 s): how fast the tail fades out;
  - **Range / Floor** (−∞ to 0 dB): close to a floor so a little tail can remain;
  - **Release Curve**: linear, exponential, log.
- **Gate Off** toggle for a plain ducked reverb.
- Gain is smooth and per-sample, with no clicks at any setting.

---

## 4. Wet EQ / filters

The EQ processes the **wet path only**. Add a **Pre / Post** switch: pre-reverb shapes what feeds the IR; post-gate shapes the final tail. Default is Post.

- **HPF**: 20 Hz–2 kHz, 12/24/48 dB/oct.
- **LPF**: 1 kHz–20 kHz, 12/24/48 dB/oct.
- **Low shelf** and **high shelf**: frequency, gain ±18 dB, Q.
- **2× parametric bell**: frequency, gain, Q.
- Use topology-preserving transform (TPT/SVF) filters so modulating them is smooth. Use high-precision coefficients and match the analog response near Nyquist (decramped bells or 2× oversampling for the EQ only).
- Show a small EQ curve display with draggable nodes.

---

## 5. Stereo width (wet only)
- **Width**: 0% (mono) to 200%, using M/S processing.
- **Mono Below**: makes the wet signal mono below a set frequency (off, 20–500 Hz), using a Linkwitz-Riley crossover.
- **Balance / Pan** for the wet signal.
- A small correlation meter.

---

## 6. Mix and output
- **Mix** (0–100%, equal-power) **and** independent **Dry Level** / **Wet Level** faders (−∞ to +6 dB). Mix and the faders both apply.
- **Mix Lock**: keeps the current Mix when browsing presets.
- **Wet Solo**.
- **Output Gain** ±24 dB.
- **Dry path latency compensation**: the dry signal is delayed by exactly the wet path's latency (convolution + lookahead), so the two stay sample-aligned and phase-coherent. Report the total latency to the host.
- Metering: input, wet (post-gate), output, each with peak and RMS.

---

## 7. Envelope visualizer (central display)

A **scrolling real-time display** that makes the duck → bloom → gate relationship visible. This is the centerpiece of the UI.

- Draws on a shared time axis:
  - **dry input waveform**, dim;
  - **wet signal waveform**, after the duck and gate;
  - **duck gain curve**;
  - **gate gain curve**;
  - **combined wet gain** (duck × gate), highlighted;
  - **threshold lines** for the ducker and the gate.
- The **bloom window** (from when the ducker releases to when the gate closes) is shaded and labeled.
- Draggable handles on the display for duck depth, bloom/release, gate hold and gate release, all mapped to the real parameters.
- Time scale options (250 ms, 500 ms, 1 s, 2 s, 4 s) and a **Freeze** button.
- **Static preview mode**: when audio is stopped, draw the theoretical envelope response to a single synthetic hit with the current settings, so users can design the shape without playback.
- Data path: audio thread → **lock-free SPSC FIFO** → UI thread at 60 fps. The UI never touches audio-thread state directly.

---

## 8. UI design — "vintage sampler hardware, flat 2D"

The look is inspired by **late-'80s / early-'90s rack samplers and MPC-era hardware**, rendered **completely flat and 2D**: no skeuomorphic gloss, no 3D bevels, no photoreal textures. It should feel like a clean vector illustration of the hardware. Use an organized, premium layout in the spirit of top-tier convolution reverbs, with the space, IR and envelope as the visual focus.

> Inspired by, not copied from: **do not use any real manufacturer names, logos, trade dress, or product layouts.** All branding is FREDDY VERB.

- **Palette**:
  - warm putty or light grey chassis (#D8D3C8-ish);
  - charcoal sections (#2B2B2B);
  - one accent color (sampler red-orange, about #E2532D) for the logo and active states;
  - a secondary muted teal or blue for the gate.
- **The LCD**: the envelope visualizer and IR display sit in a **backlit LCD-style panel** (green-yellow #B6C76A on dark olive, or amber). Use a flat pixel-grid look and a crisp bitmap-style font for readouts like `DUCK -24.0dB   BLOOM 340ms`.
- **Controls**:
  - flat circular knobs with a single indicator line and value arcs;
  - flat rectangular "rubber pad" buttons with small LED dots for on/off state;
  - a flat **data-entry slider / jog wheel** that edits whichever parameter was touched last;
  - screen-printed-style section labels in condensed uppercase (`REVERB · DUCK · GATE · TONE · SPACE · MIX`).
- **Layout**, left to right, following the signal flow:
  1. top bar: logo, preset browser, A/B compare, undo/redo, settings;
  2. left: IR browser and IR display (LCD);
  3. center: the big envelope LCD;
  4. below it: the DUCK and GATE sections side by side, with the mode switch FOLLOW / CURVE and curve-shape pads;
  5. right: TONE (wet EQ), SPACE (width), MIX, meters.
- **Fully vector-drawn** with JUCE `Graphics` / `Path` (no bitmaps except IR thumbnails). **Resizable** 75%–200% with a fixed aspect ratio. HiDPI-correct.
- Double-click resets to default, Cmd/Ctrl-drag gives fine adjustment, and every control has a hover tooltip and a value readout on the LCD.
- Smooth 60 fps rendering. Use the OpenGL context if it is needed for the visualizer.

---

## 9. Presets
- XML/JSON preset format, factory and user folders, categories, A/B compare, and undo/redo across all parameters.
- Factory presets (at least 30), each pairing an IR with duck/gate settings. Examples:
  - "Snare Bloom Plate"
  - "Kick Room Pump"
  - "Tom Swell Hall"
  - "Tight Gated '80s Snare"
  - "Rim Chamber Flick"
  - "Clap Reverse Bloom"
  - "Perc Spring Pop"
  - "Hat Air Room"
- **Init** preset: Medium Plate, Follow duck −18 dB / 250 ms bloom, gate hold 150 ms / release 300 ms, Mix 35%.

---

## 10. Engineering requirements
- Use the JUCE `AudioProcessorValueTreeState` for every parameter. All parameters are automatable and smoothed (no zipper noise). Use sensible skew curves and units.
- **Real-time safety**: no allocation, locks, file I/O or logging on the audio thread. IR loading, resampling and shaping happen on background threads with atomic pointer swaps.
- Handle any buffer size (1–8192), including variable block sizes, and host sample-rate changes.
- **Bypass** must be click-free and latency-compensated: report the same latency, and crossfade.
- CPU target: under 3% of one core at 48 kHz / 128 samples with a 3 s true-stereo IR on a modern machine.
- State save/restore is versioned so it stays compatible with future versions.
- **Tests** (Catch2 or GoogleTest):
  - convolution matches a brute-force reference (error < −120 dB);
  - dry/wet alignment null test at 0% wet and at 100% dry;
  - reported latency is correct in every quality and lookahead mode;
  - the ducker and gate produce no clicks under step and sweep inputs (bounded derivative);
  - curve shapes match their reference tables;
  - state round-trip;
  - pluginval passes at strictness level 10.
- GitHub Actions CI builds macOS and Windows and runs the tests and pluginval.

---

## 11. Suggested project structure
```
FreddyVerb/
  CMakeLists.txt
  Source/
    PluginProcessor.{h,cpp}
    PluginEditor.{h,cpp}
    dsp/      Convolver, IRProcessor, Resampler, Detector, Ducker, Gate, WetEQ, Width, LatencyDelay
    ui/       LookAndFeelFreddy, LCDPanel, EnvelopeVisualizer, IRBrowser, IRDisplay, Knob, PadButton, DataSlider
    state/    Parameters, PresetManager, IRLibrary
  Resources/  factory IRs, presets, fonts (open-licensed)
  tools/ir_forge/
  tests/
  CREDITS.md
```

---

## 12. Milestones (deliver and verify each one before moving on)
1. Project skeleton, parameters, pass-through, latency-compensated dry/wet mix.
2. Zero-latency convolution engine, IR loading and resampling, tests.
3. Shared detector, the Ducker (Follow and Curve modes) and the Gate, tests.
4. Wet EQ, width, metering.
5. IR shaping (pre-delay, trim, length, decay, size, damping, early/late, reverse).
6. UI: look-and-feel, layout, LCD, envelope visualizer, IR browser.
7. `ir_forge`, the factory IR library, factory presets.
8. Polish, CPU optimization, pluginval, CI, installers.

At each milestone, summarize what was built, how it was tested, and any trade-offs you made. Ask me before deviating from this spec.
