# GPUSHADER-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU shader model gaps

Shader model detection routes GPU shader cores to the correct
compiler backend (SPIR-V/GLSL/HLSL). ABI mismatch between shader
model and driver causes silent corruption.

### Impl routing (wubu_gpushader.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| Hardware revision   | /sys/class/drm/card0/device/hardware_rev |

Shader levels: legacy(0)=SM<5.0, baseline(1)=SM 5.x, advanced(2)=SM>=6.0.
