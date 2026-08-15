# QOS-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Ethernet switch QoS/ACL gaps

Modern NICs + switch ASICs offload QoS + ACL to hardware via tc.

### QoS/ACL offload (wubu_qos.c)
- tc flower: match IP/ports/flow -> offload (hw_tc)
- Rate shaping: htb/tbf/sfq qdiscs
- Policing: police/ingress
- DSCP/priority: prio, RED/ECN (explicit congestion)

### QoS driver routing

| ASIC | Driver |
|------|--------|
| Mellanox Spectrum | `mlxsw` |
| Microsemi Ocelot/Felix | `felix` |
| Marvell mv88e6xxx | `mv88e6xxx` |
| Microchip KSZ | `ksz` |

### Kernel summary line

```
qos[tc=1 offload=0 flower=0 shaping=1 ecn=1 drv=tc]
```

Published to `/kv/world/hw_qos` by `wubu_qos_summary()`.

**Verified live:** this host reports `tc=1 shaping=1 ecn=1`.
