# MALI_G720-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: ARM Mali G720 gaps

Mali-G720 (5th gen / v12) binds the Panthor kernel driver with
Panfrost userspace. Mesa Panfrost supports G720 Vulkan 1.4.
SBCwiki: Panfrost and Panthor are community + Arm-collabora drivers.

### Impl routing (wubu_mali_g720.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

G720 is 5th gen (v12). Panthor kernel driver; Vulkan 1.4.
