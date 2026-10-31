# ir_forge — Freddy Verb factory IR renderer

`ir_forge` is an offline JUCE console tool that synthesises the complete factory impulse-response library
shipped inside Freddy Verb. Every IR is an original render produced by the algorithms in this folder from a
fixed seed. No third-party IR data, samples or measurements are used.

## Build and run

```sh
cmake -S . -B build-irforge -G Ninja -DCMAKE_BUILD_TYPE=Release -DFV_BUILD_TESTS=OFF -DFV_BUILD_SNAPSHOTS=OFF \
      -DFETCHCONTENT_SOURCE_DIR_JUCE=... (see the top-level CMake options)
ninja -C build-irforge ir_forge
B=build-irforge/tools/ir_forge/ir_forge_artefacts/Release/ir_forge

$B --list                 # list ids
$B --all                  # render all: Resources/irs/<id>.flac + Resources/irs/library.json
$B --all --wav            # ... plus 24-bit/96 kHz WAV masters in tools/ir_forge/out/ (gitignored)
$B --id plate_medium      # re-render selected IRs (repeat --id); library.json keeps the other entries
#   --root <repo>  (default: found by walking up from the cwd)   --jobs <n>  (default: all cores)
```

A full `--all --wav` takes about 45 s on 4 cores. The output is bit-identical for any `--jobs` value. The
modal plate partitions its work into fixed buckets, so the summation order never depends on thread count.

Verification: `python3 tools/ir_forge/analyze.py Resources/irs [--plots /tmp/irplots]`. This needs numpy,
scipy and soundfile (`pip install soundfile`), plus matplotlib if you want plots. The script prints one table
row per IR and returns a non-zero exit code if any check fails (see *Verification* below).

## Output format

* 24-bit, 96 kHz, **4 channels in true-stereo order: L→L, L→R, R→L, R→R** (input → output).
* Every IR is 100 % wet: there is no direct sound. The time origin is the arrival of the direct sound, so the
  first reflection lands where it would in the real space and no artificial pre-delay is added. Plates and
  springs start immediately.
* Finishing (`PostProcess.cpp`) runs in this order:
  1. 20 Hz high-pass (DC removal), then a 4th-order Butterworth low-pass at 20 kHz. Nothing ultrasonic is
     stored.
  2. Cut and fade. The IR is cut where its Schroeder energy-decay curve reaches −90 dB (−85 dB for the
     longest IRs). A raised-cosine fade runs from the −65 dB (or −62 dB) point to the cut.
  3. Joint peak normalisation of all 4 channels to −1 dBFS.
  4. Trimming after the last sample that survives 24-bit rounding. The final sample is exactly 0 in every
     channel.
* After writing, every file is decoded again with JUCE's own reader and compared with the render
  (≤ 1.5 LSB, last sample exactly 0).
* `Resources/irs/library.json` is an array of
  `{id, file, name, category, rt60, description, channels: 4, sample_rate: 96000, length_s, generator, seed, color}`.

### How `rt60` is measured

`rt60` is **measured from the final, normalised render**, never copied from the design targets:

1. Filter all 4 channels with a 1 kHz octave band-pass (4th-order Butterworth high-pass at 707 Hz plus
   4th-order Butterworth low-pass at 1414 Hz).
2. Sum the energy of the 4 channels.
3. Apply Schroeder backward integration to get the energy-decay curve (EDC).
4. Least-squares fit the line between −5 dB and −35 dB, then extrapolate it to 60 dB. This is **T30**.

If an IR could not reach −35 dB, T20 and then EDT would be used as fallbacks. That never happens, because
every IR ends in silence.

For non-exponential IRs (gated, non-linear and reverse) the value is the same Schroeder T30. It describes how
fast the energy curve falls, not a decay time you hear. For example, `nonlin_reverse` reports 0.07 s because
nearly all of its energy sits just before the cut. The UI should label these by length rather than RT60.
`analyze.py` reports T30 in every octave band from 125 Hz to 8 kHz and broadband.

