# Credits and third-party licences

| Component | Use | Licence |
|-----------|-----|---------|
| [JUCE 8](https://github.com/juce-framework/JUCE) | Framework | AGPLv3 or commercial JUCE 8 licence (see README "Licensing") |
| [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions) | CLAP wrapper | MIT |
| [CLAP SDK](https://github.com/free-audio/clap) | via clap-juce-extensions | MIT |
| [r8brain-free-src](https://github.com/avaneev/r8brain-free-src) 6.5 | IR sample-rate conversion | MIT |
| [PFFFT](https://github.com/marton78/pffft) | FFT (Windows/Linux) | BSD-like (FFTPACK licence) |
| Apple Accelerate / vDSP | FFT (macOS) | System framework |
| [Catch2](https://github.com/catchorg/Catch2) 3.16 | Tests | BSL-1.0 |
| [pluginval](https://github.com/Tracktion/pluginval) | Validation (CI/dev only, not shipped) | GPLv3 |
| [clap-validator](https://github.com/free-audio/clap-validator) | CLAP validation (CI only, not shipped) | MIT |
| [Inno Setup](https://jrsoftware.org/isinfo.php) | Windows installer (build tool, not shipped) | Inno Setup licence |
| [Barlow Condensed](https://github.com/jpt/barlow) | UI labels | SIL OFL 1.1 (`Resources/fonts/OFL-BarlowCondensed.txt`) |
| [Share Tech Mono](https://fonts.google.com/specimen/Share+Tech+Mono) | Readouts | SIL OFL 1.1 (`Resources/fonts/OFL-ShareTechMono.txt`) |

JUCE bundles further components (FLAC, libpng, zlib, VST3 SDK, LV2, HarfBuzz…); their licences are listed in JUCE's `LICENSE.md`.

## Factory impulse responses
All factory IRs are original renders by `tools/ir_forge` (our own FDN, plate, spring and non-linear models). No third-party or commercial IR data is used. They are redistributable under the project licence.

## Photoreal skin art
All photoreal panel art is generated procedurally by `tools/panel_forge` (no third-party textures or HDRIs). The 5×7 dot-matrix glyphs and 7-segment renderer are drawn in code.
