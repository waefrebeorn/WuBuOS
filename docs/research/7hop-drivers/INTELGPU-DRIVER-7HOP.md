# INTELGPU-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Intel GPU routing gaps

Intel integrated graphics binds across Broadwell/Gen8 through
Xe2/Arc generations. Driver selection (i915/iris vs xe/anv)
is generation-dependent.

### Impl routing (wubu_intelgpu.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Driver: Gen7=i915 legacy, Gen8-9=i915/iris, Gen10=i915/xe, Gen12+=xe/anv.
Gen12+ requires GuC/HuC firmware.
