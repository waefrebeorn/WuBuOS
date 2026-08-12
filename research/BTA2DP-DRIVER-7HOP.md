# BTA2DP-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Bluetooth A2DP codec gaps

A2DP carries high-quality stereo audio over Bluetooth with
multiple codec options (SBC/AAC/aptX/LDAC). Codec selection
affects latency/power/bandwidth.

### Impl routing (wubu_bta2dp.c)

| Route | Path |
|-------|------|
| BT adapter presence | /sys/class/bluetooth/hci0 |
| Intel BT params      | /sys/module/btintel/parameters |

Codec bitrates: SBC=320, MP3=256, AAC=320, aptX=576, aptX HD=990.
