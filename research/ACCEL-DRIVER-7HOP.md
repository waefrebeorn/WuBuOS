# ACCEL-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux NPU/accelerator driver gaps

"AI PCs" ship an on-die NPU. WuBuOS routes the accelerator through the
Linux accel subsystem per vendor.

### NPU driver routing (wubu_accel.c)

| Vendor | Driver | Node |
|--------|--------|------|
| Intel (IVPU, Meteor Lake+) | `ivpu` | /dev/accel/accel0 |
| AMD (XDNA, Ryzen AI) | `amdxdna` | /dev/accel/accel0 |
| Qualcomm (Hexagon NPU) | `qaic` | /dev/accel/accel0 |
| Google (TPU/Edge) | `edgetpu` | /dev/edgetpu |

### Kernel summary line

```
accel[present=0 npu=0(none) dsp=0 drv=none]
```

Published to `/kv/world/hw_accel` by `wubu_accel_summary()`.
