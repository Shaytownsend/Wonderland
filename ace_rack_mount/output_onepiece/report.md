# ACE 1U one-piece printed ears - verification report

Chassis data from photo IMG_8471 (+/- 0.5 mm): screws at Y (17.5, 192.5), Z (9.5, 34.5); vent zone (37.8, 172.2, 7.9, 38.1); M3 spots [(32.6, 6.8), (177.4, 6.8)].

| # | Check | Result | Value |
|---|---|---|---|
| 1 | Zero interference: ears vs ACE body (310 x 44 x 210) | PASS | 0.0000 mm3 |
| 2 | Zero interference: ears vs ACE envelope incl. the 3 mm front/rear panel lips (315.6 wide) | PASS | 0.0000 mm3 |
| 3 | Zero interference: ears vs rack rails | PASS | 0.0000 mm3 |
| 4 | Rack slot centres within 0.05 mm (X 232.55; Z 6.35 / 22.225 / 38.1 from the U edge) | PASS | 3 slots, max deviation 0.0000 mm |
| 5 | Assembly width = 482.6 mm | PASS | 482.600 mm |
| 6 | Panel height = 43.6 mm (1U) | PASS | 43.600 mm |
| 7 | Structure behind the flange <= 222 mm (3 mm inside the 450 mm rail opening) | PASS | 222.00 mm |
| 8 | Every vent uncovered (side plate open over the vent field, both orientations) | PASS | 0.0000 mm3 of plate over the vent |
| 9 | Air column in front of the vent clear for 50 mm (nothing within 50 mm of the vent) | PASS | 0.0000 mm3 ; outer web at 61 mm, 2345 mm2 of diamond openings |
| 10 | Minimum wall >= 2.4 mm (thinnest: plastic under the screw heads) | PASS | 2.40 mm |
| 11 | Fillets: 6 mm at flange-plate and flange-web, 2 mm at bulkheads and floor | PASS | 10 coves |
| 12 | ear_R: one watertight solid | PASS | 1 solid(s), valid=True, 134.4 cm3 |
| 13 | ear_R: inside 256^3 build volume as oriented | PASS | 86.3 x 206.0 x 43.6 mm |
| 14 | ear_R: no overhang > 45 deg as printed (bridges <= 10 mm allowed) | PASS | OK, 0 short bridge faces (max 0.0 mm) |
| 15 | ear_L: one watertight solid | PASS | 1 solid(s), valid=True, 134.4 cm3 |
| 16 | ear_L: inside 256^3 build volume as oriented | PASS | 86.3 x 206.0 x 43.6 mm |
| 17 | ear_L: no overhang > 45 deg as printed (bridges <= 10 mm allowed) | PASS | OK, 0 short bridge faces (max 0.0 mm) |
| 18 | Load: Front screw bearing (creep) (Case A) | PASS | 3.43 MPa vs 6.75 -> SF 1.97 |
| 19 | Load: Ear bending, max (creep) (Case A) | PASS | 0.67 MPa vs 6.75 -> SF 10.04 |
| 20 | Load: Flange plate bending (creep) (Case A) | PASS | 2.68 MPa vs 6.75 -> SF 2.52 |
| 21 | Load: Chassis rear sag (Case A) | PASS | 0.47 mm vs 1.00 -> SF 2.12 |
| 22 | Load: Front screw bearing (pocket + floor) (Case B) | PASS | 12.18 MPa vs 45.00 -> SF 3.70 |
| 23 | Load: Rear screw bearing (Case B) | PASS | 0.35 MPa vs 45.00 -> SF 127.01 |
| 24 | Load: Ear root at the flange (Case B) | PASS | 1.87 MPa vs 45.00 -> SF 24.13 |
| 25 | Load: Ear bending, max along the length (Case B) | PASS | 3.36 MPa vs 45.00 -> SF 13.39 |
| 26 | Load: Lattice chords (Case B) | PASS | 1.47 MPa vs 45.00 -> SF 30.52 |
| 27 | Load: Lattice diagonals (Case B) | PASS | 0.73 MPa vs 25.00 -> SF 34.12 |
| 28 | Load: Flange plate bending at the rack screw (Case B) | PASS | 13.42 MPa vs 45.00 -> SF 3.35 |
| 29 | Load: Flange in-plane (roll) (Case B) | PASS | 3.00 MPa vs 45.00 -> SF 15.00 |
| 30 | Load: Rack screw tension (Case B) | PASS | 359.77 N vs 8000.00 -> SF 22.24 |
| 31 | Load: Flange under the rack washer (Case B) | PASS | 1.85 MPa vs 25.00 -> SF 13.48 |
| 32 | Load: Screw head pull-through (side load) (Case C) | PASS | 1.21 MPa vs 45.00 -> SF 37.06 |
| 33 | Load: Floor punching shear (side load) (Case C) | PASS | 0.67 MPa vs 25.96 -> SF 38.89 |
| 34 | Load: Front screw bearing (fore-aft) (Case C) | PASS | 1.45 MPa vs 45.00 -> SF 30.96 |
| 35 | Load: Factory screw tension (side load) (Case C) | PASS | 38.26 N vs 7000.00 -> SF 182.96 |
| 36 | Load: Chassis retained (Case C) | PASS | retained |

