"""Procedural handwriting: single-stroke glyph skeletons drawn with a jittered,
pressure-varying pen (marker / ballpoint / pencil). Deterministic per seed.

Glyph coordinates: x right, y down, cap height 0..6 (baseline at 6); lowercase
x-height starts at ~2.4.
"""
from __future__ import annotations

import math

import numpy as np
from PIL import Image, ImageDraw


def _ellipse(cx, cy, rx, ry, a0=0.0, a1=360.0, n=14):
    return [(cx + rx * math.cos(math.radians(a)), cy + ry * math.sin(math.radians(a)))
            for a in np.linspace(a0, a1, n)]


G = {
    "A": [[(0, 6), (2, 0), (4, 6)], [(0.9, 3.9), (3.1, 3.9)]],
    "B": [[(0, 6), (0, 0), (2.6, 0), (3.5, 0.8), (3.5, 2.2), (2.6, 3), (0, 3)],
          [(2.6, 3), (3.8, 3.8), (3.8, 5.2), (2.8, 6), (0, 6)]],
    "C": [[(3.8, 1), (3, 0.1), (1.5, 0), (0.4, 1), (0, 3), (0.4, 5), (1.5, 6), (3, 5.9), (3.8, 5)]],
    "D": [[(0, 0), (0, 6), (2, 6), (3.4, 5), (4, 3), (3.4, 1), (2, 0), (0, 0)]],
    "E": [[(3.6, 0), (0, 0), (0, 6), (3.6, 6)], [(0, 3), (2.8, 3)]],
    "F": [[(3.6, 0), (0, 0), (0, 6)], [(0, 3), (2.6, 3)]],
    "G": [[(3.8, 1), (3, 0.1), (1.5, 0), (0.4, 1), (0, 3), (0.4, 5), (1.5, 6), (3, 5.9), (3.8, 5), (3.8, 3.4), (2.2, 3.4)]],
    "H": [[(0, 0), (0, 6)], [(4, 0), (4, 6)], [(0, 3), (4, 3)]],
    "I": [[(0.6, 0), (0.6, 6)]],
    "K": [[(0, 0), (0, 6)], [(3.8, 0), (0, 3.6)], [(1.2, 2.6), (4, 6)]],
    "L": [[(0, 0), (0, 6), (3.4, 6)]],
    "M": [[(0, 6), (0, 0), (2, 4), (4, 0), (4, 6)]],
    "N": [[(0, 6), (0, 0), (4, 6), (4, 0)]],
    "O": [_ellipse(2, 3, 2, 3, -90, 275)],
    "P": [[(0, 6), (0, 0), (2.6, 0), (3.6, 0.9), (3.6, 2.2), (2.6, 3), (0, 3)]],
    "R": [[(0, 6), (0, 0), (2.6, 0), (3.6, 0.9), (3.6, 2.2), (2.6, 3), (0, 3)], [(1.6, 3), (3.8, 6)]],
    "S": [[(3.6, 0.8), (2.8, 0), (1.2, 0), (0.2, 0.9), (0.4, 2.3), (2, 3), (3.6, 3.8), (3.8, 5.2), (2.8, 6), (1, 6), (0, 5.2)]],
    "T": [[(0, 0), (4, 0)], [(2, 0), (2, 6)]],
    "U": [[(0, 0), (0, 4.5), (0.8, 5.8), (2, 6), (3.2, 5.8), (4, 4.5), (4, 0)]],
    "V": [[(0, 0), (2, 6), (4, 0)]],
    "W": [[(0, 0), (1.2, 6), (2.5, 2), (3.8, 6), (5, 0)]],
    "X": [[(0, 0), (4, 6)], [(4, 0), (0, 6)]],
    "Y": [[(0, 0), (2, 3), (4, 0)], [(2, 3), (2, 6)]],
    "0": [_ellipse(1.9, 3, 1.9, 3, -90, 275)],
    "1": [[(0.4, 1.2), (1.6, 0), (1.6, 6)]],
    "2": [[(0.2, 1.2), (1.2, 0.1), (2.8, 0.1), (3.6, 1.1), (3.4, 2.6), (0, 6), (3.8, 6)]],
    "3": [[(0.2, 0.8), (1.2, 0), (2.8, 0), (3.6, 0.8), (3.6, 2.2), (2.4, 3), (1.4, 3)],
          [(2.4, 3), (3.8, 3.9), (3.8, 5.2), (2.8, 6), (1.2, 6), (0, 5.2)]],
    "4": [[(3, 6), (3, 0), (0, 4.2), (4, 4.2)]],
    "5": [[(3.6, 0), (0.4, 0), (0.2, 2.8), (1.6, 2.4), (3, 2.6), (3.8, 3.8), (3.6, 5.3), (2.4, 6), (1, 6), (0, 5.2)]],
    "6": [[(3.4, 0.4), (2, 0), (0.8, 0.8), (0.1, 3), (0.3, 5.2), (1.8, 6), (3.2, 5.6), (3.8, 4.4), (3.2, 3.2), (1.8, 2.9), (0.5, 3.6)]],
    "7": [[(0, 0), (3.8, 0), (1.4, 6)]],
    "8": [[(2, 3), (0.6, 2.2), (0.6, 0.8), (2, 0), (3.4, 0.8), (3.4, 2.2), (2, 3), (0.3, 3.9), (0.3, 5.3), (2, 6), (3.7, 5.3), (3.7, 3.9), (2, 3)]],
    "9": [[(3.4, 2.6), (2.2, 3.1), (0.8, 2.8), (0.2, 1.5), (0.9, 0.2), (2.4, 0), (3.5, 0.9), (3.6, 3), (3, 5.2), (1.6, 6), (0.4, 5.6)]],
    "%": [[(0, 6), (4, 0)], _ellipse(0.8, 1.1, 0.8, 1.0, 0, 360, 10), _ellipse(3.2, 4.9, 0.8, 1.0, 0, 360, 10)],
    "!": [[(0.8, 0), (0.8, 4.2)], [(0.8, 5.7), (0.85, 5.9)]],
    ":": [[(0.5, 2.2), (0.55, 2.4)], [(0.5, 5.6), (0.55, 5.8)]],
    "-": [[(0, 3.3), (2.6, 3.1)]],
    "/": [[(0, 6), (2.6, 0)]],
    ".": [[(0.4, 5.7), (0.45, 5.9)]],
    "'": [[(0.5, 0), (0.3, 1.6)]],
    "c": [[(3, 3.2), (2, 2.4), (0.8, 2.8), (0.1, 4.2), (0.6, 5.6), (1.8, 6), (3, 5.5)]],
    "a": [[(3, 3), (2, 2.4), (0.7, 2.9), (0.1, 4.4), (0.7, 5.8), (1.9, 5.8), (3, 4.6)], [(3, 2.5), (3, 5.4), (3.4, 6)]],
    "l": [[(0.6, 0), (0.6, 5.4), (1.2, 6)]],
    "✓": [[(0, 3.6), (1.4, 6), (4.6, 0)]],
}
ADV = {" ": 2.4, "I": 1.4, "!": 1.6, ":": 1.2, ".": 1.0, "'": 1.0, "l": 1.6, "1": 2.2, "-": 3.0, "/": 3.0, "W": 5.2}


