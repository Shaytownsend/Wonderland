# DirectOut ACE – 2U printed rack cradle (PATH B)

Parametric CadQuery design, automatic verification, load calculations and print files for mounting one
DirectOut ACE in an EIA-310-D 19" rack using front rails only.

Run: `pip install cadquery && python3 ace_cradle.py` (about 15 s). The script rebuilds every part, runs all
47 checks, exports STEP/3MF/renders into `output/` and writes `output/report.md` (full calculation log).

---

## 1. Path chosen and why

**PATH B – 2U cradle. No chassis holes are used.**

Section 4 requires an official DirectOut dimensioned drawing of the ACE side mounting holes (position,
thread, depth) and the side-vent locations before PATH A may be built. The research found none:

- The DirectOut ACE product page, the ACE documents page (`directout.eu/download/documents-ace/`), the
  user manual page (`directout.eu/download/manual-ace/`, file `manual_ace_e.pdf`) and the ACE microsite
  are blocked by this session's network egress policy, so the manual could not be opened here. The
  search-engine excerpts of those pages list only the overall dimensions already given in section 1
  (310 x 44 x 210 mm, 1 RU) and no hole data.
- The Rack Ears Set DOSET1056 is listed only as a retail item (Synthax UK, "rack mounting kit for ACE",
  £72) with no drawing, screw specification or installation guide on any reachable page.
- The manual data the brief itself quotes (v1.1, Appendix C) gives chassis body, envelope and mass only.

Because the data is "not found or incomplete", section 4 mandates PATH B. The design touches the ACE only
on its bottom face (neoprene seat), the bottom edge of the front panel (2 mm lip), the outer 8 mm of the rear
panel (stops), the corner edges (ribs, 0.4 mm clearance) and 15 x 20 mm pads on its top face (clamps). The
side panels face 60 mm of open air over their full height and depth.

Sources searched: directout.eu/product/ace, directout.eu/download/documents-ace, directout.eu/download/manual-ace,
ace.directout.eu, synthax.co.uk (DOSET1056 listing), sweetwater.com ACE listing, manualslib.com (DirectOut
PRODIGY.MP only, not ACE).

## Deviations from the section 6 text, all forced by sections 2, 3, 7 or 9

| Item | Spec | Built | Why |
|---|---|---|---|
| Shelf thickness | 6 mm | **15 mm** | 6.7 recesses a 6 mm tie bar flush into the shelf underside and needs an M4 heat-set insert above it: 6.0 recess + 6.5 insert hole + 2.4 roof (section 9 min wall) = 14.9 → 15. Chassis bottom therefore sits 16 mm (not 7 mm) above the panel bottom; everything stays inside 88.1 mm. |
| Diagonal web | 4 mm web across the 60 mm gap | 45° knee gusset from X = 205 to the wall | Section 9 forbids any structure within 50 mm of the side panel; a web starting at the chassis edge would sit 0–45 mm from the vents. The gusset starts exactly at the 50 mm line. Stiffness lost is recovered by the 15 mm shelf (sag 0.10 mm vs 1.0 mm limit). |
| Flange width | 86.3 mm | 83.1 mm (X 158.2 … 241.3) | The flange inner edge must clear the 315.6 mm protrusion envelope + 0.4 mm (section 9 interference check). Rack-side edge and slot positions are unchanged. |
| Rear stop thickness | 5 mm | **7 mm** | Section 7 loop: Case C 3 g rearwards gives SF 1.98 at 5 mm (across layers); 7 mm gives SF 3.88. Shelf depth follows: 217.5 mm. |
| Clamp arm / web | 6 mm bar | **9 mm** | Section 7 loop: 5 g upward needs 8 mm for SF ≥ 3; Case A creep (≤ 15 % yield under 40 N pad preload) needs 9 mm. |
| Front lip | at the shelf front edge | 3 mm proud of the flange plane | Flange front face is coplanar with the ACE front face (6.5), so the lip must be in front of that plane. Bottom 2 mm of the front panel only, outer 60 mm per side. |
| Clamp mounting | inserts in the 5 mm wall top | 19 mm wide top rail on the wall (Z 61 … 88.1, 45° underside) | A 5.8 mm insert hole cannot live in a 5 mm wall. The rail is above the chassis top (Z 60) so it stays out of the vent band, and it stiffens the wall. Clamp feet sit in 6 mm notches so the clamp top is flush with 88.1 mm. |
| Tie bars | "flat bars 20 x 6" | aluminium EN AW-6060 flat bar 20 x 6 x 400 | Not printed: a 400 mm printed bar would need a lap joint and would be far less stiff. STEP exported; no 3MF. |

