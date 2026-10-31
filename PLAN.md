# The Queen — Plan

4-operator FM + Juno-60-style polysynth. JUCE 8.0.15 via Pamplejuce. Formats: AU (macOS only), VST3, CLAP, Standalone.

## File tree
```
CMakeLists.txt                 identity + formats; Linux builds skip AU
source/Parameters.h            constexpr table — the ONLY place controls are defined
source/PluginProcessor.{h,cpp} APVTS, SynthEngine, PatchMemory, state
source/PluginEditor.{h,cpp}    HostBar + PanelComponent, fixed aspect resize
source/dsp/FmOperator.h FmVoice.{h,cpp} Algorithms.h JunoFilter.{h,cpp} Lfo.h
source/dsp/Chorus.{h,cpp} Arpeggiator.{h,cpp} SynthEngine.{h,cpp}
source/state/PatchMemory.{h,cpp}  5 banks x 8 patches, manual/write, .queen save/verify/load
source/gui/QueenLookAndFeel.{h,cpp} Filmstrip.h Layout.{h,cpp} PanelComponent.{h,cpp}
source/gui/HostBar.{h,cpp} QueenKeyboard.{h,cpp}
assets/                        sprites (@4x) + layout.json (embedded, or installed as files on the Mac)
queen_blender/THE_QUEEN.blend  source scene
queen_blender/scripts/extract_registry.py  bpy: scene -> queen_blender/tex2/J.json (registry)
tools/make_layout.py           J.json -> assets/layout.json
tools/export_assets.py         bpy headless render of every asset
tools/make_placeholders.py     PIL placeholder art, same filenames
tests/                         Catch2: params<->registry, algorithms, voice, juno, arp, patches, gui
```

## Milestones
M1 Skeleton · M2 Parameters · M3 FM voice · M4 Juno layer · M5 Memory + factory patches ·
M6 GUI on placeholders · M7 Real assets · M8 Validation · M9 Packaging
(DONE checks: see PROGRESS.md; Linux adaptations: see NOTES.md)
