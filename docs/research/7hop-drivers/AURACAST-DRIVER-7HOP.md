# AURACAST-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Bluetooth Auracast gaps

Auracast (LE Audio broadcast) routes audio to multiple
listeners simultaneously via LC3. Broadcast supports up to
16 synchronized streams.

### Impl routing (wubu_auracast.c)

| Route | Path |
|-------|------|
| BT adapter presence | /sys/class/bluetooth/hci0 |
| Intel BT params       | /sys/module/btintel/parameters |

Streams: 1-16 valid. Broadcasting requires PA + broadcaster active.
