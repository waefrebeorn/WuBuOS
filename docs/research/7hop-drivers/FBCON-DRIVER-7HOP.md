# FBCON-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux framebuffer console gaps

fbcon provides text console on a GPU framebuffer via DRM/KMS.

### Rotation routing (wubu_fbcon.c)

| Rotation | Routing |
|----------|---------|
| normal / 0 | `normal` |
| left / 1 | `left` |
| upside / 2 | `upside-down` |
| right / 3 | `right` |
| else | `normal` |

### Mode routing

| Mode | Routing |
|------|---------|
| 800x600 | `800x600` |
| 1024x768 | `1024x768` |
| 1920x1080 | `1920x1080` |
| 2560x1440 | `2560x1440` |
| 3840x2160 | `3840x2160` |
| else | `1920x1080` |

### Kernel summary

```
fbcon[fb=0 drm=0 rotate=0 virtual=0 mode=0 drv=none]
```

Published to `/kv/world/hw_fbcon`. (No /sys/class/graphics on WSL2.)
