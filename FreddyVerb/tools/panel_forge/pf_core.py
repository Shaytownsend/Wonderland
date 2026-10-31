"""panel_forge core: deterministic noise, SDF shapes, height-field lighting, text masks.

Everything works on float32 numpy arrays in *pixel* space of the current render
scale S (logical px * S). Heights are expressed in logical px so the lighting
looks identical at every scale.
"""
from __future__ import annotations

import math
import os
from functools import lru_cache

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
FONT_DIR = os.path.join(REPO, "Resources", "fonts")
FONTS = {
    "semibold": os.path.join(FONT_DIR, "BarlowCondensed-SemiBold.ttf"),
    "medium": os.path.join(FONT_DIR, "BarlowCondensed-Medium.ttf"),
    "mono": os.path.join(FONT_DIR, "ShareTechMono-Regular.ttf"),
}

# key light: top-left, fairly high (studio ceiling light)
LIGHT = np.array([-0.42, -0.62, 0.66], np.float32)
LIGHT /= np.linalg.norm(LIGHT)
VIEW = np.array([0.0, 0.0, 1.0], np.float32)
HALF = LIGHT + VIEW
HALF /= np.linalg.norm(HALF)


# ----------------------------------------------------------------------------
# deterministic noise
# ----------------------------------------------------------------------------
def rng(seed: int) -> np.random.Generator:
    return np.random.default_rng(seed)


def value_noise(h: int, w: int, cell: float, seed: int) -> np.ndarray:
    """Smooth value noise in [0,1] with feature size ~cell px."""
    gh = max(2, int(math.ceil(h / cell)) + 3)
    gw = max(2, int(math.ceil(w / cell)) + 3)
    g = rng(seed).random((gh, gw)).astype(np.float32)
    out = ndimage.zoom(g, (cell, cell), order=3, mode="reflect", prefilter=False)
    return np.clip(out[:h, :w], 0, 1)


def fbm(h: int, w: int, cell: float, seed: int, octaves: int = 4, gain: float = 0.5) -> np.ndarray:
    acc = np.zeros((h, w), np.float32)
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        c = max(1.5, cell / (2 ** o))
        acc += amp * value_noise(h, w, c, seed * 31 + o)
        tot += amp
        amp *= gain
    return acc / tot


def white(h: int, w: int, seed: int) -> np.ndarray:
    return rng(seed).random((h, w)).astype(np.float32)


