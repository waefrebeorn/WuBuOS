# BTAMESH-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Bluetooth mesh gaps

BT mesh supports many-to-many device networking with relay
nodes and friend nodes. Hop count is clamped to 8 (Bluetooth
spec max).

### Impl routing (wubu_btamesh.c)

| Route | Path |
|-------|------|
| BT adapter presence | /sys/class/bluetooth/hci0 |
| BT Intel params       | /sys/module/btintel/parameters |

Roles: none(0), relay(1), proxy(2), friend(3), low_power(4).
Relay/proxy roles can forward mesh packets.
