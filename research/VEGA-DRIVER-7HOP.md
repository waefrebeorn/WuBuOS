# VEGA-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: AMD GCN5 Vega gaps

AMD Vega (RX Vega 56/64, GCN5) binds amdgpu + RADV Vulkan in Mesa.
Phoronix: "RADV re-enabled Vega support" (RX Vega 56/64). HN:
"AMD officially drops Vulkan for Polaris/Vega" but Mesa RADV
continues. Vega uses HBM2 memory.

### Impl routing (wubu_vega.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

GCN5 Vega. amdgpu kernel + RADV Vulkan (Mesa continues after AMD
drops own Vulkan). HBM2 memory. RX Vega 56/64 confirmed.
