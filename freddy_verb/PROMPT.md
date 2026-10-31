# Build Prompt: FREDDY VERB — Ducked, Blooming, Gated Drum Reverb Plugin

> Paste everything below this line into a fresh coding session.

---

## Role and goal

You are a senior audio DSP engineer and plugin developer. Build **Freddy Verb**, a professional-grade drum reverb plugin, in **C++ with JUCE 8** (CMake build). Targets: **VST3, AU, CLAP, AAX (if SDK present), and Standalone**, on macOS (Universal Binary) and Windows x64.

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

## 8A. Eight UI design options — show them to me as the code is written

I want to **see and choose between 8 different UI directions**, all following the brief in section 8: inspired by vintage sampler hardware, flat 2D, no real brand names or logos. Don't pick one for me. Build them so I can compare them side by side and watch them evolve as the plugin gets built.

### How to build them
- Write the UI so **all controls, layout logic and the visualizer are shared**. Each option is a **theme**: a `FreddyTheme` struct (colors, fonts, knob and button drawing style, LCD style, panel layout variant) plus its own `LookAndFeel` subclass. Swapping options must never touch DSP or parameter code.
- Add a **theme switcher** (hidden in Settings, or a dev-only dropdown) so I can flip between all 8 live in the standalone app while audio plays.
- Add a **screenshot tool**: a command-line target or test, e.g. `FreddyVerb_Snapshots`, that renders the editor offscreen in each of the 8 themes and writes PNGs to `ui_previews/<milestone>/<nn>_<theme>.png`. Render them with a realistic state: a drum hit visible in the envelope display, the bloom window shaded, and an IR loaded.
- Also generate `ui_previews/<milestone>/index.html`, a contact sheet that shows all 8 side by side with names and short descriptions.

### When to show them
- **Early, before deep UI work** (in milestone 1): rough but recognizable static mockups of all 8, drawn with real JUCE components and placeholder data.
- **At every milestone after that**, automatically re-render all 8 with whatever controls exist so far, so I see them fill in as the code is written.
- **At the UI checkpoint (milestone 6)**, render all 8 at full detail. Then **stop and ask me to choose** one, or to combine elements of several. This is the **only** point where you wait for me. Polish only the chosen direction to final quality, but keep the others selectable as alternate skins unless I say to remove them.
- At every other milestone, generate the previews and **keep going**. Don't wait for feedback on them.

### The 8 directions
Each must be a distinct take on vintage sampler / drum-machine hardware, flat and 2D. Use these as starting points; you may refine them, but keep 8 clearly different options.

1. **Putty Classic**: warm grey-beige chassis, green-yellow backlit LCD, red-orange accent, black screen-printed labels. The baseline look.
2. **Studio Black Rack**: charcoal 2U rackmount front panel with flat rack ears and screw dots, an amber LCD, orange pad LEDs, and dense, small labels.
3. **Cream Workstation**: off-white or cream body, a blue-backlit LCD with white pixels, soft grey pads, teal secondary accent. Clean and spacious.
4. **Pad Station**: drum-machine style. A prominent row or grid of large square rubber pads serves as the curve-shape and mode selectors. Chunky, playful, high-contrast pads on a dark grey body.
5. **Sunbleached '89**: aged, slightly yellowed plastic tones, muted pastel buttons, an orange LCD, and a subtly worn feel achieved with flat color only, no textures.
6. **Night Session**: dark mode. Near-black panel, red 7-segment/LED-style readouts, a dim red-on-black LCD, minimal labels. For dark studios.
7. **Service Manual**: drawn like a vintage hardware manual or schematic. Two-tone line art, signal-flow arrows printed on the panel between sections (REVERB → DUCK → GATE → TONE → SPACE → MIX), a monochrome LCD.
8. **Floppy Era Pro**: wide, horizontal and sectioned. Colored section stripes (each stage gets its own color band), a large central LCD and a "disk drive" style slot as the IR loader/browser button, inspired by the floppy-loading samplers of that era.

