# GPUKMS-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU KMS modeset gaps

KMS (Kernel Mode Setting) routes display modesetting to the
GPU kernel driver. Correct modeset routing prevents screen
corruption and unsupported resolution errors.

### Impl routing (wubu_gpukms.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| Connector status       | /sys/class/drm/card0/status |

Mode valid: resolution >= 640x480, refresh 30-240Hz.
Active: CRTC active AND connector connected.
