"""Makes the panel's small text legible.

1. Button cap labels (SAVE, VERIFY, LOAD, MANUAL, LFO TRIG, ...) were ~5-6 units tall and blurred to
   nothing at normal window sizes: each label is scaled up to ~9 units tall, never wider than 80 % of
   its cap (labels that are already large enough are left alone).
2. The BANK / PATCH ink labels sat under the LED display's bezel: the display unit (bezel, glass, LED face)
   is scaled to 86 % and moved 7 units left, clearing both labels and the MEMORY box border.

Edits queen_blender/THE_QUEEN.blend; idempotent. Afterwards: extract_registry.py, tools/make_layout.py,
tools/export_assets.py --only background,button,digits.
"""
import os

import bpy

BLEND = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "THE_QUEEN.blend")
MARKER = "queen_label_legibility"
TARGET_H, MAX_W = 0.090, 0.80          # label height (BU = 100 units), max fraction of cap width
DISPLAY = ("Cube.012", "Cube.013", "Plane.003")  # bezel, glass, LED face
DISPLAY_SCALE, DISPLAY_DX = 0.86, -0.07


def main():
    bpy.ops.wm.open_mainfile(filepath=BLEND)
    sc = bpy.context.scene
    if sc.get(MARKER):
        print("already applied")
        return

    caps = [o for o in bpy.data.objects if o.type == "MESH" and o.active_material
            and o.active_material.name.startswith("bcap")]
    scaled = 0
    for t in bpy.data.objects:
        if t.type != "FONT" or not (t.active_material and t.active_material.name.startswith("blg")):
            continue
        cap = min(caps, key=lambda c: (c.location - t.location).xy.length)
        if (cap.location - t.location).xy.length > 0.03:
            continue  # not a button label
        w, h = t.dimensions.x, t.dimensions.y
        f = min(TARGET_H / h, MAX_W * cap.dimensions.x / w)
        if f > 1.02:
            t.scale = (t.scale[0] * f, t.scale[1] * f, t.scale[2])
            scaled += 1
            print(f"label {t.data.body!r}: x{f:.2f}")
    print("scaled", scaled, "labels")

    for name in DISPLAY:
        o = bpy.data.objects[name]
        o.scale = (o.scale[0] * DISPLAY_SCALE, o.scale[1] * DISPLAY_SCALE, o.scale[2])
        o.location.x += DISPLAY_DX

    sc[MARKER] = 1
    bpy.ops.wm.save_mainfile(filepath=BLEND, compress=True)
    print("saved", BLEND)


main()