### Rules for every option
- Every option has the same functionality and every control. Only the presentation differs.
- All are fully vector-drawn, resizable, HiDPI-correct, and readable at 75% scale.
- Text contrast meets WCAG AA for labels and readouts.
- The envelope visualizer's duck, gate, combined-gain and bloom-window colors stay clearly distinguishable in every theme.

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
- **Framework: JUCE 8** is required (not iPlug2, nih-plug or others). Also build **CLAP** through `clap-juce-extensions`.
- **FFT backends**: use Apple vDSP (Accelerate) on macOS, and Intel IPP or PFFFT on Windows, for the convolution engine. Do not rely on JUCE's fallback FFT.
- **Resampling**: use **r8brain-free-src** for IR sample-rate conversion.
- **Licensing**: check and note in `README.md` the current JUCE 8 license terms (Starter tier vs. paid vs. AGPL) before release. Keep all third-party licenses in `CREDITS.md`.
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
    ui/       themes/ (8 FreddyTheme + LookAndFeel variants), SnapshotTool, LookAndFeelFreddy, LCDPanel, EnvelopeVisualizer, IRBrowser, IRDisplay, Knob, PadButton, DataSlider
    state/    Parameters, PresetManager, IRLibrary
  Resources/  factory IRs, presets, fonts (open-licensed)
  tools/ir_forge/
  ui_previews/   (generated 8-theme PNGs + contact sheets per milestone)
  tests/
  CREDITS.md
```

---

## 12. Autonomous execution rules

Run the whole build **end to end without asking me anything**, except for **the one UI selection checkpoint** (milestone 6). Specifically:

- **Never stop to ask questions or wait for approval** at any other point. If something in this spec is ambiguous, choose the option that best serves the sound described in "Role and goal" and log it in `DECISIONS.md`: what you decided, why, and how to change it. Then continue.
- **If something is blocked** (e.g. the AAX SDK isn't available, or a dependency won't download), skip or stub that piece, log it in `BLOCKERS.md`, and continue with everything else. Don't stop the build over it.
- **Self-verify instead of asking me.** At the end of every milestone:
  1. build all formats;
  2. run the full test suite;
  3. run pluginval on the available formats;
  4. render the 8 UI previews.

  If anything fails, fix it before moving on. A milestone is done only when its tests are green.
- **Commit at the end of each milestone** with a clear message, and append a short entry to `PROGRESS.md`:
  - what was built;
  - test results;
  - CPU numbers;
  - decisions made;
  - a link to that milestone's `ui_previews/.../index.html`.

  That file is my progress report; don't pause to report in chat.
- **Work efficiently:**
  - Write the interfaces and tests for each DSP block first, then the implementation.
  - Build independent pieces in parallel where your tooling allows (e.g. EQ/width alongside the ducker/gate; the theme work alongside the DSP).
  - Reuse proven libraries (JUCE, vDSP/IPP/PFFFT, r8brain, clap-juce-extensions, Catch2) instead of reinventing them.
  - Only rebuild what has changed. Keep incremental builds fast (ccache, unity builds where safe).
  - Don't gold-plate early milestones. Get each block correct and tested, then optimize in the polish milestone using measured profiling data, not guesses.

---

## 13. Milestones (run back-to-back; only milestone 6 waits for me)
1. **Foundation**:
   - CMake/JUCE/CLAP project and CI;
   - all parameters (APVTS);
   - latency-compensated dry/wet pass-through;
   - test harness;
   - theme system and snapshot tool;
   - **rough mockups of all 8 UI options**.
2. **Reverb core**:
   - zero-latency partitioned convolution (vDSP/IPP/PFFFT);
   - true stereo;
   - IR loading and r8brain resampling;
   - click-free IR swaps;
   - full IR shaping (pre-delay, trim, length, decay, size, damping, early/late, reverse, normalize);
   - tests and previews.
3. **Dynamics**:
   - shared detector (key filter, peak/RMS, lookahead, key listen);
   - Ducker Follow mode and all Curve-mode shapes;
   - Gate;
   - live envelope visualizer data feed;
   - tests and previews.
4. **Tone & space**:
   - wet EQ (pre/post) and width / mono-below;
   - mix section, metering, bypass;
   - tests and previews.
5. **Content**:
   - `ir_forge` offline IR generator;
   - the factory IR library with metadata;
   - the IR browser;
   - 30+ factory presets and the preset manager (A/B, undo/redo);
   - previews.
6. **UI checkpoint — the only stop.**
   - Finish all 8 UI options to full detail with every control wired up and working.
   - Render the final contact sheet.
   - **Ask me to pick one** (or a combination) and wait.
7. **Final UI**: polish the chosen direction to release quality: resizing, HiDPI, tooltips, fine-adjust, accessibility contrast. Keep the other 7 as alternate skins.
8. **Release**:
   - profile and optimize against the CPU target;
   - pluginval strictness 10 on all formats;
   - full test pass;
   - installers (macOS pkg, Windows installer);
   - final `README.md`, `CREDITS.md`, licensing notes;
   - a final summary of everything in `PROGRESS.md`.

Milestones 7–8 also run without my input once I've picked the UI.
