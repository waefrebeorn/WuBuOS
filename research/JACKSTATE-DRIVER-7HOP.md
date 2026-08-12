# JACKSTATE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio jack state machine gaps

Jack state machine tracks plug/unplug insertion and removal events.

### State routing (wubu_jackstate.c)

| Input | Routing |
|-------|---------|
| unpluggin | `unplugging` |
| pluggin | `plugging` |
| unplug / removed | `unplugged` |
| plug / insert | `plugged` |
| bounc | `bounce` |
| else | `unplugged` |

### Event routing

| Event | Routing |
|-------|---------|
| out / remov / unplug | `plug_out` |
| in / insert / plug | `plug_in` |
| else | `plug_out` |

### Kernel summary

```
jackstate[js=0 plug=0 unplugg=0 debounce=0 stable=0 drv=none]
```

Published to `/kv/world/hw_jackstate`. (No /sys/class/switch on WSL2.)
