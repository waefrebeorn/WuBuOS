# NVIDIA_MAXWELL-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: NVIDIA Maxwell legacy gaps

NVIDIA Maxwell (GTX 9xx) binds nvidia 535/580 driver. NVIDIA
590 drops GeForce GTX 900 support. NVK enabled for Maxwell
April 2025 (Collabora). Reddit: "NVIDIA 590 drops GTX 900."
Mesa supports Maxwell via Nouveau + NVK (Vulkan 1.4).

### Impl routing (wubu_nvidia_maxwell.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

590 drops GTX 900. Need 535+. NVK enabled April 2025. Nouveau
reverse-engineered fallback. Mesa + NVK Vulkan 1.4 support.
