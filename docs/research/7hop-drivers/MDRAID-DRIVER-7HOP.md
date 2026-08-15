# MDRAID-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage MD RAID gaps

MD (Multiple Device) RAID provides software RAID levels 0/1/5/6/10
with degraded array detection and rebuild monitoring.

### Impl routing (wubu_mdraid.c)

| Route | Path |
|-------|------|
| RAID status  | /proc/mdstat |
| DM UUID       | /sys/block dm-x/dm/uuid |

RAID levels: raid0, raid1, raid4, raid5, raid6, raid10.
Degraded = total - active. Health: healthy <85%, warning <95%, critical >=95%.
