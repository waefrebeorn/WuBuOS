# WRITEBACK-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage writeback gaps

Writeback flushes dirty pages from memory to storage via kernel threads.

### Mode routing (wubu_writeback.c)

| Mode | Routing |
|------|---------|
| async / background | `async` |
| sync | `sync` |
| periodic | `periodic` |
| else | `async` |

### Thread routing

| Component | Routing |
|-----------|---------|
| writeback | `writeback/N` |
| flush | `flush-` |
| jbd | `jbd` |
| ext4 | `ext4` |
| else | `writeback/N` |

### Kernel summary

```
writeback[wb=0 dirty=0 sync=0 interval=0 thread=0 drv=none]
```

Published to `/kv/world/hw_writeback`. (No /proc/meminfo on WSL2.)