## Generators

All processing is double precision at 96 kHz, apart from the vintage engines, which run at 32 kHz.

### 1. Acoustic spaces: rooms, chambers, halls (`SpaceReverb`, `EarlyReflections`, `FdnReverb`)

* **Early reflections** use a randomised Allen–Berkley image-source model of a shoebox room:
  * Order 3–5, limited to a window of 25–130 ms derived from the mean free path.
  * Each image gets a time jitter that grows with reflection order, plus ±1.5 dB of gain jitter. This
    removes the periodic flutter a perfect shoebox produces.
  * Part of each reflection (0.25–0.6) is scattered into a short random cluster.
  * Wall absorption is frequency dependent: the order-k reflections are low-passed k times.
  * Floor, ceiling and walls each have their own reflection coefficient.
  * Two drum sources (left and right input) at kit height feed a spaced pair of splayed cardioid or
    wide-cardioid microphones (left and right output). This gives four physically different ER paths.
* **Late field** is a feedback delay network (FDN):
  * 16 lines, or 32 for halls. Delay lengths are prime and spread geometrically between 0.55× and 2.1× the
    room's mean free path.
  * The feedback matrix is a normalised Hadamard with a random sign diagonal, so it is orthogonal and
    lossless.
  * Every line's delay is modulated by a sine plus smooth random. Rates (0.07–0.45 Hz), phases and depths
    (0.04–0.15 ms) are randomised per line. Reads use 6-point (5th-order) Lagrange fractional delay.
  * **Absorption is designed per line from the target RT60 at low, mid, high and air.** The gain is
    `g = 10^(−3·m/(fs·T60))` for the line's loop length m. It is realised as a broadband gain plus three
    first-order shelves (low shelf, high shelf, air shelf), so every line decays with the same
    frequency-dependent RT60. Nothing rings on, and the decay is a single smooth exponential per band.
  * Input diffusion is a 6-stage Schroeder allpass cascade, with a separate chain for each input.
  * Orthogonal input and output vectors give decorrelated true-stereo outputs: the late-tail correlation
    |ρ| is below 0.1.
  * The late field is excited at the direct-sound arrival and coloured like two wall bounces. It is mixed
    with the ER at a set early-to-late energy ratio, then voiced with EQ.
* **Chambers** use highly reflective rooms (β ≈ 0.95), wide-cardioid microphones, heavy scattering and
  denser input diffusion.

### 2. Modal plate (`PlateReverb`)

This is physical modal synthesis of a thin, tensioned, simply supported steel plate (2 × 1 m, 0.5 mm, EMT
140-like by default):

* Mode frequencies follow `ω_mn = sqrt(D/ρh · k⁴ + T/ρh · k²)`. Mode shapes are
  `sin(mπx/Lx) sin(nπy/Ly)`.
* The plate is driven at two points (the inputs) and picked up as velocity at two points (the outputs). This
  gives about 15–25 k modes up to 17–19 kHz, with a raised-cosine taper above 10.5–13 kHz.
* Each mode decays at the target RT60(f), interpolated smoothly in log-frequency between the low, mid, high
  and air anchors.
* Bending-wave dispersion is built into the physics: highs arrive first, which gives the plate's splashy,
  fast build-up. Echo density reaches 1 within about 4 ms. The constant modal density gives the plate's
  smooth, dense, bright tail with no flutter.
* A small random detune (0.2 %) breaks the degeneracies of the 2:1 aspect ratio.
* There is no pre-delay.

### 3. Spring tank (`SpringReverb`)

This follows Välimäki, Parker & Abel (2010). Each spring is a feedback loop containing:

* a cascade of 80–120 **stretched first-order allpasses** `(a + z^-K)/(1 + a z^-K)`, with K = fs/(2·fC) and
  fC ≈ 4.3 kHz, which gives tens of milliseconds of dispersive chirp per round trip;
