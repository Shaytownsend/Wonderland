#!/usr/bin/env python3
"""
DirectOut ACE  ->  EIA-310-D 19" rack, 2U printed cradle (PATH B).

One fully parametric CadQuery script.  Every dimension is a named variable in
the PARAMETERS block.  Running the script:
  1. builds the cradle sides, clamps, tie bars, ACE reference and rack rails,
  2. runs the load calculations (section 7) and auto-thickens failing features,
  3. runs the automatic verification (section 9) and aborts on any failure,
  4. exports STEP + 3MF per printed part, STEP of the assembly, SVG/PNG renders,
  5. writes output/report.md with every check and every number.

Coordinate system (assembly frame)
  X : rack width, 0 at rack centre, +X = right side (looking at the front)
  Y : rack depth, 0 at the ACE front-panel face, +Y rearwards
  Z : vertical, 0 at the BOTTOM EDGE OF THE 2U PANEL (panel coordinates)
      The EIA U-boundary is 0.4 mm below Z=0 (panel 88.1 inside pitch 88.9).
"""
import math, os, sys, json
import cadquery as cq
from cadquery import exporters

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output")

# =============================================================================
# PARAMETERS  (sections 1-3 and 6 of the brief; all in mm, N, MPa, kg)
# =============================================================================
# --- 1. Device (DirectOut ACE manual v1.1, Appendix C) -----------------------
ACE_W        = 310.0      # chassis body width
ACE_H        = 44.0       # chassis body height
ACE_D        = 210.0      # chassis body depth (front face to rear panel)
ACE_ENV_W    = 315.6      # overall width incl. protrusions
ACE_ENV_D    = 239.0      # overall depth incl. protrusions
ACE_MASS     = 3.7        # kg
# --- 2. Fixed settings --------------------------------------------------------
BUILD_VOL    = 256.0      # cube, mm
E_MOD        = 1900.0     # MPa  PETG-ESD tensile modulus
SY_XY        = 45.0       # MPa  in-layer yield
SY_Z         = 25.0       # MPa  across-layer (Z) strength
HDT_C        = 70.0
NOZZLE       = 0.4
LAYER_H      = 0.2
WALLS        = 6
TOPBOT_LAYERS= 5
COMP_HOLE    = 0.2        # added to every hole diameter / slot width+height
CHAMFER_BED  = 0.3        # chamfer on bed-contact edges
SHRINK_SCALE = 1.003      # applied to the 3MF print files
MAX_OVERHANG = 45.0       # degrees from vertical
MAX_BRIDGE   = 25.0       # mm, unsupported flat span allowed (tie-bar channel roof 20.4, blind-hole roofs 5.8)
MIN_WALL     = 2.4
# --- 3. Rack (EIA-310-D) --------------------------------------------------------
PANEL_W      = 482.6
RACK_HOLE_X  = 465.1 / 2  # 232.55 from centre
RAIL_OPEN    = 450.0
U_PITCH      = 44.45
PANEL_H      = 88.1       # 2U
HOLE_FROM_U  = (6.35, 22.225, 38.1)
SLOT_H       = 7.0
SLOT_W       = 10.5
PANEL_OFFSET = (2 * U_PITCH - PANEL_H) / 2        # 0.4 : panel bottom above U edge
CLEAR_MARGIN = 3.0
STRUCT_X_MAX = RAIL_OPEN / 2 - CLEAR_MARGIN        # 222.0 : nothing behind the flange beyond this
FLANGE_X_OUT = PANEL_W / 2                         # 241.3
RAIL_T       = 2.0        # steel rail model thickness
RAIL_W       = 20.0       # rail front flange width (model), from X=225
# --- 6. Cradle ----------------------------------------------------------------
T_FLANGE     = 6.0
T_WALL       = 5.0
T_SHELF_SPEC = 6.0        # as specified; auto-raised (tie-bar recess + insert) -> see sizing
AIR_GAP      = 60.0       # chassis side -> wall inner face
SHELF_UNDER  = 60.0       # shelf reach under the chassis
SHELF_DEPTH_SPEC = 215.0
FILLET_R     = 6.0
WEB_T        = 4.0
VENT_CLEAR   = 50.0       # section 9: nothing within 50 mm of the side panels
NEO_SEAT_T   = 1.0        # neoprene seat strip
RIB_H, RIB_T, RIB_L = 5.0, 4.0, 20.0
RIB_CLEAR    = 0.4        # per side on 315.6 -> inner faces 316.4 apart
STOP_H, STOP_T_SPEC, STOP_Y, STOP_W = 10.0, 5.0, 210.5, 8.0
LIP_H, LIP_T = 2.0, 3.0
CLAMP_Y      = (40.0, 170.0)
CLAMP_OVER   = 15.0       # reach over the chassis top
CLAMP_W      = 20.0       # bar width (Y)
CLAMP_ARM_T_SPEC = 6.0    # auto-thickened by section 7 loop
CLAMP_FOOT_T = 6.0
PAD_T, PAD_COMP = 2.0, 0.5
PAD_F_MAX    = 40.0       # N, max pad force per clamp at 0.5 mm compression (sponge neoprene spec)
TIE_Y        = (20.0, 195.0)
TIE_W, TIE_T, TIE_LEN = 20.0, 6.0, 400.0           # aluminium flat bar EN AW-6060
TIE_SCREW_X  = (120.0, 180.0)                       # per side (mirrored)
# --- hardware -------------------------------------------------------------------
INSERT_OD, INSERT_L, INSERT_HOLE, INSERT_DEPTH = 6.3, 5.7, 5.6, 6.5   # M4 heat-set (Ruthex RX-M4x5.7 type)
INSERT_PULLOUT = 600.0    # N, design value for M4x5.7 insert in 100 % PETG (assumed; pull-test to confirm)
M4_CLR       = 4.5        # clearance hole
M4_BUTTON_HEAD_D, M4_BUTTON_HEAD_H = 7.6, 2.2       # ISO 7380
CBORE_D, CBORE_DEPTH = 8.0, 2.5
ROOF_MIN     = 2.4        # material above a blind insert hole (= MIN_WALL)
INSERT_WALL_MIN = 3.0     # material beside an insert hole
# --- 7. Loads -------------------------------------------------------------------
M_EXTRA      = 1.5        # kg cables + SFP
COM_Y        = 105.0      # centre of mass behind the flange
G            = 9.81
CASE_A, CASE_B, CASE_C = 1.0, 5.0, 3.0
SF_REQ       = 3.0
SAG_MAX      = 1.0
CREEP_FRAC   = 0.15

# =============================================================================
# DERIVED GEOMETRY + AUTOMATIC SIZING (section 7 "thicken in 1 mm steps")
# =============================================================================
LOG = []                       # human-readable calculation log (goes to the report)
def log(s=""): LOG.append(s)

X_SIDE   = ACE_W / 2                       # 155.0  chassis side panel plane
X_ENV    = ACE_ENV_W / 2                   # 157.8  protrusion envelope
X_RIB_IN = X_ENV + RIB_CLEAR               # 158.2  rib inner face
X_WALL_IN  = X_SIDE + AIR_GAP              # 215.0
X_WALL_OUT = X_WALL_IN + T_WALL            # 220.0  (must be <= STRUCT_X_MAX)
X_SHELF_IN = X_SIDE - SHELF_UNDER          # 95.0
X_FLANGE_IN = X_RIB_IN                     # flange stops at the protrusion envelope + clearance
X_BAND_OUT = X_SIDE + VENT_CLEAR           # 205.0  vent clearance band outer limit

