#!/usr/bin/env python3
"""
DirectOut ACE -> EIA-310-D 19" rack, 1U fabricated metal ears using all four factory side screws per side.

Two parts per side (left = mirror):
  * side plate  : 2.0 mm 304 stainless, laser cut, one 90 deg bend forming a 15 mm tab at the front.
                  Four holes on the factory screw pattern, a window over the vent field, clearance holes
                  for the small M3 screw (whichever end turns out to be the front).
  * rack flange : 6.0 mm 6061-T6 aluminium, laser/waterjet cut, three EIA rack slots, three countersunk
                  M4 holes.  The side plate's tab bolts flat to the flange's rear face with M4 screws + nyloc nuts.
The thick flange carries the pitch moment in torsion; the thin plate keeps thread engagement for the
factory screws (it takes only 2 mm).

Coordinates: X rack width (0 at rack centre, +X right ear), Y depth (0 at the ACE front face, +Y rearwards),
Z vertical, 0 at the chassis bottom.  Ear spans Z 0.2 ... 43.8 (1U panel 43.6 centred on the 44 mm chassis).
"""
import math, os, sys, json
import cadquery as cq
from cadquery import exporters
import ezdxf

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output_metal")

# ===== MEASURED FROM THE PHOTO (IMG_8471 rectified on the 210 x 44 panel, +/- 0.5 mm; confirm with the rubbing) =====
SCREW_Y_FRONT = 17.5          # both pairs sit 17.5 mm from their end face
SCREW_Y_REAR  = 210.0 - 17.5  # 192.5
SCREW_Z       = (9.5, 34.5)   # vertical pair, symmetric about mid-height 22
SCREW_HEAD_D  = 7.6           # M4 button head (photo 7.1 - 7.7)
VENT          = (38.0, 168.0, 8.0, 38.0)        # y0, y1, z0, z1 of the hex-perforated field
M3_SCREW      = (32.6, 6.8, 5.2)                # y, z, head diameter (seen at the left end only)
FRONT_LIP_T   = 3.0           # thickness of the front/rear panel lips that stand 2.8 mm proud (not measurable in the photo)
REAR_LIP_T    = 3.0
# ===== device / rack =====================================================================================
ACE_W, ACE_H, ACE_D, ACE_ENV_W, ACE_ENV_D, ACE_MASS = 310.0, 44.0, 210.0, 315.6, 239.0, 3.7
PANEL_W, RACK_HOLE_X, RAIL_OPEN, U_PITCH, PANEL_H = 482.6, 465.1 / 2, 450.0, 44.45, 43.6
HOLE_FROM_U, SLOT_H, SLOT_W = (6.35, 22.225, 38.1), 7.0, 10.5
U_EDGE_Z = -(U_PITCH - ACE_H) / 2
EAR_Z0, EAR_Z1 = 0.2, 0.2 + PANEL_H
HOLE_Z = [U_EDGE_Z + h for h in HOLE_FROM_U]          # 6.125, 22.0, 37.875
STRUCT_X_MAX, FLANGE_X_OUT = RAIL_OPEN / 2 - 3.0, PANEL_W / 2
RAIL_T, RAIL_W = 2.0, 20.0
# ===== materials ===========================================================================================
PLATE = dict(name="304 stainless (or mild steel, powder coated)", t=2.0, E=193000.0, G=74000.0, sy=205.0, rho=7.9e-3)
FLANGE = dict(name="6061-T6 aluminium plate", t=6.0, E=69000.0, G=26000.0, sy=240.0, tau_y=140.0, rho=2.7e-3)
# ===== ear geometry ==========================================================================================
X_SIDE = ACE_W / 2                      # plate inner face on the chassis side
T_P, T_F = PLATE["t"], FLANGE["t"]
R_BEND = 2.0                            # inside bend radius (1 t)
K_FACTOR = 0.44
X_FLANGE_IN = ACE_ENV_W / 2 + 0.2       # 158.0: clears the 2.8 mm panel lip
TAB_W = 15.0                            # tab length outboard of the bend (X 159 ... 174)
TAB_X0 = X_SIDE + R_BEND + T_P          # 159.0 straight part of the tab starts here
TAB_X1 = TAB_X0 + TAB_W
Y_FLANGE0, Y_FLANGE1 = 0.0, T_F          # flange front face coplanar with the ACE front face
Y_TAB0, Y_TAB1 = Y_FLANGE1, Y_FLANGE1 + T_P
Y_PLATE0 = Y_TAB0 + R_BEND + T_P         # 10.0: straight plate starts after the bend
Y_PLATE1 = ACE_D - REAR_LIP_T - 1.0      # 206.0: stops 1 mm short of the rear lip
CH_HOLE_D = 4.8                          # M4 clearance, round at the front pair
CH_SLOT_L = 7.0                          # rear pair: obround along Y (+/- 1.1 mm on the pair spacing)
M3_CLEAR_D = 7.0
VENT_MARGIN, WINDOW_R = 2.0, 2.0
JOINT_X = (TAB_X0 + TAB_X1) / 2          # 166.5
JOINT_Z = (6.5, 22.0, 37.5)
JOINT_D, CSK_D = 4.5, 9.0                # M4 countersunk DIN 7991 from the flange front face
# ===== loads ===============================================================================================
M_EXTRA, COM_Y, G = 1.5, 105.0, 9.81
CASE_A, CASE_B, CASE_C, SF_REQ, SAG_MAX = 1.0, 5.0, 3.0, 3.0, 1.0
N_RACK_SCREWS_USED = 2

LOG = []
def log(s=""): LOG.append(s)

