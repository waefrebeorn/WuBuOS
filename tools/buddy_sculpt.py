#!/usr/bin/env python3
"""WuBu Buddy -- SCULPTED body + real armature rig -> DKC-style sprites.

Why this file exists next to buddy3d.py: the primitive assembly looked like taped
balloons.  Here the fur comes from METABALLS, which blend head/torso/ears/limbs
into one continuous organic surface (no seams), then voxel-remesh + smooth +
subdivide turn it into a sculpt.  Everything is joined into ONE mesh and driven
by a 16-bone ARMATURE with automatic weights -- the actual character-artist
pipeline, and the thing you can pose and animate properly.

Run headless:
  ~/opt/b313/bin/python tools/buddy_sculpt.py build OUT.blend
  ~/opt/b313/bin/python tools/buddy_sculpt.py render MODEL.blend OUTDIR
"""
import math
import os
import sys

import bpy
from mathutils import Vector

COL = {
    "fur":  (0.93, 0.92, 0.90, 1.0),
    "pink": (0.96, 0.62, 0.70, 1.0),
    "cap":  (0.20, 0.62, 0.30, 1.0),
    "red":  (0.78, 0.09, 0.10, 1.0),
    "dark": (0.06, 0.06, 0.07, 1.0),
}

VOXEL = 0.018         # remesh cell -- smaller = more detail, slower
BONES = {}            # bone name -> (head, tail)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    try:
        sc.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        sc.render.engine = "BLENDER_EEVEE"
    return sc


def material(name, rgb, rough=0.6):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = rgb
    b.inputs["Roughness"].default_value = rough
    return m


def smooth(obj):
    for p in obj.data.polygons:
        p.use_smooth = True


# ── 1. organic body from metaballs ─────────────────────────────────────────
def metaball_body(m_fur):
    mb = bpy.data.metaballs.new("body_mb")
    mb.resolution = 0.035
    mb.render_resolution = 0.02
    obj = bpy.data.objects.new("body_mb", mb)
    bpy.context.scene.collection.objects.link(obj)

    def ball(co, size, stiff=2.0):
        e = mb.elements.new(type="ELLIPSOID")
        e.co = Vector(co)
        e.size_x, e.size_y, e.size_z = size
        e.stiffness = stiff

    # torso (two elements -> pear silhouette), head, ears
    ball((0, 0, 0.46), (0.20, 0.17, 0.20))
    ball((0, 0, 0.32), (0.23, 0.19, 0.14))          # wider bottom = pear
    ball((0, 0, 0.76), (0.21, 0.20, 0.21))          # head
    ball((0.10, 0.01, 0.99), (0.045, 0.035, 0.16))  # ear R
    ball((-0.10, 0.01, 0.99), (0.045, 0.035, 0.16)) # ear L
    # arms + mitten hands
    ball((0.21, 0.0, 0.50), (0.05, 0.05, 0.10))
    ball((-0.21, 0.0, 0.50), (0.05, 0.05, 0.10))
    ball((0.25, -0.01, 0.41), (0.062, 0.062, 0.062))
    ball((-0.25, -0.01, 0.41), (0.062, 0.062, 0.062))
    # stubby legs + oversized feet
    ball((0.085, 0.0, 0.20), (0.055, 0.055, 0.11))
    ball((-0.085, 0.0, 0.20), (0.055, 0.055, 0.11))
    ball((0.085, -0.07, 0.055), (0.075, 0.14, 0.045))
    ball((-0.085, -0.07, 0.055), (0.075, 0.14, 0.045))

    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.convert(target="MESH")
    body = bpy.context.active_object
    body.name = "body"
    body.data.materials.append(m_fur)

    # sculpt: unify, relax, refine
    mod = body.modifiers.new("remesh", "REMESH")
    mod.mode = "VOXEL"
    mod.voxel_size = VOXEL
    mod.use_smooth_shade = True
    bpy.ops.object.modifier_apply(modifier=mod.name)

    sm = body.modifiers.new("smooth", "SMOOTH")
    sm.factor = 0.6
    sm.iterations = 4
    bpy.ops.object.modifier_apply(modifier=sm.name)

    sub = body.modifiers.new("subsurf", "SUBSURF")
    sub.levels = 1
    sub.render_levels = 1
    bpy.ops.object.modifier_apply(modifier=sub.name)
    smooth(body)
    return body


# ── 2. accessories (cap, brim, W emblem, inner ears, eyes, mouth) ─────────
def sphere(name, r, m, scale=(1, 1, 1)):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16,
                                         radius=r, location=(0, 0, 0))
    o = bpy.context.active_object
    o.name = name
    o.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    o.data.materials.append(m)
    smooth(o)
    return o