def brushed(h: int, w: int, S: float, seed: int, streak: float = 60.0) -> np.ndarray:
    """Horizontal brushed-metal grain in ~[-1,1]: fine per-row lines whose
    strength drifts slowly along x (long abrasive strokes)."""
    r = rng(seed)
    cols = max(8, w // 24)
    n = r.standard_normal((h, cols)).astype(np.float32)
    n = ndimage.gaussian_filter1d(n, streak / 24 * S / 2, axis=1)
    n = ndimage.zoom(n, (1, w / cols), order=1)
    n = n[:, :w] if n.shape[1] >= w else np.pad(n, ((0, 0), (0, w - n.shape[1])), mode="edge")
    n /= (np.std(n) + 1e-6)
    # a sprinkling of deeper single tool lines
    deep = (r.random((h, 1)) > 0.985).astype(np.float32) * r.uniform(0.8, 1.6, (h, 1)).astype(np.float32)
    out = n * 0.55 + deep * (0.6 + 0.4 * np.sin(np.linspace(0, r.uniform(2, 9), w, dtype=np.float32)))[None, :]
    return np.clip(out / 1.6, -1, 1)


# ----------------------------------------------------------------------------
# SDF primitives (pixel space). Return signed distance (negative inside).
# ----------------------------------------------------------------------------
def grid(h: int, w: int, x0: float = 0.0, y0: float = 0.0):
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    return xs + x0 + 0.5, ys + y0 + 0.5


def sdf_rrect(X, Y, x, y, w, h, r):
    cx, cy = x + w / 2, y + h / 2
    qx = np.abs(X - cx) - (w / 2 - r)
    qy = np.abs(Y - cy) - (h / 2 - r)
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r


def sdf_circle(X, Y, cx, cy, r):
    return np.hypot(X - cx, Y - cy) - r


def sdf_stadium(X, Y, cx, cy, half_len, r, vertical=False):
    if vertical:
        X, Y = Y, X
        cx, cy = cy, cx
    qx = np.maximum(np.abs(X - cx) - half_len, 0)
    return np.hypot(qx, Y - cy) - r


def cov(d, aa=0.75):
    """Anti-aliased coverage from a signed distance (px)."""
    return np.clip(0.5 - d / (2 * aa), 0, 1).astype(np.float32)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


# ----------------------------------------------------------------------------
# A layered material canvas with height-field lighting.
# ----------------------------------------------------------------------------
class Canvas:
    def __init__(self, w: int, h: int, S: float):
        self.w, self.h, self.S = w, h, S
        self.H = np.zeros((h, w), np.float32)        # height (logical px)
        self.A = np.zeros((h, w, 3), np.float32)     # albedo
        self.spec = np.zeros((h, w), np.float32)     # specular strength
        self.gloss = np.full((h, w), 20.0, np.float32)
        self.emit = np.zeros((h, w, 3), np.float32)  # emissive (added after lighting)
        self.aniso = np.zeros((h, w), np.float32)    # brushed highlight strength

    # window helper: returns slices + local grids for a pixel-space bbox
    def win(self, x0, y0, x1, y1, pad=2):
        xa = max(0, int(math.floor(x0)) - pad)
        ya = max(0, int(math.floor(y0)) - pad)
        xb = min(self.w, int(math.ceil(x1)) + pad)
        yb = min(self.h, int(math.ceil(y1)) + pad)
        if xb <= xa or yb <= ya:
            return None
        X, Y = grid(yb - ya, xb - xa, xa, ya)
        return (slice(ya, yb), slice(xa, xb)), X, Y

    def paint(self, sl, a, color=None, spec=None, gloss=None, emit=None, aniso=None):
        """Alpha-blend material properties inside window sl with coverage a."""
        if color is not None:
            c = np.asarray(color, np.float32)
            if c.ndim == 1:
                c = c[None, None, :]
            self.A[sl] = self.A[sl] * (1 - a[..., None]) + c * a[..., None]
        if spec is not None:
            self.spec[sl] = self.spec[sl] * (1 - a) + spec * a
        if gloss is not None:
            self.gloss[sl] = self.gloss[sl] * (1 - a) + gloss * a
        if aniso is not None:
            self.aniso[sl] = self.aniso[sl] * (1 - a) + aniso * a
        if emit is not None:
            e = np.asarray(emit, np.float32)
            if e.ndim == 1:
                e = e[None, None, :]
            self.emit[sl] = self.emit[sl] * (1 - a[..., None]) + e * a[..., None]


def normals(H: np.ndarray, S: float, blur: float = 0.6):
    Hs = ndimage.gaussian_filter(H, blur * S) if blur > 0 else H
    gy, gx = np.gradient(Hs)
    gx *= S
    gy *= S
    nz = 1.0 / np.sqrt(gx * gx + gy * gy + 1.0)
    return -gx * nz, -gy * nz, nz


def shift2d(a: np.ndarray, dy: int, dx: int) -> np.ndarray:
    """out[y, x] = a[y + dy, x + dx] with edge clamping."""
    h, w = a.shape[:2]
    py, px = abs(dy), abs(dx)
    p = np.pad(a, ((py, py), (px, px)) + ((0, 0),) * (a.ndim - 2), mode="edge")
    return p[py + dy:py + dy + h, px + dx:px + dx + w]


def cast_shadow(H: np.ndarray, S: float, steps: int = 10, reach: float = 7.0, soft: float = 1.4):
    """Soft cast shadows from the height field along the key light."""
    lx, ly, lz = LIGHT
    l2 = math.hypot(lx, ly)
    dx, dy = lx / l2, ly / l2
    tan_e = lz / l2
    occ = np.zeros_like(H)
    for i in range(1, steps + 1):
        d = reach * i / steps  # logical px
        ox, oy = int(round(dx * d * S)), int(round(dy * d * S))
        sh = shift2d(H, oy, ox)  # height toward the light
        occ = np.maximum(occ, (sh - H - d * tan_e) / 1.2)
    occ = np.clip(occ, 0, 1)
    return ndimage.gaussian_filter(occ, soft * S)


def ambient_occlusion(H: np.ndarray, S: float, radius: float = 4.0):
    blur = ndimage.gaussian_filter(H, radius * S)
    return np.clip((blur - H) * 0.35, 0, 1)


def shade(c: Canvas, ambient=0.34, diffuse=0.78, shadow_strength=0.75, ao_strength=0.6,
          light_tint=(1.0, 0.985, 0.96), aniso_noise=None):
    nx, ny, nz = normals(c.H, c.S)
    ndl = np.clip(nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2], 0, 1)
    ndh = np.clip(nx * HALF[0] + ny * HALF[1] + nz * HALF[2], 0, 1)
    sh = cast_shadow(c.H, c.S)
    ao = ambient_occlusion(c.H, c.S)
    vis = 1 - shadow_strength * sh
    amb = ambient * (1 - ao_strength * ao)
    lit = (amb + diffuse * ndl * vis)[..., None] * c.A * np.asarray(light_tint, np.float32)
    sp = c.spec * np.power(ndh, c.gloss) * vis
    # brushed metal: broad anisotropic sheen modulated by streak noise
    if aniso_noise is not None:
        sheen = c.aniso * (0.55 + 0.45 * ndl) * (0.5 + 0.5 * aniso_noise) * vis
        sp = sp + sheen * 0.35
    out = lit + sp[..., None] * np.asarray(light_tint, np.float32) + c.emit
    return np.clip(out, 0, 1)


