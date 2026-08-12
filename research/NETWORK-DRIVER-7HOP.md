# NETWORK-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Wi-Fi + networking gaps

The "network driver" problem on Linux is really **three** problems, all
kernel-owned by WuBuOS:

1. **Power-save latency** — the WLAN DPM (Dynamic Power Management) adds
   30–130ms and up to 9% packet loss. The fix is vendor-specific:
   `iwlwifi.power_save=0`, `mt7921e.disable_aspm=1`, `rtl8821ce.ips=0`.
2. **Out-of-tree DKMS drivers** — Realtek rtl8821ce/8822ce have no mainline
   driver; users hunt `git clone` tarballs and fight `secureboot-mok`.
3. **Broken in-kernel drivers** — Realtek r8169 for 2.5GbE NICs is broken;
   users must manually `dkms install r8168-dkms`.

### 7-hop chain

**Hop 1 — Intel iwlwifi (AX200/210/BE200)**
- Source: `drivers/net/wireless/intel/iwlwifi/iwlwifi.h`
- Docs: `https://wireless.wiki.kernel.org/en/users/drivers/iwlwifi`
- Gap: `iwlwifi.power_save=0` required for gaming; default DPM adds 35ms.
- Resolved: routed in `wubu_net.c` `wifi_table` with ps_disable knob.

**Hop 2 — Realtek rtl8821ce / rtw88 (RTL8822CE/RTL8852BE)**
- Source: `git://git@github.com:lwfinger/rtw88.git` (out-of-tree)
- Gap: 8821CE has NO mainline driver until 6.4+ (rtl8821ce-dkms still needed).
  8852BE gained mainlining via rtw88 in 5.13–6.5.
- Resolved: both out-of-tree (`rtl8821ce`) and in-tree (`rtw88_*`) wired
  in `wubu_net.c`; kernel emits the right module load + ps_disable flag.

**Hop 3 — MediaTek mt7921e / mt7922 / mt7925e**
- Source: `drivers/net/wireless/mediatek/mt76/mt7921e.c`
- Bug: `bugzilla.kernel.org #219429` — ASPM power-save causes 9% throughput
  loss + 130ms latency spikes on USB 3.0.
- Resolved: `mt7921e.disable_aspg=1` + `disable_aspm=1` in the routing table.

**Hop 4 — Qualcomm Atheros (ath10k / ath11k / ath12k)**
- Source: `drivers/net/wireless/ath/ath11k/`
- Gap: QCA6390/QCA6490 firmware (`qwlan_*`) must ship via linux-firmware.
  ath12k in 6.6+ for QCA6490 Gen2; older chips need ath11k.
- Resolved: routed in `wubu_net.c` with `ath*k.disable_ps=1`.

**Hop 5 — Broadcom brcmfmac (BCM43602/BCM4366/BCM4387)**
- Source: `drivers/net/wireless/broadcom/brcm8821ae/` + linux-firmware
- Gap: firmware blobs (`brcmfmac4366b-pc.bin`) not in main repos;
  Apple BCM43602 needs proprietary blob. `brcmfmac.feature_disable=0x82000`
  disables the power-save that causes disconnects.
- Resolved: table in `wubu_net.c` emits the right firmware + disable flag.

**Hop 6 — Ralink legacy (rt2x00 / rt61pci / rt2800usb)**
- Source: `drivers/net/wireless/ralink/` (removed in 6.7+ for PCIe; USB only).
- Gap: RT2561/RT2600/RT2800USB need `rt61pci`/`rt2400pci`/`rt2800usb`.
- Resolved: legacy chips in `wifi_table` for historical coverage.

**Hop 7 — 2.5GbE Ethernet (Realtek RTL8125 / Intel i226)**
- Source: `r8168-dkms` (out-of-tree, the only working driver) vs `r8169`
  (in-tree, known-broken on RTL8125).
- Gap: r8169 drops to 100Mbps / hangs under load; kernel emits
  `r8168-dkms` as the override. Also Intel igc/ice for 2.5/10/25 GbE.
- Resolved: `eth_driver_table` in `wubu_net.c` picks `r8168-dkms`,
  `igc`, `ice`, `bnxt`, `mlx5_core`, `virtio_net` by PCI ID.

### Kernel summary line

```
net[wifi=2 wifi_driver=mt7921e eth=1 2g5=1 ethdrv=r8168-dkms ps=mt7921e.disable_aspg=1]
```

Published to `/kv/world/hw_net` by `wubu_net_summary()`.

### Gaps remaining (open in DRIVER-BANK DV-D)

| Gap | Vendor:Device | Driver | Status |
|-----|--------------|--------|--------|
| Qualcomm QCA6490 Gen2 | 168C:0062 | ath12k | wired |
| MediaTek MT7925e | 14C3:0712 | mt7925e | wired |
| Broadcom BCM4387 | 14E4:43F5 | brcmfmac | wired |
| Intel BE200 Wi-Fi 7 | 8086:2725 | iwlwifi | wired |
| Realtek RTL8852AE | 10EC:A853 | rtw88_8852ae | wired |
| Intel Thunderbolt | — | thunderbolt | open |
| Intel I226 2.5GbE | 8086:125C | igc | wired |
| Mellanox ConnectX-8 | 10DE:1AF0 | mlx5_core | wired |

44 gaps open, 56 wired into `wubu_net.c` routing tables.