* a round-trip delay of 20–52 ms, modulated by a sine plus smooth random for flutter and "drip";
* in-loop low-pass and high-pass damping;
* a loop gain set from the RT60 at 1 kHz, including the chirp's group delay.

Dispersion accumulates on every round trip, as it does on a real spring. Four spring units of different
length and dispersion are mixed with Hadamard-signed input and output polarities, so the four true-stereo
paths are mutually uncorrelated. The output is band-limited by an 8th-order low-pass at about 4.3–4.8 kHz and
a 4th-order high-pass at 110–180 Hz.

### 4. Non-linear / gated (`NonLinear`)

1. Render a dense, long-RT base: a live room or hall with ERs, or a 32-line FDN with in-loop allpasses.
2. Divide out the base's own smoothed broadband envelope (30 ms), so the decay is flat while the spectral
   evolution and density stay natural.
3. Impose the target envelope: flat, slowly decaying, or a rising "reverse" ramp (−26 → 0 dB).
4. Close the gate with an 8–18 ms raised-cosine release at 220–500 ms.

The gated rooms keep their natural first 10–30 ms of reflections.

### 5. Vintage digital (`VintageDigital`)

These model the late-'80s chain:

* The algorithm runs at **32 kHz**. It is either the FDN with deeper, faster chorus modulation (0.25–0.6 ms
  at 0.3–1.3 Hz) plus a multi-tap early pattern, or the classic figure-of-eight allpass tank published by
  Dattorro (1997). The tank uses modulated (excursion) allpasses with its original output-tap table, scaled
  to 32 kHz.
* The FDN state is **20-bit fixed point with magnitude truncation**, so the tail still dies to zero.
* A **12-bit gain-ranging DAC** quantises a 12-bit mantissa per 1 ms block in 6 dB range steps, with
  first-order error-feedback noise shaping. This adds grit that rides on the signal.
* A **steep linear-phase reconstruction filter** (×3 upsampling to 96 kHz) at 10–12.5 kHz sets the
  bandwidth.

### 6. FX (`FxGenerators`)

* **Reverse**: the first 0.85–1.4 s of a plate or hall render is faded, reversed and shaped with
  `(t/T)^p`, then ends in a 5–6 ms release.
* **Bloom**: a 32-line FDN with two in-loop allpasses per line and deep slow modulation (a cloud), shaped by
  `(1 − e^(−t/τ))^p` with τ = 0.32 s.
* **Metallic**: two layers (one detuned by ±6 cents) of tuned fractional-delay comb resonators on a
  G-minor/Bb chord, 196–1568 Hz. They have slow random pitch wobble and are excited by a 4 ms noise burst.
  Each true-stereo path uses its own random gains and polarities. A short, bright FDN wash sits underneath.

## The library

`rt60` is measured (1 kHz octave T30). Length is after trimming. Sizes are FLAC level 8.

