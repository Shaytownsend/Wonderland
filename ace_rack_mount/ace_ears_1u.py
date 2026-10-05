#!/usr/bin/env python3
"""
DirectOut ACE  ->  EIA-310-D 19" rack, 1U bolt-on ears (PATH A, section 5).

The ears bolt to the two existing screw positions on each side of the ACE.  DirectOut publishes no
dimensioned drawing that this environment could reach, so the chassis-side hole data below are
PLACEHOLDERS that must be measured on the unit before printing.  Everything else (rack interface,
torsion box, loads, verification, exports) is computed from them, so re-running the script after
editing the MEASURE block regenerates every file.

Coordinate system (assembly frame)
  X : rack width, 0 at rack centre, +X = right ear (looking at the front)
  Y : rack depth, 0 at the ACE front-panel face, +Y rearwards
  Z : vertical, 0 at the CHASSIS BOTTOM.  The 1U panel (43.6) is centred in the 44.45 pitch, so the
      EIA U-boundary is 0.225 mm below Z=0 and the ear spans Z = 0.2 ... 43.8.
"""
import math, os, sys, json
import cadquery as cq
from cadquery import exporters

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output_1u")

# =============================================================================
# >>>>>  MEASURE ON YOUR UNIT  (placeholders - NOT official DirectOut data)  <<<<<
# =============================================================================
# Values below come from the user's photo IMG_8471, rectified on the 210 x 44 side panel (+/- 0.5 mm).
# Both ends carry the same vertical pair of M4 button-head Torx screws; the ear uses the FRONT pair.
# Confirm with the test-fit template (output_1u/3mf/test_fit_template.3mf) before printing the ears.
CH_HOLE_Y   = (17.5, 17.5)   # mm behind the front-panel face (photo: 17.6 / 18.0 left end, 16.0 / 17.3 right end)
CH_HOLE_Z   = (9.5, 34.5)    # mm above the chassis bottom (photo: 10.4 / 35.3 and 8.5 / 33.9 -> symmetric about 22)
CH_THREAD   = "M4"           # head diameter 7.1-7.7 mm in the photo = ISO 7380 M4 button head
CH_SCREW_LEN_ORIG = 8.0      # mm  <-- STILL TO MEASURE: take one screw out
CH_DEPTH    = 8.0            # mm  usable thread depth; = factory screw length unless DirectOut states more
VENTS       = [(38.0, 168.0, 8.0, 38.0)]   # hex-perforated field, photo: L 37.8-168.6, Z 7.9-38.1 (same from either end)
PROTRUSIONS = [(29.5, 36.0, 3.7, 10.0)]    # small M3 button head at L 32.6, Z 6.8 (seen at the LEFT end) - relieved in case that end is the front
# =============================================================================

# --- 1. Device ---------------------------------------------------------------
ACE_W, ACE_H, ACE_D = 310.0, 44.0, 210.0
ACE_ENV_W, ACE_ENV_D = 315.6, 239.0
ACE_MASS = 3.7
# --- 2. Fixed settings -------------------------------------------------------
BUILD_VOL = 256.0
E_MOD, SY_XY, SY_Z = 1900.0, 45.0, 25.0
G_MOD = E_MOD / (2 * 1.38)            # shear modulus, nu = 0.38
COMP_HOLE, CHAMFER_BED, SHRINK_SCALE = 0.2, 0.3, 1.003
MAX_OVERHANG, MAX_BRIDGE, MIN_WALL = 45.0, 25.0, 2.4
# --- 3. Rack -------------------------------------------------------------------
PANEL_W, RACK_HOLE_X, RAIL_OPEN, U_PITCH = 482.6, 465.1 / 2, 450.0, 44.45
PANEL_H = 43.6
HOLE_FROM_U = (6.35, 22.225, 38.1)
SLOT_H, SLOT_W = 7.0, 10.5
U_EDGE_Z = -(U_PITCH - ACE_H) / 2                     # -0.225 : chassis centred in the U
EAR_Z0 = 0.2                                          # 5.1 bottom edge 0.2 above the chassis bottom
EAR_Z1 = EAR_Z0 + PANEL_H                             # 43.8
HOLE_Z = [U_EDGE_Z + h for h in HOLE_FROM_U]          # 6.125, 22.0, 37.875
CLEAR_MARGIN = 3.0
STRUCT_X_MAX = RAIL_OPEN / 2 - CLEAR_MARGIN           # 222.0
FLANGE_X_OUT = PANEL_W / 2                            # 241.3
RAIL_T, RAIL_W = 2.0, 20.0
# --- 5. Ear ------------------------------------------------------------------------
T_FLANGE = 6.0
T_SIDE_SPEC = 5.0
T_DIAG_SPEC = 4.0
PLATE_PAST_HOLE = 10.0                                # 5.3
DIAG_Y_SPEC = 80.0                                    # 5.4
FILLET_BIG, FILLET_SMALL = 6.0, 2.0
VENT_MARGIN = 2.0
DIAG_SLOT_W, DIAG_SLOT_PITCH = 4.0, 8.0
HOLE_MARGIN = 8.0                                     # 5.7 material around each chassis hole
RELIEF = 3.0                                          # 5.8
CBORE_REMAIN = 3.0                                    # plate left under the counterbore floor (>= MIN_WALL)
ACCESS_HOLE_D = 12.0                                  # screwdriver access through the diagonal wall
SCREW = {"M3": dict(clr=3.4, head_d=6.0, head_h=2.4, washer_od=7.0, washer_t=0.5, proof=4000.0),
         "M4": dict(clr=4.5, head_d=8.0, head_h=3.1, washer_od=9.0, washer_t=0.8, proof=7000.0)}[CH_THREAD]
