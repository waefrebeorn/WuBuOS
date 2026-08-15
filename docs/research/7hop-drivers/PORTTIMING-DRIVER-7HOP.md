# PORTTIMING-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux display port timing gaps

Port timing sets pixel clock / HTOTAL / VTOTAL + reduced blanking.

### Port-timing routing (wubu_porttiming.c)

| Standard | Routing |
|----------|---------|
| CVT | `cvt` |
| CVT-RB | `cvt-rb` |
| VESA | `vesa` |
| DMT | `dmt` |
| unknown | `gfx` (fallback) |

| Link rate | Routing |
|-----------|---------|
| RBR | `rbr` |
| HBR | `hbr` |
| HBR2 | `hbr2` |
| HBR3 | `hbr3` |
| UHBR | `uhbr` |
| eDP | `edp` |

### Kernel summary

```
porttiming[mode=1 cvt=1 rb=1 link=1 preferred=1 drv=drm-mode]
```

Published to `/kv/world/hw_porttiming`. Verified live on this host.
