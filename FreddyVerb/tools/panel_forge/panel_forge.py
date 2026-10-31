#!/usr/bin/env python3
"""panel_forge - offline renderer for Freddy Verb's five photoreal rack skins.

Procedural pipeline (no Blender): numpy height fields + materials lit by one
fixed top-left key light, wear from fixed seeds. Writes, per skin, into
Resources/skins/<folder>/:
    layout.json, panel@{1,2,3}x.jpg, overlay@Nx.png, <knob art>@Nx.png (128-frame
    filmstrips), <button art>_{up,down,on}@Nx.png, led_{on,off}@Nx.png,
    vu_needle@Nx.png (broadcast).

usage:  python3 tools/panel_forge/panel_forge.py --all
        python3 tools/panel_forge/panel_forge.py --skin reverb84 [--only panel|parts|layout]
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time
from concurrent.futures import ProcessPoolExecutor

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import pf_core as C  # noqa: E402
import pf_parts as PT  # noqa: E402
from pf_layout import Layout  # noqa: E402
from pf_panel import PanelRenderer  # noqa: E402
from pf_skins import SKINS  # noqa: E402

MANIFEST = os.path.join(HERE, "controls_manifest.json")
OUT_ROOT = os.path.join(C.REPO, "Resources", "skins")
MASTER = 3  # everything is rendered at 3x and downsampled


def log(skin, msg):
    print(f"[{skin}] {msg}", flush=True)


def save_scaled(img: Image.Image, base: str, out_dir: str, kind: str, logical_size=None, quantize=False):
    """img is the 3x master. Writes @3x, @2x, @1x."""
    W3, H3 = img.size
    for s in (3, 2, 1):
        if s == 3:
            im = img
        else:
            im = img.resize((W3 * s // 3, H3 * s // 3), Image.LANCZOS)
        path = os.path.join(out_dir, f"{base}@{s}x.{ 'jpg' if kind == 'jpg' else 'png'}")
        if kind == "jpg":
            im.convert("RGB").save(path, quality=90 if s < 3 else 88, optimize=True, progressive=True,
                                   subsampling=0 if s == 1 else 2)
        else:
            C.save_png(im, path, quantize=quantize)


def resize_strip(arr: np.ndarray, frame3: int, s: int) -> Image.Image:
    """Downscale a 3x filmstrip frame-by-frame (no bleeding across frames)."""
    img = PT.to_rgba_img(arr)
    if s == 3:
        return img
    fs = frame3 * s // 3
    n = img.height // frame3
    out = Image.new("RGBA", (fs, fs * n))
    for i in range(n):
        fr = img.crop((0, i * frame3, frame3, (i + 1) * frame3))
        # premultiplied resize to avoid dark fringes
        out.paste(fr.resize((fs, fs), Image.LANCZOS, reducing_gap=None), (0, i * fs))
    return out


def write_strip(arr, frame3, base, out_dir):
    for s in (3, 2, 1):
        im = resize_strip(arr, frame3, s)
        C.save_png(im, os.path.join(out_dir, f"{base}@{s}x.png"), quantize=True)


def write_rgba(arr, base, out_dir, quantize=True):
    img = PT.to_rgba_img(arr)
    W3, H3 = img.size
    for s in (3, 2, 1):
        im = img if s == 3 else img.resize((max(1, round(W3 * s / 3)), max(1, round(H3 * s / 3))), Image.LANCZOS)
        C.save_png(im, os.path.join(out_dir, f"{base}@{s}x.png"), quantize=quantize)


def build_layout(name):
    manifest = json.load(open(MANIFEST))
    return Layout(manifest, name, SKINS[name]["parts"]).build()


def render_parts(name, L, out_dir):
    sk = SKINS[name]
    t0 = time.time()
    arts = {}
    for c in L.controls.values():
        if c.get("art"):
            arts.setdefault(c["art"], (c["kind"], c["w"], c["h"], c.get("led")))
    p = sk["parts"]
    for art, (kind, w, h, led) in sorted(arts.items()):
        if kind in ("pad", "choicepad", "button"):
            # caps always show the lens (unlit) so pads with and without a live LED match
            led = p["pad_led"] if art == p["pad_art"] else p["btn_led"]
        if kind == "knob":
            st = sk["knob"]
            strip = PT.filmstrip(st, w, MASTER, seed=sk["seed"])
            write_strip(strip, int(round(w * MASTER)), art, out_dir)
            log(name, f"knob strip {art} ({w:g}px)")
        elif kind == "jog":
            st = sk["jog"]
            strip = PT.filmstrip(st, w, MASTER, jog=True, seed=sk["seed"])
            write_strip(strip, int(round(w * MASTER)), art, out_dir)
            log(name, f"jog strip {art} ({w:g}px)")
        else:
            st = dict(sk["button"])
            if art == "toggle":
                st = dict(kind="toggle", **sk.get("toggle", {}))
            elif art == "rocker":
                st = dict(kind="rocker")
            states = ["up", "down"]
            if st["kind"] in ("lamp", "toggle", "rocker"):
                states.append("on")
            for state in states:
                arr = PT.button(st, w, h, MASTER, state, led=led, lit=st.get("lit"))
                write_rgba(arr, f"{art}_{state}", out_dir)
            log(name, f"button {art} {w:g}x{h:g} {states}")
    # LEDs (lens size from the layout's LED rects; a default if the skin has none)
    led_rect = next((c["led"] for c in L.controls.values() if c.get("led")), [0, 0, 5, 5])
    for on in (False, True):
        write_rgba(PT.led(sk["led"], led_rect[2], led_rect[3], MASTER, on), "led_on" if on else "led_off", out_dir, quantize=False)
    if sk["parts"].get("vu"):
        h = L.controls["meter_in"]["h"] * 0.88
        write_rgba(PT.vu_needle(MASTER, 8.0, h), "vu_needle", out_dir, quantize=False)
    log(name, f"parts done in {time.time() - t0:.1f}s")


def render_panel(name, L, out_dir):
    t0 = time.time()
    pr = PanelRenderer(name, SKINS[name], L, MASTER)
    rgb = pr.render()
    img = C.to_img(rgb)
    save_scaled(img, "panel", out_dir, "jpg")
    log(name, f"panel done in {time.time() - t0:.1f}s")
    ov = pr.overlay()
    write_rgba(ov, "overlay", out_dir, quantize=True)
    log(name, "overlay done")


def run_skin(name, only=None):
    out_dir = os.path.join(OUT_ROOT, name)
    os.makedirs(out_dir, exist_ok=True)
    L = build_layout(name)
    L.write(os.path.join(out_dir, "layout.json"))
    log(name, f"layout.json: {len(L.controls)} controls")
    if only in (None, "parts"):
        render_parts(name, L, out_dir)
    if only in (None, "panel"):
        render_panel(name, L, out_dir)
    total = sum(os.path.getsize(os.path.join(out_dir, f)) for f in os.listdir(out_dir))
    log(name, f"total {total / 1e6:.2f} MB")
    return name


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--all", action="store_true", help="render all five skins")
    ap.add_argument("--skin", action="append", choices=sorted(SKINS), help="render one skin (repeatable)")
    ap.add_argument("--only", choices=["layout", "panel", "parts"], help="render only part of the assets")
    ap.add_argument("--jobs", type=int, default=2, help="skins rendered in parallel (each needs ~3 GB RAM)")
    a = ap.parse_args()
    names = sorted(SKINS) if a.all else (a.skin or [])
    if not names:
        ap.error("give --all or --skin <folder>")
    if a.jobs > 1 and len(names) > 1:
        with ProcessPoolExecutor(max_workers=a.jobs) as ex:
            for n in ex.map(run_skin, names, [a.only] * len(names)):
                pass
    else:
        for n in names:
            run_skin(n, a.only)


if __name__ == "__main__":
    main()
