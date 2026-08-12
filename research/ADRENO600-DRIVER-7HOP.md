# ADRENO600-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Qualcomm Adreno 600 gaps

Adreno 600 (a6xx) binds freedreno + Turnip Vulkan 1.3. Mesa 26+
supports 6xx/7xx/8xx. Turnip is the Vulkan 1.3 driver for a6xx.

### Impl routing (wubu_adreno600.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Vendor 0x5143. Gen6 = a6xx. Turnip Vulkan 1.3, GLES 3.2.
