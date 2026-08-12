# UAS-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage USB Attached SCSI gaps

UAS (USB Attached SCSI) protocol routing over USB Mass Storage for
improved command queuing and lower latency vs BOT.

### Impl routing (wubu_uas.c)

| wubu fn | source |
|---|---|
| wubu_uas_present | /sys/class/scsi_host |
| wubu_uas_proto_for | USB mass storage protocol |
| wubu_uas_summary | UAS/BOT/CBI/CB protocol summary |
