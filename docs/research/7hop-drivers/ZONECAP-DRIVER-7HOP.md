# ZONECAP-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux ZNS zone capacity gaps

ZNS zones have a write pointer that must stay within capacity.
Zone capacity differs from zone size (sparse zones allowed).

### Impl routing (wubu_zonecape.c)

| Route | Path |
|-------|------|
| Zone control type    | /sys/block/nvme0n1/queue/zoned |
| Zone capacity metric | /sys/block/nvme0n1/queue/zoned_capacity |

Reset valid only when wp >= cap (full) or zone offline.
Write safe when wp + len <= capacity and len > 0.