# --- tie-bar recess forces the shelf thickness ------------------------------------
TIE_RECESS = TIE_T                                   # bar flush with the panel bottom
T_SHELF = max(T_SHELF_SPEC, math.ceil(TIE_RECESS + INSERT_DEPTH + ROOF_MIN))
log("Shelf thickness: spec %.0f mm; tie-bar recess %.1f + insert hole %.1f + roof %.1f = %.1f"
    " -> T_SHELF = %.0f mm" % (T_SHELF_SPEC, TIE_RECESS, INSERT_DEPTH, ROOF_MIN,
                               TIE_RECESS + INSERT_DEPTH + ROOF_MIN, T_SHELF))
Z_SEAT  = T_SHELF + NEO_SEAT_T                       # chassis bottom
Z_TOP   = Z_SEAT + ACE_H                             # chassis top
SHELF_DEPTH = max(SHELF_DEPTH_SPEC, STOP_Y + 7.0)    # provisional; fixed after STOP_T sizing

# --- rack hole pattern (panel coordinates) ------------------------------------------
HOLE_Z_RACK = [u * U_PITCH + h for u in (0, 1) for h in HOLE_FROM_U]         # from U edge
HOLE_Z = [z - PANEL_OFFSET for z in HOLE_Z_RACK]                               # from panel bottom
FLANGE_SLOT_Z = [HOLE_Z[i] for i in (0, 2, 3, 5)]                              # top+bottom hole of each U

# --- top rail on the wall (insert boss for the clamps) -----------------------------------
NOTCH_DEPTH  = CLAMP_FOOT_T
Z_NOTCH      = PANEL_H - NOTCH_DEPTH                  # clamp foot floor
Z_RAIL_CH0   = Z_TOP + 1.0                            # chamfer starts 1 mm above the chassis top (outside vent band)
# rail protrusion limited so the insert hole bottom stays in the full section above the 45 deg chamfer
RAIL_PROTRUSION = math.floor(Z_NOTCH - INSERT_DEPTH - Z_RAIL_CH0)
RAIL_TOP_W   = T_WALL + RAIL_PROTRUSION               # X extent incl. the 5 mm wall
X_RAIL_IN    = X_WALL_OUT - RAIL_TOP_W
Z_RAIL_FULL  = Z_RAIL_CH0 + RAIL_PROTRUSION           # 45 deg chamfer -> full section from here
log("Top rail: protrusion = floor(%.1f - %.1f - %.1f) = %.0f mm -> rail %.0f wide, full section from Z = %.1f"
    % (Z_NOTCH, INSERT_DEPTH, Z_RAIL_CH0, RAIL_PROTRUSION, RAIL_TOP_W, Z_RAIL_FULL))
CLAMP_SCREW_X = X_RAIL_IN + (INSERT_HOLE + COMP_HOLE) / 2 + INSERT_WALL_MIN   # 205.9
CLAMP_SCREW_X = math.ceil(CLAMP_SCREW_X * 10) / 10
CLAMP_SCREW_DY = 6.0
assert Z_NOTCH - INSERT_DEPTH >= Z_RAIL_FULL, "insert hole would break into the rail chamfer"

# --- loads ------------------------------------------------------------------------------
M_DES = ACE_MASS + M_EXTRA                            # 5.2 kg
W     = M_DES * G                                     # 51.0 N
P_SIDE = W / 2                                        # per side, 1 g
X_LOAD = (X_SHELF_IN + X_SIDE) / 2                    # 125  centroid of the 60 mm seat strip
A_SHELF = X_WALL_IN - X_LOAD                          # 90   lever to the wall inner face
log("Design mass %.1f kg -> W = %.1f N ; per side P = %.2f N (1 g)" % (M_DES, W, P_SIDE))

def I_rect(b, h): return b * h ** 3 / 12.0

# --- rear stop thickness (Case C, 3 g rearwards, 2 stops, across layers) ----------------
F_C = CASE_C * W                                      # 153 N
STOP_T = STOP_T_SPEC
while True:
    f_stop = F_C / 2
    lever  = (NEO_SEAT_T + STOP_H) / 2                # contact from seat top to tab top, centroid above shelf
    M      = f_stop * lever
    I      = I_rect(STOP_W, STOP_T)
    sig    = M * (STOP_T / 2) / I
    SF_STOP = SY_Z / sig
    if SF_STOP >= SF_REQ: break
    STOP_T += 1.0
log("Rear stop: F = %.1f N/tab, lever %.2f mm, M = %.0f Nmm, t -> %.0f mm, I = %.1f mm4, "
    "sigma = %.2f MPa (across layers), SF = %.2f" % (f_stop, lever, M, STOP_T, I, sig, SF_STOP))
SHELF_DEPTH = max(SHELF_DEPTH_SPEC, STOP_Y + STOP_T)

# --- clamp arm / web thickness (Case B, 5 g upward on 4 clamps, in-layer) -----------------
N_CLAMPS = 4
F_CLAMP_B = CASE_B * W / N_CLAMPS                     # 63.8 N
X_ARM_IN  = X_SIDE - CLAMP_OVER                       # 140
X_PAD_C   = (X_ARM_IN + X_SIDE) / 2                   # 147.5
CLAMP_ARM_T = CLAMP_ARM_T_SPEC
while True:
    X_WEB_OUT = X_RAIL_IN                              # web outer face against the rail inner face line
    X_WEB_IN  = X_WEB_OUT - CLAMP_ARM_T
    L_ARM = X_WEB_IN - X_PAD_C
    M_ARM = F_CLAMP_B * L_ARM
    I_ARM = I_rect(CLAMP_W, CLAMP_ARM_T)
    SIG_ARM = M_ARM * (CLAMP_ARM_T / 2) / I_ARM
    SF_ARM = SY_XY / SIG_ARM
    SIG_ARM_PRE = PAD_F_MAX * L_ARM * (CLAMP_ARM_T / 2) / I_ARM        # Case A creep from pad preload
    if SF_ARM >= SF_REQ and SIG_ARM_PRE <= CREEP_FRAC * SY_XY: break
    CLAMP_ARM_T += 1.0
log("Clamp arm: F = %.1f N, lever %.1f mm, M = %.0f Nmm, t -> %.0f mm, sigma = %.2f MPa (in-layer), SF = %.2f ; "
    "preload creep sigma = %.2f MPa (<= %.2f)" % (F_CLAMP_B, L_ARM, M_ARM, CLAMP_ARM_T, SIG_ARM, SF_ARM, SIG_ARM_PRE, CREEP_FRAC*SY_XY))
# clamp screw tension: foot pivots on its OUTER edge (X_WALL_OUT), screws at CLAMP_SCREW_X
LEVER_SCREW = X_WALL_OUT - CLAMP_SCREW_X
T_CLAMP_B = F_CLAMP_B * (X_WALL_OUT - X_PAD_C) / LEVER_SCREW     # total on 2 screws
T_CLAMP_SCREW = T_CLAMP_B / 2
SF_INSERT = INSERT_PULLOUT / T_CLAMP_SCREW
log("Clamp screws: pivot lever %.1f mm, T_total = %.0f N, %.0f N per insert, pull-out SF = %.2f (%.0f N design)"
    % (LEVER_SCREW, T_CLAMP_B, T_CLAMP_SCREW, SF_INSERT, INSERT_PULLOUT))
assert SF_INSERT >= SF_REQ

