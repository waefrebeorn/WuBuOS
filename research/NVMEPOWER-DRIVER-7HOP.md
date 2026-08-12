# NVMEPOWER-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux NVMe power state gaps

NVMe drives expose power states (PS0-PS4) and autonomous power state
transition (APST) for power-vs-latency tuning.

### Impl routing (wubu_nvmepower.c)

| Route | Path |
|-------|------|
| Power state presence | /sys/class/nvme nvme0/power |
| APST enable          | /sys/class/nvme nvme0/apst |

Power states clamped 0-4. APST latency clamped non-negative.
PS0 = active, PS1-3 = idle, PS4 = sleep.