## 2. Verification table (from `output/report.md`, all 47 PASS)

| # | Check | Result | Value |
|---|---|---|---|
| 1 | Zero interference: cradle bodies, ribs, clamps, tie bars vs ACE envelope 315.6 x 44 x 239 | PASS | 0.0000 mm³ |
| 2 | Rear stop vs ACE body 310 x 44 x 210, 0.5 mm gap behind the rear panel | PASS | 0.0000 mm³ (stop sits in the rear protrusion envelope at X 147…155 only, per 6.4 – confirm no connector there) |
| 3 | Zero interference: parts vs rack rails (flange face touches the rail) | PASS | 0.0000 mm³ |
| 4 | Zero interference: clamps and tie bars vs cradle side | PASS | 0.0000 mm³ |
| 5 | Rack slot centres vs EIA-310-D (X = 232.55; Z = 6.35, 38.1, 50.8, 82.55 from the U edge, i.e. 5.95, 37.7, 50.4, 82.15 from the panel bottom) | PASS | 4 slots, max deviation 0.0000 mm |
| 6 | Assembly width = 482.6 mm | PASS | 482.600 mm |
| 7 | Panel height = 88.1 mm | PASS | 88.100 mm |
| 8 | Nothing within 50 mm of the side panels except ribs and rear stops (band X 155…205, Y 6…210, Z 16…60) | PASS | 0.0000 mm³ in band |
| 9 | Structure behind the flange ≤ 222 mm (3 mm inside the 450 mm opening) | PASS | 220.00 mm |
| 10 | Minimum wall ≥ 2.4 mm (thinnest = insert roof in shelf) | PASS | 2.50 mm |
| 11–13 | cradle_side_R: one watertight solid / inside 256³ / no overhang > 45° | PASS | 1 solid, 559.9 cm³ / 146.3 x 220.5 x 88.1 mm / OK (bridges ≤ 25 mm: tie channel roof 20.4, blind-hole roofs 5.8) |
| 14–16 | cradle_side_L: same | PASS | same (mirror) |
| 17–28 | clamp_R_40, clamp_L_40, clamp_R_170, clamp_L_170: one solid / build volume / overhang | PASS | 16.0 cm³ / 80.0 x 26.6 x 20.0 mm / 0 bridges, teardrop holes |
| 29 | Case A shelf root ≤ 15 % yield | PASS | 0.28 MPa vs 6.75 |
| 30 | Case A wall root ≤ 15 % yield | PASS | 0.41 MPa vs 6.75 |
| 31 | Case A flange ≤ 15 % yield | PASS | 0.35 MPa vs 6.75 |
| 32 | Case A rear sag ≤ 1.0 mm | PASS | 0.10 mm |
| 33 | Case A clamp arm preload creep ≤ 15 % yield | PASS | 6.59 MPa vs 6.75 |
| 34 | Case B shelf root SF ≥ 3 | PASS | 1.41 MPa, SF 32.0 |
| 35 | Case B wall root SF ≥ 3 | PASS | 2.07 MPa, SF 21.7 |
| 36 | Case B flange in-plane SF ≥ 3 | PASS | 1.77 MPa, SF 25.5 |
| 37 | Case B flange slot bearing SF ≥ 3 | PASS | 6.46 MPa, SF 7.0 |
| 38 | Case B clamp arm root SF ≥ 3 | PASS | 10.51 MPa, SF 4.28 |
| 39 | Case B clamp web root SF ≥ 3 | PASS | 10.51 MPa, SF 4.28 |
| 40 | Case B clamp insert pull-out SF ≥ 3 (600 N design value) | PASS | 176 N, SF 3.40 |
| 41 | Case B clamp foot under screw head (Z strength) SF ≥ 3 | PASS | 6.30 MPa, SF 3.97 |
| 42 | Case B rack screw tension vs 10-32 proof load | PASS | 151 N, SF 53 |
| 43 | Case C rear stop root (Z strength) SF ≥ 3 | PASS | 6.44 MPa, SF 3.88 |
| 44 | Case C front lip root (Z strength) SF ≥ 3 | PASS | 1.70 MPa, SF 14.7 |
| 45 | Case C locating rib root (Z strength) SF ≥ 3 | PASS | 4.30 MPa, SF 5.81 |
| 46 | Case C tie-bar screw shear | PASS | 17 N, SF 116 |
| 47 | Case C chassis cannot leave the mount | PASS | positive stops in all six directions |

