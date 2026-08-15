# CLOCK-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux RTC-chip + thermal driver gaps

This module routes the RTC chip driver + thermal zones. It complements
`wubu_rtc.c` (the CMOS wall-clock reader — that one reads time, this one
routes which RTC chip driver + thermal driver the kernel binds).

### RTC chip routing (wubu_clock.c)

| RTC | Driver |
|-----|--------|
| Maxim DS1307 | `ds1307` |
| Maxim DS3231 (precision) | `ds3231` |
| NXP PCF8523 | `pcf8523` |
| NXP PCF2127 | `pcf2127` |
| ST M41T80 | `m41t80` |
| PC CMOS RTC | `rtc-cmos` |
| EFI RTC | `rtc-efi` |

### Thermal routing

| Thermal | Driver |
|---------|--------|
| Intel int340x | `int340x` |
| Intel coretemp | `coretemp` |
| Rockchip | `rockchip_thermal` |
| Samsung Exynos | `exynos_tmu` |
| ACPI (acpitz) | `acpitz` |

### Kernel summary line

```
clock[rtc=1(rtc-core) thermal=1(0 zones/thermal-core) cooling=1]
```

Published to `/kv/world/hw_clock` by `wubu_clock_summary()`.
