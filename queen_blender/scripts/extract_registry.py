"""Recover THE CONTROL REGISTRY (tex2/J.json) from THE_QUEEN.blend.

The original j60_ink.py was not available, so this inverts build3.py's mapping
S(x,y) = ((x-900)/100, -(y-415)/100) on the scene's object positions.
Every control in SPEC is snapped to exactly one scene object of the right kind;
the script fails if any scene control is unclaimed or claimed twice.

Run:  .venv/bin/python queen_blender/scripts/extract_registry.py
Also extracts the packed textures to queen_blender/textures/.
"""
import json
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BLEND = os.path.join(ROOT, "THE_QUEEN.blend")


def logical(o):
    p = o.matrix_world.translation
    return round(p.x * 100 + 900, 2), round(415 - p.y * 100, 2)


def base_colour(m):
    for n in m.node_tree.nodes:
        if n.type == "BSDF_PRINCIPLED":
            return [round(c, 3) for c in n.inputs["Base Color"].default_value[:3]]
    return [0, 0, 0]


def colour_name(rgb):
    named = {
        "cream": (0.82, 0.77, 0.61), "red": (0.63, 0.03, 0.02), "blue": (0.02, 0.14, 0.72),
        "black": (0.01, 0.01, 0.01), "orange": (0.75, 0.2, 0.01), "grey": (0.26, 0.27, 0.31),
        "ivory": (0.63, 0.6, 0.54), "yellow": (0.81, 0.57, 0.04), "white": (0.81, 0.81, 0.78),
        "silver": (0.69, 0.69, 0.65),
    }
    return min(named, key=lambda k: sum((a - b) ** 2 for a, b in zip(named[k], rgb)))


def kind(o):
    if o.type != "MESH" or not o.active_material:
        return None
    mat = o.active_material.name.split(".")[0]
    mesh = o.data.name.split(".")[0]
    if mesh == "Cylinder" and mat == "cap":
        return "knob"
    return {"scap": "slider", "sw_slot": "switch", "bcap": "button", "led": "led",
            "disp": "display", "wheel": "wheel", "white": "wk", "bk": "bk"}.get(mat)


# ---------------------------------------------------------------- the spec
OP_KNOBS = ["ratio", "level", "detune", "feedback", "velocity"]
OP_LABELS = ["RATIO", "LEVEL", "DETUNE", "FEEDBK", "VELOC"]
OP_X0 = [174.0, 558.5, 943.0, 1327.5]


