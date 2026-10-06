# ACE 1U fabricated metal ears - verification report

| # | Check | Result | Value |
|---|---|---|---|
| 1 | Zero interference: ears vs ACE body (310 x 44 x 210) | PASS | 0.0000 mm3 |
| 2 | Zero interference: ears vs 315.6 envelope incl. the 3 mm front/rear panel lips | PASS | 0.0000 mm3 |
| 3 | Zero interference: ears vs rack rails | PASS | 0.0000 mm3 |
| 4 | Side plate tab lies on the flange rear face without overlap | PASS | 0.0000 mm3 |
| 5 | Rack slot centres within 0.05 mm (X 232.55; Z 6.35 / 22.225 / 38.1 from the U edge) | PASS | 3 slots, max deviation 0.0000 mm |
| 6 | Assembly width = 482.6 mm | PASS | 482.600 mm |
| 7 | Panel height = 43.6 mm (1U) | PASS | 43.600 mm |
| 8 | Structure behind the flange <= 222 mm | PASS | 174.0 mm (tab) |
| 9 | Vent field uncovered (window with 2 mm margin) | PASS | 0.0000 mm3 of plate over the vent |
| 10 | Factory button head (7.6) bears on the plate: front round hole ring 1.40 mm, rear slot end ring 0.30 mm (slot is along Y, head always covers it across) | PASS | ok |
| 11 | Small M3 head (5.2) clears its 7.0 mm hole by 0.90 mm at either end | PASS | ok |
| 12 | Minimum web >= 1.5 t = 3.0 mm for laser-cut steel (thinnest: strip under the M3 notch / hole) | PASS | 3.10 mm |
| 13 | side_plate_R: one valid solid | PASS | 1 solid(s), 9.2 cm3 |
| 14 | flange_R: one valid solid | PASS | 1 solid(s), 20.2 cm3 |
| 15 | side_plate_L: one valid solid | PASS | 1 solid(s), 9.2 cm3 |
| 16 | flange_L: one valid solid | PASS | 1 solid(s), 20.2 cm3 |
| 17 | Load: Rear sag (Case A) | PASS | 0.51 mm vs 1.0 -> SF 1.96 |
| 18 | Load: Side plate frame (window strips) (Case B) | PASS | 36.37 MPa vs 205.0 -> SF 5.64 |
| 19 | Load: Side plate root at the bend (Case B) | PASS | 19.12 MPa vs 205.0 -> SF 10.72 |
| 20 | Load: Chassis hole bearing (Case B) | PASS | 3.32 MPa vs 205.0 -> SF 61.73 |
| 21 | Load: Chassis screw load (Case B) | PASS | 31.88 N vs 4200.0 -> SF 131.73 |
| 22 | Load: Flange torsion (Case B) | PASS | 28.02 MPa vs 140.0 -> SF 5.00 |
| 23 | Load: Flange in-plane bending (Case B) | PASS | 4.43 MPa vs 240.0 -> SF 54.16 |
| 24 | Load: Joint screw tension (M4) (Case B) | PASS | 431.96 N vs 7000.0 -> SF 16.21 |
| 25 | Load: Joint tab bearing (Case B) | PASS | 52.72 MPa vs 205.0 -> SF 3.89 |
| 26 | Load: Rack screw tension (Case B) | PASS | 421.75 N vs 8000.0 -> SF 18.97 |
| 27 | Load: Flange under rack washer (Case B) | PASS | 2.98 MPa vs 240.0 -> SF 80.45 |
| 28 | Load: Chassis screw tension (side load) (Case C) | PASS | 32.09 N vs 7000.0 -> SF 218.15 |
| 29 | Load: Chassis screw shear (front-back) (Case C) | PASS | 19.13 N vs 4200.0 -> SF 219.56 |
| 30 | Load: Chassis retained (Case C) | PASS | retained |

## Calculation log

