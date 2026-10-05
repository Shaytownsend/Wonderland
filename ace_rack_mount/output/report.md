# ACE 2U cradle - automatic verification report

| # | Check | Result | Value |
|---|---|---|---|
| 1 | Zero interference: cradle bodies, ribs, clamps, tie bars vs ACE envelope (315.6 x 44 x 239) | PASS | 0.0000 mm3 |
| 2 | Rear stop vs ACE body (310 x 44 x 210): zero interference, 0.5 mm gap behind the rear panel | PASS | 0.0000 mm3 ; stop occupies the rear protrusion envelope at X 147..155 only (spec 6.4) |
| 3 | Zero interference: parts vs rack rails (flange face touches rail, volume 0) | PASS | 0.0000 mm3 |
| 4 | Zero interference: clamps and tie bars vs cradle side | PASS | 0.0000 mm3 |
| 5 | Rack slot centres within 0.05 mm of EIA-310-D (X=232.55; Z=6.350,38.100,50.800,82.550 + 0.4 below panel edge) | PASS | 4 slots, max deviation 0.0000 mm |
| 6 | Assembly width = 482.6 mm | PASS | 482.600 mm |
| 7 | Panel height = 88.1 mm (2U) | PASS | 88.100 mm |
| 8 | Nothing within 50 mm of the ACE side panels except ribs and rear stops | PASS | 0.0000 mm3 inside band |
| 9 | Structure behind the flange stays 3 mm inside the 450 mm rail opening (X <= 222) | PASS | 220.00 mm |
| 10 | Minimum wall >= 2.4 mm (thinnest: insert roof (shelf)) | PASS | 2.50 mm |
| 11 | cradle_side_R: one watertight solid | PASS | 1 solid(s), valid=True, 559.9 cm3 |
| 12 | cradle_side_R: inside 256^3 build volume as oriented | PASS | 146.3 x 220.5 x 88.1 mm |
| 13 | cradle_side_R: no overhang > 45 deg (bridges <= 25 mm allowed) | PASS | OK, 212 bridge faces |
| 14 | cradle_side_L: one watertight solid | PASS | 1 solid(s), valid=True, 559.9 cm3 |
| 15 | cradle_side_L: inside 256^3 build volume as oriented | PASS | 146.3 x 220.5 x 88.1 mm |
| 16 | cradle_side_L: no overhang > 45 deg (bridges <= 25 mm allowed) | PASS | OK, 212 bridge faces |
| 17 | clamp_R_40: one watertight solid | PASS | 1 solid(s), valid=True, 16.0 cm3 |
| 18 | clamp_R_40: inside 256^3 build volume as oriented | PASS | 80.0 x 26.6 x 20.0 mm |
| 19 | clamp_R_40: no overhang > 45 deg (bridges <= 25 mm allowed) | PASS | OK, 0 bridge faces |
| 20 | clamp_L_40: one watertight solid | PASS | 1 solid(s), valid=True, 16.0 cm3 |
| 21 | clamp_L_40: inside 256^3 build volume as oriented | PASS | 80.0 x 26.6 x 20.0 mm |
| 22 | clamp_L_40: no overhang > 45 deg (bridges <= 25 mm allowed) | PASS | OK, 0 bridge faces |
| 23 | clamp_R_170: one watertight solid | PASS | 1 solid(s), valid=True, 16.0 cm3 |
| 24 | clamp_R_170: inside 256^3 build volume as oriented | PASS | 80.0 x 26.6 x 20.0 mm |
| 25 | clamp_R_170: no overhang > 45 deg (bridges <= 25 mm allowed) | PASS | OK, 0 bridge faces |
| 26 | clamp_L_170: one watertight solid | PASS | 1 solid(s), valid=True, 16.0 cm3 |
| 27 | clamp_L_170: inside 256^3 build volume as oriented | PASS | 80.0 x 26.6 x 20.0 mm |
| 28 | clamp_L_170: no overhang > 45 deg (bridges <= 25 mm allowed) | PASS | OK, 0 bridge faces |
| 29 | Load: Shelf root creep (Case A) | PASS | 0.28 MPa vs 6.8 -> SF 23.98 |
| 30 | Load: Wall root creep (Case A) | PASS | 0.41 MPa vs 6.8 -> SF 16.30 |
| 31 | Load: Flange creep (Case A) | PASS | 0.35 MPa vs 6.8 -> SF 19.10 |
| 32 | Load: Rear sag (Case A) | PASS | 0.10 mm vs 1.0 -> SF 10.12 |
| 33 | Load: Clamp arm preload creep (Case A) | PASS | 6.59 MPa vs 6.8 -> SF 1.02 |
| 34 | Load: Shelf root (bending) (Case B) | PASS | 1.41 MPa vs 45.0 -> SF 31.98 |
| 35 | Load: Wall root at flange (Case B) | PASS | 2.07 MPa vs 45.0 -> SF 21.74 |
| 36 | Load: Flange in-plane (Case B) | PASS | 1.77 MPa vs 45.0 -> SF 25.46 |
| 37 | Load: Flange slot bearing (Case B) | PASS | 6.46 MPa vs 45.0 -> SF 6.96 |
| 38 | Load: Clamp arm root (Case B) | PASS | 10.51 MPa vs 45.0 -> SF 4.28 |
| 39 | Load: Clamp web root (Case B) | PASS | 10.51 MPa vs 45.0 -> SF 4.28 |
| 40 | Load: Clamp insert pull-out (Case B) | PASS | 176.45 N vs 600.0 -> SF 3.40 |
| 41 | Load: Clamp foot under head (Case B) | PASS | 6.30 MPa vs 25.0 -> SF 3.97 |
| 42 | Load: Rack screw tension (Case B) | PASS | 150.63 N vs 8000.0 -> SF 53.11 |
| 43 | Load: Rear stop root (Case C) | PASS | 6.44 MPa vs 25.0 -> SF 3.88 |
| 44 | Load: Front lip root (Case C) | PASS | 1.70 MPa vs 25.0 -> SF 14.70 |
| 45 | Load: Locating rib root (Case C) | PASS | 4.30 MPa vs 25.0 -> SF 5.81 |
| 46 | Load: Tie-bar screw shear (Case C) | PASS | 17.27 N vs 2000.0 -> SF 115.78 |
| 47 | Load: Chassis retained (Case C) (Case C) | PASS | retained |

