# PERF-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU performance counter gaps

GPU perf counters monitor shader/active cycles, memory bandwidth,
and cache hits via DRM_IOC_PERF_* ioctls.

### Impl routing (wubu_perf.c)

| Metric | Source |
|--------|--------|
| GPU boost freq   | /sys/class/drm/card0/device/gt_boost_freq_mhz |
| Turbo flag       | /sys/class/drm/card0/device/turbo_boost_enable |
| Per-engine stats | /sys/class/drm/card0/device/engine_* |
| VRAM usage       | /sys/class/drm/card0/device/mem_info_vram_* |

Engines: render, blitter, decode, encode, copy, video.
