# HIDADV-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux USB HID advanced driver gaps

HID connects keyboards/mice/gamepads/touchscreens. This module routes the
HID driver (complements wubu_hid.c, the unified HID event layer).

### HID driver routing (wubu_hidadv.c)

| Vendor | Driver |
|--------|--------|
| Logitech | `hid-logitech-dj` |
| Apple | `hid-apple` |
| Sony/PS | `hid-sony` |
| Xbox | `hid-xboxone` |
| Steam | `hid-steam` |
| Multitouch | `hid-multitouch` |
| generic | `hid-generic` |

### Components
- hid-core: HID bus + report descriptor parsing
- hid-generic: the generic driver (most devices)
- hid-ff: force feedback; HIDIOC userspace ioctl

### Kernel summary line

```
hidadv[hid=0 generic=0 mt=0 ff=0 vendor=0 drv=none]
```

Published to `/kv/world/hw_hidadv` by `wubu_hidadv_summary()`.
