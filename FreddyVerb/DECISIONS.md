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
7. **Worker wake-up** — the worker polls every 300 µs instead of being signalled, because every portable signalling primitive (condition variables, `std::counting_semaphore` before macOS 11) can lock on the audio thread. Cost is negligible.
8. **Efficient mode** — one uniform 1024-sample partition stage, latency 1024 (21 ms @ 48 kHz); lower CPU than Zero Latency for long IRs, all on the audio thread. *Change:* `Convolver::kEfficientPartition`.
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
