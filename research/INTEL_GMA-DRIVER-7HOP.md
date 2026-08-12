# INTEL_GMA-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Intel GMA legacy GPU gaps

Intel GMA (Graphics Media Accelerator) 3-series (G31/G45) bind
the i915 legacy driver. GMA 950 on 945GM confirmed working with
modesetting + llvmpipe (ctrl-alt-rees). i915 docs.kernel.org:
supports all but very early Intel integrated GFX chipsets.

### Impl routing (wubu_intel_gma.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Gen 3-5 (G31/G45/GMA950) = i915. No HW 3D accel = llvmpipe fallback.