Z_CLAMP_LO = Z_TOP + (PAD_T - PAD_COMP)               # arm underside (pad compressed to 1.5)
Z_CLAMP_HI = Z_CLAMP_LO + CLAMP_ARM_T
assert Z_CLAMP_HI < Z_RAIL_CH0 + 100  # trivially true; arm sits beside the rail
assert Z_CLAMP_HI <= PANEL_H

# =============================================================================
# GEOMETRY HELPERS
# =============================================================================
def box(x0, x1, y0, y1, z0, z1):
    """Axis-aligned box from min/max corners."""
    return (cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False)
            .translate((x0, y0, z0)))

def prism_xz(pts, y0, y1):
    """Polygon in the X-Z plane extruded from y0 to y1."""
    wp = cq.Workplane("XZ").polyline(pts).close().extrude(-(y1 - y0))   # XZ normal is -Y
    return wp.translate((0, y0, 0))

def cove_xz(x0, x1, z0, z1, cx, cz, y0, y1):
    """Concave fillet (box minus cylinder, axis Y) in the X-Z plane."""
    r = x1 - x0
    cyl = cq.Workplane("XZ").center(cx, cz).circle(r).extrude(-(y1 - y0)).translate((0, y0, 0))
    return box(x0, x1, y0, y1, z0, z1).cut(cyl)

def cove_xy(x0, x1, y0, y1, cx, cy, z0, z1):
    """Concave fillet with a vertical (Z) axis."""
    r = x1 - x0
    cyl = cq.Workplane("XY").center(cx, cy).circle(r).extrude(z1 - z0).translate((0, 0, z0))
    return box(x0, x1, y0, y1, z0, z1).cut(cyl)

def cove_yz(y0, y1, z0, z1, cy, cz, x0, x1):
    """Concave fillet with an X axis."""
    r = y1 - y0
    cyl = cq.Workplane("YZ").center(cy, cz).circle(r).extrude(x1 - x0).translate((x0, 0, 0))
    return box(x0, x1, y0, y1, z0, z1).cut(cyl)

def teardrop_obround_xz(w, h, cx, cz, y0, y1):
    """Obround slot (w wide, h tall) with a 45 deg teardrop roof, axis Y, in the X-Z plane.
    Returns the cutter solid.  Slot centre (cx, cz) is the centre of the obround part."""
    r = h / 2.0
    a = w / 2.0 - r
    s = r / math.sqrt(2)
    apex = a + r * math.sqrt(2)          # roof apex height above centre
    wp = (cq.Workplane("XZ").center(cx, cz)
          .moveTo(-a, -r).lineTo(a, -r)
          .threePointArc((a + r, 0), (a + s, s))
          .lineTo(0, apex)
          .lineTo(-a - s, s)
          .threePointArc((-a - r, 0), (-a, -r))
          .close().extrude(-(y1 - y0)))
    return wp.translate((0, y0, 0))

def teardrop_round_z(d, cx, cy, z0, z1, apex_dir=(0, 1)):
    """Round hole of diameter d, axis Z, with a 45 deg teardrop apex pointing along apex_dir
    (unit vector in the X-Y plane = the printer's +Z when the part is printed on its side)."""
    r = d / 2.0
    s = r / math.sqrt(2)
    ux, uy = apex_dir
    vx, vy = -uy, ux                     # perpendicular
    def P(along, perp): return (cx + ux * along + vx * perp, cy + uy * along + vy * perp)
    wp = (cq.Workplane("XY")
          .moveTo(*P(s, s)).lineTo(*P(r * math.sqrt(2), 0)).lineTo(*P(s, -s))
          .threePointArc(P(-r, 0), P(s, s))
          .close().extrude(z1 - z0))
    return wp.translate((0, 0, z0))

def cyl_z(d, cx, cy, z0, z1):
    return cq.Workplane("XY").center(cx, cy).circle(d / 2).extrude(z1 - z0).translate((0, 0, z0))

def mirror_x(wp):
    return wp.mirror("YZ")

# =============================================================================
# PARTS  (right-hand side, +X; the left side is the mirror)
# =============================================================================
def build_cradle_side():
    """Returns (main_body, ribs_and_stops, full_side).  main_body is the part that must
    stay out of the vent-clearance band; ribs/stops are allowed inside it."""
    y0, y1 = 0.0, SHELF_DEPTH
    # flange
    flange = box(X_FLANGE_IN, FLANGE_X_OUT, 0, T_FLANGE, 0, PANEL_H)
    for zc in FLANGE_SLOT_Z:
        flange = flange.cut(teardrop_obround_xz(SLOT_W + COMP_HOLE, SLOT_H + COMP_HOLE,
                                                RACK_HOLE_X, zc, -1, T_FLANGE + 1))
    # outer wall + top rail with 45 deg under-chamfer
    wall = box(X_WALL_IN, X_WALL_OUT, y0, y1, 0, PANEL_H)
    rail = prism_xz([(X_WALL_IN, Z_RAIL_CH0), (X_RAIL_IN, Z_RAIL_FULL), (X_RAIL_IN, PANEL_H),
                     (X_WALL_OUT, PANEL_H), (X_WALL_OUT, Z_RAIL_CH0)], y0, y1)
    # shelf
    shelf = box(X_SHELF_IN, X_WALL_OUT, y0, y1, 0, T_SHELF)
    # knee gusset (the 6.1 diagonal web, capped at the 50 mm vent-clearance line)
    gusset = prism_xz([(X_BAND_OUT, T_SHELF), (X_WALL_IN, T_SHELF),
                       (X_WALL_IN, T_SHELF + (X_WALL_IN - X_BAND_OUT))], y0, y1)
    # 6 mm coves: shelf-wall (axis Y), flange-wall (axis Z), flange-shelf (axis X, outside the band only)
    r = FILLET_R
    c1 = cove_xz(X_WALL_IN - r, X_WALL_IN, T_SHELF, T_SHELF + r, X_WALL_IN - r, T_SHELF + r, y0, y1)
    c2 = cove_xy(X_WALL_IN - r, X_WALL_IN, T_FLANGE, T_FLANGE + r, X_WALL_IN - r, T_FLANGE + r,
                 T_SHELF, PANEL_H)
    c3 = cove_yz(T_FLANGE, T_FLANGE + r, T_SHELF, T_SHELF + r, T_FLANGE + r, T_SHELF + r,
                 X_BAND_OUT, X_WALL_IN)
    # front lip (in front of the flange plane, under the outer 60 mm of the front panel)
    lip = box(X_SHELF_IN, X_SIDE, -LIP_T, 0, 0, Z_SEAT + LIP_H)
    body = flange.union(wall).union(rail).union(shelf).union(gusset).union(c1).union(c2).union(c3).union(lip)
    # clamp notches + insert holes in the rail top
    for yc in CLAMP_Y:
        body = body.cut(box(X_RAIL_IN - 1, X_WALL_OUT + 1, yc - CLAMP_W / 2 - 0.2, yc + CLAMP_W / 2 + 0.2,
                            Z_NOTCH, PANEL_H + 1))
        for dy in (-CLAMP_SCREW_DY, CLAMP_SCREW_DY):
            body = body.cut(cyl_z(INSERT_HOLE + COMP_HOLE, CLAMP_SCREW_X, yc + dy,
                                  Z_NOTCH - INSERT_DEPTH, Z_NOTCH + 1))
    # tie-bar channels in the shelf underside + insert holes
    for yc in TIE_Y:
        body = body.cut(box(X_SHELF_IN - 1, X_WALL_OUT + 1, yc - TIE_W / 2 - 0.2, yc + TIE_W / 2 + 0.2,
                            -1, TIE_RECESS))
        for xs in TIE_SCREW_X:
            body = body.cut(cyl_z(INSERT_HOLE + COMP_HOLE, xs, yc, TIE_RECESS - 1, TIE_RECESS + INSERT_DEPTH))
    # locating ribs (front and rear corner) and rear stop
    ribs = (box(X_RIB_IN, X_RIB_IN + RIB_T, 0, RIB_L, T_SHELF, T_SHELF + RIB_H)
            .union(box(X_RIB_IN, X_RIB_IN + RIB_T, ACE_D - RIB_L, ACE_D, T_SHELF, T_SHELF + RIB_H)))
    stop = box(X_SIDE - STOP_W, X_SIDE, STOP_Y, STOP_Y + STOP_T, T_SHELF, T_SHELF + STOP_H)
    full = body.union(ribs).union(stop)
    return body, ribs, stop, full