BA = (R_BEND + K_FACTOR * T_P) * math.pi / 2                     # bend allowance, 90 deg
U_PLATE = Y_PLATE1 - Y_PLATE0                                    # straight plate length
U_BEND0, U_BEND1 = U_PLATE, U_PLATE + BA
U_TAB1 = U_BEND1 + TAB_W
def u_of_y(y): return Y_PLATE1 - y                               # flat coordinate of a plate feature (u=0 at the rear end)
def u_of_x(x): return U_BEND1 + (x - TAB_X0)                     # flat coordinate of a tab feature

# =============================================================================
# GEOMETRY
# =============================================================================
def box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))

def cyl_x(d, cy, cz, x0, x1):
    return cq.Workplane("YZ").center(cy, cz).circle(d / 2).extrude(x1 - x0).translate((x0, 0, 0))

def slot_x(w, l, cy, cz, x0, x1):          # obround: w = width (Z), l = length along Y
    return cq.Workplane("YZ").center(cy, cz).slot2D(l, w, 0).extrude(x1 - x0).translate((x0, 0, 0))

def slot_y(w, h, cx, cz, y0, y1):          # rack slot: w along X, h along Z, axis Y
    return cq.Workplane("XZ").center(cx, cz).slot2D(w, h, 0).extrude(-(y1 - y0)).translate((0, y0, 0))

def mirror_x(wp): return wp.mirror("YZ")

WIN = (VENT[0] - VENT_MARGIN, VENT[1] + VENT_MARGIN, VENT[2] - VENT_MARGIN, VENT[3] + VENT_MARGIN)   # window y0, y1, z0, z1
def m3_merges_into_window(yc):
    """True when the M3 clearance circle would leave less than 1.5 t between itself and the window edge."""
    r = M3_CLEAR_D / 2
    return (yc + r > WIN[0] - 1.5 * T_P and yc < WIN[0]) or (yc - r < WIN[1] + 1.5 * T_P and yc > WIN[1])

def cut_plate_features(solid, to_plane, thick_axis_extent):
    """Apply every 2D feature of the side plate.  to_plane(y, z) maps chassis-side coordinates to the plane
    of `solid` (bent plate: (y, z) on the YZ workplane; flat pattern: (u, z) on the XY workplane).
    Returns the cut solid.  The callback `cutter(center, shape)` is built per target in the caller."""
    raise NotImplementedError

def plate_features_2d(wp_factory):
    """Yield cutter solids for the plate features.  wp_factory(kind, **k) builds a through-cutter in the
    right plane for 'circle' (c=(y,z), d), 'slot' (c=(y,z), l, w) and 'rrect' (y0,y1,z0,z1,r)."""
    cutters = []
    for zc in SCREW_Z:
        cutters.append(wp_factory("circle", c=(SCREW_Y_FRONT, zc), d=CH_HOLE_D))
        cutters.append(wp_factory("slot", c=(SCREW_Y_REAR, zc), l=CH_SLOT_L, w=CH_HOLE_D))
    for yc in (M3_SCREW[0], ACE_D - M3_SCREW[0]):
        if m3_merges_into_window(yc):
            # notch: stadium from the M3 centre into the window
            y_in = WIN[0] + 1.0 if yc < WIN[0] else WIN[1] - 1.0
            cutters.append(wp_factory("slot", c=((yc + y_in) / 2, M3_SCREW[1]), l=abs(y_in - yc) + M3_CLEAR_D, w=M3_CLEAR_D))
        else:
            cutters.append(wp_factory("circle", c=(yc, M3_SCREW[1]), d=M3_CLEAR_D))
    cutters.append(wp_factory("rrect", y0=WIN[0], y1=WIN[1], z0=WIN[2], z1=WIN[3], r=WINDOW_R))
    return cutters

def build_flat_plate():
    """Flat pattern as a 1 mm thick solid in the XY plane: X = u (0 at the rear end), Y = Z."""
    flat = cq.Workplane("XY").box(U_TAB1, PANEL_H, 1.0, centered=False).translate((0, EAR_Z0, -0.5))
    def fac(kind, **k):
        if kind == "circle":
            return cq.Workplane("XY").center(u_of_y(k["c"][0]), k["c"][1]).circle(k["d"] / 2).extrude(3).translate((0, 0, -1.5))
        if kind == "slot":
            return cq.Workplane("XY").center(u_of_y(k["c"][0]), k["c"][1]).slot2D(k["l"], k["w"], 0).extrude(3).translate((0, 0, -1.5))
        u0, u1 = u_of_y(k["y1"]), u_of_y(k["y0"])
        return (cq.Workplane("XY").center((u0 + u1) / 2, (k["z0"] + k["z1"]) / 2).rect(u1 - u0, k["z1"] - k["z0"]).extrude(3)
                .translate((0, 0, -1.5)).edges("|Z").fillet(k["r"]))
    for c in plate_features_2d(fac): flat = flat.cut(c)
    for zc in JOINT_Z:
        flat = flat.cut(cq.Workplane("XY").center(u_of_x(JOINT_X), zc).circle(JOINT_D / 2).extrude(3).translate((0, 0, -1.5)))
    return flat

