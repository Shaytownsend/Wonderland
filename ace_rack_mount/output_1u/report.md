# ACE 1U bolt-on ears - automatic verification report

**Chassis hole data are placeholders until measured:** holes at Y = (17.5, 17.5), Z = (9.5, 34.5), M4, factory screw 8 mm, depth 8 mm, vents [(38.0, 168.0, 8.0, 38.0)]

| # | Check | Result | Value |
|---|---|---|---|
| 1 | Zero interference: ears vs ACE body (310 x 44 x 210) | PASS | 0.0000 mm3 |
| 2 | Ears vs 315.6 envelope: only the side plate's own footprint on the side panel (bolted there) | PASS | 4117 mm3 = plate footprint; relief pockets entered: 1 |
| 3 | Zero interference: ears vs rack rails | PASS | 0.0000 mm3 |
| 4 | Rack slot centres within 0.05 mm (X 232.55; Z 6.35 / 22.225 / 38.1 from the U edge) | PASS | 3 slots, max deviation 0.0000 mm |
| 5 | Assembly width = 482.6 mm | PASS | 482.600 mm |
| 6 | Panel height = 43.6 mm (1U) | PASS | 43.600 mm |
| 7 | Structure behind the flange <= 222 mm (3 mm inside the rail opening) | PASS | 220.00 mm |
| 8 | Every official vent uncovered (side plate windows + diagonal slots) | PASS | 0.0000 mm3 of plate over vents |
| 9 | Minimum wall >= 2.4 mm (thinnest: under counterbore) | PASS | 3.00 mm |
| 10 | 8 mm material around each chassis hole (5.7) | SPEC-CONFLICT | 6.95 mm between the lower hole and the ear bottom edge (ear bottom fixed at 0.2 mm by 5.1; hole position fixed by the chassis) |
| 11 | ear_R: one watertight solid | PASS | 1 solid(s), valid=True, 40.6 cm3 |
| 12 | ear_R: inside 256^3 build volume as oriented | PASS | 86.3 x 36.0 x 43.6 mm |
| 13 | ear_R: no overhang > 45 deg (bridges <= 25 mm allowed) | PASS | OK, 2 bridge faces |
| 14 | ear_L: one watertight solid | PASS | 1 solid(s), valid=True, 40.6 cm3 |
| 15 | ear_L: inside 256^3 build volume as oriented | PASS | 86.3 x 36.0 x 43.6 mm |
| 16 | ear_L: no overhang > 45 deg (bridges <= 25 mm allowed) | PASS | OK, 2 bridge faces |
| 17 | 2 mm fillets at the diagonal junctions applied | PASS | [True] |
| 18 | Load: Side plate root creep (Case A) | PASS | 1.21 MPa vs 6.8 -> SF 5.59 |
| 19 | Load: Diagonal wall creep (Case A) | PASS | 2.98 MPa vs 6.8 -> SF 2.26 |
| 20 | Load: Flange creep (Case A) | PASS | 0.99 MPa vs 6.8 -> SF 6.79 |
| 21 | Load: Rear sag (Case A) | PASS | 0.77 mm vs 1.0 -> SF 1.29 |
| 22 | Load: Side plate root (Case B) | PASS | 6.04 MPa vs 45.0 -> SF 7.45 |
| 23 | Load: Diagonal wall shear (Case B) | PASS | 1.71 MPa vs 26.0 -> SF 15.22 |
| 24 | Load: Diagonal wall bending (Case B) | PASS | 14.92 MPa vs 45.0 -> SF 3.02 |
| 25 | Load: Flange in-plane (Case B) | PASS | 4.97 MPa vs 45.0 -> SF 9.06 |
| 26 | Load: Flange torsion (10 % share) (Case B) | PASS | 2.75 MPa vs 26.0 -> SF 9.43 |
| 27 | Load: Chassis hole bearing (Case B) | PASS | 13.70 MPa vs 45.0 -> SF 3.28 |
| 28 | Load: Chassis screw shear (Case B) | PASS | 450.89 N vs 4200.0 -> SF 9.31 |
| 29 | Load: Rack screw tension (Case B) | PASS | 421.75 N vs 8000.0 -> SF 18.97 |
| 30 | Load: Flange under rack washer (Case B) | PASS | 2.17 MPa vs 25.0 -> SF 11.50 |
| 31 | Load: Chassis screw tension (side load) (Case C) | PASS | 64.18 N vs 7000.0 -> SF 109.07 |
| 32 | Load: Chassis screw shear (front-back) (Case C) | PASS | 38.26 N vs 4200.0 -> SF 109.78 |
| 33 | Load: Plate bearing / head pull-through (Case C) | PASS | 1.39 MPa vs 45.0 -> SF 32.44 |
| 34 | Load: Chassis retained (Case C) (Case C) | PASS | retained |

## Calculation log

