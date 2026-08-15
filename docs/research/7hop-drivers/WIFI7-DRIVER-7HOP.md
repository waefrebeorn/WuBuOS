# WIFI7-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Wi-Fi 7 (802.11be) / 6GHz driver gaps

Wi-Fi 7 = MLO, 320MHz, 4096-QAM, 6GHz. WuBuOS routes the newest cards.

### Wi-Fi 7 driver routing (wubu_wifi7.c)

| Card | Device ID | Driver |
|------|-----------|--------|
| Intel BE200 | 0x2725 | `iwlwifi` |
| Intel BE201 | 0x2726 | `iwlwifi` |
| Qualcomm WCN7850 (FC7800) | 0x1107 | `ath12k_pci` |
| MediaTek MT7925 | 0x0712 | `mt7925e` |
| Realtek RTL8922 | 0x8922 | `rtw89` |
| Realtek RTL8852C | 0x8852 | `rtw89` |
| Broadcom BCM4389 | 0x43F5 | `brcmfmac` |

### Gap: ath12k missing in some kernels
Ubuntu 24.04 shipped without `ath12k_pci` for WCN7850 — the Qualcomm Wi-Fi 7
card fails to bind. WuBuOS detects the card and flags the driver requirement.

### Features
All Wi-Fi 7 cards support MLO (multi-link), 6GHz, and 320MHz. WuBuOS flags
`wubu_wifi7_mlo()`, `wubu_wifi7_6ghz()`, `wubu_wifi7_320mhz()`.

### Kernel summary line

```
wifi7[present=0 mlo=0 6ghz=0 320mhz=0 drv=none name=-]
```

Published to `/kv/world/hw_wifi7` by `wubu_wifi7_summary()`.
