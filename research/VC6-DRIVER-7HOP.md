# VC6-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Broadcom VideoCore VI gaps

VideoCore VI (RPi4/5) uses dual kernel driver: vc4 (display) +
v3d (3D/render). RPi forums: "v3d does 3D, vc4 does rendering."
FOSDEM 2024: vc4/v3d/v3dv on Pi4/5, OpenGL ES 3.1 & Vulkan 1.2
conformant, Non-Conformant OpenGL 3.1.

### Impl routing (wubu_vc6.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

vc4 = rendering, v3d = 3D. v3dv = Vulkan 1.2.
