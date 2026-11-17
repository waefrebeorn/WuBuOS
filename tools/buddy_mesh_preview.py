#!/usr/bin/env python3
"""Inspect a generated mesh before building anything on it.

Renders a turntable: 8 azimuths around the model plus a front view, orthographic,
on a flat background so silhouette and proportions read clearly.  Run with the
Blender env:

  ~/opt/b313/bin/python tools/buddy_mesh_preview.py MESH.glb OUT_DIR [prefix]

Output: OUT_DIR/<prefix>_<i>.png  (i = 0..7 around, 8 = front)
"""
import math
import os
import sys

import bpy
from mathutils import Vector

OUT_W = OUT_H = 320


def frame_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    try:
        sc.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        sc.render.engine = "BLENDER_EEVEE"
    return sc


def import_model(path):
    bpy.ops.import_scene.gltf(filepath=os.path.abspath(path))
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if not objs:
        raise SystemExit("no mesh imported")
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    for o in objs:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector((min(lo[i], w[i]) for i in range(3)))
            hi = Vector((max(hi[i], w[i]) for i in range(3)))
    return objs, (lo + hi) / 2.0, (hi - lo)


def add_camera(centre, radius, ortho, azimuth_deg, elev_deg=6.0):
    cd = bpy.data.cameras.new("cam")
    cd.type = "ORTHO"
    cd.ortho_scale = ortho * 1.15
    cam = bpy.data.objects.new("cam", cd)
    az = math.radians(azimuth_deg)
    el = math.radians(elev_deg)
    cam.location = centre + Vector((radius * math.sin(az) * math.cos(el),
                                    -radius * math.cos(az) * math.cos(el),
                                    radius * math.sin(el)))
    d = centre - cam.location
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.collection.objects.link(cam)
    return cam


def add_lights(centre):
    def area(name, loc, energy, size, color=(1, 1, 1)):
        ld = bpy.data.lights.new(name, "AREA")
        ld.energy = energy
        ld.shape = "DISK"
        ld.size = size
        ld.color = color
        o = bpy.data.objects.new(name, ld)
        o.location = centre + Vector(loc)
        o.rotation_euler = (centre - o.location).to_track_quat("-Z", "Y").to_euler()
        bpy.context.scene.collection.objects.link(o)

    area("key", (-1.4, -1.6, 1.8), 900, 1.6, (1.0, 0.97, 0.92))
    area("fill", (1.5, -1.2, 0.8), 400, 1.6, (0.85, 0.90, 1.0))
    area("rim", (0.4, 1.6, 1.6), 700, 1.2)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    src, outdir = sys.argv[1], sys.argv[2]
    prefix = sys.argv[3] if len(sys.argv) > 3 else "turn"
    os.makedirs(outdir, exist_ok=True)

    sc = frame_scene()
    _objs, centre, size = import_model(src)
    radius = max(size) * 2.2
    ortho = max(size.x, size.y, size.z)
    add_lights(centre)

    try:
        sc.view_settings.view_transform = "Standard"
        sc.view_settings.look = "None"
    except TypeError:
        pass
    sc.render.resolution_x = OUT_W
    sc.render.resolution_y = OUT_H
    sc.render.film_transparent = False
    sc.world = bpy.data.worlds.new("w")
    sc.world.use_nodes = True
    sc.world.node_tree.nodes["Background"].inputs[0].default_value = (0.16, 0.15, 0.18, 1)
    sc.render.image_settings.file_format = "PNG"

    angles = list(range(0, 360, 45)) + [0]      # 8 around + front
    for i, az in enumerate(angles):
        cam = add_camera(centre, radius, ortho, float(az))
        sc.camera = cam
        sc.render.filepath = os.path.join(outdir, f"{prefix}_{i}.png")
        bpy.ops.render.render(write_still=True)
        bpy.data.objects.remove(cam, do_unlink=True)
    print(f"previewed {len(angles)} views -> {outdir} (centre={tuple(round(v,3) for v in centre)}, size={tuple(round(v,3) for v in size)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
