"""Sprite geometry shared by export_assets.py, make_placeholders.py and make_layout.py.

Every asset filename, frame count and pixel size is decided HERE, so the real renders and the
placeholders are interchangeable without touching C++ (the editor reads assets/layout.json).
All sizes are logical units (1800x830 registry space); files are rendered at SCALE (4x).
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REGISTRY = os.path.join(ROOT, "queen_blender", "tex2", "J.json")
ASSETS = os.path.join(ROOT, "assets")
SCALE = 4
KNOB_FRAMES = 128
PANEL = dict(file="panel@4x.png", w=1800, h=830)

SLIDER_CAP = (34.0, 42.0)  # cap 23x30 plus shadow
BUTTON_PAD = 6.0
LED_PAD = 5.0
SWITCH_W = 30.0
SWITCH_PAD = 14.0
WHITE_KEY = (43.33, 242.0)
BLACK_KEY = (31.0, 154.0)
WHEEL = (40.0, 216.0)


def load_registry():
    with open(REGISTRY) as f:
        return json.load(f)


def px(v):
    return int(round(v * SCALE))


def knob_size(r):
    return 2.6 * r


def sprite_specs(reg):
    """Returns {filename: dict(kind, frames, w, h (logical per frame), source control id)}."""
    specs = {}
    controls = reg["controls"]
    for c in controls:
        t = c["t"]
        if t == "knob" and c["id"] != "tune":
            name = f"knob_{c['cap']}@4x.png"
            s = knob_size(c["r"])
            specs.setdefault(name, dict(kind="knob", frames=KNOB_FRAMES, w=s, h=s, src=c["id"], cap=c["cap"]))
        elif t == "slider":
            name = f"slidercap_{c['cap']}@4x.png"
            specs.setdefault(name, dict(kind="slidercap", frames=1, w=SLIDER_CAP[0], h=SLIDER_CAP[1], src=c["id"], cap=c["cap"]))
        elif t == "switch":
            n = len(c["positions"])
            name = f"switch_{n}@4x.png"
            specs.setdefault(name, dict(kind="switch", frames=n, w=SWITCH_W, h=c["h"] + 2 * SWITCH_PAD, src=c["id"]))
        elif t == "button":
            for state in ("up", "down"):
                specs[f"button_{c['id']}_{state}@4x.png"] = dict(kind="button", frames=1, w=c["w"] + 2 * BUTTON_PAD,
                                                                  h=c["h"] + 2 * BUTTON_PAD, src=c["id"], state=state,
                                                                  cap=c["cap"], label=c["label"])
        elif t == "led":
            s = 2 * c["r"] + 2 * LED_PAD
            for state in ("off", "on"):
                specs.setdefault(f"led_{state}@4x.png", dict(kind="led", frames=1, w=s, h=s, src=c["id"], state=state))
        elif t == "display":
            specs["digits@4x.png"] = dict(kind="digits", frames=10, w=c["w"], h=c["h"], src=c["id"])
        elif t == "wheel":
            specs.setdefault("wheel@4x.png", dict(kind="wheel", frames=1, w=WHEEL[0], h=WHEEL[1], src=c["id"]))
        elif t == "keys":
            for colour, (w, h) in (("white", WHITE_KEY), ("black", BLACK_KEY)):
                for state in ("up", "down"):
                    specs[f"key_{colour}_{state}@4x.png"] = dict(kind="key", frames=1, w=w, h=h, src=c["id"],
                                                                 colour=colour, state=state)
    return specs


def control_sprite(c):
    """The sprite(s) a control draws with, for layout.json."""
    t = c["t"]
    if t == "knob":
        return dict(strip=f"knob_{c['cap']}@4x.png", frames=KNOB_FRAMES, size=knob_size(c["r"]))
    if t == "slider":
        return dict(cap=f"slidercap_{c['cap']}@4x.png", capW=SLIDER_CAP[0], capH=SLIDER_CAP[1])
    if t == "switch":
        n = len(c["positions"])
        return dict(strip=f"switch_{n}@4x.png", frames=n, w=SWITCH_W, h=c["h"] + 2 * SWITCH_PAD)
    if t == "button":
        return dict(up=f"button_{c['id']}_up@4x.png", down=f"button_{c['id']}_down@4x.png",
                    w=c["w"] + 2 * BUTTON_PAD, h=c["h"] + 2 * BUTTON_PAD)
    if t == "led":
        s = 2 * c["r"] + 2 * LED_PAD
        return dict(off="led_off@4x.png", on="led_on@4x.png", w=s, h=s)
    if t == "display":
        return dict(strip="digits@4x.png", frames=10, w=c["w"], h=c["h"])
    if t == "wheel":
        return dict(sprite="wheel@4x.png", w=WHEEL[0], h=WHEEL[1])
    if t == "keys":
        return dict(white_up="key_white_up@4x.png", white_down="key_white_down@4x.png",
                    black_up="key_black_up@4x.png", black_down="key_black_down@4x.png",
                    whiteW=WHITE_KEY[0], whiteH=WHITE_KEY[1], blackW=BLACK_KEY[0], blackH=BLACK_KEY[1])
    return {}


# ------------------------------------------------------------------ seven-segment display art
SEGMENTS = {"0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc", "5": "afgcd", "6": "afgedc",
            "7": "abc", "8": "abcdefg", "9": "abcdfg", " ": ""}


def seven_seg(text, size=(504, 360)):
    """Two-cell red LED display in the style of textures/J_disp.png (unlit segments stay faintly visible)."""
    from PIL import Image, ImageChops, ImageDraw, ImageFilter

    sx, sy = size[0] / 504.0, size[1] / 360.0
    img = Image.new("RGB", size, (0, 0, 0))
    glow = Image.new("RGB", size, (0, 0, 0))
    d, g = ImageDraw.Draw(img), ImageDraw.Draw(glow)
    t = 18
    for cell, ch in enumerate(text[:2]):
        x0 = 70 + cell * 200
        x1, y0, y1 = x0 + 160, 60, 300
        ym = (y0 + y1) // 2
        segs = {"a": (x0 + t, y0, x1 - t, y0 + t), "d": (x0 + t, y1 - t, x1 - t, y1),
                "g": (x0 + t, ym - t // 2, x1 - t, ym + t // 2), "f": (x0, y0 + t, x0 + t, ym - t // 2),
                "b": (x1 - t, y0 + t, x1, ym - t // 2), "e": (x0, ym + t // 2, x0 + t, y1 - t),
                "c": (x1 - t, ym + t // 2, x1, y1 - t)}
        for s, (a, b, c, e) in segs.items():
            box = (a * sx, b * sy, c * sx, e * sy)
            lit = s in SEGMENTS.get(ch, "")
            d.rounded_rectangle(box, radius=6 * sx, fill=(255, 48, 24) if lit else (44, 8, 6))
            if lit:
                g.rounded_rectangle(box, radius=6 * sx, fill=(255, 40, 16))
    glow = glow.filter(ImageFilter.GaussianBlur(14 * sx))
    return ImageChops.add(img, glow)