## 3. Load calculation summary (all formulas and numbers; full log in `output/report.md`)

**Inputs.** m = 3.7 + 1.5 = 5.2 kg; W = 5.2 x 9.81 = 51.0 N; per side P = 25.5 N (1 g). Centre of mass
105 mm behind the flange, 125 mm from the rack centre line of each seat strip (centroid of the 60 mm seat,
X 95…155). E = 1900 MPa, σy,XY = 45 MPa, σy,Z = 25 MPa.

**Sections.** I = b h³/12:
I_shelf = 217.5 x 15³/12 = 61 172 mm⁴; I_wall = 5 x 88.1³/12 = 284 916 mm⁴ (top rail ignored, conservative);
I_flange,in-plane = 6 x 88.1³/12 = 341 899 mm⁴; I_rib = 20 x 4³/12 = 107 mm⁴; I_lip = 60 x 3³/12 = 135 mm⁴;
I_stop = 8 x 7³/12 = 229 mm⁴; I_arm = 20 x 9³/12 = 1215 mm⁴.

**Case A, 1 g (creep limit 0.15 x 45 = 6.75 MPa; sag ≤ 1.0 mm).**
- Shelf root at the wall (cantilever, lever 215 − 125 = 90 mm): M = 25.5 x 90 = 2296 N·mm; σ = M c/I = 2296 x 7.5 / 61 172 = 0.28 MPa (in-layer).
- Wall root at the flange: M = 25.5 x 105 = 2678 N·mm; σ = 2678 x 44.05 / 284 916 = 0.41 MPa (in-layer).
- Flange, roll moment about Y: M = 25.5 x (232.55 − 125) = 2743 N·mm; σ = 2743 x 44.05 / 341 899 = 0.35 MPa.
- Sag at the chassis rear: shelf δ = P L³/(3EI) = 25.5 x 90³ / (3 x 1900 x 61 172) = 0.053 mm; wall tip
  δ = P a²(3L − a)/(6EI) with a = 105, L = 210 = 25.5 x 105² x 525 / (6 x 1900 x 284 916) = 0.045 mm; **total 0.10 mm**.
- Clamp arm under 40 N pad preload: σ = 40 x 44.5 x 4.5 / 1215 = 6.59 MPa ≤ 6.75 (this is why the arm is 9 mm).

**Case B, 5 g vertical (SF ≥ 3 on yield; Z strength wherever load crosses layers).** All 1 g values x 5:
- Shelf root 1.41 MPa → SF 32; wall root 2.07 MPa → SF 21.7; flange 1.77 MPa → SF 25.5 (all in-layer, cradle printed upright).
- Rack screws per side (4 screws, Z = 5.95 / 37.7 / 50.4 / 82.15): pitch moment 127.5 x 105 = 13 391 N·mm over the
  44.45 mm pair-to-pair lever = 301 N on the top pair → **151 N tension per screw**; roll moment 13 716 N·mm → 154 N
  X-shear per screw; vertical shear 32 N per screw. Slot bearing (4.8 mm shank in 6 mm PETG) 6.46 MPa → SF 7.0.
