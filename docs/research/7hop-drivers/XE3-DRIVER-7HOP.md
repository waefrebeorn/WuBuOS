# XE3-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Intel Xe3 Celestial gaps

Intel Xe3 (Celestial) binds the xe kernel driver + Iris/ANV in
Mesa. VideoCardz: Xe3P enablement started in Mesa. Phoronix
Tech Tour 2025: Xe driver + Iris/ANV being wired up for Xe3.

### Impl routing (wubu_xe3.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Xe3 = Celestial. Binds xe kernel driver + Iris/ANV (Mesa).
