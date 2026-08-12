# LVM-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux LVM storage routing gaps

LVM (Logical Volume Manager) provides volume groups + logical volumes
with thin provisioning, snapshots, and hybrid cache tiers.

### Impl routing (wubu_lvm.c)

| Route | Path |
|-------|------|
| Mounted LVM volumes | /proc/mounts dm-* entries |
| DM UUID            | /sys/block dm-x/dm/uuid |

UUID prefix: LVM_. Health: healthy <85%, warning <95%, critical >=95%.
Modes: writeback, writethrough, writearound.
