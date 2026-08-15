# ENCODE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU encode gaps

GPU encode (VCN/UVD/NVENC/QSV) handles hardware-accelerated video encoding.

### Codec routing (wubu_encode.c)

| Codec | Routing |
|-------|---------|
| h264 / avc | `H.264` |
| h265 / hevc | `H.265` |
| vp9 | `VP9` |
| av1 | `AV1` |
| mpeg | `MPEG` |
| else | `H.264` |

### API routing

| API | Routing |
|-----|---------|
| vcn / amd | `VCN` |
| qsv / intel | `QuickSync` |
| nvenc / nvidia | `NVENC` |
| v4l2 | `V4L2` |
| else | `VCN` |

### Kernel summary

```
encode[enc=0 h264=0 h265=0 vp9=0 av1=0 drv=none]
```

Published to `/kv/world/hw_encode`. (No drm/amdgpu/i915 on WSL2.)
