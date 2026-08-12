# FINGERPRINT-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux fingerprint/biometric driver gaps

Fingerprint readers secure login. WuBuOS routes them to the right driver
+ exposes biometric presence for the security layer (via libfprint/fprintd).

### Vendor routing (wubu_fingerprint.c)

| Vendor | Driver |
|--------|--------|
| Goodix | `goodixmoc` |
| Synaptics/Validity (VFS) | `vfs5011` |
| EgisTec | `egis` |
| AuthenTec | `authenc` |
| Fingerprint Cards | `fpc1020` |
| Elan | `elan-fp` |
| Other | `libfprint` |

### Kernel summary line

```
fp[present=0 goodix=0 vfs=0 egis=0 authenc=0 fpc=0 drv=none]
```

Published to `/kv/world/hw_fp` by `wubu_fingerprint_summary()`.
