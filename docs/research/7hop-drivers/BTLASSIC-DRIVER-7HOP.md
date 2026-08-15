# BTLASSIC-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Bluetooth classic gaps

BT classic carries voice audio (SCO/eSCO) and file transfer
(FTP/OBEX). Classic profile routing differs from LE.

### Impl routing (wubu_btclassic.c)

| Route | Path |
|-------|------|
| BT adapter presence | /sys/class/bluetooth/hci0 |
| BT connection state   | /proc/net/bt |

SCO/eSCO rates: CVSD=64kHz, mSBC=128kHz.
Profiles: none(0), a2dp(1), hfp(2), ftp(3).
