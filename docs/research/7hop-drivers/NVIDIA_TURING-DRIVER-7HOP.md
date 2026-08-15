# NVIDIA_TURING-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: NVIDIA Turing gaps

NVIDIA Turing (RTX 20xx) binds nvidia 535/550/590. Vulkan, OpenGL
4.6, RT cores (sm_75). Turing still supported on 550/590. Reddit:
"NVIDIA 590 drops GTX 900" but Turing remains supported.

### Impl routing (wubu_nvidia_turing.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Turing = 2nd gen RT (sm_75). Vulkan 1.3+. NVK supports Turing
(Maxwell+ April 2025). 590 drops GTX 900 but Turing stays.
