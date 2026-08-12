# SPDIFRX-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio SPDIF receiver gaps

SPDIF RX (receiver) captures S/PDIF digital audio input via IEC 60958.

### Format routing (wubu_spdifrx.c)

| Format | Routing |
|--------|--------|
| dts-hd | DTS-HD |
| truehd | TrueHD |
| eac3 | E-AC-3 |
| pcm | PCM |
| dts | DTS |
| ac3 | AC3 |

### Lock routing

| Lock state | Routing |
|-----------|--------|
| lock / plck | locked |
| unl / nol | unlocked |
| invalid | invalid |