# ----------------------------------------------------------------------------
# text
# ----------------------------------------------------------------------------
@lru_cache(maxsize=256)
def font(kind: str, px: float):
    return ImageFont.truetype(FONTS[kind], max(4, int(round(px))))


def text_size(s: str, kind: str, px: float, tracking: float = 0.0):
    f = font(kind, px)
    l, t, r, b = f.getbbox(s)
    return (r - l) + tracking * max(0, len(s) - 1), (b - t)


def draw_text_mask(mask: Image.Image, s: str, cx: float, cy: float, kind: str, px: float,
                   anchor: str = "mm", tracking: float = 0.0, fill: int = 255, rotate: float = 0.0):
    """Draw text into an 'L' mask (pixel space). anchor like PIL ('mm','lm','rm')."""
    f = font(kind, px)
    if tracking == 0.0 and rotate == 0.0:
        ImageDraw.Draw(mask).text((cx, cy), s, font=f, fill=fill, anchor=anchor)
        return
    # tracked / rotated: render to a temp image
    widths = [f.getlength(ch) for ch in s]
    tw = sum(widths) + tracking * max(0, len(s) - 1)
    asc, desc = f.getmetrics()
    th = asc + desc
    tmp = Image.new("L", (int(tw + 8), int(th + 8)), 0)
    d = ImageDraw.Draw(tmp)
    x = 4.0
    for ch, wch in zip(s, widths):
        d.text((x, 4 + asc), ch, font=f, fill=255, anchor="ls")
        x += wch + tracking
    # vertical centre on the cap-height middle
    l, t, r, b = f.getbbox("H")
    mid = 4 + asc - (b - t) / 2 - 0.0
    if rotate:
        tmp = tmp.rotate(rotate, resample=Image.BICUBIC, expand=True)
        ox = cx - tmp.width / 2
        oy = cy - tmp.height / 2
    else:
        if anchor[0] == "l":
            ox = cx - 4
        elif anchor[0] == "r":
            ox = cx - tw - 4
        else:
            ox = cx - tw / 2 - 4
        oy = cy - mid
    mask.paste(Image.new("L", tmp.size, fill), (int(round(ox)), int(round(oy))), tmp)


def to_img(arr: np.ndarray) -> Image.Image:
    a = np.clip(arr * 255 + 0.5, 0, 255).astype(np.uint8)
    return Image.fromarray(a)


def srgb(hexstr: str):
    hexstr = hexstr.lstrip("#")
    return np.array([int(hexstr[i:i + 2], 16) / 255.0 for i in (0, 2, 4)], np.float32)


def save_png(img: Image.Image, path: str, quantize: bool = False, colors: int = 256):
    if quantize:
        if img.mode == "RGBA":
            q = img.quantize(colors=colors, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.FLOYDSTEINBERG)
        else:
            q = img.convert("RGB").quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
        q.save(path, optimize=True)
    else:
        img.save(path, optimize=True)
