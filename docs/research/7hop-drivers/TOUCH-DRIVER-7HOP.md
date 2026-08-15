# TOUCH-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux touchscreen/trackpad driver gaps

Touch is how laptops/tablets/AIOs take input. WuBuOS routes every family.

### Touch/trackpad routing (wubu_touch.c)

| Device | Driver |
|--------|--------|
| Elan (trackpads) | `elan_i2c` |
| Synaptics RMI4 | `rmi4` |
| ALPS | `alps` |
| Wacom (tablets) | `wacom` |
| Goodix (touchscreens) | `goodix_ts` |
| Cypress (Surface) | `cypress-sf` |
| Silead | `silead` |
| Generic HID MT | `hid-multitouch` |

### Kernel summary line

```
touch[touch=0 elan=0 synaptics=0 mt=0 wacom=0 drv=none]
```

Published to `/kv/world/hw_touch` by `wubu_touch_summary()`.
