# Freddy Verb

A drum reverb **insert** for kick, snare, toms and percussion. It recreates a duck → bloom → gate rack in one plugin:

```
Track in ─┬─► WET: IR reverb ─► Ducker (keyed from dry) ─► Gate (keyed from dry) ─► Wet EQ ─► Width ─┐
          │                                                                                         ├─► Mix ─► Out
          └─► DRY: untouched, latency-compensated ──────────────────────────────────────────────────┘
```

The reverb gets out of the way while the drum hits (ducked), blooms in as the hit dies away, and is then cut off on purpose by a gate keyed from the dry drum. Both dynamics stages are keyed internally from the plugin's own input; no sidechain routing needed.

Status and per-milestone reports: [`PROGRESS.md`](PROGRESS.md). Design choices: [`DECISIONS.md`](DECISIONS.md). Known gaps: [`BLOCKERS.md`](BLOCKERS.md).

## Using it

1. Insert Freddy Verb on a drum track (mono or stereo). The default **Init** sound is a Medium Plate with Follow ducking (−18 dB, 250 ms bloom) and a gate (150 ms hold, 300 ms release) at 35 % mix.
2. Pick an IR in the browser (left LCD) or a factory preset from the preset LCD (35 presets by drum: Snare, Kick, Toms, Perc, …).
3. Shape the **bloom** in the envelope LCD: drag the DEPTH handle (how far the verb ducks), the BLOOM handle (how fast it comes back), and HOLD / RELEASE (when and how fast the gate cuts the tail). The shaded window is the bloom; its length is printed on it.
4. DUCK: **Follow** tracks the drum's envelope; **Curve** fires one of seven shapes per hit (Linear, Exponential, Log, S, Hold-Ramp, Pump, Swell). GATE OFF gives a plain ducked reverb.
5. DETECT filters the internal key (e.g. high-pass to ignore kick bleed on a snare); LISTEN solos it. Lookahead lets the duck land before the hit (adds latency, reported to the host).
6. TONE / SPACE shape only the wet signal; MIX sets the balance (Mix Lock keeps it while browsing presets).

Hover any control to see its value on the readout LCD; drag the DATA wheel to edit the last touched control; Cmd/Ctrl-drag for fine adjustment; double-click to reset. SETUP switches between the 13 skins and UI sizes (75–200 %). The default skin is **Tour-Worn Champagne**.

## Building

Requirements: CMake ≥ 3.22, a C++20 compiler, Ninja (recommended). Linux also needs the usual JUCE packages (ALSA, X11, freetype, fontconfig, GL headers — see `.github/workflows/ci.yml`).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build
```

```bash
ctest --test-dir build --output-on-failure
```

- macOS Universal: add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` (minimum macOS 11).
- AAX: add `-DFV_AAX_SDK_PATH=/path/to/aax-sdk`.
- Offline builds: point `-DFETCHCONTENT_SOURCE_DIR_JUCE=…` (and `_CLAP_JUCE_EXTENSIONS`, `_R8BRAIN`, `_PFFFT`, `_CATCH2`) at local clones.

Outputs land in `build/FreddyVerb_artefacts/Release/{VST3,AU,CLAP,LV2,Standalone}`.

### Tools
- `FreddyVerb_Snapshots --out ui_previews/<milestone> --milestone "<name>"` renders all 13 UI options to PNG plus a contact sheet `index.html`. `--manifest <file>` exports the control list used by `panel_forge`.
- `tools/ir_forge` — offline factory IR generator (FDN, plate, spring, non-linear models).
- `tools/panel_forge` — offline renderer for the five photoreal rack skins.
- `tools/presets/make_presets.py` — regenerates the factory presets; `tools/curve_reference/make_curve_tables.py` — reference tables for the duck-curve tests.
- Installers: `packaging/macos/build_pkg.sh <build-dir> <version> [signing identity]` and `packaging/windows/FreddyVerb.iss` (Inno Setup); both run in CI.

## Licensing (check before release)

- **JUCE 8** is dual-licensed: **AGPLv3**, or the commercial **JUCE 8 licence**. Under the JUCE 8 EULA the commercial tiers are, as published when this was written: **Starter** (free, for individuals/companies under a small annual revenue cap — USD 50k at the time of writing), **Indie** (paid, revenue cap USD 500k), **Pro** (paid, no cap), plus Educational. JUCE 8 no longer requires the "Made with JUCE" splash screen. A closed-source commercial release needs one of the commercial tiers that matches your revenue; otherwise the plugin's source must be released under AGPLv3. **These tier details could not be re-verified from this build environment (juce.com was unreachable) — confirm them at https://juce.com/legal/juce-8-licence/ before shipping.**
- **AAX** requires an Avid developer agreement and PACE signing.
- **VST3** SDK is MIT (since 3.8); the VST trademark/logo has its own usage guidelines.
- All other third-party components and their licences: [`CREDITS.md`](CREDITS.md).
