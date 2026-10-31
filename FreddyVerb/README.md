# Freddy Verb

A drum reverb **insert** for kick, snare, toms and percussion. It recreates a duck → bloom → gate rack in one plugin:

```
Track in ─┬─► WET: IR reverb ─► Ducker (keyed from dry) ─► Gate (keyed from dry) ─► Wet EQ ─► Width ─┐
          │                                                                                         ├─► Mix ─► Out
          └─► DRY: untouched, latency-compensated ──────────────────────────────────────────────────┘
```

The reverb gets out of the way while the drum hits (ducked), blooms in as the hit dies away, and is then cut off on purpose by a gate keyed from the dry drum. Both dynamics stages are keyed internally from the plugin's own input; no sidechain routing needed.

Status and per-milestone reports: [`PROGRESS.md`](PROGRESS.md). Design choices: [`DECISIONS.md`](DECISIONS.md). Known gaps: [`BLOCKERS.md`](BLOCKERS.md).

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

- macOS Universal: add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.
- AAX: add `-DFV_AAX_SDK_PATH=/path/to/aax-sdk`.
- Offline builds: point `-DFETCHCONTENT_SOURCE_DIR_JUCE=…` (and `_CLAP_JUCE_EXTENSIONS`, `_R8BRAIN`, `_PFFFT`, `_CATCH2`) at local clones.

Outputs land in `build/FreddyVerb_artefacts/Release/{VST3,AU,CLAP,LV2,Standalone}`.

### Tools
- `FreddyVerb_Snapshots --out ui_previews/<milestone> --milestone "<name>"` renders all 13 UI options to PNG plus a contact sheet `index.html`. `--manifest <file>` exports the control list used by `panel_forge`.
- `tools/ir_forge` — offline factory IR generator (FDN, plate, spring, non-linear models).
- `tools/panel_forge` — offline renderer for the five photoreal rack skins.

## Licensing (check before release)

- **JUCE 8** is dual-licensed: **AGPLv3**, or the commercial **JUCE 8 licence**. Under the JUCE 8 EULA the commercial tiers are, as published when this was written: **Starter** (free, for individuals/companies under a small annual revenue cap — USD 50k at the time of writing), **Indie** (paid, revenue cap USD 500k), **Pro** (paid, no cap), plus Educational. JUCE 8 no longer requires the "Made with JUCE" splash screen. A closed-source commercial release needs one of the commercial tiers that matches your revenue; otherwise the plugin's source must be released under AGPLv3. **These tier details could not be re-verified from this build environment (juce.com was unreachable) — confirm them at https://juce.com/legal/juce-8-licence/ before shipping.**
- **AAX** requires an Avid developer agreement and PACE signing.
- **VST3** SDK is MIT (since 3.8); the VST trademark/logo has its own usage guidelines.
- All other third-party components and their licences: [`CREDITS.md`](CREDITS.md).
