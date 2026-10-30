"""Headless Blender render of every GUI asset from queen_blender/THE_QUEEN.blend.

Run:  .venv/bin/python tools/export_assets.py [--samples 24] [--only knob,led,...] [--out assets]
Prints one line per asset written. Geometry/filenames come from tools/sprites.py.

Method: an orthographic top-down camera maps registry space exactly (x = X*100+900, y = 415-Y*100),
so every sprite lands pixel-aligned on panel@2x.png. Sprites render with film_transparent and every
static object turned into a shadow catcher, so contact shadows come along in the alpha.
"""
import argparse
import math
import os
import sys
import tempfile
import time

import bpy
from mathutils import Matrix, Vector
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sprites  # noqa: E402

BLEND = os.path.join(sprites.ROOT, "queen_blender", "THE_QUEEN.blend")
TMP = tempfile.mkdtemp(prefix="queen_export_")


# ------------------------------------------------------------------ scene helpers
def W(x, y):
    return (x - 900.0) / 100.0, -(y - 415.0) / 100.0


def logical(o):
    p = o.matrix_world.translation
    return p.x * 100 + 900, 415 - p.y * 100


def mat(o):
    m = getattr(o, "active_material", None)
    return m.name.split(".")[0] if m else ""


def mesh(o):
    return o.data.name.split(".")[0] if o.type == "MESH" else o.type


def near(o, x, y, rx, ry):
    lx, ly = logical(o)
    return abs(lx - x) <= rx and abs(ly - y) <= ry


def parts_for(c):
    """Scene objects that make up control c (moving parts only; bezels/slots/rings stay in the background)."""
    x, y, t = c["x"], c["y"], c["t"]
    objs = [o for o in bpy.data.objects if o.type in ("MESH", "FONT")]
    if t == "knob":
        r = c["r"] + 1
        return [o for o in objs if near(o, x, y, r, r) and
                (mat(o) in ("cap", "grip", "ind") or (mat(o) == "chrome" and mesh(o) == "Cylinder"))]
    if t == "slider":
        return [o for o in objs if near(o, x, y, 4, 70) and mat(o) in ("scap", "sc_l", "sc_g")]
    if t == "switch":
        return [o for o in objs if near(o, x, y, 4, c["h"] / 2 + 4) and
                (mat(o) == "sw_g" or (mat(o) == "chrome" and mesh(o) == "Cube"))]
    if t == "button":
        return [o for o in objs if near(o, x, y, 3, 3) and (mat(o) == "bcap" or (o.type == "FONT" and mat(o) == "blg"))]
    if t == "led":
        return [o for o in objs if near(o, x, y, 2, 2) and mat(o) == "led"]
    if t == "wheel":
        return [o for o in objs if near(o, x, y, 3, 3) and mat(o) == "wheel"]
    if t == "keys":
        return [o for o in objs if mat(o) in ("white", "bk")]
    return []


def all_control_parts(reg):
    s = set()
    for c in reg["controls"]:
        if c["t"] != "display":
            s.update(o.name for o in parts_for(c))
    return s


class Pivot:
    """Temporarily parents objects to an empty so they can be rotated/translated as a group."""

    def __init__(self, objs, location):
        self.objs = objs
        self.empty = bpy.data.objects.new("pivot", None)
        bpy.context.scene.collection.objects.link(self.empty)
        self.empty.location = location
        bpy.context.view_layer.update()
        self.saved = {o.name: (o.parent, o.matrix_world.copy()) for o in objs}
        for o in objs:
            mw = o.matrix_world.copy()
            o.parent = self.empty
            o.matrix_parent_inverse = self.empty.matrix_world.inverted()
            o.matrix_world = mw

    def restore(self):
        for o in self.objs:
            parent, mw = self.saved[o.name]
            o.parent = parent
            o.matrix_world = mw
        bpy.data.objects.remove(self.empty)
        bpy.context.view_layer.update()


