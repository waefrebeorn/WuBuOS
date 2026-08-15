# BTHFP-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Bluetooth HSP/HFP gaps

HSP/HFP profiles carry voice audio with call control (AT commands).
Profiles must auto-select based on use case (media vs voice).

### Impl routing (wubu_bthfp.c)

| Route | Path |
|-------|------|
| BT adapter presence | /sys/class/bluetooth/hci0 |
| Intel BT params      | /sys/module/btintel/parameters |

Profile auto-select: latency < 20ms = voice (HSP/HFP), else media (A2DP).
States: idle(0), active(1), ringing(2).
