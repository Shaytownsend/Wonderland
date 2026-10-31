"""Moves the slider scales off the section borders (no scale number may touch a border line).

In seven sections the leftmost slider's "10"/"0" sat on the box's left border. This shifts each
section's slider group (ink: scales, ticks, labels; scene: slots, caps) right by SHIFT units, then
re-stamps the leftmost "10"/"0" from a clean scale (the originals were partly hidden under the border).
HPF: the generic "10"/"0" overprinted its own 3/2/1/0 labels -> they are removed and 3/2/1/0 redrawn
in the panel font (Oswald SemiBold, packed in the .blend).

Edits queen_blender/THE_QUEEN.blend (repacks J_ink_panel.png) and queen_blender/textures/J_ink_panel.png.
Idempotent (marker property on the scene). Afterwards: extract_registry.py, tools/make_layout.py,
tools/export_assets.py --only background,slidercap.
Run: .venv/bin/python queen_blender/scripts/fix_scale_clearance.py
"""
import os
import sys
import tempfile

import bpy
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLEND = os.path.join(ROOT, "THE_QUEEN.blend")
INK = os.path.join(ROOT, "textures", "J_ink_panel.png")
MARKER = "queen_scale_clearance"
PX = 6            # ink pixels per logical unit
SHIFT = 6.0       # logical units

# (x0, x1, y0, y1): slider group of each section, x0 just right of the section's left border line
GROUPS = [
    (136.6, 262, 300, 475, [162.0, 202.0, 240.0]),        # CONTROLLERS (with BEND SENS header)
    (335.3, 382, 320, 475, [360.8]),                      # ARPEGGIO
    (520.1, 592, 320, 475, [543.5, 577.5]),               # LFO
    (654.8, 727, 320, 475, [678.3, 714.3]),               # DCO (left pair)
    (996.3, 1201, 320, 475, [1019.7, 1061.7, 1103.7, 1145.7, 1187.8]),  # VCF
    (1267.1, 1306, 320, 475, [1290.5]),                   # VCA
    (1371.8, 1531, 320, 475, [1397.2, 1437.2, 1477.3, 1517.3]),         # ENV
]
REF_SLIDER = 202.0                     # its labels are clean
LABEL_ROWS = [(321, 331), (433, 443)]  # "10" and "0" rows (logical y)
LABEL_COLS = (-27.0, -19.6)            # label block relative to the slider x (left of the tick)
HPF_X, HPF_LABELS = 959.0, [("3", 326.0), ("2", 364.0), ("1", 401.2), ("0", 438.0)]
HPF_LABEL_LEFT = 931.3


def px(x):
    return int(round((x - 120.0) * PX))


def py(y):
    return int(round(y * PX))


def fix_ink(src):
    out = src.copy()
    d = int(round(SHIFT * PX))
    for x0, x1, y0, y1, sliders in GROUPS:
        a, b, c, e = px(x0), px(x1), py(y0), py(y1)
        if (src[c:e, b:b + d, 3] > 20).any():
            sys.exit(f"group {x0}..{x1}: no empty room on the right to shift into")
        block = src[c:e, a:b].copy()
        out[c:e, a:b] = 0
        out[c:e, a + d:b + d] = block
        # clear the band left of the new scale ticks (old label remnants, the border's shifted
        # anti-aliased edge), then re-stamp the leftmost slider's labels from the reference scale
        new_x = sliders[0] + SHIFT
        out[c:e, a:px(new_x - 17.5)] = 0
        for r0, r1 in LABEL_ROWS:
            ra, rb = py(r0), py(r1)
            ref = src[ra:rb, px(REF_SLIDER + LABEL_COLS[0]):px(REF_SLIDER + LABEL_COLS[1])]
            ta = px(new_x + LABEL_COLS[0])
            out[ra:rb, ta:ta + ref.shape[1]] = ref
    return out