# ------------------------------------------------------------------ render setup
def setup(samples):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = samples
    sc.cycles.use_adaptive_sampling = True
    sc.render.use_persistent_data = True
    try:
        sc.cycles.use_denoising = True
        sc.cycles.denoiser = "OPENIMAGEDENOISE"
    except Exception as e:  # pragma: no cover
        print("denoiser unavailable:", e)
    device = "CPU"
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        for backend in ("METAL", "OPTIX", "CUDA", "HIP", "ONEAPI"):
            try:
                prefs.compute_device_type = backend
                prefs.get_devices()
                if any(d.type == backend for d in prefs.devices):
                    for d in prefs.devices:
                        d.use = d.type == backend
                    sc.cycles.device = "GPU"
                    device = "GPU/" + backend
                    break
            except Exception:
                continue
    except Exception as e:
        print("GPU setup failed, using CPU:", e)
    if device == "CPU":
        sc.cycles.device = "CPU"
    print("render device:", device, "samples:", samples)

    cam_data = bpy.data.cameras.new("export_cam")
    cam_data.type = "ORTHO"
    cam_data.sensor_fit = "AUTO"
    cam = bpy.data.objects.new("export_cam", cam_data)
    sc.collection.objects.link(cam)
    cam.rotation_euler = (0, 0, 0)
    sc.camera = cam
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.render.image_settings.color_depth = "8"
    return cam


def frame_region(cam, x0, y0, x1, y1):
    sc = bpy.context.scene
    wpx, hpx = sprites.px(x1 - x0), sprites.px(y1 - y0)
    sc.render.resolution_x, sc.render.resolution_y = wpx, hpx
    # ortho_scale spans the larger pixel dimension
    cam.data.ortho_scale = max(x1 - x0, y1 - y0) / 100.0 * (max(wpx, hpx) / max(wpx, hpx))
    if wpx >= hpx:
        cam.data.ortho_scale = (x1 - x0) / 100.0
    else:
        cam.data.ortho_scale = (y1 - y0) / 100.0
    cx, cy = W((x0 + x1) / 2, (y0 + y1) / 2)
    cam.location = (cx, cy, 30.0)
    cam.data.clip_start, cam.data.clip_end = 1.0, 60.0


