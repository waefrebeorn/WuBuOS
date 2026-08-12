# ADRENO700-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Qualcomm Adreno 700 gaps

Adreno 700 (7xx series) binds the free-software freedreno driver.
Mainlined in upstream Mesa/DRM. Supports Vulkan 1.3+ via turnip.

### Impl routing (wubu_adreno700.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Vendor 0x5143 (Qualcomm). Gen 7 = Adreno 7xx.