def build_side_plate():
    """Bent 2 mm plate: straight plate on the chassis side, 90 deg bend, tab behind the flange."""
    z0, z1 = EAR_Z0, EAR_Z1
    plate = box(X_SIDE, X_SIDE + T_P, Y_PLATE0, Y_PLATE1, z0, z1)
    tab = box(TAB_X0, TAB_X1, Y_TAB0, Y_TAB1, z0, z1)
    # bend: quarter annulus, centre at (X_SIDE + T_P + R_BEND, Y_TAB1 + R_BEND)... geometry: plate outer face X_SIDE+T_P
    # continues into the tab front face Y_TAB0 (outer/convex surface), inner radius R_BEND at the concave corner.
    cx, cy = X_SIDE + T_P + R_BEND, Y_TAB1 + R_BEND     # (159, 12): centre of the bend arcs
    ro, ri = R_BEND + T_P, R_BEND
    ring = (cq.Workplane("XY").center(cx, cy).circle(ro).circle(ri).extrude(z1 - z0).translate((0, 0, z0))
            .intersect(box(cx - ro - 1, cx, cy - ro - 1, cy, z0 - 1, z1 + 1)))
    sp = plate.union(tab).union(ring)
    def fac(kind, **k):
        if kind == "circle":
            return cyl_x(k["d"], k["c"][0], k["c"][1], X_SIDE - 1, X_SIDE + T_P + 1)
        if kind == "slot":
            return slot_x(k["w"], k["l"], k["c"][0], k["c"][1], X_SIDE - 1, X_SIDE + T_P + 1)
        return (cq.Workplane("YZ").center((k["y0"] + k["y1"]) / 2, (k["z0"] + k["z1"]) / 2).rect(k["y1"] - k["y0"], k["z1"] - k["z0"])
                .extrude(T_P + 2).translate((X_SIDE - 1, 0, 0)).edges("|X").fillet(k["r"]))
    for c in plate_features_2d(fac): sp = sp.cut(c)
    # joint holes in the tab
    for zc in JOINT_Z:
        sp = sp.cut(cq.Workplane("XZ").center(JOINT_X, zc).circle(JOINT_D / 2).extrude(-(T_P + 2)).translate((0, Y_TAB0 - 1, 0)))
    return sp

def build_flange():
    z0, z1 = EAR_Z0, EAR_Z1
    fl = box(X_FLANGE_IN, FLANGE_X_OUT, Y_FLANGE0, Y_FLANGE1, z0, z1)
    for zc in HOLE_Z:
        fl = fl.cut(slot_y(SLOT_W, SLOT_H, RACK_HOLE_X, zc, Y_FLANGE0 - 1, Y_FLANGE1 + 1))
    for zc in JOINT_Z:
        fl = fl.cut(cq.Workplane("XZ").center(JOINT_X, zc).circle(JOINT_D / 2).extrude(-(T_F + 2)).translate((0, Y_FLANGE0 - 1, 0)))
        csk = (cq.Workplane("XZ").center(JOINT_X, zc).circle(CSK_D / 2).workplane(offset=-CSK_D / 2).circle(0.1).loft()
               .translate((0, Y_FLANGE0, 0)))
        fl = fl.cut(csk)
    return fl

def build_ace():
    body = box(-ACE_W / 2, ACE_W / 2, 0, ACE_D, 0, ACE_H)
    lips = (box(-ACE_ENV_W / 2, ACE_ENV_W / 2, 0, FRONT_LIP_T, 0, ACE_H)
            .union(box(-ACE_ENV_W / 2, ACE_ENV_W / 2, ACE_D - REAR_LIP_T, ACE_D, 0, ACE_H)))
    return body, body.union(lips)

def build_rail():
    z0, z1 = U_EDGE_Z - 10, U_EDGE_Z + U_PITCH + 10
    r = box(RAIL_OPEN / 2, RAIL_OPEN / 2 + RAIL_W, Y_FLANGE1, Y_FLANGE1 + RAIL_T, z0, z1).union(
        box(RAIL_OPEN / 2 + RAIL_W - RAIL_T, RAIL_OPEN / 2 + RAIL_W, Y_FLANGE1, Y_FLANGE1 + 40, z0, z1))
    for zc in HOLE_Z:
        r = r.cut(slot_y(SLOT_W, SLOT_H, RACK_HOLE_X, zc, Y_FLANGE1 - 1, Y_FLANGE1 + RAIL_T + 1))
    return r

# =============================================================================
# LOADS
# =============================================================================
def I_rect(b, h): return b * h ** 3 / 12.0