- Clamps, 5 g upward on 4 clamps: F = 255/4 = 63.8 N at the pad centroid (X 147.5). Arm lever to the web = 44.5 mm,
  M = 2838 N·mm, σ = 2838 x 4.5 / 1215 = 10.5 MPa → SF 4.28 (in-layer, clamp printed on its side).
  Foot pivots on its outer edge (X 220), screws at X 206.9: T = 63.8 x 72.5 / 13.1 = 353 N on two inserts →
  **176 N per M4 insert**, SF 3.40 on a 600 N pull-out design value (assumed for a 5.7 mm M4 insert in 100 % PETG;
  confirm with one pull test). Foot under the button head: 6.3 MPa across layers → SF 4.0.

**Case C, 3 g (F = 153 N), chassis must stay in the mount.**
- Rearwards → 2 rear stops: 76.5 N each at 5.5 mm above the shelf: M = 421 N·mm; σ = 421 x 3.5 / 229 = 6.44 MPa across layers → SF 3.88 (5 mm tab failed at SF 1.98; thickened to 7 mm).
- Forwards → 2 front lips: 76.5 N at 2.0 mm: M = 153 N·mm; σ = 153 x 1.5 / 135 = 1.70 MPa → SF 14.7.
- Sideways → 2 ribs on one side: 76.5 N at 3.0 mm: M = 230 N·mm; σ = 230 x 2 / 107 = 4.30 MPa → SF 5.8; shear 0.96 MPa.
- Yaw moment 153 x 105 = 16 069 N·mm is carried as a Y-couple between the two flanges through the tie bars:
  16 069 / 465.1 = 35 N (17 N per tie-bar screw). Without tie bars one flange alone would need 1836 N of screw tension
  on an 8.75 mm lever, which is why the tie bars are mandatory.
- Rack screw shear sideways or rearwards: 153/8 = 19 N per screw.
- Retention: down = shelves; up = 4 clamps (1.5 mm compressed pad, then hard stop); sideways = ribs (0.4 mm play);
  forwards = lips (2 mm engagement); rearwards = stops (9 mm engagement above the seat).

## 4. Files

| File | Content |
|---|---|
| `ace_cradle.py` | the parametric CadQuery script (build, loads, verification, export, renders, report) |
| `output/report.md`, `output/verification.json` | the 47-row verification table, calculation log, derived dimensions |
| `output/step/cradle_side_R.step`, `cradle_side_L.step` | cradle sides, assembly frame (nominal) |
| `output/step/clamp_{R,L}_{40,170}.step` | four top clamps, assembly frame |
| `output/step/*_print_oriented.step` | same parts in print orientation with the 0.3 mm bed chamfer |
| `output/step/tie_bar_20.step`, `tie_bar_195.step` | aluminium tie bars with countersunk holes |
| `output/step/assembly_in_rack.step` | everything plus the ACE body, neoprene seats and both rack rails with the EIA hole pattern |
| `output/3mf/*.3mf` | the six printed parts, print-oriented, bed chamfer applied, **scaled x 1.003** (do not scale again in the slicer) |
| `output/renders/assembly_{front,top,side,iso}.{svg,png}` | full assembly in the rack |
| `output/renders/cradle_side_R_*`, `clamp_R_40_*`, `tie_bar_20_*` | per-part front / top / side / iso (left parts are mirrors) |
| `output/renders/clamp_print_orientation.png` | the clamp as it sits on the bed |

## 5. Hardware list

