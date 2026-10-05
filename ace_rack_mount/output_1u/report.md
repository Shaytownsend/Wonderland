# ACE 1U bolt-on ears - automatic verification report

**Chassis hole data are placeholders until measured:** holes at Y = (20.0, 60.0), Z = (22.0, 22.0), M4, factory screw 8 mm, depth 8 mm, vents NOT ENTERED

| # | Check | Result | Value |
|---|---|---|---|
| 1 | Zero interference: ears vs ACE body (310 x 44 x 210) | PASS | 0.0000 mm3 |
| 2 | Ears vs 315.6 envelope: only the side plate's own footprint on the side panel (bolted there) | PASS | 8485 mm3 = plate footprint; relief pockets entered: 0 |
| 3 | Zero interference: ears vs rack rails | PASS | 0.0000 mm3 |
| 4 | Rack slot centres within 0.05 mm (X 232.55; Z 6.35 / 22.225 / 38.1 from the U edge) | PASS | 3 slots, max deviation 0.0000 mm |
| 5 | Assembly width = 482.6 mm | PASS | 482.600 mm |
| 6 | Panel height = 43.6 mm (1U) | PASS | 43.600 mm |
| 7 | Structure behind the flange <= 222 mm (3 mm inside the rail opening) | PASS | 220.00 mm |
| 8 | Every official vent uncovered (side plate windows + diagonal slots) | UNVERIFIED | NO VENT DATA ENTERED - measure the side panel and fill VENTS, then re-run |
| 9 | Minimum wall >= 2.4 mm (thinnest: under counterbore) | PASS | 3.00 mm |
| 10 | ear_R: one watertight solid | PASS | 1 solid(s), valid=True, 48.9 cm3 |
| 11 | ear_R: inside 256^3 build volume as oriented | PASS | 86.3 x 70.3 x 43.6 mm |
| 12 | ear_R: no overhang > 45 deg (bridges <= 25 mm allowed) | PASS | OK, 0 bridge faces |
| 13 | ear_L: one watertight solid | PASS | 1 solid(s), valid=True, 48.9 cm3 |
| 14 | ear_L: inside 256^3 build volume as oriented | PASS | 86.3 x 70.3 x 43.6 mm |
| 15 | ear_L: no overhang > 45 deg (bridges <= 25 mm allowed) | PASS | OK, 0 bridge faces |
| 16 | 2 mm fillets at the diagonal junctions applied | PASS | [True] |
| 17 | Load: Side plate root creep (Case A) | PASS | 1.69 MPa vs 6.8 -> SF 3.99 |
| 18 | Load: Diagonal wall creep (Case A) | PASS | 2.60 MPa vs 6.8 -> SF 2.59 |
| 19 | Load: Flange creep (Case A) | PASS | 1.01 MPa vs 6.8 -> SF 6.70 |
| 20 | Load: Rear sag (Case A) | PASS | 0.69 mm vs 1.0 -> SF 1.46 |
| 21 | Load: Side plate root (Case B) | PASS | 8.45 MPa vs 45.0 -> SF 5.32 |
| 22 | Load: Diagonal wall shear (Case B) | PASS | 1.09 MPa vs 26.0 -> SF 23.79 |
| 23 | Load: Diagonal wall bending (Case B) | PASS | 13.01 MPa vs 45.0 -> SF 3.46 |
| 24 | Load: Flange in-plane (Case B) | PASS | 5.03 MPa vs 45.0 -> SF 8.94 |
| 25 | Load: Flange torsion (10 % share) (Case B) | PASS | 2.75 MPa vs 26.0 -> SF 9.43 |
| 26 | Load: Chassis hole bearing (Case B) | PASS | 11.53 MPa vs 45.0 -> SF 3.90 |
| 27 | Load: Chassis screw shear (Case B) | PASS | 271.00 N vs 4200.0 -> SF 15.50 |
| 28 | Load: Rack screw tension (Case B) | PASS | 421.75 N vs 8000.0 -> SF 18.97 |
| 29 | Load: Flange under rack washer (Case B) | PASS | 2.17 MPa vs 25.0 -> SF 11.50 |
| 30 | Load: Chassis screw tension (side load) (Case C) | PASS | 64.18 N vs 7000.0 -> SF 109.07 |
| 31 | Load: Chassis screw shear (front-back) (Case C) | PASS | 38.26 N vs 4200.0 -> SF 109.78 |
| 32 | Load: Plate bearing / head pull-through (Case C) | PASS | 1.39 MPa vs 45.0 -> SF 32.44 |
| 33 | Load: Chassis retained (Case C) (Case C) | PASS | retained |

## Calculation log

