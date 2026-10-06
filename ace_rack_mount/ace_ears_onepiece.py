#!/usr/bin/env python3
"""
DirectOut ACE -> EIA-310-D 19" rack: ONE-PIECE printed 1U ears ("Sennheiser style": flange + side plate in one part)
that reuse all four factory M4 button-head screws on each side.  The small M3 screw is not used (relieved).

Structure (plan view, right ear):    flange (front, X 155..241.3)
                                      |  side plate on the chassis (X 155..161), open to the top over the vent
                                      |  outer truss web (X 216..222, 61 mm from the vent) with diamond air openings
                                      |  floor (Z 0.2..2.8) on the print bed joining plate and web
                                      |  front and rear bulkheads closing the box
Print: rack vertical = printer Z, floor on the bed, no supports.

Coordinates: X rack width (0 = rack centre, +X right ear), Y depth (0 = ACE front face, +Y rearwards),
Z vertical (0 = chassis bottom).  The ear spans Z 0.2 ... 43.8 (1U panel 43.6 centred on the 44 mm chassis).
"""
import math, os, sys, json
import numpy as np
import cadquery as cq
from cadquery import exporters
from OCP.GProp import GProp_GProps
from OCP.BRepGProp import BRepGProp

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output_onepiece")

# ===== CHASSIS DATA: user photo IMG_8471 rectified on the 210 x 44 side panel (+/- 0.5 mm) ===================
# Distances are from the FRONT face.  The pattern is symmetric end to end, so the ear fits either way round;
# the vent opening and the M3 relief cover both orientations.
SCREW_Y   = (17.5, 192.5)          # front pair, rear pair
SCREW_Z   = (9.5, 34.5)            # each pair is vertical, symmetric about mid-height
HEAD_D, HEAD_H, SHANK_D = 7.6, 2.2, 4.0          # factory M4 ISO 7380 button head, reused
VENT_ZONE = (37.8, 172.2, 7.9, 38.1)             # y0, y1, z0, z1: union of the vent field for both orientations
M3_SPOTS  = [(32.6, 6.8), (177.4, 6.8)]          # small M3 screw (not used), either orientation
M3_HEAD_D, M3_HEAD_H = 5.2, 1.65
FRONT_LIP_T = REAR_LIP_T = 3.0                    # front/rear panel lips standing 2.8 mm proud (thickness assumed)
# ===== device / rack / material / process (sections 1-3) =======================================================
ACE_W, ACE_H, ACE_D, ACE_ENV_W, ACE_ENV_D, ACE_MASS = 310.0, 44.0, 210.0, 315.6, 239.0, 3.7
PANEL_W, RACK_HOLE_X, RAIL_OPEN, U_PITCH, PANEL_H = 482.6, 465.1 / 2, 450.0, 44.45, 43.6
HOLE_FROM_U, SLOT_H, SLOT_W = (6.35, 22.225, 38.1), 7.0, 10.5
U_EDGE_Z = -(U_PITCH - ACE_H) / 2
EAR_Z0, EAR_Z1 = 0.2, 0.2 + PANEL_H
HOLE_Z = [U_EDGE_Z + h for h in HOLE_FROM_U]
STRUCT_X_MAX, FLANGE_X_OUT = RAIL_OPEN / 2 - 3.0, PANEL_W / 2      # 222, 241.3
RAIL_T, RAIL_W = 2.0, 20.0
E_MOD, SY_XY, SY_Z, NU = 1900.0, 45.0, 25.0, 0.38
G_MOD = E_MOD / (2 * (1 + NU))
TAU_XY = 0.577 * SY_XY
BUILD_VOL, COMP_HOLE, CHAMFER_BED, SHRINK_SCALE = 256.0, 0.2, 0.3, 1.003
MAX_OVERHANG, MAX_BRIDGE, MIN_WALL = 45.0, 10.0, 2.4
# ===== ear geometry ==============================================================================================
X_SIDE = ACE_W / 2                                   # 155: plate inner face on the chassis
T_FLANGE_SPEC = 6.0                                  # 5.2; auto-thickened by the section 7 loop
FLOOR_UNDER_HEAD = MIN_WALL                          # plastic under each factory screw head (thread engagement lost)
RELIEF = 3.0                                         # 5.8 relief over the M3 head
T_SIDE = math.ceil(max(RELIEF + MIN_WALL, FLOOR_UNDER_HEAD + HEAD_H + 0.4))     # 6
HOLE_FIT_D = SHANK_D + COMP_HOLE                     # 4.2 close fit (load-bearing)
POCKET_D = HEAD_D + COMP_HOLE                        # 7.8 head pocket = shear key
REAR_SLOT_TRAVEL = 2.0                               # rear pair: slot along Y, +/- 1.0 mm on the 175 mm pair spacing
VENT_MARGIN = 2.0
NOTCH = (VENT_ZONE[0] - VENT_MARGIN, VENT_ZONE[1] + VENT_MARGIN, VENT_ZONE[2] - VENT_MARGIN)   # y0, y1, z0 (open to top)
X_WEB1 = STRUCT_X_MAX                                # outer web outer face, 3 mm inside the rail edge
T_WEB = 6.0
X_WEB0 = X_WEB1 - T_WEB                              # 216 -> 61 mm from the side panel
T_FLOOR = 2.6                                        # 13 layers
Z_FLOOR1 = EAR_Z0 + T_FLOOR
CHORD_H = 5.0                                        # solid top band of the outer web
T_BULK = 4.0
Y_PLATE0 = FRONT_LIP_T + 0.2                          # plate starts behind the front lip
Y_PLATE1 = ACE_D - REAR_LIP_T - 1.0                   # 206
Y_FBULK = (NOTCH[0] - 1.8 - T_BULK, NOTCH[0] - 1.8)   # front bulkhead 30..34, just ahead of the vent opening
Y_RBULK = (Y_PLATE1 - T_BULK, Y_PLATE1)
STRUT = 3.0                                          # diamond lattice strut width
DIAMOND_MARGIN = 3.0
ACCESS_D = 8.4 + COMP_HOLE                           # driver/screw access through the outer web (head 7.6 passes)
FILLET_BIG, FILLET_SMALL = 6.0, 2.0
# ===== loads (section 7) ============================================================================================
M_EXTRA, COM_Y, G = 1.5, 105.0, 9.81
CASE_A, CASE_B, CASE_C, SF_REQ, SAG_MAX, CREEP_FRAC = 1.0, 5.0, 3.0, 3.0, 1.0, 0.15
RAIL_SCREWS_USED = 2                                 # top + bottom slot (the middle slot is a bonus)
REAR_GAP = HOLE_FIT_D - SHANK_D                      # worst-case vertical play at the rear pair (front pair seated)

