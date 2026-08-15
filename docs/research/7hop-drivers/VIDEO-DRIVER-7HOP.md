# VIDEO-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux video codec/VA-API driver gaps

Hardware video acceleration (H.264/H.265/VP9/AV1) offloads to GPU/media
blocks. WuBuOS routes the codec engine.

### VA-API/VDPAU/m2m routing (wubu_video.c)

| GPU | Driver |
|-----|--------|
| Intel (iHD/i965) | `iHD` |
| AMD (radeonsi) | `radeonsi` |
| NVIDIA (vdpau) | `vdpau` |
| Qualcomm (venus) | `venus` |
| Rockchip (rkvdec) | `rkvdec` |
| Broadcom/RPi | `rpi-hw-accel` |
| embedded | `v4l2-m2m` |

### Codecs
AV1 (royalty-free, modern), H.264/HEVC, VP8/VP9, MJPEG.

### Kernel summary line

```
video[vaapi=1 vdpau=0 m2m=0 av1=1 hevc=1 vp9=1 drv=vaapi eng=Intel iHD]
```

Published to `/kv/world/hw_video` by `wubu_video_summary()`.

**Verified live:** this host reports `vaapi=1 eng=Intel iHD` — real VA-API.
