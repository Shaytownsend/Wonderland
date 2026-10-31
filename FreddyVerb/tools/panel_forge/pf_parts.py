"""panel_forge parts: knob/jog filmstrips, button states, LEDs, VU needle.

Every part is rendered as a height field + material and lit with the same key
light as the panel (pf_core.LIGHT), so highlights and shadows stay fixed while
the knob geometry rotates.
"""
from __future__ import annotations

import math

import numpy as np
from PIL import Image

import pf_core as C

SS = 2  # supersampling factor for parts


def _finish(c: C.Canvas, alpha: np.ndarray, ambient=0.34, diffuse=0.8):
    rgb = C.shade(c, ambient=ambient, diffuse=diffuse, shadow_strength=0.6, ao_strength=0.5)
    return np.concatenate([rgb, alpha[..., None]], axis=-1)


def _down(rgba: np.ndarray, ss=SS) -> np.ndarray:
    """Box-downsample a supersampled RGBA (premultiplied for clean edges)."""
    h, w = rgba.shape[0] // ss, rgba.shape[1] // ss
    a = rgba[: h * ss, : w * ss]
    pm = a.copy()
    pm[..., :3] *= pm[..., 3:4]
    pm = pm.reshape(h, ss, w, ss, 4).mean(axis=(1, 3))
    out = pm.copy()
    al = np.maximum(pm[..., 3:4], 1e-6)
    out[..., :3] = np.where(pm[..., 3:4] > 0, pm[..., :3] / al, 0)
    return out


def to_rgba_img(arr: np.ndarray) -> Image.Image:
    return Image.fromarray(np.clip(arr * 255 + 0.5, 0, 255).astype(np.uint8), "RGBA")


