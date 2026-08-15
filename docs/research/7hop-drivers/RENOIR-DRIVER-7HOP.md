# RENOIR-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: AMD Raven/Renoir APU gaps

AMD Raven/Renoir APU (GCN 5.1) binds amdgpu + RADV Vulkan + Mesa
OpenGL 4.6. Gentoo Wiki: "supports Vulkan (RADV driver) and OpenGL."
TechPowerUp: Renoir = GCN 5.1, DirectX 12, 512 shading units.

### Impl routing (wubu_renoir.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

GCN 5.1 Renoir APU. amdgpu + RADV Vulkan 1.4 + OpenGL 4.6 (Mesa 25.1).
