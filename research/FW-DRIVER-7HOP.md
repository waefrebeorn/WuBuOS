# FW-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage controller firmware gaps

Storage controller firmware (RAID/HBA flash) loaded via kernel fw loader.

### Firmware routing (wubu_fw.c)

| Component | Role |
|-----------|------|
| firmware_class | kernel fw loader (request_firmware) |
| /lib/firmware | firmware blob storage |
| megaraid_sas | MegaRAID flash |
| mpt3sas / mpt2sas | LSI SAS flash |
| hpsa | HP Smart Array firmware |

### Controllers

| Controller | Routing |
|------------|---------|
| megaraid | `megaraid-sas` |
| hpsa | `hpsa` |
| mpt3 | `mpt3sas` |
| mpt2 | `mpt2sas` |
| aac | `aacraid` |

### Stages: load / verify / apply / commit

### Kernel summary line

```
fw[fw=1 lib=1 raid=0 hba=0 update=1 drv=fw-loader]
```

Published to `/kv/world/hw_fw` by `wubu_fw_summary()`.

**Verified live:** this host reports `fw=1 lib=1 update=1`.
