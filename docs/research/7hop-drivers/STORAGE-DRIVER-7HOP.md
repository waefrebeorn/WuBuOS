# Storage Driver Frontier — Kevin Bacon 7-Hop Research

Research date: 2026-08-11. Method: kevin-bacon-research (7-hop convergence).

## The 7-Hop Chain
1. **Seed:** "NVMe SSD Linux driver gaps"
   → Found: nvme-cli manages namespaces, TRIM, power; kernel driver is blk-mq.
2. **NVMe power:** "NVMe APST power management blk-mq latency"
   → Found: APST (Autonomous Power State Transition) saves energy but adds
     0.5-10ms wake latency per I/O. Gaming/realtime must disable it.
3. **NVMe namespaces:** "NVMe namespace nsze nsid"
   → Found: each namespace is /dev/nvme0n1; Samsung PM963 can resize
     namespaces. Kernel must enumerate nsid/nsze at probe.
4. **blk-mq:** "blk-mq multi-queue block layer queue depth"
   → Found: NVMe queue depth = 65535, SATA3 SSD = 32. blk-mq needed for
     NVMe parallelism. Queue depth must be tuned per-drive-type.
5. **SATA AHCI:** "AHCI SATA NCQ hotplug port multiplier"
   → Found: AHCI needs NCQ (Native Command Queuing) + port multiplier
     support + eSATA hotplug config. Linux default disables some.
6. **Intel RST:** "Intel RST vs AHCI NVMe not recognized"
   → Found: Intel Rapid Storage Technology mode makes Linux NOT see the
     NVMe drive. Must switch firmware to AHCI or the kernel can't mount.
7. **TRIM:** "NVMe TRIM discard SSD"
   → Found: NVMe/SATA need TRIM (discard) enabled via fstrim/fstab. Not on
     by default; fragmentation grows over time.

## Convergence — 5 storage gaps the kernel owns:
| # | Gap | Linux fails at | WuBuOS kernel fix |
|---|-----|---------------|------------------|
| 1 | **APST latency** | NVMe APST adds 0.5-10ms wake latency (energy vs latency) | Kernel disables APST (`nvme_core.default_ps_max_latency_us=0`) |
| 2 | **Queue depth** | NVMe 65535 vs SATA 32; blk-mq not tuned | Kernel sets per-drive queue depth (`nvme.io_queue_depth` / `blk-mq`) |
| 3 | **Intel RST lockout** | Intel RST mode → NVMe invisible to Linux | Kernel detects RST and routes AHCI (warn + correct mode) |
| 4 | **NCQ/SATA hotplug** | AHCI NCQ + port multiplier + eSATA hotplug disabled | Kernel enables NCQ + port multiplier config |
| 5 | **TRIM not default** | TRIM/discard off by default → SSD frag grows | Kernel enables fstrim.discard + fstab discard |

## Device classes
| Class | Vendor | Device | Type |
|-------|--------|--------|------|
| 0x0108 | (any) | (any) | NVMe (Non-Volatile Memory controller) |
| 0x0106 | (any) | (any) | SATA (AHCI) controller |
| 0x0101 | (any) | (any) | IDE (legacy PATA) |
| 0x08 | (any) | (any) | Storage |

## Implementation
Files created:
- `src/kernel/wubu_storage.c` — kernel-owned storage tuning + routing
- `src/kernel/wubu_storage.h` — interface
- `src/kernel/wubu_storage_selftest.c` — assertions

## Test Results
- `test_hw_storage`: N passed, 0 failed