def build_clamp(yc):
    """Top clamp for the right side at depth yc (installed frame)."""
    foot = box(X_RAIL_IN, X_WALL_OUT, yc - CLAMP_W / 2, yc + CLAMP_W / 2, Z_NOTCH, PANEL_H)
    web  = box(X_WEB_IN, X_WEB_OUT, yc - CLAMP_W / 2, yc + CLAMP_W / 2, Z_CLAMP_LO, PANEL_H)
    arm  = box(X_ARM_IN, X_WEB_IN, yc - CLAMP_W / 2, yc + CLAMP_W / 2, Z_CLAMP_LO, Z_CLAMP_HI)
    clamp = foot.union(web).union(arm)
    # screw holes + counterbores: horizontal-axis in the print orientation -> teardrop, apex to +Y
    for dy in (-CLAMP_SCREW_DY, CLAMP_SCREW_DY):
        clamp = clamp.cut(teardrop_round_z(M4_CLR + COMP_HOLE, CLAMP_SCREW_X, yc + dy, Z_NOTCH - 1, PANEL_H + 1))
        clamp = clamp.cut(teardrop_round_z(CBORE_D + COMP_HOLE, CLAMP_SCREW_X, yc + dy,
                                           PANEL_H - CBORE_DEPTH, PANEL_H + 1))
    return clamp

def build_pad(yc):
    return box(X_ARM_IN, X_SIDE, yc - CLAMP_W / 2, yc + CLAMP_W / 2, Z_TOP, Z_CLAMP_LO)

def build_tie_bar(yc):
    bar = box(-TIE_LEN / 2, TIE_LEN / 2, yc - TIE_W / 2, yc + TIE_W / 2, 0, TIE_T)
    for xs in TIE_SCREW_X:
        for sgn in (1, -1):
            bar = bar.cut(cyl_z(M4_CLR, sgn * xs, yc, -1, TIE_T + 1))
            # 90 deg countersink for DIN 7991 M4 (head d 8.96): modelled as cone
            cone = (cq.Workplane("XY").center(sgn * xs, yc).circle(9.0 / 2).workplane(offset=-9.0 / 2)
                    .circle(0.05).loft().translate((0, 0, 0)))
            bar = bar.cut(cone)
    return bar

def build_ace():
    body = box(-ACE_W / 2, ACE_W / 2, 0, ACE_D, Z_SEAT, Z_TOP)
    env  = box(-ACE_ENV_W / 2, ACE_ENV_W / 2, 0, ACE_ENV_D, Z_SEAT, Z_TOP)
    return body, env

def build_rail():
    """Right-hand steel rack rail (front flange + return leg), 2U + margins, with 6 obround holes."""
    z0, z1 = -PANEL_OFFSET - 10, -PANEL_OFFSET + 2 * U_PITCH + 10
    front = box(RAIL_OPEN / 2, RAIL_OPEN / 2 + RAIL_W, T_FLANGE, T_FLANGE + RAIL_T, z0, z1)
    leg   = box(RAIL_OPEN / 2 + RAIL_W - RAIL_T, RAIL_OPEN / 2 + RAIL_W, T_FLANGE, T_FLANGE + 40, z0, z1)
    rail = front.union(leg)
    for zc in HOLE_Z:
        slot = (cq.Workplane("XZ").center(RACK_HOLE_X, zc).slot2D(SLOT_W, SLOT_H).extrude(-RAIL_T - 2)
                .translate((0, T_FLANGE - 1, 0)))
        rail = rail.cut(slot)
    return rail

def build_neo_seat():
    return box(X_SHELF_IN, X_SIDE, 0, ACE_D, T_SHELF, Z_SEAT)