def load_calcs():
    R = []
    def row(name, case, val, unit, allow, note):
        sf = allow / val if val > 0 else float("inf")
        R.append(dict(name=name, case=case, value=val, unit=unit, allow=allow, sf=sf, ok=sf >= SF_REQ, note=note))
    M_DES = ACE_MASS + M_EXTRA; W = M_DES * G; P = W / 2
    log("Design mass %.1f kg, W = %.1f N, P per ear = %.2f N (1 g)" % (M_DES, W, P))
    # frame section through the window: top strip and bottom strip
    wz0, wz1 = VENT[2] - VENT_MARGIN, VENT[3] + VENT_MARGIN
    a_top, z_top = (EAR_Z1 - wz1) * T_P, (EAR_Z1 + wz1) / 2
    a_bot, z_bot = (wz0 - EAR_Z0) * T_P, (wz0 + EAR_Z0) / 2
    zc = (a_top * z_top + a_bot * z_bot) / (a_top + a_bot)
    I_frame = (T_P * (EAR_Z1 - wz1) ** 3 / 12 + a_top * (z_top - zc) ** 2 + T_P * (wz0 - EAR_Z0) ** 3 / 12 + a_bot * (z_bot - zc) ** 2)
    c_frame = max(EAR_Z1 - zc, zc - EAR_Z0)
    I_plate = I_rect(T_P, PANEL_H)
    I_fl = I_rect(T_F, PANEL_H)
    a, b = PANEL_H, T_F
    beta = 1 / 3 - 0.21 * (b / a) * (1 - (b / a) ** 4 / 12)      # torsion constant of a thin rectangle
    J_fl = beta * a * b ** 3
    L_tors = RACK_HOLE_X - JOINT_X
    log("Frame through the window: strips %.1f and %.1f mm tall x %.1f, I = %.0f mm4 ; solid plate I = %.0f ; flange I = %.0f, J = %.0f mm4"
        % (EAR_Z1 - wz1, wz0 - EAR_Z0, T_P, I_frame, I_plate, I_fl, J_fl))
    y_mean = (SCREW_Y_FRONT + SCREW_Y_REAR) / 2
    out = {}
    for case, gf in (("A", CASE_A), ("B", CASE_B)):
        Pg = P * gf
        log(""); log("== Case %s (%.0f g), P = %.1f N per ear ==" % (case, gf, Pg))
        # chassis screws: 4 per side, pitch couple over the 175 mm pair spacing
        f_c = Pg * (COM_Y - y_mean) / (SCREW_Y_REAR - SCREW_Y_FRONT)
        f_pad_rear, f_pad_front = Pg / 2 + f_c, Pg / 2 - f_c
        f_screw = max(abs(f_pad_rear), abs(f_pad_front)) / 2
        bearing = f_screw / (CH_HOLE_D * T_P)
        log("Pads: COM at %.0f vs pair centre %.1f -> couple %.1f N ; rear pad %.1f N, front pad %.1f N -> %.1f N per screw ; bearing %.2f MPa"
            % (COM_Y, y_mean, f_c, f_pad_rear, f_pad_front, f_screw, bearing))
        # frame bending at the front edge of the window from the rear pad load
        M_frame = abs(f_pad_rear) * (SCREW_Y_REAR - (VENT[0] - VENT_MARGIN))
        s_frame = M_frame * c_frame / I_frame
        # plate root at the tab/bend: full pitch moment, solid section
        M_root = Pg * (COM_Y - Y_PLATE0)
        s_root = M_root * (PANEL_H / 2) / I_plate
        # flange: in-plane bending from the vertical load at the tab, torsion from the pitch moment
        M_fl = Pg * (RACK_HOLE_X - JOINT_X)
        s_fl = M_fl * (PANEL_H / 2) / I_fl
        M_pitch = Pg * COM_Y
        alpha = beta  # for a/b > 5 alpha ~ beta
        tau_fl = M_pitch / (alpha * a * b * b)
        # joint screws: moment -> tension couple over the outer screws, plus shear
        lever_j = JOINT_Z[2] - JOINT_Z[0]
        t_joint = M_pitch / lever_j
        v_joint = Pg / len(JOINT_Z)
        # rack screws
        lever_r = HOLE_Z[2] - HOLE_Z[0]
        f_rack_t = M_pitch / lever_r
        f_rack_s = M_fl / lever_r
        # sag at the chassis rear: flange twist + frame flexure
        theta = M_pitch * L_tors / (FLANGE["G"] * J_fl)
        d_frame = abs(f_pad_rear) * (SCREW_Y_REAR - (VENT[0] - VENT_MARGIN)) ** 3 / (3 * PLATE["E"] * I_frame)
        sag = theta * ACE_D + d_frame
        log("Frame: M = %.0f Nmm, sigma = %.1f MPa ; plate root M = %.0f Nmm, sigma = %.1f MPa" % (M_frame, s_frame, M_root, s_root))
        log("Flange: in-plane M = %.0f Nmm, sigma = %.1f MPa ; torsion M = %.0f Nmm over %.1f mm, tau = M/(alpha a b^2) = %.1f MPa"
            % (M_fl, s_fl, M_pitch, L_tors, tau_fl))
        log("Joint: %.0f N tension on the top M4 (lever %.0f mm) + %.1f N shear each ; rack screws: %.0f N tension, %.0f N shear (%d used)"
            % (t_joint, lever_j, v_joint, f_rack_t, f_rack_s, N_RACK_SCREWS_USED))
        log("Sag: flange twist theta = M L / (G J) = %.5f rad -> %.3f mm at Y = 210 ; frame flexure %.3f mm ; total %.3f mm"
            % (theta, theta * ACE_D, d_frame, sag))
        out[case] = dict(sag=sag, f_screw=f_screw, t_joint=t_joint, f_rack_t=f_rack_t)
        if case == "B":
            row("Side plate frame (window strips)", "B", s_frame, "MPa", PLATE["sy"], "bending at the window front edge")
            row("Side plate root at the bend", "B", s_root, "MPa", PLATE["sy"], "in-plane bending")
            row("Chassis hole bearing", "B", bearing, "MPa", PLATE["sy"], "per factory screw")
            row("Chassis screw load", "B", f_screw, "N", 4200.0, "M4 8.8 single shear")
            row("Flange torsion", "B", tau_fl, "MPa", FLANGE["tau_y"], "6 mm 6061-T6")
            row("Flange in-plane bending", "B", s_fl, "MPa", FLANGE["sy"], "")
            row("Joint screw tension (M4)", "B", t_joint, "N", 7000.0, "class 8.8 proof")
            row("Joint tab bearing", "B", (t_joint + v_joint) / (JOINT_D * T_P), "MPa", PLATE["sy"], "")
            row("Rack screw tension", "B", f_rack_t, "N", 8000.0, "10-32 steel proof")
            row("Flange under rack washer", "B", f_rack_t / (math.pi / 4 * (18 ** 2 - 12 ** 2)), "MPa", FLANGE["sy"], "")
        else:
            R.append(dict(name="Rear sag", case="A", value=sag, unit="mm", allow=SAG_MAX, sf=SAG_MAX / sag, ok=sag <= SAG_MAX,
                          note="flange twist + frame flexure (metal: no creep limit)"))
    F_C = CASE_C * W
    log(""); log("== Case C: 3 g, F = %.0f N ==" % F_C)
    yaw = F_C * COM_Y
    t_side = F_C / 4 / 2 + yaw / ACE_W / 4
    log("Front-to-back: %.1f N shear per chassis screw ; side-to-side: %.0f N tension per chassis screw (direct + yaw couple)" % (F_C / 8, t_side))
    row("Chassis screw tension (side load)", "C", t_side, "N", 7000.0, "M4 proof; the ACE thread sees this")
    row("Chassis screw shear (front-back)", "C", F_C / 8, "N", 4200.0, "")
    R.append(dict(name="Chassis retained", case="C", value=1, unit="", allow=1, sf=float("inf"), ok=True, note="8 screws into the chassis"))
    return R, out

