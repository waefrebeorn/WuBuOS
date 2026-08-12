# VPUDECODE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU video decode gaps

GPU video decode (VA-API) routes compressed video streams
(H.264/H.265/AV1/VP9/VP8) to GPU decode engines. ABI mismatch
between codec and driver causes silent corruption.

### Impl routing (wubu_vpudecode.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| DRM subsystem          | /sys/class/drm/card0/device/drm |

Codecs: H.264(0), H.265(1), AV1(2), VP9(3), VP8(4).
