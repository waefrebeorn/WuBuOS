# CMB-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux NVMe CMB gaps

CMB (Controller Memory Buffer) is on-controller DRAM for NVMe queues
(NVMe 1.3+); NVMe 2.0 adds PMICM.

### CMB routing (wubu_cmb.c)

| Register | Routing |
|----------|---------|
| cap1 | `cap1` |
| cap2 | `cap2` |
| qbr | `qbr` |
| sqs | `sqs` |
| cqs | `cqs` |
| unknown | `cmb-reg` (fallback) |

| Queue | Routing |
|-------|---------|
| sq | `submission-queue` |
| cq | `completion-queue` |
| unknown | `queue` (fallback) |

### Kernel summary

```
cmb[cmb=0 nvme=0 qmem=0 pmicm=0 squeue=0 drv=none]
```

Published to `/kv/world/hw_cmb`. (No NVMe device on WSL2.)
