"""queen_blender/tex2/J.json -> assets/layout.json (same coordinates + per-control sprite info).

Run: python3 tools/make_layout.py
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sprites  # noqa: E402


def main():
    reg = sprites.load_registry()
    out = dict(space=reg["space"], panel=reg["panel"], keybed=reg["keybed"], scale=sprites.SCALE,
               background=sprites.PANEL["file"], controls=[])
    keep = ("id", "t", "x", "y", "r", "w", "h", "param", "value", "action", "positions", "labels", "led_for",
            "momentary", "source", "label", "lowest_note", "white", "black", "white_w", "black_w", "black_h", "cap")
    for c in reg["controls"]:
        e = {k: c[k] for k in keep if k in c}
        e["sprite"] = sprites.control_sprite(c)
        out["controls"].append(e)
    path = os.path.join(sprites.ASSETS, "layout.json")
    with open(path, "w") as f:
        json.dump(out, f, indent=1)
    print("wrote", path, len(out["controls"]), "controls")


if __name__ == "__main__":
    main()