| id | name | category | rt60 (s) | length (s) | size (MB) |
|---|---|---|---|---|---|
| `room_kick_tight` | Tight Kick Room | Rooms | 0.33 | 0.52 | 0.16 |
| `room_wood` | Wood Room | Rooms | 0.65 | 0.99 | 0.28 |
| `room_live` | Live Room | Rooms | 1.08 | 1.47 | 0.49 |
| `room_dead` | Dead Room | Rooms | 0.22 | 0.38 | 0.11 |
| `room_air_hat` | Air Room (Hats) | Rooms | 0.59 | 0.85 | 0.38 |
| `room_studio_snare` | Studio Snare Room | Rooms | 0.54 | 0.76 | 0.25 |
| `room_drum_booth` | Drum Booth | Rooms | 0.20 | 0.32 | 0.11 |
| `room_stone` | Stone Room | Rooms | 1.41 | 1.96 | 0.61 |
| `room_garage` | Garage | Rooms | 0.83 | 1.20 | 0.35 |
| `plate_medium` | Medium Plate | Plates | 1.58 | 2.14 | 0.65 |
| `plate_bright_snare` | Bright Snare Plate | Plates | 1.30 | 1.79 | 0.66 |
| `plate_short` | Short Plate | Plates | 0.79 | 1.08 | 0.34 |
| `plate_long` | Long Plate | Plates | 2.75 | 3.52 | 1.05 |
| `plate_dark` | Dark Plate | Plates | 1.77 | 2.45 | 0.53 |
| `chamber_rim` | Rim Chamber | Chambers | 0.88 | 1.20 | 0.43 |
| `chamber_echo` | Echo Chamber | Chambers | 1.72 | 2.41 | 0.72 |
| `chamber_small` | Small Chamber | Chambers | 1.12 | 1.56 | 0.51 |
| `hall_tom` | Tom Hall | Halls | 1.85 | 2.81 | 0.73 |
| `hall_large` | Large Hall | Halls | 3.08 | 4.19 | 1.17 |
| `hall_medium` | Medium Hall | Halls | 2.12 | 3.00 | 0.84 |
| `hall_bright` | Bright Hall | Halls | 2.43 | 3.31 | 1.13 |
| `spring_perc` | Perc Spring | Springs | 1.23 | 1.90 | 0.28 |
| `spring_twin` | Twin Spring | Springs | 1.93 | 2.84 | 0.42 |
| `spring_tight` | Tight Spring | Springs | 0.73 | 1.15 | 0.17 |
| `vintage_digital_room` | Vintage Digital Room | Vintage | 0.87 | 1.11 | 0.27 |
| `vintage_digital_plate` | Vintage Digital Plate | Vintage | 1.90 | 2.76 | 0.76 |
| `vintage_digital_hall` | Vintage Digital Hall | Vintage | 2.42 | 2.94 | 0.64 |
| `vintage_ambience` | Vintage Ambience | Vintage | 0.44 | 0.58 | 0.15 |
| `gated_80s` | Gated '80s | Non-Linear | 0.51 | 0.40 | 0.25 |
| `nonlin_reverse` | Reverse Non-Lin | Non-Linear | 0.07 | 0.38 | 0.25 |
| `gated_short` | Short Gate | Non-Linear | 0.29 | 0.26 | 0.16 |
| `gated_big` | Big Gated Hall | Non-Linear | 0.87 | 0.55 | 0.32 |
| `nonlin_flat` | Flat Non-Lin | Non-Linear | 0.30 | 0.33 | 0.24 |
| `fx_reverse_clap` | Reverse Clap Swell | FX | 0.16 | 0.85 | 0.37 |
| `fx_bloom` | Bloom Cloud | FX | 2.98 | 4.39 | 1.90 |
| `fx_metallic` | Metal Shimmer | FX | 1.24 | 2.10 | 0.66 |
| `fx_reverse_hall` | Reverse Hall | FX | 0.49 | 1.41 | 0.62 |

Total: 37 IRs, **18.9 MB** of FLAC.

### Parameters per IR

`Library.cpp` is the authoritative source; every IR is one readable block there. RT targets are listed as
low / mid / high / air in seconds. Room dimensions are in metres.

