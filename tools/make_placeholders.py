"""Writes every asset filename the GUI needs as simple drawn PIL art (1x scale, 32-frame knobs).

Lets the GUI build and run before tools/export_assets.py has rendered the real thing. Placeholders are
deliberately low-res so tests/AssetsTest can tell them apart from the real renders.
Run: python3 tools/make_placeholders.py   (needs Pillow)
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sprites  # noqa: E402

S = 1  # placeholder scale (real assets are 2x)
KNOB_FRAMES = 32
COLOURS = {"cream": (210, 196, 156), "red": (160, 20, 16), "blue": (20, 40, 170), "black": (24, 24, 26),
           "orange": (190, 70, 10), "grey": (80, 82, 90), "ivory": (170, 164, 150), "yellow": (205, 150, 30),
           "white": (220, 220, 214), "silver": (176, 176, 168)}


def p(v):
    return max(1, int(round(v * S)))


def font(size):
    try:
        return ImageFont.truetype("DejaVuSans-Bold.ttf", size)
    except OSError:
        return ImageFont.load_default()


def save(img, name):
    img.save(os.path.join(sprites.ASSETS, name))
    print("wrote", name, f"{img.size[0]}x{img.size[1]}")


def panel():
    reg = sprites.load_registry()
    w, h = p(sprites.PANEL["w"]), p(sprites.PANEL["h"])
    img = Image.new("RGB", (w, h), (12, 12, 12))
    d = ImageDraw.Draw(img)
    d.rectangle((0, 0, p(120), h), fill=(110, 62, 38))
    d.rectangle((p(1680), 0, w, h), fill=(110, 62, 38))
    x0, y0, x1, y1 = reg["panel"]
    d.rectangle((p(x0), p(y0), p(x1), p(y1)), fill=(42, 42, 44))
    ink = os.path.join(sprites.ROOT, "queen_blender", "textures", "J_ink_panel.png")
    if os.path.exists(ink):
        layer = Image.open(ink).convert("RGBA").resize((p(x1 - x0), p(y1 - y0)))
        img.paste(layer, (p(x0), p(y0)), layer)
    d.rectangle((p(120), p(549.5 - 22), p(1680), p(572)), fill=(110, 62, 38))
    for c in reg["controls"]:
        if c["t"] == "knob":
            r = c["r"] * 1.3
            d.ellipse((p(c["x"] - r), p(c["y"] - r), p(c["x"] + r), p(c["y"] + r)), fill=(8, 8, 8))
        elif c["t"] == "slider":
            d.rectangle((p(c["x"] - 3), p(c["y"] - 63), p(c["x"] + 3), p(c["y"] + 63)), fill=(5, 5, 5))
        elif c["t"] == "switch":
            d.rectangle((p(c["x"] - 4), p(c["y"] - c["h"] / 2), p(c["x"] + 4), p(c["y"] + c["h"] / 2)), fill=(5, 5, 5))
        elif c["t"] == "display":
            d.rectangle((p(c["x"] - 42), p(c["y"] - 34), p(c["x"] + 42), p(c["y"] + 34)), fill=(5, 5, 5))
            disp = sprites.seven_seg("  ", (p(c["w"]), p(c["h"])))
            img.paste(disp, (p(c["x"] - c["w"] / 2), p(c["y"] - c["h"] / 2)))
        elif c["t"] == "wheel":
            d.rectangle((p(c["x"] - 22), p(c["y"] - 110), p(c["x"] + 22), p(c["y"] + 110)), fill=(30, 30, 30))
    save(img, sprites.PANEL["file"])


def knob(name, spec):
    fs = p(spec["w"])
    col = COLOURS[spec["cap"]]
    strip = Image.new("RGBA", (fs, fs * KNOB_FRAMES), (0, 0, 0, 0))
    import math
    for i in range(KNOB_FRAMES):
        d = ImageDraw.Draw(strip)
        cx, cy, r = fs / 2, fs * i + fs / 2, fs * 0.38
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=col + (255,), outline=(0, 0, 0, 255), width=max(1, fs // 30))
        a = math.radians(-135 + 270 * i / (KNOB_FRAMES - 1))
        ind = (240, 240, 230, 255) if spec["cap"] in ("black", "blue", "red") else (20, 20, 20, 255)
        d.line((cx, cy, cx + r * math.sin(a), cy - r * math.cos(a)), fill=ind, width=max(2, fs // 16))
    save(strip, name)


def slidercap(name, spec):
    w, h = p(spec["w"]), p(spec["h"])
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((w * 0.16, h * 0.14, w * 0.84, h * 0.86), radius=3, fill=COLOURS[spec["cap"]] + (255,),
                        outline=(0, 0, 0, 255))
    d.line((w * 0.2, h / 2, w * 0.8, h / 2), fill=(10, 10, 10, 255), width=2)
    save(img, name)


def switch(name, spec):
    w, h, n = p(spec["w"]), p(spec["h"]), spec["frames"]
    strip = Image.new("RGBA", (w, h * n), (0, 0, 0, 0))
    d = ImageDraw.Draw(strip)
    travel = (spec["h"] - 2 * sprites.SWITCH_PAD - 20) * S
    for k in range(n):
        top = h * k
        ly = top + h / 2 - travel / 2 + travel * k / (n - 1)
        d.rounded_rectangle((w * 0.2, ly - p(8), w * 0.8, ly + p(8)), radius=3, fill=(200, 200, 205, 255),
                            outline=(40, 40, 40, 255))
    save(strip, name)


def button(name, spec):
    w, h = p(spec["w"]), p(spec["h"])
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    col = COLOURS[spec["cap"]]
    if spec["state"] == "down":
        col = tuple(int(c * 0.75) for c in col)
    pad = sprites.BUTTON_PAD * S
    d.rounded_rectangle((pad, pad, w - pad, h - pad), radius=3, fill=col + (255,), outline=(0, 0, 0, 255))
    txt = (250, 250, 250, 255) if spec["cap"] in ("grey", "red") else (20, 20, 20, 255)
    f = font(max(8, int(h * 0.3)))
    d.text((w / 2, h / 2), spec["label"], fill=txt, font=f, anchor="mm")
    save(img, name)


def led(name, spec):
    s = p(spec["w"])
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = s * 0.25
    col = (255, 40, 30, 255) if spec["state"] == "on" else (60, 6, 6, 255)
    d.ellipse((s / 2 - r, s / 2 - r, s / 2 + r, s / 2 + r), fill=col)
    save(img, name)


def digits(name, spec):
    w, h = p(spec["w"]), p(spec["h"])
    strip = Image.new("RGBA", (w, h * 10))
    for dgt in range(10):
        strip.paste(sprites.seven_seg(f"{dgt}{dgt}", (w, h)), (0, h * dgt))
    save(strip, name)


def wheel(name, spec):
    w, h = p(spec["w"]), p(spec["h"])
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((w * 0.18, h * 0.12, w * 0.82, h * 0.88), radius=4, fill=(28, 28, 30, 255))
    for y in range(int(h * 0.14), int(h * 0.86), 6):
        d.line((w * 0.2, y, w * 0.8, y), fill=(55, 55, 58, 255))
    save(img, name)


def key(name, spec):
    w, h = p(spec["w"]), p(spec["h"])
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    down = spec["state"] == "down"
    if spec["colour"] == "white":
        d.rectangle((1, 0, w - 2, h - 2), fill=(214, 210, 200, 255) if down else (238, 236, 228, 255),
                    outline=(60, 60, 60, 255))
    else:
        d.rectangle((2, 0, w - 3, h - 3), fill=(45, 45, 48, 255) if down else (16, 16, 18, 255), outline=(0, 0, 0, 255))
    save(img, name)


def main():
    os.makedirs(sprites.ASSETS, exist_ok=True)
    panel()
    fns = dict(knob=knob, slidercap=slidercap, switch=switch, button=button, led=led, digits=digits, wheel=wheel, key=key)
    for name, spec in sprites.sprite_specs(sprites.load_registry()).items():
        fns[spec["kind"]](name, spec)


if __name__ == "__main__":
    main()
