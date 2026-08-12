# BT-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Bluetooth/LE Audio driver gaps

Bluetooth connects controllers, headsets, audio, and IoT. WuBuOS routes
the controller by transport and flags LE Audio capability.

### Controller driver routing (wubu_bt.c)

| Vendor | Driver |
|--------|--------|
| Intel (Wireless-AC/BE) | `btintel` |
| Broadcom/Cypress | `btbcm` |
| Realtek | `btrtl` |
| MediaTek (MT7921/22) | `btmtk` |
| USB generic | `btusb` |
| UART | `hci_uart` |

### LE Audio
LE Audio (replacing A2DP) runs over ISO channels — BAP + Auracast.
Needs kernel 6.1+ isochronous support + BlueZ 5.65+. WuBuOS flags
`wubu_bt_le_audio()`.

### Kernel summary line

```
bt[present=0 usb=0 pci=0 uart=0 le_audio=0 drv=none]
```

Published to `/kv/world/hw_bt` by `wubu_bt_summary()`.
