# THERMALTHROTTLE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux thermal throttling gaps

Thermal throttling (CPU/GPU thermal zone) reduces frequency at high temp.

### Governor routing (wubu_thermalthrottle.c)

| Governor | Routing |
|----------|---------|
| step | `step_wise` |
| fair | `fair_share` |
| user | `user_space` |
| bang | `bang_bang` |
| else | `step_wise` |

### Trip routing

| Trip | Routing |
|------|---------|
| crit | `critical` |
| hot | `hot` |
| pass | `passive` |
| active | `active` |
| else | `passive` |

### Kernel summary

```
thermalthrottle[throttle=0 zone=0 cooling=0 trip=0 governor=0 drv=none]
```

Published to `/kv/world/hw_thermalthrottle`. (No /sys/class/thermal on WSL2.)
