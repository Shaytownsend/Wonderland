# NOTES — decisions log

## Pins
- Pamplejuce template: sudara/pamplejuce main @ ca3cae0b288a644d11bb64651f81c2e0a7e7b942 (2026-08-16)
- JUCE submodule: tag 8.0.15 (91ad83ae34)
- clap-juce-extensions: submodule (Pamplejuce mechanism, not CPM) pinned to commit 7adee3a (main 2026-10-06).
  Its newest tag 0.26.0 is from 2022 and fails to compile against JUCE 8.0.15 (missing LegacyAudioParameter.cpp).
- cmake-includes @ 8de22da, Catch2 3.8.1 (CPM, from cmake-includes)
- bpy 5.2.3 + Pillow in .venv (Python 3.13 worked, no 3.11 needed)

## Environment (session 1)
- Sessions run in a Linux cloud container (Ubuntu 24.04, gcc 13 / clang 18, no Xcode, no macOS).
  Build here: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`.
  On the Mac the pinned Xcode command works unchanged. AU is added to FORMATS only if APPLE.
- macOS-only DONE checks (auval, codesign, `open ...app`, top -pid, ~/Library install) cannot run here;
  Linux equivalents are used and logged per milestone; mac steps are scripted for the user.
- Repo root also contains the unrelated `ace_rack_mount/` project — left untouched.

## Inputs
- The upload contained ONLY THE_QUEEN.blend (now queen_blender/THE_QUEEN.blend). j60_ink.py, build3.py,
  tex2/J.json and textures/ were NOT provided. Decision: recover the registry from the scene itself.
  queen_blender/scripts/extract_registry.py reads object world positions (inverse of S(x,y)):
  x = X*100+900, y = 415 - Y*100, classifies by mesh/material name (cap, scap, sw_slot, bcap, led, white, bk,
  wheel, disp) and assigns param ids from a hand mapping (by panel section, ordered by x/y) checked against
  the panel ink texture. Packed textures are extracted to queen_blender/textures/.
- Scene object names are generic (Cube.123); semantic names live in mesh-data / material names.
  export_assets.py hides by material/mesh name prefixes (equivalent to the knurl/capk/... list).

## Decisions
- Dropped from template: IPP (not needed), benchmarks, melatonin_inspector (build time).
- M1 Linux check: VST3/CLAP/Standalone built; Standalone ran 8 s under xvfb-run without crashing (no sound
  card in container); "plays a sine from MIDI" proven by tests/SkeletonTest (processBlock with note-on).