# ----------------------------------------------------------------------------
# knobs
# ----------------------------------------------------------------------------
def knob_frame(st: dict, size: float, S: int, angle_deg: float, seed: int = 1, jog=False):
    """One knob frame (size logical px) rendered at scale S. angle 0 = pointer up."""
    Se = S * SS
    N = int(round(size * S)) * SS
    c = C.Canvas(N, N, Se)
    X, Y = C.grid(N, N)
    u = X / Se - size / 2
    v = Y / Se - size / 2
    r = np.hypot(u, v)
    th = math.radians(angle_deg)
    # local (rotating) coordinates: lu across the pointer, lv along it (negative = pointer tip)
    lu = u * math.cos(th) + v * math.sin(th)
    lv = -u * math.sin(th) + v * math.cos(th)
    phi_l = np.arctan2(lu, -lv)   # angle around the knob in the knob's own frame
    phi_s = np.arctan2(u, -v)     # screen angle (for lighting-bound effects)
    noise = C.value_noise(N, N, 3 * Se, seed) - 0.5

    body = np.asarray(st["body"], np.float32)
    skirt_c = np.asarray(st.get("skirt", st["body"]), np.float32)
    line_c = np.asarray(st.get("line", (0.9, 0.9, 0.9)), np.float32)

    if jog:
        R = 0.47 * size
        knurl = 0.012 * np.cos(72 * phi_l)
        Rk = R * (1 + knurl)
        d = r - Rk
        alpha = C.cov(d * Se, 0.8) if False else np.clip(0.5 - d * Se / 1.5, 0, 1)
        h = 5.0 * C.smoothstep(0, 1.8, -d)
        # dished face
        h -= 0.9 * (1 - (r / R) ** 2).clip(0, 1)
        # finger dimple (rotates)
        fd = np.hypot(lu, lv + 0.26 * size)
        dimple = np.clip(1 - (fd / (0.10 * size)) ** 2, 0, 1)
        h -= 1.6 * dimple
        c.H[:] = h
        c.A[:] = body * (1 + 0.15 * noise)[..., None]
        rim = C.smoothstep(R - 3.0, R - 1.5, r)
        c.A[:] = c.A * (1 - rim[..., None]) + np.asarray(st["rim"], np.float32) * 0.25 * rim[..., None]
        c.spec[:] = 0.35
        c.gloss[:] = 28
        # subtle rubber ring
        ring = np.exp(-((r - 0.36 * size) / 0.5) ** 2)
        c.H[:] -= 0.25 * ring
        return _down(_finish(c, alpha))

    kind = st["kind"]
    if kind == "chicken":
        # fluted round skirt + sculpted pointer (domed head, raised spine, hollow flanks, beak)
        Rs = 0.29 * size
        Rse = Rs * (1 + 0.012 * np.cos(36 * phi_l))
        ds = r - Rse
        a_sk = np.clip(0.5 - ds * Se / 1.5, 0, 1)
        h_sk = 1.6 * C.smoothstep(0, 0.9, -ds) + 0.7 * (1 - r / Rs).clip(0, 1)

        def capsule(px, py, ax, ay, ra, bx, by, rb):
            pax, pay = px - ax, py - ay
            bax, bay = bx - ax, by - ay
            L2 = bax * bax + bay * bay
            t = np.clip((pax * bax + pay * bay) / L2, 0, 1)
            rr = ra + (rb - ra) * t
            dx, dy = pax - bax * t, pay - bay * t
            return np.hypot(dx, dy) - rr

        d1 = capsule(lu, lv, 0, -0.43 * size, 0.035 * size, 0, -0.02 * size, 0.17 * size)
        d2 = capsule(lu, lv, 0, -0.02 * size, 0.17 * size, 0, 0.33 * size, 0.075 * size)
        dp = np.minimum(d1, d2)
        a_p = np.clip(0.5 - dp * Se / 1.5, 0, 1)
        prof = np.sqrt(np.clip(-dp / (0.09 * size), 0, 1))
        spine = np.exp(-(lu / (0.035 * size)) ** 2)
        flank = np.exp(-((np.abs(lu) - 0.10 * size) / (0.035 * size)) ** 2) * (lv < 0.12 * size)
        head = np.exp(-((lu / (0.15 * size)) ** 2 + ((lv + 0.01 * size) / (0.15 * size)) ** 2))
        beak = np.clip((-lv - 0.18 * size) / (0.25 * size), 0, 1)
        h_p = 3.0 + prof * (4.2 + 1.4 * spine + 1.6 * head - 1.0 * flank - 2.0 * beak)
        alpha = np.maximum(a_sk, a_p)
        c.H[:] = np.maximum(h_sk * a_sk, h_p * a_p)
        c.A[:] = skirt_c * (1 + 0.2 * noise)[..., None]
        c.A[:] = c.A * (1 - a_p[..., None]) + body * (1 + 0.25 * noise)[..., None] * a_p[..., None]
        c.spec[:] = st["spec"]
        c.gloss[:] = st["gloss"]
        ln = C.cov((np.abs(lu) - 0.02 * size) * Se) * (lv < -0.08 * size) * (dp < -0.012 * size)
        c.paint((slice(None), slice(None)), ln, line_c, spec=0.3, gloss=20)
        c.H[:] -= ln * 0.2
        knob = _finish(c, alpha)
        # cast shadow on the panel, rotating with the pointer (panel only bakes the skirt's)
        from scipy import ndimage as _nd
        sh = C.shift2d(a_p, -int(round(1.8 * Se)), -int(round(1.2 * Se)))
        sh = _nd.gaussian_filter(sh, 1.1 * Se) * 0.7
        sh = sh * np.clip((r - Rs * 0.9) / 1.5, 0, 1)
        out = knob.copy()
        out[..., 3] = knob[..., 3] + sh * (1 - knob[..., 3])
        out[..., :3] = np.where(out[..., 3:4] > 0, knob[..., :3] * (knob[..., 3:4] / np.maximum(out[..., 3:4], 1e-6)), 0)
        return _down(out)

    # --- skirted round knob
    Rs = 0.47 * size
    Rb = 0.355 * size
    Rc = 0.25 * size
    ribs = st.get("ribs", 0)
    sk_ribs = st.get("skirt_ribs", 0)
    ds = r - Rs
    alpha = np.clip(0.5 - ds * Se / 1.5, 0, 1)
    h_sk = 2.2 * C.smoothstep(0, 1.1, -ds) + 0.9 * ((Rs - r) / (Rs - Rb)).clip(0, 1)
    if sk_ribs:
        zone = C.smoothstep(Rb + 0.6, Rb + 1.6, r) * C.smoothstep(Rs, Rs - 1.2, r)
        h_sk += st.get("knurl", 0.35) * zone * np.cos(sk_ribs * phi_l)
    Rbe = Rb * (1 + (0.02 * np.cos(ribs * phi_l) if ribs else 0))
    db = r - Rbe
    h_b = 3.0 + 5.0 * np.sqrt(np.clip(-db / 1.6, 0, 1))
    a_b = np.clip(0.5 - db * Se / 1.2, 0, 1)
    h = np.maximum(h_sk, h_b * a_b + h_sk * (1 - a_b))
    # cap insert (slight dish)
    a_c = np.clip(0.5 - (r - Rc) * Se / 1.0, 0, 1)
    h -= a_c * (0.25 * (1 - (r / Rc) ** 2).clip(0, 1) + 0.25)
    c.H[:] = h
    c.A[:] = skirt_c * (1 + 0.18 * noise)[..., None]
    c.A[:] = c.A * (1 - a_b[..., None]) + body * (1 + 0.18 * noise)[..., None] * a_b[..., None]
    c.spec[:] = st["spec"]
    c.gloss[:] = st["gloss"]
    cap = st.get("cap")
    if cap is not None:
        capc = np.asarray(cap, np.float32)
        if st.get("cap_kind") == "metal":
            # concentric machining: bow-tie highlight along the light axis (lighting-bound)
            phiL = math.atan2(C.LIGHT[0], -C.LIGHT[1])
            bow = np.exp(-(np.sin(phi_s - phiL) / 0.22) ** 2)
            rings = 0.5 + 0.5 * np.sin(r * Se * 2.1)
            col = capc * (0.62 + 0.55 * bow + 0.06 * rings)[..., None]
            c.paint((slice(None), slice(None)), a_c, col, spec=0.6, gloss=50)
        else:
            c.paint((slice(None), slice(None)), a_c, capc * (1 + 0.1 * noise)[..., None], spec=st["spec"] * 0.8, gloss=st["gloss"])
    # indicator line: on the body ring (and down the skirt)
    lw = 0.028 * size
    on_cap = st.get("line_on_cap", False)
    r0 = 0.04 * size if on_cap else (Rc + 0.3 if cap is not None else 0.08 * size)
    r1 = Rb - 0.4 if on_cap else Rs - 0.7
    ln = C.cov((np.abs(lu) - lw) * Se) * (lv < 0) * C.smoothstep(r0, r0 + 0.3, r) * C.smoothstep(r1, r1 - 0.3, r)
    c.paint((slice(None), slice(None)), ln, line_c, spec=0.25, gloss=20)
    c.H[:] -= ln * 0.12
    return _down(_finish(c, alpha))


