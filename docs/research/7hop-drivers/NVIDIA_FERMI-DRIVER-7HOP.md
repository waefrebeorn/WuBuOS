# NVIDIA_FERMI-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: NVIDIA Fermi legacy gaps

NVIDIA Fermi (GTX 4xx/5xx) binds nvidia legacy 470.xx driver.
470.256.02 is EOL since June 2024. Nouveau provides reverse-
engineered fallback for Fermi. Mesa supports Fermi via llvmpipe.

### Impl routing (wubu_nvidia_fermi.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

470.xx legacy EOL June 2024. Nouveau reverse-engineered fallback.
NVK does NOT support Fermi. Mesa + llvmpipe fallback only.