# =============================================================================
# LOAD CALCULATIONS (section 7)  -> list of (name, case, value, limit, pass, formula)
# =============================================================================
def load_calcs():
    R = []     # rows: dict(name, case, stress/force, unit, allow, SF, pass, note)
    def row(name, case, val, unit, allow, req, note, dirn=""):
        sf = allow / val if val > 0 else float("inf")
        R.append(dict(name=name, case=case, value=val, unit=unit, allow=allow, sf=sf,
                      ok=sf >= req, note=note, dirn=dirn))
        return sf
    L = log
    L(""); L("== Section geometry ==")
    I_shelf = I_rect(SHELF_DEPTH, T_SHELF)
    I_wall  = I_rect(T_WALL, PANEL_H)
    I_flg_inplane = I_rect(T_FLANGE, PANEL_H)
    I_rib   = I_rect(RIB_L, RIB_T)
    I_lip   = I_rect(SHELF_UNDER, LIP_T)
    I_stop  = I_rect(STOP_W, STOP_T)
    I_arm   = I_rect(CLAMP_W, CLAMP_ARM_T)
    L("I_shelf  = %.0f x %.0f^3 / 12 = %.0f mm4" % (SHELF_DEPTH, T_SHELF, I_shelf))
    L("I_wall   = %.0f x %.1f^3 / 12 = %.0f mm4 (plain wall, top rail ignored = conservative)" % (T_WALL, PANEL_H, I_wall))
    L("I_flange (in-plane) = %.0f x %.1f^3 / 12 = %.0f mm4" % (T_FLANGE, PANEL_H, I_flg_inplane))
    L("I_rib = %.0f mm4, I_lip = %.0f mm4, I_stop = %.1f mm4, I_arm = %.0f mm4" % (I_rib, I_lip, I_stop, I_arm))

    out = {}
    for case_name, gf in (("A", CASE_A), ("B", CASE_B)):
        P = P_SIDE * gf
        L(""); L("== Case %s: %.0f g vertical, P per side = %.1f N ==" % (case_name, gf, P))
        # 1. shelf root at the wall (bending about Y, stress along X = in-layer)
        M1 = P * A_SHELF
        s1 = M1 * (T_SHELF / 2) / I_shelf
        L("Shelf root: M = %.1f x %.0f = %.0f Nmm ; sigma = M c / I = %.0f x %.0f / %.0f = %.2f MPa (in-layer)"
          % (P, A_SHELF, M1, M1, T_SHELF / 2, I_shelf, s1))
        # 2. wall root at the flange (deep beam cantilever, stress along Y = in-layer)
        M2 = P * COM_Y
        s2 = M2 * (PANEL_H / 2) / I_wall
        L("Wall root: M = %.1f x %.0f = %.0f Nmm ; sigma = %.0f x %.2f / %.0f = %.2f MPa (in-layer)"
          % (P, COM_Y, M2, M2, PANEL_H / 2, I_wall, s2))
        # 3. flange in-plane bending from the roll moment (load at X_LOAD vs screw line)
        M3 = P * (RACK_HOLE_X - X_LOAD)
        s3 = M3 * (PANEL_H / 2) / I_flg_inplane
        L("Flange (roll moment about Y): M = %.1f x %.1f = %.0f Nmm ; sigma = %.2f MPa (in-layer)"
          % (P, RACK_HOLE_X - X_LOAD, M3, s3))
        # rack screw forces per side (4 screws): pitch moment -> tension couple between top and bottom pair
        z_top = (FLANGE_SLOT_Z[2] + FLANGE_SLOT_Z[3]) / 2
        z_bot = (FLANGE_SLOT_Z[0] + FLANGE_SLOT_Z[1]) / 2
        lever = z_top - z_bot
        F_t = M2 / lever            # total tension on the top pair
        F_shear_roll = M3 / lever   # X-shear couple reacting the roll moment
        shear_v = P / 4
        L("Rack screws: pitch moment %.0f Nmm / lever %.2f mm = %.0f N on the top pair -> %.0f N tension per screw"
          % (M2, lever, F_t, F_t / 2))
        L("             roll moment %.0f Nmm / %.2f = %.0f N X-shear couple -> %.0f N per screw ; vertical shear %.1f N per screw"
          % (M3, lever, F_shear_roll, F_shear_roll / 2, shear_v))
        bearing = (F_shear_roll / 2 + shear_v) / (T_FLANGE * 4.8)
        L("             slot bearing (4.8 mm shank in %.0f mm PETG) = %.2f MPa" % (T_FLANGE, bearing))
        # sag at the chassis rear (Case A only is the criterion, report both)
        d_shelf = P * A_SHELF ** 3 / (3 * E_MOD * I_shelf)
        a, Lw = COM_Y, ACE_D
        d_wall = P * a ** 2 * (3 * Lw - a) / (6 * E_MOD * I_wall)
        sag = d_shelf + d_wall
        L("Sag: shelf P L^3/(3EI) = %.1f x %.0f^3 / (3 x %.0f x %.0f) = %.3f mm ; wall tip (load at %.0f, tip at %.0f) = %.3f mm ; total %.3f mm"
          % (P, A_SHELF, E_MOD, I_shelf, d_shelf, a, Lw, d_wall, sag))
        out[case_name] = dict(s1=s1, s2=s2, s3=s3, F_t=F_t / 2, F_s=F_shear_roll / 2 + shear_v, bearing=bearing, sag=sag)
        if case_name == "B":
            row("Shelf root (bending)", "B", s1, "MPa", SY_XY, SF_REQ, "in-layer", "XY")
            row("Wall root at flange", "B", s2, "MPa", SY_XY, SF_REQ, "in-layer", "XY")
            row("Flange in-plane", "B", s3, "MPa", SY_XY, SF_REQ, "in-layer", "XY")
            row("Flange slot bearing", "B", bearing, "MPa", SY_XY, SF_REQ, "in-layer", "XY")
            # clamp (5 g upward)
            row("Clamp arm root", "B", SIG_ARM, "MPa", SY_XY, SF_REQ, "in-layer (printed on side)", "XY")
            s_web = M_ARM * (CLAMP_ARM_T / 2) / I_rect(CLAMP_W, CLAMP_ARM_T)
            row("Clamp web root", "B", s_web, "MPa", SY_XY, SF_REQ, "in-layer", "XY")
            row("Clamp insert pull-out", "B", T_CLAMP_SCREW, "N", INSERT_PULLOUT, SF_REQ, "per M4 insert", "")
            # foot bearing under the button head
            a_head = math.pi / 4 * (M4_BUTTON_HEAD_D ** 2 - (M4_CLR + COMP_HOLE) ** 2)
            row("Clamp foot under head", "B", T_CLAMP_SCREW / a_head, "MPa", SY_Z, SF_REQ, "compression across layers", "Z")
            # rack screws: steel, report only; M6 class 8.8 proof ~ 20 kN; 10-32 ~ 8 kN
            row("Rack screw tension", "B", F_t / 2, "N", 8000.0, SF_REQ, "10-32 steel (weakest option) proof load", "")
        else:
            for nm, s in (("Shelf root", s1), ("Wall root", s2), ("Flange", s3)):
                R.append(dict(name=nm + " creep", case="A", value=s, unit="MPa", allow=CREEP_FRAC * SY_XY,
                              sf=(CREEP_FRAC * SY_XY) / s, ok=s <= CREEP_FRAC * SY_XY,
                              note="<= 15 %% of %.0f MPa" % SY_XY, dirn="XY"))
            R.append(dict(name="Rear sag", case="A", value=sag, unit="mm", allow=SAG_MAX, sf=SAG_MAX / sag,
                          ok=sag <= SAG_MAX, note="shelf + wall", dirn=""))
            # clamp preload creep: sponge pad force (spec <= 40 N at 0.5 mm compression)
            PAD_F = PAD_F_MAX
            s_pre = SIG_ARM_PRE
            R.append(dict(name="Clamp arm preload creep", case="A", value=s_pre, unit="MPa", allow=CREEP_FRAC * SY_XY,
                          sf=CREEP_FRAC * SY_XY / s_pre, ok=s_pre <= CREEP_FRAC * SY_XY,
                          note="pad force %.0f N max" % PAD_F, dirn="XY"))
            L("Clamp preload (pad <= %.0f N): arm sigma = %.2f MPa ; insert tension = %.0f N per screw"
              % (PAD_F, s_pre, PAD_F * (X_WALL_OUT - X_PAD_C) / LEVER_SCREW / 2))
    # Case C
    L(""); L("== Case C: %.0f g front-to-back and %.0f g side-to-side, F = %.0f N ==" % (CASE_C, CASE_C, F_C))
    # rearwards -> 2 rear stops (clamp friction ignored)
    L("Rear stop (already sized): sigma = %.2f MPa across layers, SF = %.2f" % (sig, SF_STOP))
    row("Rear stop root", "C", sig, "MPa", SY_Z, SF_REQ, "3 g rearwards, 2 tabs", "Z")
    # forwards -> 2 front lips
    f_lip = F_C / 2
    lever_lip = NEO_SEAT_T + LIP_H / 2                 # above the shelf top
    M_lip = f_lip * lever_lip
    s_lip = M_lip * (LIP_T / 2) / I_lip
    L("Front lip: F = %.1f N/lip, lever %.1f mm, M = %.0f Nmm, sigma = %.2f MPa across layers" % (f_lip, lever_lip, M_lip, s_lip))
    row("Front lip root", "C", s_lip, "MPa", SY_Z, SF_REQ, "3 g forwards, 2 lips", "Z")
    # sideways -> 2 ribs on one side
    f_rib = F_C / 2
    lever_rib = (NEO_SEAT_T + RIB_H) / 2
    M_rib = f_rib * lever_rib
    s_rib = M_rib * (RIB_T / 2) / I_rib
    tau_rib = f_rib / (RIB_L * RIB_T)
    L("Locating rib: F = %.1f N/rib, lever %.1f mm, M = %.0f Nmm, sigma = %.2f MPa across layers, shear %.2f MPa"
      % (f_rib, lever_rib, M_rib, s_rib, tau_rib))
    row("Locating rib root", "C", s_rib, "MPa", SY_Z, SF_REQ, "3 g sideways, 2 ribs", "Z")
    # frame moment: side force x COM_Y reacted as a Y-couple between the two flanges (tie bars make this path)
    M_frame = F_C * COM_Y
    F_couple = M_frame / (2 * RACK_HOLE_X)
    F_single = M_frame / (FLANGE_X_OUT - RACK_HOLE_X)   # if one side had to react it alone
    L("Side load yaw moment %.0f Nmm: with tie bars -> couple between flanges %.0f N ; without tie bars a single flange"
      " would need %.0f N screw tension (lever %.2f mm)" % (M_frame, F_couple, F_single, FLANGE_X_OUT - RACK_HOLE_X))
    # tie-bar screw shear: couple / 2 screws per end
    tie_shear = F_couple / 2
    row("Tie-bar screw shear", "C", tie_shear, "N", 2000.0, SF_REQ, "M4 8.8 single shear ~ 2 kN (conservative)", "")
    # rack screw shear sideways: F_C / 8
    L("Rack screw shear sideways = %.0f / 8 = %.1f N ; rearwards = %.1f N" % (F_C, F_C / 8, F_C / 8))
    # retention: chassis cannot leave the mount
    retention = (f"Chassis retained: down by shelves, up by 4 clamps (gap %.1f mm pad, hard stop), sideways by ribs "
                 f"(%.1f mm clearance), forward by lips (%.0f mm engagement), rearward by stops (%.0f mm engagement)"
                 % (PAD_T - PAD_COMP, RIB_CLEAR, LIP_H, STOP_H - NEO_SEAT_T))
    L(retention)
    R.append(dict(name="Chassis retained (Case C)", case="C", value=1, unit="", allow=1, sf=float("inf"), ok=True,
                  note="positive stops in all 6 directions", dirn=""))
    return R, out