def render_to(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    return Image.open(path).convert("RGBA")


def set_visibility(visible_names, hidden_names, catcher_mode):
    for o in bpy.data.objects:
        if o.type not in ("MESH", "FONT", "CURVE"):
            continue
        o.hide_render = o.name in hidden_names and o.name not in visible_names
        o.is_shadow_catcher = catcher_mode and o.name not in visible_names
    bpy.context.scene.render.film_transparent = catcher_mode


def save_strip(frames, name, out_dir):
    w, h = frames[0].size
    strip = Image.new("RGBA", (w, h * len(frames)))
    for i, f in enumerate(frames):
        strip.paste(f, (0, i * h))
    path = os.path.join(out_dir, name)
    strip.save(path, optimize=True)
    print(f"wrote {name} {strip.size[0]}x{strip.size[1]} ({len(frames)} frames)", flush=True)


# ------------------------------------------------------------------ seven-segment display texture
SEGS = {"0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc", "5": "afgcd", "6": "afgedc",
        "7": "abc", "8": "abcdefg", "9": "abcdfg", " ": ""}


def seven_seg_texture(text, path, size=(504, 360)):
    w, h = size
    img = Image.new("RGB", size, (0, 0, 0))
    glow = Image.new("RGB", size, (0, 0, 0))
    d, g = ImageDraw.Draw(img), ImageDraw.Draw(glow)
    t = 18
    for cell, ch in enumerate(text):
        x0 = 70 + cell * 200
        x1, y0, y1 = x0 + 160, 60, 300
        ym = (y0 + y1) // 2
        segs = {"a": (x0 + t, y0, x1 - t, y0 + t), "d": (x0 + t, y1 - t, x1 - t, y1), "g": (x0 + t, ym - t // 2, x1 - t, ym + t // 2),
                "f": (x0, y0 + t, x0 + t, ym - t // 2), "b": (x1 - t, y0 + t, x1, ym - t // 2),
                "e": (x0, ym + t // 2, x0 + t, y1 - t), "c": (x1 - t, ym + t // 2, x1, y1 - t)}
        for s, box in segs.items():
            lit = s in SEGS.get(ch, "")
            d.rounded_rectangle(box, radius=6, fill=(255, 48, 24) if lit else (44, 8, 6))
            if lit:
                g.rounded_rectangle(box, radius=6, fill=(255, 40, 16))
    glow = glow.filter(ImageFilter.GaussianBlur(14))
    out = Image.blend(img, glow, 0.0)
    out = Image.eval(Image.merge("RGB", [Image.eval(a, lambda v: v) for a in out.split()]), lambda v: v)
    from PIL import ImageChops
    out = ImageChops.add(img, glow)
    out.save(path)
    return path


def set_display(text):
    m = bpy.data.materials["disp"]
    node = [n for n in m.node_tree.nodes if n.type == "TEX_IMAGE"][0]
    path = seven_seg_texture(text, os.path.join(TMP, f"disp_{text.strip() or 'blank'}.png"))
    img = bpy.data.images.load(path, check_existing=False)
    node.image = img


# ------------------------------------------------------------------ asset passes
def do_background(cam, reg, out_dir):
    hidden = all_control_parts(reg)
    set_visibility(set(), hidden, catcher_mode=False)
    set_display("88" if False else "  ")
    frame_region(cam, 0, 0, sprites.PANEL["w"], sprites.PANEL["h"])
    img = render_to(os.path.join(TMP, "panel.png")).convert("RGB")
    img.save(os.path.join(out_dir, sprites.PANEL["file"]), optimize=True)
    print(f"wrote {sprites.PANEL['file']} {img.size[0]}x{img.size[1]}", flush=True)


def isolate(reg, objs):
    hidden = all_control_parts(reg)
    set_visibility({o.name for o in objs}, hidden, catcher_mode=True)


def indicator_angle(c, parts):
    ind = [o for o in parts if mat(o) == "ind"]
    if not ind:
        return 0.0
    ix, iy = logical(ind[0])
    return math.degrees(math.atan2(ix - c["x"], -(iy - c["y"])))  # clockwise from 12 o'clock


def do_knob(cam, reg, name, spec, out_dir):
    c = next(x for x in reg["controls"] if x["id"] == spec["src"])
    parts = parts_for(c)
    isolate(reg, parts)
    s = spec["w"]
    frame_region(cam, c["x"] - s / 2, c["y"] - s / 2, c["x"] + s / 2, c["y"] + s / 2)
    cx, cy = W(c["x"], c["y"])
    current = indicator_angle(c, parts)
    pivot = Pivot(parts, (cx, cy, 0.9))
    frames = []
    try:
        for i in range(spec["frames"]):
            target = -135.0 + 270.0 * i / (spec["frames"] - 1)
            pivot.empty.rotation_euler = (0, 0, math.radians(-(target - current)))
            bpy.context.view_layer.update()
            frames.append(render_to(os.path.join(TMP, f"{name}_{i}.png")))
    finally:
        pivot.restore()
    save_strip(frames, name, out_dir)


def do_slidercap(cam, reg, name, spec, out_dir):
    c = next(x for x in reg["controls"] if x["id"] == spec["src"])
    parts = parts_for(c)
    isolate(reg, parts)
    cap = next(o for o in parts if mat(o) == "scap")
    _, capy = logical(cap)
    w, h = spec["w"], spec["h"]
    frame_region(cam, c["x"] - w / 2, capy - h / 2, c["x"] + w / 2, capy + h / 2)
    save_strip([render_to(os.path.join(TMP, name))], name, out_dir)


def do_switch(cam, reg, name, spec, out_dir):
    c = next(x for x in reg["controls"] if x["id"] == spec["src"])
    parts = parts_for(c)
    isolate(reg, parts)
    w, h = spec["w"], spec["h"]
    frame_region(cam, c["x"] - w / 2, c["y"] - h / 2, c["x"] + w / 2, c["y"] + h / 2)
    lever = next(o for o in parts if mat(o) == "chrome")
    _, ly = logical(lever)
    n = spec["frames"]
    travel = c["h"] - 20.0  # 52 -> 32 (+-16), 36 -> 16 (+-8)
    pivot = Pivot(parts, (W(c["x"], ly)[0], W(c["x"], ly)[1], 0.9))
    frames = []
    try:
        for k in range(n):
            ty = c["y"] - travel / 2 + travel * k / (n - 1)
            pivot.empty.location.y = W(c["x"], ty)[1]
            bpy.context.view_layer.update()
            frames.append(render_to(os.path.join(TMP, f"{name}_{k}.png")))
    finally:
        pivot.restore()
    save_strip(frames, name, out_dir)


def do_button(cam, reg, name, spec, out_dir):
    c = next(x for x in reg["controls"] if x["id"] == spec["src"])
    parts = parts_for(c)
    isolate(reg, parts)
    w, h = spec["w"], spec["h"]
    frame_region(cam, c["x"] - w / 2, c["y"] - h / 2, c["x"] + w / 2, c["y"] + h / 2)
    saved = [(o, o.location.z) for o in parts]
    if spec["state"] == "down":
        for o in parts:
            o.location.z -= 0.04
    try:
        img = render_to(os.path.join(TMP, name))
    finally:
        for o, z in saved:
            o.location.z = z
    save_strip([img], name, out_dir)


def do_led(cam, reg, name, spec, out_dir):
    leds = [x for x in reg["controls"] if x["t"] == "led"]
    want_on = spec["state"] == "on"
    c = next(x for x in leds if bool(x["v"]) == want_on)
    parts = parts_for(c)
    isolate(reg, parts)
    s = spec["w"]
    frame_region(cam, c["x"] - s / 2, c["y"] - s / 2, c["x"] + s / 2, c["y"] + s / 2)
    save_strip([render_to(os.path.join(TMP, name))], name, out_dir)


def do_digits(cam, reg, name, spec, out_dir):
    c = next(x for x in reg["controls"] if x["t"] == "display")
    hidden = all_control_parts(reg)
    set_visibility(set(), hidden, catcher_mode=False)
    w, h = spec["w"], spec["h"]
    frame_region(cam, c["x"] - w / 2, c["y"] - h / 2, c["x"] + w / 2, c["y"] + h / 2)
    frames = []
    for d in range(10):
        set_display(f"{d}{d}")
        frames.append(render_to(os.path.join(TMP, f"{name}_{d}.png")))
    set_display("  ")
    save_strip(frames, name, out_dir)


def do_wheel(cam, reg, name, spec, out_dir):
    c = next(x for x in reg["controls"] if x["id"] == spec["src"])
    parts = parts_for(c)
    isolate(reg, parts)
    w, h = spec["w"], spec["h"]
    frame_region(cam, c["x"] - w / 2, c["y"] - h / 2, c["x"] + w / 2, c["y"] + h / 2)
    save_strip([render_to(os.path.join(TMP, name))], name, out_dir)


def do_key(cam, reg, name, spec, out_dir):
    keys = parts_for(next(x for x in reg["controls"] if x["t"] == "keys"))
    m = "white" if spec["colour"] == "white" else "bk"
    key = min((o for o in keys if mat(o) == m), key=lambda o: logical(o)[0])
    isolate(reg, [key])
    kx, _ = logical(key)
    w, h = spec["w"], spec["h"]
    y0 = 572.0 - 3.0
    frame_region(cam, kx - w / 2, y0, kx + w / 2, y0 + h)
    top = max((key.matrix_world @ v.co).z for v in key.data.vertices)
    pivot = Pivot([key], (W(kx, 572.0)[0], W(kx, 572.0)[1], top))
    try:
        if spec["state"] == "down":
            pivot.empty.rotation_euler = (math.radians(2.5 if m == "white" else 3.5), 0, 0)
            bpy.context.view_layer.update()
        img = render_to(os.path.join(TMP, name))
    finally:
        pivot.restore()
    save_strip([img], name, out_dir)


PASSES = {"knob": do_knob, "slidercap": do_slidercap, "switch": do_switch, "button": do_button, "led": do_led,
          "digits": do_digits, "wheel": do_wheel, "key": do_key}


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--samples", type=int, default=24)
    ap.add_argument("--only", default="", help="comma list of kinds: background,knob,slidercap,switch,button,led,digits,wheel,key")
    ap.add_argument("--out", default=sprites.ASSETS)
    args = ap.parse_args(argv)
    only = set(filter(None, args.only.split(",")))
    os.makedirs(args.out, exist_ok=True)

    t0 = time.time()
    bpy.ops.wm.open_mainfile(filepath=BLEND)
    reg = sprites.load_registry()
    cam = setup(args.samples)

    if not only or "background" in only:
        do_background(cam, reg, args.out)
    for name, spec in sprites.sprite_specs(reg).items():
        if only and spec["kind"] not in only:
            continue
        PASSES[spec["kind"]](cam, reg, name, spec, args.out)
    print(f"done in {time.time() - t0:.0f} s", flush=True)


if __name__ == "__main__":
    main()
