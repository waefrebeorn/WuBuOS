#!/usr/bin/env python3
"""Build the buddy FROM the reference art instead of approximating it.

Every earlier attempt invented shapes and then compared them to the reference --
that is why it reads as an invented blob.  Here the reference drawing IS the
geometry: we take the clean front-view slice, turn its opaque mask into a
surface (thickness from the distance to the silhouette edge), and project the
artwork back onto it.  The silhouette therefore matches the reference exactly,
by construction rather than by tuning.

  head/torso/feet are whatever the drawing says they are.

Run:
  ~/opt/b313/bin/python tools/buddy_from_ref.py build REF.png OUT.blend [--rows 72]
"""
import argparse
import os
import sys

try:
    import bpy
except ImportError:
    bpy = None
import numpy as np
from PIL import Image
from scipy import ndimage


def write_obj(path, verts, faces, uv):
    with open(path, "w") as f:
        for v in verts:
            f.write("v %.5f %.5f %.5f\n" % v)
        for t in uv:
            f.write("vt %.5f %.5f\n" % t)
        for face in faces:
            f.write("f " + " ".join(f"{i+1}/{i+1}" for i in face) + "\n")
    print(f"wrote {path} ({len(verts)} verts, {len(faces)} faces)")


def load_mask(path, rows=72):
    """Downsample the slice to rows x cols, return boolean mask."""
    im = Image.open(path).convert("RGBA")
    a = np.asarray(im)
    alpha = a[:, :, 3] > 10
    ys, xs = np.where(alpha)
    if len(ys) == 0:
        raise SystemExit(f"{path}: fully transparent")
    im2 = im.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))
    w = max(8, int(round(rows * im2.size[0] / im2.size[1])))
    small = im2.resize((w, rows), Image.LANCZOS)
    m = np.asarray(small.convert("RGBA"))[:, :, 3] > 40
    return small, m


def build_surface(mask, depth_scale=0.0, front_only=True):
    """Outer skin of the reference silhouette (front projection).

    Thickness is left to Blender's Solidify + voxel Remesh: hand-rolling rim and
    back faces tore the surface into crumpled sheets.  With depth_scale=0 the
    skin is flat; Solidify inflates it symmetrically.
    """
    rows, cols = mask.shape
    dist = ndimage.distance_transform_edt(mask)
    dmax = max(dist.max(), 1e-6)
    bulge = depth_scale * (dist / dmax) ** 0.75
    sx = 1.5 / max(cols, rows)
    verts, front = [], {}
    for r in range(rows):
        for c in range(cols):
            if not mask[r, c]:
                continue
            x = (c - cols / 2.0) * sx
            z = (rows - 1 - r) * sx
            verts.append((x, -bulge[r, c], z))
            front[(r, c)] = len(verts) - 1

    faces = []
    for r in range(rows - 1):
        for c in range(cols - 1):
            q = [(r, c), (r, c + 1), (r + 1, c + 1), (r + 1, c)]
            if all(p in front for p in q):
                faces.append([front[p] for p in q])

    if front_only:
        return verts, faces

    # symmetrical back skin + rim, for callers wanting an immediate solid
    back = {}
    for k, (x, y, z) in enumerate(verts):
        back[k] = len(verts)
        verts.append((x, y * 0.35, z))
    faces = [f[:] for f in faces]
    for f in list(faces):
        faces.append([back[i] for i in reversed(f)])
    for r in range(rows - 1):
        for c in range(cols - 1):
            for (a, b) in (((r, c), (r, c + 1)), ((r, c), (r + 1, c))):
                if a in front and b in front:
                    across = (a[0] + (b[1] - a[1]), a[1] + (b[0] - a[0]))
                    if across not in front:
                        faces.append([front[a], front[b], back[front[b]],
                                      back[front[a]]])
    return verts, faces


def uv_from_grid(rows, cols, verts):
    """Front projection UVs so the reference art textures the model."""
    us = [v[0] for v in verts]
    zs = [v[2] for v in verts]
    u0, u1 = min(us), max(us)
    z0, z1 = min(zs), max(zs)
    uv = [( (x - u0) / (u1 - u0 or 1), 1.0 - (z - z0) / (z1 - z0 or 1))
          for x, _, z in verts]
    return uv


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd")
    ap.add_argument("src", nargs="?")
    ap.add_argument("out", nargs="?")
    ap.add_argument("--rows", type=int, default=72)
    a = ap.parse_args()
    if a.cmd != "build" or not a.src or not a.out:
        print(__doc__)
        return 2

    small, mask = load_mask(a.src, a.rows)
    verts, faces = build_surface(mask)
    uv = uv_from_grid(*mask.shape, verts)

    if bpy is None:
        write_obj(os.path.abspath(a.out), verts, faces, uv)
        return 0

    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    try:
        sc.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        sc.render.engine = "BLENDER_EEVEE"

    me = bpy.data.meshes.new("buddy")
    me.from_pydata(verts, [], faces)
    me.update()
    ob = bpy.data.objects.new("buddy", me)
    sc.collection.objects.link(ob)

    layer = me.uv_layers.new(name="UVMap")
    for i, co in enumerate(me.loops):
        layer.data[i].uv = uv[co.vertex_index]

    tex = bpy.data.images.load(os.path.abspath(a.src))
    mat = bpy.data.materials.new("ref_art")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    img = nt.nodes.new("ShaderNodeTexImage")
    img.image = tex
    img.interpolation = "Closest"
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    nt.links.new(img.outputs["Color"], bsdf.inputs["Base Color"])
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    me.materials.append(mat)

    for p in me.polygons:
        p.use_smooth = True
    sub = ob.modifiers.new("subsurf", "SUBSURF")
    sub.levels = 1
    sub.render_levels = 1

    sc.world = bpy.data.worlds.new("w")
    sc.world.use_nodes = True
    sc.world.node_tree.nodes["Background"].inputs[0].default_value = (0.16, 0.15, 0.18, 1)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(a.out))
    print(f"built {len(me.vertices)} verts / {len(me.polygons)} faces -> {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
