# ICELAKE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Intel Gen11 Ice Lake gaps

Intel Gen11 Ice Lake binds i915 kernel driver + Iris Mesa driver.
ANV provides Vulkan 1.2. ArchWiki: Gen11 Ice Lake supported by
i915 + Iris/ANV. Fedora: both i915 and xe kernel modules load.

### Impl routing (wubu_intel_icelake.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Gen11 Ice Lake (11th gen). i915 kernel driver + Iris Mesa + ANV Vulkan 1.2.