| id | generator settings |
|---|---|
| room_kick_tight | 4.3×3.7×2.6 room, RT 0.42/0.34/0.26/0.17, β wall/floor/ceil 0.80/0.86/0.62, wall LP 7 kHz, ER/late +1.5 dB, +2 dB low shelf at 110 Hz |
| room_wood | 6.6×5.3×3.3, RT 0.78/0.68/0.52/0.32, wall LP 5.5 kHz, +2 dB at 420 Hz, −2 dB air shelf |
| room_live | 9.4×7.2×4.2, RT 1.15/1.1/0.95/0.6, β 0.9, wall LP 11 kHz, ER/late −1.5 dB, +1.5 dB at 6 kHz |
| room_dead | 4.6×4.0×2.7, RT 0.30/0.22/0.15/0.10, β 0.55/0.78/0.42, ER/late +4 dB, −3 dB above 7 kHz |
| room_air_hat | 7.2×5.6×3.7, RT 0.42/0.6/0.62/0.48, wall LP 15 kHz, 4th-order HP at 280 Hz, +3 dB above 7.5 kHz |
| room_studio_snare | 5.6×4.7×3.0, RT 0.6/0.55/0.47/0.3, ER/late +0.5 dB, +1 dB at 1.8 kHz |
| room_drum_booth | 3.3×2.9×2.5, RT 0.26/0.2/0.16/0.11, ER/late +3 dB |
| room_stone | 8.3×6.1×5.2, RT 1.5/1.45/1.2/0.75, β 0.93, ER/late −3 dB |
| room_garage | 6.1×3.5×2.6 concrete, RT 0.95/0.85/0.68/0.42, β 0.92, +2.5 dB at 330 Hz |
| plate_medium | 2×1 m, 0.5 mm, T = 1500 N/m, RT 1.35/1.6/1.35/0.75, HP 80 Hz (4th order), +1 dB above 6 kHz |
| plate_bright_snare | 1.7×0.95 m, RT 0.9/1.3/1.25/0.9, taper 13–19 kHz, HP 150 Hz, +3.5 dB above 4.5 kHz |
| plate_short | 1.8×0.9 m, RT 0.65/0.8/0.7/0.42 |
| plate_long | 2.1×1.05 m, RT 2.3/2.8/2.25/1.2 (cut at −85 dB) |
| plate_dark | 0.6 mm, RT 1.6/1.8/1.0/0.45, taper 9–14 kHz, −3 dB above 4.5 kHz, +1.5 dB at 250 Hz |
| chamber_rim | 4.7×3.3×2.9 tiled, β 0.95, wide cardioid, RT 0.85/0.9/0.8/0.55, +2 dB at 2.4 kHz |
| chamber_echo | 7.6×5.1×4.1 concrete, β 0.95, RT 1.75/1.8/1.45/0.85, ER/late −6 dB, +1.5 dB at 1.2 kHz |
| chamber_small | 5.2×3.9×3.1, β 0.94, RT 1.2/1.15/0.95/0.6 |
| hall_tom | 21×15×9.5, 32 lines, RT 2.25/1.9/1.45/0.8, ER/late −8 dB, +1.5 dB low shelf |
| hall_large | 34×24×15, 32 lines, RT 3.5/3.2/2.5/1.4, ER/late −10 dB (cut at −85 dB) |
| hall_medium | 18×13×8, 32 lines, RT 2.35/2.2/1.8/1.0, ER/late −7 dB |
| hall_bright | 24×17×11, 32 lines, RT 2.2/2.5/2.3/1.5, +2 dB above 5 kHz |
| spring_perc | 4 springs, 27–34 ms loops, 90–100 stages, a ≈ 0.6–0.65, RT 1.5–1.7 s, band 140 Hz–4.8 kHz |
| spring_twin | 4 springs, 38–52 ms loops, 100–120 stages, RT 2.3–2.7 s, band 110 Hz–4.5 kHz |
| spring_tight | 4 springs, 20–25 ms loops, 78–85 stages, RT 0.9–1.0 s, loop LP 2.6 kHz, band 180 Hz–4.3 kHz |
| vintage_digital_room | FDN 16 lines at 32 kHz, 7–24 ms, RT 0.95/0.9/0.7/0.5, 0.35 ms chorus, 12 early taps, 11 kHz bandwidth |
| vintage_digital_plate | figure-8 tank at 32 kHz, size 0.8, RT 1.8, damping 0.15, excursion 14, 12 kHz bandwidth |
| vintage_digital_hall | FDN at 32 kHz, 22–68 ms, RT 2.8/2.5/1.7/1.0, 0.6 ms chorus, 18 ms pre-delay, 10 kHz bandwidth |
| vintage_ambience | FDN at 32 kHz, 4–13 ms, RT 0.5/0.45/0.36/0.25, 14 early taps, 12.5 kHz bandwidth |
| gated_80s | 10×8×4.5 live room, base RT 2.4, flattened, 0 → −3.5 dB, gate 360 ms, 14 ms release |
| nonlin_reverse | 32-line dense FDN (base RT 3), ramp −26 → 0 dB, gate 320 ms, 8 ms release |
| gated_short | 7×5.5×3.5 room, base RT 2.0, flat, gate 220 ms, 10 ms release |
| gated_big | 20×14×9 hall, base RT 3.0, 0 → −6 dB, gate 500 ms, 18 ms release |
| nonlin_flat | 32-line dense FDN, flat, gate 280 ms, 8 ms release |
| fx_reverse_clap | modal plate (RT 1.3), first 0.85 s reversed, (t/T)^0.6 shaping |
| fx_bloom | 32-line cloud FDN, RT 3.0, 2 in-loop allpasses per line, bloom τ 0.32 s, power 2.2 (cut at −85 dB) |
| fx_metallic | 9 tuned combs × 2 layers (±6 cents), RT 1.5, wobble 4 cents, plus a 16-line bright FDN wash at −7 dB |
| fx_reverse_hall | 22×16×10 hall (RT 2.4), first 1.4 s reversed |