| Item | Spec | Qty | Where |
|---|---|---|---|
| Rack screws | 10-32 x 5/8" or M6 x 16, pan/truss head, steel | 8 | 4 per flange (top and bottom hole of each U) |
| Rack washers | M6 DIN 9021 (Ø 18) or #10 fender washer, steel | 8 | under every rack screw; the wide washer covers the teardrop apex |
| Heat-set inserts | M4 x 5.7 long, Ø 6.3 brass (Ruthex RX-M4x5.7 type), hole printed Ø 5.8 x 6.5 deep | 16 | 8 in the wall top rails (clamps), 8 in the shelf undersides (tie bars) |
| Clamp screws | M4 x 10 ISO 7380 button head, steel 8.8 | 8 | 2 per clamp, counterbored Ø 8.2 x 2.5 so the head is flush with 88.1 mm |
| Tie-bar screws | M4 x 12 DIN 7991 countersunk, steel 8.8 | 8 | 2 per bar end, from below, flush |
| Tie bars | aluminium EN AW-6060 flat bar 20 x 6 x 400 mm, 4 holes Ø 4.5 countersunk 90° at X = ±120 and ±180 | 2 | under both shelves at Y = 20 and Y = 195, flush in the 20.4 x 6 channels |
| Seat strips | solid neoprene 60 Shore A, 1.0 x 60 x 210 mm, adhesive-backed | 2 | on each shelf, X 95…155, Y 0…210 |
| Clamp pads | closed-cell sponge neoprene, 2.0 x 15 x 20 mm, adhesive-backed, ≤ 40 N at 0.5 mm compression | 4 | under each clamp arm tip |

Screw-length check: clamp 10 mm = 3.5 mm foot (under a 2.5 mm counterbore) + 6.5 mm into the 6.5 mm insert hole;
tie bar 12 mm = 6 mm bar + 6 mm into the 6.5 mm hole with 2.5 mm of printed roof above. Nothing can reach the chassis.

## 6. Slicer settings and print orientation

Common: PETG-ESD, 0.4 nozzle, 0.2 mm layers, 6 walls, 5 top / 5 bottom layers, 40 % gyroid, **100 % infill modifier
within 10 mm of every hole and every root** (flange–wall corner, shelf–wall corner, rib/stop/lip roots, clamp foot and
web corners, both top rails), no supports, brim not required (large flat contact), part-cooling low for PETG.
The 3MF files already include the 0.3 % shrink scale and the 0.3 mm bed-edge chamfer: import at 100 %.

| Part | Qty | Orientation on the bed | Footprint x height | Notes |
|---|---|---|---|---|
| cradle_side_R / _L | 1 + 1 | **as installed: rack vertical = printer Z**, shelf underside on the bed | 146.3 x 220.5 x 88.1 mm (fits flat, no diagonal) | Only overhang is the 45° rail chamfer and the 45° knee gusset; tie channels (20.4 mm) and blind insert holes (5.8 mm) print as bridges. Rack slots are teardrops. Chosen over "rack width = printer Z" because it puts the shelf-root and wall-root bending stresses in-layer (SF 32 and 22) and keeps every face ≤ 45°; a width-up print would put the shelf root across layers (25 MPa) and need support under the shelf. |
| clamp_R_40, clamp_L_40, clamp_R_170, clamp_L_170 | 4 | **bar width (rack depth axis) = printer Z**, lying on a 20 mm side face | 80.0 x 26.6 x 20.0 mm | Constant profile in every layer, zero overhangs; the arm bending stress is in-layer. Screw holes and counterbores are teardrops pointing up. |
| tie bars | 2 | not printed (aluminium) | – | cut 400 mm, drill 4 x Ø 4.5, countersink 90° x Ø 9 on the bottom face |

## 7. Assembly steps

