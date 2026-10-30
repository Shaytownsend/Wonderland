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
- M2: registry = queen_blender/tex2/J.json from extract_registry.py (121 controls + keys). Memory buttons
  (bank/patch/manual/write/save/verify/load) carry `action`, not `param`. Radio buttons (algo_1..8, range_16/8/4)
  carry `param` + `value`. Switch `positions` = choice indices top->bottom. arp_sync is the only param with
  onPanel=false (lives in HostBar settings). Unspecified defaults: op velocity .5, pwm_mode man, arp_mode up.
  Log-ish skews are chosen so the slider midpoint = geometric mean of the range.
- M3: FM depth: modulator level 1 = 1 cycle phase offset; feedback 1 = 0.35 cycle. Carrier sum /numCarriers *0.5.
  op3 as carrier = SUB (ratio x0.5, gated by wave_sub, x sub_level); op1/op2 carriers gated by pulse/saw.
  PWM factor on op2/op4 level: man = 2*pwm (0.5 neutral), env = 1+pwm*(2env-1), lfo = 1+pwm*lfo.
  Engine renders in <=16-sample control chunks split at MIDI/arp events (sample-accurate); voices read a
  VoiceGlobals snapshot (SmoothedValue 10 ms per float param). 2x oversampling via juce::dsp::Oversampling
  (render into the upsampled block). Linux: "audibly a synth" verified by pitch test (zero-crossing C4/oct/tune).
- M4: VCF cutoff octaves = env*8*pol + lfo*4 + kybd*(note-60)/12 + bend*bend_sens_vcf*2; res -> ladder 0..0.97.
  Chorus: juce::dsp::Chorus is not stereo, so each mode = mono L + mono R Chorus, R pre-rolled half an LFO
  period at prepare (anti-phase). Arp: updown doesn't repeat end notes; KEY TR = while HOLD-latched and all keys
  released, a new key transposes the pattern relative to its lowest note. HOLD with arp off = sustain pedal.
  LFO delay fade restarts when the first key goes down (auto) or on LFO TRIG press (man).
