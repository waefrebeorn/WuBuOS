# NVME_GEN5-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux NVMe Gen5 gaps

NVMe Gen5 supports up to 64 GT/s per lane (256 GB/s for x4).
Higher bandwidth requires careful thermal and power management.

### Impl routing (wubu_nvme_gen5.c)

| Route | Path |
|-------|------|
| NVMe device presence | /sys/class/nvme/nvme0/device/uevent |
| Max HW sectors metric   | /sys/block/nvme0n1/queue/max_hw_sectors_kb |

Speed = gen * lanes * base GT/s. Fast threshold: >= 256 GT/s.