1. Print both cradle sides and the four clamps from the 3MF files. Deburr the slots and channels; do not scale.
2. Heat-set eight M4 inserts into the top rails (two per clamp notch at X 206.9, Y = 40 ± 6 and 170 ± 6) and eight into the shelf undersides (channels at Y = 20 and 195, X = 120 and 180 per side). Insert flush, let cool under the iron.
3. Cut the two aluminium tie bars to 400 mm, drill and countersink the four holes each (X = ±120, ±180 from the bar centre).
4. Stick the 1 mm neoprene seat strip onto each shelf between the two locating ribs, flush with the front lip. Stick a 2 mm sponge pad under each clamp arm tip (the 15 x 20 mm area over the chassis).
5. Lay both cradle sides on their outer walls, shelves facing each other, 482.6 mm flange edge to flange edge. Drop the tie bars into the shelf channels (Y = 20 and 195) and fit the eight countersunk M4 x 12 screws from below; torque to 1.5 N·m. The frame is now one rigid unit.
6. Stand the frame upright on a bench. Lower the ACE onto the seat strips from above: front panel against the two front lips, rear panel in front of the two rear stops (0.5 mm gap), the 315.6 mm envelope between the four locating ribs (0.4 mm per side).
7. Fit the four clamps: foot into its notch in the top rail, arm over the chassis top, pad touching the lid 15 mm in from the edge. Fit the M4 x 10 button-head screws and tighten to 1.0 N·m; the pad compresses 0.5 mm and the clamp top is flush with the 88.1 mm panel height.
8. Check that the side vents see open air (nothing within 50 mm of either side panel between the shelf and the chassis top) and that no rear connector sits in the outer 8 mm covered by the rear stops.
9. Offer the frame to the rack at a 2U opening with the flange front face against the rails. Fit eight rack screws with the wide washers through the teardrop slots (both holes of each U, top and bottom) and tighten to 3 N·m for 10-32 or 5 N·m for M6.
10. Connect cables. The cradle is rated for 5 g vertical shock and 3 g in both horizontal directions with the stated 1.5 kg of cables and SFP modules.

---

# 1U bolt-on ears (PATH A) – `ace_ears_1u.py`

Two one-piece ears (left = mirror of right) that bolt to the two existing screw positions on each side of the
ACE and carry it on the front rails alone. Output in `output_1u/` (STEP, 3MF, renders, `report.md`).

## Chassis hole data (from your photo IMG_8471, rectified on the 210 x 44 side panel)

| Feature | Measured | Used in the script |
|---|---|---|
| Corner screws, each end | 17.6 / 18.0 mm from the left end, 16.0 / 17.3 mm from the right end; Z 10.4 / 35.3 (left), 8.5 / 33.9 (right) | symmetric pattern: **Y = 17.5 from the end, Z = 9.5 and 34.5** |
| Screw head | 7.1 - 7.7 mm button head, Torx | **M4** (ISO 7380 head = 7.6 mm) |
| Vent field (hex perforation) | 37.8 - 168.6 mm from the left end, Z 7.9 - 38.1 | `VENTS = [(38, 168, 8, 38)]` |
| Small screw, lower front-left | 32.6 mm from the left end, Z 6.8, 5.2 mm head (M3) | relief pocket `PROTRUSIONS = [(29.5, 36, 3.7, 10)]` |

Accuracy of a rectified photo is about ±0.5 mm, so **print `output_1u/3mf/test_fit_template.3mf` first** (2 mm plate
with the two holes and a lip that hooks over the front-panel face, prints in minutes). If the holes are off, shift
`CH_HOLE_Y` / `CH_HOLE_Z` by the amount you see and re-run. Still to measure: the factory screw length
(`CH_SCREW_LEN_ORIG`, placeholder 8 mm) and which end of the unit is the front (the pattern is symmetric, so only
the M3 relief pocket depends on it).

## Design (section 5, as sized)

- **Flange** 6 mm, front face coplanar with the ACE front face, X 155 ... 241.3, Z 0.2 ... 43.8 (43.6 tall). Three
  teardrop rack slots 10.7 x 7.2 at X = 232.55, Z = 6.125 / 22.0 / 37.875 (= 6.35 / 22.225 / 38.1 from the U edge).
  Loads use only the top and bottom screw.
- **Side plate 7 mm** (auto-thickened from 5 for hole bearing), on the chassis side, Y 0 ... 36: at least 10 mm past
  the holes (5.3) and extended to 2 mm short of the vent so the torsion triangle is as large as the solid panel allows.
  Teardrop clearance holes Ø 4.7 with flat-bottom counterbores Ø 9.2 x 4 mm (button head + washer fully recessed,
  3 mm of plate under the washer).
- **Torsion box**: 5 mm diagonal (auto-thickened from 4) from the flange rear face (outer face at 220, inside the 222
  limit) to the plate end at Y = 36, closing the triangle. Ø 12 teardrop access holes in line with both chassis screws.
- **Fillets** 6 mm at the flange-plate corner, 2 mm at the diagonal junctions. **Vents** untouched: the plate stops
  2 mm before the perforation. **Relief** 3 mm pocket over the M3 screw head (plate 4 mm there).