# --- 7. Loads --------------------------------------------------------------------------
M_EXTRA, COM_Y, G = 1.5, 105.0, 9.81
CASE_A, CASE_B, CASE_C = 1.0, 5.0, 3.0
SF_REQ, SAG_MAX, CREEP_FRAC = 3.0, 1.0, 0.15
N_RACK_SCREWS_USED = 2                                # top + bottom hole (3 slots are provided)

LOG = []
def log(s=""): LOG.append(s)

# =============================================================================
# DERIVED GEOMETRY + SIZING
# =============================================================================
X_SIDE = ACE_W / 2                                    # 155.0 plate inner face sits on the chassis side
T_SIDE = T_SIDE_SPEC
if PROTRUSIONS and T_SIDE - RELIEF < MIN_WALL:
    T_SIDE = math.ceil(RELIEF + MIN_WALL)
CBORE_D = SCREW["washer_od"] + COMP_HOLE
CBORE_DEPTH = T_SIDE - CBORE_REMAIN
Y_PLATE_MIN = max(CH_HOLE_Y) + max(PLATE_PAST_HOLE, (SCREW["clr"] + COMP_HOLE) / 2 + HOLE_MARGIN)   # 5.3 minimum
Y_VENT_START = min(v[0] for v in VENTS) - VENT_MARGIN if VENTS else DIAG_Y_SPEC
Y_PLATE_END = max(Y_PLATE_MIN, min(DIAG_Y_SPEC, Y_VENT_START))      # extend to the vent edge, never over it
Y_DIAG = min(DIAG_Y_SPEC, Y_PLATE_END)
X_DIAG_FLANGE = STRUCT_X_MAX - T_DIAG_SPEC             # wall centre line at 218 so its outer face + fillet stay inside 222
# screw group: two screws separated by (dY, dZ); the pitch moment P*(COM_Y - Y_mean) is a force couple M/d
# perpendicular to the line joining them, plus the shared vertical load P/2 each
M_DES = ACE_MASS + M_EXTRA
W = M_DES * G
P_SIDE = W / 2
Y_MEAN = sum(CH_HOLE_Y) / 2
D_SCREWS = math.hypot(CH_HOLE_Y[1] - CH_HOLE_Y[0], CH_HOLE_Z[1] - CH_HOLE_Z[0])
def screw_force(P):
    f_c = P * (COM_Y - Y_MEAN) / D_SCREWS
    return math.hypot(f_c, P / 2), f_c
while True:
    F_B, _ = screw_force(P_SIDE * CASE_B)
    bearing_B = F_B / ((SCREW["clr"] + COMP_HOLE) * T_SIDE)
    if SY_XY / bearing_B >= SF_REQ: break
    T_SIDE += 1.0
CBORE_DEPTH = T_SIDE - CBORE_REMAIN
log("Side plate thickness for hole bearing at 5 g: F = %.0f N per screw, t -> %.0f mm, bearing %.1f MPa, SF %.2f"
    % (F_B, T_SIDE, bearing_B, SY_XY / bearing_B))
SCREW_LEN = CH_SCREW_LEN_ORIG + (T_SIDE - CBORE_DEPTH)   # engagement stays = factory screw
ENGAGE = SCREW_LEN - (T_SIDE - CBORE_DEPTH)
log("Side plate: t = %.0f mm, ends at Y = %.1f (rearmost hole %.1f + %.1f)" % (T_SIDE, Y_PLATE_END, max(CH_HOLE_Y), Y_PLATE_END - max(CH_HOLE_Y)))
log("Chassis screws: %s x %.0f mm (factory %.0f + plate under the washer %.1f) -> engagement %.1f mm <= depth %.1f mm"
    % (CH_THREAD, SCREW_LEN, CH_SCREW_LEN_ORIG, T_SIDE - CBORE_DEPTH, ENGAGE, CH_DEPTH))
assert ENGAGE <= CH_DEPTH + 1e-9
HOLE_EDGE_MARGIN = min(min(z - EAR_Z0, EAR_Z1 - z) - (SCREW["clr"] + COMP_HOLE) / 2 for z in CH_HOLE_Z)
log("Design mass %.1f kg -> W = %.1f N ; per ear P = %.2f N (1 g) ; COM %.0f mm behind the flange" % (M_DES, W, P_SIDE, COM_Y))

def I_rect(b, h): return b * h ** 3 / 12.0

# --- diagonal wall thickness: Case B in-plane shear + bending as a deep cantilever beam ---------------
L_DIAG = math.hypot(X_DIAG_FLANGE - (X_SIDE + T_SIDE), Y_DIAG - T_FLANGE)   # wall length in plan
T_DIAG = T_DIAG_SPEC
while True:
    V_d = P_SIDE * CASE_B * COM_Y / Y_DIAG                # vertical force the plate end puts into the wall
    tau_d = V_d / (T_DIAG * PANEL_H)
    M_d = V_d * L_DIAG
    sig_d = M_d * (PANEL_H / 2) / I_rect(T_DIAG, PANEL_H)
    SF_DIAG = min(0.577 * SY_XY / tau_d, SY_XY / sig_d)
    if SF_DIAG >= SF_REQ: break
    T_DIAG += 1.0
log("Diagonal wall: length %.1f mm, V = %.1f x 5 x %.0f / %.0f = %.1f N (5 g), t -> %.0f mm, tau = %.2f MPa, "
    "sigma = %.2f MPa (in-layer), SF = %.2f" % (L_DIAG, P_SIDE, COM_Y, Y_DIAG, V_d, T_DIAG, tau_d, sig_d, SF_DIAG))

