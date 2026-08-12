# CAMERA-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux V4L2 camera/ISP driver gaps

Cameras feed video calls, streaming, and AGI vision. WuBuOS routes the
pipeline (sensor -> ISP -> video node) per device.

### Pipeline driver routing (wubu_camera.c)

| Device | Driver |
|--------|--------|
| USB webcam | `uvcvideo` |
| Rockchip ISP | `rkisp1` |
| MIPI-CSI sensor imx219 | `imx219` |
| MIPI-CSI sensor imx290 | `imx290` |
| MIPI-CSI sensor ov5640 | `ov5640` |
| MIPI-CSI sensor ov9281 | `ov9281` |
| NXP i.MX8 ISI | `imx8-isi` |
| Virtual media controller | `vimc` |
| Virtual test driver | `vivid` |

### Kernel summary line

```
camera[v4l2=0 uvc=0 isp=0 mipi=0 video=0 media=0 drv=none]
```

Published to `/kv/world/hw_camera` by `wubu_camera_summary()`.
