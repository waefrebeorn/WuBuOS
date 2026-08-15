# RADEON_6000-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: AMD Radeon HD 6000 gaps

Radeon HD 6000 (Northern Islands, pre-GCN) binds the legacy
radeon driver, folded into amdgpu from Linux 6.19. Without
amdgpu (pre-6.19), NI needs the legacy radeon driver.

### Impl routing (wubu_radeon_6000.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Needs legacy when amdgpu support absent. NI = pre-GCN (family 2).