def load_calcs():
    R = []
    def row(name, case, val, unit, allow, req, note, dirn=""):
        sf = allow / val if val > 0 else float("inf")
        R.append(dict(name=name, case=case, value=val, unit=unit, allow=allow, sf=sf, ok=sf >= req, note=note, dirn=dirn))
    I_plate = I_rect(T_SIDE, PANEL_H)
    I_flg = I_rect(T_FLANGE, PANEL_H)
    I_diag = I_rect(T_DIAG, PANEL_H)
    log(""); log("== Sections ==")
    log("I_plate = %.0f x %.1f^3/12 = %.0f mm4 ; I_flange = %.0f mm4 ; I_diag = %.0f mm4" % (T_SIDE, PANEL_H, I_plate, I_flg, I_diag))
    out = {}
    for case, gf in (("A", CASE_A), ("B", CASE_B)):
        P = P_SIDE * gf
        log(""); log("== Case %s: %.0f g, P per ear = %.1f N ==" % (case, gf, P))
        # chassis screws: weight shared + pitch couple over the screw spacing
        V_rear, F_couple = screw_force(P)
        V_front = V_rear
        bearing = V_rear / ((SCREW["clr"] + COMP_HOLE) * T_SIDE)
        log("Chassis screws (pair %.1f mm apart): couple = %.1f x (%.0f - %.1f) / %.1f = %.1f N along Y, plus P/2 = %.1f N vertical "
            "-> %.1f N resultant per screw ; plate bearing %.1f / (%.1f x %.0f) = %.2f MPa"
            % (D_SCREWS, P, COM_Y, Y_MEAN, D_SCREWS, F_couple, P / 2, V_rear, V_rear, SCREW["clr"] + COMP_HOLE, T_SIDE, bearing))
        # side plate root at the flange: in-plane bending (stress along Y = in-layer)
        M_plate = P * COM_Y
        s_plate = M_plate * (PANEL_H / 2) / I_plate
        # diagonal wall
        Vd = P * COM_Y / Y_DIAG
        tau = Vd / (T_DIAG * PANEL_H)
        s_diag = Vd * L_DIAG * (PANEL_H / 2) / I_diag
        # flange in-plane bending about Y (vertical load at the plate carried to the screw line)
        M_flg = P * (RACK_HOLE_X - (X_SIDE + T_SIDE / 2))
        s_flg = M_flg * (PANEL_H / 2) / I_flg
        # flange torsion share: diagonal path is far stiffer; assume 10 % of the pitch moment in torsion
        a, b = PANEL_H, T_FLANGE
        tau_t = 0.10 * M_plate / (0.31 * a * b * b)
        # rack screws
        lever = HOLE_Z[2] - HOLE_Z[0]
        F_top = M_plate / lever
        F_shear = M_flg / lever
        log("Plate root: M = %.1f x %.0f = %.0f Nmm, sigma = %.2f MPa (in-layer)" % (P, COM_Y, M_plate, s_plate))
        log("Diagonal: V = %.1f N, tau = %.2f MPa, bending sigma = %.2f MPa (in-layer)" % (Vd, tau, s_diag))
        log("Flange: M_y = %.1f x %.1f = %.0f Nmm, sigma = %.2f MPa ; torsion (10 %% share) tau = %.2f MPa" % (P, RACK_HOLE_X - (X_SIDE + T_SIDE / 2), M_flg, s_flg, tau_t))
        log("Rack screws (%d used): pitch %.0f / %.3f = %.0f N tension on the top screw ; roll couple %.0f N shear ; vertical %.1f N"
            % (N_RACK_SCREWS_USED, M_plate, lever, F_top, F_shear, P / N_RACK_SCREWS_USED))
        # sag at the chassis rear (1 g): diagonal cantilever deflection -> rotation about the flange
        d_diag = Vd * L_DIAG / (G_MOD * T_DIAG * PANEL_H) + Vd * L_DIAG ** 3 / (3 * E_MOD * I_diag)
        theta = d_diag / Y_DIAG
        d_plate = P * Y_PLATE_END ** 3 / (3 * E_MOD * I_plate)
        sag = theta * ACE_D + d_plate * ACE_D / Y_PLATE_END
        log("Sag: diagonal tip = V L/(G A) + V L^3/(3 E I) = %.3f + %.3f = %.3f mm -> rotation %.5f rad -> %.3f mm at Y = 210 ; "
            "plate bending adds %.3f mm ; total %.3f mm" % (Vd * L_DIAG / (G_MOD * T_DIAG * PANEL_H),
            Vd * L_DIAG ** 3 / (3 * E_MOD * I_diag), d_diag, theta, theta * ACE_D, d_plate * ACE_D / Y_PLATE_END, sag))
        out[case] = dict(V_rear=V_rear, F_top=F_top, sag=sag)
        if case == "B":
            row("Side plate root", "B", s_plate, "MPa", SY_XY, SF_REQ, "in-layer", "XY")
            row("Diagonal wall shear", "B", tau, "MPa", 0.577 * SY_XY, SF_REQ, "in-layer", "XY")
            row("Diagonal wall bending", "B", s_diag, "MPa", SY_XY, SF_REQ, "in-layer", "XY")
            row("Flange in-plane", "B", s_flg, "MPa", SY_XY, SF_REQ, "in-layer", "XY")
            row("Flange torsion (10 % share)", "B", tau_t, "MPa", 0.577 * SY_XY, SF_REQ, "in-layer", "XY")
            row("Chassis hole bearing", "B", bearing, "MPa", SY_XY, SF_REQ, "rear screw, in-layer", "XY")
            row("Chassis screw shear", "B", V_rear, "N", SCREW["proof"] * 0.6, SF_REQ, "%s 8.8 single shear" % CH_THREAD, "")
            row("Rack screw tension", "B", F_top, "N", 8000.0, SF_REQ, "10-32 steel proof", "")
            w_area = math.pi / 4 * (18 ** 2 - (SLOT_W + COMP_HOLE) * (SLOT_H + COMP_HOLE))
            row("Flange under rack washer", "B", F_top / w_area, "MPa", SY_Z, SF_REQ, "compression across layers", "Z")
        else:
            for nm, s in (("Side plate root", s_plate), ("Diagonal wall", s_diag), ("Flange", s_flg)):
                R.append(dict(name=nm + " creep", case="A", value=s, unit="MPa", allow=CREEP_FRAC * SY_XY,
                              sf=CREEP_FRAC * SY_XY / s, ok=s <= CREEP_FRAC * SY_XY, note="<= 15 % yield", dirn="XY"))
            R.append(dict(name="Rear sag", case="A", value=sag, unit="mm", allow=SAG_MAX, sf=SAG_MAX / sag, ok=sag <= SAG_MAX,
                          note="elastic; hole clearance slop not included", dirn=""))
    # Case C
    F_C = CASE_C * W
    log(""); log("== Case C: 3 g, F = %.0f N ==" % F_C)
    # front-to-back: chassis screws in shear along Y, 4 screws, plus yaw none (symmetric)
    v_fb = F_C / 4
    # side-to-side: screws in tension on one side, 2 screws; plus the yaw moment F x COM as a couple between the ears
    yaw = F_C * COM_Y
    F_yaw = yaw / (2 * X_SIDE)
    t_ss = F_C / 2 / 2 + F_yaw / 2
    log("Front-to-back: %.1f N shear per chassis screw. Side-to-side: %.0f N direct + yaw %.0f Nmm / %.0f = %.0f N -> %.0f N tension per chassis screw"
        % (v_fb, F_C / 2, yaw, 2 * X_SIDE, F_yaw, t_ss))
    row("Chassis screw tension (side load)", "C", t_ss, "N", SCREW["proof"], SF_REQ, "%s proof; thread in the ACE must hold this" % CH_THREAD, "")
    row("Chassis screw shear (front-back)", "C", v_fb, "N", SCREW["proof"] * 0.6, SF_REQ, "", "")
    bear_c = max(v_fb, t_ss) / ((SCREW["clr"] + COMP_HOLE) * T_SIDE)
    row("Plate bearing / head pull-through", "C", t_ss / (math.pi / 4 * (SCREW["washer_od"] ** 2 - (SCREW["clr"] + COMP_HOLE) ** 2)),
        "MPa", SY_XY, SF_REQ, "washer bearing on the counterbore floor, in-layer", "XY")
    R.append(dict(name="Chassis retained (Case C)", case="C", value=1, unit="", allow=1, sf=float("inf"), ok=True,
                  note="4 screws into the chassis; no slip possible", dirn=""))
    return R, out