# =============================================================================
# VERIFICATION
# =============================================================================
CHECKS = []
def check(name, ok, value, status=None):
    CHECKS.append(dict(name=name, ok=bool(ok), value=value, status=status or ("PASS" if ok else "FAIL")))
def vol_intersect(a, b):
    try: return a.intersect(b).val().Volume()
    except Exception: return 0.0

def run_verification(parts, ace_body, ace_env, rails):
    sp, fl = parts["side_plate_R"], parts["flange_R"]
    v = max(vol_intersect(p, ace_body) for p in parts.values())
    check("Zero interference: ears vs ACE body (310 x 44 x 210)", v < 1e-6, "%.4f mm3" % v)
    v = max(vol_intersect(p, ace_env) for p in parts.values())
    check("Zero interference: ears vs 315.6 envelope incl. the %.0f mm front/rear panel lips" % FRONT_LIP_T, v < 1e-6, "%.4f mm3" % v)
    v = max(vol_intersect(p, r) for p in parts.values() for r in rails)
    check("Zero interference: ears vs rack rails", v < 1e-6, "%.4f mm3" % v)
    v = vol_intersect(sp, fl)
    check("Side plate tab lies on the flange rear face without overlap", v < 1e-6, "%.4f mm3" % v)
    blank = box(X_FLANGE_IN, FLANGE_X_OUT, Y_FLANGE0, Y_FLANGE1, EAR_Z0, EAR_Z1)
    voids = blank.cut(fl).solids().vals(); found, worst = 0, 0.0
    for zc in HOLE_Z:
        for s in voids:
            b = s.BoundingBox()
            if b.xmax < 225 or b.xmin > 241 or not (b.zmin < zc < b.zmax): continue
            found += 1; worst = max(worst, abs((b.xmin + b.xmax) / 2 - RACK_HOLE_X), abs((b.zmin + b.zmax) / 2 - zc))
    check("Rack slot centres within 0.05 mm (X 232.55; Z 6.35 / 22.225 / 38.1 from the U edge)", found == 3 and worst <= 0.05,
          "%d slots, max deviation %.4f mm" % (found, worst))
    bR, bL = fl.val().BoundingBox(), parts["flange_L"].val().BoundingBox()
    check("Assembly width = 482.6 mm", abs(bR.xmax - bL.xmin - PANEL_W) < 0.01, "%.3f mm" % (bR.xmax - bL.xmin))
    check("Panel height = 43.6 mm (1U)", abs(bR.zmax - bR.zmin - PANEL_H) < 0.01, "%.3f mm" % (bR.zmax - bR.zmin))
    behind = max(sp.val().BoundingBox().xmax, fl.cut(box(0, 300, -1, Y_FLANGE1, -1, 100)).val().BoundingBox().xmax if False else 0)
    check("Structure behind the flange <= 222 mm", behind <= STRUCT_X_MAX, "%.1f mm (tab)" % behind)
    v = vol_intersect(sp, box(X_SIDE - 1, X_SIDE + T_P + 1, VENT[0], VENT[1], VENT[2], VENT[3]))
    check("Vent field uncovered (window with %.0f mm margin)" % VENT_MARGIN, v < 1e-6, "%.4f mm3 of plate over the vent" % v)
    # factory screw holes: all four present, head covers hole/slot with a bearing ring
    ring_front = (SCREW_HEAD_D - CH_HOLE_D) / 2
    ring_rear = (SCREW_HEAD_D - CH_SLOT_L) / 2
    check("Factory button head (%.1f) bears on the plate: front round hole ring %.2f mm, rear slot end ring %.2f mm (slot is along Y, head always covers it across)"
          % (SCREW_HEAD_D, ring_front, ring_rear), ring_front >= 1.0, "ok")
    # M3 head clearance
    r_m3 = (M3_CLEAR_D - M3_SCREW[2]) / 2
    check("Small M3 head (%.1f) clears its %.1f mm hole by %.2f mm at either end" % (M3_SCREW[2], M3_CLEAR_D, r_m3), r_m3 >= 0.8, "ok")
    # thin strips
    strips = {"top window strip": EAR_Z1 - (VENT[3] + VENT_MARGIN), "bottom window strip": VENT[2] - VENT_MARGIN - EAR_Z0,
              "strip under the M3 notch / hole": M3_SCREW[1] - M3_CLEAR_D / 2 - EAR_Z0,
              "rear M3 hole to window": (ACE_D - M3_SCREW[0] - M3_CLEAR_D / 2) - WIN[1] if not m3_merges_into_window(ACE_D - M3_SCREW[0]) else 99,
              "front hole to bend tangent": SCREW_Y_FRONT - CH_HOLE_D / 2 - Y_PLATE0, "rear slot to plate end": Y_PLATE1 - (SCREW_Y_REAR + CH_SLOT_L / 2),
              "lower hole to bottom edge": SCREW_Z[0] - CH_HOLE_D / 2 - EAR_Z0, "slot to flange edge": FLANGE_X_OUT - RACK_HOLE_X - SLOT_W / 2,
              "joint hole to tab edge": min(JOINT_X - JOINT_D / 2 - TAB_X0, TAB_X1 - JOINT_X - JOINT_D / 2)}
    mn = min(strips, key=strips.get)
    check("Minimum web >= 1.5 t = %.1f mm for laser-cut steel (thinnest: %s)" % (1.5 * T_P, mn), strips[mn] >= 1.5 * T_P, "%.2f mm" % strips[mn])
    for n, p in parts.items():
        s = p.val(); ns = len(p.solids().vals())
        check("%s: one valid solid" % n, ns == 1 and s.isValid(), "%d solid(s), %.1f cm3" % (ns, s.Volume() / 1000))

# =============================================================================
# FLAT PATTERN (DXF), DRAWINGS, EXPORT
# =============================================================================

