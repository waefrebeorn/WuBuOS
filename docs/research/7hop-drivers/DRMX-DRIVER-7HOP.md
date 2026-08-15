# DRMX-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux DRM writeback/overlay + HDR/color gaps

Advanced display features beyond base KMS.

### Capabilities (wubu_drmx.c)
- **DRM writeback**: capture composed frames (screen capture, vkms)
- **Overlays/planes**: hardware video/cursor/scaled planes
- **HDR** (HDR10/HLG): output metadata (PQ transfer, SDR luminance)
- **Color management**: gamma LUT, CTM, color pipeline

### Writeback driver routing

| GPU | Driver |
|-----|--------|
| vkms (virtual) | `vkms` |
| AMD | `amdgpu-writeback` |
| Intel | `i915-writeback` |
| msm/vc4 | `drm-writeback` |

### HDR modes
HDR10, HLG, PQ, BT.2020, SDR.

### Kernel summary line

```
drmx[writeback=0 overlay=0 hdr=0 color=0 vkms=0 drv=drm-kms]
```

Published to `/kv/world/hw_drmx` by `wubu_drmx_summary()`.