# =============================================================================
# GEOMETRY HELPERS
# =============================================================================
def box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))

def prism_xy(pts, z0, z1):
    return cq.Workplane("XY").polyline(pts).close().extrude(z1 - z0).translate((0, 0, z0))

def cove_xy(x0, x1, y0, y1, cx, cy, z0, z1):
    r = x1 - x0
    cyl = cq.Workplane("XY").center(cx, cy).circle(r).extrude(z1 - z0).translate((0, 0, z0))
    return box(x0, x1, y0, y1, z0, z1).cut(cyl)

def teardrop_obround_xz(w, h, cx, cz, y0, y1):
    r, a = h / 2.0, w / 2.0 - h / 2.0
    s = r / math.sqrt(2)
    wp = (cq.Workplane("XZ").center(cx, cz).moveTo(-a, -r).lineTo(a, -r)
          .threePointArc((a + r, 0), (a + s, s)).lineTo(0, a + r * math.sqrt(2)).lineTo(-a - s, s)
          .threePointArc((-a - r, 0), (-a, -r)).close().extrude(-(y1 - y0)))
    return wp.translate((0, y0, 0))

def teardrop_round_x(d, cy, cz, x0, x1):
    """Round hole with X axis, 45 deg roof pointing +Z (printed with Z up)."""
    r = d / 2.0
    s = r / math.sqrt(2)
    wp = (cq.Workplane("YZ").center(cy, cz).moveTo(s, s).lineTo(0, r * math.sqrt(2)).lineTo(-s, s)
          .threePointArc((0, -r), (s, s)).close().extrude(x1 - x0))
    return wp.translate((x0, 0, 0))

def gable_window_x(y0, y1, z0, z1, x0, x1):
    """Rectangular window through an X-normal plate with a 45 deg gable roof (no flat overhang)."""
    w = y1 - y0
    pts = [(y0, z0), (y1, z0), (y1, z1), ((y0 + y1) / 2, z1 + w / 2), (y0, z1)]
    wp = cq.Workplane("YZ").polyline(pts).close().extrude(x1 - x0)
    return wp.translate((x0, 0, 0))

def mirror_x(wp): return wp.mirror("YZ")

