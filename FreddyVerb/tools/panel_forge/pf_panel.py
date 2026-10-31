"""panel_forge: renders the static front panel (and the glass/dust overlay) for one skin."""
from __future__ import annotations

import math

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

import pf_core as C
from pf_layout import EAR, W as LW, H as LH, X0, X1
from pf_skins import USAGE

EAR_HOLES_Y = [21, 126, 463, 568]


class PanelRenderer:
    def __init__(self, name: str, sk: dict, layout, S: int = 3):
        self.name, self.sk, self.L, self.S = name, sk, layout, S
        self.W, self.H = LW * S, LH * S
        self.c = C.Canvas(self.W, self.H, S)
        self.seed = sk["seed"]
        self.r = C.rng(self.seed)
        self.post_mul = np.zeros((self.H, self.W), np.float32)   # contact/drop shadows of parts
        self.text_guard = Image.new("L", (self.W, self.H), 0)    # where essential text lives
        self.aniso_noise = None

    # ---------------------------------------------------------------- helpers
    def P(self, v):
        return v * self.S

    def rect_px(self, c):
        S = self.S
        return c["x"] * S, c["y"] * S, c["w"] * S, c["h"] * S

    def win(self, x0, y0, x1, y1, pad=4):
        return self.c.win(x0, y0, x1, y1, pad * self.S)

    def usage_map(self):
        """Smooth map in [0,1] of how much hand time each panel area gets."""
        sc = 8
        h, w = LH // sc + 2, LW // sc + 2
        u = np.zeros((h, w), np.float32)
        for cid, wgt in USAGE.items():
            c = self.L.controls.get(cid)
            if not c:
                continue
            cx, cy = (c["x"] + c["w"] / 2) / sc, (c["y"] + c["h"] / 2) / sc
            u[int(cy), int(cx)] += wgt
        u = ndimage.gaussian_filter(u, 34 / sc) * (2 * math.pi * (34 / sc) ** 2) * 0.55
        u = np.clip(u, 0, 1)
        return np.clip(ndimage.zoom(u, (self.H / h, self.W / w), order=1)[: self.H, : self.W], 0, 1)

    # ---------------------------------------------------------------- material
    def base_material(self):
        c, P, S = self.c, self.sk["panel"], self.S
        base = np.asarray(P["color"], np.float32)
        mat = P["material"]
        c.spec[:] = P["spec"]
        c.gloss[:] = P["gloss"]
        if mat == "brushed":
            n = C.brushed(self.H, self.W, S, self.seed + 1)
            self.aniso_noise = n
            tone = 1 + P.get("streak", 0.07) * n
            mott = C.fbm(self.H, self.W, 300 * S, self.seed + 2, 2)
            tone *= 1 + 0.035 * (mott - 0.5)
            c.A[:] = base[None, None, :] * tone[..., None]
            c.aniso[:] = P["aniso"]
            c.H += 0.015 * n
        elif mat == "paint":
            peel = C.fbm(self.H, self.W, 3.0 * S, self.seed + 3, 3)
            c.H += 0.05 * (peel - 0.5)
            mott = C.fbm(self.H, self.W, 160 * S, self.seed + 4, 3)
            c.A[:] = base[None, None, :] * (1 + 0.035 * (mott - 0.5))[..., None]
            z = P.get("zone2")
            if z:
                x0, y0, x1, y1 = [v * S for v in z["rect"]]
                sl = (slice(int(y0), int(y1)), slice(int(x0), int(x1)))
                col = np.asarray(z["color"], np.float32)
                c.A[sl] = col[None, None, :] * (1 + 0.035 * (mott[sl] - 0.5))[..., None]
                # the blue-grey sub-panel is a separate, slightly proud plate
                X, Y = C.grid(int(y1 - y0), int(x1 - x0), x0, y0)
                d = C.sdf_rrect(X, Y, x0, y0, x1 - x0, y1 - y0 + 40 * S, 3 * S)
                c.H[sl] += 0.5 * C.smoothstep(0, 1.6 * S, -d)
        elif mat == "hammertone":
            a = C.fbm(self.H, self.W, 9.0 * S, self.seed + 5, 2, 0.45)
            b = C.fbm(self.H, self.W, 3.0 * S, self.seed + 6, 2, 0.5)
            ham = np.clip(np.abs(a - 0.5) * 2.6, 0, 1)
            c.H += 0.8 * (1 - ham) ** 1.5 + 0.15 * b
            flake = C.white(self.H, self.W, self.seed + 7)
            c.A[:] = base[None, None, :] * (0.84 + 0.24 * (1 - ham) + 0.06 * (flake - 0.5))[..., None]
            c.spec[:] = P["spec"] * (0.6 + 0.8 * (flake > 0.92))
        # panel outline chamfer + ears
        self.edges_and_ears()

    def edges_and_ears(self):
        c, S = self.c, self.S
        X, Y = C.grid(self.H, self.W)
        d_edge = np.minimum(np.minimum(X, self.W - X), np.minimum(Y, self.H - Y))
        c.H -= 1.2 * (1 - C.smoothstep(0, 1.8 * S, d_edge))
        # ears: slightly set back + a crease where the ear was bent over time
        e = EAR * S
        for side in (0, 1):
            xe = X if side == 0 else self.W - X
            ear = 1 - C.smoothstep(e - 3 * S, e + 1 * S, xe)
            tilt = (e - xe) / (e) * (0.9 if side == 0 else 0.5)
            c.H -= ear * (0.55 + tilt)
        self._d_edge = d_edge

    # ---------------------------------------------------------------- hardware
    def rack_screws(self):
        c, S, sk = self.c, self.S, self.sk
        r = C.rng(self.seed + 11)
        # which holes hold a screw; two stay empty
        empty = {(0, 1), (1, 2)}
        kinds = ["phillips_nylon", "hex_metal", "phillips_metal", "hex_nylon_black"]
        for side in (0, 1):
            cx = (EAR / 2) * S if side == 0 else self.W - (EAR / 2) * S
            for i, y in enumerate(EAR_HOLES_Y):
                cy = y * S
                w = self.win(cx - 20 * S, cy - 17 * S, cx + 20 * S, cy + 17 * S)
                sl, X, Y = w
                # slotted oval hole
                d = C.sdf_stadium(X, Y, cx, cy, 4.6 * S, 7.0 * S)
                a = C.cov(d, 0.8)
                c.H[sl] = c.H[sl] * (1 - a) + (-6.0) * a
                c.paint(sl, a, (0.10, 0.10, 0.105), spec=0.15, gloss=10, aniso=0.0)
                # rack rail visible through the slot (lit metal behind)
                rail = C.cov(C.sdf_stadium(X, Y, cx, cy, 3.6 * S, 2.6 * S)) * 0.0
                if (side, i) in empty:
                    # cage-nut square hole in the rail
                    dn = C.sdf_rrect(X, Y, cx - 4.6 * S, cy - 4.6 * S, 9.2 * S, 9.2 * S, 0.8 * S)
                    an = C.cov(dn)
                    c.H[sl] = c.H[sl] * (1 - an) + (-9) * an
                    c.paint(sl, an, (0.015, 0.015, 0.015), spec=0.0)
                    continue
                kind = kinds[(i + side * 2) % len(kinds)]
                sx = cx + r.uniform(-3.0, 3.0) * S
                sy = cy + r.uniform(-0.6, 0.6) * S
                rot = r.uniform(0, math.pi)
                self.screw(sx, sy, kind, rot, big=True)

    def screw(self, sx, sy, kind, rot, big=False, rust=0.0):
        c, S = self.c, self.S
        R = (9.0 if big else 4.2) * S
        washer = (12.5 if big else 0) * S
        w = self.win(sx - 16 * S, sy - 16 * S, sx + 16 * S, sy + 16 * S)
        sl, X, Y = w
        rr = np.hypot(X - sx, Y - sy)
        if washer:
            dw = rr - washer
            aw = C.cov(dw)
            nylon = "nylon" in kind
            wcol = (0.86, 0.84, 0.76) if (nylon and "black" not in kind) else ((0.05, 0.05, 0.05) if nylon else (0.62, 0.62, 0.63))
            hw = 0.9 * C.smoothstep(0, 1.0 * S, -dw)
            c.H[sl] = np.maximum(c.H[sl] * (1 - aw) + hw * aw, c.H[sl] * (1 - aw))
            c.paint(sl, aw, wcol, spec=0.25 if nylon else 0.6, gloss=20 if nylon else 40, aniso=0.0)
            self.post_mul[sl] = np.maximum(self.post_mul[sl], 0.0)
        # head
        if "hex" in kind:
            ang = np.arctan2(Y - sy, X - sx) - rot
            hexr = R * 0.92 / np.cos(((ang + math.pi / 6) % (math.pi / 3)) - math.pi / 6)
            dh = rr - hexr
            ah = C.cov(dh)
            hh = 1.4 + 2.6 * C.smoothstep(0, 1.6 * S, -dh)
            flange = C.cov(rr - R * 1.12)
            c.H[sl] = c.H[sl] * (1 - flange) + (1.1 + c.H[sl] * 0) * flange
            c.H[sl] = c.H[sl] * (1 - ah) + hh * ah
            col = (0.56, 0.57, 0.58)
            c.paint(sl, np.maximum(ah, flange), col, spec=0.7, gloss=35, aniso=0.0)
        else:
            dh = rr - R
            ah = C.cov(dh)
            dome = 1.0 + (3.2 if big else 2.0) * np.sqrt(np.clip(1 - (rr / R) ** 2, 0, 1))
            c.H[sl] = c.H[sl] * (1 - ah) + dome * ah
            col = (0.60, 0.60, 0.61) if "metal" in kind or big else (0.62, 0.62, 0.62)
            c.paint(sl, ah, col, spec=0.75, gloss=45, aniso=0.0)
            # phillips cross recess
            for k in (0, 1):
                a = rot + k * math.pi / 2
                u = (X - sx) * math.cos(a) + (Y - sy) * math.sin(a)
                v = -(X - sx) * math.sin(a) + (Y - sy) * math.cos(a)
                dcr = np.maximum(np.abs(u) - R * 0.55, np.abs(v) - R * 0.12)
                acr = C.cov(dcr) * ah
                c.H[sl] -= acr * (1.6 * (1 - np.clip(np.abs(u) / (R * 0.55), 0, 1)) + 0.3)
                c.paint(sl, acr * 0.6, (0.2, 0.2, 0.2))
        if rust > 0:
            rn = C.fbm(X.shape[0], X.shape[1], 2.2 * S, int(sx + sy) % 9999, 3)
            ra = np.clip((rn - (0.62 - 0.25 * rust)) * 4, 0, 1) * C.cov(rr - R * 1.05)
            c.paint(sl, ra * 0.9, (0.42, 0.20, 0.08), spec=0.03, gloss=6)
            c.H[sl] += ra * 0.15
        # contact shadow
        sh = C.cov(np.hypot(X - sx - 0.9 * S, Y - sy - 1.3 * S) - (max(R, washer) + 0.6 * S), 2.0 * S)
        self.post_mul[sl] = np.maximum(self.post_mul[sl], sh * 0.35)

    def corner_screws(self):
        c, S = self.c, self.S
        r = C.rng(self.seed + 13)
        rust = 0.8 if self.sk["wear"].get("rust") else 0.0
        for (x, y) in ((66, 13), (66, 576), (LW - 66, 13), (LW - 66, 576)):
            sx, sy = x * S, y * S
            # chipped paint around the screw head (bare metal)
            w = self.win(sx - 10 * S, sy - 10 * S, sx + 10 * S, sy + 10 * S)
            sl, X, Y = w
            rr = np.hypot(X - sx, Y - sy)
            n = C.fbm(X.shape[0], X.shape[1], 1.8 * S, int(x * 7 + y), 3)
            chip = (rr < (5.0 + 2.8 * n) * S).astype(np.float32) * (n > 0.42)
            chip = ndimage.gaussian_filter(chip, 0.4 * S)
            c.paint(sl, chip * 0.9 * self.sk["wear"]["edge"], self.sk["panel"]["bare"], spec=0.5, gloss=30, aniso=0.0)
            # countersink
            cs = C.cov(rr - 5.0 * S)
            c.H[sl] -= cs * 0.6 * (1 - rr / (5.0 * S)).clip(0, 1)
            self.screw(sx, sy, "phillips_metal", r.uniform(0, math.pi), big=False, rust=rust)

    # ---------------------------------------------------------------- bezels / wells
    def bezel(self, rect, bw, radius, hf, depth, color, spec, gloss, window, crinkle=False, lip=1.6, screws=False):
        c, S = self.c, self.S
        x, y, w, h = [v * S for v in rect]
        bw, radius = bw * S, radius * S
        win = self.win(x - bw, y - bw, x + w + bw, y + h + bw)
        sl, X, Y = win
        outer = C.sdf_rrect(X, Y, x - bw, y - bw, w + 2 * bw, h + 2 * bw, radius + bw * 0.6)
        inner = C.sdf_rrect(X, Y, x, y, w, h, max(0.5 * S, radius * 0.4))
        a_out = C.cov(outer)
        prof = C.smoothstep(0, 2.4 * S, -outer)
        Hf = hf * prof
        if crinkle:
            cr = C.fbm(X.shape[0], X.shape[1], 1.2 * S, int(x + y) % 9999, 2)
            Hf = Hf + 0.12 * cr
        t = C.smoothstep(0, lip * S, inner)
        Hn = np.where(inner < 0, -depth, -depth + (Hf + depth) * t)
        c.H[sl] = c.H[sl] * (1 - a_out) + Hn * a_out
        fr = a_out * np.clip(inner / (0.6 * S), 0, 1)
        col = np.asarray(color, np.float32)
        if crinkle:
            col = col[None, None, :] * (0.85 + 0.3 * cr)[..., None]
        c.paint(sl, fr, col, spec=spec, gloss=gloss, aniso=0.0)
        c.paint(sl, C.cov(inner), window, spec=0.05, gloss=8, aniso=0.0)
        # contact shadow of the proud frame
        sh = C.cov(C.sdf_rrect(X, Y, x - bw + 1.0 * S, y - bw + 1.5 * S, w + 2 * bw, h + 2 * bw, radius + bw * 0.6), 1.8 * S)
        self.post_mul[sl] = np.maximum(self.post_mul[sl], sh * (1 - a_out) * 0.28 * min(1, hf))
        if screws:
            for (px, py) in ((x - bw / 2, y - bw / 2), (x + w + bw / 2, y - bw / 2), (x - bw / 2, y + h + bw / 2), (x + w + bw / 2, y + h + bw / 2)):
                self.screw(px, py, "phillips_metal", (px * 0.37 + py) % 3.1, big=False,
                           rust=0.8 if self.sk["wear"].get("rust") else 0.0)

    def well(self, rect, gap=1.6, radius=3.0, depth=3.0):
        c, S = self.c, self.S
        x, y, w, h = [v * S for v in rect]
        g = gap * S
        sl, X, Y = self.win(x - g, y - g, x + w + g, y + h + g)
        d = C.sdf_rrect(X, Y, x - g, y - g, w + 2 * g, h + 2 * g, (radius + gap) * S)
        a = C.cov(d)
        c.H[sl] = c.H[sl] * (1 - a) + (-depth) * a
        c.paint(sl, a, (0.015, 0.015, 0.015), spec=0.0, gloss=4, aniso=0.0)
        # tiny chamfer highlight ring
        ring = C.cov(np.abs(d + 0.4 * S) - 0.5 * S) * 0.35
        c.paint(sl, ring, self.sk["panel"]["bare"], spec=0.6, gloss=40)

    def knob_shadow(self, cx, cy, rad, soft=2.4, off=(1.6, 2.4), strength=0.55):
        S = self.S
        cx, cy = cx * S, cy * S
        sl, X, Y = self.win(cx - rad * S - 12 * S, cy - rad * S - 12 * S, cx + rad * S + 12 * S, cy + rad * S + 12 * S)
        d = np.hypot(X - cx - off[0] * S, Y - cy - off[1] * S) - rad * S
        sh = C.cov(d, soft * S)
        contact = C.cov(np.hypot(X - cx - 0.4 * S, Y - cy - 0.7 * S) - (rad + 0.4) * S, 0.7 * S)
        self.post_mul[sl] = np.maximum(self.post_mul[sl], np.maximum(sh * strength, contact * 0.7))

    # ---------------------------------------------------------------- printing
    def ink_layers(self):
        """Returns L-mode masks: (ink, ink2, engrave, red) at panel scale."""
        size = (self.W, self.H)
        return {k: Image.new("L", size, 0) for k in ("ink", "ink2", "red", "engr")}

    def ink_key_for(self, y):
        z = self.sk["panel"].get("zone2")
        if z and y >= z["rect"][1]:
            return "ink2"
        return "ink"

    def text(self, layers, s, cx, cy, px, kind="semibold", anchor="mm", tracking=0.0, key=None, guard=True, rotate=0.0):
        S = self.S
        key = key or self.ink_key_for(cy)
        C.draw_text_mask(layers[key], s, cx * S, cy * S, kind, px * S, anchor, tracking * S, rotate=rotate)
        if guard:
            C.draw_text_mask(self.text_guard, s, cx * S, cy * S, kind, px * S, anchor, tracking * S, rotate=rotate)

    def line(self, layers, pts, width, key=None):
        S = self.S
        key = key or self.ink_key_for(pts[0][1])
        ImageDraw.Draw(layers[key]).line([(x * S, y * S) for x, y in pts], fill=255, width=max(1, int(round(width * S))))

    def dial(self, layers, c, style):
        S = self.S
        cx, cy = c["x"] + c["w"] / 2, c["y"] + c["h"] / 2
        art_r = 0.47 * (c["w"] - 2)
        key = self.ink_key_for(cy)
        kind = style["kind"]
        d = ImageDraw.Draw(layers["engr" if kind == "engraved" else key])
        n = 11
        for i in range(n):
            a = math.radians(-135 + 270 * i / (n - 1))
            sa, ca = math.sin(a), -math.cos(a)
            end = i in (0, n - 1)
            if kind == "dots":
                rr = art_r + 3.6
                rad = (1.0 if end else 0.7) * S
                px, py = (cx + sa * rr) * S, (cy + ca * rr) * S
                d.ellipse([px - rad, py - rad, px + rad, py + rad], fill=255)
            else:
                r0 = art_r + 1.6
                r1 = art_r + (4.6 if end else 3.6)
                d.line([((cx + sa * r0) * S, (cy + ca * r0) * S), ((cx + sa * r1) * S, (cy + ca * r1) * S)],
                       fill=255, width=max(1, int(round((1.1 if end else 0.8) * S))))
        if kind == "numbers" and c["w"] < 44:
            pass

    # ---------------------------------------------------------------- the main pass
    def render(self):
        sk, S, c = self.sk, self.S, self.c
        self.base_material()
        self.rack_screws()
        self.corner_screws()
        layers = self.ink_layers()
        bz = sk["bezel"]

        # displays, steppers, meters
        for ctl in self.L.controls.values():
            deco = ctl["_deco"]
            rect = (ctl["x"], ctl["y"], ctl["w"], ctl["h"])
            b = deco.get("bezel")
            if b == "display":
                self.bezel(rect, bz["width"], bz["radius"], 1.4, 2.5, bz["color"], bz["spec"], bz["gloss"], sk["window"],
                           crinkle=bz.get("crinkle", False), screws=bz.get("screws", False) and ctl["w"] > 200)
            elif b == "stepper":
                self.bezel(rect, 3.0, 2.0, 0.9, 2.0, bz["color"], bz["spec"], bz["gloss"], sk["window"], lip=1.0)
            elif b == "meter":
                self.bezel(rect, 3.0, 1.5, 0.9, 2.0, bz["color"], bz["spec"], bz["gloss"], (0.02, 0.02, 0.02), lip=1.0)
            elif b == "corr":
                self.bezel(rect, 2.5, 1.5, 0.8, 1.5, bz["color"], bz["spec"], bz["gloss"], (0.02, 0.02, 0.02), lip=0.8)
            elif b == "vu":
                self.vu_meter(layers, ctl)
            if deco.get("well"):
                kind = sk["parts"]["pad_art"] if ctl["kind"] != "button" else sk["parts"]["btn_art"]
                if kind in ("key", "lamp"):
                    self.well(rect, gap=1.4, radius=2.5 if kind == "key" else 2.0)
                elif kind == "toggle":
                    cx, cy = ctl["x"] + ctl["w"] / 2, ctl["y"] + ctl["h"] / 2
                    self.knob_shadow(cx, cy + 2, 7.5, soft=1.6, off=(1.2, 1.8), strength=0.45)
                elif kind == "rocker":
                    x, y, w, h = rect
                    self.well(rect, gap=0.6, radius=1.5, depth=2.0)
            if deco.get("dial"):
                cx, cy = ctl["x"] + ctl["w"] / 2, ctl["y"] + ctl["h"] / 2
                rad = 0.47 * (ctl["w"] - 2)
                self.knob_shadow(cx, cy, rad)
                self.dial(layers, ctl, sk["dial"])
                if sk["knob"]["kind"] == "chicken":
                    self.knob_nut(cx, cy, rad)
            if deco.get("jog"):
                cx, cy = ctl["x"] + ctl["w"] / 2, ctl["y"] + ctl["h"] / 2
                rad = 0.48 * (ctl["w"] - 4)
                self.knob_shadow(cx, cy, rad, strength=0.6)
                # recessed collar around the data wheel
                sl, X, Y = self.win((cx - rad - 5) * S, (cy - rad - 5) * S, (cx + rad + 5) * S, (cy + rad + 5) * S)
                dd = np.hypot(X - cx * S, Y - cy * S) - (rad + 3.2) * S
                a = C.cov(dd)
                c.H[sl] = c.H[sl] * (1 - a) + (-1.4) * a
                c.paint(sl, a, (0.03, 0.03, 0.03), spec=0.2, gloss=20, aniso=0.0)

        self.section_titles(layers)
        self.legends(layers)
        self.branding(layers)
        self.extras_pre(layers)

        # ---- wear (before ink so worn ink can be partially restored as legible)
        U = self.usage_map()
        guard = ndimage.maximum_filter(np.asarray(self.text_guard, np.float32) / 255.0, size=int(2 * S) + 1)
        self.wear(U, guard)
        self.apply_ink(layers, U)
        self.extras_post(layers, U)
        self.dust(U)
        out = C.shade(c, aniso_noise=self.aniso_noise)
        pm = ndimage.gaussian_filter(self.post_mul, 0.6 * S)
        out *= (1 - pm)[..., None]
        out = self.photo_finish(out)
        return out

    def knob_nut(self, cx, cy, rad):
        """Hex nut + washer under chicken-head knobs (lab)."""
        c, S = self.c, self.S
        sx, sy = cx * S, cy * S
        sl, X, Y = self.win(sx - (rad + 4) * S, sy - (rad + 4) * S, sx + (rad + 4) * S, sy + (rad + 4) * S)
        rr = np.hypot(X - sx, Y - sy)
        dw = rr - (rad + 1.5) * S
        aw = C.cov(dw)
        c.H[sl] = c.H[sl] * (1 - aw) + 0.6 * aw
        c.paint(sl, aw, (0.55, 0.55, 0.56), spec=0.6, gloss=30, aniso=0.0)

    # ---------------------------------------------------------------- text content
    def section_titles(self, layers):
        sk, S = self.sk, self.S
        style = sk["ink"]["title"]
        for sec in self.L.sections:
            t, x0, y0, x1, y1 = sec["title"], sec["x0"], sec["y0"], sec["x1"], sec["y1"]
            ty = y0 + 8
            if style == "rule":
                self.text(layers, t, x0 + 2, ty, 15, anchor="lm", tracking=2.2)
                tw, _ = C.text_size(t, "semibold", 15 * S, 2.2 * S)
                self.line(layers, [(x0 + 2 + tw / S + 8, ty), (x1 - 2, ty)], 1.0)
                self.line(layers, [(x1 - 2, ty), (x1 - 2, ty + 6)], 1.0)
            elif style == "box":
                key = self.ink_key_for(ty)
                tw, _ = C.text_size(t, "semibold", 14 * S, 2.0 * S)
                bx0, bx1 = x0 + 2, x0 + 2 + tw / S + 14
                d = ImageDraw.Draw(layers[key])
                d.rounded_rectangle([bx0 * S, (ty - 7) * S, bx1 * S, (ty + 7) * S], radius=2 * S, fill=255)
                # knocked-out title text
                C.draw_text_mask(layers[key], t, (bx0 + 7) * S, ty * S, "semibold", 14 * S, "lm", 2.0 * S, fill=0)
                C.draw_text_mask(self.text_guard, t, (bx0 + 7) * S, ty * S, "semibold", 14 * S, "lm", 2.0 * S)
                d.line([(bx1 * S, ty * S), ((x1 - 2) * S, ty * S)], fill=255, width=int(1.0 * S))
            elif style == "engraved_box":
                self.text(layers, t, x0 + 6, ty, 15, anchor="lm", tracking=2.6, key="engr")
                tw, _ = C.text_size(t, "semibold", 15 * S, 2.6 * S)
                d = ImageDraw.Draw(layers["engr"])
                lw = int(round(1.0 * S))
                pts = [(x0 + 12 + tw / S, ty), (x1 - 1, ty), (x1 - 1, y1 - 1), (x0 + 1, y1 - 1), (x0 + 1, ty), (x0 + 3, ty)]
                d.line([(px * S, py * S) for px, py in pts], fill=255, width=lw, joint="curve")
            elif style == "dymo":
                self.dymo(x0 + 2, ty - 7, t, color=(0.06, 0.06, 0.07), rot=self.r.uniform(-1.2, 1.2))

    def legends(self, layers):
        for lb in self.L.labels:
            t = lb["text"]
            px = 12.5 if not lb["owner"] or not lb["owner"].startswith("meter") else 10.5
            if t in ("\u25c4", "\u25ba"):
                key = "engr" if self.sk["ink"]["style"] == "engraved" else self.ink_key_for(lb["cy"])
                S = self.S
                sgn = -1 if t == "\u25c4" else 1
                cx, cy = lb["cx"], lb["cy"]
                pts = [((cx + sgn * 4.5) * S, cy * S), ((cx - sgn * 3.5) * S, (cy - 4.5) * S), ((cx - sgn * 3.5) * S, (cy + 4.5) * S)]
                ImageDraw.Draw(layers[key]).polygon(pts, fill=255)
                ImageDraw.Draw(self.text_guard).polygon(pts, fill=255)
                continue
            if self.sk["ink"]["style"] == "engraved":
                self.text(layers, t, lb["cx"], lb["cy"], px, anchor=lb["anchor"], tracking=0.6, key="engr")
            else:
                self.text(layers, t, lb["cx"], lb["cy"], px, anchor=lb["anchor"], tracking=0.6)
        # meter scale marks for LED meters (dB) + correlation scale
        for cid in ("meter_in", "meter_wet", "meter_out"):
            ctl = self.L.controls[cid]
            if ctl["_deco"].get("bezel") != "meter":
                continue
            x, y, w, h = ctl["x"], ctl["y"], ctl["w"], ctl["h"]
            for frac in (0.0, 0.25, 0.5, 0.75, 1.0):
                yy = y + h - frac * h
                self.line(layers, [(x - 4.5, yy), (x - 3.2, yy)], 0.8, key="engr" if self.sk["ink"]["style"] == "engraved" else None)
        corr = self.L.controls["correlation"]
        x, y, w, h = corr["x"], corr["y"], corr["w"], corr["h"]
        for frac, t in ((0.0, "-1"), (0.5, "0"), (1.0, "+1")):
            self.text(layers, t, x + frac * w, y + h + 9, 10, key="engr" if self.sk["ink"]["style"] == "engraved" else None)
        # duck sub-groups
        a, b, yy = self.L.notes["follow_curve"]
        # curve shape glyph strokes above the 7 shape keys are given as legends already
        # TONE: bracket over the selected-band knobs
        x0b, x1b = self.L.notes["band_bracket"]
        yb = 314
        key = "engr" if self.sk["ink"]["style"] == "engraved" else None
        self.line(layers, [(x0b - 22, yb - 5), (x0b - 22, yb), (x1b + 22, yb), (x1b + 22, yb - 5)], 0.9, key=key)
        self.text(layers, "SELECTED BAND", (x0b + x1b) / 2, yb + 9, 10.5, tracking=1.2, key=key)
        self.line(layers, [(1000 - 22, yb - 5), (1000 - 22, yb), (1073 + 22, yb), (1073 + 22, yb - 5)], 0.9, key=key)
        self.text(layers, "FILTERS", (1000 + 1073) / 2, yb + 9, 10.5, tracking=1.2, key=key)

    def branding(self, layers):
        sk, S = self.sk, self.S
        bx0, by0, bx1, by1 = self.L.notes["brand"]
        key = "engr" if sk["ink"]["style"] == "engraved" else "ink"
        self.text(layers, "FREDDY", bx0, 25, 30, anchor="lm", tracking=3.0, key=key)
        fw, _ = C.text_size("FREDDY", "semibold", 30 * S, 3.0 * S)
        vkey = "red" if self.name in ("harmonizer79",) else key
        self.text(layers, "VERB", bx0 + fw / S + 10, 25, 30, anchor="lm", tracking=3.0, key=vkey, kind="medium")
        self.text(layers, sk["tagline"], bx0 + 1, 46, 10.5, anchor="lm", tracking=1.8, key=key)
        # model plate text, right side of the top bar
        px0, py0, px1, py1 = self.L.notes["plate"]
        m1, m2 = sk["plate_text"]
        self.text(layers, m1, px1 - 6, 23, 19, anchor="rm", tracking=2.0, key=key)
        self.text(layers, m2, px1 - 6, 42, 10, anchor="rm", tracking=1.6, key=key)
        if self.name == "harmonizer79":
            d = ImageDraw.Draw(layers["red"])
            d.rectangle([(bx0) * S, 50 * S, (bx0 + 90) * S, 51.4 * S], fill=255)

    # ---------------------------------------------------------------- VU face (broadcast)
    def vu_meter(self, layers, ctl):
        c, S = self.c, self.S
        x, y, w, h = ctl["x"], ctl["y"], ctl["w"], ctl["h"]
        # chrome/black frame
        self.bezel((x, y, w, h), 5.0, 3.0, 1.6, 2.0, (0.06, 0.06, 0.06), 0.4, 40, (0.93, 0.85, 0.62), lip=1.2)
        sl, X, Y = self.win(x * S, y * S, (x + w) * S, (y + h) * S, pad=0)
        inside = C.cov(C.sdf_rrect(X, Y, x * S, y * S, w * S, h * S, 1.0 * S))
        # backlit face: warm, brighter in the middle-bottom, yellowed glass
        px, py = (x + w / 2) * S, (y + h - 0.06 * h) * S
        glow = np.exp(-(((X - px) / (w * 0.7 * S)) ** 2 + ((Y - (y + h * 0.65) * S) / (h * 0.9 * S)) ** 2))
        face = np.asarray((0.95, 0.86, 0.60), np.float32)
        c.paint(sl, inside, face[None, None, :] * (0.78 + 0.22 * glow)[..., None], spec=0.05, gloss=8, aniso=0.0)
        c.emit[sl] += (inside * glow * 0.32)[..., None] * np.asarray((1.0, 0.72, 0.35), np.float32)
        c.H[sl] = c.H[sl] * (1 - inside) + (-2.0) * inside
        # scale (drawn straight into albedo with a private mask)
        m = Image.new("L", (sl[1].stop - sl[1].start, sl[0].stop - sl[0].start), 0)
        red = Image.new("L", m.size, 0)
        ox, oy = sl[1].start, sl[0].start
        d, dr = ImageDraw.Draw(m), ImageDraw.Draw(red)
        pvx, pvy = px - ox, py - oy
        R = 0.70 * h * S

        def ang(vu):
            norm = (vu + 20.0) / 23.0
            return math.radians(-48.0 + 96.0 * norm ** 1.6)

        pts = []
        for i in range(61):
            vu = -20 + 23 * i / 60
            a = ang(vu)
            pts.append((pvx + math.sin(a) * R, pvy - math.cos(a) * R))
        zero = [p for i, p in enumerate(pts) if -20 + 23 * i / 60 >= 0]
        d.line(pts, fill=255, width=max(1, int(0.7 * S)))
        # red zone: thick arc above 0 VU
        reds = []
        for i in range(21):
            vu = 0 + 3 * i / 20
            a = ang(vu)
            reds.append((pvx + math.sin(a) * (R + 1.6 * S), pvy - math.cos(a) * (R + 1.6 * S)))
        dr.line(reds, fill=255, width=max(1, int(2.4 * S)))
        for vu in (-20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3):
            a = ang(vu)
            big = vu in (-20, -10, -5, 0, 3)
            r1 = R + (4.5 if big else 3.0) * S
            tgt = dr if vu > 0 else d
            tgt.line([(pvx + math.sin(a) * R, pvy - math.cos(a) * R), (pvx + math.sin(a) * r1, pvy - math.cos(a) * r1)],
                     fill=255, width=max(1, int((0.9 if big else 0.7) * S)))
            if vu in (-20, -10, -5, 0, 3):
                tr = R + 9.5 * S
                lbl = str(abs(vu)) if vu != 3 else "+3"
                C.draw_text_mask(red if vu > 0 else m, lbl, pvx + math.sin(a) * tr, pvy - math.cos(a) * tr, "semibold", 7.5 * S)
        C.draw_text_mask(m, "VU", pvx, pvy - 0.36 * h * S, "semibold", 12 * S, tracking=0.8 * S)
        C.draw_text_mask(red, "−", pvx - 0.36 * w * S, pvy - 0.20 * h * S, "semibold", 9 * S)
        C.draw_text_mask(red, "+", pvx + 0.36 * w * S, pvy - 0.20 * h * S, "semibold", 9 * S)
        mk = np.asarray(m, np.float32) / 255 * inside
        rk = np.asarray(red, np.float32) / 255 * inside
        c.paint(sl, mk, (0.08, 0.07, 0.06))
        c.paint(sl, rk, (0.72, 0.10, 0.06))
        c.emit[sl] *= (1 - 0.85 * np.maximum(mk, rk))[..., None]
        # needle pivot housing (dark half-disc at the bottom)
        hub = C.cov(np.hypot(X - px, Y - py) - 5.0 * S) * inside
        c.paint(sl, hub, (0.05, 0.05, 0.05), spec=0.3, gloss=20)
        c.emit[sl] *= (1 - hub)[..., None]

    # ---------------------------------------------------------------- wear
    def wear(self, U, guard):
        c, S, sk = self.c, self.S, self.sk
        wear = sk["wear"]
        lvl = wear["level"]
        r = C.rng(self.seed + 21)
        bare = np.asarray(sk["panel"]["bare"], np.float32)
        # --- scratches
        mask = Image.new("L", (self.W, self.H), 0)
        deep = Image.new("L", (self.W, self.H), 0)
        d, dd = ImageDraw.Draw(mask), ImageDraw.Draw(deep)
        hot = [(self.L.controls[k], v) for k, v in USAGE.items() if k in self.L.controls]
        n_rand = int(140 * lvl + 40)
        n_hot = int(700 * lvl + 100)
        for i in range(n_rand + n_hot):
            if i < n_rand:
                x0, y0 = r.uniform(0, LW), r.uniform(0, LH)
                ang = r.uniform(0, math.pi)
                ln = r.uniform(3, 46)
                arc = r.uniform(-0.02, 0.02)
            else:
                ctl, wgt = hot[r.integers(len(hot))]
                if r.random() > wgt:
                    continue
                cx, cy = ctl["x"] + ctl["w"] / 2, ctl["y"] + ctl["h"] / 2
                rad = r.uniform(0.45, 1.6) * max(ctl["w"], ctl["h"]) / 2 + r.uniform(0, 8)
                th = r.uniform(0, 2 * math.pi)
                x0, y0 = cx + math.cos(th) * rad, cy + math.sin(th) * rad
                ang = th + math.pi / 2 + r.normal(0, 0.4)   # mostly tangential (fingernails, rings)
                ln = r.uniform(2, 18)
                arc = r.uniform(-0.05, 0.05)
            pts = []
            steps = max(2, int(ln / 3))
            a = ang
            x, y = x0, y0
            for k in range(steps + 1):
                pts.append((x * S, y * S))
                x += math.cos(a) * ln / steps
                y += math.sin(a) * ln / steps
                a += arc * 3
            alpha = int(r.uniform(30, 150))
            width = 1 if r.random() < 0.8 else 2
            d.line(pts, fill=alpha, width=width)
            if wear.get("deep") and r.random() < 0.12:
                dd.line(pts, fill=230, width=int(r.uniform(2, 4)))
        if wear.get("deep"):
            # long road-case gouges and a few cable-drag scrapes
            for i in range(34):
                x, y = r.uniform(0, LW), r.uniform(0, LH)
                a = r.uniform(-0.5, 0.5) + (0 if r.random() < 0.7 else math.pi / 2)
                ln = r.uniform(30, 170)
                pts = []
                for k in range(12):
                    pts.append((x * S, y * S))
                    x += math.cos(a) * ln / 11
                    y += math.sin(a) * ln / 11
                    a += r.normal(0, 0.02)
                dd.line(pts, fill=int(r.uniform(150, 255)), width=int(r.uniform(2, 5)))
                d.line([(px + 2, py + 2) for px, py in pts], fill=90, width=1)
        sc = np.asarray(mask, np.float32) / 255.0
        sc *= (1 - 0.8 * guard)
        dp = np.asarray(deep, np.float32) / 255.0 * (1 - 0.9 * guard)
        scol = np.asarray(wear["scratch"], np.float32)
        mode = wear["scratch_mode"]
        k = 0.55 if mode != "mixed" else 0.35
        c.A[:] = c.A * (1 - (sc * k)[..., None]) + scol[None, None, :] * (sc * k)[..., None]
        c.spec += sc * 0.25
        c.H -= sc * 0.05
        if dp.any():
            c.A[:] = c.A * (1 - dp[..., None]) + bare[None, None, :] * dp[..., None]
            c.H -= dp * 0.25
            c.spec = c.spec * (1 - dp) + 0.6 * dp
        # --- fine hairline swirl (visible mostly as a spec/albedo change)
        hl = C.white(self.H, self.W, self.seed + 22)
        hl = ndimage.gaussian_filter1d((hl > 0.999).astype(np.float32), 6 * S, axis=1)
        hl = np.clip(hl * 8, 0, 1) * (0.25 + 0.75 * U) * lvl * 0.5
        c.A[:] += (hl * 0.06)[..., None]
        # --- scuffs: polished / grimy halos around hot controls
        sn = C.fbm(self.H, self.W, 10 * S, self.seed + 23, 3)
        scuff = np.clip(U * (sn - 0.35) * 2.2, 0, 1) * lvl
        if wear.get("deep"):
            # anodizing rubbed through around the busiest controls
            thru = np.clip((U - 0.35) * 2.5 * (sn - 0.3) * 2.5, 0, 1)
            c.A[:] = c.A * (1 - (0.45 * thru)[..., None]) + bare[None, None, :] * (0.45 * thru)[..., None]
            c.spec += thru * 0.2
        c.aniso *= 1 - 0.6 * scuff
        c.A[:] = c.A * (1 - 0.10 * scuff)[..., None] + bare[None, None, :] * (0.10 * scuff)[..., None]
        grime = np.clip(U * (C.fbm(self.H, self.W, 18 * S, self.seed + 24, 3) - 0.5) * 2, 0, 1) * lvl
        c.A[:] *= (1 - 0.10 * grime)[..., None]
        c.spec *= 1 - 0.3 * grime
        # --- edge wear to bare metal (panel border, ears): a thin rubbed line + occasional chips
        edge = wear["edge"]
        en = C.fbm(self.H, self.W, 1.6 * S, self.seed + 25, 4)
        n1 = C.fbm(self.H, self.W, 30 * S, self.seed + 26, 2)
        dist = self._d_edge / S
        X = np.arange(self.W, dtype=np.float32)[None, :] / S
        ear_d = np.minimum(X, LW - X)
        ear_zone = np.clip(1 - ear_d / EAR, 0, 1)
        width = 0.4 + 1.6 * edge * n1 * (0.5 + ear_zone)
        em = np.clip((width - dist) / 0.5, 0, 1)
        chips = np.clip((en - 0.70) * 6, 0, 1) * (dist < width * 2.6 + 0.8)
        em = np.maximum(em, chips)
        # ear faces: rubbed by rack neighbours / hands
        rub = np.clip((en - 0.66) * 4, 0, 1) * ear_zone ** 2 * edge * 0.6
        em = np.clip(np.maximum(em, rub), 0, 1) * min(1.0, edge + 0.2)
        if sk["panel"]["material"] in ("paint", "hammertone"):
            em *= 0.8
        em *= 0.85
        c.A[:] = c.A * (1 - em[..., None]) + bare[None, None, :] * em[..., None] * (0.85 + 0.2 * en[..., None])
        c.spec = c.spec * (1 - em) + 0.55 * em
        c.aniso = c.aniso * (1 - em) + 0.4 * em

    def apply_ink(self, layers, U):
        c, S, sk = self.c, self.S, self.sk
        ink = sk["ink"]
        fade_n = C.fbm(self.H, self.W, 4 * S, self.seed + 31, 3)
        speck = C.white(self.H, self.W, self.seed + 32)
        thin = ink["wear_thin"]
        fade = np.clip(thin * U * (0.6 + 0.8 * fade_n), 0, 0.42)
        holes = (speck > 1 - 0.18 * thin * (0.3 + U)).astype(np.float32)
        holes = ndimage.gaussian_filter(holes, 0.35 * S)
        keep = np.clip(1 - fade - 0.6 * holes, 0.55, 1)
        cols = {"ink": ink["color"], "ink2": ink.get("color2", ink["color"]), "red": ink["accent"]}
        for k, col in cols.items():
            a = np.asarray(layers[k], np.float32) / 255.0
            if not a.any():
                continue
            a *= keep
            c.paint((slice(None), slice(None)), a, col, spec=0.08, gloss=10)
            c.aniso *= 1 - a
            c.H += a * 0.03
        e = np.asarray(layers["engr"], np.float32) / 255.0
        if e.any():
            # engraved, black paint-filled; fill wears out of the busiest legends a bit
            ef = e * np.clip(1 - 0.5 * fade - 0.3 * holes, 0.6, 1)
            c.H -= ndimage.gaussian_filter(e, 0.3 * S) * 0.45
            c.paint((slice(None), slice(None)), ef, ink["color"], spec=0.05, gloss=8)
            c.aniso *= 1 - e

    def dust(self, U):
        c, S, sk = self.c, self.S, self.sk
        lvl = sk["wear"]["dust"]
        ao = np.clip(ndimage.gaussian_filter(c.H, 3 * S) - c.H, 0, 3) / 3
        n = C.fbm(self.H, self.W, 6 * S, self.seed + 41, 4)
        dm = np.clip(ao * 1.5, 0, 1) * (0.4 + 0.8 * n) * lvl * 0.6
        # bottom-heavy: dust settles on the lower parts of a vertical panel
        Y = np.arange(self.H, dtype=np.float32)[:, None] / self.H
        dm *= 0.6 + 0.6 * Y
        dcol = np.asarray((0.58, 0.56, 0.52), np.float32)
        c.A[:] = c.A * (1 - dm[..., None]) + dcol[None, None, :] * dm[..., None]
        c.spec *= 1 - 0.6 * dm
        # specks / lint
        r = C.rng(self.seed + 42)
        m = Image.new("L", (self.W, self.H), 0)
        d = ImageDraw.Draw(m)
        for i in range(int(2600 * lvl)):
            x, y = r.uniform(0, self.W), r.uniform(0, self.H)
            rad = r.uniform(0.3, 1.0) * S * (1.6 if r.random() < 0.05 else 1)
            d.ellipse([x - rad, y - rad, x + rad, y + rad], fill=int(r.uniform(40, 170)))
        for i in range(int(40 * lvl)):  # lint fibres
            x, y = r.uniform(0, self.W), r.uniform(0, self.H)
            pts = [(x, y)]
            a = r.uniform(0, 6.28)
            for k in range(6):
                a += r.normal(0, 0.5)
                x += math.cos(a) * 1.6 * S
                y += math.sin(a) * 1.6 * S
                pts.append((x, y))
            d.line(pts, fill=int(r.uniform(60, 140)), width=1)
        sp = np.asarray(m, np.float32) / 255.0 * 0.55
        c.A[:] = c.A * (1 - sp[..., None]) + dcol[None, None, :] * 1.15 * sp[..., None]

    # ---------------------------------------------------------------- extras
    def dymo(self, x, y, text, color=(0.05, 0.05, 0.06), rot=0.0, h=15.0, px=12.0, layers=None):
        """Embossed label-maker tape: raised glossy tape, letters stretched whitish."""
        c, S = self.c, self.S
        tw, _ = C.text_size(text, "semibold", px * S, 1.8 * S)
        w = tw / S + 14
        tape = Image.new("L", (int(w * S) + 8, int(h * S) + 8), 0)
        ImageDraw.Draw(tape).rectangle([4, 4, 4 + w * S, 4 + h * S], fill=255)
        let = Image.new("L", tape.size, 0)
        C.draw_text_mask(let, text, 4 + 7 * S, 4 + h * S / 2, "semibold", px * S, "lm", 1.8 * S)
        if rot:
            tape = tape.rotate(rot, resample=Image.BICUBIC, expand=False)
            let = let.rotate(rot, resample=Image.BICUBIC, expand=False)
        ox, oy = int(x * S) - 4, int(y * S) - 4
        sl = (slice(oy, oy + tape.height), slice(ox, ox + tape.width))
        ta = np.asarray(tape, np.float32) / 255.0
        la = np.asarray(let, np.float32) / 255.0
        # tape edges are slightly ragged (scissor cut ends)
        c.H[sl] = c.H[sl] * (1 - ta) + (c.H[sl] + 0.5) * ta + ndimage.gaussian_filter(la, 0.5 * S) * 0.35
        c.paint(sl, ta, color, spec=0.6, gloss=50, aniso=0.0)
        c.paint(sl, la * 0.9, (0.86, 0.86, 0.84), spec=0.4, gloss=30)
        self.text_guard.paste(255, (ox, oy), let)
        sh = ndimage.gaussian_filter(ta, 1.2 * S)
        sh = C.shift2d(sh, -int(1.5 * S), -int(1.0 * S))
        self.post_mul[sl] = np.maximum(self.post_mul[sl], sh * (1 - ta) * 0.35)

    def tape(self, x, y, w, h, text, rot=0.0, ink=(0.08, 0.1, 0.35), px=15, color=(0.95, 0.93, 0.82), alpha=0.96):
        """Masking tape with a handwritten-looking marker note."""
        c, S = self.c, self.S
        r = C.rng(int(x * 13 + y))
        pad = int(8 * S)
        tw, th = int(w * S), int(h * S)
        base = Image.new("L", (tw + 2 * pad, th + 2 * pad), 0)
        dr = ImageDraw.Draw(base)
        # torn ends
        left = [(pad + r.uniform(0, 2.5) * S, pad + th * k / 8) for k in range(9)]
        right = [(pad + tw - r.uniform(0, 2.5) * S, pad + th * (8 - k) / 8) for k in range(9)]
        dr.polygon(left + right, fill=255)
        let = Image.new("L", base.size, 0)
        # handwriting: per-letter jitter in size, baseline and angle
        xx = pad + 6 * S
        f_kind = "medium"
        for ch in text:
            s = px * S * r.uniform(0.92, 1.1)
            glyph = Image.new("L", (int(s * 1.4), int(s * 1.6)), 0)
            C.draw_text_mask(glyph, ch, glyph.width / 2, glyph.height / 2, f_kind, s)
            glyph = glyph.rotate(r.normal(0, 5) - 4, resample=Image.BICUBIC)
            glyph = glyph.transform(glyph.size, Image.AFFINE, (1, 0.18, -0.1 * glyph.height, 0, 1, 0), Image.BICUBIC)
            let.paste(255, (int(xx - glyph.width * 0.2), int(pad + th / 2 - glyph.height / 2 + r.normal(0, 0.6) * S)), glyph)
            xx += C.font(f_kind, s).getlength(ch) * r.uniform(0.95, 1.05) + 0.5 * S
        let = Image.fromarray(ndimage.maximum_filter(np.asarray(let), size=2))
        if rot:
            base = base.rotate(rot, resample=Image.BICUBIC)
            let = let.rotate(rot, resample=Image.BICUBIC)
        ox, oy = int(x * S) - pad, int(y * S) - pad
        sl = (slice(oy, oy + base.height), slice(ox, ox + base.width))
        ta = np.asarray(base, np.float32) / 255.0
        la = np.asarray(let, np.float32) / 255.0 * ta
        fib = C.fbm(ta.shape[0], ta.shape[1], 1.5 * S, int(x + y), 3)
        tcol = np.asarray(color, np.float32)[None, None, :] * (0.94 + 0.12 * fib)[..., None]
        c.H[sl] = c.H[sl] * (1 - ta) + (c.H[sl] + 0.25) * ta
        c.paint(sl, ta * alpha, tcol, spec=0.04, gloss=6, aniso=0.0)
        c.paint(sl, la * 0.88, ink, spec=0.1, gloss=10)
        self.text_guard.paste(255, (ox, oy), let)
        sh = C.shift2d(ndimage.gaussian_filter(ta, 1.0 * S), -int(1.0 * S), -int(0.8 * S))
        self.post_mul[sl] = np.maximum(self.post_mul[sl], sh * (1 - ta) * 0.3)

    def plate(self, x, y, w, h, lines, rivets=True):
        """Small brushed aluminium plate with engraved/stamped text."""
        c, S = self.c, self.S
        sl, X, Y = self.win(x * S, y * S, (x + w) * S, (y + h) * S)
        d = C.sdf_rrect(X, Y, x * S, y * S, w * S, h * S, 1.6 * S)
        a = C.cov(d)
        n = C.brushed(X.shape[0], X.shape[1], S, int(x + y), 30)
        c.H[sl] = c.H[sl] * (1 - a) + (c.H[sl] + 0.6 * C.smoothstep(0, 1.2 * S, -d)) * a
        col = np.asarray((0.74, 0.74, 0.73), np.float32)[None, None, :] * (1 + 0.08 * n)[..., None]
        c.paint(sl, a, col, spec=0.55, gloss=30, aniso=0.6)
        m = Image.new("L", (self.W, self.H), 0)
        for i, (txt, sz, kind) in enumerate(lines):
            yy = y + h * (i + 1) / (len(lines) + 1)
            C.draw_text_mask(m, txt, (x + w / 2) * S, yy * S, kind, sz * S, "mm", 0.8 * S)
            C.draw_text_mask(self.text_guard, txt, (x + w / 2) * S, yy * S, kind, sz * S, "mm", 0.8 * S)
        ma = np.asarray(m, np.float32)[sl] / 255.0
        c.paint(sl, ma, (0.1, 0.1, 0.1), spec=0.05)
        c.H[sl] -= ma * 0.2
        if rivets:
            for px in (x + 3.2, x + w - 3.2):
                rr = np.hypot(X - px * S, Y - (y + h / 2) * S)
                ra = C.cov(rr - 1.5 * S)
                c.H[sl] += ra * 0.8 * np.sqrt(np.clip(1 - (rr / (1.5 * S)) ** 2, 0, 1))
                c.paint(sl, ra, (0.6, 0.6, 0.6), spec=0.7, gloss=40)
        sh = C.cov(C.sdf_rrect(X, Y, (x + 0.8) * S, (y + 1.2) * S, w * S, h * S, 1.6 * S), 1.2 * S)
        self.post_mul[sl] = np.maximum(self.post_mul[sl], sh * (1 - a) * 0.3)

    def sticker(self, x, y, w, h, lines, rot=-3.0, color=(0.93, 0.88, 0.62), band=(0.70, 0.2, 0.12)):
        c, S = self.c, self.S
        pad = int(6 * S)
        img = Image.new("L", (int(w * S) + 2 * pad, int(h * S) + 2 * pad), 0)
        ImageDraw.Draw(img).rounded_rectangle([pad, pad, pad + w * S, pad + h * S], radius=2 * S, fill=255)
        bandm = Image.new("L", img.size, 0)
        ImageDraw.Draw(bandm).rectangle([pad, pad, pad + w * S, pad + 7 * S], fill=255)
        txt = Image.new("L", img.size, 0)
        C.draw_text_mask(txt, lines[0], pad + w * S / 2, pad + 3.6 * S, "semibold", 7.5 * S, "mm", 0.8 * S, fill=0)
        bandm = Image.fromarray(np.minimum(np.asarray(bandm), 255 - np.asarray(txt) * 0 ))
        btxt = Image.new("L", img.size, 0)
        C.draw_text_mask(btxt, lines[0], pad + w * S / 2, pad + 3.6 * S, "semibold", 7.5 * S, "mm", 0.8 * S)
        for i, t in enumerate(lines[1:]):
            C.draw_text_mask(txt, t, pad + w * S / 2, pad + (12 + i * 9.5) * S, "mono" if i else "semibold", 8.5 * S, "mm", 0.4 * S)
        mats = [img, bandm, btxt, txt]
        mats = [m.rotate(rot, resample=Image.BICUBIC) for m in mats]
        ox, oy = int(x * S) - pad, int(y * S) - pad
        sl = (slice(oy, oy + img.height), slice(ox, ox + img.width))
        a, b, bt, t = [np.asarray(m, np.float32) / 255.0 for m in mats]
        fadeN = C.fbm(a.shape[0], a.shape[1], 6 * S, int(x), 3)
        c.H[sl] = c.H[sl] * (1 - a) + (c.H[sl] + 0.15) * a
        c.paint(sl, a, np.asarray(color, np.float32)[None, None, :] * (0.92 + 0.1 * fadeN)[..., None], spec=0.1, gloss=12, aniso=0.0)
        c.paint(sl, b * (1 - bt) * a, band, spec=0.1)
        c.paint(sl, t * a * 0.8, (0.15, 0.13, 0.12))
        # sun-faded + scuffed corners
        c.A[sl] = c.A[sl] * (1 - (0.15 * fadeN * a)[..., None]) + 0.15 * (fadeN * a)[..., None] * np.asarray((0.95, 0.95, 0.9))

    def extras_pre(self, layers):
        sk, S = self.sk, self.S
        ex = sk["extras"]
        vu = sk["parts"].get("vu")
        if "serial_plate" in ex:
            self.plate(1350, 288, 92, 26, [("SERIAL No.", 7.5, "semibold"), (f"{self.seed * 53 + 1017:06d}", 9, "mono")])
        if "inventory_sticker" in ex:
            self.sticker(1452, 282, 64, 40, ["PROPERTY OF", "STUDIO 2", "INV 0117"], rot=-4)
        if "asset_tag" in ex:
            self.plate(1030, 16, 118, 28, [("STUDIO B  ·  REVERB 2", 8.5, "semibold"), ("ASSET 0731-FV", 8.5, "mono")])
        if "dymo_proto" in ex:
            self.dymo(1036, 22, "PROTO REV C", color=(0.62, 0.08, 0.06), rot=-1.4)
            self.dymo(1352, 300, "DO NOT SHIP", color=(0.08, 0.20, 0.45), rot=1.0)
        if "masking_tape" in ex:
            self.tape(130, 481, 206, 26, "FREDDY - SNARE BLOOM", rot=-1.8, px=16)
        if "tape_small" in ex:
            self.tape(1356, 284, 150, 26, "FOH: MIX 35%!", rot=2.2, ink=(0.05, 0.05, 0.05), px=16)
        if "gaffer" in ex:
            self.gaffer_residue(1030, 14, 150, 30)
            self.gaffer_residue(600, 534, 52, 40)
        if "dent" in ex:
            self.dent()
        if "pencil_marks" in ex:
            self.text(layers, "cal 3/86 ✓", 1184, 579, 10, kind="medium", anchor="mm", tracking=0.4, guard=False, rotate=2)

    def extras_post(self, layers, U):
        pass

    def gaffer_residue(self, x, y, w, h):
        c, S = self.c, self.S
        sl, X, Y = self.win(x * S, y * S, (x + w) * S, (y + h) * S)
        n = C.fbm(X.shape[0], X.shape[1], 5 * S, int(x * 3 + y), 4)
        d = C.sdf_rrect(X, Y, x * S, y * S, w * S, h * S, 2 * S)
        edge = C.smoothstep(4 * S, -6 * S, d)
        a = np.clip((n - 0.38) * 3, 0, 1) * edge * 0.55
        # residue: matte, slightly darker, picks up fibres + grime
        fib = ndimage.gaussian_filter1d(C.white(X.shape[0], X.shape[1], int(y)), 2 * S, axis=1)
        c.A[sl] = c.A[sl] * (1 - (a * 0.45)[..., None]) + (a * 0.45)[..., None] * np.asarray((0.30, 0.29, 0.27))
        c.A[sl] *= (1 - 0.15 * a * (fib > 0.55))[..., None]
        c.spec[sl] *= 1 - 0.8 * a
        c.aniso[sl] *= 1 - 0.9 * a

    def dent(self):
        """Dented bottom-right ear corner (dropped flight case)."""
        c, S = self.c, self.S
        cx, cy = (LW - 12) * S, (LH - 10) * S
        sl, X, Y = self.win(cx - 40 * S, cy - 40 * S, self.W, self.H)
        rr = np.hypot(X - cx, Y - cy)
        c.H[sl] -= 4.0 * np.exp(-(rr / (12 * S)) ** 2)
        crease = np.exp(-(((X - cx) + (Y - cy)) / (2.2 * S)) ** 2) * np.exp(-(rr / (22 * S)) ** 2)
        c.H[sl] += 0.9 * crease
        n = C.fbm(X.shape[0], X.shape[1], 2 * S, 77, 3)
        bare = np.clip(np.exp(-(rr / (14 * S)) ** 2) * 1.6 * (n > 0.4), 0, 1)
        c.paint(sl, bare, self.sk["panel"]["bare"], spec=0.6, gloss=30, aniso=0.0)

    # ---------------------------------------------------------------- final look
    def photo_finish(self, out):
        S = self.S
        Y, X = np.mgrid[0:self.H, 0:self.W].astype(np.float32)
        # key light falloff from top-left + gentle lens vignette
        fall = 1.06 - 0.12 * ((X / self.W) * 0.6 + (Y / self.H) * 0.4)
        vig = 1 - 0.10 * (((X / self.W - 0.5) * 1.6) ** 2 + ((Y / self.H - 0.5) * 1.2) ** 2)
        out = out * (fall * vig)[..., None]
        grain = (C.white(self.H, self.W, self.seed + 99) - 0.5) * 0.012
        out = out + grain[..., None]
        return np.clip(out, 0, 1)

    # ---------------------------------------------------------------- overlay
    def overlay(self):
        """Transparent glass layer: reflections, dust and fingerprints over display windows."""
        S = self.S
        rgba = np.zeros((self.H, self.W, 4), np.float32)
        r = C.rng(self.seed + 71)
        prints_on = {"envelope", "readout"}
        for ctl in self.L.controls.values():
            b = ctl["_deco"].get("bezel")
            if b not in ("display", "vu", "stepper"):
                continue
            x, y, w, h = self.rect_px(ctl)
            sl, X, Y = self.win(x, y, x + w, y + h, pad=0)
            inside = C.cov(C.sdf_rrect(X, Y, x, y, w, h, 1.0 * S))
            u = (X - x) / max(w, 1)
            v = (Y - y) / max(h, 1)
            # broad diagonal reflection of the studio light
            diag = (u * w * 0.35 + v * h) / (w * 0.35 + h)
            refl = np.exp(-((diag - 0.18) / 0.10) ** 2) * 0.07 + np.exp(-((diag - 0.30) / 0.03) ** 2) * 0.035
            refl += (1 - v) * 0.02
            a = refl
            col = np.ones(X.shape + (3,), np.float32)
            # dust specks on the glass
            m = Image.new("L", (X.shape[1], X.shape[0]), 0)
            d = ImageDraw.Draw(m)
            for i in range(int(w * h / (S * S) / 160)):
                px, py = r.uniform(0, X.shape[1]), r.uniform(0, X.shape[0])
                rad = r.uniform(0.25, 0.8) * S
                d.ellipse([px - rad, py - rad, px + rad, py + rad], fill=int(r.uniform(40, 120)))
            dust = np.asarray(m, np.float32) / 255.0 * 0.35
            a = np.maximum(a, dust)
            if ctl["id"] in prints_on:
                for k in range(2 if ctl["id"] == "envelope" else 1):
                    fx = x + r.uniform(0.15, 0.85) * w
                    fy = y + r.uniform(0.3, 0.9) * h
                    rot = r.uniform(0, math.pi)
                    du = (X - fx) * math.cos(rot) + (Y - fy) * math.sin(rot)
                    dv = -(X - fx) * math.sin(rot) + (Y - fy) * math.cos(rot)
                    rx, ry = 9 * S, 12 * S
                    e = (du / rx) ** 2 + (dv / ry) ** 2
                    ridges = 0.5 + 0.5 * np.sin(np.sqrt(e) * 26 + 0.6 * np.sin(du / (2 * S)))
                    fp = np.clip(1 - e, 0, 1) ** 0.6 * ridges * 0.05
                    a = np.maximum(a, fp)
            if b == "vu":
                a = a * 1.4
            a = a * inside
            rgba[sl][..., :3] = np.maximum(rgba[sl][..., :3], col)
            rgba[sl][..., 3] = np.maximum(rgba[sl][..., 3], a)
        return np.clip(rgba, 0, 1)
