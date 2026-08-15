# VBLANK-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU VBLANK interrupt gaps

VBLANK (vertical blank) interrupt synchronizes display refresh and page flips.

### Source routing (wubu_vblank.c)

| Source | Routing |
|--------|--------|
| crtc | CRTC |
| connector | Connector |
| encoder | Encoder |
| plane | Plane |
| primary | Primary |
| cursor | Cursor |

### Mode routing

| Mode | Routing |
|------|--------|
| event | event |
| flip | page-flip |
| counter | counter |
| time | time |
| disable | disabled |
| enable | enabled |
