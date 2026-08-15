# RADEON_LEGACY-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: AMD Radeon legacy GPU gaps

AMD Radeon HD 5000/6000 (Evergreen/Northern Islands, pre-GCN)
bind the legacy `radeon` driver. Linux 6.3 dropped obsolete
DRM drivers (mga, r128, savage, sis, tdfx, via, i810); 6.19
folds pre-GCN radeon hardware into amdgpu (~30% perf boost).

### Impl routing (wubu_radeon_legacy.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Family: 1=Evergreen(HD5000), 2=NI(HD6000). Both fold into amdgpu 6.19.
