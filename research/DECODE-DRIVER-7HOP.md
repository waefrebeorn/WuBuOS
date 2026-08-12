# DECODE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU decode gaps

GPU decode (VCN/UVD/NVDEC/QSV) handles hardware-accelerated video decode.

### Codec routing (wubu_decode.c)

| Codec | Routing |
|-------|---------|
| h264 / avc | `H.264` |
| h265 / hevc | `H.265` |
| av1 | `AV1` |
| vp9 | `VP9` |
| mpeg | `MPEG` |
| else | `H.264` |

### API routing

| API | Routing |
|-----|---------|
| uvd / vcn / amd | `VCN` |
| qsv / intel | `QuickSync` |
| nvdec / nvidia | `NVDEC` |
| v4l2 | `V4L2` |
| else | `VCN` |

### Kernel summary

```
decode[dec=0 h264=0 h265=0 av1=0 vp9=0 drv=none]
```

Published to `/kv/world/hw_decode`. (No drm/amdgpu/i915 on WSL2.)
