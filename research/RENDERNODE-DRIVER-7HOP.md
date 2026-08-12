# RENDERNODE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU render node gaps

GPU render node (/dev/dri/renderD128) exposes the GPU for
compute/render tasks without display control. KMS is not
required for render node operations.

### Impl routing (wubu_rendernode.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| Render node presence | /sys/class/drm/renderD128 |

Priority levels: normal(1, 0-99), high(2, 100+), real-time(3, negative invalid).
