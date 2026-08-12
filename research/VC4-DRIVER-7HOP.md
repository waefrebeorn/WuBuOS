# VC4-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Broadcom VideoCore gaps

Broadcom VideoCore (RPi) uses dual driver: vc4 (rendering) +
v3d (3D). RPi forums confirm: "v3d does 3D, vc4 does rendering."
Both load, both used. Igalia: V3D driver got OpenGL ES 3.1
conformance on RPi4.

### Impl routing (wubu_vc4.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

vc4 = rendering, v3d = 3D (OpenGL ES 3.1+).
