# BACKLIGHT-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux display backlight + NIC WoL gaps

Two capabilities: display brightness + Wake-on-LAN.

### Backlight routing (wubu_backlight.c)

| Device | Driver |
|--------|--------|
| ACPI video | `acpi-video` |
| Intel | `intel-backlight` |
| AMD | `amdgpu-bl` |
| PWM | `pwm-backlight` |
| NVIDIA | `nouveau-backlight` |

### WoL routing

| Wake mode | Routing |
|-----------|---------|
| Magic packet | `magic-packet` |
| Unicast | `unicast` |
| Broadcast | `broadcast` |
| ARP | `arp` |
| Multicast | `multicast` |

### Kernel summary line

```
backlight[bl=0(none) acpi=0 native=0 wol=1 magic=1]
```

Published to `/kv/world/hw_backlight` by `wubu_backlight_summary()`.

**Verified live:** this host reports `wol=1 magic=1`.