def accessories(m_cap, m_red, m_pink, m_dark):
    parts = []
    cap = sphere("cap", 0.225, m_cap, (1.02, 1.0, 0.80))
    cap.location = (0, 0.005, 0.855)
    brim = sphere("brim", 0.10, m_cap, (1.25, 1.1, 0.28))
    brim.location = (0, -0.19, 0.865)
    parts += [cap, brim]

    for side, sx in (("L", -1), ("R", 1)):
        inner = sphere(f"ear_in{side}", 0.026, m_pink, (1.0, 0.4, 3.0))
        inner.location = (sx * 0.105, -0.03, 0.99)
        inner.rotation_euler = (0, math.radians(sx * 20), 0)
        parts.append(inner)
        eye = sphere(f"eye{side}", 0.024, m_dark)
        eye.location = (sx * 0.080, -0.20, 0.80)
        parts.append(eye)

    mouth = sphere("mouth", 0.020, m_dark, (1.4, 0.6, 0.9))
    mouth.location = (0, -0.212, 0.735)
    parts.append(mouth)

    bpy.ops.object.text_add(location=(0, 0, 0))
    w = bpy.context.active_object
    w.name = "emblem_W"
    w.data.body = "W"
    w.data.align_x = "CENTER"
    w.data.size = 0.150
    w.data.extrude = 0.014
    w.rotation_euler = (math.radians(90), 0, 0)
    w.location = (0, -0.248, 0.858)
    bpy.ops.object.convert(target="MESH")
    w = bpy.context.active_object
    w.data.materials.append(m_red)
    parts.append(w)
    return parts


# ── 3. armature ────────────────────────────────────────────────────────────
def make_armature():
    data = bpy.data.armatures.new("rig")
    arm = bpy.data.objects.new("rig", data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")

    def bone(name, head, tail, parent=None):
        eb = data.edit_bones.new(name)
        eb.head = Vector(head)
        eb.tail = Vector(tail)
        if parent:
            eb.parent = data.edit_bones[parent]
            eb.use_connect = False
        BONES[name] = (Vector(head), Vector(tail))

    bone("root", (0, 0, 0.02), (0, 0, 0.20))
    bone("spine", (0, 0, 0.34), (0, 0, 0.56), "root")
    bone("head", (0, 0, 0.62), (0, 0, 0.80), "spine")
    bone("earL", (-0.09, 0.01, 0.92), (-0.13, 0.01, 1.10), "head")
    bone("earR", (0.09, 0.01, 0.92), (0.13, 0.01, 1.10), "head")
    for s, sx in (("L", -1), ("R", 1)):
        bone(f"arm{s}", (sx * 0.19, 0, 0.56), (sx * 0.25, 0, 0.44), "spine")
        bone(f"fore{s}", (sx * 0.25, 0, 0.44), (sx * 0.26, 0, 0.37), f"arm{s}")
        bone(f"hand{s}", (sx * 0.26, 0, 0.37), (sx * 0.27, 0, 0.32), f"fore{s}")
        bone(f"leg{s}", (sx * 0.085, 0, 0.30), (sx * 0.085, 0, 0.14), "root")
        bone(f"shin{s}", (sx * 0.085, 0, 0.14), (sx * 0.085, 0, 0.05), f"leg{s}")
        bone(f"foot{s}", (sx * 0.085, 0, 0.05), (sx * 0.085, -0.10, 0.05), f"shin{s}")

    bpy.ops.object.mode_set(mode="OBJECT")
    return arm


def bind(mesh, arm):
    bpy.ops.object.select_all(action="DESELECT")
    mesh.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")


# ── 4. build / render ──────────────────────────────────────────────────────
def stage_camera_lights(sc):
    cd = bpy.data.cameras.new("cam")
    cd.type = "ORTHO"
    cd.ortho_scale = 1.30
    cam = bpy.data.objects.new("cam", cd)
    cam.location = (0, -3.0, 0.55)
    cam.rotation_euler = (math.radians(90), 0, 0)
    sc.collection.objects.link(cam)
    sc.camera = cam

    def area(name, loc, energy, size, color=(1, 1, 1)):
        d = bpy.data.lights.new(name, "AREA")
        d.energy = energy
        d.shape = "DISK"
        d.size = size
        d.color = color
        o = bpy.data.objects.new(name, d)
        o.location = loc
        sc.collection.objects.link(o)
        o.rotation_euler = (Vector((0, 0, 0.5)) - o.location).to_track_quat("-Z", "Y").to_euler()

    area("key", (-1.6, -2.0, 2.2), 85, 2.0, (1.0, 0.97, 0.92))
    area("fill", (1.8, -1.6, 1.0), 38, 2.0, (0.85, 0.90, 1.0))
    area("rim", (0.6, 1.8, 2.0), 60, 1.6)

    try:
        sc.view_settings.view_transform = "Standard"
        sc.view_settings.look = "None"
    except TypeError:
        pass
    sc.render.resolution_x = 512
    sc.render.resolution_y = 512
    sc.render.film_transparent = True
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"


def cmd_build(out):
    reset()
    m_fur = material("fur", COL["fur"])
    m_cap = material("cap", COL["cap"], 0.5)
    m_red = material("red", COL["red"], 0.5)
    m_pink = material("pink", COL["pink"], 0.6)
    m_dark = material("dark", COL["dark"], 0.4)

    body = metaball_body(m_fur)
    acc = accessories(m_cap, m_red, m_pink, m_dark)

    bpy.ops.object.select_all(action="DESELECT")
    for o in acc:
        o.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()                       # one skinned mesh
    char = bpy.context.active_object
    char.name = "buddy"

    arm = make_armature()
    bind(char, arm)
    stage_camera_lights(bpy.context.scene)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(out))
    print(f"sculpted {len(char.data.vertices)} verts, {len(arm.data.bones)} bones -> {out}")


