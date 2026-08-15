# VOLCANIC_ISLANDS-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: AMD GCN3 Volcanic gaps

AMD GCN3 Volcanic Islands (R9 285, R9 380/X) binds amdgpu + RADV
Vulkan in Mesa. Timur.hu 2025 EOY: "DC now supports analog connectors"
on VI. Linux 6.19 folds older GCN GPUs to amdgpu. ArchWiki: amdgpu
supports GCN 1.1 (Sea Islands) experimentally, folded to default.

### Impl routing (wubu_volcanic_islands.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

GCN3 Volcanic Islands. amdgpu kernel 6.19+. RADV Vulkan 1.3 (GCN 1-2)
/ Vulkan 1.4 (GCN 3+). DC analog connector support (2025).