```
Side plate thickness for hole bearing at 5 g: F = 451 N per screw, t -> 7 mm, bearing 13.7 MPa, SF 3.28
Side plate: t = 7 mm, ends at Y = 36.0 (rearmost hole 17.5 + 18.5)
Chassis screws: M4 x 11 mm (factory 8 + plate under the washer 3.0) -> engagement 8.0 mm <= depth 8.0 mm
Design mass 5.2 kg -> W = 51.0 N ; per ear P = 25.51 N (1 g) ; COM 105 mm behind the flange
Diagonal wall: length 63.5 mm, V = 25.5 x 5 x 105 / 36 = 372.0 N (5 g), t -> 5 mm, tau = 1.71 MPa, sigma = 14.92 MPa (in-layer), SF = 3.02

== Sections ==
I_plate = 7 x 43.6^3/12 = 48348 mm4 ; I_flange = 41441 mm4 ; I_diag = 34534 mm4

== Case A: 1 g, P per ear = 25.5 N ==
Chassis screws (pair 25.0 mm apart): couple = 25.5 x (105 - 17.5) / 25.0 = 89.3 N along Y, plus P/2 = 12.8 N vertical -> 90.2 N resultant per screw ; plate bearing 90.2 / (4.7 x 7) = 2.74 MPa
Plate root: M = 25.5 x 105 = 2678 Nmm, sigma = 1.21 MPa (in-layer)
Diagonal: V = 74.4 N, tau = 0.34 MPa, bending sigma = 2.98 MPa (in-layer)
Flange: M_y = 25.5 x 74.1 = 1889 Nmm, sigma = 0.99 MPa ; torsion (10 % share) tau = 0.55 MPa
Rack screws (2 used): pitch 2678 / 31.750 = 84 N tension on the top screw ; roll couple 59 N shear ; vertical 12.8 N
Sag: diagonal tip = V L/(G A) + V L^3/(3 E I) = 0.031 + 0.097 = 0.128 mm -> rotation 0.00357 rad -> 0.749 mm at Y = 210 ; plate bending adds 0.025 mm ; total 0.774 mm

== Case B: 5 g, P per ear = 127.5 N ==
Chassis screws (pair 25.0 mm apart): couple = 127.5 x (105 - 17.5) / 25.0 = 446.4 N along Y, plus P/2 = 63.8 N vertical -> 450.9 N resultant per screw ; plate bearing 450.9 / (4.7 x 7) = 13.70 MPa
Plate root: M = 127.5 x 105 = 13391 Nmm, sigma = 6.04 MPa (in-layer)
Diagonal: V = 372.0 N, tau = 1.71 MPa, bending sigma = 14.92 MPa (in-layer)
Flange: M_y = 127.5 x 74.1 = 9444 Nmm, sigma = 4.97 MPa ; torsion (10 % share) tau = 2.75 MPa
Rack screws (2 used): pitch 13391 / 31.750 = 422 N tension on the top screw ; roll couple 297 N shear ; vertical 63.8 N
Sag: diagonal tip = V L/(G A) + V L^3/(3 E I) = 0.157 + 0.485 = 0.642 mm -> rotation 0.01783 rad -> 3.745 mm at Y = 210 ; plate bending adds 0.126 mm ; total 3.871 mm

== Case C: 3 g, F = 153 N ==
Front-to-back: 38.3 N shear per chassis screw. Side-to-side: 77 N direct + yaw 16069 Nmm / 310 = 52 N -> 64 N tension per chassis screw
```

## Load table

| Feature | Case | Value | Allowable | SF | Pass | Note |
|---|---|---|---|---|---|---|
| Side plate root creep | A | 1.21 MPa | 6.8 MPa | 5.59 | yes | <= 15 % yield [XY] |
| Diagonal wall creep | A | 2.98 MPa | 6.8 MPa | 2.26 | yes | <= 15 % yield [XY] |
| Flange creep | A | 0.99 MPa | 6.8 MPa | 6.79 | yes | <= 15 % yield [XY] |
| Rear sag | A | 0.77 mm | 1.0 mm | 1.29 | yes | elastic; hole clearance slop not included  |
| Side plate root | B | 6.04 MPa | 45.0 MPa | 7.45 | yes | in-layer [XY] |
| Diagonal wall shear | B | 1.71 MPa | 26.0 MPa | 15.22 | yes | in-layer [XY] |
| Diagonal wall bending | B | 14.92 MPa | 45.0 MPa | 3.02 | yes | in-layer [XY] |
| Flange in-plane | B | 4.97 MPa | 45.0 MPa | 9.06 | yes | in-layer [XY] |
| Flange torsion (10 % share) | B | 2.75 MPa | 26.0 MPa | 9.43 | yes | in-layer [XY] |
| Chassis hole bearing | B | 13.70 MPa | 45.0 MPa | 3.28 | yes | rear screw, in-layer [XY] |
| Chassis screw shear | B | 450.89 N | 4200.0 N | 9.31 | yes | M4 8.8 single shear  |
| Rack screw tension | B | 421.75 N | 8000.0 N | 18.97 | yes | 10-32 steel proof  |
| Flange under rack washer | B | 2.17 MPa | 25.0 MPa | 11.50 | yes | compression across layers [Z] |
| Chassis screw tension (side load) | C | 64.18 N | 7000.0 N | 109.07 | yes | M4 proof; thread in the ACE must hold this  |
| Chassis screw shear (front-back) | C | 38.26 N | 4200.0 N | 109.78 | yes |   |
| Plate bearing / head pull-through | C | 1.39 MPa | 45.0 MPa | 32.44 | yes | washer bearing on the counterbore floor, in-layer [XY] |
| Chassis retained (Case C) | C | 1.00  | 1.0  | inf | yes | 4 screws into the chassis; no slip possible  |

## Derived dimensions

- T_SIDE = 7.0
- T_DIAG = 5.0
- Y_PLATE_END = 36.0
- Y_DIAG = 36.0
- CBORE_D = 9.2
- CBORE_DEPTH = 4.0
- SCREW_LEN = 11.0
- HOLE_Z_rack = [6.125, 22.0, 37.875]
- EAR_Z = (0.2, 43.800000000000004)
- mass_g_solid = 52

## Files

- step/ear_R.step
- 3mf/ear_R.3mf (x1.003 shrink, bed chamfer applied)
- step/ear_L.step
- 3mf/ear_L.3mf (x1.003 shrink, bed chamfer applied)
- 3mf/test_fit_template.3mf (2 mm plate, holes + front lip)
- step/assembly_in_rack.step
