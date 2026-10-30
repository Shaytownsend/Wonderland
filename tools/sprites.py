"""Sprite geometry shared by export_assets.py, make_placeholders.py and make_layout.py.

Every asset filename, frame count and pixel size is decided HERE, so the real renders and the
placeholders are interchangeable without touching C++ (the editor reads assets/layout.json).
All sizes are logical units (1800x830 registry space); files are rendered at SCALE (2x).
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REGISTRY = os.path.join(ROOT, "queen_blender", "tex2", "J.json")
ASSETS = os.path.join(ROOT, "assets")
SCALE = 2
KNOB_FRAMES = 128
PANEL = dict(file="panel@2x.png", w=1800, h=830)

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
            name = f"knob_{c['cap']}@2x.png"
            s = knob_size(c["r"])
            specs.setdefault(name, dict(kind="knob", frames=KNOB_FRAMES, w=s, h=s, src=c["id"], cap=c["cap"]))
        elif t == "slider":
            name = f"slidercap_{c['cap']}@2x.png"
            specs.setdefault(name, dict(kind="slidercap", frames=1, w=SLIDER_CAP[0], h=SLIDER_CAP[1], src=c["id"], cap=c["cap"]))
        elif t == "switch":
            n = len(c["positions"])
            name = f"switch_{n}@2x.png"
            specs.setdefault(name, dict(kind="switch", frames=n, w=SWITCH_W, h=c["h"] + 2 * SWITCH_PAD, src=c["id"]))
        elif t == "button":
            for state in ("up", "down"):
                specs[f"button_{c['id']}_{state}@2x.png"] = dict(kind="button", frames=1, w=c["w"] + 2 * BUTTON_PAD,
                                                                  h=c["h"] + 2 * BUTTON_PAD, src=c["id"], state=state,
                                                                  cap=c["cap"], label=c["label"])
        elif t == "led":
            s = 2 * c["r"] + 2 * LED_PAD
            for state in ("off", "on"):
                specs.setdefault(f"led_{state}@2x.png", dict(kind="led", frames=1, w=s, h=s, src=c["id"], state=state))
        elif t == "display":
            specs["digits@2x.png"] = dict(kind="digits", frames=10, w=c["w"], h=c["h"], src=c["id"])
        elif t == "wheel":
            specs.setdefault("wheel@2x.png", dict(kind="wheel", frames=1, w=WHEEL[0], h=WHEEL[1], src=c["id"]))
        elif t == "keys":
            for colour, (w, h) in (("white", WHITE_KEY), ("black", BLACK_KEY)):
                for state in ("up", "down"):
                    specs[f"key_{colour}_{state}@2x.png"] = dict(kind="key", frames=1, w=w, h=h, src=c["id"],
                                                                 colour=colour, state=state)
    return specs


def control_sprite(c):
    """The sprite(s) a control draws with, for layout.json."""
    t = c["t"]
    if t == "knob":
        return dict(strip=f"knob_{c['cap']}@2x.png", frames=KNOB_FRAMES, size=knob_size(c["r"]))
    if t == "slider":
        return dict(cap=f"slidercap_{c['cap']}@2x.png", capW=SLIDER_CAP[0], capH=SLIDER_CAP[1])
    if t == "switch":
        n = len(c["positions"])
        return dict(strip=f"switch_{n}@2x.png", frames=n, w=SWITCH_W, h=c["h"] + 2 * SWITCH_PAD)
    if t == "button":
        return dict(up=f"button_{c['id']}_up@2x.png", down=f"button_{c['id']}_down@2x.png",
                    w=c["w"] + 2 * BUTTON_PAD, h=c["h"] + 2 * BUTTON_PAD)
    if t == "led":
        s = 2 * c["r"] + 2 * LED_PAD
        return dict(off="led_off@2x.png", on="led_on@2x.png", w=s, h=s)
    if t == "display":
        return dict(strip="digits@2x.png", frames=10, w=c["w"], h=c["h"])
    if t == "wheel":
        return dict(sprite="wheel@2x.png", w=WHEEL[0], h=WHEEL[1])
    if t == "keys":
        return dict(white_up="key_white_up@2x.png", white_down="key_white_down@2x.png",
                    black_up="key_black_up@2x.png", black_down="key_black_down@2x.png",
                    whiteW=WHITE_KEY[0], whiteH=WHITE_KEY[1], blackW=BLACK_KEY[0], blackH=BLACK_KEY[1])
    return {}