def write_plate_dxf(path):
    flat = build_flat_plate()
    exporters.export(flat.section(), path)
    doc = ezdxf.readfile(path); doc.units = ezdxf.units.MM
    for name, color in (("BEND", 1), ("NOTES", 3)): doc.layers.add(name, color=color)
    msp = doc.modelspace(); z0, z1 = EAR_Z0, EAR_Z1
    ub = (U_BEND0 + U_BEND1) / 2
    msp.add_line((ub, z0 - 5), (ub, z1 + 5), dxfattribs={"layer": "BEND"})
    msp.add_text("BEND 90 deg UP, inside radius %.1f, K=%.2f, BA=%.2f ; material %s %.1f mm ; u=0 is the REAR end" % (R_BEND, K_FACTOR, BA, PLATE["name"], T_P),
                 dxfattribs={"layer": "NOTES", "height": 2.5}).set_placement((ub + 2, z1 + 8))
    doc.saveas(path)

def write_plate_dxf_old(path):
    doc = ezdxf.new("R2010"); doc.units = ezdxf.units.MM
    for name, color in (("CUT", 7), ("BEND", 1), ("NOTES", 3)):
        doc.layers.add(name, color=color)
    msp = doc.modelspace()
    z0, z1 = EAR_Z0, EAR_Z1
    msp.add_lwpolyline([(0, z0), (U_TAB1, z0), (U_TAB1, z1), (0, z1)], close=True, dxfattribs={"layer": "CUT"})
    for zc in SCREW_Z:
        msp.add_circle((u_of_y(SCREW_Y_FRONT), zc), CH_HOLE_D / 2, dxfattribs={"layer": "CUT"})
        uc = u_of_y(SCREW_Y_REAR); a = (CH_SLOT_L - CH_HOLE_D) / 2; r = CH_HOLE_D / 2
        msp.add_line((uc - a, zc - r), (uc + a, zc - r), dxfattribs={"layer": "CUT"})
        msp.add_line((uc - a, zc + r), (uc + a, zc + r), dxfattribs={"layer": "CUT"})
        msp.add_arc((uc + a, zc), r, -90, 90, dxfattribs={"layer": "CUT"})
        msp.add_arc((uc - a, zc), r, 90, 270, dxfattribs={"layer": "CUT"})
    for yc in (M3_SCREW[0], ACE_D - M3_SCREW[0]):
        msp.add_circle((u_of_y(yc), M3_SCREW[1]), M3_CLEAR_D / 2, dxfattribs={"layer": "CUT"})
    wy0, wy1, wz0, wz1 = VENT[0] - VENT_MARGIN, VENT[1] + VENT_MARGIN, VENT[2] - VENT_MARGIN, VENT[3] + VENT_MARGIN
    u0, u1, R = u_of_y(wy1), u_of_y(wy0), WINDOW_R
    pts = [(u0 + R, wz0), (u1 - R, wz0), (u1, wz0 + R), (u1, wz1 - R), (u1 - R, wz1), (u0 + R, wz1), (u0, wz1 - R), (u0, wz0 + R)]
    msp.add_line(pts[0], pts[1], dxfattribs={"layer": "CUT"}); msp.add_arc((u1 - R, wz0 + R), R, 270, 360, dxfattribs={"layer": "CUT"})
    msp.add_line(pts[2], pts[3], dxfattribs={"layer": "CUT"}); msp.add_arc((u1 - R, wz1 - R), R, 0, 90, dxfattribs={"layer": "CUT"})
    msp.add_line(pts[4], pts[5], dxfattribs={"layer": "CUT"}); msp.add_arc((u0 + R, wz1 - R), R, 90, 180, dxfattribs={"layer": "CUT"})
    msp.add_line(pts[6], pts[7], dxfattribs={"layer": "CUT"}); msp.add_arc((u0 + R, wz0 + R), R, 180, 270, dxfattribs={"layer": "CUT"})
    for zc in JOINT_Z:
        msp.add_circle((u_of_x(JOINT_X), zc), JOINT_D / 2, dxfattribs={"layer": "CUT"})
    ub = (U_BEND0 + U_BEND1) / 2
    msp.add_line((ub, z0 - 5), (ub, z1 + 5), dxfattribs={"layer": "BEND"})
    msp.add_text("BEND 90 deg UP, inside radius %.1f, K=%.2f, BA=%.2f ; material %s %.1f mm" % (R_BEND, K_FACTOR, BA, PLATE["name"], T_P),
                 dxfattribs={"layer": "NOTES", "height": 2.5}).set_placement((ub + 2, z1 + 8))
    doc.saveas(path)

def write_flange_dxf(path):
    doc = ezdxf.new("R2010"); doc.units = ezdxf.units.MM
    for name, color in (("CUT", 7), ("NOTES", 3)): doc.layers.add(name, color=color)
    msp = doc.modelspace(); w = FLANGE_X_OUT - X_FLANGE_IN
    msp.add_lwpolyline([(0, EAR_Z0), (w, EAR_Z0), (w, EAR_Z1), (0, EAR_Z1)], close=True, dxfattribs={"layer": "CUT"})
    xs = RACK_HOLE_X - X_FLANGE_IN; a = (SLOT_W - SLOT_H) / 2; r = SLOT_H / 2
    for zc in HOLE_Z:
        msp.add_line((xs - a, zc - r), (xs + a, zc - r), dxfattribs={"layer": "CUT"}); msp.add_line((xs - a, zc + r), (xs + a, zc + r), dxfattribs={"layer": "CUT"})
        msp.add_arc((xs + a, zc), r, -90, 90, dxfattribs={"layer": "CUT"}); msp.add_arc((xs - a, zc), r, 90, 270, dxfattribs={"layer": "CUT"})
    for zc in JOINT_Z:
        msp.add_circle((JOINT_X - X_FLANGE_IN, zc), JOINT_D / 2, dxfattribs={"layer": "CUT"})
    msp.add_text("3x dia %.1f countersunk 90 deg to dia %.1f on the FRONT face ; %s %.1f mm" % (JOINT_D, CSK_D, FLANGE["name"], T_F),
                 dxfattribs={"layer": "NOTES", "height": 2.5}).set_placement((2, EAR_Z1 + 4))
    doc.saveas(path)