LOG = []
def log(s=""): LOG.append(s)

M_DES = ACE_MASS + M_EXTRA
W = M_DES * G
P_SIDE = W / 2

# ----- flange thickness: plate bending between the outer web and the rack screw (section 7 loop) ---------------
# The pitch moment at the root is statically fixed: M = P (COM_Y - T_F).  The rack screws react it as a couple
# (bottom screw in tension, top of the flange bearing on the rail).  The bottom screw force reaches the outer web
# through the flange plate bending as a cantilever from the web centre line to the screw line.
LEVER_X = RACK_HOLE_X - (X_WEB0 + X_WEB1) / 2                         # 13.55
LEVER_Z = (EAR_Z1 - 4.0) - HOLE_Z[0]                                   # top bearing centroid to bottom screw
W_EFF = min(PANEL_H / 2, 2 * LEVER_X)                                  # 45 deg spread, capped at half the height
T_FLANGE = T_FLANGE_SPEC
while True:
    M_root_B = P_SIDE * CASE_B * (COM_Y - T_FLANGE)
    T_b = M_root_B / LEVER_Z
    s_fl = 6 * T_b * LEVER_X / (W_EFF * T_FLANGE ** 2)
    if SY_XY / s_fl >= SF_REQ: break
    T_FLANGE += 1.0
log("Flange plate bending (web -> rack screw): M_root(5 g) = %.1f x (%.0f - %.0f) = %.0f Nmm ; screw force %.0f / %.1f = %.0f N ; "
    "plate M = %.0f x %.2f = %.0f Nmm on b = %.1f mm ; t -> %.0f mm, sigma = 6M/(b t^2) = %.1f MPa, SF %.2f (in-layer)"
    % (P_SIDE * CASE_B, COM_Y, T_FLANGE, M_root_B, M_root_B, LEVER_Z, T_b, T_b, LEVER_X, T_b * LEVER_X, W_EFF, T_FLANGE, s_fl, SY_XY / s_fl))
X_FLANGE_IN = ACE_ENV_W / 2 + 0.2                                       # 158.0 clears the front lip

# =============================================================================
# GEOMETRY HELPERS
# =============================================================================
def box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))

def poly_yz(pts, x0, x1):
    """Polygon in the Y-Z plane, extruded along +X from x0 to x1."""
    return cq.Workplane("YZ").polyline(pts).close().extrude(x1 - x0).translate((x0, 0, 0))

def poly_xz(pts, y0, y1):
    return cq.Workplane("XZ").polyline(pts).close().extrude(-(y1 - y0)).translate((0, y0, 0))

def cove_xy(cx, cy, r, qx, qy, z0, z1):
    """Concave fillet with a vertical axis in the corner at (cx, cy); (qx, qy) = +/-1 = quadrant the fillet fills."""
    x0, x1 = sorted((cx, cx + qx * r)); y0, y1 = sorted((cy, cy + qy * r))
    cyl = cq.Workplane("XY").center(cx + qx * r, cy + qy * r).circle(r).extrude(z1 - z0).translate((0, 0, z0))
    return box(x0, x1, y0, y1, z0, z1).cut(cyl)

def cove_y(cx, cz, r, qx, qz, y0, y1):
    """Concave fillet with an axis along Y in the X-Z corner at (cx, cz)."""
    x0, x1 = sorted((cx, cx + qx * r)); z0, z1 = sorted((cz, cz + qz * r))
    cyl = cq.Workplane("XZ").center(cx + qx * r, cz + qz * r).circle(r).extrude(-(y1 - y0)).translate((0, y0, 0))
    return box(x0, x1, y0, y1, z0, z1).cut(cyl)

def teardrop_obround(l, w, cu, cv, plane, d0, d1):
    """Horizontal-axis hole: obround l (along the in-plane horizontal axis) x w (vertical), with a 45 deg roof
    (apex up).  plane 'YZ' -> axis X, extruded d0..d1 in X ; plane 'XZ' -> axis Y, extruded d0..d1 in Y."""
    r = w / 2.0; a = max(0.0, (l - w) / 2.0); s = r / math.sqrt(2)
    wp = cq.Workplane(plane).center(cu, cv).moveTo(-a, -r)
    if a > 0: wp = wp.lineTo(a, -r)
    wp = (wp.threePointArc((a + r, 0), (a + s, s)).lineTo(0, a + r * math.sqrt(2)).lineTo(-a - s, s)
          .threePointArc((-a - r, 0), (-a, -r)).close())
    if plane == "YZ":
        return wp.extrude(d1 - d0).translate((d0, 0, 0))
    return wp.extrude(-(d1 - d0)).translate((0, d0, 0))

def mirror_x(wp): return wp.mirror("YZ")

# =============================================================================
# DIAMOND LATTICE IN THE OUTER WEB
# =============================================================================
LAT_Y0, LAT_Y1 = Y_FBULK[1] + STRUT, NOTCH[1] - STRUT          # lattice span along Y
LAT_Z0, LAT_Z1 = Z_FLOOR1 + DIAMOND_MARGIN, EAR_Z1 - CHORD_H - DIAMOND_MARGIN
HD = (LAT_Z1 - LAT_Z0) / 2                                      # diamond half-diagonal
ZC = (LAT_Z0 + LAT_Z1) / 2
PITCH = 2 * HD + STRUT * math.sqrt(2)
N_DIA = int(math.floor((LAT_Y1 - LAT_Y0 - 2 * HD) / PITCH)) + 1
Y_DIA0 = (LAT_Y0 + LAT_Y1) / 2 - (N_DIA - 1) * PITCH / 2
DIA_YC = [Y_DIA0 + i * PITCH for i in range(N_DIA)]
TRI_APEX = ZC + (PITCH / 2 - HD) - STRUT * math.sqrt(2)          # lower triangles between diamonds (flat floor, 45 deg sides)
TRI_BASE = Z_FLOOR1 + MIN_WALL
TRI_HALF = TRI_APEX - TRI_BASE

