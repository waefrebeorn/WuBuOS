# LEAUDIOLDR-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Bluetooth LE Audio routing gaps

LE Audio (Low Complexity Communication Codec / LC3) routes
high-quality audio over Bluetooth Low Energy. LC3 supports
24-192 sample frames at 7.5ms/10ms intervals.

### Impl routing (wubu_leaudioldr.c)

| Route | Path |
|-------|------|
| BT adapter presence | /sys/class/bluetooth/hci0 |
| Intel BT params       | /sys/module/btintel/parameters |

Samples = frame_us * 8000 / 1000000.
Valid frames: 24, 48, 72, 96, 120, 144, 168, 192 samples.