## Verification (`analyze.py`)

For every file the script measures:

* T30 per octave band (125 Hz–8 kHz) and broadband, compared with library.json
* normalised echo density (Abel & Huang, 20 ms window): time to 0.9 and the late median
* EDC linearity: RMS deviation from the −5 to −35 dB fit
* tail flutter: the standard deviation of a 5 ms envelope around a 50 ms envelope
* late-tail correlation for LL/LR and LL/RR
* spectral centroid, and the energy shares below 100 Hz and above 10 kHz
* DC, peak level, clipping, and whether the last sample is exactly zero

Pass thresholds:

* All IRs: no clipping, last sample 0, DC < −60 dB, < 50 % of the energy above 10 kHz.
* Non-spring IRs: late echo density ≥ 0.9.
* Exponential IRs (everything except springs, gated, non-linear and reverse):
  * density reaches 0.9 within 100 ms (150 ms for vintage);
  * EDC RMS < 0.6 dB;
  * flutter < 1 dB;
  * library rt60 within 5 % of the measured value.
* |ρ| < 0.2 (0.35 for springs).

Current results: **all 37 IRs pass**. Typical values:

* Rooms, chambers and halls:
  * density 0.9 reached 14–31 ms after onset (halls 54–79 ms, as in real halls with a sparse early
    section);
  * EDC RMS 0.05–0.3 dB;
  * flutter 0.2–0.8 dB;
  * |ρ| ≤ 0.09;
  * measured T30 at 1 kHz within 1–5 % of the design target.
* Plates: density 0.9 within 4 ms, EDC RMS 0.1–0.35 dB.
* Springs: echo density 0.8–0.9, with deliberate periodic flutter of about 5–6 dB at the echo rate.

## Licensing

Every impulse response in `Resources/irs/` is an original synthetic render produced by this tool
(`tools/ir_forge`) from the algorithms and fixed seeds in this folder. No recorded, measured or third-party
IR material is involved. The published algorithms we implemented are cited only as ideas; no code was
copied. They are:

* Allen & Berkley image method
* Jot / Schlecht FDN design
* Dattorro's 1997 figure-of-eight tank
* Välimäki–Parker–Abel spring model
* Abel & Huang echo density

The IRs are freely redistributable. We suggest releasing the IR files under **CC0 1.0** (public domain
dedication) or, if preferred, under the project's own license. Suggested CREDITS.md line:

> Factory impulse responses: original renders by Freddy Verb's `ir_forge` tool (tools/ir_forge). No
> third-party IR data. Released under CC0 1.0.