def filmstrip(st, size, S, frames=128, sweep=270.0, start=-135.0, jog=False, seed=1):
    out = []
    for i in range(frames):
        if jog:
            ang = 360.0 * i / frames
        else:
            ang = start + sweep * i / (frames - 1)
        out.append(knob_frame(st, size, S, ang, seed, jog))
    return np.concatenate(out, axis=0)


# ----------------------------------------------------------------------------
# buttons
# ----------------------------------------------------------------------------
def button(st: dict, w: float, h: float, S: int, state: str, led=None, lit=None):
    """state: up | down | on.  Returns RGBA float array (w*S x h*S)."""
    Se = S * SS
    Wp, Hp = int(round(w * S)) * SS, int(round(h * S)) * SS
    c = C.Canvas(Wp, Hp, Se)
    X, Y = C.grid(Hp, Wp)
    u, v = X / Se, Y / Se
    kind = st["kind"]
    base = np.asarray(st.get("color", (0.5, 0.5, 0.5)), np.float32)
    noise = C.value_noise(Hp, Wp, 2 * Se, 5) - 0.5
    pressed = state == "down"
    if kind in ("square", "membrane", "lamp"):
        inset = 0.4
        rad = {"square": 2.2, "membrane": 4.5, "lamp": 1.6}[kind]
        d = C.sdf_rrect(u, v, inset, inset, w - 2 * inset, h - 2 * inset, rad)
        alpha = np.clip(0.5 - d * Se / 1.5, 0, 1)
        if kind == "square":
            top = 3.2 if not pressed else 2.0
            hh = top * C.smoothstep(0, 1.6, -d) - 0.25 * (1 - ((u - w / 2) / (w / 2)) ** 2 - ((v - h / 2) / (h / 2)) ** 2).clip(0, 1)
            col = base * (1 + 0.15 * noise)[..., None] * (0.86 if pressed else 1.0)
            c.spec[:] = st["spec"]
            c.gloss[:] = st["gloss"]
        elif kind == "membrane":
            top = 2.4 if not pressed else 1.2
            hh = top * np.power(C.smoothstep(0, 5.0, -d), 0.7)
            col = base * (1 + 0.08 * noise)[..., None] * (0.9 if pressed else 1.0)
            c.spec[:] = st["spec"]
            c.gloss[:] = st["gloss"]
        else:  # lamp: dark collar + milky lens
            top = 2.6 if not pressed else 1.8
            dl = C.sdf_rrect(u, v, 2.4, 2.4, w - 4.8, h - 4.8, 1.0)
            lens = np.clip(0.5 - dl * Se / 1.2, 0, 1)
            hh = top * C.smoothstep(0, 1.2, -d) + 0.4 * lens * C.smoothstep(0, 2.0, -dl)
            collar = np.asarray((0.06, 0.06, 0.06), np.float32)
            lensc = base * (1 + 0.05 * noise)[..., None]
            col = collar * (1 - lens[..., None]) + lensc * lens[..., None]
            c.spec[:] = 0.5
            c.gloss[:] = 50
            if state == "on" and lit is not None:
                litc = np.asarray(lit, np.float32)
                hot = np.exp(-(((u - w / 2) / (w * 0.45)) ** 2 + ((v - h / 2) / (h * 0.45)) ** 2))
                c.emit[:] = (lens * (0.55 + 0.45 * hot))[..., None] * litc
                col = col * (1 - 0.5 * lens[..., None]) + litc * 0.5 * lens[..., None]
            # legend-less frosted lens texture
        c.H[:] = hh
        c.A[:] = col
        if led is not None and st.get("lens") is not None:
            lx, ly, lw_, lh_ = led
            dl = C.sdf_rrect(u, v, lx - 0.6, ly - 0.6, lw_ + 1.2, lh_ + 1.2, min(lw_, lh_) / 2 + 0.6)
            al = np.clip(0.5 - dl * Se / 1.0, 0, 1)
            c.H[:] -= al * 0.5
            c.paint((slice(None), slice(None)), al, (0.02, 0.02, 0.02), spec=0.2)
            dl2 = C.sdf_rrect(u, v, lx, ly, lw_, lh_, min(lw_, lh_) / 2)
            al2 = np.clip(0.5 - dl2 * Se / 1.0, 0, 1)
            c.paint((slice(None), slice(None)), al2, st["lens"], spec=0.9, gloss=80)
        return _down(_finish(c, alpha))

    if kind == "toggle":
        # bat-handle toggle seen from the front: small black-oxide nut, long chrome bat
        # with a ball tip. ON = bat up, OFF = bat down, pressed = mid travel.
        metal = np.asarray(st.get("metal", (0.8, 0.8, 0.8)), np.float32)
        cx, cy = w / 2, h / 2
        rr = np.hypot(u - cx, v - cy)
        ang = np.arctan2(v - cy, u - cx)
        hexr = 5.0 / np.cos(((ang + math.pi / 6) % (math.pi / 3)) - math.pi / 6)
        dw = rr - 6.8
        dn = rr - hexr
        aw = np.clip(0.5 - dw * Se / 1.5, 0, 1)
        an = np.clip(0.5 - dn * Se / 1.5, 0, 1)
        hh = 0.7 * aw + an * 1.8 * C.smoothstep(0, 1.0, -dn)
        db = rr - 3.0
        ab = np.clip(0.5 - db * Se / 1.2, 0, 1)
        hh = np.maximum(hh, ab * (3.4 + 0.8 * np.sqrt(np.clip(1 - (rr / 3.0) ** 2, 0, 1))))
        L = 10.2 if state != "down" else 2.5
        sgn = -1.0 if state == "on" else 1.0
        pax, pay = u - cx, v - cy
        bay = sgn * L
        t = np.clip((pay * bay) / max(1e-3, bay * bay), 0, 1)
        rad = 1.7 + 0.5 * t
        dbat = np.hypot(pax, pay - bay * t) - rad
        dball = np.hypot(pax, pay - bay) - 3.4
        dbat = np.minimum(dbat, dball)
        abat = np.clip(0.5 - dbat * Se / 1.2, 0, 1)
        hbat = (6.0 + 5.0 * (1 - t)) + 2.2 * np.sqrt(np.clip(-dbat / 3.0, 0, 1))
        hh = np.maximum(hh, abat * hbat)
        # dark rim around the bat so it separates from the nut at 1x
        rim = np.clip(0.5 - (dbat - 0.7) * Se / 1.2, 0, 1) * (1 - abat)
        alpha = np.maximum(np.maximum(aw, abat), rim * 0.0)
        c.H[:] = hh
        c.A[:] = np.asarray((0.16, 0.16, 0.17), np.float32) * (1 + 0.2 * noise)[..., None]   # black-oxide nut
        c.spec[:] = 0.6
        c.gloss[:] = 40
        c.paint((slice(None), slice(None)), aw * (1 - an), (0.45, 0.45, 0.45), spec=0.5, gloss=30)
        c.paint((slice(None), slice(None)), rim, (0.02, 0.02, 0.02), spec=0.0)
        c.paint((slice(None), slice(None)), abat, metal * 0.85, spec=1.0, gloss=70)
        env = np.exp(-((pax + 0.7) / 0.8) ** 2) * abat
        c.emit[:] += (env * 0.35)[..., None]
        return _down(_finish(c, alpha, ambient=0.32))

    if kind == "rocker":
        # illuminated rocker: smoked-red paddle that glows when ON, I/O printed
        d = C.sdf_rrect(u, v, 0.3, 0.3, w - 0.6, h - 0.6, 1.8)
        alpha = np.clip(0.5 - d * Se / 1.5, 0, 1)
        di = C.sdf_rrect(u, v, 2.4, 2.4, w - 4.8, h - 4.8, 1.0)
        ai = np.clip(0.5 - di * Se / 1.0, 0, 1)
        frame_h = 1.6 * C.smoothstep(0, 1.0, -d)
        tq = (v - h / 2) / (h / 2 - 2.4)
        tilt = {"up": -1.0, "on": 1.0, "down": 0.0}[state]
        paddle = 2.4 + 1.6 * (-tq * tilt) - 0.4 * (1 - (2 * (u - w / 2) / (w - 4.8)) ** 2).clip(0, 1)
        c.H[:] = frame_h * (1 - ai) + ai * paddle
        c.A[:] = np.asarray((0.05, 0.05, 0.05), np.float32) * (1 + 0.3 * noise)[..., None]
        c.spec[:] = 0.4
        c.gloss[:] = 30
        red = np.asarray((0.30, 0.03, 0.02), np.float32)
        c.paint((slice(None), slice(None)), ai, red * (1 + 0.1 * noise)[..., None], spec=0.6, gloss=50)
        m = Image.new("L", (Wp, Hp), 0)
        C.draw_text_mask(m, "I", Wp / 2, Hp * 0.31, "semibold", 9 * Se)
        C.draw_text_mask(m, "O", Wp / 2, Hp * 0.69, "semibold", 8 * Se)
        ma = np.asarray(m, np.float32) / 255.0 * ai
        if state == "on":
            hot = np.exp(-(((u - w / 2) / (w * 0.5)) ** 2 + ((v - h * 0.45) / (h * 0.45)) ** 2))
            glow = ai * (0.55 + 0.45 * hot)
            c.emit[:] += glow[..., None] * np.asarray((1.0, 0.16, 0.05), np.float32)
            c.paint((slice(None), slice(None)), ma * 0.9, (0.25, 0.02, 0.0))
            c.emit[:] *= (1 - 0.75 * ma)[..., None]
        else:
            c.paint((slice(None), slice(None)), ma * 0.85, (0.85, 0.82, 0.8), spec=0.2)
        return _down(_finish(c, alpha))
    raise ValueError(kind)


