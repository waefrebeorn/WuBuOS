# UCODE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux CPU microcode loading gaps

Microcode is CPU firmware (early boot + late reload) that patches silicon
bugs + security.

### Microcode loader routing (wubu_ucode.c)

| CPU | Loader |
|-----|--------|
| Intel | `intel-ucode` |
| AMD | `amd-ucode` |
| Hygon | `amd-ucode` |

### Load paths

| Mode | Path |
|------|------|
| early | `initrd-early` |
| late | `dev-cpu-microcode` |

### Components
- intel-ucode / amd-ucode: early initrd load
- /dev/cpu/microcode: late reload
- /sys/devices/system/cpu/microcode: version + revision

### Kernel summary line

```
ucode[intel=0 amd=1 early=0 late=0 loaded=0 drv=amd-ucode]
```

Published to `/kv/world/hw_ucode` by `wubu_ucode_summary()`.

**Verified live:** this host reports `amd=1 drv=amd-ucode`.
