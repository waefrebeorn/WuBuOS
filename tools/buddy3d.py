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
    # Subdivision turns faceted primitives into sculpted-looking surfaces
    # WITHOUT fusing the parts together (metaballs over-fused and the model
    # lost its arms, legs and feet -- the silhouette collapsed into a blob).
    sub = obj.modifiers.new("subsurf", "SUBSURF")
    sub.levels = 1
    sub.render_levels = 1
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=sub.name)
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


INK = None


def make_ink():
    global INK
    if INK is None:
        INK = bpy.data.materials.new("ink")
        INK.use_nodes = True
        b = INK.node_tree.nodes["Principled BSDF"]
        b.inputs["Base Color"].default_value = (0.03, 0.03, 0.04, 1.0)
        b.inputs["Roughness"].default_value = 0.9
    return INK


def outline(obj, thickness=0.010):
    """Inverted-hull ink shell: a slightly larger back-facing copy, parented to
    the part so it follows every pose.  EEVEE has no Freestyle, and without
    contour lines every white form merges into the next."""
    o = obj.copy()
    o.data = obj.data.copy()
    bpy.context.scene.collection.objects.link(o)
    for p in o.data.polygons:
        p.flip()
    o.data.materials.clear()
    o.data.materials.append(make_ink())
    d = o.modifiers.new("push", "DISPLACE")
    d.direction = "NORMAL"
    d.strength = thickness
    d.mid_level = 0.0
    o.parent = obj
    o.matrix_parent_inverse = obj.matrix_world.inverted()
    return o


def put(name, obj, local_offset, world, rot=(0.0, 0.0, 0.0), ink=False):
    """Move the mesh so the object ORIGIN is the joint, then place the joint."""
    for v in obj.data.vertices:
        v.co += Vector(local_offset)
    obj.location = Vector(world)
    obj.rotation_euler = rot
    PARTS[name] = (obj, Vector(world), tuple(rot))
    if ink and name not in ("eyeL", "eyeR", "mouth"):
        outline(obj)
    return obj


def build_model():
    m_fur = mat("fur", COL["fur"], 0.6)
    m_pink = mat("pink", COL["pink"], 0.6)
    m_cap = mat("cap", COL["cap"], 0.5)
    m_red = mat("red", COL["red"], 0.5)
    m_dark = mat("dark", COL["dark"], 0.4)

    # legs + oversized feet (mesh hangs DOWN from the hip joint)
    for side, sx in (("L", -1), ("R", 1)):
        leg = cyl(f"leg{side}", 0.048, 0.20, m_fur)
        put(f"leg{side}", leg, (0, 0, -0.10), (sx * 0.085, 0.0, 0.30))
        foot = sphere(f"foot{side}", 0.085, m_fur, scale=(1.0, 1.8, 0.55))
        put(f"foot{side}", foot, (0, 0, 0.0), (sx * 0.105, -0.055, 0.048))

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

    # arms + mitten hands (mesh hangs DOWN from the shoulder joint; the hand is
    # parented to the arm so it rides along when the arm swings)
    for side, sx in (("L", -1), ("R", 1)):
        arm = cyl(f"arm{side}", 0.050, 0.17, m_fur)
        arm_o = put(f"arm{side}", arm, (0, 0, -0.085),
                    (sx * 0.245, 0.0, 0.575),
                    rot=(0, math.radians(-sx * 22), 0))
        hand = sphere(f"hand{side}", 0.062, m_fur)
        hand_o = put(f"hand{side}", hand, (0, 0, 0),
                     (sx * 0.315, 0.0, 0.395))
        bpy.context.view_layer.update()
        hand_o.parent = arm_o
        hand_o.matrix_parent_inverse = arm_o.matrix_world.inverted()

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

    # red W emblem on the cap front, built from four slanted bars: a font glyph
    # subdivides into a blurry smudge, real geometry stays crisp at any angle.
    for i, (dx, ang) in enumerate(((-0.055, -0.50), (-0.018, 0.50),
                                   (0.018, -0.50), (0.055, 0.50))):
        bar = box(f"Wbar{i}", (0.021, 0.032, 0.120), m_red)
        put(f"Wbar{i}", bar, (0, 0, 0), (dx, -0.252, 0.848), rot=(0, ang, 0))

    # dot eyes + mouth
    for side, sx in (("L", -1), ("R", 1)):
        eye = sphere(f"eye{side}", 0.024, m_dark)
        put(f"eye{side}", eye, (0, 0, 0), (sx * 0.080, -0.196, 0.800))
    mouth = sphere("mouth", 0.032, m_dark, scale=(2.0, 0.5, 0.7))
    put("mouth", mouth, (0, 0, 0), (0.0, -0.212, 0.724))


# ── animation drivers: frame -> ABSOLUTE joint eulers ─────────────────────
# Rest yaw values matter (arms/ears splay outward), so animations state whole
# eulers rather than deltas -- additive Euler deltas produced a sideways wave.
EAR_L_YAW = math.radians(-26)
EAR_R_YAW = math.radians(26)
ARM_L_YAW = math.radians(16)
ARM_R_YAW = math.radians(-16)


def _apply(rot=None, loc=None):
    """Reset every part to rest, then set absolute eulers / apply bob.

    Bobbing skips parented parts: they inherit their parent's motion."""
    for name, (obj, rest_loc, rest_rot) in PARTS.items():
        obj.rotation_euler = rest_rot
        obj.location = rest_loc
    if rot:
        for n, e in rot.items():
            PARTS[n][0].rotation_euler = e
    if loc:
        for n, d in loc.items():
            o = PARTS[n][0]
            if o.parent is not None:
                continue
            o.location = PARTS[n][1] + Vector(d)


def anim_idle(t):
    s = math.sin(t * 2 * math.pi)
    _apply(rot={"earL": (s * 0.18, EAR_L_YAW, 0),
                "earR": (-s * 0.18, EAR_R_YAW, 0)},
           loc={n: (0, 0, s * 0.020) for n in PARTS})


def anim_walk(t):
    s = math.sin(t * 2 * math.pi)
    c = math.cos(t * 2 * math.pi)
    _apply(rot={"legL": (s * 0.85, 0, 0), "legR": (-s * 0.85, 0, 0),
                "footL": (-s * 0.45, 0, 0), "footR": (s * 0.45, 0, 0),
                "armL": (-s * 0.60, ARM_L_YAW, 0),
                "armR": (s * 0.60, ARM_R_YAW, 0),
                "torso": (0.10, 0, 0), "head": (-0.06, 0, 0)},
           loc={n: (0, 0, abs(c) * 0.032) for n in PARTS})


def anim_wave(t):
    s = math.sin(t * 2 * math.pi * 2)
    _apply(rot={"armR": (math.radians(168), ARM_R_YAW,
                       math.radians(18 + s * 24)),
                "head": (0, math.radians(-8), 0)},
           loc={n: (0, 0, abs(math.sin(t * 2 * math.pi)) * 0.016)
                for n in PARTS})


def anim_celebrate(t):
    j = abs(math.sin(t * 2 * math.pi))
    _apply(rot={"armL": (math.radians(170), ARM_L_YAW, math.radians(-20)),
                "armR": (math.radians(170), ARM_R_YAW, math.radians(20)),
                "earL": (0.25, EAR_L_YAW, 0),
                "earR": (-0.25, EAR_R_YAW, 0)},
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