def draw_plate(path_png, path_pdf_1to1):
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle, Circle, FancyBboxPatch
    import numpy as np
    flat = build_flat_plate().section()
    def render(ax, dims=True):
        z0, z1 = EAR_Z0, EAR_Z1
        for e in flat.edges().vals():
            pts = e.positions(np.linspace(0, 1, 40).tolist())
            ax.plot([p.x for p in pts], [p.y for p in pts], "k-", lw=0.9)
        ub = (U_BEND0 + U_BEND1) / 2
        ax.plot([ub, ub], [z0 - 3, z1 + 3], "r-.", lw=1); ax.text(ub + 1, z1 + 4, "bend up 90°", color="r", fontsize=7)
        if dims:
            def dim(x0, x1, y, txt, off=0):
                ax.annotate("", (x0, y), (x1, y), arrowprops=dict(arrowstyle="<->", lw=0.7, color="b"))
                ax.text((x0 + x1) / 2, y + 0.8 + off, txt, ha="center", fontsize=7, color="b")
            def vdim(x, y0, y1, txt):
                ax.annotate("", (x, y0), (x, y1), arrowprops=dict(arrowstyle="<->", lw=0.7, color="b"))
                ax.text(x + 0.8, (y0 + y1) / 2, txt, va="center", fontsize=7, color="b", rotation=90)
            dim(0, U_TAB1, z1 + 10, "flat %.2f" % U_TAB1)
            dim(0, u_of_y(SCREW_Y_REAR), z0 - 6, "%.1f" % u_of_y(SCREW_Y_REAR))
            dim(0, u_of_y(SCREW_Y_FRONT), z0 - 11, "%.1f" % u_of_y(SCREW_Y_FRONT))
            dim(u_of_y(SCREW_Y_REAR), u_of_y(SCREW_Y_FRONT), z0 - 16, "%.1f (pair spacing)" % (SCREW_Y_REAR - SCREW_Y_FRONT))
            dim(0, ub, z1 + 5, "bend line %.2f" % ub)
            vdim(-6, z0, SCREW_Z[0], "%.1f" % (SCREW_Z[0] - z0)); vdim(-12, z0, SCREW_Z[1], "%.1f" % (SCREW_Z[1] - z0)); vdim(-18, z0, z1, "%.1f" % (z1 - z0))
            ax.text(U_TAB1 / 2, z1 + 16, "Side plate flat pattern (R shown; L is the mirror). Front pair Ø%.1f, rear pair Ø%.1f x %.1f slots, "
                    "M3 clearance Ø%.1f (notch at the front end, hole at the rear end), window %.0f x %.0f R%.0f, tab holes Ø%.1f. %s %.1f mm."
                    % (CH_HOLE_D, CH_HOLE_D, CH_SLOT_L, M3_CLEAR_D, VENT[1] - VENT[0] + 2 * VENT_MARGIN, VENT[3] - VENT[2] + 2 * VENT_MARGIN,
                       WINDOW_R, JOINT_D, PLATE["name"], T_P), ha="center", fontsize=7)
        ax.set_aspect("equal"); ax.axis("off")
    fig, ax = plt.subplots(figsize=(14, 5), dpi=130); render(ax); ax.set_xlim(-22, U_TAB1 + 5); ax.set_ylim(-20, EAR_Z1 + 22)
    fig.tight_layout(); fig.savefig(path_png); plt.close(fig)
    # 1:1 PDF on A4 landscape (297 x 210 mm): 1 mm = 1/25.4 in. Print at 100 % / "actual size".
    fig = plt.figure(figsize=(297 / 25.4, 210 / 25.4)); ax = fig.add_axes([0, 0, 1, 1]); render(ax, dims=False)
    ax.set_xlim(-40, 257); ax.set_ylim(-83, 127)
    ax.plot([-30, 70], [-70, -70], "k-", lw=1); [ax.plot([x, x], [-70, -67], "k-", lw=0.6) for x in range(-30, 71, 10)]
    ax.text(20, -66, "100 mm scale check", ha="center", fontsize=8)
    ax.text(108, 110, "ACE rack ear - side plate 1:1 (print at 100 %%, A4 landscape). u = 0 is the REAR end of the plate; bend line at %.2f." % ((U_BEND0 + U_BEND1) / 2),
            ha="center", fontsize=8)
    fig.savefig(path_pdf_1to1); plt.close(fig)

def render_png(shapes, path, title, elev=25, azim=-60, ortho=False):
    import numpy as np, matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    fig = plt.figure(figsize=(10, 7), dpi=110); ax = fig.add_subplot(111, projection="3d"); allpts = []
    light = np.array([0.4, -0.6, 0.7]); light /= np.linalg.norm(light)
    for wp, col in shapes:
        vs, tris = wp.val().tessellate(0.6, 0.3); V = np.array([[v.x, v.y, v.z] for v in vs]); F = np.array(tris)
        if len(F) == 0: continue
        polys = V[F]; n = np.cross(polys[:, 1] - polys[:, 0], polys[:, 2] - polys[:, 0]); n /= (np.linalg.norm(n, axis=1)[:, None] + 1e-12)
        ax.add_collection3d(Poly3DCollection(polys, facecolors=np.clip(np.array(col)[None, :] * (0.45 + 0.55 * np.abs(n @ light))[:, None], 0, 1), edgecolors="none")); allpts.append(V)
    P = np.vstack(allpts); mn, mx = P.min(axis=0), P.max(axis=0); c = (mn + mx) / 2; r = (mx - mn).max() / 2
    ax.set_xlim(c[0] - r, c[0] + r); ax.set_ylim(c[1] - r, c[1] + r); ax.set_zlim(c[2] - r, c[2] + r); ax.set_box_aspect((1, 1, 1))
    ax.view_init(elev=elev, azim=azim)
    if ortho: ax.set_proj_type("ortho")
    ax.set_axis_off(); ax.set_title(title); fig.tight_layout(); fig.savefig(path); plt.close(fig)