```
Design mass 5.2 kg, W = 51.0 N, P per ear = 25.51 N (1 g)
Frame through the window: strips 3.8 and 5.8 mm tall x 2.0, I = 6954 mm4 ; solid plate I = 13814 ; flange I = 41441, J = 2867 mm4

== Case A (1 g), P = 25.5 N per ear ==
Pads: COM at 105 vs pair centre 105.0 -> couple 0.0 N ; rear pad 12.8 N, front pad 12.8 N -> 6.4 N per screw ; bearing 0.66 MPa
Frame: M = 1996 Nmm, sigma = 7.3 MPa ; plate root M = 2423 Nmm, sigma = 3.8 MPa
Flange: in-plane M = 1685 Nmm, sigma = 0.9 MPa ; torsion M = 2678 Nmm over 66.1 mm, tau = M/(alpha a b^2) = 5.6 MPa
Joint: 86 N tension on the top M4 (lever 31 mm) + 8.5 N shear each ; rack screws: 84 N tension, 53 N shear (2 used)
Sag: flange twist theta = M L / (G J) = 0.00237 rad -> 0.498 mm at Y = 210 ; frame flexure 0.012 mm ; total 0.510 mm

== Case B (5 g), P = 127.5 N per ear ==
Pads: COM at 105 vs pair centre 105.0 -> couple 0.0 N ; rear pad 63.8 N, front pad 63.8 N -> 31.9 N per screw ; bearing 3.32 MPa
Frame: M = 9979 Nmm, sigma = 36.4 MPa ; plate root M = 12115 Nmm, sigma = 19.1 MPa
Flange: in-plane M = 8423 Nmm, sigma = 4.4 MPa ; torsion M = 13391 Nmm over 66.1 mm, tau = M/(alpha a b^2) = 28.0 MPa
Joint: 432 N tension on the top M4 (lever 31 mm) + 42.5 N shear each ; rack screws: 422 N tension, 265 N shear (2 used)
Sag: flange twist theta = M L / (G J) = 0.01186 rad -> 2.492 mm at Y = 210 ; frame flexure 0.061 mm ; total 2.552 mm

== Case C: 3 g, F = 153 N ==
Front-to-back: 19.1 N shear per chassis screw ; side-to-side: 32 N tension per chassis screw (direct + yaw couple)
```

## Load table

| Feature | Case | Value | Allowable | SF | Pass | Note |
|---|---|---|---|---|---|---|
| Rear sag | A | 0.51 mm | 1.0 mm | 1.96 | yes | flange twist + frame flexure (metal: no creep limit) |
| Side plate frame (window strips) | B | 36.37 MPa | 205.0 MPa | 5.64 | yes | bending at the window front edge |
| Side plate root at the bend | B | 19.12 MPa | 205.0 MPa | 10.72 | yes | in-plane bending |
| Chassis hole bearing | B | 3.32 MPa | 205.0 MPa | 61.73 | yes | per factory screw |
| Chassis screw load | B | 31.88 N | 4200.0 N | 131.73 | yes | M4 8.8 single shear |
| Flange torsion | B | 28.02 MPa | 140.0 MPa | 5.00 | yes | 6 mm 6061-T6 |
| Flange in-plane bending | B | 4.43 MPa | 240.0 MPa | 54.16 | yes |  |
| Joint screw tension (M4) | B | 431.96 N | 7000.0 N | 16.21 | yes | class 8.8 proof |
| Joint tab bearing | B | 52.72 MPa | 205.0 MPa | 3.89 | yes |  |
| Rack screw tension | B | 421.75 N | 8000.0 N | 18.97 | yes | 10-32 steel proof |
| Flange under rack washer | B | 2.98 MPa | 240.0 MPa | 80.45 | yes |  |
| Chassis screw tension (side load) | C | 32.09 N | 7000.0 N | 218.15 | yes | M4 proof; the ACE thread sees this |
| Chassis screw shear (front-back) | C | 19.13 N | 4200.0 N | 219.56 | yes |  |
| Chassis retained | C | 1.00  | 1.0  | inf | yes | 8 screws into the chassis |

## Flat pattern

- straight plate 196.0 + bend allowance 4.52 (R2.0, K 0.44) + tab 15.0 = flat 215.52 mm, bend line at 198.26 from the rear end
- masses: side_plate_R 73 g, flange_R 55 g, side_plate_L 73 g, flange_L 55 g

## Files

- step/side_plate_R.step
- step/flange_R.step
- step/side_plate_L.step
- step/flange_L.step
- step/assembly_in_rack.step
- dxf/side_plate_flat_R.dxf (mirror for L)
- dxf/flange.dxf (same part both sides)
- drawings/side_plate_flat.png
- drawings/side_plate_1to1_A4.pdf