## Load calculation log

```
Shelf thickness: spec 6 mm; tie-bar recess 6.0 + insert hole 6.5 + roof 2.4 = 14.9 -> T_SHELF = 15 mm
Top rail: protrusion = floor(82.1 - 6.5 - 61.0) = 14 mm -> rail 19 wide, full section from Z = 75.0
Design mass 5.2 kg -> W = 51.0 N ; per side P = 25.51 N (1 g)
Rear stop: F = 76.5 N/tab, lever 5.50 mm, M = 421 Nmm, t -> 7 mm, I = 228.7 mm4, sigma = 6.44 MPa (across layers), SF = 3.88
Clamp arm: F = 63.8 N, lever 44.5 mm, M = 2838 Nmm, t -> 9 mm, sigma = 10.51 MPa (in-layer), SF = 4.28 ; preload creep sigma = 6.59 MPa (<= 6.75)
Clamp screws: pivot lever 13.1 mm, T_total = 353 N, 176 N per insert, pull-out SF = 3.40 (600 N design)

== Section geometry ==
I_shelf  = 218 x 15^3 / 12 = 61172 mm4
I_wall   = 5 x 88.1^3 / 12 = 284916 mm4 (plain wall, top rail ignored = conservative)
I_flange (in-plane) = 6 x 88.1^3 / 12 = 341899 mm4
I_rib = 107 mm4, I_lip = 135 mm4, I_stop = 228.7 mm4, I_arm = 1215 mm4

== Case A: 1 g vertical, P per side = 25.5 N ==
Shelf root: M = 25.5 x 90 = 2296 Nmm ; sigma = M c / I = 2296 x 8 / 61172 = 0.28 MPa (in-layer)
Wall root: M = 25.5 x 105 = 2678 Nmm ; sigma = 2678 x 44.05 / 284916 = 0.41 MPa (in-layer)
Flange (roll moment about Y): M = 25.5 x 107.6 = 2743 Nmm ; sigma = 0.35 MPa (in-layer)
Rack screws: pitch moment 2678 Nmm / lever 44.45 mm = 60 N on the top pair -> 30 N tension per screw
             roll moment 2743 Nmm / 44.45 = 62 N X-shear couple -> 31 N per screw ; vertical shear 6.4 N per screw
             slot bearing (4.8 mm shank in 6 mm PETG) = 1.29 MPa
Sag: shelf P L^3/(3EI) = 25.5 x 90^3 / (3 x 1900 x 61172) = 0.053 mm ; wall tip (load at 105, tip at 210) = 0.045 mm ; total 0.099 mm
Clamp preload (pad <= 40 N): arm sigma = 6.59 MPa ; insert tension = 111 N per screw

== Case B: 5 g vertical, P per side = 127.5 N ==
Shelf root: M = 127.5 x 90 = 11478 Nmm ; sigma = M c / I = 11478 x 8 / 61172 = 1.41 MPa (in-layer)
Wall root: M = 127.5 x 105 = 13391 Nmm ; sigma = 13391 x 44.05 / 284916 = 2.07 MPa (in-layer)
Flange (roll moment about Y): M = 127.5 x 107.6 = 13716 Nmm ; sigma = 1.77 MPa (in-layer)
Rack screws: pitch moment 13391 Nmm / lever 44.45 mm = 301 N on the top pair -> 151 N tension per screw
             roll moment 13716 Nmm / 44.45 = 309 N X-shear couple -> 154 N per screw ; vertical shear 31.9 N per screw
             slot bearing (4.8 mm shank in 6 mm PETG) = 6.46 MPa
Sag: shelf P L^3/(3EI) = 127.5 x 90^3 / (3 x 1900 x 61172) = 0.267 mm ; wall tip (load at 105, tip at 210) = 0.227 mm ; total 0.494 mm

== Case C: 3 g front-to-back and 3 g side-to-side, F = 153 N ==
Rear stop (already sized): sigma = 6.44 MPa across layers, SF = 3.88
Front lip: F = 76.5 N/lip, lever 2.0 mm, M = 153 Nmm, sigma = 1.70 MPa across layers
Locating rib: F = 76.5 N/rib, lever 3.0 mm, M = 230 Nmm, sigma = 4.30 MPa across layers, shear 0.96 MPa
Side load yaw moment 16069 Nmm: with tie bars -> couple between flanges 35 N ; without tie bars a single flange would need 1836 N screw tension (lever 8.75 mm)
Rack screw shear sideways = 153 / 8 = 19.1 N ; rearwards = 19.1 N
Chassis retained: down by shelves, up by 4 clamps (gap 1.5 mm pad, hard stop), sideways by ribs (0.4 mm clearance), forward by lips (2 mm engagement), rearward by stops (9 mm engagement)
```

