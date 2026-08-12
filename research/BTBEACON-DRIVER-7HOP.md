# BTBEACON-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Bluetooth beacon gaps

BT beacons (iBeacon/AltBeacon/Eddystone) broadcast advertising
packets for proximity, telemetry, and UID.

### Impl routing (wubu_btbeacon.c)

| Route | Path |
|-------|------|
| BT adapter presence | /sys/class/bluetooth/hci0 |
| Intel BT params       | /sys/module/btintel/parameters |

Proximity: immediate(0, rssi > -70), near(1, -70 to -85), far(2, < -85).
UUID prefixes: fda5(iBeacon), febe(Eddystone), 0123(AltBeacon).
