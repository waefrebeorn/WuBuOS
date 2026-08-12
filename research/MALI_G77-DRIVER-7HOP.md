# MALI_G77-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: ARM Mali G77 gaps

Mali-G77 (valhall) binds Panfrost + PanVK Vulkan. Arm officially
backs Panfrost (collabora). PanVK is the Vulkan implementation.

### Impl routing (wubu_mali_g77.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

G77 = valhall. Panfrost GLES; PanVK Vulkan (non-conformant).