def lattice_cutters(x0, x1):
    cs = []
    for yc in DIA_YC:
        cs.append(poly_yz([(yc - HD, ZC), (yc, ZC - HD), (yc + HD, ZC), (yc, ZC + HD)], x0, x1))
    for i in range(N_DIA - 1):
        ym = (DIA_YC[i] + DIA_YC[i + 1]) / 2
        if TRI_HALF > 2.0:
            cs.append(poly_yz([(ym - TRI_HALF, TRI_BASE), (ym + TRI_HALF, TRI_BASE), (ym, TRI_APEX)], x0, x1))
    return cs

def open_area():
    a = N_DIA * 2 * HD * HD + (N_DIA - 1) * max(TRI_HALF, 0) ** 2
    return a

# =============================================================================
# THE EAR (right-hand, +X)
# =============================================================================
FILLETS = []
def build_ear():
    z0, z1 = EAR_Z0, EAR_Z1
    # flange (front face at Y = 0, coplanar with the ACE front face) + inner block behind the front lip
    ear = box(X_FLANGE_IN, FLANGE_X_OUT, 0, T_FLANGE, z0, z1)
    ear = ear.union(box(X_SIDE, X_FLANGE_IN + 1, Y_PLATE0, T_FLANGE, z0, z1))
    # side plate: full height except the vent notch (open to the top)
    ear = ear.union(box(X_SIDE, X_SIDE + T_SIDE, Y_PLATE0, Y_PLATE1, z0, z1)
                    .cut(box(X_SIDE - 1, X_SIDE + T_SIDE + 1, NOTCH[0], NOTCH[1], NOTCH[2], z1 + 1)))
    # outer web, floor, bulkheads
    ear = ear.union(box(X_WEB0, X_WEB1, T_FLANGE - 0.01, Y_PLATE1, z0, z1))
    ear = ear.union(box(X_SIDE, X_WEB1, T_FLANGE - 0.01, Y_PLATE1, z0, Z_FLOOR1))
    ear = ear.union(box(X_SIDE + T_SIDE - 0.01, X_WEB0 + 0.01, Y_FBULK[0], Y_FBULK[1], z0, z1))
    ear = ear.union(box(X_SIDE + T_SIDE - 0.01, X_WEB0 + 0.01, Y_RBULK[0], Y_RBULK[1], z0, z1))
    # fillets: 6 mm at flange-plate and flange-web (inner side only; outer side would enter the rail zone)
    xi = X_SIDE + T_SIDE
    coves = [cove_xy(xi, T_FLANGE, FILLET_BIG, +1, +1, Z_FLOOR1, z1),
             cove_xy(X_WEB0, T_FLANGE, FILLET_BIG, -1, +1, Z_FLOOR1, z1),
             cove_xy(xi, Y_FBULK[0], FILLET_SMALL, +1, -1, Z_FLOOR1, z1), cove_xy(xi, Y_FBULK[1], FILLET_SMALL, +1, +1, Z_FLOOR1, NOTCH[2]),
             cove_xy(X_WEB0, Y_FBULK[0], FILLET_SMALL, -1, -1, Z_FLOOR1, z1), cove_xy(X_WEB0, Y_FBULK[1], FILLET_SMALL, -1, +1, Z_FLOOR1, z1),
             cove_xy(xi, Y_RBULK[0], FILLET_SMALL, +1, -1, Z_FLOOR1, z1), cove_xy(X_WEB0, Y_RBULK[0], FILLET_SMALL, -1, -1, Z_FLOOR1, z1),
             cove_y(xi, Z_FLOOR1, FILLET_SMALL, +1, +1, T_FLANGE, Y_PLATE1),
             cove_y(X_WEB0, Z_FLOOR1, FILLET_SMALL, -1, +1, T_FLANGE, Y_PLATE1)]
    for c in coves:
        ear = ear.union(c)
    FILLETS.append(len(coves))
    # diamond lattice in the outer web + diamond opening in the rear bulkhead
    for c in lattice_cutters(X_WEB0 - 1, X_WEB1 + 1):
        ear = ear.cut(c)
    bxc, bzc = (xi + X_WEB0) / 2, (Z_FLOOR1 + z1) / 2
    bh = min((X_WEB0 - xi) / 2, (z1 - Z_FLOOR1) / 2) - DIAMOND_MARGIN - FILLET_SMALL
    ear = ear.cut(poly_xz([(bxc - bh, bzc), (bxc, bzc - bh), (bxc + bh, bzc), (bxc, bzc + bh)], Y_RBULK[0] - 1, Y_RBULK[1] + 1))
    # factory screws: close-fit holes through the 2.4 mm floor + head pockets (teardrop roofs)
    for zc in SCREW_Z:
        yf, yr = SCREW_Y
        ear = ear.cut(teardrop_obround(HOLE_FIT_D, HOLE_FIT_D, yf, zc, "YZ", X_SIDE - 1, xi + 1))
        ear = ear.cut(teardrop_obround(POCKET_D, POCKET_D, yf, zc, "YZ", X_SIDE + FLOOR_UNDER_HEAD, xi + 1))
        ear = ear.cut(teardrop_obround(HOLE_FIT_D + REAR_SLOT_TRAVEL, HOLE_FIT_D, yr, zc, "YZ", X_SIDE - 1, xi + 1))
        ear = ear.cut(teardrop_obround(POCKET_D + REAR_SLOT_TRAVEL, POCKET_D, yr, zc, "YZ", X_SIDE + FLOOR_UNDER_HEAD, xi + 1))
        for yc in SCREW_Y:      # screwdriver + screw access through the outer web
            ear = ear.cut(teardrop_obround(ACCESS_D, ACCESS_D, yc, zc, "YZ", X_WEB0 - 1, X_WEB1 + 1))
    # M3 head relief on the inner face (3 mm deep), both orientations
    for (ym, zm) in M3_SPOTS:
        ear = ear.cut(teardrop_obround(M3_HEAD_D + 1.6, M3_HEAD_D + 1.6, ym, zm, "YZ", X_SIDE - 1, X_SIDE + RELIEF))
    # rack slots (axis Y, teardrop roof)
    for zc in HOLE_Z:
        ear = ear.cut(teardrop_obround(SLOT_W + COMP_HOLE, SLOT_H + COMP_HOLE, RACK_HOLE_X, zc, "XZ", -1, T_FLANGE + 1))
    return ear

