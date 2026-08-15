# BLKQOS-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux blk-QoS throttling gaps

Block IO QoS throttle limits disk I/O via cgroup (io.max/io.weight).

### Mode routing (wubu_blkqos.c)

| Mode | Routing |
|------|---------|
| latency | `latency` |
| cost_model | `cost-model` |
| throttle / throt | `throttle` |
| weight | `weight` |
| else | `throttle` |

### Unit routing

| Unit | Routing |
|------|---------|
| b/s / bytes | `bytes` |
| iops | `iops` |
| kbps | `kbps` |
| mbps | `mbps` |
| else | `bytes` |

### Kernel summary

```
blkqos[qos=0 throttle=0 weight=0 cg=0 limit=0 drv=none]
```

Published to `/kv/world/hw_blkqos`. (No /sys/fs/cgroup/io on WSL2.)
