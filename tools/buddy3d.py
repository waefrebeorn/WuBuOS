#!/usr/bin/env python3
"""WuBu Buddy 3-D model -> Donkey-Kong-Country style pre-rendered sprites.

Why this exists: DKC's sprites were Silicon Graphics renders of 3-D models,
downsampled to pixels.  Building the buddy as a real model and rendering it
gives (a) correct anatomy from every angle, (b) a consistent character across
every pose, and (c) alpha straight from the renderer -- no background removal,
because the film is transparent.

Run headless:
  ~/opt/b313/bin/python tools/buddy3d.py build OUT.blend
  ~/opt/b313/bin/python tools/buddy3d.py render MODEL.blend OUTDIR [frames]

Parts are separate objects whose ORIGINS sit at their joints, so animation is
just per-object rotation/location keyframes -- no armature needed for a chibi.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

# ── palette (flat brand colours; DKC reads through shading, not texture) ────
COL = {
    "fur":   (0.93, 0.92, 0.90, 1.0),
    "pink":  (0.96, 0.62, 0.70, 1.0),
    "cap":   (0.20, 0.62, 0.30, 1.0),
    "red":   (0.78, 0.09, 0.10, 1.0),
    "dark":  (0.06, 0.06, 0.07, 1.0),
    "white": (0.98, 0.98, 0.98, 1.0),
}

PARTS = {}          # name -> (obj, rest_location, rest_rotation_euler)


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    try:
        sc.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        sc.render.engine = "BLENDER_EEVEE"


def mat(name, rgb, rough=0.55):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = rgb
    bsdf.inputs["Roughness"].default_value = rough
    return m


def _finish(obj, material, smooth=True):
    obj.data.materials.append(material)
    if smooth:
        for p in obj.data.polygons:
            p.use_smooth = True
    return obj


def sphere(name, radius, mat_, scale=(1, 1, 1)):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16,
                                         radius=radius, location=(0, 0, 0))
    o = bpy.context.active_object
    o.name = name
    o.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    return _finish(o, mat_)


def cyl(name, radius, depth, mat_):
    bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=radius,
                                        depth=depth, location=(0, 0, 0))
    o = bpy.context.active_object
    o.name = name
    return _finish(o, mat_)


def torus(name, major, minor, mat_):
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor,
                                     major_segments=32, minor_segments=12,
                                     location=(0, 0, 0))
    o = bpy.context.active_object
    o.name = name
    return _finish(o, mat_)


def box(name, size, mat_):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0))
    o = bpy.context.active_object
    o.name = name
    o.scale = size
    bpy.ops.object.transform_apply(scale=True)
    return _finish(o, mat_, smooth=False)


def put(name, obj, local_offset, world, rot=(0.0, 0.0, 0.0)):
    """Move the mesh so the object ORIGIN is the joint, then place the joint."""
    for v in obj.data.vertices:
        v.co += Vector(local_offset)
    obj.location = Vector(world)
    obj.rotation_euler = rot
    PARTS[name] = (obj, Vector(world), tuple(rot))


def build_model():
    m_fur = mat("fur", COL["fur"], 0.6)
    m_pink = mat("pink", COL["pink"], 0.6)
    m_cap = mat("cap", COL["cap"], 0.5)
    m_red = mat("red", COL["red"], 0.5)
    m_dark = mat("dark", COL["dark"], 0.4)

    # legs + oversized feet (origin at hip / ankle so they swing)
    for side, sx in (("L", -1), ("R", 1)):
        leg = cyl(f"leg{side}", 0.048, 0.20, m_fur)
        put(f"leg{side}", leg, (0, 0, 0.10), (sx * 0.085, 0.0, 0.30))
        foot = sphere(f"foot{side}", 0.075, m_fur, scale=(1.0, 1.7, 0.55))
        put(f"foot{side}", foot, (0, 0, 0.0), (sx * 0.085, -0.05, 0.045))

    # pear torso: widen the lower half
    torso = sphere("torso", 0.21, m_fur, scale=(1.0, 0.88, 1.05))
    for v in torso.data.vertices:
        t = (v.co.z / 0.22 + 1.0) / 2.0              # 0 bottom .. 1 top
        s = 1.0 + 0.30 * (1.0 - t) ** 2
        v.co.x *= s
        v.co.y *= s
    put("torso", torso, (0, 0, 0), (0.0, 0.0, 0.44))

    # scarf ring + two tails down the chest
    ring = torus("scarf_ring", 0.150, 0.036, m_red)
    put("scarf_ring", ring, (0, 0, 0), (0.0, 0.0, 0.615))
    for i, sx in enumerate((-1, 1)):
        tail = box(f"scarf_tail{i}", (0.055, 0.02, 0.11), m_red)
        put(f"scarf_tail{i}", tail, (0, 0, 0.055),
            (sx * 0.055, -0.175, 0.560), rot=(math.radians(6), 0, 0))

    # arms + mitten hands (origin at shoulder)
    for side, sx in (("L", -1), ("R", 1)):
        arm = cyl(f"arm{side}", 0.042, 0.16, m_fur)
        put(f"arm{side}", arm, (0, 0, 0.08),
            (sx * 0.215, 0.0, 0.575), rot=(0, math.radians(-sx * 16), 0))
        hand = sphere(f"hand{side}", 0.055, m_fur)
        put(f"hand{side}", hand, (0, 0, 0),
            (sx * 0.255, 0.0, 0.505))

    # head
    head = sphere("head", 0.215, m_fur, scale=(1.0, 0.98, 1.0))
    put("head", head, (0, 0, 0), (0.0, 0.0, 0.775))

    # ears: long, angled outward, pink inner
    for side, sx in (("L", -1), ("R", 1)):
        ear = sphere(f"ear{side}", 0.050, m_fur, scale=(1.0, 0.75, 3.4))
        put(f"ear{side}", ear, (0, 0, 0.155),
            (sx * 0.105, 0.015, 0.955), rot=(0, math.radians(sx * 26), 0))
        inner = sphere(f"ear_in{side}", 0.028, m_pink, scale=(1.0, 0.45, 3.1))
        put(f"ear_in{side}", inner, (0, 0, 0.150),
            (sx * 0.108, -0.022, 0.952), rot=(0, math.radians(sx * 26), 0))

    # aviator cap + brim
    cap = sphere("cap", 0.225, m_cap, scale=(1.02, 1.0, 0.82))
    put("cap", cap, (0, 0, 0), (0.0, 0.005, 0.845))
    brim = sphere("brim", 0.10, m_cap, scale=(1.25, 1.1, 0.28))
    put("brim", brim, (0, 0, 0), (0.0, -0.185, 0.855))

    # red W emblem on the cap front
    bpy.ops.object.text_add(location=(0, 0, 0))
    w = bpy.context.active_object
    w.name = "emblem_W"
    w.data.body = "W"
    w.data.align_x = "CENTER"
    w.data.size = 0.150
    w.data.extrude = 0.014
    w.rotation_euler = (math.radians(90), 0, 0)
    w.location = (0.0, -0.243, 0.848)
    bpy.ops.object.convert(target="MESH")
    w = bpy.context.active_object
    _finish(w, m_red, smooth=False)

    # dot eyes + mouth
    for side, sx in (("L", -1), ("R", 1)):
        eye = sphere(f"eye{side}", 0.024, m_dark)
        put(f"eye{side}", eye, (0, 0, 0), (sx * 0.080, -0.196, 0.800))
    mouth = sphere("mouth", 0.020, m_dark, scale=(1.4, 0.6, 0.9))
    put("mouth", mouth, (0, 0, 0), (0.0, -0.205, 0.735))


# ── animation drivers: frame -> per-part transform deltas ─────────────────
def _apply(rot=None, loc=None):
    for name, (obj, rest_loc, rest_rot) in PARTS.items():
        obj.rotation_euler = rest_rot
        obj.location = rest_loc
    if rot:
        for n, e in rot.items():
            PARTS[n][0].rotation_euler = tuple(
                a + b for a, b in zip(PARTS[n][2], e))
    if loc:
        for n, d in loc.items():
            PARTS[n][0].location = PARTS[n][1] + Vector(d)


def anim_idle(t):
    b = math.sin(t * 2 * math.pi) * 0.020
    _apply(loc={n: (0, 0, b) for n in PARTS},
           rot={"earL": (math.sin(t * 2 * math.pi) * 0.18, 0, 0),
                "earR": (-math.sin(t * 2 * math.pi) * 0.18, 0, 0)})


def anim_walk(t):
    s = math.sin(t * 2 * math.pi)
    c = math.cos(t * 2 * math.pi)
    rot = {"legL": (s * 0.85, 0, 0), "legR": (-s * 0.85, 0, 0),
           "footL": (-s * 0.45, 0, 0), "footR": (s * 0.45, 0, 0),
           "armL": (-s * 0.65, 0, 0), "armR": (s * 0.65, 0, 0)}
    loc = {n: (0, 0, abs(c) * 0.032) for n in PARTS}
    _apply(rot=rot, loc=loc)


def anim_wave(t):
    s = math.sin(t * 2 * math.pi * 2)
    _apply(rot={"armR": (math.radians(-165), 0, math.radians(30 + s * 26)),
                "handR": (0, 0, s * 0.3)},
           loc={n: (0, 0, abs(math.sin(t * 2 * math.pi)) * 0.016)
                for n in PARTS})


def anim_celebrate(t):
    j = abs(math.sin(t * 2 * math.pi))
    _apply(rot={"armL": (math.radians(-170), 0, math.radians(-26)),
                "armR": (math.radians(-170), 0, math.radians(26)),
                "earL": (0.25, 0, 0), "earR": (-0.25, 0, 0)},
           loc={n: (0, 0, j * 0.16) for n in PARTS})


ANIMS = {"idle": (anim_idle, 8), "walk": (anim_walk, 8),
         "wave": (anim_wave, 8), "celebrate": (anim_celebrate, 8)}


def setup_camera_lights():
    sc = bpy.context.scene
    cam_d = bpy.data.cameras.new("cam")
    cam_d.type = "ORTHO"
    cam_d.ortho_scale = 1.30
    cam = bpy.data.objects.new("cam", cam_d)
    cam.location = (0.0, -3.0, 0.52)
    cam.rotation_euler = (math.radians(90), 0, 0)
    sc.collection.objects.link(cam)
    sc.camera = cam

    def area(name, loc, energy, size, color):
        d = bpy.data.lights.new(name, "AREA")
        d.energy = energy
        d.shape = "DISK"
        d.size = size
        d.color = color
        o = bpy.data.objects.new(name, d)
        o.location = loc
        sc.collection.objects.link(o)
        # aim at the character
        dirv = Vector((0, 0, 0.5)) - o.location
        o.rotation_euler = dirv.to_track_quat("-Z", "Y").to_euler()

    # AgX (Blender's default) desaturates highlights -- the flat brand colours
    # bleached to pastel.  'Standard' keeps them punchy, which is the DKC read.
    try:
        sc.view_settings.view_transform = "Standard"
        sc.view_settings.look = "None"
    except TypeError:
        pass

    area("key", (-1.6, -2.0, 2.2), 85, 2.0, (1.0, 0.97, 0.92))
    area("fill", (1.8, -1.6, 1.0), 38, 2.0, (0.85, 0.90, 1.0))
    area("rim", (0.6, 1.8, 2.0), 60, 1.6, (1.0, 1.0, 1.0))

    sc.render.resolution_x = 512
    sc.render.resolution_y = 512
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = True
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"


def collect_parts():
    """Rebuild the part table from the saved scene (rest pose)."""
    PARTS.clear()
    for o in bpy.data.objects:
        if o.type == "MESH":
            PARTS[o.name] = (o, o.location.copy(), tuple(o.rotation_euler))


def cmd_build(out):
    reset_scene()
    build_model()
    setup_camera_lights()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(out))
    print(f"built {len(PARTS)} parts -> {out}")


def cmd_render(model, outdir):
    bpy.ops.wm.open_mainfile(filepath=os.path.abspath(model))
    collect_parts()
    setup_camera_lights()
    sc = bpy.context.scene
    os.makedirs(outdir, exist_ok=True)
    n = 0
    for name, (fn, frames) in ANIMS.items():
        for f in range(frames):
            fn(f / float(frames))
            sc.render.filepath = os.path.join(
                outdir, f"{name}_{f:02d}.png")
            bpy.ops.render.render(write_still=True)
            n += 1
    print(f"rendered {n} frames -> {outdir}")


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
