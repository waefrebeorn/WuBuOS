# ZONEFMT-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux ZNS zone reset gaps

ZNS (Zoned Namespaces) zones must be reset before reuse after
becoming full. Incorrect reset timing wastes write bandwidth.

### Impl routing (wubu_zonefmt.c)

| Route | Path |
|-------|------|
| Zone device presence | /sys/block/nvme0n1/queue/zoned |
| Zone state            | /proc/mdstat |

Reset valid only on offline(0) and full(6) zones; active/implicit/
explicit/ closed zones must close first.