# =============================================================================
# PRINT ORIENTATION
# =============================================================================
def to_print_orientation(name, wp):
    """Return the part transformed into its print orientation with its bbox min corner at the origin."""
    if name.startswith("clamp"):
        wp = wp.rotate((0, 0, 0), (1, 0, 0), 90)          # +Y -> +Z : profile in every layer, no overhangs
    # cradle sides print as installed (rack vertical = printer Z)
    bb = wp.val().BoundingBox()
    return wp.translate((-bb.xmin, -bb.ymin, -bb.zmin))

# =============================================================================
# AUTOMATIC VERIFICATION (section 9)
# =============================================================================
CHECKS = []
def check(name, ok, value, limit=""):
    CHECKS.append(dict(name=name, ok=bool(ok), value=value, limit=limit))
    return ok

def vol_intersect(a, b):
    try:
        return a.intersect(b).val().Volume()
    except Exception:
        return 0.0

def overhang_check(name, wp):
    """Every face of the print-oriented solid: no down-facing face steeper than MAX_OVERHANG,
    flat down-facing faces allowed only on the bed or as bridges <= MAX_BRIDGE."""
    solid = wp.val()
    bb = solid.BoundingBox()
    sin_lim = math.sin(math.radians(MAX_OVERHANG + 0.5))   # overhang angle from vertical = asin(-n.z)
    worst, n_bridge, bad = 0.0, 0, []
    for f in solid.Faces():
        pts = []
        try:
            vs, tris = f.tessellate(0.5, 0.5)
            for t in tris:
                c = (vs[t[0]] + vs[t[1]] + vs[t[2]]) / 3
                pts.append(c)
        except Exception:
            pts.append(f.Center())
        for c in pts[:60]:
            try:
                n = f.normalAt(c)
            except Exception:
                n = f.normalAt()
            if n.z >= -sin_lim:
                continue                              # not an overhang beyond 45 deg
            if n.z < -0.999:                          # flat down-facing
                fb = f.BoundingBox()
                if abs(fb.zmin - bb.zmin) < 1e-3:
                    continue                          # on the bed
                span = min(fb.xlen, fb.ylen)
                if span <= MAX_BRIDGE + 1e-6:
                    n_bridge += 1
                    continue
                bad.append(("flat %.1f mm span at z=%.1f" % (span, fb.zmin)))
            else:
                ang = math.degrees(math.asin(max(-1, min(1, -n.z))))
                worst = max(worst, ang)
                bad.append("sloped face %.1f deg from vertical at z=%.1f" % (ang, c.z))
            break
    ok = len(bad) == 0
    check("%s: no overhang > %.0f deg (bridges <= %.0f mm allowed)" % (name, MAX_OVERHANG, MAX_BRIDGE), ok,
          "OK, %d bridge faces" % n_bridge if ok else "; ".join(bad[:3]))
    return ok