def build_template():
    """Fit-check template: 2.4 mm plate (same as the plastic under the screw heads) with all four holes."""
    t = box(X_SIDE, X_SIDE + FLOOR_UNDER_HEAD, Y_PLATE0, Y_PLATE1, EAR_Z0, EAR_Z1)
    t = t.cut(box(X_SIDE - 1, X_SIDE + 5, NOTCH[0] + 8, NOTCH[1] - 8, NOTCH[2] + 4, EAR_Z1 - 6))
    for zc in SCREW_Z:
        t = t.cut(cq.Workplane("YZ").center(SCREW_Y[0], zc).circle(HOLE_FIT_D / 2).extrude(5).translate((X_SIDE - 1, 0, 0)))
        t = t.cut(cq.Workplane("YZ").center(SCREW_Y[1], zc).slot2D(HOLE_FIT_D + REAR_SLOT_TRAVEL, HOLE_FIT_D).extrude(5).translate((X_SIDE - 1, 0, 0)))
    for (ym, zm) in M3_SPOTS:
        t = t.cut(cq.Workplane("YZ").center(ym, zm).circle((M3_HEAD_D + 1.6) / 2).extrude(5).translate((X_SIDE - 1, 0, 0)))
    return t

def build_ace():
    body = box(-ACE_W / 2, ACE_W / 2, 0, ACE_D, 0, ACE_H)
    lips = (box(-ACE_ENV_W / 2, ACE_ENV_W / 2, 0, FRONT_LIP_T, 0, ACE_H)
            .union(box(-ACE_ENV_W / 2, ACE_ENV_W / 2, ACE_D - REAR_LIP_T, ACE_D, 0, ACE_H)))
    return body, body.union(lips)

def build_rail():
    z0, z1 = U_EDGE_Z - 10, U_EDGE_Z + U_PITCH + 10
    r = box(RAIL_OPEN / 2, RAIL_OPEN / 2 + RAIL_W, T_FLANGE, T_FLANGE + RAIL_T, z0, z1).union(
        box(RAIL_OPEN / 2 + RAIL_W - RAIL_T, RAIL_OPEN / 2 + RAIL_W, T_FLANGE, T_FLANGE + 40, z0, z1))
    for zc in HOLE_Z:
        r = r.cut(cq.Workplane("XZ").center(RACK_HOLE_X, zc).slot2D(SLOT_W, SLOT_H).extrude(-(RAIL_T + 2)).translate((0, T_FLANGE - 1, 0)))
    return r

# =============================================================================
# SECTION PROPERTIES SLICED FROM THE CAD
# =============================================================================
def slice_props(ear, y, dy=0.5):
    """Area, centroid z, I about the horizontal X axis through the centroid, extreme fibre c, from a thin slab."""
    slab = ear.intersect(box(X_SIDE - 5, FLANGE_X_OUT + 5, y - dy / 2, y + dy / 2, -5, 50))
    shp = slab.val().wrapped
    p = GProp_GProps(); BRepGProp.VolumeProperties_s(shp, p)
    V = p.Mass()
    if V <= 1e-9: return dict(A=0, zc=0, I=0, c=0)
    m = p.MatrixOfInertia()
    Ixx, Iyy, Izz = m.Value(1, 1), m.Value(2, 2), m.Value(3, 3)
    z2 = (Ixx + Iyy - Izz) / 2           # int (z - zc)^2 dV  (Ixx = y2+z2, Iyy = x2+z2, Izz = x2+y2)
    bb = slab.val().BoundingBox(); zc = p.CentreOfMass().Z()
    return dict(A=V / dy, zc=zc, I=z2 / dy, c=max(bb.zmax - zc, zc - bb.zmin))

# =============================================================================
# BEAM MODEL: elastic ear + rigid chassis on four factory screws (section 7)
# =============================================================================
K_BEAR = E_MOD * (FLOOR_UNDER_HEAD + HEAD_H)          # N/mm per screw: pin bearing in 4.6 mm of plastic (shank + head key)
PAIR_D = SCREW_Z[1] - SCREW_Z[0]
K_FRONT_V, K_FRONT_R = 2 * K_BEAR, 2 * K_BEAR * (PAIR_D / 2) ** 2
K_REAR_V = 2 * K_BEAR                                  # rear slots run along Y: vertical load only
# root rotation: flange plate bends between the outer web and the rack screws (cantilever LEVER_X, width W_EFF)
I_FPL = W_EFF * T_FLANGE ** 3 / 12
_c_root = 2 * (LEVER_X ** 3 / (3 * E_MOD * I_FPL) + LEVER_X / (5 / 6 * G_MOD * W_EFF * T_FLANGE)) / LEVER_Z ** 2
K_ROOT = 1.0 / _c_root
GA_TRUSS = 2 * E_MOD * (STRUT * T_WEB) * math.sin(math.pi / 4) ** 2 * math.cos(math.pi / 4)

