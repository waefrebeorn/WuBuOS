# BAP-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Bluetooth BAP gaps

BAP (Basic Audio Profile) routes high-quality audio over
Bluetooth Classic and LE Audio. Supports 44.1/48/96kHz at
16/24/32 bit depth.

### Impl routing (wubu_bap.c)

| Route | Path |
|-------|------|
| BT adapter presence | /sys/class/bluetooth/hci0 |
| Intel BT params       | /sys/module/btintel/parameters |

Codec valid: 44.1-96kHz, 16-32 bits. Ready = configured AND connected.
