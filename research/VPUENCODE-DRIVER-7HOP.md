# VPUENCODE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU video encode gaps

GPU video encode (VA-API/VAAPI) routes raw video streams
to GPU encode engines (H.264/H.265/AV1). ABI mismatch between
codec and driver causes silent corruption.

### Impl routing (wubu_vpuencode.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| DRM subsystem          | /sys/class/drm/card0/device/drm |

Codecs: H.264(0), H.265(1), AV1(2).
Bitrate = width * height * fps / 1000.