- **Screws**: M4 x (factory length + 3), so engagement never exceeds the factory depth. Placeholder: M4 x 11 button head + Ø 9 washer.

## Verification (34 checks: 33 PASS, 1 spec conflict reported)

| Check | Result |
|---|---|
| Ears vs ACE body 310 x 44 x 210 | 0 mm³ |
| Ears vs 315.6 envelope | only the plate's own footprint (it bolts there); M3 head relieved |
| Ears vs rack rails | 0 mm³ |
| Rack slot centres | 3 slots, 0.0000 mm deviation |
| Width / panel height | 482.600 / 43.600 mm |
| Structure behind the flange | 220.0 mm ≤ 222 |
| Every vent uncovered | 0 mm³ of plate over the vent field |
| Minimum wall | 3.00 mm (under the counterbore) |
| 8 mm around each chassis hole (5.7) | **6.95 mm** below the lower hole: the ear bottom is fixed at 0.2 mm (5.1) and the hole at Z 9.5 by the chassis, so 8 mm is geometrically impossible; still 2.9 x the minimum wall |
| Each ear | one valid solid, 40.6 cm³ (≈ 52 g solid), 86.3 x 36.0 x 43.6 mm, no face past 45° |

## Loads (vertical screw pair 25 mm apart at Y = 17.5)

P per ear = 25.5 N at 1 g. The pitch moment is a Y-direction couple over the 25 mm screw spacing:
25.5 x (105 − 17.5) / 25 = 89 N per screw at 1 g, **446 N at 5 g** (451 N resultant with the weight share).
Plate bearing 451 / (4.7 x 7) = 13.7 MPa, SF 3.28 in-layer (this sized the plate to 7 mm). M4 8.8 single shear SF 9.3.
Diagonal wall at 5 g: V = 127.5 x 105 / 36 = 372 N over a 63.5 mm span, bending 14.9 MPa, SF 3.02 (sized to 5 mm).
Flange in-plane 5.0 MPa (SF 8.9); flange torsion share 2.75 MPa (SF 9.4); rack screws 422 N tension / 301 N shear at 5 g.
Case A sag at the chassis rear: 0.75 mm from diagonal flexure + 0.03 mm plate = **0.77 mm ≤ 1.0 mm** (elastic; take up the
0.2 mm hole clearance by lifting the rear while tightening). Case C: 64 N tension / 38 N shear per chassis screw.
The factory screws go into the chassis sheet or inserts; 451 N shear at 5 g is well within an M4 thread in steel or aluminium.

## Hardware (1U ears)

| Item | Qty |
|---|---|
| M4 x (factory + 3 mm) ISO 7380 button head Torx, steel 8.8 (placeholder M4 x 11) | 4 |
| Steel flat washer M4 Ø 9 | 4 |
| Rack screws 10-32 x 5/8" or M6 x 16 with Ø 18 washers | 4 (6 if all three slots are used) |

## Print

Template first: flat on the bed, 2 mm, no settings of note. Ears: **upright, rack vertical = printer Z**, flange bottom
edge on the bed, 86.3 x 36.0 footprint, 43.6 tall; all holes are teardrops, no overhangs, no supports. Same slicer
settings as the cradle, 100 % infill around the chassis holes, rack slots and the flange-plate corner. 3MF files are
scaled x 1.003 with the 0.3 mm bed chamfer.

## Assembly

1. Print the template, hook its lip over the front-panel edge, check both holes line up with the screws; correct and re-run if not.
2. Take one factory screw out, measure its length, set `CH_SCREW_LEN_ORIG`, re-run, print both ears.
3. Remove the two front screws on one side, offer the ear flush to the front panel, fit M4 x (factory + 3) with washers through the access holes in the diagonal, torque 1.2 N·m. Repeat on the other side.
4. Lift the rear of the ACE slightly while tightening so the hole clearance is taken up upward.
5. Fit to the rack with at least the top and bottom slot of each ear; torque 3 N·m (10-32) or 5 N·m (M6).
