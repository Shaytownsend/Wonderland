"""Renders the untouched scene (every control in place) through the export camera, for visual checks
of the GUI against the Blender original.  .venv/bin/python tools/render_reference.py out.png [samples]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402
import export_assets as ea  # noqa: E402
import sprites  # noqa: E402

out = sys.argv[1] if len(sys.argv) > 1 else "reference.png"
samples = int(sys.argv[2]) if len(sys.argv) > 2 else 16
bpy.ops.wm.open_mainfile(filepath=ea.BLEND)
cam = ea.setup(samples)
ea.frame_region(cam, 0, 0, sprites.PANEL["w"], sprites.PANEL["h"])
sc = bpy.context.scene
sc.render.resolution_x, sc.render.resolution_y = sprites.PANEL["w"], sprites.PANEL["h"]
ea.set_visibility(set(), set(), catcher_mode=False)
ea.set_display("14")
ea.render_to(os.path.abspath(out))
print("wrote", out)
