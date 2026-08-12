# BIO-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux block I/O bio gaps

Bio (block I/O) represents requests in the block layer.

### Op routing (wubu_bio.c)

| Op | Routing |
|----|---------|
| read | `READ` |
| write | `WRITE` |
| discard | `DISCARD` |
| flush | `FLUSH` |
| secure | `WRITE_SECURE` |
| else | `READ` |

### Layer routing

| Layer | Routing |
|-------|---------|
| block | `block` |
| bio | `bio` |
| iov | `iov` |
| mm | `mm` |
| else | `block` |

### Kernel summary

```
bio[bio=0 vec=0 bdi=0 read=0 write=0 drv=none]
```

Published to `/kv/world/hw_bio`. (No /sys/block on WSL2.)