def fix_hpf(img, font_path):
    """Erase HPF's overprinted label column and redraw 3/2/1/0."""
    a = np.array(img)
    col = (185, 185, 178)
    x_a, x_b = px(HPF_LABEL_LEFT - 1.0), px(HPF_X + LABEL_COLS[1] + 0.2)
    for _, y in HPF_LABELS:
        a[py(y - 6):py(y + 6), x_a:x_b] = 0
    img = Image.fromarray(a)
    draw = ImageDraw.Draw(img)
    font = ImageFont.truetype(font_path, 10)
    # size the font so a digit is 6.33 units (38 px) tall like the existing scale numbers
    for size in range(20, 120):
        font = ImageFont.truetype(font_path, size)
        l, t, r, bt = font.getbbox("0")
        if bt - t >= 38:
            break
    for text, y in HPF_LABELS:
        l, t, r, bt = font.getbbox(text)
        draw.text((px(HPF_LABEL_LEFT) - l, py(y) - (t + bt) / 2), text, font=font, fill=col + (255,))
    return img


MARKER2 = "queen_scale_clearance_borders"
LEFT_BORDERS = [135.3, 334.0, 518.8, 653.5, 995.0, 1265.8, 1370.5]  # section left border lines (x)


def clean_borders(a):
    """Old '1' glyphs that sat on a border line were left behind by the shift: restore a clean border
    column in the label rows by copying the border's profile from a label-free row (y 385)."""
    for b in LEFT_BORDERS:
        xa, xb = px(b - 3), px(b + 4)
        ref = a[py(385), xa:xb].copy()
        for r0, r1 in LABEL_ROWS:
            a[py(r0):py(r1), xa:xb] = ref[None, :, :]
    return a


def repack_ink(ink_img, arr):
    Image.fromarray(arr).save(INK)
    if ink_img.packed_file:
        ink_img.unpack(method="REMOVE")
    ink_img.filepath = INK
    ink_img.reload()
    ink_img.pack()


def main():
    bpy.ops.wm.open_mainfile(filepath=BLEND)
    sc = bpy.context.scene
    if sc.get(MARKER):
        if not sc.get(MARKER2):
            ink_img = bpy.data.images["J_ink_panel.png"]
            tmp = tempfile.mkdtemp()
            ink_img.filepath_raw = os.path.join(tmp, "ink.png")
            ink_img.file_format = "PNG"
            ink_img.save()
            repack_ink(ink_img, clean_borders(np.array(Image.open(ink_img.filepath_raw).convert("RGBA"))))
            sc[MARKER2] = 1
            bpy.ops.wm.save_mainfile(filepath=BLEND, compress=True)
            print("border remnants cleaned, saved", BLEND)
            return
        print("already applied")
        return

    ink_img = bpy.data.images["J_ink_panel.png"]
    tmp = tempfile.mkdtemp()
    ink_img.filepath_raw = os.path.join(tmp, "ink.png")
    ink_img.file_format = "PNG"
    ink_img.save()
    src = np.array(Image.open(ink_img.filepath_raw).convert("RGBA"))

    font = bpy.data.fonts["Oswald SemiBold"]
    font_path = os.path.join(tmp, "oswald.ttf")
    with open(font_path, "wb") as f:
        f.write(font.packed_file.data)

    fixed = fix_hpf(Image.fromarray(clean_borders(fix_ink(src))), font_path)
    fixed.save(INK)
    print("wrote", INK)

    # scene: move slider parts of each group
    moved = 0
    for x0, x1, y0, y1, sliders in GROUPS:
        for o in bpy.data.objects:
            if o.type != "MESH" or not o.active_material:
                continue
            if o.active_material.name.split(".")[0] not in ("slot", "slot2", "scap", "sc_l", "sc_g"):
                continue
            p = o.matrix_world.translation
            x, y = p.x * 100 + 900, 415 - p.y * 100
            if x0 <= x <= x1 and y0 <= y <= y1 and o.parent is None:
                o.location.x += SHIFT / 100.0
                moved += 1
    print("moved", moved, "slider parts")
    if moved != 5 * sum(len(g[4]) for g in GROUPS):
        sys.exit("unexpected slider part count")

    repack_ink(ink_img, np.array(fixed))
    sc[MARKER] = 1
    sc[MARKER2] = 1
    bpy.ops.wm.save_mainfile(filepath=BLEND, compress=True)
    print("saved", BLEND)


main()