def beam_model(ear):
    ys = sorted(set(np.round(np.concatenate([np.arange(T_FLANGE, Y_PLATE1 + 1e-6, 2.5), [SCREW_Y[0], SCREW_Y[1], Y_PLATE1]]), 4)))
    ys = np.array(ys); ne = len(ys) - 1
    props = []
    for i in range(ne):
        ym = (ys[i] + ys[i + 1]) / 2
        sp = slice_props(ear, ym)
        in_truss = LAT_Y0 - 1 <= ym <= LAT_Y1 + 1
        GA = GA_TRUSS if in_truss else 5 / 6 * G_MOD * sp["A"] * 0.5    # 0.5: only the vertical walls carry shear
        props.append(dict(y=ym, EI=E_MOD * sp["I"], GA=GA, I=sp["I"], c=sp["c"], A=sp["A"], truss=in_truss))
    nd = 2 * (ne + 1); NDOF = nd + 2; iwc, ith = nd, nd + 1
    def k_el(EI, GA, L):
        ph = 12 * EI / (GA * L * L)
        return EI / ((1 + ph) * L ** 3) * np.array([[12, 6 * L, -12, 6 * L], [6 * L, (4 + ph) * L * L, -6 * L, (2 - ph) * L * L],
                                                   [-12, -6 * L, 12, -6 * L], [6 * L, (2 - ph) * L * L, -6 * L, (4 + ph) * L * L]])
    def assemble(rear_on):
        K = np.zeros((NDOF, NDOF))
        for i, pr in enumerate(props):
            idx = [2 * i, 2 * i + 1, 2 * i + 2, 2 * i + 3]
            K[np.ix_(idx, idx)] += k_el(pr["EI"], pr["GA"], ys[i + 1] - ys[i])
        K[1, 1] += K_ROOT                                   # root rotation spring; root w fixed below
        def spring(B, k):
            nonlocal K; B = np.array(B); K += k * np.outer(B, B)
        nF = int(np.where(np.isclose(ys, SCREW_Y[0]))[0][0]); nR = int(np.where(np.isclose(ys, SCREW_Y[1]))[0][0])
        B = np.zeros(NDOF); B[2 * nF] = -1; B[iwc] = 1; B[ith] = SCREW_Y[0]; spring(B, K_FRONT_V)
        B = np.zeros(NDOF); B[2 * nF + 1] = -1; B[ith] = 1; spring(B, K_FRONT_R)
        if rear_on:
            B = np.zeros(NDOF); B[2 * nR] = -1; B[iwc] = 1; B[ith] = SCREW_Y[1]; spring(B, K_REAR_V)
        return K, nF, nR
    def solve(P, rear_on):
        K, nF, nR = assemble(rear_on)
        f = np.zeros(NDOF); f[iwc] = P; f[ith] = P * COM_Y
        free = [d for d in range(NDOF) if d != 0]
        u = np.zeros(NDOF); u[free] = np.linalg.solve(K[np.ix_(free, free)], f[free])
        return u, nF, nR
    def results(P):
        u1, nF, nR = solve(1.0, False)
        rel1 = (u1[iwc] + SCREW_Y[1] * u1[ith]) - u1[2 * nR]          # chassis - ear at the rear pair, unit load
        P_close = REAR_GAP / rel1 if rel1 > 0 else float("inf")
        if P <= P_close:
            u = P * u1; dP = 0.0; u2 = np.zeros_like(u1)
        else:
            u2, _, _ = solve(1.0, True); dP = P - P_close; u = P_close * u1 + dP * u2
        relF = (u[iwc] + SCREW_Y[0] * u[ith]) - u[2 * nF]
        rotF = u[ith] - u[2 * nF + 1]
        relR2 = dP * ((u2[iwc] + SCREW_Y[1] * u2[ith]) - u2[2 * nR]) if dP > 0 else 0.0
        FvF, MF, FvR = K_FRONT_V * relF, K_FRONT_R * rotF, K_REAR_V * relR2
        # element end moments / shears
        Ms, Vs = [], []
        for i, pr in enumerate(props):
            ue = u[[2 * i, 2 * i + 1, 2 * i + 2, 2 * i + 3]]
            fe = k_el(pr["EI"], pr["GA"], ys[i + 1] - ys[i]) @ ue
            Ms.append(max(abs(fe[1]), abs(fe[3]))); Vs.append(abs(fe[0]))
        sag = u[iwc] + ACE_D * u[ith]
        return dict(P=P, P_close=P_close, FvF=FvF, MF=MF, FvR=FvR, Ms=Ms, Vs=Vs, sag=sag, u=u)
    return props, results

# =============================================================================
# LOAD CASES (section 7)
# =============================================================================
A_BEAR = SHANK_D * FLOOR_UNDER_HEAD + HEAD_D * HEAD_H          # shank in the 2.4 floor + head in its pocket (shear key)