def _catmull(pts, n=6):
    if len(pts) < 3:
        out = []
        for i in range(len(pts) - 1):
            for t in np.linspace(0, 1, n * 2, endpoint=False):
                out.append((pts[i][0] + (pts[i + 1][0] - pts[i][0]) * t, pts[i][1] + (pts[i + 1][1] - pts[i][1]) * t))
        out.append(pts[-1])
        return out
    p = [pts[0]] + list(pts) + [pts[-1]]
    out = []
    for i in range(1, len(p) - 2):
        p0, p1, p2, p3 = p[i - 1], p[i], p[i + 1], p[i + 2]
        for t in np.linspace(0, 1, n, endpoint=False):
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[k]) + (-p0[k] + p2[k]) * t + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2
                                    + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3) for k in (0, 1)))
    out.append(pts[-1])
    return out


def write(text: str, cap_px: float, seed: int, pen: float = 1.0, slant: float = 0.2, jitter: float = 0.09,
          spacing: float = 1.15):
    """Returns an 'L' image with the handwritten text (ink = 255) and its baseline y."""
    r = np.random.default_rng(seed)
    u = cap_px / 6.0
    total = sum(ADV.get(ch, 4.2) + spacing for ch in text) * u + cap_px * 2
    W, H = int(total + cap_px), int(cap_px * 2.4)
    img = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(img)
    x = cap_px * 0.6
    base = cap_px * 1.7
    drift = 0.0
    for ch in text:
        adv = ADV.get(ch, 4.2)
        strokes = G.get(ch) or G.get(ch.upper())
        if strokes:
            sc = r.uniform(0.93, 1.07)
            rot = r.normal(0, 0.04)
            dy = r.normal(0, 0.12) + drift
            for s in strokes:
                pts = []
                for (gx, gy) in s:
                    gx += r.normal(0, jitter)
                    gy += r.normal(0, jitter)
                    gx2 = gx * math.cos(rot) - (gy - 3) * math.sin(rot)
                    gy2 = gx * math.sin(rot) + (gy - 3) * math.cos(rot) + 3
                    px = x + (gx2 + (6 - gy2) * slant) * u * sc
                    py = base - (6 - gy2 - dy) * u * sc
                    pts.append((px, py))
                dense0 = _catmull(pts, 7)
                step = max(0.4, pen * u * 0.12)
                dense = [dense0[0]]
                for (ax, ay), (bx, by) in zip(dense0, dense0[1:]):
                    k = max(1, int(math.hypot(bx - ax, by - ay) / step))
                    dense += [(ax + (bx - ax) * j / k, ay + (by - ay) * j / k) for j in range(1, k + 1)]
                n = len(dense)
                # pen pressure: tapered ends, slow random swell, heavier at the stroke start (ink pooling)
                ph = r.uniform(0, 6.28)
                for i, (px, py) in enumerate(dense):
                    t = i / max(1, n - 1)
                    taper = min(1.0, 0.45 + 2.2 * t, 0.45 + 2.6 * (1 - t))
                    press = 0.82 + 0.18 * math.sin(ph + t * 5.0) + (0.15 if t < 0.05 else 0)
                    rad = max(0.45, pen * u * 0.55 * taper * press)
                    a = int(min(255, 205 + 50 * press))
                    d.ellipse([px - rad, py - rad, px + rad, py + rad], fill=a)
        x += (adv + spacing) * u * r.uniform(0.95, 1.06)
        drift += r.normal(0, 0.05)
    bbox = img.getbbox() or (0, 0, W, H)
    return img.crop((max(0, bbox[0] - 4), 0, min(W, bbox[2] + 4), H)), base
