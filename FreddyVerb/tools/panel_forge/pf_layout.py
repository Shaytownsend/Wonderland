"""panel_forge layout: places every manifest control on the 1600x589 (19" x 4U) panel.

The layout is shared by all five skins; per-skin differences (button/toggle/rocker
sizes, knob sizes, LED lens rects, the broadcast VU meters) come from the skin's
`parts` config. Besides the control rects (which go to layout.json) the layout
also returns decoration info used by the panel renderer: printed legends,
section boxes, dial scales.
"""
from __future__ import annotations

import json

W, H = 1600, 589
EAR = 56            # rack ear width (each side)
X0, X1 = 76, 1524   # usable panel face between ears + corner screws

# bottom band row tops (legend line starts at the row top)
TITLE_Y = 352
ROW_MISC1 = 368
ROW_KNOB1 = 414
ROW_MISC2 = 474
ROW_KNOB2 = 520
LEGEND_H = 13       # legend text line + gap above a control


class Layout:
    def __init__(self, manifest: dict, skin: str, parts: dict):
        self.skin = skin
        self.parts = parts
        self.man = {c["id"]: c for c in manifest["controls"]}
        self.controls: dict[str, dict] = {}
        self.labels: list[dict] = []     # printed legends
        self.sections: list[dict] = []   # section frames + titles
        self.notes: dict = {}

    # -- primitives -----------------------------------------------------------
    def add(self, cid, x, y, w, h, art="", led=None, **deco):
        m = self.man[cid]
        c = {"id": cid, "kind": m["kind"], "x": round(x, 1), "y": round(y, 1), "w": round(w, 1), "h": round(h, 1)}
        if art:
            c["art"] = art
        if led is not None:
            c["led"] = [round(v, 2) for v in led]
        c["_deco"] = deco
        self.controls[cid] = c
        return c

    def label(self, text, cx, cy, role="legend", anchor="mm", owner=None):
        self.labels.append({"text": text, "cx": cx, "cy": cy, "role": role, "anchor": anchor, "owner": owner})

    def knob(self, cid, cx, top, size=None, art=None, label=None):
        size = size or self.parts["knob_s"]
        art = art or self.parts["knob_s_art"]
        lab = label if label is not None else self.man[cid]["label"]
        self.label(lab, cx, top + 5, "knob", owner=cid)
        off = 16 if size <= 42 else 17   # leave room for the dial scale under the legend
        return self.add(cid, cx - size / 2, top + off, size, size, art, dial=True)

    def knob_big(self, cid, cx, top):
        return self.knob(cid, cx, top, self.parts["knob_m"], self.parts["knob_m_art"])

    def pad(self, cid, cx, top, legend=None, art=None, kind_art="pad"):
        p = self.parts
        pw, ph = p[kind_art + "_size"]
        art = art or p[kind_art + "_art"]
        led = p.get(kind_art + "_led")
        m = self.man[cid]
        if m["kind"] == "button" and cid != "viz_freeze":
            led = None
        lab = legend if legend is not None else m["label"]
        self.label(lab, cx, top + 5, "pad", owner=cid)
        y = top + LEGEND_H + max(0, (26 - ph) / 2)
        return self.add(cid, cx - pw / 2, y, pw, ph, art, led=led, well=True)

    def button(self, cid, cx, top, legend=None):
        return self.pad(cid, cx, top, legend, kind_art="btn")

    def stepper(self, cid, x, top, w, h=24, legend=None):
        lab = legend if legend is not None else self.man[cid]["label"]
        self.label(lab, x + w / 2, top + 5, "pad", owner=cid)
        return self.add(cid, x, top + LEGEND_H, w, h, bezel="stepper")

    def section(self, title, x0, y0, x1, y1):
        self.sections.append({"title": title, "x0": x0, "y0": y0, "x1": x1, "y1": y1})

    @staticmethod
    def spread(x0, x1, n):
        """Centres of n equal cells between x0 and x1."""
        cw = (x1 - x0) / n
        return [x0 + cw * (i + 0.5) for i in range(n)]

    # -- the panel ------------------------------------------------------------
    def build(self):
        p = self.parts
        self.top_bar()
        self.top_band()
        self.bottom_band()
        missing = [k for k in self.man if k not in self.controls]
        assert not missing, f"unplaced controls: {missing}"
        self.validate()
        return self

    def top_bar(self):
        ty = 9
        # brand block over the IR column (drawn by the panel renderer)
        self.notes["brand"] = (X0 + 4, 8, 352, 52)
        self.button("preset_prev", 388, ty, legend="◄")
        self.add("preset", 410, 14, 300, 34, bezel="display")
        self.button("preset_next", 732, ty, legend="►")
        self.notes["preset_label"] = (560, 6)
        for cid, cx in (("ab_a", 786), ("ab_b", 826), ("ab_copy", 870), ("undo", 930), ("redo", 976)):
            self.button(cid, cx, ty)
        self.button("settings", X1 - 26, ty)
        # model plate area between REDO and SETUP
        self.notes["plate"] = (1010, 10, 1460, 50)

    def top_band(self):
        p = self.parts
        # IR column
        self.add("ir_browser", 82, 64, 264, 152, bezel="display")
        self.add("ir_display", 82, 232, 264, 102, bezel="display")
        # centre: envelope + readout row
        self.add("envelope", 372, 64, 568, 210, bezel="display", main=True)
        self.add("readout", 372, 292, 340, 38, bezel="display")
        self.button("viz_scale", 754, 291)
        self.button("viz_freeze", 818, 291)
        js = p["jog_size"]
        self.label("DATA", 868, 311, "pad", owner="jog")
        self.add("jog", 935 - js, 311 - js / 2, js, js, p["jog_art"], dial=False, jog=True)

        # TONE
        tx0, tx1 = 960, 1330
        self.section("TONE", tx0, 56, tx1, 338)
        self.add("eq_display", 966, 78, 250, 96, bezel="display")
        self.stepper("eq_hpf_slope@stepper", 1230, 74, 96)
        self.stepper("eq_lpf_slope@stepper", 1230, 124, 96)
        ids = ["eq_on", "eq_position#0", "eq_position#1", "eq_hpf_on", "eq_ls_on", "eq_b1_on", "eq_b2_on", "eq_hs_on", "eq_lpf_on"]
        # two groups: EQ PRE POST | HPF LS B1 B2 HS LPF
        xs = self.spread(tx0 + 2, tx0 + 128, 3) + self.spread(tx0 + 136, tx1 - 2, 6)
        for cid, cx in zip(ids, xs):
            self.pad(cid, cx, 186)
        for cid, cx in zip(["eq_hpf_freq", "eq_lpf_freq", "eqsel_freq", "eqsel_gain", "eqsel_q"],
                           self.spread(tx0 + 4, tx1 - 4, 5)):
            self.knob_big(cid, cx, 236)
        self.notes["band_bracket"] = (self.spread(tx0 + 4, tx1 - 4, 5)[2], self.spread(tx0 + 4, tx1 - 4, 5)[4])

        # MIX
        mx0, mx1 = 1340, X1
        self.section("MIX", mx0, 56, mx1, 338)
        if p.get("vu"):
            self.add("meter_in", mx0 + 4, 76, 84, 58, bezel="vu")
            self.add("meter_wet", mx1 - 88, 76, 84, 58, bezel="vu")
            # big knobs 2x2, OUT bar meter in the right column, keys underneath
            for cid, cx, top in (("mix_pct", mx0 + 32, 151), ("dry_level_db", mx0 + 94, 151),
                                 ("wet_level_db", mx0 + 32, 222), ("output_gain_db", mx0 + 94, 222)):
                self.knob(cid, cx, top)
            for cid, cx in zip(["mix_lock", "wet_solo", "bypass"], self.spread(mx0, mx1, 3)):
                self.pad(cid, cx, 292)
            self.add("meter_out", mx1 - 32, 162, 14, 108, bezel="meter")
            self.label("OUT", mx1 - 25, 280, "pad", owner="meter_out")
        else:
            cxs = [mx0 + 29, mx0 + 87]
            self.knob_big("mix_pct", cxs[0], 74)
            self.knob_big("dry_level_db", cxs[1], 74)
            self.knob_big("wet_level_db", cxs[0], 144)
            self.knob_big("output_gain_db", cxs[1], 144)
            for i, cid in enumerate(["meter_in", "meter_wet", "meter_out"]):
                x = mx1 - 66 + i * 23
                self.add(cid, x, 80, 14, 122, bezel="meter")
                self.label(self.man[cid]["label"], x + 7, 214, "pad", owner=cid)
            for cid, cx in zip(["mix_lock", "wet_solo", "bypass"], self.spread(mx0, mx1, 3)):
                self.pad(cid, cx, 228)

    def bottom_band(self):
        RK2 = self.parts.get("row_knob2", ROW_KNOB2)
        ks = self.parts["knob_s"]
        secs = {"REVERB": (X0, 476), "DETECT": (490, 640), "DUCK": (654, 1084), "GATE": (1098, 1358), "SPACE": (1372, X1)}
        for t, (a, b) in secs.items():
            self.section(t, a, 346, b, 583)

        # REVERB
        a, b = secs["REVERB"]
        self.stepper("quality@stepper", a + 6, ROW_MISC1, 118)
        self.pad("ir_reverse", a + 160, ROW_MISC1)
        self.pad("ir_normalize", a + 206, ROW_MISC1)
        self.pad("predelay_sync", a + 254, ROW_MISC1)
        self.stepper("predelay_note@stepper", b - 92, ROW_MISC1, 86)
        r1 = ["predelay_ms", "ir_start_ms", "ir_length_pct", "ir_size_pct", "ir_decay_pct"]
        r2 = ["ir_xover_ms", "ir_early_db", "ir_late_db", "ir_damping_pct", "ir_damp_freq"]
        for cid, cx in zip(r1, self.spread(a, b, 5)):
            self.knob(cid, cx, ROW_KNOB1)
        for cid, cx in zip(r2, self.spread(a, b, 5)):
            self.knob(cid, cx, RK2)

        # DETECT
        a, b = secs["DETECT"]
        self.stepper("det_mode@stepper", a + 6, ROW_MISC1, 74)
        self.pad("key_listen", b - 34, ROW_MISC1)
        for cid, cx in zip(["key_hpf", "key_lpf"], self.spread(a, b, 2)):
            self.knob(cid, cx, ROW_KNOB1)
        for cid, cx in zip(["det_rms_ms", "lookahead_ms"], self.spread(a, b, 2)):
            self.knob(cid, cx, RK2)

        # DUCK
        a, b = secs["DUCK"]
        self.pad("duck_on", a + 30, ROW_MISC1)
        self.pad("duck_mode#0", a + 86, ROW_MISC1)
        self.pad("duck_mode#1", a + 140, ROW_MISC1)
        self.notes["follow_curve"] = (a + 86, a + 140, ROW_MISC1)
        self.stepper("curve_retrigger@stepper", a + 186, ROW_MISC1, 96)
        kx = self.spread(a, a + 300, 5)
        for cid, cx in zip(["duck_depth_db", "duck_threshold_db", "duck_attack_ms", "duck_release_ms", "duck_amount"], kx):
            self.knob(cid, cx, ROW_KNOB1)
        self.stepper("duck_rel_curve@stepper", b - 124, ROW_KNOB1 + 8, 118)
        cx7 = self.spread(a, a + 330, 7)
        for i, cx in enumerate(cx7):
            self.pad(f"curve_shape#{i}", cx, ROW_MISC2)
        self.notes["curve_glyphs"] = cx7
        self.pad("curve_sync", a + 368, ROW_MISC2)
        for cid, cx in zip(["curve_duration_ms", "curve_tension", "curve_guard_ms", "curve_hyst_db"], kx[:4]):
            self.knob(cid, cx, RK2)
        self.stepper("curve_note@stepper", b - 124, RK2 + 8, 86)
        # FOLLOW / CURVE groups (printed brackets)
        self.notes["duck_groups"] = [("FOLLOW", a + 4, a + 300, ROW_KNOB1), ("CURVE", a + 4, b - 4, ROW_MISC2)]

        # GATE
        a, b = secs["GATE"]
        self.pad("gate_on", a + 32, ROW_MISC1)
        self.stepper("gate_rel_curve@stepper", b - 128, ROW_MISC1, 120)
        for cid, cx in zip(["gate_threshold_db", "gate_hyst_db", "gate_attack_ms"], self.spread(a, b, 3)):
            self.knob(cid, cx, ROW_KNOB1)
        self.pad("gate_hold_sync", a + 52, ROW_MISC2)
        self.stepper("gate_hold_note@stepper", b - 128, ROW_MISC2, 86)
        for cid, cx in zip(["gate_hold_ms", "gate_release_ms", "gate_floor_db"], self.spread(a, b, 3)):
            self.knob(cid, cx, RK2)

        # SPACE
        a, b = secs["SPACE"]
        self.label("CORR", a + 18, ROW_MISC1 + 5, "pad", owner="correlation")
        self.add("correlation", a + 10, ROW_MISC1 + 18, b - a - 20, 12, bezel="corr")
        for cid, cx in zip(["width_pct", "wet_balance"], self.spread(a, b, 2)):
            self.knob(cid, cx, ROW_KNOB1)
        c = self.spread(a, b, 2)
        self.knob("mono_below_hz", c[0], RK2)
        self.pad("mono_below_on", c[1], RK2 + 6)

    # -- checks -----------------------------------------------------------------
    def validate(self):
        cs = list(self.controls.values())
        for c in cs:
            assert c["x"] >= EAR + 4 and c["x"] + c["w"] <= W - EAR - 4, f"{c['id']} outside ears"
            assert c["y"] >= 4 and c["y"] + c["h"] <= H - 4, f"{c['id']} outside panel"
            if c["kind"] == "knob":
                assert c["w"] >= 34, c["id"]
            if c["kind"] in ("pad", "choicepad", "button"):
                assert c["h"] >= 22, c["id"]
        for i in range(len(cs)):
            for j in range(i + 1, len(cs)):
                a, b = cs[i], cs[j]
                if a["x"] < b["x"] + b["w"] and b["x"] < a["x"] + a["w"] and a["y"] < b["y"] + b["h"] and b["y"] < a["y"] + a["h"]:
                    raise AssertionError(f"overlap: {a['id']} / {b['id']}")
        # shared arts must share a size
        sizes = {}
        for c in cs:
            if c.get("art"):
                s = (c["w"], c["h"])
                assert sizes.setdefault(c["art"], s) == s, f"art {c['art']} used with different sizes"

    def to_json(self):
        out = []
        for c in self.controls.values():
            d = {k: v for k, v in c.items() if not k.startswith("_")}
            out.append(d)
        return {"skin": self.skin, "width": W, "height": H, "knob_frames": 128, "controls": out}

    def write(self, path):
        with open(path, "w") as f:
            json.dump(self.to_json(), f, indent=1)