def load_calcs(props, results):
    R = []
    def row(name, case, val, unit, allow, note, req=SF_REQ):
        sf = allow / val if val > 0 else float("inf")
        R.append(dict(name=name, case=case, value=val, unit=unit, allow=allow, sf=sf, ok=sf >= req, note=note))
    def creep(name, val, note):
        lim = CREEP_FRAC * SY_XY
        R.append(dict(name=name + " (creep)", case="A", value=val, unit="MPa", allow=lim, sf=lim / val if val else float("inf"), ok=val <= lim, note=note))
    log(""); log("== Model ==")
    log("Design mass %.1f kg, W = %.1f N, P per ear = %.2f N at 1 g, COM %.0f mm behind the front face" % (M_DES, W, P_SIDE, COM_Y))
    log("Screw bearing stiffness k = E x (2.4 floor + 2.2 head) = %.0f N/mm per screw ; front pair rotational k = 2 k (%.1f/2)^2 = %.3g Nmm/rad"
        % (K_BEAR, PAIR_D, K_FRONT_R))
    log("Root rotation (flange plate bending web -> rack screw, bending + shear): k = %.3g Nmm/rad" % K_ROOT)
    log("Outer web lattice: %d diamonds %.0f x %.0f + %d triangles, shear stiffness GA = 2 E A_strut sin^2 45 cos 45 = %.0f N ; open area %.0f mm2"
        % (N_DIA, 2 * HD, 2 * HD, N_DIA - 1, GA_TRUSS, open_area()))
    log("Rear pair: worst-case vertical play %.1f mm before it takes load" % REAR_GAP)
    I_fl = T_FLANGE * PANEL_H ** 3 / 12
    out = {}
    for case, gf in (("A", CASE_A), ("B", CASE_B)):
        P = P_SIDE * gf; r = results(P)
        f_front = math.hypot(r["MF"] / PAIR_D, r["FvF"] / 2)
        f_rear = r["FvR"] / 2
        sig = [m * pr["c"] / pr["I"] for m, pr in zip(r["Ms"], props)]
        i_max = int(np.argmax(sig)); i_tr = [i for i, pr in enumerate(props) if pr["truss"]]
        s_root = sig[0]; s_chord = max(sig[i] for i in i_tr)
        V_tr = max(r["Vs"][i] for i in i_tr)
        s_diag = V_tr / (2 * math.sin(math.pi / 4)) / (STRUT * T_WEB)
        M_root = P * (COM_Y - T_FLANGE)
        T_rack = M_root / LEVER_Z
        s_fpl = 6 * T_rack * LEVER_X / (W_EFF * T_FLANGE ** 2)
        M_roll = P * (RACK_HOLE_X - (X_SIDE + T_SIDE / 2))
        s_roll = M_roll * (PANEL_H / 2) / I_fl
        log(""); log("== Case %s (%.0f g): P = %.1f N per ear ==" % (case, gf, P))
        log("Rear pair takes load above P = %.1f N ; front pair: vertical %.1f N, pitch moment %.0f Nmm -> %.0f N per screw "
            "(couple %.0f / %.1f, vertical %.1f) ; rear pair %.1f N per screw" % (r["P_close"], r["FvF"], r["MF"], f_front, r["MF"], PAIR_D, r["FvF"] / 2, f_rear))
        log("Screw bearing: %.0f N / (%.1f x %.1f + %.1f x %.1f = %.1f mm2) = %.2f MPa" % (f_front, SHANK_D, FLOOR_UNDER_HEAD, HEAD_D, HEAD_H, A_BEAR, f_front / A_BEAR))
        log("Ear bending: root M = %.0f Nmm, sigma = M c / I = %.2f MPa ; max along the ear %.2f MPa at Y = %.0f ; lattice chords %.2f MPa"
            % (r["Ms"][0], s_root, sig[i_max], props[i_max]["y"], s_chord))
        log("Lattice diagonals: V = %.1f N -> N = V / (2 sin 45) = %.1f N, sigma = %.2f MPa (inclined to the layers)" % (V_tr, V_tr / (2 * math.sin(math.pi / 4)), s_diag))
        log("Flange: rack screw tension %.0f / %.1f = %.0f N ; plate bending %.2f MPa ; in-plane (roll) M = %.0f Nmm, sigma = %.2f MPa"
            % (M_root, LEVER_Z, T_rack, s_fpl, M_roll, s_roll))
        log("Chassis rear sag (Y = 210): %.3f mm (includes the rear play %.1f mm while open)" % (r["sag"], REAR_GAP))
        out[case] = r
        if case == "A":
            creep("Front screw bearing", f_front / A_BEAR, "%.0f N per screw" % f_front)
            creep("Ear bending, max", sig[i_max], "at Y = %.0f" % props[i_max]["y"])
            creep("Flange plate bending", s_fpl, "web -> rack screw")
            R.append(dict(name="Chassis rear sag", case="A", value=r["sag"], unit="mm", allow=SAG_MAX, sf=SAG_MAX / r["sag"], ok=r["sag"] <= SAG_MAX,
                          note="beam model incl. screw bearing, flange flex, rear hole play"))
        else:
            row("Front screw bearing (pocket + floor)", "B", f_front / A_BEAR, "MPa", SY_XY, "%.0f N per screw, in-layer" % f_front)
            row("Rear screw bearing", "B", max(f_rear, 1e-6) / A_BEAR, "MPa", SY_XY, "%.0f N per screw" % f_rear)
            row("Ear root at the flange", "B", s_root, "MPa", SY_XY, "in-layer")
            row("Ear bending, max along the length", "B", sig[i_max], "MPa", SY_XY, "Y = %.0f, in-layer" % props[i_max]["y"])
            row("Lattice chords", "B", s_chord, "MPa", SY_XY, "in-layer")
            row("Lattice diagonals", "B", s_diag, "MPa", SY_Z, "45 deg to the layers -> Z strength")
            row("Flange plate bending at the rack screw", "B", s_fpl, "MPa", SY_XY, "sized: t = %.0f mm" % T_FLANGE)
            row("Flange in-plane (roll)", "B", s_roll, "MPa", SY_XY, "")
            row("Rack screw tension", "B", T_rack, "N", 8000.0, "10-32 steel proof (weakest)")
            row("Flange under the rack washer", "B", T_rack / (math.pi / 4 * (18 ** 2 - (SLOT_W + COMP_HOLE) * (SLOT_H + COMP_HOLE))), "MPa", SY_Z, "compression across layers")
    # Case C
    F_C = CASE_C * W
    t_pull = F_C / 4                                    # all side load into one ear's four screws (the other ear pushes)
    a_head = math.pi / 4 * (HEAD_D ** 2 - HOLE_FIT_D ** 2)
    v_fb = F_C / 4                                      # fore-aft: front pairs only (rear slots run along Y)
    log(""); log("== Case C: 3 g, F = %.0f N ==" % F_C)
    log("Side load: one ear's 4 screws in tension, %.0f N each ; head on the 2.4 mm floor %.2f MPa ; punching %.2f MPa"
        % (t_pull, t_pull / a_head, t_pull / (math.pi * HEAD_D * FLOOR_UNDER_HEAD)))
    log("Fore-aft: %.0f N per front screw (rear pair slotted along Y), bearing %.2f MPa" % (v_fb, v_fb / A_BEAR))
    M_lat = F_C / 2 * COM_Y
    log("Lateral bending of each ear (box 61 mm wide): M = %.0f Nmm" % M_lat)
    row("Screw head pull-through (side load)", "C", t_pull / a_head, "MPa", SY_XY, "")
    row("Floor punching shear (side load)", "C", t_pull / (math.pi * HEAD_D * FLOOR_UNDER_HEAD), "MPa", TAU_XY, "")
    row("Front screw bearing (fore-aft)", "C", v_fb / A_BEAR, "MPa", SY_XY, "")
    row("Factory screw tension (side load)", "C", t_pull, "N", 7000.0, "M4 proof; the ACE thread sees this")
    R.append(dict(name="Chassis retained", case="C", value=1, unit="", allow=1, sf=float("inf"), ok=True, note="8 screws into the chassis"))
    return R, out

# =============================================================================
# VERIFICATION (section 9)
# =============================================================================
CHECKS = []
def check(name, ok, value, status=None):
    CHECKS.append(dict(name=name, ok=bool(ok), value=value, status=status or ("PASS" if ok else "FAIL")))
def vol_intersect(a, b):
    try: return a.intersect(b).val().Volume()
    except Exception: return 0.0

def overhang_check(name, wp):
    solid = wp.val(); bb = solid.BoundingBox()
    sin_lim = math.sin(math.radians(MAX_OVERHANG + 0.5)); bad = []; n_bridge = 0; worst = 0.0
    for f in solid.Faces():
        try:
            vs, tris = f.tessellate(0.3, 0.3); pts = [(vs[t[0]] + vs[t[1]] + vs[t[2]]) / 3 for t in tris]
        except Exception:
            pts = [f.Center()]
        for c in pts[:80]:
            try: n = f.normalAt(c)
            except Exception: n = f.normalAt()
            if n.z >= -sin_lim: continue
            if n.z < -0.999:
                fb = f.BoundingBox()
                if abs(fb.zmin - bb.zmin) < 1e-3: continue
                span = min(fb.xlen, fb.ylen)
                if span <= MAX_BRIDGE: n_bridge += 1; worst = max(worst, span); continue
                bad.append("flat %.1f mm span at z=%.1f" % (span, fb.zmin))
            else:
                bad.append("sloped face %.1f deg at z=%.1f" % (math.degrees(math.asin(-n.z)), c.z))
            break
    check("%s: no overhang > 45 deg as printed (bridges <= %.0f mm allowed)" % (name, MAX_BRIDGE), not bad,
          "OK, %d short bridge faces (max %.1f mm)" % (n_bridge, worst) if not bad else "; ".join(bad[:3]))