## Load table

| Feature | Case | Value | Allowable | SF | Pass | Note |
|---|---|---|---|---|---|---|
| Shelf root creep | A | 0.28 MPa | 6.8 MPa | 23.98 | yes | <= 15 % of 45 MPa [XY] |
| Wall root creep | A | 0.41 MPa | 6.8 MPa | 16.30 | yes | <= 15 % of 45 MPa [XY] |
| Flange creep | A | 0.35 MPa | 6.8 MPa | 19.10 | yes | <= 15 % of 45 MPa [XY] |
| Rear sag | A | 0.10 mm | 1.0 mm | 10.12 | yes | shelf + wall  |
| Clamp arm preload creep | A | 6.59 MPa | 6.8 MPa | 1.02 | yes | pad force 40 N max [XY] |
| Shelf root (bending) | B | 1.41 MPa | 45.0 MPa | 31.98 | yes | in-layer [XY] |
| Wall root at flange | B | 2.07 MPa | 45.0 MPa | 21.74 | yes | in-layer [XY] |
| Flange in-plane | B | 1.77 MPa | 45.0 MPa | 25.46 | yes | in-layer [XY] |
| Flange slot bearing | B | 6.46 MPa | 45.0 MPa | 6.96 | yes | in-layer [XY] |
| Clamp arm root | B | 10.51 MPa | 45.0 MPa | 4.28 | yes | in-layer (printed on side) [XY] |
| Clamp web root | B | 10.51 MPa | 45.0 MPa | 4.28 | yes | in-layer [XY] |
| Clamp insert pull-out | B | 176.45 N | 600.0 N | 3.40 | yes | per M4 insert  |
| Clamp foot under head | B | 6.30 MPa | 25.0 MPa | 3.97 | yes | compression across layers [Z] |
| Rack screw tension | B | 150.63 N | 8000.0 N | 53.11 | yes | 10-32 steel (weakest option) proof load  |
| Rear stop root | C | 6.44 MPa | 25.0 MPa | 3.88 | yes | 3 g rearwards, 2 tabs [Z] |
| Front lip root | C | 1.70 MPa | 25.0 MPa | 14.70 | yes | 3 g forwards, 2 lips [Z] |
| Locating rib root | C | 4.30 MPa | 25.0 MPa | 5.81 | yes | 3 g sideways, 2 ribs [Z] |
| Tie-bar screw shear | C | 17.27 N | 2000.0 N | 115.78 | yes | M4 8.8 single shear ~ 2 kN (conservative)  |
| Chassis retained (Case C) | C | 1.00  | 1.0  | inf | yes | positive stops in all 6 directions  |