## Calculation log

```
Flange plate bending (web -> rack screw): M_root(5 g) = 127.5 x (105 - 10) = 12115 Nmm ; screw force 12115 / 33.7 = 360 N ; plate M = 360 x 13.55 = 4875 Nmm on b = 21.8 mm ; t -> 10 mm, sigma = 6M/(b t^2) = 13.4 MPa, SF 3.35 (in-layer)

== Model ==
Design mass 5.2 kg, W = 51.0 N, P per ear = 25.51 N at 1 g, COM 105 mm behind the front face
Screw bearing stiffness k = E x (2.4 floor + 2.2 head) = 8740 N/mm per screw ; front pair rotational k = 2 k (25.0/2)^2 = 2.73e+06 Nmm/rad
Root rotation (flange plate bending web -> rack screw, bending + shear): k = 1.63e+06 Nmm/rad
Outer web lattice: 4 diamonds 30 x 30 + 3 triangles, shear stiffness GA = 2 E A_strut sin^2 45 cos 45 = 24183 N ; open area 2345 mm2
Rear pair: worst-case vertical play 0.2 mm before it takes load

== Case A (1 g): P = 25.5 N per ear ==
Rear pair takes load above P = 35.3 N ; front pair: vertical 25.5 N, pitch moment 2232 Nmm -> 90 N per screw (couple 2232 / 25.0, vertical 12.8) ; rear pair 0.0 N per screw
Screw bearing: 90 N / (4.0 x 2.4 + 7.6 x 2.2 = 26.3 mm2) = 3.43 MPa
Ear bending: root M = 2423 Nmm, sigma = M c / I = 0.37 MPa ; max along the ear 0.67 MPa at Y = 16 ; lattice chords 0.00 MPa
Lattice diagonals: V = 0.0 N -> N = V / (2 sin 45) = 0.0 N, sigma = 0.00 MPa (inclined to the layers)
Flange: rack screw tension 2423 / 33.7 = 72 N ; plate bending 2.68 MPa ; in-plane (roll) M = 1901 Nmm, sigma = 0.60 MPa
Chassis rear sag (Y = 210): 0.472 mm (includes the rear play 0.2 mm while open)

== Case B (5 g): P = 127.5 N per ear ==
Rear pair takes load above P = 35.3 N ; front pair: vertical 108.9 N, pitch moment 7895 Nmm -> 320 N per screw (couple 7895 / 25.0, vertical 54.4) ; rear pair 9.3 N per screw
Screw bearing: 320 N / (4.0 x 2.4 + 7.6 x 2.2 = 26.3 mm2) = 12.18 MPa
Ear bending: root M = 12115 Nmm, sigma = M c / I = 1.87 MPa ; max along the ear 3.36 MPa at Y = 16 ; lattice chords 1.47 MPa
Lattice diagonals: V = 18.7 N -> N = V / (2 sin 45) = 13.2 N, sigma = 0.73 MPa (inclined to the layers)
Flange: rack screw tension 12115 / 33.7 = 360 N ; plate bending 13.42 MPa ; in-plane (roll) M = 9507 Nmm, sigma = 3.00 MPa
Chassis rear sag (Y = 210): 2.131 mm (includes the rear play 0.2 mm while open)

== Case C: 3 g, F = 153 N ==
Side load: one ear's 4 screws in tension, 38 N each ; head on the 2.4 mm floor 1.21 MPa ; punching 0.67 MPa
Fore-aft: 38 N per front screw (rear pair slotted along Y), bearing 1.45 MPa
Lateral bending of each ear (box 61 mm wide): M = 8034 Nmm
```