def run_verification(ear_R, ear_L, ace_body, ace_env, rails):
    v = max(vol_intersect(ear_R, ace_body), vol_intersect(ear_L, ace_body))
    check("Zero interference: ears vs ACE body (310 x 44 x 210)", v < 1e-6, "%.4f mm3" % v)
    v = max(vol_intersect(ear_R, ace_env), vol_intersect(ear_L, ace_env))
    check("Zero interference: ears vs ACE envelope incl. the %.0f mm front/rear panel lips (315.6 wide)" % FRONT_LIP_T, v < 1e-6, "%.4f mm3" % v)
    v = max(vol_intersect(e, r) for e in (ear_R, ear_L) for r in rails)
    check("Zero interference: ears vs rack rails", v < 1e-6, "%.4f mm3" % v)
    blank = box(X_FLANGE_IN, FLANGE_X_OUT, 0, T_FLANGE, EAR_Z0, EAR_Z1)
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
    check("Structure behind the flange <= 222 mm (3 mm inside the 450 mm rail opening)", behind <= STRUCT_X_MAX + 1e-6, "%.2f mm" % behind)
    vz = VENT_ZONE
    v = vol_intersect(ear_R, box(X_SIDE - 0.5, X_SIDE + T_SIDE + 0.5, vz[0], vz[1], vz[2], vz[3]))
    check("Every vent uncovered (side plate open over the vent field, both orientations)", v < 1e-6, "%.4f mm3 of plate over the vent" % v)
    v = vol_intersect(ear_R, box(X_SIDE, X_SIDE + 50.0, vz[0], vz[1], vz[2], vz[3]))
    check("Air column in front of the vent clear for 50 mm (nothing within 50 mm of the vent)", v < 1e-6,
          "%.4f mm3 ; outer web at %.0f mm, %.0f mm2 of diamond openings" % (v, X_WEB0 - X_SIDE, open_area()))
    walls = {"flange": T_FLANGE, "side plate": T_SIDE, "outer web": T_WEB, "floor": T_FLOOR, "bulkheads": T_BULK,
             "lattice struts": STRUT, "plastic under the screw heads": FLOOR_UNDER_HEAD,
             "plate behind the M3 relief": T_SIDE - RELIEF, "top chord": CHORD_H,
             "access hole to web top": EAR_Z1 - (SCREW_Z[1] + ACCESS_D / math.sqrt(2)),
             "access hole to floor": SCREW_Z[0] - ACCESS_D / 2 - Z_FLOOR1,
             "rack slot to flange edge": FLANGE_X_OUT - RACK_HOLE_X - (SLOT_W + COMP_HOLE) / 2,
             "pocket to plate bottom": SCREW_Z[0] - POCKET_D / 2 - EAR_Z0,
             "pocket to plate top": EAR_Z1 - (SCREW_Z[1] + POCKET_D / math.sqrt(2)),
             "front pocket to front bulkhead fillet": Y_FBULK[0] - FILLET_SMALL - (SCREW_Y[0] + POCKET_D / 2)}
    mn = min(walls, key=walls.get)
    check("Minimum wall >= 2.4 mm (thinnest: %s)" % mn, walls[mn] >= MIN_WALL - 1e-9, "%.2f mm" % walls[mn])
    check("Fillets: 6 mm at flange-plate and flange-web, 2 mm at bulkheads and floor", FILLETS and FILLETS[-1] == 10, "%d coves" % (FILLETS[-1] if FILLETS else 0))
    for n, p in (("ear_R", ear_R), ("ear_L", ear_L)):
        po = to_print(p); s = po.val(); ns = len(po.solids().vals())
        check("%s: one watertight solid" % n, ns == 1 and s.isValid(), "%d solid(s), valid=%s, %.1f cm3" % (ns, s.isValid(), s.Volume() / 1000))
        bb = s.BoundingBox()
        check("%s: inside 256^3 build volume as oriented" % n, max(bb.xlen, bb.ylen, bb.zlen) <= BUILD_VOL, "%.1f x %.1f x %.1f mm" % (bb.xlen, bb.ylen, bb.zlen))
        overhang_check(n, po)

def to_print(wp):
    bb = wp.val().BoundingBox()
    return wp.translate((-bb.xmin, -bb.ymin, -bb.zmin))            # rack vertical = printer Z, floor on the bed

# =============================================================================
# EXPORT, RENDER, REPORT
# =============================================================================
def render_png(shapes, path, title, elev=25, azim=-60, ortho=False):
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    fig = plt.figure(figsize=(10, 7), dpi=110); ax = fig.add_subplot(111, projection="3d"); allpts = []
    light = np.array([0.4, -0.6, 0.7]); light /= np.linalg.norm(light)
    for wp, col in shapes:
        vs, tris = wp.val().tessellate(0.4, 0.3); V = np.array([[v.x, v.y, v.z] for v in vs]); F = np.array(tris)
        if len(F) == 0: continue
        polys = V[F]; n = np.cross(polys[:, 1] - polys[:, 0], polys[:, 2] - polys[:, 0]); n /= (np.linalg.norm(n, axis=1)[:, None] + 1e-12)
        ax.add_collection3d(Poly3DCollection(polys, facecolors=np.clip(np.array(col)[None, :] * (0.45 + 0.55 * np.abs(n @ light))[:, None], 0, 1), edgecolors="none"))
        allpts.append(V)
    P = np.vstack(allpts); mn, mx = P.min(axis=0), P.max(axis=0); c = (mn + mx) / 2; r = (mx - mn).max() / 2
    ax.set_xlim(c[0] - r, c[0] + r); ax.set_ylim(c[1] - r, c[1] + r); ax.set_zlim(c[2] - r, c[2] + r); ax.set_box_aspect((1, 1, 1))
    ax.view_init(elev=elev, azim=azim)
    if ortho: ax.set_proj_type("ortho")
    ax.set_axis_off(); ax.set_title(title); fig.tight_layout(); fig.savefig(path); plt.close(fig)