# ── 5. bone-driven animation ───────────────────────────────────────────────
def _reset_pose(arm):
    for b in arm.pose.bones:
        b.rotation_mode = "XYZ"
        b.rotation_euler = (0.0, 0.0, 0.0)
        b.location = (0.0, 0.0, 0.0)
        b.scale = (1.0, 1.0, 1.0)


def _rot(arm, name, x=0.0, y=0.0, z=0.0):
    arm.pose.bones[name].rotation_euler = (x, y, z)


def _move(arm, name, dz=0.0):
    arm.pose.bones[name].location = (0.0, 0.0, dz)


def anim_idle(t, arm):
    s = math.sin(t * 2 * math.pi)
    _move(arm, "root", s * 0.02)
    _rot(arm, "spine", s * 0.03)
    _rot(arm, "head", math.sin(t * 2 * math.pi + 0.6) * 0.05)
    _rot(arm, "earL", 0, 0, s * 0.12)
    _rot(arm, "earR", 0, 0, -s * 0.12)


def anim_walk(t, arm):
    s = math.sin(t * 2 * math.pi)
    c = math.cos(t * 2 * math.pi)
    _rot(arm, "legL", s * 0.7)
    _rot(arm, "legR", -s * 0.7)
    _rot(arm, "shinL", max(0.0, -s) * 0.9)
    _rot(arm, "shinR", max(0.0, s) * 0.9)
    _rot(arm, "footL", -s * 0.4)
    _rot(arm, "footR", s * 0.4)
    _rot(arm, "armL", -s * 0.55)
    _rot(arm, "armR", s * 0.55)
    _rot(arm, "spine", 0.12)                 # forward lean
    _move(arm, "root", abs(c) * 0.03)


def anim_wave(t, arm):
    s = math.sin(t * 2 * math.pi * 2)
    _rot(arm, "armR", math.radians(165))
    _rot(arm, "foreR", 0, 0, math.radians(20 + s * 22))
    _rot(arm, "head", 0, math.radians(-6))
    _move(arm, "root", abs(math.sin(t * 2 * math.pi)) * 0.015)


def anim_celebrate(t, arm):
    j = abs(math.sin(t * 2 * math.pi))
    _rot(arm, "armL", math.radians(168))
    _rot(arm, "armR", math.radians(168))
    _rot(arm, "earL", 0, 0, 0.22)
    _rot(arm, "earR", 0, 0, -0.22)
    _move(arm, "root", j * 0.14)


ANIMS = {"idle": (anim_idle, 8), "walk": (anim_walk, 8),
         "wave": (anim_wave, 8), "celebrate": (anim_celebrate, 8)}


def cmd_render(model, outdir):
    bpy.ops.wm.open_mainfile(filepath=os.path.abspath(model))
    sc = bpy.context.scene
    stage_camera_lights(sc)
    arm = bpy.data.objects["rig"]
    os.makedirs(outdir, exist_ok=True)
    n = 0
    for name, (fn, frames) in ANIMS.items():
        for f in range(frames):
            _reset_pose(arm)
            fn(f / float(frames), arm)
            sc.render.filepath = os.path.join(outdir, f"{name}_{f:02d}.png")
            bpy.ops.render.render(write_still=True)
            n += 1
    print(f"rendered {n} rigged frames -> {outdir}")
    return 0


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    cmd, rest = sys.argv[1], sys.argv[2:]
    if cmd == "build":
        cmd_build(rest[0])
    elif cmd == "render":
        cmd_render(rest[0], rest[1])
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
