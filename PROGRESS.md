# PROGRESS (handoff — keep < 60 lines)

- [x] M1 Skeleton — DONE: Release build of all formats; Standalone plays a sine from MIDI
- [x] M2 Parameters — DONE: ParametersTest (registry coverage, no dup ids, defaults in range)
- [x] M3 FM voice — DONE: VoiceTest per algorithm: non-silent, no NaN, peak < 0 dBFS
- [x] M4 Juno layer — DONE: filter/env/lfo/chorus/arp tests; arp order up/updown/down x range 1-3
- [x] M5 Memory + factory patches — DONE: PatchRoundTrip, 8 factory patches, .queen save/verify/load
- [x] M6 GUI on placeholders — DONE: editor opens, every control moves its param, resizes
- [x] M7 Real assets — DONE: export ran, placeholder test passes, docs/ui.png checked
- [~] M8 Validation — Linux part done (pluginval VST3 SUCCESS, CPU 2.5%); mac: run tools/validate_mac.sh (auval + AU)
- [ ] M9 Packaging — DONE: install.sh, README, all checked

## Build / test
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
ctest --test-dir build -C Release
