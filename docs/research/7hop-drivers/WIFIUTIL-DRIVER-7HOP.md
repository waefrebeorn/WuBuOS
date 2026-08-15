# WIFIUTIL-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux WiFi channel utilization gaps

Channel utilization measures CCA busy + airtime per channel.

### Band routing (wubu_wifiutil.c)

| Band | Routing |
|------|---------|
| 24 / 2g | `2.4ghz` |
| 5 | `5ghz` |
| 6 | `6ghz` |
| else | `2.4ghz` |

### State routing

| State | Routing |
|-------|---------|
| busy | `busy` |
| rx | `rx` |
| tx | `tx` |
| idle | `idle` |
| else | `unknown` |

### Kernel summary

```
wifiutil[util=0 cca=0 airtime=0 survey=0 chan=0 drv=none]
```

Published to `/kv/world/hw_wifiutil`. (No mac80211 on WSL2.)
