# PD-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux USB Power Delivery + NIC flow steering gaps

Two capabilities: USB PD (Type-C power negotiation) + NIC flow steering.

### USB PD routing (wubu_pd.c)

| Component | Driver |
|-----------|--------|
| Type-C port manager | `typec` |
| PD negotiation | `tcpm` |
| PD power supply | `tcpm-psy` / charger |

PD contracts: PDO/RDO negotiation, 5V-48V (up to 240W), source/sink/dual roles.
/sys/class/typec: port, partner, power role.

### Flow steering routing

| NIC | Offload |
|-----|---------|
| ixgbe | `ixgbe-arfs` |
| i40e  | `i40e-arfs`  |
| mlx5  | `mlx5-arfs`  |
| igc   | `igc-rfs`    |

RFS (receive flow steering), aRFS (accelerated RFS), ethtool -N flow rules.

### Kernel summary line

```
pd[typec=0 pd=1 tcpm=0 rfs=1 arfs=0 drv=none]
```

Published to `/kv/world/hw_pd` by `wubu_pd_summary()`.

**Verified live:** this host reports `pd=1 rfs=1`.
