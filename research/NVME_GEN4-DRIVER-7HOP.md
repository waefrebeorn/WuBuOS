# NVME_GEN4-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux NVMe Gen4 gaps

NVMe Gen4 supports up to 16 GT/s per lane (64 GT/s x4).
Lower latency and higher IOPS vs Gen3, with reduced power.

### Impl routing (wubu_nvme_gen4.c)

| Route | Path |
|-------|------|
| NVMe device presence | /sys/class/nvme/nvme0/device/uevent |
| Max HW sectors metric   | /sys/block/nvme0n1/queue/max_hw_sectors_kb |

Speed = 16 * lanes. Fast >= 64 GT/s (Gen4 x4).
