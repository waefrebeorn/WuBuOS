# WIFIREG-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux WiFi regulatory/DFS gaps

Regulatory domains define allowed channels + TX power by country; DFS
governs 5GHz radar channels.

### Regulatory routing (wubu_wifi_reg.c)

| Component | Role |
|-----------|------|
| regulatory.db | wireless-regdb |
| crda | regulatory daemon |
| cfg80211 | regulatory core |

### Country routing

Alpha2 ISO codes are normalized to uppercase. World (00) default.

### DFS band routing

| Band | Routing |
|------|---------|
| 5GHz | `dfs-5ghz` |
| 6GHz | `6ghz` |
| 2.4GHz | `2.4ghz` |

### Components
- regulatory.db (wireless-regdb), crda/regdbdump
- cfg80211: set_regdom, DFS regions, passive channels
- DFS: radar detection, CAC (channel availability check)

### Kernel summary line

```
wifireg[db=0 crda=0 dfs=0 radar=0 country=0 drv=none]
```

Published to `/kv/world/hw_wifireg` by `wubu_wifi_reg_summary()`.
