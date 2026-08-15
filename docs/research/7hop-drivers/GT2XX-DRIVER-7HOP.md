# GT2XX-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: NVIDIA GT2xx legacy gaps

NVIDIA GT2xx (G8x/G9x) is legacy. Debian Wiki: 340.108 legacy driver
(EOL). Nouveau is the open-source fallback. Linux Mint forums:
"Nouveau is the open source driver for NVIDIA GPUs in Linux."
freedesktop: "Nouveau = accelerated open-source driver."

### Impl routing (wubu_gt2xx.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

GT2xx legacy. 340.108 EOL. Nouveau fallback via kernel nouveau driver.
