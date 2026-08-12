# SKYLAKE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Intel Gen9 Skylake gaps

Intel Gen9 Skylake binds i915 kernel driver + Iris Mesa driver.
ANV provides Vulkan 1.2. ArchWiki: Gen9 Skylake supported by
i915 + Iris. mesa-amber for older GL2.1.

### Impl routing (wubu_intel_skylake.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Gen9 Skylake. i915 kernel driver + Iris Mesa (Gallium). ANV Vulkan 1.2.
