# FC-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux ethernet flow control gaps

Flow control (pause frames + PFC) stops the sender when RX overflows.

### FC routing (wubu_fc.c)

| Capability | Role |
|------------|------|
| pause frames | 802.3x PAUSE |
| PFC | 802.1Qbb priority flow control |
| autoneg | pause autonegotiation |
| ethtool | ethtool -A |

### Mode routing

| Mode | Routing |
|------|---------|
| rx | `rx-pause` |
| tx | `tx-pause` |
| both | `both-pause` |
| pfc | `pfc` |

### Autoneg routing

| Setting | Routing |
|---------|---------|
| on | `on` |
| off | `off` |

### Kernel summary line

```
fc[fc=1 pause=1 pfc=0 autoneg=1 ethtool=1 drv=pause]
```

Published to `/kv/world/hw_fc` by `wubu_fc_summary()`.

**Verified live:** this host reports `fc=1 pause=1 autoneg=1`.