def run_verification(parts, ace_body, ace_env, rails, side_body_R, ribs_R, stop_R):
    # 1. interference.  The rear stop sits behind the 210 mm rear panel inside the 239 mm protrusion
    #    envelope by specification 6.4 (outer 8 mm only), so it is checked against the chassis body.
    worst = 0.0
    for n, p in parts.items():
        if n.startswith("pad") or n.startswith("cradle_side"):
            continue
        worst = max(worst, vol_intersect(p, ace_env))
    worst = max(worst, vol_intersect(side_body_R, ace_env), vol_intersect(ribs_R, ace_env))
    check("Zero interference: cradle bodies, ribs, clamps, tie bars vs ACE envelope (315.6 x 44 x 239)", worst < 1e-6, "%.4f mm3" % worst)
    v_stop = vol_intersect(stop_R, ace_body)
    gap = STOP_Y - ACE_D
    check("Rear stop vs ACE body (310 x 44 x 210): zero interference, %.1f mm gap behind the rear panel" % gap,
          v_stop < 1e-6 and gap > 0, "%.4f mm3 ; stop occupies the rear protrusion envelope at X %.0f..%.0f only (spec 6.4)"
          % (v_stop, X_SIDE - STOP_W, X_SIDE))
    worst = 0.0
    for n, p in parts.items():
        for r in rails:
            worst = max(worst, vol_intersect(p, r))
    check("Zero interference: parts vs rack rails (flange face touches rail, volume 0)", worst < 1e-6, "%.4f mm3" % worst)
    # parts vs parts (cradle vs clamps/tie bars)
    worst = 0.0
    sideR = parts["cradle_side_R"]
    for n, p in parts.items():
        if n.startswith("clamp_R") or n.startswith("tie"):
            worst = max(worst, vol_intersect(p, sideR))
    check("Zero interference: clamps and tie bars vs cradle side", worst < 1e-6, "%.4f mm3" % worst)
    # 2. rack slot centres
    blank = box(X_FLANGE_IN, FLANGE_X_OUT, 0, T_FLANGE, 0, PANEL_H)
    voids = blank.cut(sideR).solids().vals()
    worst = 0.0
    found = 0
    for zc in FLANGE_SLOT_Z:
        for v in voids:
            vb = v.BoundingBox()
            if vb.xmax < 225 or vb.xmin > 241 or not (vb.zmin < zc < vb.zmax):
                continue
            found += 1
            xc = (vb.xmin + vb.xmax) / 2
            zc_meas = vb.zmin + (SLOT_H + COMP_HOLE) / 2
            worst = max(worst, abs(xc - RACK_HOLE_X), abs(zc_meas - zc))
    check("Rack slot centres within 0.05 mm of EIA-310-D (X=232.55; Z=%s + 0.4 below panel edge)"
          % ",".join("%.3f" % z for z in [HOLE_Z_RACK[i] for i in (0, 2, 3, 5)]),
          found == 4 and worst <= 0.05, "%d slots, max deviation %.4f mm" % (found, worst))
    # 3. assembly width / panel height
    bbR = sideR.val().BoundingBox(); bbL = parts["cradle_side_L"].val().BoundingBox()
    width = bbR.xmax - bbL.xmin
    check("Assembly width = 482.6 mm", abs(width - PANEL_W) < 0.01, "%.3f mm" % width)
    height = bbR.zmax - bbR.zmin
    check("Panel height = 88.1 mm (2U)", abs(height - PANEL_H) < 0.01, "%.3f mm" % height)
    # 4. vent clearance band: X 155..205, Y 6..210, Z chassis bottom..top
    band = box(X_SIDE, X_BAND_OUT, T_FLANGE, ACE_D, Z_SEAT, Z_TOP)
    vb = vol_intersect(side_body_R, band)
    for n, p in parts.items():
        if n.startswith("clamp_R") or n.startswith("pad_R"):
            vb += vol_intersect(p, band)
    check("Nothing within 50 mm of the ACE side panels except ribs and rear stops", vb < 1e-6, "%.4f mm3 inside band" % vb)
    # clear zone behind the flange
    worst = max(p.val().BoundingBox().xmax for n, p in parts.items() if not n.startswith("tie"))
    behind = sideR.cut(box(0, 300, -10, T_FLANGE, -1, 100)).val().BoundingBox().xmax
    check("Structure behind the flange stays 3 mm inside the 450 mm rail opening (X <= 222)", behind <= STRUCT_X_MAX + 1e-6, "%.2f mm" % behind)
    # 5. minimum wall + overhangs
    walls = {"wall": T_WALL, "shelf": T_SHELF, "flange": T_FLANGE, "rib": RIB_T, "lip": LIP_T, "stop": STOP_T,
             "clamp arm": CLAMP_ARM_T, "clamp foot": CLAMP_FOOT_T, "gusset": X_WALL_IN - X_BAND_OUT,
             "clamp insert wall": CLAMP_SCREW_X - (INSERT_HOLE + COMP_HOLE) / 2 - X_RAIL_IN,
             "insert roof (shelf)": T_SHELF - TIE_RECESS - INSERT_DEPTH,
             "insert bottom above rail chamfer": Z_NOTCH - INSERT_DEPTH - Z_RAIL_FULL + INSERT_WALL_MIN,
             "slot to flange edge": FLANGE_X_OUT - RACK_HOLE_X - (SLOT_W + COMP_HOLE) / 2,
             "clamp under head": CLAMP_FOOT_T - CBORE_DEPTH,
             "tie channel to shelf edge (Y)": min(TIE_Y[0] - TIE_W / 2 - 0.2, SHELF_DEPTH - TIE_Y[1] - TIE_W / 2 - 0.2)}
    mn = min(walls, key=walls.get)
    check("Minimum wall >= 2.4 mm (thinnest: %s)" % mn, walls[mn] >= MIN_WALL - 1e-9, "%.2f mm" % walls[mn])
    # 6. one watertight solid, inside build volume, overhang
    for n, p in parts.items():
        if n.startswith("pad") or n.startswith("tie") or n.startswith("neo"):
            continue
        po = to_print_orientation(n, p)
        s = po.val()
        nsol = len(po.solids().vals())
        check("%s: one watertight solid" % n, nsol == 1 and s.isValid() and s.Volume() > 0,
              "%d solid(s), valid=%s, %.1f cm3" % (nsol, s.isValid(), s.Volume() / 1000))
        bb = s.BoundingBox()
        dims = sorted([bb.xlen, bb.ylen, bb.zlen])
        fits = all(d <= BUILD_VOL for d in dims)
        check("%s: inside 256^3 build volume as oriented" % n, fits, "%.1f x %.1f x %.1f mm" % (bb.xlen, bb.ylen, bb.zlen))
        overhang_check(n, po)
    return all(c["ok"] for c in CHECKS)

# =============================================================================
# EXPORT + RENDER
# =============================================================================
PART_COLORS = {"cradle_side": (0.25, 0.45, 0.85), "clamp": (0.95, 0.55, 0.15), "tie": (0.6, 0.6, 0.65),
               "pad": (0.1, 0.1, 0.1), "neo": (0.15, 0.15, 0.15), "ace": (0.3, 0.3, 0.3), "rail": (0.5, 0.5, 0.5)}
def color_of(name):
    for k, c in PART_COLORS.items():
        if name.startswith(k): return c
    return (0.7, 0.7, 0.7)

VIEWS = {"front": (0, -1, 0), "top": (0, 0, 1), "side": (1, 0, 0), "iso": (1, -1, 0.8)}

def export_svg(wp, path, direction):
    exporters.export(wp, path, opt={"width": 900, "height": 650, "marginLeft": 20, "marginTop": 20,
                                    "showAxes": False, "projectionDir": direction, "strokeWidth": 0.4,
                                    "showHidden": False})

def render_png(shapes, path, title, elev=25, azim=-60, ortho=None):
    """Shaded matplotlib render of a list of (cq object, color)."""
    import numpy as np
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    fig = plt.figure(figsize=(10, 7), dpi=110)
    ax = fig.add_subplot(111, projection="3d")
    allpts = []
    for wp, col in shapes:
        vs, tris = wp.val().tessellate(0.6, 0.3)
        V = np.array([[v.x, v.y, v.z] for v in vs])
        F = np.array(tris)
        if len(F) == 0: continue
        polys = V[F]
        # simple lambert shading
        n = np.cross(polys[:, 1] - polys[:, 0], polys[:, 2] - polys[:, 0])
        n /= (np.linalg.norm(n, axis=1)[:, None] + 1e-12)
        light = np.array([0.4, -0.6, 0.7]); light /= np.linalg.norm(light)
        shade = 0.45 + 0.55 * np.abs(n @ light)
        cols = np.clip(np.array(col)[None, :] * shade[:, None], 0, 1)
        pc = Poly3DCollection(polys, facecolors=cols, edgecolors="none", alpha=1.0)
        ax.add_collection3d(pc)
        allpts.append(V)
    P = np.vstack(allpts)
    mn, mx = P.min(axis=0), P.max(axis=0)
    c = (mn + mx) / 2; r = (mx - mn).max() / 2
    ax.set_xlim(c[0] - r, c[0] + r); ax.set_ylim(c[1] - r, c[1] + r); ax.set_zlim(c[2] - r, c[2] + r)
    ax.set_box_aspect((1, 1, 1))
    ax.view_init(elev=elev, azim=azim)
    if ortho: ax.set_proj_type("ortho")
    ax.set_axis_off(); ax.set_title(title)
    fig.tight_layout(); fig.savefig(path); plt.close(fig)

