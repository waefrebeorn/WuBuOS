# COLORMGMT-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux display color management gaps

Color management sets CTM/GAMMA/DEGAMMA LUT + CSC for correct color.

### Color-mgmt routing (wubu_colormgmt.c)

| Component | Role |
|-----------|------|
| DRM CTM | color transform matrix |
| GAMMA / DEGAMMA | 1-D LUT |
| 3-D LUT / CSC | colorspace conversion |
| amdgpu dm | AMD color mgmt |
| i915 cdclk | Intel color |

### LUT routing

| LUT | Routing |
|-----|---------|
| gamma | `gamma-lut` |
| degamma | `degamma-lut` |
| 3d | `3d-lut` |
| ctm | `ctm` |

### CSC routing

| Colorspace | Routing |
|------------|---------|
| bt709 | `bt709` |
| bt2020 | `bt2020` |
| rgb | `rgb` |
| ycbcr | `ycbcr` |

### Kernel summary line

```
colormgmt[ctm=0 gamma=0 degamma=0 csc=0 lut3d=0 drv=none]
```

Published to `/kv/world/hw_colormgmt` by `wubu_colormgmt_summary()`.
