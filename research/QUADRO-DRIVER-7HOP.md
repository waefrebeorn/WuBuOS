# QUADRO-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: NVIDIA Quadro gaps

NVIDIA Quadro professional GPUs bind nvidia 535/550/590 driver —
same kernel driver as GeForce, but Quadro gets ISV certifications
(Autodesk, Adobe, etc). NVIDIA: "Professional Workstations Software."
NVIDIA 535.104.05 (Aug 2023). NVIDIA: ISV certifications page.

### Impl routing (wubu_quadro.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Quadro = professional tier. Same nvidia driver, ISV certified.
535/550/590 on Linux. NVIDIA product = workstation GPUs.
