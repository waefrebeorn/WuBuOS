# MALI_G52-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: ARM Mali G52 gaps

Mali-G52 binds the Panfrost open-source driver. Arm officially
backed Panfrost (long-term). Mesa Panfrost supports G52 with
OpenGL ES 3.2.

### Impl routing (wubu_mali_g52.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

G52 supports up to OpenGL ES 3.2.