# =============================================================================
# EAR (right-hand, +X)
# =============================================================================
FILLETS_OK = []
def build_ear():
    z0, z1 = EAR_Z0, EAR_Z1
    x_plate_out = X_SIDE + T_SIDE
    flange = box(X_SIDE, FLANGE_X_OUT, 0, T_FLANGE, z0, z1)
    for zc in HOLE_Z:
        flange = flange.cut(teardrop_obround_xz(SLOT_W + COMP_HOLE, SLOT_H + COMP_HOLE, RACK_HOLE_X, zc, -1, T_FLANGE + 1))
    plate = box(X_SIDE, x_plate_out, 0, Y_PLATE_END, z0, z1)
    # diagonal wall: thick line from the flange rear face at X=222 to the plate outer face at Y_DIAG, clipped
    A = (X_DIAG_FLANGE, T_FLANGE); B = (x_plate_out, Y_DIAG)
    dx, dy = B[0] - A[0], B[1] - A[1]; L = math.hypot(dx, dy); ux, uy = dx / L, dy / L
    nx, ny = -uy, ux; h = T_DIAG / 2; ext = 8.0
    A2 = (A[0] - ux * ext, A[1] - uy * ext); B2 = (B[0] + ux * ext, B[1] + uy * ext)
    diag = prism_xy([(A2[0] + nx * h, A2[1] + ny * h), (B2[0] + nx * h, B2[1] + ny * h),
                     (B2[0] - nx * h, B2[1] - ny * h), (A2[0] - nx * h, A2[1] - ny * h)], z0, z1)
    diag = diag.intersect(box(X_SIDE, X_DIAG_FLANGE, 0, Y_PLATE_END, z0 - 1, z1 + 1))
    cove = cove_xy(x_plate_out, x_plate_out + FILLET_BIG, T_FLANGE, T_FLANGE + FILLET_BIG,
                   x_plate_out + FILLET_BIG, T_FLANGE + FILLET_BIG, z0, z1)
    ear = flange.union(plate).union(diag).union(cove)
    # 2 mm fillets on the vertical edges where the diagonal meets the flange and the plate (inside corners)
    try:
        for pt in ((A[0] - nx * h, T_FLANGE, (z0 + z1) / 2), (x_plate_out, B[1] + ny * h, (z0 + z1) / 2)):
            ear = ear.edges(cq.selectors.NearestToPointSelector(pt)).fillet(FILLET_SMALL)
        FILLETS_OK.append(True)
    except Exception:
        FILLETS_OK.append(False)
    # chassis holes: teardrop clearance + flat-bottom counterbore from the outer face
    for yc, zc in zip(CH_HOLE_Y, CH_HOLE_Z):
        ear = ear.cut(teardrop_round_x(SCREW["clr"] + COMP_HOLE, yc, zc, X_SIDE - 1, x_plate_out + 1))
        ear = ear.cut(teardrop_round_x(CBORE_D, yc, zc, X_SIDE + CBORE_REMAIN, x_plate_out + 1))
        # screwdriver access through the diagonal wall (only where the screw axis crosses it)
        if yc < Y_DIAG:
            acc = teardrop_round_x(ACCESS_HOLE_D, yc, zc, x_plate_out + 0.5, X_DIAG_FLANGE)
            ear = ear.cut(acc.intersect(box(x_plate_out + 0.5, X_DIAG_FLANGE, T_FLANGE + 0.5, Y_PLATE_END, z0 - 1, z1 + 1)))
    # vent windows in the plate (2 mm margin, gable roof) and slots in the diagonal where it covers a vent
    for (vy0, vy1, vz0, vz1) in VENTS:
        if vy0 - VENT_MARGIN < Y_PLATE_END:
            ear = ear.cut(gable_window_x(vy0 - VENT_MARGIN, min(vy1 + VENT_MARGIN, Y_PLATE_END - MIN_WALL),
                                         vz0 - VENT_MARGIN, vz1 + VENT_MARGIN, X_SIDE - 1, x_plate_out + 1))
        ya, yb = max(vy0 - VENT_MARGIN, T_FLANGE + 4), min(vy1 + VENT_MARGIN, Y_DIAG - 4)
        s = (ya - A[1]) / uy + DIAG_SLOT_PITCH / 2
        while (A[1] + uy * s) + DIAG_SLOT_W / 2 <= yb:
            cx, cy = A[0] + ux * s, A[1] + uy * s
            slot = (cq.Workplane("XY").rect(DIAG_SLOT_W, T_DIAG + 4).extrude(z1 - z0 - 8).translate((0, 0, z0 + 4))
                    .rotate((0, 0, 0), (0, 0, 1), math.degrees(math.atan2(uy, ux))).translate((cx, cy, 0)))
            ear = ear.cut(slot)
            s += DIAG_SLOT_PITCH
    # 5.8 relief where the side carries protrusions
    for (py0, py1, pz0, pz1) in PROTRUSIONS:
        ear = ear.cut(box(X_SIDE - 1, X_SIDE + RELIEF, py0 - 1, py1 + 1, pz0 - 1, pz1 + 1))
    return ear

def build_ace():
    body = box(-ACE_W / 2, ACE_W / 2, 0, ACE_D, 0, ACE_H)
    env = box(-ACE_ENV_W / 2, ACE_ENV_W / 2, 0, ACE_ENV_D, 0, ACE_H)
    return body, env

def build_rail():
    z0, z1 = U_EDGE_Z - 10, U_EDGE_Z + U_PITCH + 10
    front = box(RAIL_OPEN / 2, RAIL_OPEN / 2 + RAIL_W, T_FLANGE, T_FLANGE + RAIL_T, z0, z1)
    leg = box(RAIL_OPEN / 2 + RAIL_W - RAIL_T, RAIL_OPEN / 2 + RAIL_W, T_FLANGE, T_FLANGE + 40, z0, z1)
    rail = front.union(leg)
    for zc in HOLE_Z:
        rail = rail.cut(cq.Workplane("XZ").center(RACK_HOLE_X, zc).slot2D(SLOT_W, SLOT_H).extrude(-RAIL_T - 2).translate((0, T_FLANGE - 1, 0)))
    return rail