## Load table

| Feature | Case | Value | Allowable | SF | Pass | Note |
|---|---|---|---|---|---|---|
| Front screw bearing (creep) | A | 3.43 MPa | 6.75 MPa | 1.97 | yes | 90 N per screw |
| Ear bending, max (creep) | A | 0.67 MPa | 6.75 MPa | 10.04 | yes | at Y = 16 |
| Flange plate bending (creep) | A | 2.68 MPa | 6.75 MPa | 2.52 | yes | web -> rack screw |
| Chassis rear sag | A | 0.47 mm | 1.00 mm | 2.12 | yes | beam model incl. screw bearing, flange flex, rear hole play |
| Front screw bearing (pocket + floor) | B | 12.18 MPa | 45.00 MPa | 3.70 | yes | 320 N per screw, in-layer |
| Rear screw bearing | B | 0.35 MPa | 45.00 MPa | 127.01 | yes | 9 N per screw |
| Ear root at the flange | B | 1.87 MPa | 45.00 MPa | 24.13 | yes | in-layer |
| Ear bending, max along the length | B | 3.36 MPa | 45.00 MPa | 13.39 | yes | Y = 16, in-layer |
| Lattice chords | B | 1.47 MPa | 45.00 MPa | 30.52 | yes | in-layer |
| Lattice diagonals | B | 0.73 MPa | 25.00 MPa | 34.12 | yes | 45 deg to the layers -> Z strength |
| Flange plate bending at the rack screw | B | 13.42 MPa | 45.00 MPa | 3.35 | yes | sized: t = 10 mm |
| Flange in-plane (roll) | B | 3.00 MPa | 45.00 MPa | 15.00 | yes |  |
| Rack screw tension | B | 359.77 N | 8000.00 N | 22.24 | yes | 10-32 steel proof (weakest) |
| Flange under the rack washer | B | 1.85 MPa | 25.00 MPa | 13.48 | yes | compression across layers |
| Screw head pull-through (side load) | C | 1.21 MPa | 45.00 MPa | 37.06 | yes |  |
| Floor punching shear (side load) | C | 0.67 MPa | 25.96 MPa | 38.89 | yes |  |
| Front screw bearing (fore-aft) | C | 1.45 MPa | 45.00 MPa | 30.96 | yes |  |
| Factory screw tension (side load) | C | 38.26 N | 7000.00 N | 182.96 | yes | M4 proof; the ACE thread sees this |
| Chassis retained | C | 1.00  | 1.00  | inf | yes | 8 screws into the chassis |

## Derived dimensions

- T_FLANGE = 10.0
- T_SIDE = 6
- FLOOR_UNDER_HEAD = 2.4
- POCKET_D = 7.8
- HOLE_FIT_D = 4.2
- X_WEB = (216.0, 222.0)
- NOTCH = (35.8, 174.2, 5.9)
- Y_FBULK = (30.0, 34.0)
- Y_RBULK = (202.0, 206.0)
- DIAMONDS = [52.7, 87.0, 121.2, 155.5]
- RACK_SLOT_Z = [6.125, 22.0, 37.875]
- mass_g_solid = 171

## Files

- step/ear_R.step (assembly position)
- 3mf/ear_R.3mf (x1.003 shrink, bed chamfer applied)
- step/ear_L.step (assembly position)
- 3mf/ear_L.3mf (x1.003 shrink, bed chamfer applied)
- 3mf/fit_template.3mf (x1.003 shrink, bed chamfer applied)
- step/assembly_in_rack.step
- renders/assembly_{iso,front,top,side}.png
- renders/ear_R_{iso,front,top,side,outside,inside,as_printed}.png