# ----------------------------------------------------------------------------
# LEDs / needle
# ----------------------------------------------------------------------------
def led(st: dict, lw: float, lh: float, S: int, on: bool):
    """Canvas = 2.5x the lens rect, lens centred; 'on' includes bloom."""
    Se = S * SS
    cw, ch = lw * 2.5, lh * 2.5
    Wp, Hp = max(2, int(round(cw * S))) * SS, max(2, int(round(ch * S))) * SS
    X, Y = C.grid(Hp, Wp)
    u, v = X / Se, Y / Se
    x0, y0 = (cw - lw) / 2, (ch - lh) / 2
    rad = min(lw, lh) / 2 if st["shape"] == "round" else min(lw, lh) * 0.3
    d = C.sdf_rrect(u, v, x0, y0, lw, lh, rad)
    lens = np.clip(0.5 - d * Se / 1.0, 0, 1)
    col = np.asarray(st["color"], np.float32)
    # lens: domed, glossy
    nx = (u - cw / 2) / (lw / 2)
    ny = (v - ch / 2) / (lh / 2)
    glint = np.exp(-(((nx + 0.35) / 0.35) ** 2 + ((ny + 0.45) / 0.35) ** 2)) * lens
    rgba = np.zeros((Hp, Wp, 4), np.float32)
    if on:
        core = np.exp(-((nx / 0.9) ** 2 + (ny / 0.9) ** 2))
        lit = col * (0.75 + 0.25 * core[..., None]) + (1 - col) * (0.55 * core ** 2)[..., None]
        # bloom: radial falloff that reaches zero before the canvas edge
        ex = np.clip(1 - np.abs(u - cw / 2) / (cw / 2), 0, 1)
        ey = np.clip(1 - np.abs(v - ch / 2) / (ch / 2), 0, 1)
        edge = np.minimum(ex / 0.6, 1) * np.minimum(ey / 0.6, 1)
        glow_d = np.maximum(d, 0)
        glow = np.exp(-glow_d / (0.5 * min(lw, lh))) * 0.8 * edge ** 1.5
        rgba[..., :3] = lit * lens[..., None] + col * (1 - lens[..., None])
        rgba[..., :3] += glint[..., None] * 0.4
        rgba[..., 3] = np.maximum(lens, glow * (1 - lens))
    else:
        dark = col * 0.22 + 0.02
        shade = 0.7 + 0.3 * np.clip(-(nx + ny) * 0.5 + 0.5, 0, 1)
        rgba[..., :3] = dark * shade[..., None] + glint[..., None] * 0.55
        rim = np.clip(0.5 - (d + 0.0) * Se / 1.6, 0, 1) - lens
        rgba[..., 3] = np.clip(lens + rim * 0.6, 0, 1)
    return _down(np.clip(rgba, 0, 1))


def vu_needle(S: int, w=8.0, h=56.0):
    """Needle pointing straight up; pivot at the bottom centre of the image."""
    Se = S * SS
    Wp, Hp = int(round(w * S)) * SS, int(round(h * S)) * SS
    X, Y = C.grid(Hp, Wp)
    u, v = X / Se, Y / Se
    cx = w / 2
    t = v / h                      # 0 top .. 1 pivot
    half = 0.35 + 0.45 * t ** 2    # tapered
    d = np.abs(u - cx) - half
    a = np.clip(0.5 - d * Se / 1.0, 0, 1) * (v > 1.0)
    # counter-paddle at the pivot end
    pad = (np.abs(u - cx) < 1.6) & (v > h - 7)
    a = np.maximum(a, pad.astype(np.float32) * np.clip((v - (h - 7)) / 1.0, 0, 1))
    rgba = np.zeros((Hp, Wp, 4), np.float32)
    rgba[..., :3] = np.asarray((0.06, 0.05, 0.05), np.float32)
    # red tip
    tip = (v < h * 0.22)
    rgba[..., 0] = np.where(tip, 0.6, rgba[..., 0])
    rgba[..., 3] = a
    return _down(rgba)
