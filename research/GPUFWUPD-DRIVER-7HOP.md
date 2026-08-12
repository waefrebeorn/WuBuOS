# GPUFWUPD-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU firmware update gaps

GPU firmware (GDDR VBIOS + microcode) must be version-checked
before driver bind to prevent ABI mismatch crashes.

### Impl routing (wubu_gpufwupd.c)

| Route | Path |
|-------|------|
| VBIOS ROM presence | /sys/class/drm/card0/device/rom |
| Driver version      | /sys/class/drm/card0/device/uevent |

Vendor IDs: NVIDIA=0x10de, AMD=0x1002, Intel=0x8086.
Status: ok(0), stale(1), mismatch(2).
