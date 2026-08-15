# IOSCHED-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage I/O scheduler gaps

The I/O scheduler (elevator) orders block requests for throughput + latency.

### Algo routing (wubu_iosched.c)

| Algorithm | Routing |
|-----------|---------|
| deadline | `mq-deadline` |
| kyber | `kyber` |
| bfq | `bfq` |
| noop / none | `none` |
| cfq | `cfq` |
| else | `mq-deadline` |

### Mode routing

| Mode | Routing |
|------|---------|
| wrr / fair | `wrr` |
| fifo | `fifo` |
| prio | `priority` |
| else | `fifo` |

### Kernel summary

```
iosched[sched=0 mq=0 deadline=0 kyber=0 bfq=0 drv=none]
```

Published to `/kv/world/hw_iosched`. (No /sys/block on WSL2.)
