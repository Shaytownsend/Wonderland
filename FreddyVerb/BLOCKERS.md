# Blockers and workarounds

Things that could not be done as specified in this environment, what was done instead, and how to finish them.

| # | Item | Status | Workaround / how to finish |
|---|------|--------|----------------------------|
| 1 | **AAX format** | Skipped | The AAX SDK is not public. Pass `-DFV_AAX_SDK_PATH=/path/to/aax-sdk` to CMake and the AAX target is added automatically (`juce_set_aax_sdk_path`). Signing (PACE wraptool) is also required for Pro Tools release builds. |
| 2 | **macOS / Windows builds verified locally** | Not possible here | Development runs on Linux (VST3, CLAP, LV2, Standalone are built and validated here). macOS Universal (AU/VST3/CLAP/Standalone) and Windows x64 are built and tested by `.github/workflows/ci.yml`. The vDSP FFT backend is compiled only on macOS, so it is exercised by CI, not locally. |
| 3 | **pluginval release binaries** | Downloads blocked (HTTP 403 through the session proxy) | Built pluginval v1.0.4 from source (`/home/user/deps/pluginval`). CI downloads the official release binaries. |
| 4 | **Blender (photoreal skins)** | Not installed | `tools/panel_forge` uses the procedural Python pipeline (numpy/Pillow/scipy) as the spec allows; see DECISIONS.md. |
| 5 | **JUCE licence page (juce.com)** | Host not reachable from this session | README licence notes are based on the repository's LICENSE.md plus the published JUCE 8 tiers as known at writing time. **Re-check https://juce.com/legal/juce-8-licence/ before release.** |
| 6 | **Intel IPP FFT backend** | Not implemented | `FV_USE_IPP` is wired in CMake but the convolver ships with PFFFT (double, AVX) on Windows/Linux, which the spec allows ("Intel IPP or PFFFT"). |
