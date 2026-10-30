# PROGRESS (handoff — keep < 60 lines)

- [x] M1 Skeleton — DONE: Release build of all formats; Standalone plays a sine from MIDI
- [x] M2 Parameters — DONE: ParametersTest (registry coverage, no dup ids, defaults in range)
- [x] M3 FM voice — DONE: VoiceTest per algorithm: non-silent, no NaN, peak < 0 dBFS
- [x] M4 Juno layer — DONE: filter/env/lfo/chorus/arp tests; arp order up/updown/down x range 1-3
- [x] M5 Memory + factory patches — DONE: PatchRoundTrip, 8 factory patches, .queen save/verify/load
- [x] M6 GUI on placeholders — DONE: editor opens, every control moves its param, resizes
- [x] M7 Real assets — DONE: export ran, placeholder test passes, docs/ui.png checked
- [x] M8 Validation — Linux: pluginval VST3 strictness 8 SUCCESS, CPU 2.5%. MAC TODO: tools/validate_mac.sh (auval + AU pluginval + top)
- [x] M9 Packaging — build/install.sh (from tools/install.sh.in), README (build, re-render, sign+notarize)

## Build / test
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
ctest --test-dir build -C Release

## Next session (on the Mac)
1. Build with the Xcode command (README), `ctest --test-dir build -C Release`, `build/install.sh`.
2. `tools/validate_mac.sh` → expect "AU VALIDATION SUCCEEDED", pluginval SUCCESS for AU+VST3, CPU < 5 %.
   Log results in NOTES.md (M8 mac part). Untested on macOS so far: AU build, Xcode generator.
