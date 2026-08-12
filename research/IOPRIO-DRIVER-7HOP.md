# IOPRIO-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux I/O priority gaps

I/O priority assigns class (RT/BE/IDLE) + level to block requests.

### Class routing (wubu_ioprio.c)

| Class | Routing |
|-------|---------|
| be / best | `be` |
| rt / realtime | `rt` |
| idle | `idle` |
| none | `none` |
| else | `be` |

### Scheduler routing

| Scheduler | Routing |
|-----------|---------|
| mq-deadline | `mq-deadline` |
| deadline | `deadline` |
| cfq | `cfq` |
| bfq | `bfq` |
| kyber | `kyber` |
| noop | `noop` |
| else | `mq-deadline` |

### Kernel summary

```
ioprio[iop=0 rt=0 be=0 idle=0 sched=0 drv=none]
```

Published to `/kv/world/hw_ioprio`. (No /sys/block or ionice on WSL2.)
