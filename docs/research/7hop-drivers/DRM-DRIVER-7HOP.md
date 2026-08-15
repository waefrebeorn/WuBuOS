# DRM-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU DRM subsystem gaps

DRM (Direct Rendering Manager) manages GPU display + buffer management (KMS, GEM, PRIME).

### Subsys routing (wubu_drm.c)

| Subsys | Routing |
|--------|---------|
| amdgpu / amd | `amdgpu` |
| i915 / intel | `i915` |
| nouveau | `nouveau` |
| mgag200 / mga | `mgag200` |
| ast | `ast` |
| virt/virgl | `virtio-gpu` |
| bochs | `bochs` |
| else | `amdgpu` |

### Object routing

| Object | Routing |
|--------|---------|
| crtc | `CRTC` |
| connector | `Connector` |
| encoder | `Encoder` |
| plane | `Plane` |
| framebuffer | `Framebuffer` |
| gem | `GEM` |
| else | `Framebuffer` |

### Kernel summary

```
drm[drm=0 kms=0 gem=0 prime=0 msi=0 drv=none]
```

Published to `/kv/world/hw_drm`. (No /dev/dri on WSL2.)
