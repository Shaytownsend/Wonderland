# panel_forge

Offline renderer for Freddy Verb's five photoreal "aged studio rack gear" skins
(`harmonizer79`, `reverb84`, `broadcast`, `champagne`, `lab`). It turns
`controls_manifest.json` into everything `Source/ui/themes/photoreal/` consumes:
`Resources/skins/<folder>/layout.json` plus the panel, knob, button, LED,
needle and overlay images.

```
python3 tools/panel_forge/panel_forge.py --all                 # all five skins (~2.5 min, 3 in parallel)
python3 tools/panel_forge/panel_forge.py --skin lab            # one skin
python3 tools/panel_forge/panel_forge.py --skin lab --only panel   # just the panel (or: parts | layout)
```

Requirements: Python 3, numpy, scipy, Pillow. Fonts: the OFL fonts in
`Resources/fonts/` (Barlow Condensed SemiBold/Medium, Share Tech Mono). No network,
no downloaded textures. After adding/removing a skin folder re-run CMake (the
skin list is globbed at configure time).

## Pipeline decision (for DECISIONS.md)

**The procedural fallback pipeline is used; Blender is not available on the build
machine.** All art is generated with numpy/scipy/Pillow:

* Every surface is a *height field + material* (albedo, specular strength/gloss,
  brushed-anisotropic sheen, emissive) rendered at 3x (4800 x 1767) and lit by a
  single fixed top-left key light (`pf_core.LIGHT`): Blinn-Phong diffuse/spec,
  ray-marched cast shadows from the height field, a blurred-height ambient
  occlusion term and a light falloff/vignette. @2x and @1x are Lanczos downscales
  of the 3x master.
* Knob/jog filmstrips and button states use the *same* light and shading code,
  rendered per frame with the geometry rotated and the lighting fixed (so
  highlights stay put while the indicator turns). 2x supersampled.
* Materials: brushed aluminium (row-noise grain with slow drift), satin paint
  (orange peel), hammertone (two-scale fbm dimples + metallic flake), anodizing
  (brushed + tint), crinkle finish on the broadcast bezels.
* Wear is fully deterministic (fixed seed per skin): a "hand time" map built from
  the most-used controls (`pf_skins.USAGE`) drives scratch density (tangential
  fingernail/ring arcs around knobs), polished scuffs, grime, and silkscreen
  fade. Edge wear to bare metal, chipped paint around the corner screws, dust in
  recesses (AO-weighted, heavier toward the bottom), lint specks.
* Readability guard: all essential text is rendered into a guard mask; scratches
  are attenuated 80 % over it and silkscreen fade is clamped so every legend
  keeps >= 55 % ink.

## Files

| file | role |
| --- | --- |
| `panel_forge.py` | CLI / orchestration, output writing (JPEG panels, quantized PNG parts) |
| `pf_layout.py` | control placement on the 1600 x 589 (19" x 4U) panel, printed legends, section frames; validates "every id placed / inside the ears / no overlaps / shared art = same size" |
| `pf_skins.py` | the five directions as data (materials, inks, part styles, wear level, extras) and the usage weights |
| `pf_panel.py` | panel renderer: material, ears + EIA slots + rack screws/washers, corner screws, display/stepper/meter bezels, VU faces (broadcast), button wells, knob shadows + dial scales, silkscreen/engraving/label-tape, wear, dust, tape notes, plates, stickers; plus the glass overlay |
| `pf_parts.py` | knob / chicken-head / jog filmstrips, square keys, membrane keys, lamp buttons, toggles, rockers, LEDs, VU needle |
| `pf_core.py` | noise, SDF primitives, lighting, text masks, PNG helpers |
| `controls_manifest.json` | input from `FreddyVerb_Snapshots --manifest` |

## Layout (shared by all skins)

* Rack ears 56 px each side (4 slotted EIA holes per ear at 0.25", 1.5", 5.5",
  6.75"); content between x = 76 and 1524.
* Top bar: branding, preset `<` LCD `>`, A / B / A>B, UNDO / REDO, model plate, SETUP.
* Top band: IR browser + IR waveform | envelope (568 x 210, the centrepiece) with
  readout, TIME / FREEZE and the DATA wheel | TONE (EQ display, slope steppers,
  band keys, filter + selected-band knobs) | MIX (knobs, meters, LOCK / SOLO / BYPASS).
* Bottom band: REVERB | DETECT | DUCK | GATE | SPACE, legends printed above every
  control. The FOLLOW-only and CURVE-only DUCK controls have their own positions
  (the C++ dims the inactive group).
* Broadcast only: `meter_in` / `meter_wet` are wide analog VU meters (face baked
  into the panel, `vu_needle@Nx.png` drawn by code); `meter_out` stays an LED bar.
* Per skin only the part sizes change (pad 30x26 keys, 34x22 membrane keys,
  26x30 toggles, 22x28 rockers).

Art names: `knob_small` (40 px) / `knob_large` (46 px) (`knob_chicken`,
`knob_chicken_large` on `lab`), `jog`, `key`, `lamp` (broadcast), `toggle`
(champagne pads), `rocker` (lab pads), `led_on/off`, `overlay`, `vu_needle`.

## Tuning

Most look decisions live in `pf_skins.py` (colours, wear level, extras). Seeds
are fixed per skin (`seed`), so output is reproducible bit-for-bit for a given
numpy/scipy/Pillow version.