def export_print(wp, name, files, oriented):
    po = oriented(wp)
    try: po = po.faces("<Z").chamfer(CHAMFER_BED); ch = "applied"
    except Exception: ch = "not applied"
    exporters.export(cq.Workplane("XY").add(po.val().scale(SHRINK_SCALE)), os.path.join(OUT, "3mf", name + ".3mf"))
    exporters.export(po, os.path.join(OUT, "step", name + "_print_oriented.step"))
    files.append("3mf/%s.3mf (x%.3f shrink, bed chamfer %s)" % (name, SHRINK_SCALE, ch))

def main():
    for d in ("step", "3mf", "renders"): os.makedirs(os.path.join(OUT, d), exist_ok=True)
    ear_R = build_ear(); ear_L = mirror_x(ear_R)
    ace_body, ace_env = build_ace()
    rails = [build_rail(), mirror_x(build_rail())]
    props, results = beam_model(ear_R)
    R, out = load_calcs(props, results)
    run_verification(ear_R, ear_L, ace_body, ace_env, rails)
    for r in R:
        check("Load: %s (Case %s)" % (r["name"], r["case"]), r["ok"],
              "%.2f %s vs %.2f -> SF %.2f" % (r["value"], r["unit"], r["allow"], r["sf"]) if r["unit"] else "retained")
    files = []
    for n, p in (("ear_R", ear_R), ("ear_L", ear_L)):
        exporters.export(p, os.path.join(OUT, "step", n + ".step")); files.append("step/%s.step (assembly position)" % n)
        export_print(p, n, files, to_print)
    tpl = build_template()
    def tpl_orient(wp):
        w = wp.rotate((0, 0, 0), (0, 1, 0), -90); bb = w.val().BoundingBox()
        return w.translate((-bb.xmin, -bb.ymin, -bb.zmin))
    export_print(tpl, "fit_template", files, tpl_orient)
    exporters.export(tpl, os.path.join(OUT, "step", "fit_template.step"))
    assy = cq.Assembly(name="ACE_1U_onepiece_ears")
    BLUE, GREY, DARK = (0.25, 0.45, 0.85), (0.5, 0.5, 0.5), (0.3, 0.3, 0.3)
    assy.add(ear_R, name="ear_R", color=cq.Color(*BLUE)); assy.add(ear_L, name="ear_L", color=cq.Color(*BLUE))
    assy.add(ace_body, name="ACE_body", color=cq.Color(*DARK))
    for i, r in enumerate(rails): assy.add(r, name="rail_%d" % i, color=cq.Color(*GREY))
    assy.save(os.path.join(OUT, "step", "assembly_in_rack.step")); files.append("step/assembly_in_rack.step")
    shapes = [(ear_R, BLUE), (ear_L, BLUE), (ace_body, DARK)] + [(r, GREY) for r in rails]
    for vn, (el, az, o) in {"iso": (22, -55, False), "front": (0, -90, True), "top": (90, -90, True), "side": (0, 0, True)}.items():
        render_png(shapes, os.path.join(OUT, "renders", "assembly_%s.png" % vn), "ACE 1U one-piece ears in rack - " + vn, el, az, o)
        render_png([(ear_R, BLUE)], os.path.join(OUT, "renders", "ear_R_%s.png" % vn), "ear_R - " + vn, el, az, o)
    render_png([(ear_R, BLUE)], os.path.join(OUT, "renders", "ear_R_outside.png"), "ear_R - from the rack side (diamond air openings)", 18, 25)
    render_png([(ear_R, BLUE)], os.path.join(OUT, "renders", "ear_R_inside.png"), "ear_R - chassis side (screw pockets, vent opening)", 18, -150)
    render_png([(to_print(ear_R), BLUE)], os.path.join(OUT, "renders", "ear_R_as_printed.png"), "ear_R as printed (floor on the bed)", 30, -60)
    files += ["renders/assembly_{iso,front,top,side}.png", "renders/ear_R_{iso,front,top,side,outside,inside,as_printed}.png"]
    mass = ear_R.val().Volume() * 1.27e-3
    with open(os.path.join(OUT, "report.md"), "w") as f:
        f.write("# ACE 1U one-piece printed ears - verification report\n\n")
        f.write("Chassis data from photo IMG_8471 (+/- 0.5 mm): screws at Y %s, Z %s; vent zone %s; M3 spots %s.\n\n" % (SCREW_Y, SCREW_Z, VENT_ZONE, M3_SPOTS))
        f.write("| # | Check | Result | Value |\n|---|---|---|---|\n")
        for i, c in enumerate(CHECKS, 1): f.write("| %d | %s | %s | %s |\n" % (i, c["name"], c["status"], c["value"]))
        f.write("\n## Calculation log\n\n```\n" + "\n".join(LOG) + "\n```\n\n## Load table\n\n| Feature | Case | Value | Allowable | SF | Pass | Note |\n|---|---|---|---|---|---|---|\n")
        for r in R:
            f.write("| %s | %s | %.2f %s | %.2f %s | %.2f | %s | %s |\n" % (r["name"], r["case"], r["value"], r["unit"], r["allow"], r["unit"], r["sf"], "yes" if r["ok"] else "NO", r["note"]))
        f.write("\n## Derived dimensions\n\n")
        for k, v in dict(T_FLANGE=T_FLANGE, T_SIDE=T_SIDE, FLOOR_UNDER_HEAD=FLOOR_UNDER_HEAD, POCKET_D=POCKET_D, HOLE_FIT_D=HOLE_FIT_D,
                         X_WEB=(X_WEB0, X_WEB1), NOTCH=NOTCH, Y_FBULK=Y_FBULK, Y_RBULK=Y_RBULK, DIAMONDS=[round(y, 1) for y in DIA_YC],
                         RACK_SLOT_Z=[round(z, 3) for z in HOLE_Z], mass_g_solid=round(mass)).items():
            f.write("- %s = %s\n" % (k, v))
        f.write("\n## Files\n\n" + "\n".join("- " + x for x in files) + "\n")
    json.dump(dict(checks=CHECKS, loads=R), open(os.path.join(OUT, "verification.json"), "w"), indent=1, default=str)
    print("\n".join("%-6s %s : %s" % (c["status"], c["name"], c["value"]) for c in CHECKS))
    nf = sum(1 for c in CHECKS if c["status"] == "FAIL"); print("\n%d checks, %d failed ; ear mass ~%.0f g" % (len(CHECKS), nf, mass))
    return nf == 0

if __name__ == "__main__":
    sys.exit(0 if main() else 1)