def spec():
    s = []
    for n in range(4):
        for k in range(5):
            s.append(dict(id=f"op{n+1}_{OP_KNOBS[k]}", t="knob", x=OP_X0[n] + k * 74.625, y=192,
                          param=f"op{n+1}_{OP_KNOBS[k]}", label=OP_LABELS[k], group=f"FM OPERATOR {n+1}"))
    s.append(dict(id="tune", t="knob", x=1620, y=476, param="tune", label="TUNE", group="CHORUS"))

    sliders = [(162, "volume", "VOL", "CONTROLLERS"), (202, "bend_sens_dco", "DCO", "CONTROLLERS"),
               (240, "bend_sens_vcf", "VCF", "CONTROLLERS"), (360.8, "arp_rate", "RATE", "ARPEGGIO"),
               (543.5, "lfo_rate", "RATE", "LFO"), (577.5, "lfo_delay", "DELAY", "LFO"),
               (678.3, "dco_lfo", "LFO", "DCO"), (714.3, "dco_pwm", "PWM", "DCO"),
               (804.3, "sub_level", "SUB", "DCO"), (840.2, "noise_level", "NOISE", "DCO"),
               (959, "hpf", "HPF", "HPF"), (1019.7, "vcf_freq", "FREQ", "VCF"), (1061.7, "vcf_res", "RES", "VCF"),
               (1103.7, "vcf_env", "ENV", "VCF"), (1145.7, "vcf_lfo", "LFO", "VCF"), (1187.8, "vcf_kybd", "KYBD", "VCF"),
               (1290.5, "vca_level", "LEVEL", "VCA"), (1397.2, "attack", "A", "ENV"), (1437.2, "decay", "D", "ENV"),
               (1477.3, "sustain", "S", "ENV"), (1517.3, "release", "R", "ENV")]
    for x, p, lab, g in sliders:
        s.append(dict(id=p, t="slider", x=x, y=382, param=p, label=lab, group=g))

    # positions are top -> bottom, given as choice indices of the parameter
    switches = [(284, "octave", [2, 1, 0], ["UP", "NORM", "DOWN"], "CONTROLLERS"),
                (404.8, "arp_mode", [0, 1, 2], ["UP", "U&D", "DOWN"], "ARPEGGIO"),
                (466.8, "arp_range", [0, 1, 2], ["1", "2", "3"], "ARPEGGIO"),
                (603.5, "lfo_trig_mode", [0, 1], ["AUTO", "MAN"], "LFO"),
                (740.3, "pwm_mode", [0, 1, 2], ["ENV", "MAN", "LFO"], "DCO"),
                (1227.7, "vcf_env_polarity", [0, 1], ["+", "-"], "VCF"),
                (1322.5, "vca_mode", [0, 1], ["ENV", "GATE"], "VCA")]
    for x, p, vals, labs, g in switches:
        s.append(dict(id=p, t="switch", x=x, y=362 if len(vals) == 3 else 354, param=p,
                      positions=vals, labels=labs, group=g))

    def btn(id, x, y, label, group, led=None, **kw):
        s.append(dict(id=id, t="button", x=x, y=y, label=label, group=group, **kw))
        if led:
            s.append(dict(id=id + "_led", t="led", x=led[0], y=led[1], led_for=id, group=group))

    for b in range(5):
        btn(f"bank_{b+1}", 839 + 50 * b, 62, str(b + 1), "MEMORY", (839 + 50 * b, 41), action=f"bank:{b}")
    for p in range(8):
        btn(f"patch_{p+1}", 839 + 50 * p, 102, str(p + 1), "MEMORY", (839 + 50 * p, 81), action=f"patch:{p}")
    btn("manual", 1115, 62, "MANUAL", "MEMORY", (1115, 41), action="manual")
    btn("write", 1195, 62, "WRITE", "MEMORY", (1195, 41), action="write")
    for i, a in enumerate(["save", "verify", "load"]):
        btn(a, 1283, 50 + 26 * i, a.upper(), "MEMORY", (1236, 50 + 26 * i), action=a)
    for a in range(8):
        col, row = a % 4, a // 4
        x = [1403.2, 1473.8, 1544.3, 1614.8][col]
        btn(f"algo_{a+1}", x, [63, 101][row], str(a + 1), "FM ALGORITHM", (x + 25.3, [56, 94][row]),
            param="fm_algorithm", value=a)
    for i, lab in enumerate(["16'", "8'", "4'"]):
        btn(f"range_{lab[:-1]}", 894.2, 343 + 32 * i, lab, "DCO", (868.3, 343 + 32 * i), param="dco_range", value=i)
    btn("chorus_1", 1620, 355, "I", "CHORUS", (1620, 331), param="chorus_1")
    btn("chorus_2", 1620, 405, "II", "CHORUS", (1620, 381), param="chorus_2")
    btn("lfo_trigger", 268, 498, "LFO TRIG", "CONTROLLERS", param="lfo_trigger", momentary=True)
    btn("arp_on", 369.8, 498, "ARP", "ARPEGGIO", (369.8, 477), param="arp_on")
    btn("hold", 421.8, 498, "HOLD", "ARPEGGIO", (421.8, 477), param="hold")
    btn("key_transpose", 477.8, 498, "KEY TR", "ARPEGGIO", (477.8, 477), param="key_transpose")
    btn("wave_pulse", 700.3, 498, "PULSE", "DCO", (700.3, 477), param="wave_pulse")
    btn("wave_saw", 774.2, 498, "SAW", "DCO", (774.2, 477), param="wave_saw")
    btn("wave_sub", 848.3, 498, "SUB", "DCO", (848.3, 477), param="wave_sub")

    s.append(dict(id="display", t="display", x=750, y=72, group="MEMORY"))
    s.append(dict(id="bend_wheel", t="wheel", x=42, y=687, source="pitchbend", label="BEND"))
    s.append(dict(id="mod_wheel", t="wheel", x=80, y=687, source="cc1", label="MOD"))
    return s


