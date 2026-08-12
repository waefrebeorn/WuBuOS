# ZONSEQWRITE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux ZNS sequential write gaps

ZNS (Zoned Namespaces) zones require sequential writes within
each zone. Write pointer must not exceed zone capacity.

### Impl routing (wubu_zonseqwrite.c)

| Route | Path |
|-------|------|
| Zone control type    | /sys/block/nvme0n1/queue/zoned |
| Zone capacity metric | /sys/block/nvme0n1/queue/zoned_capacity |

Sequential write valid only in open zones (active/implicit/
explicit open), within capacity, with positive length.
Write safe when wp + len <= max_len and len > 0.