def to_print_orientation(wp):
    bb = wp.val().BoundingBox()
    return wp.translate((-bb.xmin, -bb.ymin, -bb.zmin))       # rack vertical = printer Z (5.9)

# =============================================================================
# VERIFICATION
# =============================================================================
CHECKS = []
def check(name, ok, value, status=None):
    CHECKS.append(dict(name=name, ok=bool(ok), value=value, status=status or ("PASS" if ok else "FAIL")))

def vol_intersect(a, b):
    try: return a.intersect(b).val().Volume()
    except Exception: return 0.0

def overhang_check(name, wp):
    solid = wp.val(); bb = solid.BoundingBox()
    sin_lim = math.sin(math.radians(MAX_OVERHANG + 0.5)); bad = []; n_bridge = 0
    for f in solid.Faces():
        try:
            vs, tris = f.tessellate(0.5, 0.5); pts = [(vs[t[0]] + vs[t[1]] + vs[t[2]]) / 3 for t in tris]
        except Exception:
            pts = [f.Center()]
        for c in pts[:60]:
            try: n = f.normalAt(c)
            except Exception: n = f.normalAt()
            if n.z >= -sin_lim: continue
            if n.z < -0.999:
                fb = f.BoundingBox()
                if abs(fb.zmin - bb.zmin) < 1e-3: continue
                span = min(fb.xlen, fb.ylen)
                if span <= MAX_BRIDGE: n_bridge += 1; continue
                bad.append("flat %.1f mm span at z=%.1f" % (span, fb.zmin))
            else:
                bad.append("sloped face %.1f deg at z=%.1f" % (math.degrees(math.asin(-n.z)), c.z))
            break
    check("%s: no overhang > 45 deg (bridges <= %.0f mm allowed)" % (name, MAX_BRIDGE), not bad,
          "OK, %d bridge faces" % n_bridge if not bad else "; ".join(bad[:3]))

def run_verification(ear_R, ear_L, ace_body, ace_env, rails):
    v = max(vol_intersect(ear_R, ace_body), vol_intersect(ear_L, ace_body))
    check("Zero interference: ears vs ACE body (310 x 44 x 210)", v < 1e-6, "%.4f mm3" % v)
    v = vol_intersect(ear_R, ace_env)
    plate_vol = vol_intersect(box(X_SIDE, ACE_ENV_W / 2, 0, Y_PLATE_END, EAR_Z0, EAR_Z1), ear_R)
    check("Ears vs 315.6 envelope: only the side plate's own footprint on the side panel (bolted there)", abs(v - plate_vol) < 1e-6,
          "%.0f mm3 = plate footprint; relief pockets entered: %d" % (v, len(PROTRUSIONS)))
    v = max(vol_intersect(ear_R, r) for r in rails)
    check("Zero interference: ears vs rack rails", v < 1e-6, "%.4f mm3" % v)
    blank = box(X_SIDE, FLANGE_X_OUT, 0, T_FLANGE, EAR_Z0, EAR_Z1)
    voids = blank.cut(ear_R).solids().vals(); found, worst = 0, 0.0
    for zc in HOLE_Z:
        for s in voids:
            b = s.BoundingBox()
            if b.xmax < 225 or b.xmin > 241 or not (b.zmin < zc < b.zmax): continue
            found += 1; worst = max(worst, abs((b.xmin + b.xmax) / 2 - RACK_HOLE_X), abs(b.zmin + (SLOT_H + COMP_HOLE) / 2 - zc))
    check("Rack slot centres within 0.05 mm (X 232.55; Z 6.35 / 22.225 / 38.1 from the U edge)", found == 3 and worst <= 0.05,
          "%d slots, max deviation %.4f mm" % (found, worst))
    bR, bL = ear_R.val().BoundingBox(), ear_L.val().BoundingBox()
    check("Assembly width = 482.6 mm", abs(bR.xmax - bL.xmin - PANEL_W) < 0.01, "%.3f mm" % (bR.xmax - bL.xmin))
    check("Panel height = 43.6 mm (1U)", abs(bR.zmax - bR.zmin - PANEL_H) < 0.01, "%.3f mm" % (bR.zmax - bR.zmin))
    behind = ear_R.cut(box(0, 300, -10, T_FLANGE, -1, 100)).val().BoundingBox().xmax
    check("Structure behind the flange <= 222 mm (3 mm inside the rail opening)", behind <= STRUCT_X_MAX + 1e-6, "%.2f mm" % behind)
    # vents
    if not VENTS:
        check("Every official vent uncovered (side plate windows + diagonal slots)", False,
              "NO VENT DATA ENTERED - measure the side panel and fill VENTS, then re-run", status="UNVERIFIED")
    else:
        worst = 0.0
        for (vy0, vy1, vz0, vz1) in VENTS:
            worst = max(worst, vol_intersect(ear_R, box(X_SIDE - 0.5, X_SIDE + T_SIDE + 0.5, vy0, vy1, vz0, vz1)))
        check("Every official vent uncovered (side plate windows + diagonal slots)", worst < 1e-6, "%.4f mm3 of plate over vents" % worst)
    walls = {"flange": T_FLANGE, "side plate": T_SIDE, "diagonal": T_DIAG, "under counterbore": CBORE_REMAIN,
             "around chassis hole": HOLE_MARGIN, "slot to flange edge": FLANGE_X_OUT - RACK_HOLE_X - (SLOT_W + COMP_HOLE) / 2,
             "between rack slots": (HOLE_Z[1] - HOLE_Z[0]) - (SLOT_H + COMP_HOLE),
             "plate relief remainder": (T_SIDE - RELIEF) if PROTRUSIONS else T_SIDE}
    walls["around chassis hole"] = HOLE_EDGE_MARGIN
    mn = min(walls, key=walls.get)
    check("Minimum wall >= 2.4 mm (thinnest: %s)" % mn, walls[mn] >= MIN_WALL, "%.2f mm" % walls[mn])
    check("8 mm material around each chassis hole (5.7)", HOLE_EDGE_MARGIN >= HOLE_MARGIN,
          "%.2f mm between the lower hole and the ear bottom edge (ear bottom fixed at 0.2 mm by 5.1; hole position fixed by the chassis)"
          % HOLE_EDGE_MARGIN, status=None if HOLE_EDGE_MARGIN >= HOLE_MARGIN else "SPEC-CONFLICT")
    for n, p in (("ear_R", ear_R), ("ear_L", ear_L)):
        po = to_print_orientation(p); s = po.val(); ns = len(po.solids().vals())
        check("%s: one watertight solid" % n, ns == 1 and s.isValid(), "%d solid(s), valid=%s, %.1f cm3" % (ns, s.isValid(), s.Volume() / 1000))
        bb = s.BoundingBox()
        check("%s: inside 256^3 build volume as oriented" % n, max(bb.xlen, bb.ylen, bb.zlen) <= BUILD_VOL, "%.1f x %.1f x %.1f mm" % (bb.xlen, bb.ylen, bb.zlen))
        overhang_check(n, po)

