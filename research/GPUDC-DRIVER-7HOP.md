# GPUDC-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU display controller gaps

GPU display controller (DC) binds output to displays via KMS/DRM.
DC handles connector status, modeset, and output routing.

### Impl routing (wubu_gpudc.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| Connector status       | /sys/class/drm/card0/status |

Types: none(0), single(1), multi(2), wide multi(3).
