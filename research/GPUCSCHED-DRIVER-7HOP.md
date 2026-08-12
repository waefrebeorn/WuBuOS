# GPUCSCHED-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU compute scheduler gaps

Compute queues feed async work to GPU compute engines; a scheduler
balances queue priority, preemption, and time-slice fairness.

### Impl routing (wubu_gpucsched.c)

| Route | Path |
|-------|------|
| Compute engine presence | /sys/class/drm card0/device |
| Compute render node       | /dev/dri renderD128 |

Timeslice grows with queue index: 8ms base + 4ms per queue.