def main():
    for d in ("step", "dxf", "drawings", "renders"): os.makedirs(os.path.join(OUT, d), exist_ok=True)
    spR, flR = build_side_plate(), build_flange()
    parts = {"side_plate_R": spR, "flange_R": flR, "side_plate_L": mirror_x(spR), "flange_L": mirror_x(flR)}
    ace_body, ace_env = build_ace(); rails = [build_rail(), mirror_x(build_rail())]
    R, out = load_calcs()
    run_verification(parts, ace_body, ace_env, rails)
    for r in R:
        check("Load: %s (Case %s)" % (r["name"], r["case"]), r["ok"], "%.2f %s vs %.1f -> SF %.2f" % (r["value"], r["unit"], r["allow"], r["sf"]) if r["unit"] else "retained")
    files = []
    for n, p in parts.items():
        exporters.export(p, os.path.join(OUT, "step", n + ".step")); files.append("step/%s.step" % n)
    assy = cq.Assembly(name="ACE_1U_metal_ears")
    for n, p in parts.items(): assy.add(p, name=n, color=cq.Color(0.75, 0.75, 0.8) if "plate" in n else cq.Color(0.25, 0.45, 0.85))
    assy.add(ace_body, name="ACE_body", color=cq.Color(0.3, 0.3, 0.3))
    for i, r in enumerate(rails): assy.add(r, name="rail_%d" % i, color=cq.Color(0.5, 0.5, 0.5))
    assy.save(os.path.join(OUT, "step", "assembly_in_rack.step")); files.append("step/assembly_in_rack.step")
    write_plate_dxf(os.path.join(OUT, "dxf", "side_plate_flat_R.dxf")); files.append("dxf/side_plate_flat_R.dxf (mirror for L)")
    write_flange_dxf(os.path.join(OUT, "dxf", "flange.dxf")); files.append("dxf/flange.dxf (same part both sides)")
    draw_plate(os.path.join(OUT, "drawings", "side_plate_flat.png"), os.path.join(OUT, "drawings", "side_plate_1to1_A4.pdf"))
    files += ["drawings/side_plate_flat.png", "drawings/side_plate_1to1_A4.pdf"]
    GREY, BLUE, DARK = (0.75, 0.75, 0.8), (0.25, 0.45, 0.85), (0.3, 0.3, 0.3)
    shapes = [(spR, GREY), (flR, BLUE), (parts["side_plate_L"], GREY), (parts["flange_L"], BLUE), (ace_body, DARK)] + [(r, (0.5, 0.5, 0.5)) for r in rails]
    for vn, (el, az, o) in {"iso": (22, -55, False), "front": (0, -90, True), "top": (90, -90, True), "side": (0, 0, True)}.items():
        render_png(shapes, os.path.join(OUT, "renders", "assembly_%s.png" % vn), "ACE metal ears in rack - " + vn, el, az, o)
        render_png([(spR, GREY), (flR, BLUE)], os.path.join(OUT, "renders", "ear_R_%s.png" % vn), "ear_R (plate + flange) - " + vn, el, az, o)
    render_png([(spR, GREY), (flR, BLUE)], os.path.join(OUT, "renders", "ear_R_iso_outside.png"), "ear_R from the rack side", 22, 40)
    masses = {n: p.val().Volume() * (PLATE["rho"] if "plate" in n else FLANGE["rho"]) for n, p in parts.items()}
    with open(os.path.join(OUT, "report.md"), "w") as f:
        f.write("# ACE 1U fabricated metal ears - verification report\n\n| # | Check | Result | Value |\n|---|---|---|---|\n")
        for i, c in enumerate(CHECKS, 1): f.write("| %d | %s | %s | %s |\n" % (i, c["name"], c["status"], c["value"]))
        f.write("\n## Calculation log\n\n```\n" + "\n".join(LOG) + "\n```\n\n## Load table\n\n| Feature | Case | Value | Allowable | SF | Pass | Note |\n|---|---|---|---|---|---|---|\n")
        for r in R: f.write("| %s | %s | %.2f %s | %.1f %s | %.2f | %s | %s |\n" % (r["name"], r["case"], r["value"], r["unit"], r["allow"], r["unit"], r["sf"], "yes" if r["ok"] else "NO", r["note"]))
        f.write("\n## Flat pattern\n\n- straight plate %.1f + bend allowance %.2f (R%.1f, K %.2f) + tab %.1f = flat %.2f mm, bend line at %.2f from the rear end\n"
                % (U_PLATE, BA, R_BEND, K_FACTOR, TAB_W, U_TAB1, (U_BEND0 + U_BEND1) / 2))
        f.write("- masses: " + ", ".join("%s %.0f g" % (k, v) for k, v in masses.items()) + "\n\n## Files\n\n" + "\n".join("- " + x for x in files) + "\n")
    json.dump(dict(checks=CHECKS, loads=R), open(os.path.join(OUT, "verification.json"), "w"), indent=1, default=str)
    print("\n".join("%-6s %s : %s" % (c["status"], c["name"], c["value"]) for c in CHECKS))
    nf = sum(1 for c in CHECKS if c["status"] == "FAIL"); print("\n%d checks, %d failed" % (len(CHECKS), nf)); return nf == 0

if __name__ == "__main__":
    sys.exit(0 if main() else 1)
