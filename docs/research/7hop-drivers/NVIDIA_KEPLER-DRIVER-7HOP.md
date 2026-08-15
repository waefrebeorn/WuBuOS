# NVIDIA_KEPLER-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: NVIDIA Kepler legacy gaps

NVIDIA Kepler (GTX 6xx/7xx) binds nvidia legacy 470.xx (EOL
June 2024) or Nouveau reverse-engineered driver. NVK enabled
for Maxwell+ (April 2025) but NOT Kepler. Phoronix: "Kepler
still needs 470 or Nouveau."

### Impl routing (wubu_nvidia_kepler.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

470.xx legacy EOL. NVK no Kepler support (gen=1). Nouveau is
fallback. Mesa supports Kepler via Nouveau + llvmpipe.