```
Side plate: t = 5 mm, ends at Y = 70.3 (rearmost hole 60.0 + 10.3)
Chassis screws: M4 x 11 mm (factory 8 + plate under the washer 3.0) -> engagement 8.0 mm <= depth 8.0 mm
Design mass 5.2 kg -> W = 51.0 N ; per ear P = 25.51 N (1 g) ; COM 105 mm behind the flange
Diagonal wall: length 86.6 mm, V = 25.5 x 5 x 105 / 70 = 190.3 N (5 g), t -> 4 mm, tau = 1.09 MPa, sigma = 13.01 MPa (in-layer), SF = 3.46

== Sections ==
I_plate = 5 x 43.6^3/12 = 34534 mm4 ; I_flange = 41441 mm4 ; I_diag = 27627 mm4

== Case A: 1 g, P per ear = 25.5 N ==
Chassis screws: couple = 25.5 x (105 - 40) / 40 = 41.4 N ; rear screw shear 54.2 N, front 28.7 N ; plate bearing 54.2 / (4.7 x 5) = 2.31 MPa
Plate root: M = 25.5 x 105 = 2678 Nmm, sigma = 1.69 MPa (in-layer)
Diagonal: V = 38.1 N, tau = 0.22 MPa, bending sigma = 2.60 MPa (in-layer)
Flange: M_y = 25.5 x 75.1 = 1914 Nmm, sigma = 1.01 MPa ; torsion (10 % share) tau = 0.55 MPa
Rack screws (2 used): pitch 2678 / 31.750 = 84 N tension on the top screw ; roll couple 60 N shear ; vertical 12.8 N
Sag: diagonal tip = V L/(G A) + V L^3/(3 E I) = 0.027 + 0.157 = 0.185 mm -> rotation 0.00262 rad -> 0.551 mm at Y = 210 ; plate bending adds 0.135 mm ; total 0.686 mm

== Case B: 5 g, P per ear = 127.5 N ==
Chassis screws: couple = 127.5 x (105 - 40) / 40 = 207.2 N ; rear screw shear 271.0 N, front 143.5 N ; plate bearing 271.0 / (4.7 x 5) = 11.53 MPa
Plate root: M = 127.5 x 105 = 13391 Nmm, sigma = 8.45 MPa (in-layer)
Diagonal: V = 190.3 N, tau = 1.09 MPa, bending sigma = 13.01 MPa (in-layer)
Flange: M_y = 127.5 x 75.1 = 9571 Nmm, sigma = 5.03 MPa ; torsion (10 % share) tau = 2.75 MPa
Rack screws (2 used): pitch 13391 / 31.750 = 422 N tension on the top screw ; roll couple 301 N shear ; vertical 63.8 N
Sag: diagonal tip = V L/(G A) + V L^3/(3 E I) = 0.137 + 0.786 = 0.923 mm -> rotation 0.01312 rad -> 2.756 mm at Y = 210 ; plate bending adds 0.673 mm ; total 3.429 mm

== Case C: 3 g, F = 153 N ==
Front-to-back: 38.3 N shear per chassis screw. Side-to-side: 77 N direct + yaw 16069 Nmm / 310 = 52 N -> 64 N tension per chassis screw
```

## Load table

| Feature | Case | Value | Allowable | SF | Pass | Note |
|---|---|---|---|---|---|---|
| Side plate root creep | A | 1.69 MPa | 6.8 MPa | 3.99 | yes | <= 15 % yield [XY] |
| Diagonal wall creep | A | 2.60 MPa | 6.8 MPa | 2.59 | yes | <= 15 % yield [XY] |
| Flange creep | A | 1.01 MPa | 6.8 MPa | 6.70 | yes | <= 15 % yield [XY] |
| Rear sag | A | 0.69 mm | 1.0 mm | 1.46 | yes | elastic; hole clearance slop not included  |
| Side plate root | B | 8.45 MPa | 45.0 MPa | 5.32 | yes | in-layer [XY] |
| Diagonal wall shear | B | 1.09 MPa | 26.0 MPa | 23.79 | yes | in-layer [XY] |
| Diagonal wall bending | B | 13.01 MPa | 45.0 MPa | 3.46 | yes | in-layer [XY] |
| Flange in-plane | B | 5.03 MPa | 45.0 MPa | 8.94 | yes | in-layer [XY] |
| Flange torsion (10 % share) | B | 2.75 MPa | 26.0 MPa | 9.43 | yes | in-layer [XY] |
| Chassis hole bearing | B | 11.53 MPa | 45.0 MPa | 3.90 | yes | rear screw, in-layer [XY] |
| Chassis screw shear | B | 271.00 N | 4200.0 N | 15.50 | yes | M4 8.8 single shear  |
| Rack screw tension | B | 421.75 N | 8000.0 N | 18.97 | yes | 10-32 steel proof  |
| Flange under rack washer | B | 2.17 MPa | 25.0 MPa | 11.50 | yes | compression across layers [Z] |
| Chassis screw tension (side load) | C | 64.18 N | 7000.0 N | 109.07 | yes | M4 proof; thread in the ACE must hold this  |
| Chassis screw shear (front-back) | C | 38.26 N | 4200.0 N | 109.78 | yes |   |
| Plate bearing / head pull-through | C | 1.39 MPa | 45.0 MPa | 32.44 | yes | washer bearing on the counterbore floor, in-layer [XY] |
| Chassis retained (Case C) | C | 1.00  | 1.0  | inf | yes | 4 screws into the chassis; no slip possible  |

## Derived dimensions

- T_SIDE = 5.0
- T_DIAG = 4.0
- Y_PLATE_END = 70.35
- Y_DIAG = 70.35
- CBORE_D = 9.2
- CBORE_DEPTH = 2.0
- SCREW_LEN = 11.0
- HOLE_Z_rack = [6.125, 22.0, 37.875]
- EAR_Z = (0.2, 43.800000000000004)
- mass_g_solid = 62

## Files

- step/ear_R.step
- 3mf/ear_R.3mf (x1.003 shrink, bed chamfer applied)
- step/ear_L.step
- 3mf/ear_L.3mf (x1.003 shrink, bed chamfer applied)
- step/assembly_in_rack.step
