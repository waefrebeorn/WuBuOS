# CALIB-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux display calibration gaps

Calibration adjusts brightness/gamma/color so output matches intent.

### Calibration routing (wubu_calib.c)

| Capability | Driver |
|------------|--------|
| DRM color mgmt | `drm-color` |
| DDC/CI monitor | `ddc-ci` |
| Gamma/LUT | `gamma-lut` |
| ICC profile | `icc-profile` |
| colord | `colord` |

### Components
- drm_color_mgmt: CRTC gamma/degamma/CTM LUTs
- ddcutil: DDC/CI monitor control (i2c)
- gamma: xgamma, colord color profiles
- backlight: sysfs brightness curve

### Kernel summary line

```
calib[drm=1 ddc=1 gamma=0 colord=0 icc=1 drv=drm-color]
```

Published to `/kv/world/hw_calib` by `wubu_calib_summary()`.

**Verified live:** this host reports `drm=1 ddc=1 icc=1`.
