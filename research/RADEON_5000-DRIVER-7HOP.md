# RADEON_5000-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: AMD Radeon HD 5000 gaps

Radeon HD 5000 (Evergreen, pre-GCN) binds the legacy radeon
driver, folded into amdgpu from Linux 6.19. Evergreen support
extended from R600 drivers (Phoronix: AMD Radeon HD 5000/6000
series open-source).

### Impl routing (wubu_radeon_5000.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Evergreen = family 1. Legacy radeon supports when available.