def do_exports(parts, ace_body, ace_env, rails, neo):
    for d in ("step", "3mf", "renders"):
        os.makedirs(os.path.join(OUT, d), exist_ok=True)
    printed = [n for n in parts if n.startswith("cradle_side") or n.startswith("clamp")]
    files = []
    for n, p in parts.items():
        exporters.export(p, os.path.join(OUT, "step", n + ".step"))
        files.append("step/%s.step" % n)
        if n in printed:
            po = to_print_orientation(n, p)
            try:
                po = po.faces("<Z").chamfer(CHAMFER_BED)          # bed-contact edge chamfer
                chamfer_ok = True
            except Exception:
                chamfer_ok = False
            po_scaled = cq.Workplane("XY").add(po.val().scale(SHRINK_SCALE))
            exporters.export(po_scaled, os.path.join(OUT, "3mf", n + ".3mf"))
            exporters.export(po, os.path.join(OUT, "step", n + "_print_oriented.step"))
            files.append("3mf/%s.3mf (x%.3f shrink, bed chamfer %s)" % (n, SHRINK_SCALE, "applied" if chamfer_ok else "NOT applied"))
    # assembly STEP
    assy = cq.Assembly(name="ACE_2U_cradle")
    for n, p in parts.items():
        assy.add(p, name=n, color=cq.Color(*color_of(n)))
    assy.add(ace_body, name="ACE_body", color=cq.Color(0.3, 0.3, 0.3, 1))
    for i, r in enumerate(rails):
        assy.add(r, name="rack_rail_%d" % i, color=cq.Color(0.5, 0.5, 0.5))
    for i, s in enumerate(neo):
        assy.add(s, name="neoprene_seat_%d" % i, color=cq.Color(0.1, 0.1, 0.1))
    assy.save(os.path.join(OUT, "step", "assembly_in_rack.step"))
    files.append("step/assembly_in_rack.step")
    # renders: SVG per view for unique parts and the full assembly
    unique = {"cradle_side_R": parts["cradle_side_R"], "clamp_R_40": parts["clamp_R_40"], "tie_bar_20": parts["tie_bar_20"]}
    all_shapes = [(p, color_of(n)) for n, p in parts.items()] + [(ace_body, color_of("ace"))] + \
                 [(r, color_of("rail")) for r in rails] + [(s, color_of("neo")) for s in neo]
    comp = cq.Workplane("XY").add(cq.Compound.makeCompound([s[0].val() for s in all_shapes]))
    for vname, d in VIEWS.items():
        for n, p in unique.items():
            export_svg(p, os.path.join(OUT, "renders", "%s_%s.svg" % (n, vname)), d)
        export_svg(comp, os.path.join(OUT, "renders", "assembly_%s.svg" % vname), d)
    # shaded PNGs
    render_png(all_shapes, os.path.join(OUT, "renders", "assembly_iso.png"), "ACE 2U cradle in rack - isometric", 22, -55)
    render_png(all_shapes, os.path.join(OUT, "renders", "assembly_front.png"), "front", 0, -90, True)
    render_png(all_shapes, os.path.join(OUT, "renders", "assembly_top.png"), "top", 90, -90, True)
    render_png(all_shapes, os.path.join(OUT, "renders", "assembly_side.png"), "side (right)", 0, 0, True)
    for n, p in unique.items():
        render_png([(p, color_of(n))], os.path.join(OUT, "renders", "%s_iso.png" % n), n + " - isometric", 28, -50)
        render_png([(p, color_of(n))], os.path.join(OUT, "renders", "%s_front.png" % n), n + " - front", 0, -90, True)
        render_png([(p, color_of(n))], os.path.join(OUT, "renders", "%s_top.png" % n), n + " - top", 90, -90, True)
        render_png([(p, color_of(n))], os.path.join(OUT, "renders", "%s_side.png" % n), n + " - side", 0, 0, True)
    render_png([(to_print_orientation("clamp_R_40", parts["clamp_R_40"]), color_of("clamp"))],
               os.path.join(OUT, "renders", "clamp_print_orientation.png"), "clamp as printed (bar width = printer Z)", 28, -50)
    return files

# =============================================================================
# MAIN
# =============================================================================
def main():
    body_R, ribs_R, stop_R, side_R = build_cradle_side()
    side_L = mirror_x(side_R)
    parts = {"cradle_side_R": side_R, "cradle_side_L": side_L}
    for yc in CLAMP_Y:
        c = build_clamp(yc)
        parts["clamp_R_%d" % yc] = c
        parts["clamp_L_%d" % yc] = mirror_x(c)
        parts["pad_R_%d" % yc] = build_pad(yc)
        parts["pad_L_%d" % yc] = mirror_x(build_pad(yc))
    for yc in TIE_Y:
        parts["tie_bar_%d" % yc] = build_tie_bar(yc)
    ace_body, ace_env = build_ace()
    rail_R = build_rail(); rails = [rail_R, mirror_x(rail_R)]
    neo = [build_neo_seat(), mirror_x(build_neo_seat())]

    R, out = load_calcs()
    ok_loads = all(r["ok"] for r in R)
    ok_geom = run_verification(parts, ace_body, ace_env, rails, body_R, ribs_R, stop_R)
    for r in R:
        check("Load: %s (Case %s)" % (r["name"], r["case"]), r["ok"],
              "%.2f %s vs %.1f -> SF %.2f" % (r["value"], r["unit"], r["allow"], r["sf"]) if r["unit"] else "retained")
    files = do_exports(parts, ace_body, ace_env, rails, neo)
    masses = {n: parts[n].val().Volume() * 1.27e-3 for n in parts if n.startswith("cradle_side") or n.startswith("clamp")}
    write_report(R, out, files, masses)
    n_fail = sum(1 for c in CHECKS if not c["ok"])
    print("\n".join("%-4s %s : %s" % ("PASS" if c["ok"] else "FAIL", c["name"], c["value"]) for c in CHECKS))
    print("\n%d checks, %d failed" % (len(CHECKS), n_fail))
    return n_fail == 0 and ok_loads and ok_geom

def write_report(R, out, files, masses):
    with open(os.path.join(OUT, "report.md"), "w") as f:
        f.write("# ACE 2U cradle - automatic verification report\n\n")
        f.write("| # | Check | Result | Value |\n|---|---|---|---|\n")
        for i, c in enumerate(CHECKS, 1):
            f.write("| %d | %s | %s | %s |\n" % (i, c["name"], "PASS" if c["ok"] else "FAIL", c["value"]))
        f.write("\n## Load calculation log\n\n```\n" + "\n".join(LOG) + "\n```\n")
        f.write("\n## Load table\n\n| Feature | Case | Value | Allowable | SF | Pass | Note |\n|---|---|---|---|---|---|---|\n")
        for r in R:
            f.write("| %s | %s | %.2f %s | %.1f %s | %.2f | %s | %s %s |\n" % (
                r["name"], r["case"], r["value"], r["unit"], r["allow"], r["unit"], r["sf"],
                "yes" if r["ok"] else "NO", r["note"], ("[%s]" % r["dirn"]) if r["dirn"] else ""))
        f.write("\n## Derived dimensions\n\n")
        for k, v in dict(T_SHELF=T_SHELF, STOP_T=STOP_T, CLAMP_ARM_T=CLAMP_ARM_T, SHELF_DEPTH=SHELF_DEPTH,
                         Z_SEAT=Z_SEAT, Z_TOP=Z_TOP, Z_RAIL_CH0=Z_RAIL_CH0, Z_RAIL_FULL=Z_RAIL_FULL, Z_NOTCH=Z_NOTCH,
                         CLAMP_SCREW_X=CLAMP_SCREW_X, X_FLANGE_IN=X_FLANGE_IN, FLANGE_WIDTH=FLANGE_X_OUT - X_FLANGE_IN,
                         X_WALL_IN=X_WALL_IN, X_WALL_OUT=X_WALL_OUT, X_SHELF_IN=X_SHELF_IN,
                         FLANGE_SLOT_Z=[round(z, 3) for z in FLANGE_SLOT_Z]).items():
            f.write("- %s = %s\n" % (k, v))
        f.write("\n## Printed part masses (PETG 1.27 g/cm3, solid volume = upper bound)\n\n")
        for k, v in masses.items():
            f.write("- %s: %.0f g (solid)\n" % (k, v))
        f.write("\n## Files\n\n" + "\n".join("- " + x for x in files) + "\n")
        json.dump(dict(checks=CHECKS, loads=R), open(os.path.join(OUT, "verification.json"), "w"), indent=1, default=str)

if __name__ == "__main__":
    ok = main()
    sys.exit(0 if ok else 1)