def main():
    bpy.ops.wm.open_mainfile(filepath=BLEND)

    for im in bpy.data.images:
        if im.packed_file:
            path = os.path.join(ROOT, "textures", im.name)
            im.filepath_raw = path
            im.file_format = "PNG"
            im.save()
            print("texture", path)

    pool = {}
    for o in bpy.data.objects:
        k = kind(o)
        if k:
            pool.setdefault(k, []).append(o)

    claimed = set()
    out = []
    for c in spec():
        cands = pool[c["t"]]
        wy = 0 if c["t"] == "slider" else 1  # slider caps sit anywhere along the slot
        o = min(cands, key=lambda o: (logical(o)[0] - c["x"]) ** 2 + wy * (logical(o)[1] - c["y"]) ** 2)
        x, y = logical(o)
        if abs(x - c["x"]) > 1.5 or wy * abs(y - c["y"]) > 1.5:
            sys.exit(f"no scene object for {c['id']} near {c['x']},{c['y']} (closest {o.name} at {x},{y})")
        if o.name in claimed:
            sys.exit(f"{o.name} claimed twice ({c['id']})")
        claimed.add(o.name)
        c["x"], c["y"], c["obj"] = x, y, o.name
        d = o.dimensions
        if c["t"] == "knob":
            knurl = [k for k in bpy.data.objects if k.data and k.data.name.startswith("knurl")
                     and abs(logical(k)[0] - x) < 1 and abs(logical(k)[1] - y) < 1][0]
            c["r"] = round(knurl.dimensions.x * 50, 2)
            c["cap"] = colour_name(base_colour(o.active_material))
            c["v"] = 0.5
        elif c["t"] == "slider":
            # cap object, not the slot: value from cap offset (travel 112 units, centre 382)
            c["w"], c["h"] = 23.0, 112.0
            c["cap"] = colour_name(base_colour(o.active_material))
            c["v"] = round(0.5 + (382 - y) / 112, 3)
            c["y"] = 382.0
        elif c["t"] == "switch":
            c["w"], c["h"] = round(d.x * 100, 2), round(d.y * 100, 2)
            lever = [k for k in bpy.data.objects if k.active_material and k.active_material.name.startswith("chrome")
                     and k.data.name.startswith("Cube") and abs(logical(k)[0] - x) < 1][0]
            c["v"] = round((logical(lever)[1] - (y - c["h"] / 2)) / c["h"], 3)
        elif c["t"] == "button":
            c["w"], c["h"] = round(d.x * 100, 2), round(d.y * 100, 2)
            c["cap"] = colour_name(base_colour(o.active_material))
        elif c["t"] == "led":
            c["r"] = round(d.x * 50, 2)
            c["v"] = 1 if base_colour(o.active_material)[0] > 0.5 else 0
        elif c["t"] == "display":
            c["w"], c["h"] = round(d.x * 100, 2), round(d.y * 100, 2)
        elif c["t"] == "wheel":
            c["w"], c["h"] = 27.0, 150.0
        out.append(c)

    # sliders are matched by cap; claim must cover every scene control of each kind
    for k in ["knob", "slider", "switch", "button", "led", "display", "wheel"]:
        rest = [o.name for o in pool[k] if o.name not in claimed]
        if rest:
            sys.exit(f"unclaimed {k} objects: {rest}")

    whites = sorted(pool["wk"], key=lambda o: logical(o)[0])
    blacks = sorted(pool["bk"], key=lambda o: logical(o)[0])
    out.append(dict(id="keys", t="keys", x=120, y=572, w=1560, h=236, lowest_note=36,
                    white=[logical(o)[0] for o in whites], black=[logical(o)[0] for o in blacks],
                    white_w=40.3, black_w=25.5, black_h=146.3))

    reg = dict(space=[1800, 830], panel=[120, 0, 1680, 572], keybed=[120, 572, 1680, 808], controls=out)
    path = os.path.join(ROOT, "tex2", "J.json")
    with open(path, "w") as f:
        json.dump(reg, f, indent=1)
    counts = {}
    for c in out:
        counts[c["t"]] = counts.get(c["t"], 0) + 1
    print("wrote", path, counts)


main()
