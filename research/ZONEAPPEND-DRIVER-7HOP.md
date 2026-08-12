# ZONEAPPEND-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux ZNS zone append gaps

ZNS (Zoned Namespaces) exposes SSDs as zones that must be written
sequentially via zone append commands for application-managed placement.

### Impl routing (wubu_zoneappend.c)

| Route | Path |
|-------|------|
| Zone type presence | /sys/block/nvme0n1/queue/zoned |
| Zone device          | /proc/mdstat |

Zone append valid only in sequential zones (active/implicit/explicit open).
States: offline(0), active(1), implicit_open(2), explicit_open(3),
closed(4), read_only(5), full(6).
