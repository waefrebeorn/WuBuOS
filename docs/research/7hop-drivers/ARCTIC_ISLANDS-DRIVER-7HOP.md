# ARCTIC_ISLANDS-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: AMD GCN4 Arctic gaps

AMD GCN4 Arctic Islands (RX 480/580) binds amdgpu + RADV Vulkan 1.4
in Mesa. Mesa docs: "GFX8 and newer (GCN 3-5 and RDNA): Vulkan 1.4."
RX 580 8GB confirmed. Reddit r/linux_gaming: Vulkan via RADV.

### Impl routing (wubu_arctic_islands.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

GCN4 Arctic Islands (RX 4xx/5xx). amdgpu kernel + RADV Vulkan 1.4.
Mesa supports GCN3-5 + RDNA with Vulkan 1.4.