## Derived dimensions

- T_SHELF = 15
- STOP_T = 7.0
- CLAMP_ARM_T = 9.0
- SHELF_DEPTH = 217.5
- Z_SEAT = 16.0
- Z_TOP = 60.0
- Z_RAIL_CH0 = 61.0
- Z_RAIL_FULL = 75.0
- Z_NOTCH = 82.1
- CLAMP_SCREW_X = 206.9
- X_FLANGE_IN = 158.20000000000002
- FLANGE_WIDTH = 83.1
- X_WALL_IN = 215.0
- X_WALL_OUT = 220.0
- X_SHELF_IN = 95.0
- FLANGE_SLOT_Z = [5.95, 37.7, 50.4, 82.15]

## Printed part masses (PETG 1.27 g/cm3, solid volume = upper bound)

- cradle_side_R: 711 g (solid)
- cradle_side_L: 711 g (solid)
- clamp_R_40: 20 g (solid)
- clamp_L_40: 20 g (solid)
- clamp_R_170: 20 g (solid)
- clamp_L_170: 20 g (solid)

## Files

- step/cradle_side_R.step
- 3mf/cradle_side_R.3mf (x1.003 shrink, bed chamfer applied)
- step/cradle_side_L.step
- 3mf/cradle_side_L.3mf (x1.003 shrink, bed chamfer applied)
- step/clamp_R_40.step
- 3mf/clamp_R_40.3mf (x1.003 shrink, bed chamfer applied)
- step/clamp_L_40.step
- 3mf/clamp_L_40.3mf (x1.003 shrink, bed chamfer applied)
- step/pad_R_40.step
- step/pad_L_40.step
- step/clamp_R_170.step
- 3mf/clamp_R_170.3mf (x1.003 shrink, bed chamfer applied)
- step/clamp_L_170.step
- 3mf/clamp_L_170.3mf (x1.003 shrink, bed chamfer applied)
- step/pad_R_170.step
- step/pad_L_170.step
- step/tie_bar_20.step
- step/tie_bar_195.step
- step/assembly_in_rack.step