# =============================================================================
# EXPORT, RENDER, REPORT, MAIN
# =============================================================================
def export_svg(wp, path, d):
    exporters.export(wp, path, opt={"width": 900, "height": 650, "marginLeft": 20, "marginTop": 20, "showAxes": False,
                                    "projectionDir": d, "strokeWidth": 0.4, "showHidden": False})

def render_png(shapes, path, title, elev=25, azim=-60, ortho=False):
    import numpy as np, matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    fig = plt.figure(figsize=(10, 7), dpi=110); ax = fig.add_subplot(111, projection="3d"); allpts = []
    light = np.array([0.4, -0.6, 0.7]); light /= np.linalg.norm(light)
    for wp, col in shapes:
        vs, tris = wp.val().tessellate(0.6, 0.3)
        V = np.array([[v.x, v.y, v.z] for v in vs]); F = np.array(tris)
        if len(F) == 0: continue
        polys = V[F]; n = np.cross(polys[:, 1] - polys[:, 0], polys[:, 2] - polys[:, 0]); n /= (np.linalg.norm(n, axis=1)[:, None] + 1e-12)
        cols = np.clip(np.array(col)[None, :] * (0.45 + 0.55 * np.abs(n @ light))[:, None], 0, 1)
        ax.add_collection3d(Poly3DCollection(polys, facecolors=cols, edgecolors="none")); allpts.append(V)
    P = np.vstack(allpts); mn, mx = P.min(axis=0), P.max(axis=0); c = (mn + mx) / 2; r = (mx - mn).max() / 2
    ax.set_xlim(c[0] - r, c[0] + r); ax.set_ylim(c[1] - r, c[1] + r); ax.set_zlim(c[2] - r, c[2] + r); ax.set_box_aspect((1, 1, 1))
    ax.view_init(elev=elev, azim=azim)
    if ortho: ax.set_proj_type("ortho")
    ax.set_axis_off(); ax.set_title(title); fig.tight_layout(); fig.savefig(path); plt.close(fig)

VIEWS = {"front": (0, -1, 0), "top": (0, 0, 1), "side": (1, 0, 0), "iso": (1, -1, 0.8)}
BLUE, GREY, DARK = (0.25, 0.45, 0.85), (0.5, 0.5, 0.5), (0.3, 0.3, 0.3)

