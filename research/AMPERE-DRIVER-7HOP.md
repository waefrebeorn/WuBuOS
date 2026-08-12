# AMPERE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: NVIDIA Ampere gaps

NVIDIA Ampere (RTX 30xx) binds nvidia 535/550/590 driver. NVIDIA
Developer: Vulkan 1.4 beta for Ampere (RTX 3060/3070/3080/3090).
RT cores (2nd gen). NVIDIA Forums: "Ampere and Ada same experience."
Reddit r/linux_gaming: ray tracing via Vulkan RT.

### Impl routing (wubu_ampere.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Ampere (sm_89 on RTX 40, sm_8x on RTX 30). RTX 30xx. Vulkan 1.4,
RT cores, Vulkan RT extensions. nvidia 535/550/590.
