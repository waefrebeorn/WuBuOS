# STORAGESCHED-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage scheduler gaps

Storage scheduler (mq-deadline, BFQ, kyber) orders block I/O requests.

### Scheduler routing (wubu_storagesched.c)

| Scheduler | Routing |
|-----------|---------|
| mq-deadline / mq_deadline | `mq-deadline` |
| bfq | `bfq` |
| deadline | `deadline` |
| cfq | `cfq` |
| kyber | `kyber` |
| none | `none` |
| else | `mq-deadline` |

### Mode routing

| Mode | Routing |
|------|---------|
| mq / multi | `multi-queue` |
| single | `single-queue` |
| else | `multi-queue` |

### Kernel summary

```
storagesched[ss=0 mq=0 bfq=0 deadline=0 none=0 drv=none]
```

Published to `/kv/world/hw_storagesched`. (No /sys/block schedulers on WSL2.)
