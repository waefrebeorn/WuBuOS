# GPUMEM-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: GPU memory bandwidth gaps

GPU memory bandwidth (GB/s) is measured to route memory-bound
compute kernels to the correct tier. ABI mismatch between
shader memory profile and driver causes silent corruption.

### Impl routing (wubu_gpumem.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| VRAM total           | /sys/class/drm/card0/device/mem_info_vram_total |

Bandwidth tiers: entry(<200), mid(<400), high(<800), ultra(>=800).