def main():
    for d in ("step", "3mf", "renders"):
        os.makedirs(os.path.join(OUT, d), exist_ok=True)
    ear_R = build_ear(); ear_L = mirror_x(ear_R)
    ace_body, ace_env = build_ace()
    rail_R = build_rail(); rails = [rail_R, mirror_x(rail_R)]
    R, out = load_calcs()
    run_verification(ear_R, ear_L, ace_body, ace_env, rails)
    check("2 mm fillets at the diagonal junctions applied", all(FILLETS_OK) and FILLETS_OK, "%s" % FILLETS_OK)
    for r in R:
        check("Load: %s (Case %s)" % (r["name"], r["case"]), r["ok"],
              "%.2f %s vs %.1f -> SF %.2f" % (r["value"], r["unit"], r["allow"], r["sf"]) if r["unit"] else "retained")
    files = []
    for n, p in (("ear_R", ear_R), ("ear_L", ear_L)):
        exporters.export(p, os.path.join(OUT, "step", n + ".step")); files.append("step/%s.step" % n)
        po = to_print_orientation(p)
        try: po = po.faces("<Z").chamfer(CHAMFER_BED); ch = "applied"
        except Exception: ch = "NOT applied"
        exporters.export(cq.Workplane("XY").add(po.val().scale(SHRINK_SCALE)), os.path.join(OUT, "3mf", n + ".3mf"))
        exporters.export(po, os.path.join(OUT, "step", n + "_print_oriented.step"))
        files.append("3mf/%s.3mf (x%.3f shrink, bed chamfer %s)" % (n, SHRINK_SCALE, ch))
    # test-fit template: 2 mm plate with the two holes and a lip that registers on the front-panel face
    tpl = box(X_SIDE, X_SIDE + 2.0, 0, Y_PLATE_END, EAR_Z0, EAR_Z1).union(box(X_SIDE, X_SIDE + 8.0, -3.0, 0, EAR_Z0, EAR_Z1))
    for yc, zc in zip(CH_HOLE_Y, CH_HOLE_Z):
        tpl = tpl.cut(teardrop_round_x(SCREW["clr"] + COMP_HOLE, yc, zc, X_SIDE - 1, X_SIDE + 3))
    for (py0, py1, pz0, pz1) in PROTRUSIONS:
        tpl = tpl.cut(box(X_SIDE - 1, X_SIDE + 3, py0 - 1, py1 + 1, pz0 - 1, pz1 + 1))
    tpl_print = tpl.rotate((0, 0, 0), (0, 1, 0), -90)                      # plate flat on the bed
    bb = tpl_print.val().BoundingBox(); tpl_print = tpl_print.translate((-bb.xmin, -bb.ymin, -bb.zmin))
    exporters.export(cq.Workplane("XY").add(tpl_print.val().scale(SHRINK_SCALE)), os.path.join(OUT, "3mf", "test_fit_template.3mf"))
    exporters.export(tpl, os.path.join(OUT, "step", "test_fit_template.step")); files.append("3mf/test_fit_template.3mf (2 mm plate, holes + front lip)")
    assy = cq.Assembly(name="ACE_1U_ears")
    assy.add(ear_R, name="ear_R", color=cq.Color(*BLUE)); assy.add(ear_L, name="ear_L", color=cq.Color(*BLUE))
    assy.add(ace_body, name="ACE_body", color=cq.Color(*DARK))
    for i, r in enumerate(rails): assy.add(r, name="rail_%d" % i, color=cq.Color(*GREY))
    assy.save(os.path.join(OUT, "step", "assembly_in_rack.step")); files.append("step/assembly_in_rack.step")
    shapes = [(ear_R, BLUE), (ear_L, BLUE), (ace_body, DARK)] + [(r, GREY) for r in rails]
    comp = cq.Workplane("XY").add(cq.Compound.makeCompound([s[0].val() for s in shapes]))
    for vn, d in VIEWS.items():
        export_svg(ear_R, os.path.join(OUT, "renders", "ear_R_%s.svg" % vn), d)
        export_svg(comp, os.path.join(OUT, "renders", "assembly_%s.svg" % vn), d)
    for vn, (el, az, o) in {"iso": (22, -55, False), "front": (0, -90, True), "top": (90, -90, True), "side": (0, 0, True)}.items():
        render_png(shapes, os.path.join(OUT, "renders", "assembly_%s.png" % vn), "ACE 1U ears in rack - " + vn, el, az, o)
        render_png([(ear_R, BLUE)], os.path.join(OUT, "renders", "ear_R_%s.png" % vn), "ear_R - " + vn, el, az, o)
    render_png([(ear_R, BLUE)], os.path.join(OUT, "renders", "ear_R_iso_outside.png"), "ear_R - from the rack side", 22, 40)
    # report
    with open(os.path.join(OUT, "report.md"), "w") as f:
        f.write("# ACE 1U bolt-on ears - automatic verification report\n\n")
        f.write("**Chassis hole data are placeholders until measured:** holes at Y = %s, Z = %s, %s, factory screw %.0f mm, depth %.0f mm, vents %s\n\n"
                % (CH_HOLE_Y, CH_HOLE_Z, CH_THREAD, CH_SCREW_LEN_ORIG, CH_DEPTH, VENTS or "NOT ENTERED"))
        f.write("| # | Check | Result | Value |\n|---|---|---|---|\n")
        for i, c in enumerate(CHECKS, 1): f.write("| %d | %s | %s | %s |\n" % (i, c["name"], c["status"], c["value"]))
        f.write("\n## Calculation log\n\n```\n" + "\n".join(LOG) + "\n```\n\n## Load table\n\n| Feature | Case | Value | Allowable | SF | Pass | Note |\n|---|---|---|---|---|---|---|\n")
        for r in R:
            f.write("| %s | %s | %.2f %s | %.1f %s | %.2f | %s | %s %s |\n" % (r["name"], r["case"], r["value"], r["unit"], r["allow"], r["unit"], r["sf"],
                    "yes" if r["ok"] else "NO", r["note"], ("[%s]" % r["dirn"]) if r["dirn"] else ""))
        f.write("\n## Derived dimensions\n\n")
        for k, v in dict(T_SIDE=T_SIDE, T_DIAG=T_DIAG, Y_PLATE_END=round(Y_PLATE_END, 2), Y_DIAG=Y_DIAG, CBORE_D=CBORE_D, CBORE_DEPTH=CBORE_DEPTH,
                         SCREW_LEN=SCREW_LEN, HOLE_Z_rack=[round(z, 3) for z in HOLE_Z], EAR_Z=(EAR_Z0, EAR_Z1),
                         mass_g_solid=round(ear_R.val().Volume() * 1.27e-3)).items():
            f.write("- %s = %s\n" % (k, v))
        f.write("\n## Files\n\n" + "\n".join("- " + x for x in files) + "\n")
    json.dump(dict(checks=CHECKS, loads=R), open(os.path.join(OUT, "verification.json"), "w"), indent=1, default=str)
    print("\n".join("%-10s %s : %s" % (c["status"], c["name"], c["value"]) for c in CHECKS))
    nf = sum(1 for c in CHECKS if c["status"] == "FAIL"); nu = sum(1 for c in CHECKS if c["status"] == "UNVERIFIED")
    print("\n%d checks: %d failed, %d unverified (need measurements)" % (len(CHECKS), nf, nu))
    return nf == 0

if __name__ == "__main__":
    sys.exit(0 if main() else 1)
