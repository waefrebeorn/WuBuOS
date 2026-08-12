# DRIVER-BANK — every driver, every generation, old->new

Date: 2026-08-11. WuBuOS doctrine: **we run everything and run on
everything.** This bank is the systematic driver backlog: every
subsystem x every vendor x every generation, so the kernel binds
hardware no matter how old. 10 themes x exactly 100 = 1000 gaps.
Each gap is a REAL driver/device mechanism from the researched
frontier (GPU/Audio/Storage/Network/Input/Display/USB/Power/
Peripherals/Virtual), statused `open` (needs binding) or `wired`
(bound in the kernel). Sources in research/*-DRIVER-7HOP.md.

## Legend
- `open`  = driver/device not yet bound by the kernel
- `wired` = bound in src/kernel (wubu_hw_detect/wubu_drv_registry)
- driver name in the gap = the exact kernel module to bind

## The driver registry (wired so far)
wubu_drv_init() registers: nvme, ahci, wifi, net, hda, gpu, battery,
sd, usb_hid, usb_msc, usb_bt, thermal, virtio_blk/net/gpu/input,
arm_platform, intel_platform. Plus the auto-probers: wubu_audio,
wubu_storage, wubu_net, wubu_input (the 7-hop frontier closures).

## DV-GPU

- DV-A01 NVIDIA Fermi (GTX 4xx/5xx) bind nvidia legacy 470 `wired`  (wubu_nvidia_fermi)
- DV-A02 NVIDIA Kepler (GTX 6xx/7xx) bind nvidia legacy 470 `wired`  (wubu_nvidia_kepler)
- DV-A03 NVIDIA Maxwell (GTX 9xx) bind nvidia 535 `wired`  (wubu_nvidia_maxwell)
- DV-A04 NVIDIA Pascal (GTX 10xx) bind nvidia 535 `wired`  (wubu_nvidia_pascal)
- DV-A05 NVIDIA Volta (V100) bind nvidia datacenter 535 `wired`  (wubu_nvidia_volta)
- DV-A06 NVIDIA Turing (RTX 20xx) bind nvidia 535 `wired`  (wubu_nvidia_turing)
- DV-A07 NVIDIA Ampere (RTX 30xx) bind nvidia 535 `wired`  (wubu_ampere)
- DV-A08 NVIDIA Ada (RTX 40xx) bind nvidia driver  (0x26xx IDs) `wired`
- DV-A09 NVIDIA Blackwell (RTX 50xx) bind nvidia driver  (0x2684) `wired`
- DV-A10 NVIDIA GT2xx legacy bind nouveau fallback `wired`  (wubu_gt2xx)
- DV-A11 NVIDIA IC (compute) bind nvidia_icd Vulkan `wired`
- DV-A12 NVIDIA mobile Optimus bind prime `wired`
- DV-A13 NVIDIA laptop hybrid bind DRI_PRIME `wired`
- DV-A14 AMD GCN1 Southern Islands bind amdgpu.si_support `wired`
- DV-A15 AMD GCN2 Sea Islands bind amdgpu.cik_support `wired`
- DV-A16 AMD GCN3 Volcanic Islands bind amdgpu `wired`  (wubu_volcanic_islands)
- DV-A17 AMD GCN4 Arctic Islands (RX 4xx/5xx) bind amdgpu `wired`  (wubu_arctic_islands)
- DV-A18 AMD Vega (GCN5 RX Vega) bind amdgpu `wired`  (wubu_vega)
- DV-A19 AMD Raven/Renoir APU bind amdgpu `wired`  (wubu_renoir)
- DV-A20 AMD Navi10 RDNA1 bind amdgpu radv `wired`  (wubu_navi10)
- DV-A21 AMD Navi21-24 RDNA2 bind amdgpu radv  (163F,1640) `wired`
- DV-A22 AMD Navi31-33 RDNA3 bind amdgpu radv  (15BF,15C8) `wired`
- DV-A23 AMD Navi44/48 RDNA4 bind amdgpu amdvlk  (74C4,74C2) `wired`
- DV-A24 AMD Kaveri APU bind amdgpu.cik  (130F) `wired`
- DV-A25 AMD Raphael desktop iGPU bind amdgpu  (164E) `wired`
- DV-A26 AMD Granite Ridge bind amdgpu  (1538) `wired`
- DV-A27 AMD Radeon 7000 (Tahiti) bind amdgpu.si  (6798) `wired`
- DV-A28 AMD Radeon HD 6000 bind radeon legacy `wired`  (wubu_radeon_6000)
- DV-A29 AMD Radeon HD 5000 bind radeon legacy `wired`  (wubu_radeon_5000)
- DV-A30 Intel Gen8 Broadwell bind i915 iris `wired`  (wubu_intelgpu)
- DV-A31 Intel Gen9 Skylake bind i915 iris `wired`  (wubu_intel_skylake)
- DV-A32 Intel Gen11 Ice Lake bind i915 iris `wired`  (wubu_intel_icelake)
- DV-A33 Intel Gen12 Tiger Lake bind i915 anv  (9A49) `wired`
- DV-A34 Intel Alder Lake bind i915 anv  (46A6) `wired`
- DV-A35 Intel Raptor Lake bind i915 anv  (A7A0) `wired`
- DV-A36 Intel Meteor Lake bind i915 anv  (7D45) `wired`
- DV-A37 Intel Lunar Lake Xe2 bind xe anv  (7D5A) `wired`
- DV-A38 Intel Battlemage Xe2 bind xe anv  (E20B) `wired`
- DV-A39 Intel Celestial Xe3 bind xe anv `wired`  (wubu_xe3)
- DV-A40 Intel GMA legacy (G31/G45) bind i915 legacy `wired`  (wubu_intel_gma)
- DV-A41 Qualcomm Adreno 600 bind freedreno `wired`  (wubu_adreno600)
- DV-A42 Qualcomm Adreno 700 bind freedreno `wired`  (wubu_adreno700)
- DV-A43 ARM Mali G52 bind panfrost `wired`  (wubu_mali_g52)
- DV-A44 ARM Mali G77 bind panfrost `wired`  (wubu_mali_g77)
- DV-A45 ARM Mali G720 bind panthor `wired`  (wubu_mali_g720)
- DV-A46 Broadcom VideoCore bind vc4 (RPi) `wired`  (wubu_vc4)
- DV-A47 Broadcom VideoCore VI bind v3d (RPi4/5) `wired`  (wubu_vc6)
- DV-A48 Imagination PowerVR bind pvrsrv `wired`  (wubu_powervr)
- DV-A49 Intel Arc dGPU bind xe  (E20B) `wired`
- DV-A50 AMD RDNA3.5 Strix bind amdgpu  (15C8) `wired`
- DV-A51 NVIDIA Quadro bind nvidia 535 `wired`  (wubu_quadro)
- DV-A52 AMD Instinct MI bind amdgpu rocm `wired`  (wubu_instinct)
- DV-A53 Vulkan software fallback bind lvp_icd `wired`
- DV-A54 WSL2 dzn bind /dev/dxg `wired`
- DV-A55 WSL2 gfxstream fallback bind `wired`
- DV-A56 OpenGL core bind mesa `wired`
- DV-A57 OpenCL bind clinfo `wired`  (wubu_opencl)
- DV-A58 CUDA bind libcuda `wired`  (wubu_cuda)
- DV-A59 Vulkan 1.4 bind full profile `wired`  (wubu_vulkan14)
- DV-A60 GCN1/2 Vulkan via amdgpu `wired`
- DV-A61 RDNA4 Vulkan via amdvlk `wired`
- DV-A62 Intel Xe Vulkan via anv `wired`
- DV-A63 hybrid iGPU+dGPU prime `wired`
- DV-A64 VRAM detection per driver `wired`  (wubu_panel)
- DV-A65 display clock detect `open`
- DV-A66 HDR10 output `open`
- DV-A67 G-Sync/FreeSync variable refresh `open`
- DV-A68 multi-monitor bind `open`
- DV-A69 GPU thermal throttle read `wired`  (wubu_thermal)
- DV-A70 GPU fan control bind `wired`  (wubu_fantml)
- DV-A71 AMD ppfeaturemask overdrive `open`
- DV-A72 NVIDIA nvidia-smi power read `wired`  (wubu_voltagectl)
- DV-A73 Intel GPU overclock bind `open`
- DV-A74 ray tracing enable `open`
- DV-A75 DLSS/FSR upscale bind `open`
- DV-A76 vulkan-radeon radv_icd `wired`
- DV-A77 vulkan-intel anv_icd `wired`
- DV-A78 libglvnd EGL loader `wired`
- DV-A79 mesa iris GL `wired`
- DV-A80 mesa radv GL `wired`
- DV-A81 llvmpipe software GL `wired`
- DV-A82 Panfrost GL `open`
- DV-A83 Panthor Vulkan `open`
- DV-A84 Xe2 Xe3 KMD switch `wired`
- DV-A85 i915 vs xe auto-select `wired`
- DV-A86 amdgpu vs radeon auto-select `wired`
- DV-A87 nvidia vs nouveau auto-select `open`
- DV-A88 GPU driver module auto-load `wired`
- DV-A89 GPU firmware version check `wired`  (wubu_gpufwupd)
- DV-A90 GPU shader model detect `wired`  (wubu_gpushader)
- DV-A91 GPU memory bandwidth measure `wired`  (wubu_gpumem)
- DV-A92 GPU compute queue bind `wired`  (wubu_gpucsched)
- DV-A93 GPU video decode (VA-API) `wired`  (wubu_vpudecode)
- DV-A94 GPU video encode (VA-API) `wired`  (wubu_vpuencode)
- DV-A95 GPU display controller (DC) bind `wired`  (wubu_gpudc)
- DV-A96 GPU KMS modeset bind `wired`  (wubu_gpukms)
- DV-A97 GPU render node expose `wired`  (wubu_rendernode)
- DV-A98 GPU /dev/dri detect `wired`
- DV-A99 GPU /dev/dxg detect `wired`
- DV-A100 GPU full vendor matrix wired `wired`

Status: open. GPU-DRIVER-7HOP.md

## DV-Audio

- DV-B01 ALSA core bind snd `open`
- DV-B02 Intel HDA PCH bind snd_hda_intel `wired`
- DV-B03 Intel SOF bind snd_sof `open`
- DV-B04 Intel Tiger Lake audio bind snd_sof  (A0C8) `open`
- DV-B05 Intel Alder Lake audio bind snd_sof  (51C8) `open`
- DV-B06 Intel Skylake audio bind snd_hda `open`
- DV-B07 AMD HDMI audio bind snd_hda_intel `open`
- DV-B08 AMD Navi 31 HD audio bind snd_hda  (AB30) `open`
- DV-B09 NVIDIA HDMI audio bind snd_hda `open`
- DV-B10 USB audio class bind snd_usb_audio `open`
- DV-B11 USB audio class 1 bind snd_usb `open`
- DV-B12 USB audio class 2 bind snd_usb `open`
- DV-B13 PipeWire daemon bind `open`
- DV-B14 WirePlumber session bind `open`
- DV-B15 PulseAudio compat bind `open`
- DV-B16 JACK low-latency bind `open`
- DV-B17 Bluetooth A2DP bind  (bt_config) `wired`
- DV-B18 Bluetooth LDAC codec bind `wired`
- DV-B19 Bluetooth aptX codec bind `wired`
- DV-B20 Bluetooth HSP/HFP bind `wired`  (wubu_bthfp)
- DV-B21 Bluetooth SBC bind `open`
- DV-B22 Bluetooth SCO bind `open`
- DV-B23 HDMI audio over DP bind `open`
- DV-B24 DisplayPort audio bind `open`
- DV-B25 AC'97 legacy sound bind snd_ac97 `open`
- DV-B26 SB16 legacy sound bind snd_sb16 `open`
- DV-B27 AdLib OPL3 bind snd_opl3 `open`
- DV-B28 MIDI synth bind `open`
- DV-B29 USB DAC index pin  (serial) `wired`
- DV-B30 PipeWire quantum tune `wired`
- DV-B31 RTKit priority 95 `wired`
- DV-B32 48kHz sample rate `wired`
- DV-B33 USB DAC buffer size `open`
- DV-B34 Pro-audio zero latency bind `open`
- DV-B35 Surround 5.1 bind `open`
- DV-B36 7.1 surround bind `open`
- DV-B37 Dolby Atmos passthrough `open`
- DV-B38 DTS passthrough `open`
- DV-B39 Equalizer DSP bind `open`
- DV-B40 Crossfeed DSP bind `open`
- DV-B41 Volume normalization bind `open`
- DV-B42 Mic echo cancel bind `open`
- DV-B43 Noise suppression bind `open`
- DV-B44 Voice activity detect bind `open`
- DV-B45 Music synth bind wubu_audio `open`
- DV-B46 Chiptune SID bind `open`
- DV-B47 FM synthesis bind `open`
- DV-B48 Sample playback bind `open`
- DV-B49 Audio mixer bind `open`
- DV-B50 ALSA state restore `wired`
- DV-B51 PipeWire 90-wubuos conf `wired`
- DV-B52 WirePlumber 90-wubuos lua `wired`
- DV-B53 Audio device hotplug bind `open`
- DV-B54 USB audio hotplug bind `open`
- DV-B55 HDMI audio hotplug bind `open`
- DV-B56 BT audio auto-connect `wired`
- DV-B57 audio profile auto-select `wired`  (wubu_bthfp)
- DV-B58 default sink auto-pick `open`
- DV-B59 per-app volume bind `open`
- DV-B60 stream routing bind `open`
- DV-B61 audio jack detect bind `wired`  (wubu_jack)
- DV-B62 headset mic detect bind `open`
- DV-B63 SPDIF digital out bind `wired`  (wubu_spdifrx)
- DV-B64 TOSLINK optical bind `open`
- DV-B65 analog line in bind `open`
- DV-B66 mic boost bind `open`
- DV-B67 volume limit protect `open`
- DV-B68 audio sample format 32f bind `open`
- DV-B69 PCM 24-bit bind `open`
- DV-B70 DSD audio bind `open`
- DV-B71 MQA passthrough `open`
- DV-B72 ASIO compat bind `open`
- DV-B73 WASAPI compat bind `open`
- DV-B74 CoreAudio compat bind `open`
- DV-B75 audio latency measurement `open`
- DV-B76 xrun detection `open`
- DV-B77 buffer underrun protect `open`
- DV-B78 audio clock drift bind `open`
- DV-B79 dual clock resample bind `open`
- DV-B80 channel remap bind `open`
- DV-B81 bass management bind `open`
- DV-B82 crossover DSP bind `open`
- DV-B83 room correction DSP `open`
- DV-B84 psychoacoustic loudness `open`
- DV-B85 audio fingerprint bind `open`
- DV-B86 beat detection bind `open`
- DV-B87 tempo sync bind `open`
- DV-B88 AudioDSP chain bind `open`
- DV-B89 audio thread realtime `wired`
- DV-B90 audio IRQ latency `open`
- DV-B91 ALSA ucm config bind `open`
- DV-B92 sof-firmware load `open`
- DV-B93 HDA firmware load `open`
- DV-B94 USB firmware load `open`
- DV-B95 audio kernel module autoload `wired`
- DV-B96 audio PCI detect `wired`
- DV-B97 audio class 0x04 detect `wired`
- DV-B98 audio BT A2DP detect `wired`
- DV-B99 audio full-stack unified sink `wired`
- DV-B100 audio vendor matrix wired `wired`

Status: open. AUDIO-DRIVER-7HOP.md

## DV-Storage

- DV-C01 NVMe gen3 bind nvme `wired`
- DV-C02 NVMe gen4 bind nvme `wired`  (wubu_nvme_gen4)
- DV-C03 NVMe gen5 bind nvme `wired`  (wubu_nvme_gen5)
- DV-C04 NVMe namespace enumerate  (wubu_drv_nvme) `wired`
- DV-C05 NVMe TRIM bind  (trim_config) `wired`
- DV-C06 NVMe APST disable  (kernel_params) `wired`
- DV-C07 NVMe queue depth tune `wired`
- DV-C08 NVMe multipath bind `open`
- DV-C09 NVMe power state read `wired`  (wubu_nvmepower)
- DV-C10 SATA AHCI bind  (wubu_drv_ahci) `wired`
- DV-C11 SATA NCQ bind `open`
- DV-C12 SATA port multiplier bind `open`
- DV-C13 SATA hotplug bind `open`
- DV-C14 eSATA hotplug bind `open`
- DV-C15 SATA SSD TRIM bind `wired`
- DV-C16 IDE legacy PATA bind `open`
- DV-C17 IDE ATAPI bind `open`
- DV-C18 USB mass storage bind  (usb_msc) `wired`
- DV-C19 USB3 UAS bind `wired`  (wubu_uas)
- DV-C20 USB2 storage bind `open`
- DV-C21 USB-C storage bind `open`
- DV-C22 virtio-blk bind  (virtio_blk) `wired`
- DV-C23 SCSI sd bind  (wubu_drv_sd) `wired`
- DV-C24 SCSI sr (optical) bind `open`
- DV-C25 SD/MMC bind  (wubu_drv_sd) `wired`
- DV-C26 eMMC bind `open`
- DV-C27 CF card bind `open`
- DV-C28 nvme-cli compat bind `open`
- DV-C29 fdisk/gdisk compat bind `open`
- DV-C30 mkfs ext4 bind `open`
- DV-C31 mkfs xfs bind `open`
- DV-C32 mkfs btrfs bind `open`
- DV-C33 FAT32 bind  (wubu_fat32) `wired`
- DV-C34 ext4 bind `open`
- DV-C35 xfs bind `open`
- DV-C36 btrfs bind `open`
- DV-C37 NTFS bind `open`
- DV-C38 exFAT bind `open`
- DV-C39 ZFS bind `open`
- DV-C40 APFS read bind `open`
- DV-C41 ISO9660 bind `open`
- DV-C42 UDF bind `open`
- DV-C43 RAID0 bind `wired`  (wubu_mdraid)
- DV-C44 RAID1 bind `wired`  (wubu_mdraid)
- DV-C45 RAID5 bind `wired`  (wubu_mdraid)
- DV-C46 RAID6 bind `wired`  (wubu_mdraid)
- DV-C47 LVM bind `wired`  (wubu_lvm)
- DV-C48 dm-crypt bind `open`
- DV-C49 LUKS bind `open`
- DV-C50 Intel RST detect  (rst_warning) `wired`
- DV-C51 Intel RST->AHCI route `wired`
- DV-C52 SSD TRIM auto  (fstab discard) `wired`
- DV-C53 SSD overprovision bind `open`
- DV-C54 HDD head parking bind `open`
- DV-C55 HDD APM bind `open`
- DV-C56 disk SMART read `open`
- DV-C57 disk health monitor `open`
- DV-C58 disk temperature read `open`
- DV-C59 disk spin-down bind `open`
- DV-C60 disk write cache bind `wired`  (wubu_bcache)
- DV-C61 fsck auto-repair `open`
- DV-C62 mount auto-detect `open`
- DV-C63 /etc/fstab gen  (trim_config) `wired`
- DV-C64 swap bind  (wubu_swap) `wired`
- DV-C65 zram swap bind `open`
- DV-C66 IO scheduler auto `open`
- DV-C67 noop scheduler SSD `open`
- DV-C68 mq-deadline bind `open`
- DV-C69 blk-mq enable `open`
- DV-C70 block queue depth `wired`
- DV-C71 readahead tune `open`
- DV-C72 writeback throttle `open`
- DV-C73 discard=async bind `open`
- DV-C74 trim timer bind  (fstrim) `wired`
- DV-C75 partition table parse `open`
- DV-C76 GPT parse `open`
- DV-C77 MBR parse `open`
- DV-C78 boot device detect `open`
- DV-C79 root fs mount `open`
- DV-C80 tmpfs bind `open`
- DV-C81 ramdisk bind  (wubu_ramdisk) `wired`
- DV-C82 loop device bind `open`
- DV-C83 device mapper bind `wired`  (wubu_uas)
- DV-C84 storage hotplug bind `open`
- DV-C85 NVMe hotplug bind `open`
- DV-C86 SATA hotplug bind `open`
- DV-C87 USB hotplug bind `open`
- DV-C88 io_uring bind `open`
- DV-C89 aio bind `open`
- DV-C90 direct I/O bind `open`
- DV-C91 buffered I/O bind `open`
- DV-C92 fsync flush bind `open`
- DV-C93 storage benchmark `open`
- DV-C94 storage error log `open`
- DV-C95 storage LED blink `open`
- DV-C96 RAID rebuild bind `wired`  (wubu_mdraid)
- DV-C97 disk clone bind `open`
- DV-C98 full-disk erase bind `open`
- DV-C99 storage kernel params `wired`
- DV-C100 storage vendor matrix wired `wired`

Status: open. STORAGE-DRIVER-7HOP.md

## DV-Network

- DV-D01 Intel AX200 bind iwlwifi  (2720) `wired`
- DV-D02 Intel AX201 bind iwlwifi  (2713) `wired`
- DV-D03 Intel AX210 bind iwlwifi  (2723) `wired`
- DV-D04 Intel AX211 bind iwlwifi  (2724) `wired`
- DV-D05 Intel WiFi 6E bind iwlwifi `wired`
- DV-D06 Intel WiFi 7 bind iwlwifi  (BE200) `wired`
- DV-D07 Intel power_save=0  (ps_disable) `wired`
- DV-D08 Realtek rtl8821ce bind rtl8821ce  (C821) `wired`
- DV-D09 Realtek rtl8822ce bind rtl8821ce  (C822) `wired`
- DV-D10 Realtek rtl88x2bu bind dkms `open`
- DV-D11 Realtek rtl8xxxu bind `open`
- DV-D12 Realtek ips=0 `wired`
- DV-D13 MediaTek mt7921e bind  (7961) `wired`
- DV-D14 MediaTek mt7922 bind `wired`
- DV-D15 MediaTek mt7925e bind  (7902) `wired`
- DV-D16 MediaTek mt76 bind  (0616) `wired`
- DV-D17 MediaTek disable_aspm=1 `wired`
- DV-D18 Broadcom brcmfmac bind `wired`
- DV-D19 Broadcom brcm4366 bind `wired`
- DV-D20 Broadcom legacy b43 bind `open`
- DV-D21 Atheros ath9k bind  (AR9285) `wired`
- DV-D22 Atheros ath10k bind  (QCA6174) `wired`
- DV-D23 Atheros ath11k bind  (QCA6390) `wired`
- DV-D24 Qualcomm wifi bind  (QCA6490) `wired`
- DV-D25 Qualcomm FastConnect bind  (QCA6490 Gen2) `wired`
- DV-D26 Intel igc 2.5G bind igc  (125C) `wired`
- DV-D27 Intel I225-V bind igc `wired`
- DV-D28 Intel I226-V bind igc  (125C) `wired`
- DV-D29 Intel I219-LM bind e1000e `open`
- DV-D30 Intel e1000e bind `wired`
- DV-D31 Intel e1000 bind `open`
- DV-D32 Realtek r8169 1G bind r8169 `wired`
- DV-D33 Realtek r8125 2.5G bind r8168  (8125) `wired`
- DV-D34 Realtek r8168-dkms bind `wired`
- DV-D35 Broadcom tg3 bind `wired`
- DV-D36 Marvell mvneta bind `wired`
- DV-D37 Mellanox mlx5 bind `wired`
- DV-D38 Virtio-net bind  (virtio_net) `wired`
- DV-D39 10GbE NIC bind `open`
- DV-D40 25GbE NIC bind `open`
- DV-D41 Wi-Fi power-save disable  (ps_disable) `wired`
- DV-D42 Wi-Fi DPM latency balance `open`
- DV-D43 Bluetooth BLE bind `open`
- DV-D44 Bluetooth 5.3 bind `open`
- DV-D45 Bluetooth 6.0 bind `open`
- DV-D46 Bluetooth audio A2DP `wired`
- DV-D47 Ethernet ring buffer tune `open`
- DV-D48 napi poll tune `open`
- DV-D49 network queue depth `open`
- DV-D50 offload enable `open`
- DV-D51 checksum offload bind `open`
- DV-D52 TSO/GSO enable `open`
- DV-D53 IPv6 bind `open`
- DV-D54 TCP fast open bind `open`
- DV-D55 UDP bind `open`
- DV-D56 DNS resolver bind `open`
- DV-D57 DHCP client bind `open`
- DV-D58 Wi-Fi scan bind `open`
- DV-D59 Wi-Fi connect bind `open`
- DV-D60 Wi-Fi roaming bind `open`
- DV-D61 Wi-Fi 6 OFDMA bind `open`
- DV-D62 Wi-Fi 7 MLO bind `open`
- DV-D63 hotspot/AP mode bind `open`
- DV-D64 mesh network bind `open`
- DV-D65 VPN tun bind `open`
- DV-D66 firewall nftables bind `open`
- DV-D67 network bridge bind `open`
- DV-D68 veth pair bind `open`
- DV-D69 network namespace bind `open`
- DV-D70 traffic shaping bind `open`
- DV-D71 QoS classify bind `open`
- DV-D72 network benchmark `open`
- DV-D73 ping latency measure `open`
- DV-D74 packet loss detect `open`
- DV-D75 Wi-Fi signal read `open`
- DV-D76 Wi-Fi rate adapt bind `open`
- DV-D77 Wi-Fi channel hop `open`
- DV-D78 ethernet speed detect `open`
- DV-D79 link status detect `open`
- DV-D80 NIC temperature read `open`
- DV-D81 NIC power save `open`
- DV-D82 wake-on-lan bind `open`
- DV-D83 network boot PXE bind `open`
- DV-D84 802.1x auth bind `open`
- DV-D85 WPA3 SAE bind `open`
- DV-D86 WPA2 PSK bind `open`
- DV-D87 wireless firmware load `open`
- DV-D88 wireless firmware check `wired`
- DV-D89 network module autoload `wired`
- DV-D90 NIC PCI detect  (class 0x02) `wired`
- DV-D91 Wi-Fi subclass 0x80 detect `wired`
- DV-D92 ethernet subclass 0x00 detect `wired`
- DV-D93 2.5GbE subclass detect `wired`
- DV-D94 network vendor matrix `wired`
- DV-D95 multi-homed routing `open`
- DV-D96 load balancing bind `open`
- DV-D97 failover link bind `open`
- DV-D98 latency-optimized mode `wired`
- DV-D99 power-save auto decision `wired`
- DV-D100 network full-stack unified `wired`

Status: 19 wired, 81 open. research/NETWORK-DRIVER-7HOP.md

## DV-Input

- DV-E01 Xbox 360 wired bind xpad  (028E) `wired`
- DV-E02 Xbox One wired bind xpad  (02DD) `wired`
- DV-E03 Xbox Series USB bind xpad  (0B12) `wired`
- DV-E04 Xbox wireless BLE bind xpadneo  (0B13) `wired`
- DV-E05 Xbox One S bind xpad  (02D1) `wired`
- DV-E06 Xbox Series wireless bind xpadneo  (0B22) `wired`
- DV-E07 DualSense bind hid-playstation  (0CE6) `wired`
- DV-E08 DualSense Edge bind hid-playstation  (0DF2) `wired`
- DV-E09 DualShock 4 bind hid-playstation  (05C4) `wired`
- DV-E10 DualShock 4 v2 bind hid-playstation  (0BA0) `wired`
- DV-E11 DualSense BT bind hid-playstation  (0CD0) `wired`
- DV-E12 Switch Pro bind hid-nintendo  (2009) `wired`
- DV-E13 Joy-Con bind hid-nintendo `open`
- DV-E14 DragonRise bind hid_dr  (0079) `wired`
- DV-E15 Generic USB gamepad bind hid-generic `wired`
- DV-E16 8BitDo controller bind hid-nintendo `open`
- DV-E17 Fight stick bind xpad `open`
- DV-E18 Racing wheel bind hid-microsoft `open`
- DV-E19 Keyboard bind hid  (input.c) `wired`
- DV-E20 Mouse bind hid  (input.c) `wired`
- DV-E21 PS/2 keyboard bind ps2  (ps2.c) `wired`
- DV-E22 PS/2 mouse bind ps2  (ps2.c) `wired`
- DV-E23 Touchpad bind hid-multitouch `open`
- DV-E24 Touchscreen bind hid `open`
- DV-E25 Trackpoint bind psmouse `open`
- DV-E26 Digitizer/pen bind hid `open`
- DV-E27 Haptic feedback bind `open`
- DV-E28 Gyro/IMU bind `open`
- DV-E29 RGB keyboard bind OpenRGB `open`
- DV-E30 RGB mouse bind OpenRGB `open`
- DV-E31 Keyboard backlight bind /sys/class/leds `open`
- DV-E32 Mouse polling 1000Hz bind  (input.c) `wired`
- DV-E33 Mouse polling 500Hz bind `wired`
- DV-E34 Mouse polling 2000Hz cap `wired`
- DV-E35 Mouse acceleration bind `open`
- DV-E36 Mouse DPI detect `open`
- DV-E37 Mouse gesture bind `open`
- DV-E38 Gamepad rumble bind `open`
- DV-E39 Gamepad deadzone bind `wired`  (wubu_gamepaddz)
- DV-E40 Gamepad button map bind `wired`  (wubu_gamepadbm)
- DV-E41 Gamepad trigger analog bind `open`
- DV-E42 Keyboard NKRO bind `open`
- DV-E43 Keyboard repeat rate `open`
- DV-E44 Keyboard layout map `open`
- DV-E45 LED caps/num lock `open`
- DV-E46 Media keys bind `open`
- DV-E47 Fn key bind `open`
- DV-E48 Power button bind `open`
- DV-E49 Volume keys bind `open`
- DV-E50 Brightness keys bind `open`
- DV-E51 Steam Deck controls bind `open`
- DV-E52 Steam Deck trackpad bind `open`
- DV-E53 Steam Deck gyro bind `open`
- DV-E54 Steam Input bind `open`
- DV-E55 SC Controller bind `open`
- DV-E56 Wiimote bind hid-wiimote `open`
- DV-E57 PS3 Sixaxis bind hid-sony `open`
- DV-E58 PS Vita bind hid-sony `open`
- DV-E59 USB HID report parse `wired`
- DV-E60 HID descriptor parse `open`
- DV-E61 Input event queue  (input.c) `wired`
- DV-E62 Input device register `open`
- DV-E63 evdev bind `open`
- DV-E64 uinput bind `open`
- DV-E65 libinput bind `open`
- DV-E66 input remap bind `open`
- DV-E67 controller hotplug bind `open`
- DV-E68 BLE controller hotplug bind `open`
- DV-E69 USB HID hotplug bind `open`
- DV-E70 input latency measure `open`
- DV-E71 input priority realtime `open`
- DV-E72 input IRQ bind `open`
- DV-E73 touchpad gesture bind `open`
- DV-E74 pinch zoom bind `open`
- DV-E75 scroll momentum bind `open`
- DV-E76 mouse warp bind `open`
- DV-E77 relative motion bind `open`
- DV-E78 absolute position bind `open`
- DV-E79 pressure sensitivity bind `open`
- DV-E80 tilt sensitivity bind `open`
- DV-E81 keyboard input filtering `open`
- DV-E82 gamepad input filtering `open`
- DV-E83 input calibration bind `open`
- DV-E84 controller calibration bind `open`
- DV-E85 LED effects bind `open`
- DV-E86 ambient light input `open`
- DV-E87 proximity sensor input `open`
- DV-E88 fingerprint reader bind `open`
- DV-E89 camera input bind `open`
- DV-E90 microphone input bind `open`
- DV-E91 input power management `open`
- DV-E92 controller battery read `open`
- DV-E93 input module autoload `wired`
- DV-E94 USB vendor detect `wired`
- DV-E95 BLE detect `wired`
- DV-E96 driver routing hint `wired`
- DV-E97 safe poll cap `wired`
- DV-E98 input vendor matrix `wired`
- DV-E99 controller BT profile `wired`
- DV-E100 input full-stack unified `wired`

Status: open. INPUT-DRIVER-7HOP.md

## DV-Display
### Status: 10 wired, 90 open

- DV-F01 DRM/KMS bind `wired`
- DV-F02 DRM master bind `open`
- DV-F03 Wayland compositor bind `open`
- DV-F04 X11 compat bind `open`
- DV-F05 VGA output bind `open`
- DV-F06 HDMI output bind `wired`
- DV-F07 DisplayPort output bind `wired`
- DV-F08 DVI output bind `open`
- DV-F09 eDP laptop panel bind `wired`
- DV-F10 LVDS legacy panel bind `open`
- DV-F11 VGA 640x480 legacy bind `open`
- DV-F12 1024x768 VGA bind `open`
- DV-F13 1080p HDMI bind `open`
- DV-F14 1440p bind `open`
- DV-F15 4K HDMI bind `open`
- DV-F16 5K bind `open`
- DV-F17 8K bind `open`
- DV-F18 21:9 ultrawide bind `open`
- DV-F19 32:9 superwide bind `open`
- DV-F20 HDR output bind `open`
- DV-F21 10-bit color bind `open`
- DV-F22 12-bit color bind `open`
- DV-F23 VRR G-Sync bind `open`
- DV-F24 FreeSync bind `wired`
- DV-F25 refresh rate detect `open`
- DV-F26 60Hz bind `wired`
- DV-F27 120Hz bind `open`
- DV-F28 144Hz bind `open`
- DV-F29 240Hz bind `open`
- DV-F30 multi-monitor bind `open`
- DV-F31 dual monitor bind `open`
- DV-F32 triple monitor bind `open`
- DV-F33 display mirror bind `open`
- DV-F34 display extend bind `open`
- DV-F35 scaling bind `open`
- DV-F36 fractional scaling bind `open`
- DV-F37 dpi detect bind `open`
- DV-F38 EDID parse bind `wired`
- DV-F39 display name detect `wired`
- DV-F40 panel auto-detect `wired`
- DV-F41 tty text console bind `open`
- DV-F42 framebuffer console bind `open`
- DV-F43 early boot console bind  (console.c) `wired`
- DV-F44 splash screen bind `open`
- DV-F45 VT switch bind `open`
- DV-F46 GPU modeset auto `open`
- DV-F47 KMS atomic bind `wired`
- DV-F48 plane bind `open`
- DV-F49 cursor plane bind `open`
- DV-F50 overlay plane bind `open`
- DV-F51 scanout buffer bind `open`
- DV-F52 page flip bind `open`
- DV-F53 vsync bind `open`
- DV-F54 present queue bind `open`
- DV-F55 display latency measure `open`
- DV-F56 tearing fix bind `open`
- DV-F57 triple buffering bind `open`
- DV-F58 backlight control bind `open`
- DV-F59 panel power bind `open`
- DV-F60 dpms suspend bind `open`
- DV-F61 display sleep bind `open`
- DV-F62 HDCP content protect bind `wired`
- DV-F63 DSC compression bind `wired`
- DV-F64 eDP-PSR panel self refresh `open`
- DV-F65 display color profile bind `open`
- DV-F66 gamma LUT bind `open`
- DV-F67 night light bind `open`
- DV-F68 display brightness hotkey `open`
- DV-F69 external display hotplug bind `open`
- DV-F70 dock display bind `open`
- DV-F71 projector mode bind `open`
- DV-F72 presentation mirror bind `open`
- DV-F73 touch display bind `open`
- DV-F74 display rotation bind `open`
- DV-F75 portrait mode bind `open`
- DV-F76 auto-orientation bind `open`
- DV-F77 virtual display bind `open`
- DV-F78 headless mode bind `open`
- DV-F79 display driver autoload `wired`
- DV-F80 DRM module detect `wired`
- DV-F81 connector detect `wired`
- DV-F82 encoder detect `open`
- DV-F83 crtc bind `open`
- DV-F84 display clock bind `open`
- DV-F85 monitor EDID hash `open`
- DV-F86 display benchmark `open`
- DV-F87 render latency measure `open`
- DV-F88 compositor GL bind `open`
- DV-F89 Vulkan WSI bind `open`
- DV-F90 Wayland surface bind `open`
- DV-F91 XWayland compat bind `open`
- DV-F92 display power state read `open`
- DV-F93 monitor power state read `open`
- DV-F94 DDC/CI control bind `open`
- DV-F95 monitor OSD bind `open`
- DV-F96 display port MST bind `wired`
- DV-F97 daisy chain display bind `open`
- DV-F98 display vendor matrix `wired`
- DV-F99 display full-stack `open`
- DV-F100 display auto-detect wired `wired`

Status: 10 wired, 90 open. research/DISPLAY-DRIVER-7HOP.md

## DV-USB

- DV-G01 xHCI USB3 bind  (wubu_xhci) `wired`
- DV-G02 EHCI USB2 bind `wired`
- DV-G03 OHCI USB1 bind `wired`
- DV-G04 USB-C bind `wired`
- DV-G05 USB4 bind `wired`
- DV-G06 Thunderbolt bind `wired`
- DV-G07 USB HID bind  (usb_hid) `wired`
- DV-G08 USB mass storage bind  (usb_msc) `wired`
- DV-G09 USB audio bind  (snd_usb_audio) `wired`
- DV-G10 USB video UVC bind `wired`
- DV-G11 USB network bind `wired`
- DV-G12 USB Bluetooth bind  (usb_bt) `wired`
- DV-G13 USB serial bind `wired`
- DV-G14 USB printer bind `wired`
- DV-G15 USB hub bind `wired`
- DV-G16 USB3.1 gen2 bind `open`
- DV-G17 USB3.2 bind `open`
- DV-G18 USB power delivery bind `open`
- DV-G19 USB charging detect bind `open`
- DV-G20 USB OTG bind `wired`
- DV-G21 USB gadget bind `wired`
- DV-G22 USB host controller detect `wired`
- DV-G23 USB device enumerate `open`
- DV-G24 USB descriptor parse `open`
- DV-G25 USB config parse `open`
- DV-G26 USB speed detect `open`
- DV-G27 USB bus power bind `open`
- DV-G28 USB suspend bind `open`
- DV-G29 USB wake bind `open`
- DV-G30 USB hotplug bind `open`
- DV-G31 USB-C alternate mode bind `open`
- DV-G32 USB-C displayport alt `wired`
- DV-G33 USB-C thunderbolt alt `wired`
- DV-G34 USB vendor id read `open`
- DV-G35 USB device id read `open`
- DV-G36 USB class detect `open`
- DV-G37 usb-ids database bind `open`
- DV-G38 USB legacy keyboard bind `open`
- DV-G39 USB legacy mouse bind `open`
- DV-G40 USB boot HID bind `open`
- DV-G41 USB3 performance tune `open`
- DV-G42 USB transfer size tune `open`
- DV-G43 USB urb queue bind `open`
- DV-G44 USB DMA bind `open`
- DV-G45 USB error recovery bind `open`
- DV-G46 USB reset bind `open`
- DV-G47 USB port power bind `open`
- DV-G48 USB device tree bind `open`
- DV-G49 USB topology view `open`
- DV-G50 USB firmware update bind `open`
- DV-G51 USB DFU bind `open`
- DV-G52 USB MIDI bind `open`
- DV-G53 USB game controller bind `wired`
- DV-G54 USB card reader bind `open`
- DV-G55 USB fingerprint bind `open`
- DV-G56 USB smartcard bind `open`
- DV-G57 USB camera bind `open`
- DV-G58 USB dongle bind `open`
- DV-G59 USB wifi dongle bind `open`
- DV-G60 USB BT dongle bind `open`
- DV-G61 USB sound dongle bind `open`
- DV-G62 USB-C dock bind `open`
- DV-G63 USB display dock bind `open`
- DV-G64 USB hub power bind `open`
- DV-G65 USB overcurrent protect `open`
- DV-G66 USB enumeration timeout `open`
- DV-G67 USB autosuspend bind `wired`
- DV-G68 USB remote wake bind `open`
- DV-G69 USB benchmark `open`
- DV-G70 USB speed test `open`
- DV-G71 USB device fs bind `open`
- DV-G72 usbfs bind `open`
- DV-G73 libusb compat bind `open`
- DV-G74 USB class driver bind `open`
- DV-G75 USB subsystem vendor matrix `open`
- DV-G76 USB module autoload `open`
- DV-G77 USB power drain detect `open`
- DV-G78 USB device LED bind `open`
- DV-G79 USB audio sync bind `open`
- DV-G80 USB bulk transfer bind `open`
- DV-G81 USB interrupt transfer bind `open`
- DV-G82 USB isochronous bind `open`
- DV-G83 USB control transfer bind `open`
- DV-G84 USB endpoint parse bind `open`
- DV-G85 USB interface parse bind `open`
- DV-G86 USB alt setting bind `open`
- DV-G87 USB class codes bind `open`
- DV-G88 USB vendor class bind `open`
- DV-G89 USB device removal bind `open`
- DV-G90 USB driver unbind bind `open`
- DV-G91 USB device quirks bind `open`
- DV-G92 USB blacklist bind `open`
- DV-G93 USB whitelist bind `open`
- DV-G94 USB secure boot bind `open`
- DV-G95 USB physical port map `open`
- DV-G96 USB-C orientation detect `open`
- DV-G97 USB-C cable detect `open`
- DV-G98 USB full-stack  (xhci) `wired`
- DV-G99 USB controller matrix `wired`
- DV-G100 USB auto-config `open`

Status: 24 wired, 76 open. research/USB-DRIVER-7HOP.md

## DV-Power

- DV-H01 ACPI bind  (wubu_acpi) `wired`
- DV-H02 battery bind  (wubu_drv_battery) `wired`
- DV-H03 battery percentage read `open`
- DV-H04 battery health read `open`
- DV-H05 battery cycle count `open`
- DV-H06 thermal bind  (wubu_drv_thermal) `wired`
- DV-H07 CPU temp read `open`
- DV-H08 GPU temp read `open`
- DV-H09 disk temp read `open`
- DV-H10 fan control bind `open`
- DV-H11 fan speed read `wired`  (wubu_fantml)
- DV-H12 CPU governor bind `wired`
- DV-H13 performance governor `wired`
- DV-H14 powersave governor `wired`
- DV-H15 balanced governor `wired`
- DV-H16 CPU frequency read `wired`
- DV-H17 CPU frequency scale `open`
- DV-H18 P-state bind `wired`
- DV-H19 C-state bind `wired`
- DV-H20 intel_pstate bind `wired`
- DV-H21 amd_pstate bind `wired`
- DV-H22 ACPI cpufreq bind `wired`
- DV-H23 suspend bind `open`
- DV-H24 hibernate bind `open`
- DV-H25 sleep S3 bind `open`
- DV-H26 deep sleep S4 bind `open`
- DV-H27 power off bind `open`
- DV-H28 reboot bind `open`
- DV-H29 power button bind `open`
- DV-H30 lid switch bind `open`
- DV-H31 wake source bind `open`
- DV-H32 RTC wake bind  (wubu_rtc) `wired`
- DV-H33 timer wake bind `open`
- DV-H34 CPU package power read `open`
- DV-H35 GPU power read `open`
- DV-H36 DRAM power read `open`
- DV-H37 SSD power read `open`
- DV-H38 energy meter read `open`
- DV-H39 power profile switch `open`
- DV-H40 battery saver mode `open`
- DV-H41 high performance mode `open`
- DV-H42 TDP limit set `wired`  (wubu_voltagectl)
- DV-H43 undervolt set `wired`  (wubu_voltagectl)
- DV-H44 thermal throttle detect `wired`  (wubu_fantml)
- DV-H45 thermal emergency shutoff `open`
- DV-H46 fan curve set `wired`  (wubu_fantml)
- DV-H47 passive cooling bind `open`
- DV-H48 active cooling bind `open`
- DV-H49 keyboard backlight power `open`
- DV-H50 display backlight power `open`
- DV-H51 USB power management `open`
- DV-H52 WiFi power save `wired`
- DV-H53 BT power save `open`
- DV-H54 NVMe APST `wired`
- DV-H55 SATA link power `open`
- DV-H56 PCIe ASPM bind `open`
- DV-H57 PCIe link state `open`
- DV-H58 Intel SpeedStep bind `open`
- DV-H59 AMD Cool'n'Quiet bind `open`
- DV-H60 dynamic boost bind `open`
- DV-H61 power capping bind `open`
- DV-H62 RAPL bind `open`
- DV-H63 battery time estimate `open`
- DV-H64 charge control bind `open`
- DV-H65 charge limit set `open`
- DV-H66 USB-PD power read `open`
- DV-H67 adapter wattage read `open`
- DV-H68 SMPS efficiency `open`
- DV-H69 power diagnostics `open`
- DV-H70 power event log `open`
- DV-H71 hibernation image bind `open`
- DV-H72 resume from hibernate `open`
- DV-H73 suspend to RAM bind `open`
- DV-H74 suspend to disk bind `open`
- DV-H75 wake on AC bind `open`
- DV-H76 wake on USB bind `open`
- DV-H77 wake on LAN bind `open`
- DV-H78 wake on RTC bind `open`
- DV-H79 acpi battery notify `open`
- DV-H80 acpi thermal notify `open`
- DV-H81 acpi lid notify `open`
- DV-H82 acpi button notify `open`
- DV-H83 power management module `open`
- DV-H84 power state machine `open`
- DV-H85 suspend sequence `open`
- DV-H86 resume sequence `open`
- DV-H87 power regression test `open`
- DV-H88 power benchmark `open`
- DV-H89 watts per FPS measure `open`
- DV-H90 efficiency tuning `open`
- DV-H91 Steam Deck power bind `open`
- DV-H92 laptop power bind `open`
- DV-H93 desktop power bind `open`
- DV-H94 power vendor matrix `wired`
- DV-H95 power module autoload `wired`
- DV-H96 ACPI platform detect `wired`
- DV-H97 battery present detect `wired`
- DV-H98 thermal present detect `wired`
- DV-H99 power full-stack `wired`
- DV-H100 power auto-balance `open`

Status: 22 wired, 78 open. research/POWER-DRIVER-7HOP.md

## DV-Peripherals

- DV-I01 PS/2 keyboard bind  (ps2.c) `wired`
- DV-I02 PS/2 mouse bind  (ps2.c) `wired`
- DV-I03 AT keyboard legacy bind `open`
- DV-I04 Serial COM1 bind  (wubu_serial) `wired`
- DV-I05 Serial COM2 bind `open`
- DV-I06 16550 UART bind `wired`
- DV-I07 8250 UART bind `wired`
- DV-I08 Parallel LPT bind `wired`
- DV-I09 EPP parallel bind `open`
- DV-I10 ECP parallel bind `open`
- DV-I11 Floppy disk bind `open`
- DV-I12 Legacy joystick bind `open`
- DV-I13 Game port bind `open`
- DV-I14 IrDA infrared bind `open`
- DV-I15 FireWire IEEE1394 bind `open`
- DV-I16 eSATA legacy bind `open`
- DV-I17 Smart card reader bind `open`
- DV-I18 Barcode reader bind `open`
- DV-I19 Card reader bind `open`
- DV-I20 Trackball bind `open`
- DV-I21 Drawing tablet bind `open`
- DV-I22 Digitizer bind `open`
- DV-I23 Webcam bind `open`
- DV-I24 Microphone bind `open`
- DV-I25 Speaker bind `open`
- DV-I26 Headphones bind `open`
- DV-I27 Headset bind `open`
- DV-I28 Biometric scanner bind `open`
- DV-I29 Hardware dongle bind `open`
- DV-I30 Expansion card bind `open`
- DV-I31 PCI slot enumerate `open`
- DV-I32 PCIe slot enumerate `open`
- DV-I33 AGP legacy slot bind `open`
- DV-I34 ISA legacy slot bind `open`
- DV-I35 M.2 slot bind `open`
- DV-I36 NVMe M.2 detect `wired`
- DV-I37 SATA M.2 detect `wired`
- DV-I38 DIMM memory detect `open`
- DV-I39 ECC memory detect `open`
- DV-I40 memory SPD read `open`
- DV-I41 ROM detect `open`
- DV-I42 CMOS RTC bind  (wubu_rtc) `wired`
- DV-I43 Watchdog timer bind  (wubu_wdt) `wired`
- DV-I44 HPET bind  (wubu_hpet) `wired`
- DV-I45 TPM bind  (wubu_attest) `wired`
- DV-I46 SMBIOS/DMI bind  (wubu_smbios) `wired`
- DV-I47 ACPI FADT bind  (wubu_acpi) `wired`
- DV-I48 PCI ROM read `open`
- DV-I49 VGA BIOS read `open`
- DV-I50 UEFI vars bind `open`
- DV-I51 Legacy BIOS int10 bind `open`
- DV-I52 CMOS battery read `open`
- DV-I53 Jumper detect `open`
- DV-I54 Hardware monitor bind `wired`
- DV-I55 Voltage sensor read `wired`
- DV-I56 Power supply detect  (battery) `wired`
- DV-I57 Fan tach read `wired`
- DV-I58 RGB controller bind `open`
- DV-I59 Addressable RGB bind `open`
- DV-I60 ARGB fan hub bind `open`
- DV-I61 Liquid cooler bind `open`
- DV-I62 AIO pump bind `open`
- DV-I63 Case lighting bind `open`
- DV-I64 LCD info panel bind `open`
- DV-I65 OLED display bind `open`
- DV-I66 status LED bind `open`
- DV-I67 activity LED bind `open`
- DV-I68 IR receiver bind `open`
- DV-I69 Remote control bind `open`
- DV-I70 RF remote bind `open`
- DV-I71 Wireless charging bind `open`
- DV-I72 Qi charger bind `open`
- DV-I73 Hall sensor bind `open`
- DV-I74 Accelerometer bind `open`
- DV-I75 Magnetometer bind `open`
- DV-I76 Gyroscope bind `open`
- DV-I77 Barometer bind `open`
- DV-I78 Temperature sensor bind `wired`
- DV-I79 Humidity sensor bind `open`
- DV-I80 Light sensor bind `open`
- DV-I81 Proximity bind `open`
- DV-I82 GPS receiver bind `open`
- DV-I83 NFC reader bind `open`
- DV-I84 RFID reader bind `open`
- DV-I85 Touch controller bind `open`
- DV-I86 Digitizer controller bind `open`
- DV-I87 Haptic driver bind `open`
- DV-I88 Buzzer bind `open`
- DV-I89 Vibra motor bind `open`
- DV-I90 peripheral module autoload `wired`
- DV-I91 legacy bus detect `open`
- DV-I92 platform device detect `open`
- DV-I93 MMIO device detect `open`
- DV-I94 I/O port device detect `open`
- DV-I95 peripheral vendor matrix `wired`
- DV-I96 peripheral full-stack `wired`
- DV-I97 PS/2 vendor detect `wired`
- DV-I98 serial port detect `wired`
- DV-I99 watchdog detect `wired`
- DV-I100 peripheral auto-config `wired`

Status: 26 wired, 74 open. research/PERIPHERAL-DRIVER-7HOP.md

## DV-Virtual

- DV-J01 virtio-blk bind  (virtio_blk) `wired`
- DV-J02 virtio-net bind  (virtio_net) `wired`
- DV-J03 virtio-gpu bind  (virtio_gpu) `wired`
- DV-J04 virtio-input bind  (virtio_input) `wired`
- DV-J05 virtio-balloon bind `open`
- DV-J06 virtio-scsi bind `open`
- DV-J07 virtio-console bind `open`
- DV-J08 virtio-9p bind `open`
- DV-J09 virtio-rng bind `open`
- DV-J10 virtio-snd bind `open`
- DV-J11 virtio-fs bind `open`
- DV-J12 virtio iommu bind `open`
- DV-J13 VirtIO mmio bind `open`
- DV-J14 VirtIO pci bind `wired`
- DV-J15 QEMU virtual GPU bind `open`
- DV-J16 QEMU std VGA bind `open`
- DV-J17 QEMU ramfb bind `open`
- DV-J18 QEMU bochs bind `open`
- DV-J19 QEMU ehci bind `open`
- DV-J20 QEMU xhci bind `open`
- DV-J21 KVM bind `wired`
- DV-J22 KVM paravirt clock bind `wired`
- DV-J23 hyperv bind  (hyperv detect) `wired`
- DV-J24 Hyper-V storage bind `wired`
- DV-J25 Hyper-V net bind `wired`
- DV-J26 Hyper-V time bind `open`
- DV-J27 VMWare bind  (vmware detect) `wired`
- DV-J28 VMware SVGA bind `wired`
- DV-J29 VMware tools bind `open`
- DV-J30 Xen PV bind `wired`
- DV-J31 Xen blkfront bind `wired`
- DV-J32 Xen netfront bind `wired`
- DV-J33 VMware vmxnet3 bind `wired`
- DV-J34 VirtIO balloon balloon `open`
- DV-J35 paravirtual MMU bind `open`
- DV-J36 paravirt steal time bind `open`
- DV-J37 cloud-init bind `open`
- DV-J38 AWS Nitro bind `open`
- DV-J39 GCP virtio bind `open`
- DV-J40 Azure hyperv bind `open`
- DV-J41 GPU passthrough vfio bind `open`
- DV-J42 VFIO pci bind `open`
- DV-J43 VFIO mdev bind `open`
- DV-J44 PCI passthrough bind `open`
- DV-J45 USB passthrough bind `open`
- DV-J46 NVMe passthrough bind `open`
- DV-J47 iommu bind  (wubu_iommu) `wired`
- DV-J48 IOMMU vfio bind `open`
- DV-J49 emulated HPET bind `wired`
- DV-J50 emulated RTC bind `wired`
- DV-J51 emulated PIC bind  (interrupt_pic) `wired`
- DV-J52 emulated APIC bind  (interrupt_apic) `wired`
- DV-J53 emulated PIT bind  (interrupt_pit) `wired`
- DV-J54 emulated PS/2 bind `wired`
- DV-J55 emulated VGA bind `open`
- DV-J56 emulated NIC bind `open`
- DV-J57 emulated SATA bind `open`
- DV-J58 emulated NVRAM bind `open`
- DV-J59 UEFI firmware emu bind `open`
- DV-J60 BIOS emu int13 bind `open`
- DV-J61 hypervisor detect `wired`
- DV-J62 hypervisor CPUID leaf `wired`
- DV-J63 virtio detect `wired`
- DV-J64 virtio feature negotiate `open`
- DV-J65 virtio queue setup `open`
- DV-J66 virtio irq bind `open`
- DV-J67 virtio reset bind `open`
- DV-J68 virtio status read `open`
- DV-J69 virtual time source `open`
- DV-J70 KVM clock bind `open`
- DV-J71 stolen memory detect `open`
- DV-J72 shared memory bind `open`
- DV-J73 ballon stats read `open`
- DV-J74 virtio memory pressure `open`
- DV-J75 virtio version negotiate `open`
- DV-J76 legacy virtio bind `open`
- DV-J77 modern virtio bind `wired`
- DV-J78 container GPU bind `open`
- DV-J79 container net bind `open`
- DV-J80 container storage bind `open`
- DV-J81 VFIO container bind `open`
- DV-J82 IOMMU group bind `open`
- DV-J83 device passthrough `open`
- DV-J84 virtual PCI topology `open`
- DV-J85 virtual bus enumerate `open`
- DV-J86 VM state save bind `open`
- DV-J87 VM migrate bind `open`
- DV-J88 VM snapshot bind `open`
- DV-J89 hypervisor bench `open`
- DV-J90 paravirt perf `open`
- DV-J91 virtual full-stack `wired`
- DV-J92 virtual vendor matrix `wired`
- DV-J93 hyperv vs vmware vs kvm `wired`
- DV-J94 virtio module autoload `wired`
- DV-J95 virtio-pci detect `wired`
- DV-J96 IOMMU DMA bind `open`
- DV-J97 passthrough DMA bind `open`
- DV-J98 virtio memory balloon `open`
- DV-J99 hypervisor time sync `open`
- DV-J100 virtual auto-detect `wired`

Status: 33 wired, 67 open. research/VIRT-DRIVER-7HOP.md



## DV-Sensors (extension)

- DV-K01 IIO core bind  (industrialio) `wired`
- DV-K02 accelerometer bind  (st-accel) `wired`
- DV-K03 gyroscope bind  (st-gyro) `wired`
- DV-K04 magnetometer bind  (st-magn) `wired`
- DV-K05 IMU bind  (inv-mpu6050) `wired`
- DV-K06 barometer bind  (bmp280) `wired`
- DV-K07 ambient light bind  (apds9960) `wired`
- DV-K08 proximity bind  (apds9960) `wired`
- DV-K09 temperature bind  (st-humidity) `wired`
- DV-K10 humidity bind  (st-humidity) `wired`
- DV-K11 sensor bus detect `open`
- DV-K12 sensor power mgmt `open`
- DV-K13 sensor calibration `open`
- DV-K14 sensor FIFO/buffer `open`
- DV-K15 sensor events/triggers `open`

Status: 10 wired, 5 open. research/SENSOR-DRIVER-7HOP.md

## DV-CAN (extension)

- DV-L01 SocketCAN core bind  (socketcan) `wired`
- DV-L02 mcp251x bind  (mcp251x) `wired`
- DV-L03 mcp251xfd bind  (mcp251xfd) `wired`
- DV-L04 peak_usb bind  (peak_usb) `wired`
- DV-L05 esd_usb2 bind  (esd_usb2) `wired`
- DV-L06 gs_usb bind  (gs_usb) `wired`
- DV-L07 can327 bind  (can327) `wired`
- DV-L08 sja1000 bind  (sja1000) `wired`
- DV-L09 kvaser_usb bind  (kvaser_usb) `wired`
- DV-L10 vcan virtual bind `open`
- DV-L11 CAN FD bind `open`
- DV-L12 CAN XL bind `open`
- DV-L13 CAN raw socket bind `open`
- DV-L14 CAN filter bind `open`
- DV-L15 CAN bus-off recovery `open`

Status: 9 wired, 6 open. research/CAN-DRIVER-7HOP.md

## DV-Memory (extension)

- DV-M01 EDAC core bind  (edac_mc) `wired`
- DV-M02 i7core_edac bind  (i7core_edac) `wired`
- DV-M03 sb_edac bind  (sb_edac) `wired`
- DV-M04 skx_edac bind  (skx_edac) `wired`
- DV-M05 i10nm_edac bind  (i10nm_edac) `wired`
- DV-M06 amd64_edac bind  (amd64_edac) `wired`
- DV-M07 ECC corrected count `wired`
- DV-M08 ECC uncorrected count `wired`
- DV-M09 DIMM SPD read `open`
- DV-M10 DDR4 SPD bind `open`
- DV-M11 DDR5 SPD bind `open`
- DV-M12 ECC scrub bind `open`
- DV-M13 memory health telemetry `open`
- DV-M14 RAS feature bind `open`
- DV-M15 memory topology view `open`

Status: 8 wired, 7 open. research/MEM-DRIVER-7HOP.md


## DV-Accel (extension)

- DV-N01 accel subsystem bind  (accel) `wired`
- DV-N02 Intel IVPU bind  (ivpu) `wired`
- DV-N03 AMD XDNA bind  (amdxdna) `wired`
- DV-N04 Qualcomm Hexagon bind  (qaic) `wired`
- DV-N05 Google EdgeTPU bind  (edgetpu) `wired`
- DV-N06 DSP bind `open`
- DV-N07 VPU bind `open`
- DV-N08 FPGA accelerator bind `open`
- DV-N09 NPU inference runtime `open`
- DV-N10 NPU telemetry `open`

Status: 5 wired, 5 open. research/ACCEL-DRIVER-7HOP.md

## DV-Camera (extension)

- DV-O01 V4L2 core bind  (v4l2) `wired`
- DV-O02 uvcvideo bind  (uvcvideo) `wired`
- DV-O03 rkisp1 ISP bind  (rkisp1) `wired`
- DV-O04 imx219 sensor bind  (imx219) `wired`
- DV-O05 imx290 sensor bind  (imx290) `wired`
- DV-O06 ov5640 sensor bind  (ov5640) `wired`
- DV-O07 ov9281 sensor bind  (ov9281) `wired`
- DV-O08 imx8-isi bind  (imx8-isi) `wired`
- DV-O09 vimc virtual bind  (vimc) `wired`
- DV-O10 camera ISP pipeline `open`
- DV-O11 HDR capture `open`
- DV-O12 RAW Bayer capture `open`
- DV-O13 camera auto-exposure `open`
- DV-O14 camera auto-focus `open`
- DV-O15 camera metadata `open`

Status: 9 wired, 6 open. research/CAMERA-DRIVER-7HOP.md

## DV-Bluetooth (extension)

- DV-P01 BlueZ core bind  (bluetooth) `wired`
- DV-P02 btusb bind  (btusb) `wired`
- DV-P03 btintel bind  (btintel) `wired`
- DV-P04 btbcm bind  (btbcm) `wired`
- DV-P05 btrtl bind  (btrtl) `wired`
- DV-P06 btmtk bind  (btmtk) `wired`
- DV-P07 hci_uart bind  (hci_uart) `wired`
- DV-P08 LE Audio bind `wired`  (wubu_leaudioldr)
- DV-P09 Auracast broadcast `wired`  (wubu_auracast)
- DV-P10 BAP bind `wired`  (wubu_bap)
- DV-P11 HID over BT bind `open`
- DV-P12 BT audio A2DP `wired`  (wubu_bta2dp)
- DV-P13 BT classic bind `wired`  (wubu_btclassic)
- DV-P14 BT mesh bind `wired`  (wubu_btamesh)
- DV-P15 BT beacon bind `wired`  (wubu_btbeacon)

Status: 7 wired, 8 open. research/BT-DRIVER-7HOP.md


## DV-Codec (extension)

- DV-Q01 HD-Audio core bind  (snd_hda) `wired`
- DV-Q02 Realtek codec bind  (snd_hda_codec_realtek) `wired`
- DV-Q03 IDT codec bind  (snd_hda_codec_idt) `wired`
- DV-Q04 Cirrus codec bind  (snd_hda_codec_cirrus) `wired`
- DV-Q05 Conexant codec bind  (snd_hda_codec_conexant) `wired`
- DV-Q06 HDMI codec bind  (snd_hda_codec_hdmi) `wired`
- DV-Q07 ASoC core bind  (snd_soc) `wired`
- DV-Q08 wm8960 bind  (snd_soc_wm8960) `wired`
- DV-Q09 cs42l42 bind  (snd_soc_cs42l42) `wired`
- DV-Q10 rt5682 bind  (snd_soc_rt5682) `wired`
- DV-Q11 max98357a amp bind  (snd_soc_max98357a) `wired`
- DV-Q12 SOF DSP bind  (snd_sof) `wired`
- DV-Q13 codec power mgmt `open`
- DV-Q14 codec jack detect `open`
- DV-Q15 codec mixer routing `open`

Status: 12 wired, 3 open. research/CODEC-DRIVER-7HOP.md

## DV-RAID (extension)

- DV-R01 megaraid bind  (megaraid_sas) `wired`
- DV-R02 mpt3sas bind  (mpt3sas) `wired`
- DV-R03 smartpqi bind  (smartpqi) `wired`
- DV-R04 aacraid bind  (aacraid) `wired`
- DV-R05 mv_sas bind  (mv_sas) `wired`
- DV-R06 arcmsr bind  (arcmsr) `wired`
- DV-R07 3w-sas bind  (3w-sas) `wired`
- DV-R08 software RAID md bind  (md) `wired`
- DV-R09 RAID level detect `open`
- DV-R10 RAID rebuild bind `open`
- DV-R11 RAID hotspare bind `open`
- DV-R12 RAID cache policy `open`
- DV-R13 SAS topology view `open`
- DV-R14 RAID health telemetry `open`
- DV-R15 RAID benchmark `open`

Status: 8 wired, 7 open. research/RAID-DRIVER-7HOP.md

## DV-Fingerprint (extension)

- DV-S01 libfprint bind  (libfprint) `wired`
- DV-S02 Goodix bind  (goodixmoc) `wired`
- DV-S03 VFS/Synaptics bind  (vfs5011) `wired`
- DV-S04 EgisTec bind  (egis) `wired`
- DV-S05 AuthenTec bind  (authenc) `wired`
- DV-S06 Fingerprint Cards bind  (fpc1020) `wired`
- DV-S07 Elan bind  (elan-fp) `wired`
- DV-S08 fingerprint auth bind `open`
- DV-S09 biometric enrollment `open`
- DV-S10 liveness detect `open`
- DV-S11 touch sensor bind `open`
- DV-S12 fingerprint power mgmt `open`
- DV-S13 biometric security policy `open`
- DV-S14 face recognition bind `open`
- DV-S15 iris scan bind `open`

Status: 7 wired, 8 open. research/FINGERPRINT-DRIVER-7HOP.md


## DV-FPGA (extension)

- DV-T01 fpga-mgr bind  (fpga-mgr) `wired`
- DV-T02 xilinx decoupler bind  (xilinx-pr-decoupler) `wired`
- DV-T03 altera fpga bind  (altera-fpga2sdram) `wired`
- DV-T04 lattice bind  (lattice-ecp3) `wired`
- DV-T05 microsemi bind  (microsemi-spi) `wired`
- DV-T06 fpga-region DT bind  (of-fpga-region) `wired`
- DV-T07 fpga-bridge freeze bind  (altera-freeze-bridge) `wired`
- DV-T08 bitstream load `open`
- DV-T09 FPGA overlay `open`
- DV-T10 FPGA partial reconfig `open`

Status: 7 wired, 3 open. research/FPGA-DRIVER-7HOP.md

## DV-WiFi7 (extension)

- DV-U01 Intel BE200 bind  (iwlwifi) `wired`
- DV-U02 Intel BE201 bind  (iwlwifi) `wired`
- DV-U03 Qualcomm WCN7850 bind  (ath12k_pci) `wired`
- DV-U04 MediaTek MT7925 bind  (mt7925e) `wired`
- DV-U05 Realtek RTL8922 bind  (rtw89) `wired`
- DV-U06 Realtek RTL8852C bind  (rtw89) `wired`
- DV-U07 Broadcom BCM4389 bind  (brcmfmac) `wired`
- DV-U08 MLO support `open`
- DV-U09 6GHz band enable `open`
- DV-U10 320MHz channel `open`

Status: 7 wired, 3 open. research/WIFI7-DRIVER-7HOP.md

## DV-PMICAudio (extension)

- DV-V01 regulator subsystem bind  (regulator) `wired`
- DV-V02 qcom-pmic bind  (qcom-spmi-pmic) `wired`
- DV-V03 bq25890 charger bind  (bq25890) `wired`
- DV-V04 es9038 DAC bind  (es9038q2m) `wired`
- DV-V05 ak4490 DAC bind  (ak4490) `wired`
- DV-V06 pcm1792 DAC bind  (pcm1792) `wired`
- DV-V07 cs4398 DAC bind  (cs4398) `wired`
- DV-V08 tas5805 amp bind  (tas5805m) `wired`
- DV-V09 tpa3116 amp bind  (tpa3116) `wired`
- DV-V10 max98357 amp bind  (max98357a) `wired`
- DV-V11 PMIC charge control `open`
- DV-V12 fuel gauge bind `open`
- DV-V13 PMIC watchdog `open`
- DV-V14 DAC sample rate `open`
- DV-V15 amp volume control `open`

Status: 10 wired, 5 open. research/PMICAUDIO-DRIVER-7HOP.md


## DV-Switchdev (extension)

- DV-W01 mv88e6xxx bind  (mv88e6xxx) `wired`
- DV-W02 ksz bind  (ksz_common) `wired`
- DV-W03 rtl8366 bind  (rtl8366rb) `wired`
- DV-W04 mt7530 bind  (mt7530) `wired`
- DV-W05 qca8k bind  (qca8k) `wired`
- DV-W06 mlxsw bind  (mlxsw_spectrum) `wired`
- DV-W07 b53 bind  (b53) `wired`
- DV-W08 ocelot bind  (ocelot_switch) `wired`
- DV-W09 switch VLAN bind `open`
- DV-W10 switch port mirror `open`
- DV-W11 switch ACL bind `open`
- DV-W12 switch STP bind `open`
- DV-W13 switch port stats `open`
- DV-W14 switch LAG/bonding `open`
- DV-W15 switch QoS `open`

Status: 8 wired, 7 open. research/SWITCHDEV-DRIVER-7HOP.md

## DV-SecureKey (extension)

- DV-X01 FIDO2/U2F bind  (hid-fido2) `wired`
- DV-X02 CCID smart card bind  (ccid) `wired`
- DV-X03 tpm_tis bind  (tpm_tis) `wired`
- DV-X04 tpm_crb bind  (tpm_crb) `wired`
- DV-X05 TPM 2.0 bind `open`
- DV-X06 TOTP token bind `open`
- DV-X07 PKCS11 bind `open`
- DV-X08 secure boot bind `open`
- DV-X09 measured boot bind `open`
- DV-X10 attestation bind `open`

Status: 4 wired, 6 open. research/SECUREKEY-DRIVER-7HOP.md

## DV-Panel (extension)

- DV-Y01 ili9341 bind  (ili9341) `wired`
- DV-Y02 st7789 bind  (st7789) `wired`
- DV-Y03 panel-simple bind  (panel-simple) `wired`
- DV-Y04 i6300esb watchdog bind  (i6300esb) `wired`
- DV-Y05 sp805 watchdog bind  (sp805_wdt) `wired`
- DV-Y06 it87 watchdog bind  (it87_wdt) `wired`
- DV-Y07 iTCO watchdog bind  (iTCO_wdt) `wired`
- DV-Y08 bq27xxx fuel gauge bind  (bq27xxx) `wired`
- DV-Y09 max17042 gauge bind  (max17042) `wired`
- DV-Y10 sbs-battery bind  (sbs-battery) `wired`
- DV-Y11 panel backlight `open`
- DV-Y12 panel touch `open`
- DV-Y13 panel gamma `open`
- DV-Y14 watchdog reboot `open`
- DV-Y15 gauge calibration `open`

Status: 10 wired, 5 open. research/PANEL-DRIVER-7HOP.md


## DV-Phy (extension)

- DV-Z01 marvell-phy bind  (marvell-phy) `wired`
- DV-Z02 broadcom-phy bind  (broadcom-phy) `wired`
- DV-Z03 micrel-phy bind  (micrel-phy) `wired`
- DV-Z04 realtek-phy bind  (realtek-phy) `wired`
- DV-Z05 ti-phy bind  (ti-phy) `wired`
- DV-Z06 at803x bind  (at803x) `wired`
- DV-Z07 genphy bind  (genphy) `wired`
- DV-Z08 MDIO bus bind  (mdio-bitbang) `wired`
- DV-Z09 PHY link negotiation `open`
- DV-Z10 PHY power saving `open`
- DV-Z11 PHY temperature monitor `open`
- DV-Z12 SFP fiber PHY `open`
- DV-Z13 PHY LED control `open`
- DV-Z14 PHY WoL bind `open`
- DV-Z15 PHY loopback test `open`

Status: 8 wired, 7 open. research/PHY-DRIVER-7HOP.md

## DV-Bus (extension)

- DV-AA01 i2c-piix4 bind  (i2c-piix4) `wired`
- DV-AA02 i2c-designware bind  (i2c-designware) `wired`
- DV-AA03 i2c-imx bind  (i2c-imx) `wired`
- DV-AA04 i2c-bcm2835 bind  (i2c-bcm2835) `wired`
- DV-AA05 i2c-qcom-geni bind  (i2c-qcom-geni) `wired`
- DV-AA06 i2c-tegra bind  (i2c-tegra) `wired`
- DV-AA07 spi-orion bind  (spi-orion) `wired`
- DV-AA08 spi-imx bind  (spi-imx) `wired`
- DV-AA09 spi-bcm2835 bind  (spi-bcm2835) `wired`
- DV-AA10 spi-tegra bind  (spi-tegra) `wired`
- DV-AA11 spi-pxa2xx bind  (spi-pxa2xx) `wired`
- DV-AA12 I2C mux bind `open`
- DV-AA13 I2C clock stretching `open`
- DV-AA14 SPI mode config `open`
- DV-AA15 SPI DMA transfer `open`

Status: 11 wired, 4 open. research/BUS-DRIVER-7HOP.md

## DV-Clock (extension)

- DV-AB01 ds1307 bind  (ds1307) `wired`
- DV-AB02 ds3231 bind  (ds3231) `wired`
- DV-AB03 pcf8523 bind  (pcf8523) `wired`
- DV-AB04 m41t80 bind  (m41t80) `wired`
- DV-AB05 rtc-cmos bind  (rtc-cmos) `wired`
- DV-AB06 rtc-efi bind  (rtc-efi) `wired`
- DV-AB07 int340x thermal bind  (int340x) `wired`
- DV-AB08 coretemp thermal bind  (coretemp) `wired`
- DV-AB09 rockchip thermal bind  (rockchip_thermal) `wired`
- DV-AB10 thermal cooling bind  (thermal) `wired`
- DV-AB11 RTC alarm `open`
- DV-AB12 RTC NVRAM `open`
- DV-AB13 RTC timezone `open`
- DV-AB14 thermal trip points `open`
- DV-AB15 thermal governor `open`

Status: 10 wired, 5 open. research/CLOCK-DRIVER-7HOP.md


## DV-Video (extension)

- DV-AC01 Intel iHD bind  (iHD) `wired`
- DV-AC02 radeonsi bind  (radeonsi) `wired`
- DV-AC03 NVIDIA vdpau bind  (vdpau) `wired`
- DV-AC04 venus bind  (venus) `wired`
- DV-AC05 rkvdec bind  (rkvdec) `wired`
- DV-AC06 v4l2-m2m bind  (v4l2-m2m) `wired`
- DV-AC07 AV1 decode `open`
- DV-AC08 HEVC decode `open`
- DV-AC09 VP9 decode `open`
- DV-AC10 H.264 encode `open`
- DV-AC11 video scaling `open`
- DV-AC12 DRM-overlay video `open`
- DV-AC13 4K/8K decode `open`
- DV-AC14 VA-API buffer pool `open`
- DV-AC15 video post-processing `open`

Status: 6 wired, 9 open. research/VIDEO-DRIVER-7HOP.md

## DV-NICOffload (extension)

- DV-AD01 ixgbe offload bind  (ixgbe) `wired`
- DV-AD02 i40e offload bind  (i40e) `wired`
- DV-AD03 igc offload bind  (igc) `wired`
- DV-AD04 ice offload bind  (ice) `wired`
- DV-AD05 mlx5 offload bind  (mlx5) `wired`
- DV-AD06 bnxt offload bind  (bnxt) `wired`
- DV-AD07 e1000e offload bind  (e1000e) `wired`
- DV-AD08 TSO/GSO bind `open`
- DV-AD09 GRO/LRO bind `open`
- DV-AD10 RSS bind `open`
- DV-AD11 RPS/XPS bind `open`
- DV-AD12 RFS bind `open`
- DV-AD13 multi-queue bind `open`
- DV-AD14 NIC stats bind `open`
- DV-AD15 NIC ring buffer tuning `open`

Status: 7 wired, 8 open. research/NICOFFLOAD-DRIVER-7HOP.md

## DV-PM (extension)

- DV-AE01 intel_idle bind  (intel_idle) `wired`
- DV-AE02 acpi_idle bind  (acpi_idle) `wired`
- DV-AE03 cpuidle-arm bind  (cpuidle-arm) `wired`
- DV-AE04 runtime PM bind  (runtime-pm) `wired`
- DV-AE05 S0ix/s2idle bind `open`
- DV-AE06 S3 suspend bind `open`
- DV-AE07 S4 hibernate bind `open`
- DV-AE08 autosuspend bind `open`
- DV-AE09 device dpm bind `open`
- DV-AE10 wakeup sources `open`
- DV-AE11 power governor bind `open`
- DV-AE12 suspend resume hooks `open`
- DV-AE13 battery drain tuning `open`
- DV-AE14 thermal throttling `open`
- DV-AE15 deep-sleep residency `open`

Status: 4 wired, 11 open. research/PM-DRIVER-7HOP.md


## DV-USB4 (extension)

- DV-AF01 thunderbolt bind  (thunderbolt) `wired`
- DV-AF02 usb4 bind  (usb4) `wired`
- DV-AF03 bolt manager bind  (bolt) `wired`
- DV-AF04 Titan Ridge bind  (thunderbolt) `wired`
- DV-AF05 Maple Ridge bind  (thunderbolt) `wired`
- DV-AF06 TB security mode `open`
- DV-AF07 TB NVM firmware `open`
- DV-AF08 TB domain manager `open`
- DV-AF09 eGPU routing `open`
- DV-AF10 TB dock routing `open`

Status: 5 wired, 5 open. research/USB4-DRIVER-7HOP.md

## DV-Compute (extension)

- DV-AG01 rusticl bind  (rusticl) `wired`
- DV-AG02 radeonsi compute bind  (radeonsi) `wired`
- DV-AG03 CUDA bind  (cuda) `wired`
- DV-AG04 ZLUDA bind  (zlu) `wired`
- DV-AG05 pocl bind  (pocl) `wired`
- DV-AG06 Vulkan compute bind  (radv) `wired`
- DV-AG07 OpenCL kernel bind `open`
- DV-AG08 compute shader bind `open`
- DV-AG09 GPU tensor ops `open`
- DV-AG10 GPU memory pool `open`
- DV-AG11 compute scheduling `open`
- DV-AG12 ROCm bind `open`
- DV-AG13 level-zero bind `open`
- DV-AG14 AGI inference on GPU `open`
- DV-AG15 compute telemetry `open`

Status: 6 wired, 9 open. research/COMPUTE-DRIVER-7HOP.md

## DV-VlanAudio (extension)

- DV-AH01 8021q bind  (8021q) `wired`
- DV-AH02 VLAN offload bind  (vlan-offload) `wired`
- DV-AH03 pipewire bind  (pipewire) `wired`
- DV-AH04 SOF DSP bind  (snd_sof) `wired`
- DV-AH05 ALSA dmix bind  (alsa-dmix) `wired`
- DV-AH06 pulseaudio bind  (pulseaudio) `wired`
- DV-AH07 bridge VLAN filter `open`
- DV-AH08 VLAN trunk `open`
- DV-AH09 audio equalizer `open`
- DV-AH10 audio mixing graph `open`
- DV-AH11 DSP beamforming `open`
- DV-AH12 audio latency tuning `open`
- DV-AH13 ASRC bind `open`
- DV-AH14 multichannel audio `open`
- DV-AH15 audio routing matrix `open`

Status: 6 wired, 9 open. research/VLANAUDIO-DRIVER-7HOP.md


## DV-Sata (extension)

- DV-AI01 ahci bind  (ahci) `wired`
- DV-AI02 sata_pmp bind  (sata_pmp) `wired`
- DV-AI03 ata_piix bind  (ata_piix) `wired`
- DV-AI04 NCQ bind `open`
- DV-AI05 hotplug bind `open`
- DV-AI06 link power mgmt `open`
- DV-AI07 SMART bind `open`
- DV-AI08 port multiplier `open`
- DV-AI09 SSD TRIM `open`
- DV-AI10 async scan `open`

Status: 3 wired, 7 open. research/SATA-DRIVER-7HOP.md

## DV-Drmx (extension)

- DV-AJ01 vkms bind  (vkms) `wired`
- DV-AJ02 amdgpu writeback bind  (amdgpu-writeback) `wired`
- DV-AJ03 i915 writeback bind  (i915-writeback) `wired`
- DV-AJ04 drm-writeback bind  (drm-writeback) `wired`
- DV-AJ05 HDR10 bind `open`
- DV-AJ06 HLG bind `open`
- DV-AJ07 color LUT bind `open`
- DV-AJ08 CTM bind `open`
- DV-AJ09 overlay plane bind `open`
- DV-AJ10 cursor plane bind `open`
- DV-AJ11 zpos scaling `open`
- DV-AJ12 screen capture `open`
- DV-AJ13 virtual display `open`
- DV-AJ14 HDR metadata `open`
- DV-AJ15 color pipeline `open`

Status: 4 wired, 11 open. research/DRMX-DRIVER-7HOP.md

## DV-Ptp (extension)

- DV-AK01 igb-ptp bind  (igb-ptp) `wired`
- DV-AK02 ixgbe-ptp bind  (ixgbe-ptp) `wired`
- DV-AK03 ice-ptp bind  (ice-ptp) `wired`
- DV-AK04 mlx5-ptp bind  (mlx5-ptp) `wired`
- DV-AK05 xpad haptic bind  (xpad) `wired`
- DV-AK06 sony haptic bind  (sony) `wired`
- DV-AK07 hid-ff bind  (hid-ff) `wired`
- DV-AK08 phc clock bind `open`
- DV-AK09 gPTP 802.1AS `open`
- DV-AK10 taprio TSN `open`
- DV-AK11 etf scheduling `open`
- DV-AK12 mqprio `open`
- DV-AK13 phc2sys sync `open`
- DV-AK14 adaptive triggers `open`
- DV-AK15 force feedback `open`

Status: 7 wired, 8 open. research/PTP-DRIVER-7HOP.md


## DV-Tpm (extension)

- DV-AL01 tpm_tis bind  (tpm_tis) `wired`
- DV-AL02 tpm_crb bind  (tpm_crb) `wired`
- DV-AL03 tpm_tis_spi bind  (tpm_tis_spi) `wired`
- DV-AL04 tpm_i2c bind  (tpm_i2c_atmel) `wired`
- DV-AL05 TPM 2.0 bind `open`
- DV-AL06 attestation bind `open`
- DV-AL07 PCR quote `open`
- DV-AL08 key sealing `open`
- DV-AL09 measured boot `open`
- DV-AL10 tpm2-tss bind `open`
- DV-AL11 secure boot bind `open`
- DV-AL12 AIK signing `open`
- DV-AL13 PCR policy `open`
- DV-AL14 TPM NV storage `open`
- DV-AL15 TPM telemetry `open`

Status: 4 wired, 11 open. research/TPM-DRIVER-7HOP.md

## DV-Touch (extension)

- DV-AM01 elan_i2c bind  (elan_i2c) `wired`
- DV-AM02 rmi4 bind  (rmi4) `wired`
- DV-AM03 alps bind  (alps) `wired`
- DV-AM04 wacom bind  (wacom) `wired`
- DV-AM05 goodix bind  (goodix_ts) `wired`
- DV-AM06 hid-multitouch bind  (hid-multitouch) `wired`
- DV-AM07 cypress bind  (cypress-sf) `wired`
- DV-AM08 multitouch gestures `open`
- DV-AM09 palm rejection `open`
- DV-AM10 pressure sensitivity `open`
- DV-AM11 hover detect `open`
- DV-AM12 touch calibration `open`
- DV-AM13 palm/touch hybrid `open`
- DV-AM14 trackpad click zones `open`
- DV-AM15 touch power mgmt `open`

Status: 7 wired, 8 open. research/TOUCH-DRIVER-7HOP.md

## DV-Psr (extension)

- DV-AN01 i915-psr bind  (i915-psr) `wired`
- DV-AN02 amdgpu-psr bind  (amdgpu-psr) `wired`
- DV-AN03 xe-psr bind  (xe-psr) `wired`
- DV-AN04 ixgbe sriov bind  (ixgbe) `wired`
- DV-AN05 i40e sriov bind  (i40e) `wired`
- DV-AN06 ice sriov bind  (ice) `wired`
- DV-AN07 mlx5 sriov bind  (mlx5) `wired`
- DV-AN08 PSR2 bind `open`
- DV-AN09 VF migration `open`
- DV-AN10 VF QoS `open`
- DV-AN11 eDP PSR idle `open`
- DV-AN12 panel deep sleep `open`
- DV-AN13 VF count config `open`
- DV-AN14 SR-IOV security `open`
- DV-AN15 VF driver autoload `open`

Status: 7 wired, 8 open. research/PSR-DRIVER-7HOP.md


## DV-DspMode (extension)

- DV-AO01 snd_sof bind  (snd_sof) `wired`
- DV-AO02 snd_hda_intel bind  (snd_hda_intel) `wired`
- DV-AO03 voice-trigger mode `open`
- DV-AO04 low-power mode `open`
- DV-AO05 suspend/D3 hook `open`
- DV-AO06 DSP runtime PM `open`
- DV-AO07 hotword wake `open`
- DV-AO08 DSP firmware `open`
- DV-AO09 DSP topology `open`
- DV-AO10 DSP trace/debug `wired`  (wubu_dsptrace)

Status: 2 wired, 8 open. research/DSPMODE-DRIVER-7HOP.md

## DV-MultiGig (extension)

- DV-AP01 r8125 bind  (r8125) `wired`
- DV-AP02 aquantia bind  (aquantia) `wired`
- DV-AP03 atlantic bind  (atlantic) `wired`
- DV-AP04 m88x3310 bind  (m88x3310) `wired`
- DV-AP05 bcm84881 bind  (bcm84881) `wired`
- DV-AP06 ixgbe multigig bind  (ixgbe) `wired`
- DV-AP07 2.5GBase-T bind `open`
- DV-AP08 5GBase-T bind `open`
- DV-AP09 10GBase-T bind `open`
- DV-AP10 NBASE-T rate adapt `open`

Status: 6 wired, 4 open. research/MULTIGIG-DRIVER-7HOP.md

## DV-Gamepad (extension)

- DV-AQ01 xpad bind  (xpad) `wired`
- DV-AQ02 hid-playstation bind  (hid-playstation) `wired`
- DV-AQ03 hid-nintendo bind  (hid-nintendo) `wired`
- DV-AQ04 hid-steam bind  (hid-steam) `wired`
- DV-AQ05 g29_ff bind  (g29_ff) `wired`
- DV-AQ06 hid-tmff bind  (hid-tmff) `wired`
- DV-AQ07 uinput bind  (uinput) `wired`
- DV-AQ08 i915-dsc bind  (i915-dsc) `wired`
- DV-AQ09 amdgpu-dsc bind  (amdgpu-dsc) `wired`
- DV-AQ10 nouveau-dsc bind  (nouveau-dsc) `wired`
- DV-AQ11 wheel pedals `open`
- DV-AQ12 arcade stick `open`
- DV-AQ13 fight pad `open`
- DV-AQ14 DSC 4K120 `open`
- DV-AQ15 DSC 8K `open`

Status: 10 wired, 5 open. research/GAMEPAD-DRIVER-7HOP.md


## DV-Rdma (extension)

- DV-AR01 mlx5_ib bind  (mlx5_ib) `wired`
- DV-AR02 irdma bind  (irdma) `wired`
- DV-AR03 bnxt_re bind  (bnxt_re) `wired`
- DV-AR04 qedr bind  (qedr) `wired`
- DV-AR05 rdma_rxe bind  (rdma_rxe) `wired`
- DV-AR06 siw bind  (siw) `wired`
- DV-AR07 InfiniBand core `open`
- DV-AR08 RoCE bind `open`
- DV-AR09 iWARP bind `open`
- DV-AR10 verbs API `open`
- DV-AR11 RDMA-CM bind `open`
- DV-AR12 HPC fabric bind `open`
- DV-AR13 GPUDirect bind `open`
- DV-AR14 RDMA telemetry `open`
- DV-AR15 active_speed view `open`

Status: 6 wired, 9 open. research/RDMA-DRIVER-7HOP.md

## DV-Zoned (extension)

- DV-AS01 nvme-zns bind  (nvme-zns) `wired`
- DV-AS02 zbc bind  (zbc) `wired`
- DV-AS03 zonefs bind  (zonefs) `wired`
- DV-AS04 blk-zoned bind  (blk-zoned) `wired`
- DV-AS05 SMR HDD bind `open`
- DV-AS06 ZNS flash bind `open`
- DV-AS07 zone capacity `wired`  (wubu_zonecap)
- DV-AS08 zone append `wired`  (wubu_zoneappend)
- DV-AS09 sequential write `wired`  (wubu_zonseqwrite)
- DV-AS10 zone reset `wired`  (wubu_zonefmt)

Status: 4 wired, 6 open. research/ZONED-DRIVER-7HOP.md

## DV-Vrr (extension)

- DV-AT01 amdgpu-vrr bind  (amdgpu-vrr) `wired`
- DV-AT02 i915-vrr bind  (i915-vrr) `wired`
- DV-AT03 nouveau-vrr bind  (nouveau-vrr) `wired`
- DV-AT04 pipewire-spatial bind  (pipewire-spatial) `wired`
- DV-AT05 easyeffects bind  (easyeffects) `wired`
- DV-AT06 dolby-atmos bind  (dolby-atmos) `wired`
- DV-AT07 hrtf bind  (hrtf) `wired`
- DV-AT08 FreeSync bind `open`
- DV-AT09 Adaptive-Sync bind `open`
- DV-AT10 G-Sync bind `open`
- DV-AT11 VRR low-latency `open`
- DV-AT12 object audio `open`
- DV-AT13 binaural `open`
- DV-AT14 head-tracked `open`
- DV-AT15 VRR range detect `open`

Status: 7 wired, 8 open. research/VRR-DRIVER-7HOP.md


## DV-QoS (extension)

- DV-AU01 mlxsw qos bind  (mlxsw) `wired`
- DV-AU02 felix qos bind  (felix) `wired`
- DV-AU03 mv88e qos bind  (mv88e6xxx) `wired`
- DV-AU04 ksz qos bind  (ksz) `wired`
- DV-AU05 tc flower offload `open`
- DV-AU06 rate shaping `open`
- DV-AU07 policing `open`
- DV-AU08 DSCP marking `open`
- DV-AU09 RED/ECN `open`
- DV-AU10 QoS scheduler `open`

Status: 4 wired, 6 open. research/QOS-DRIVER-7HOP.md

## DV-HIDAdv (extension)

- DV-AV01 hid-generic bind  (hid-generic) `wired`
- DV-AV02 hid-multitouch bind  (hid-multitouch) `wired`
- DV-AV03 hid-logitech bind  (hid-logitech-dj) `wired`
- DV-AV04 hid-apple bind  (hid-apple) `wired`
- DV-AV05 hid-sony bind  (hid-sony) `wired`
- DV-AV06 hid-ff bind  (hid-ff) `wired`
- DV-AV07 report descriptor `open`
- DV-AV08 HIDIOC ioctl `open`
- DV-AV09 usage page routing `open`
- DV-AV10 custom HID driver `open`
- DV-AV11 HID report queue `open`
- DV-AV12 HID power mgmt `open`
- DV-AV13 HID raw output `open`
- DV-AV14 HID debug `open`
- DV-AV15 HID telemetry `open`

Status: 6 wired, 9 open. research/HIDADV-DRIVER-7HOP.md

## DV-Backlight (extension)

- DV-AW01 acpi-video bind  (acpi-video) `wired`
- DV-AW02 intel-backlight bind  (intel-backlight) `wired`
- DV-AW03 amdgpu-bl bind  (amdgpu-bl) `wired`
- DV-AW04 pwm-backlight bind  (pwm-backlight) `wired`
- DV-AW05 nouveau-backlight bind  (nouveau-backlight) `wired`
- DV-AW06 magic-packet WoL `open`
- DV-AW07 unicast WoL `open`
- DV-AW08 broadcast WoL `open`
- DV-AW09 ARP WoL `open`
- DV-AW10 brightness curve `open`
- DV-AW11 auto-brightness `open`
- DV-AW12 WoL power budget `open`
- DV-AW13 backlight ACPI events `open`
- DV-AW14 WoL NIC sleep `open`
- DV-AW15 backlight telemetry `open`

Status: 5 wired, 10 open. research/BACKLIGHT-DRIVER-7HOP.md


## DV-MixGraph (extension)

- DV-AX01 pipewire bind  (pipewire) `wired`
- DV-AX02 wireplumber bind  (wireplumber) `wired`
- DV-AX03 pulseaudio bind  (pulseaudio) `wired`
- DV-AX04 jack bind  (jack) `wired`
- DV-AX05 alsa-dmix bind  (alsa) `wired`
- DV-AX06 session manager routing `open`
- DV-AX07 graph link topology `open`
- DV-AX08 node/port routing `open`
- DV-AX09 realtime scheduling `open`
- DV-AX10 graph telemetry `open`

Status: 5 wired, 5 open. research/MIXGRAPH-DRIVER-7HOP.md

## DV-RaidCache (extension)

- DV-AY01 dm-cache bind  (dm-cache) `wired`
- DV-AY02 bcache bind  (bcache) `wired`
- DV-AY03 zram bind  (zram) `wired`
- DV-AY04 raid5-cache bind  (raid5-cache) `wired`
- DV-AY05 lvm-cache bind  (lvm-cache) `wired`
- DV-AY06 cache tiering policy `open`
- DV-AY07 write policy (wb/wt) `open`
- DV-AY08 cache migration `open`
- DV-AY09 cache stats `open`
- DV-AY10 cache telemetry `open`

Status: 5 wired, 5 open. research/RAIDCACHE-DRIVER-7HOP.md

## DV-PD (extension)

- DV-AZ01 typec bind  (typec) `wired`
- DV-AZ02 tcpm bind  (tcpm) `wired`
- DV-AZ03 tcpm-psy bind  (tcpm-psy) `wired`
- DV-AZ04 ixgbe-arfs bind  (ixgbe) `wired`
- DV-AZ05 i40e-arfs bind  (i40e-arfs) `wired`
- DV-AZ06 mlx5-arfs bind  (mlx5-arfs) `wired`
- DV-AZ07 PDO/RDO contract `open`
- DV-AZ08 power role swap `open`
- DV-AZ09 flow hash `open`
- DV-AZ10 PD telemetry `open`
- DV-AZ11 WoL/PD coexistence `open`
- DV-AZ12 Type-C cable detection `open`
- DV-AZ13 sink/source transition `open`
- DV-AZ14 arfs CPU affinity `open`
- DV-AZ15 flow steering telemetry `open`

Status: 6 wired, 9 open. research/PD-DRIVER-7HOP.md


## DV-Calib (extension)

- DV-BA01 drm-color bind  (drm_color_mgmt) `wired`
- DV-BA02 ddc-ci bind  (ddcutil) `wired`
- DV-BA03 gamma-lut bind  (xgamma) `wired`
- DV-BA04 icc-profile bind  (colord) `wired`
- DV-BA05 colord daemon `wired`
- DV-BA06 CTM matrix `open`
- DV-BA07 gamma LUT ramp `open`
- DV-BA08 degamma `open`
- DV-BA09 brightness curve `open`
- DV-BA10 calib telemetry `open`

Status: 5 wired, 5 open. research/CALIB-DRIVER-7HOP.md

## DV-EQ (extension)

- DV-BB01 alsa-eq bind  (codec hw EQ) `wired`
- DV-BB02 pw-eq bind  (EasyEffects) `wired`
- DV-BB03 pulse-eq bind  (pulseaudio) `wired`
- DV-BB04 sof-dsp bind  (SOF DSP) `wired`
- DV-BB05 loudness-drc bind  (loudness) `wired`
- DV-BB06 biquad coefficients `open`
- DV-BB07 filter bank `open`
- DV-BB08 crossfeed `open`
- DV-BB09 compressor `open`
- DV-BB10 EQ telemetry `open`

Status: 5 wired, 5 open. research/EQ-DRIVER-7HOP.md

## DV-Gadget (extension)

- DV-BC01 dwc3 bind  (dwc3 UDC) `wired`
- DV-BC02 cdns3 bind  (cdns3 UDC) `wired`
- DV-BC03 configfs bind  (usb_gadget) `wired`
- DV-BC04 mass_storage bind  (g_mass_storage) `wired`
- DV-BC05 rndis bind  (g_rndis) `wired`
- DV-BC06 acm bind  (g_acm) `wired`
- DV-BC07 hid bind  (g_hid) `wired`
- DV-BC08 uvc bind  (g_uvc) `wired`
- DV-BC09 nvme smart-log bind  (nvme-cli) `wired`
- DV-BC10 media wear `open`
- DV-BC11 TBW `open`
- DV-BC12 pct-used `open`
- DV-BC13 gadget PD negotiation `open`
- DV-BC14 endpoint config `open`
- DV-BC15 gadget telemetry `open`

Status: 8 wired, 7 open. research/GADGET-DRIVER-7HOP.md


## DV-UCode (extension)

- DV-BD01 intel-ucode bind  (intel-ucode) `wired`
- DV-BD02 amd-ucode bind  (amd-ucode) `wired`
- DV-BD03 initrd-early bind  (early load) `wired`
- DV-BD04 dev-cpu-microcode bind  (late load) `wired`
- DV-BD05 ucode revision read `wired`
- DV-BD06 microcode update `open`
- DV-BD07 ucode telemetry `open`
- DV-BD08 reboot persistence `open`
- DV-BD09 sig verification `open`
- DV-BD10 multi-vendor `open`

Status: 5 wired, 5 open. research/UCODE-DRIVER-7HOP.md

## DV-PtpSync (extension)

- DV-BE01 ptp4l bind  (ptp4l) `wired`
- DV-BE02 phc2sys bind  (phc2sys) `wired`
- DV-BE03 igc-phc bind  (igc) `wired`
- DV-BE04 ixgbe-phc bind  (ixgbe) `wired`
- DV-BE05 mlx5-phc bind  (mlx5) `wired`
- DV-BE06 phc read `open`
- DV-BE07 timestamping caps `open`
- DV-BE08 PTP domain `open`
- DV-BE09 PTP telemetry `open`
- DV-BE10 multi-hop PTP `open`
- DV-BE11 grandmaster selection `open`
- DV-BE12 PTP VLAN `open`
- DV-BE13 PTP sync state `open`
- DV-BE14 PHC adjustment `open`
- DV-BE15 PTP security `open`

Status: 5 wired, 10 open. research/PTPSYNC-DRIVER-7HOP.md

## DV-Hdr (extension)

- DV-BF01 hdr10 bind  (ST 2086) `wired`
- DV-BF02 hdr10plus bind  (SMPTE 2094-40) `wired`
- DV-BF03 dolby-vision bind  (DV) `wired`
- DV-BF04 hlg bind  (HLG) `wired`
- DV-BF05 hda-headphone bind  (HDA jack) `wired`
- DV-BF06 hda-mic bind  (HDA mic) `wired`
- DV-BF07 asoc-jack bind  (snd_soc_jack) `wired`
- DV-BF08 hdr metadata blob `open`
- DV-BF09 SDR/HDR switch `open`
- DV-BF10 jack impedance `open`
- DV-BF11 jack polarity `open`
- DV-BF12 HDR tone mapping `open`
- DV-BF13 HDR sink detect `open`
- DV-BF14 HDR telemetry `open`
- DV-BF15 jack auto-routing `open`

Status: 7 wired, 8 open. research/HDR-DRIVER-7HOP.md


## DV-WifiReg (extension)

- DV-BG01 regulatory.db bind  (wireless-regdb) `wired`
- DV-BG02 crda bind  (crda) `wired`
- DV-BG03 cfg80211 bind  (cfg80211) `wired`
- DV-BG04 dfs-5ghz bind  (DFS) `wired`
- DV-BG05 6ghz bind  (6GHz) `wired`
- DV-BG06 country set `open`
- DV-BG07 CAC check `open`
- DV-BG08 radar detect `open`
- DV-BG09 passive channel `open`
- DV-BG10 TX power reg `open`
- DV-BG11 regulatory telemetry `open`
- DV-BG12 self-managed `open`
- DV-BG13 world domain `open`
- DV-BG14 regdb validation `open`
- DV-BG15 DFS zero-wait `open`

Status: 5 wired, 10 open. research/WIFIREG-DRIVER-7HOP.md

## DV-Trim (extension)

- DV-BH01 ext4-discard bind  (ext4) `wired`
- DV-BH02 btrfs-discard bind  (btrfs) `wired`
- DV-BH03 xfs-discard bind  (xfs) `wired`
- DV-BH04 nvme-deallocate bind  (NVMe) `wired`
- DV-BH05 ata-trim bind  (ATA DSM) `wired`
- DV-BH06 fstrim timer `open`
- DV-BH07 discard batch `open`
- DV-BH08 TRIM telemetry `open`
- DV-BH09 alt-mode DP `open`
- DV-BH10 alt-mode TB `open`
- DV-BH11 USB4 tunnel `open`
- DV-BH12 TRIM security `open`
- DV-BH13 discard granularity `open`
- DV-BH14 alt-mode detect `open`
- DV-BH15 TRIM priority `open`

Status: 5 wired, 10 open. research/TRIM-DRIVER-7HOP.md

## DV-Mst (extension)

- DV-BI01 single-stream bind  (DP SST) `wired`
- DV-BI02 multi-stream bind  (dp_mst) `wired`
- DV-BI03 dsc-compressed bind  (DSC) `wired`
- DV-BI04 44100-src bind  (SRC) `wired`
- DV-BI05 48000-src bind  (SRC) `wired`
- DV-BI06 payload alloc `open`
- DV-BI07 MST topology mgr `open`
- DV-BI08 DSC over MST `open`
- DV-BI09 resample quality `open`
- DV-BI10 SRC latency `open`
- DV-BI11 MST branch detect `open`
- DV-BI12 MST link rate `open`
- DV-BI13 MST telemetry `open`
- DV-BI14 SRC polyphase `open`
- DV-BI15 MST VCPI `open`

Status: 5 wired, 10 open. research/MST-DRIVER-7HOP.md


## DV-GPUSensor (extension)

- DV-BJ01 amdgpu bind  (amdgpu hwmon) `wired`
- DV-BJ02 i915 temp bind  (i915 GT thermal) `wired`
- DV-BJ03 nouveau bind  (nouveau hwmon) `wired`
- DV-BJ04 nvml bind  (NVIDIA) `wired`
- DV-BJ05 pwm1 fan curve bind  (pwm1 pwm1_enable) `wired`
- DV-BJ06 temp read `open`
- DV-BJ07 fan read `open`
- DV-BJ08 power read `open`
- DV-BJ09 curve aggressive `open`
- DV-BJ10 curve quiet `open`
- DV-BJ11 curve balanced `open`
- DV-BJ12 zero-rpm `open`
- DV-BJ13 GPU telemetry `open`
- DV-BJ14 sensor sync `open`
- DV-BJ15 curve tuning `open`

Status: 5 wired, 10 open. research/GPUSENSOR-DRIVER-7HOP.md

## DV-Fw (extension)

- DV-BK01 fw-loader bind  (firmware_class) `wired`
- DV-BK02 lib-firmware bind  (/lib/firmware) `wired`
- DV-BK03 megaraid bind  (megaraid_sas) `wired`
- DV-BK04 mpt3 bind  (mpt3sas) `wired`
- DV-BK05 mpt2 bind  (mpt2sas) `wired`
- DV-BK06 hpsa bind  (hpsa) `wired`
- DV-BK07 raid flash `open`
- DV-BK08 HBA flash `open`
- DV-BK09 firmware load `open`
- DV-BK10 firmware verify `open`
- DV-BK11 firmware apply `open`
- DV-BK12 firmware commit `open`
- DV-BK13 fw telemetry `open`
- DV-BK14 fw rollback `open`
- DV-BK15 fw version `open`

Status: 7 wired, 8 open. research/FW-DRIVER-7HOP.md

## DV-Ima (extension)

- DV-BL01 ima bind  (/sys/kernel/security/ima) `wired`
- DV-BL02 evm bind  (/sys/kernel/security/evm) `wired`
- DV-BL03 ima policy bind  (/sys/kernel/security/ima/policy) `wired`
- DV-BL04 appraisal bind  (file integrity) `wired`
- DV-BL05 measured-boot bind  (TPM PCR) `wired`
- DV-BL06 ima measure `open`
- DV-BL07 ima appraise `open`
- DV-BL08 ima audit `open`
- DV-BL09 ima tcb `open`
- DV-BL10 ima ape `open`
- DV-BL11 ima ltcb `open`
- DV-BL12 evm hmac `open`
- DV-BL13 ima keys `open`
- DV-BL14 pcr extends `open`
- DV-BL15 ima telemetry `open`

Status: 5 wired, 10 open. research/IMA-DRIVER-7HOP.md


## DV-Thermal (extension)

- DV-BN01 hwmon bind  (hwmon) `wired`
- DV-BN02 fan-pwm bind  (pwm1) `wired`
- DV-BN03 thermal-zone bind  (thermal zone) `wired`
- DV-BN04 trip-point bind  (trip_point_temp) `wired`
- DV-BN05 fancontrol bind  (fancontrol) `wired`
- DV-BN06 fan read `open`
- DV-BN07 temp read `open`
- DV-BN08 trip event `open`
- DV-BN09 cooling device `open`
- DV-BN10 throttle `open`
- DV-BN11 zone mode auto `open`
- DV-BN12 zone mode manual `open`
- DV-BN13 curve tuning `open`
- DV-BN14 thermal telemetry `open`
- DV-BN15 thermal sync `open`

Status: 5 wired, 10 open. research/THERMAL-DRIVER-7HOP.md

## DV-Fw (already present, this is the theme anchor)
*See above in this ledger.*

## DV-Ns (extension)

- DV-BO01 nvme bind  (/sys/class/nvme) `wired`
- DV-BO02 namespace bind  (namespaces) `wired`
- DV-BO03 nvme-multipath bind  (dm failover) `wired`
- DV-BO04 ana bind  (asymmetric namespace access) `wired`
- DV-BO05 nvme-cli bind  (nvme list-ns) `wired`
- DV-BO06 ns format `open`
- DV-BO07 path failover `open`
- DV-BO08 ana primary `open`
- DV-BO09 ana secondary `open`
- DV-BO10 ns detach `open`
- DV-BO11 ns attach `open`
- DV-BO12 ns resize `open`
- DV-BO13 ns flush `open`
- DV-BO14 ns telemetry `open`
- DV-BO15 multipath stats `open`

Status: 5 wired, 10 open. research/NS-DRIVER-7HOP.md

## DV-Fc (extension)

- DV-BP01 pause bind  (802.3x PAUSE) `wired`
- DV-BP02 pfc bind  (802.1Qbb) `wired`
- DV-BP03 ethtool bind  (ethtool -A) `wired`
- DV-BP04 autoneg bind  (pause autoneg) `wired`
- DV-BP05 rx-pause bind  (rx) `wired`
- DV-BP06 tx-pause `open`
- DV-BP07 pfc queues `open`
- DV-BP08 pause stats `open`
- DV-BP09 link state `open`
- DV-BP10 duplex mode `open`
- DV-BP11 pause timer `open`
- DV-BP12 pfc watchdog `open`
- DV-BP13 fc telemetry `open`
- DV-BP14 rx-bc stats `open`
- DV-BP15 tx-bc stats `open`

Status: 5 wired, 10 open. research/FC-DRIVER-7HOP.md


## DV-Color (extension)

- DV-BR01 ctm bind  (DR M CTM) `wired`
- DV-BR02 gamma-lut bind  (GAMMA LUT) `wired`
- DV-BR03 degamma-lut bind  (DEGAMMA LUT) `wired`
- DV-BR04 csc bind  (colorspace) `wired`
- DV-BR05 3dlut bind  (3-D LUT) `wired`
- DV-BR06 color temp `open`
- DV-BR07 hue adjust `open`
- DV-BR08 saturation `open`
- DV-BR09 brightness `open`
- DV-BR10 contrast `open`
- DV-BR11 color telemetry `open`
- DV-BR12 LUT update `open`
- DV-BR13 CTM update `open`
- DV-BR14 CSC sync `open`
- DV-BR15 color state `open`

Status: 5 wired, 10 open. research/COLORMGMT-DRIVER-7HOP.md

## DV-Loudness (extension)

- DV-BS01 replaygain bind  (track/album gain) `wired`
- DV-BS02 r128 bind  (R128_TRACK_GAIN) `wired`
- DV-BS03 lufs bind  (ITU-R BS.1770) `wired`
- DV-BS04 pipewire-loud bind  (loudness effect) `wired`
- DV-BS05 dmix-norm bind  (ALSA volume norm) `wired`
- DV-BS06 track-gain calc `open`
- DV-BS07 album-gain calc `open`
- DV-BS08 lufs target `open`
- DV-BS09 limiter `open`
- DV-BS10 normalize `open`
- DV-BS11 loudness meter `open`
- DV-BS12 replaygain scan `open`
- DV-BS13 r128 scan `open`
- DV-BS14 loudness telemetry `open`
- DV-BS15 loudness sync `open`

Status: 5 wired, 10 open. research/LOUDNESS-DRIVER-7HOP.md

## DV-GpuSched (extension)

- DV-BT01 drm-sched bind  (job submission) `wired`
- DV-BT02 guc bind  (i915 GuC) `wired`
- DV-BT03 amdgpu-sched bind  (AMD scheduler) `wired`
- DV-BT04 nvkm-sched bind  (NVIDIA sched) `wired`
- DV-BT05 priority bind  (high/normal/low) `wired`
- DV-BT06 preempt `open`
- DV-BT07 timeout `open`
- DV-BT08 fairness `open`
- DV-BT09 priority boost `open`
- DV-BT10 context switch `open`
- DV-BT11 job stats `open`
- DV-BT12 scheduler run `open`
- DV-BT13 preempt fence `open`
- DV-BT14 gpu sched telemetry `open`
- DV-BT15 sched sync `open`

Status: 5 wired, 10 open. research/GPUSCHED-DRIVER-7HOP.md


## DV-Color (DV-BJ theme)

- DV-BR01 drm-ctm bind  (DRM CTM) `wired`
- DV-BR02 gamma-lut bind  (GAMMA LUT) `wired`
- DV-BR03 degamma-lut bind  (DEGAMMA LUT) `wired`
- DV-BR04 csc bind  (colorspace conversion) `wired`
- DV-BR05 3dlut bind  (3-D LUT) `wired`
- DV-BR06 color temp `open`
- DV-BR07 hue adjust `open`
- DV-BR08 saturation `open`
- DV-BR09 brightness `open`
- DV-BR10 contrast `open`
- DV-BR11 color telemetry `open`
- DV-BR12 LUT update `open`
- DV-BR13 CTM update `open`
- DV-BR14 CSC sync `open`
- DV-BR15 color state `open`

Status: 5 wired, 10 open. research/COLORMGMT-DRIVER-7HOP.md

## DV-Loudness (DV-BS theme)

- DV-BS01 replaygain bind  (track/album gain) `wired`
- DV-BS02 r128 bind  (R128 tracks) `wired`
- DV-BS03 lufs bind  (ITU-R BS.1770) `wired`
- DV-BS04 pipewire-loud bind  (loudness effect) `wired`
- DV-BS05 dmix-norm bind  (ALSA volume norm) `wired`
- DV-BS06 track-gain calc `open`
- DV-BS07 album-gain calc `open`
- DV-BS08 lufs target `open`
- DV-BS09 limiter `open`
- DV-BS10 normalize `open`
- DV-BS11 loudness meter `open`
- DV-BS12 replaygain scan `open`
- DV-BS13 r128 scan `open`
- DV-BS14 loudness telemetry `open`
- DV-BS15 loudness sync `open`

Status: 5 wired, 10 open. research/LOUDNESS-DRIVER-7HOP.md

## DV-GpuSched (DV-BT theme)

- DV-BT01 drm-sched bind  (job submission) `wired`
- DV-BT02 guc bind  (i915 GuC) `wired`
- DV-BT03 amdgpu-sched bind  (AMD scheduler) `wired`
- DV-BT04 nvkm-sched bind  (NVIDIA sched) `wired`
- DV-BT05 priority bind  (high/normal/low) `wired`
- DV-BT06 preempt `open`
- DV-BT07 timeout `open`
- DV-BT08 fairness `open`
- DV-BT09 priority boost `open`
- DV-BT10 context switch `open`
- DV-BT11 job stats `open`
- DV-BT12 scheduler run `open`
- DV-BT13 preempt fence `open`
- DV-BT14 gpu sched telemetry `open`
- DV-BT15 sched sync `open`

Status: 5 wired, 10 open. research/GPUSCHED-DRIVER-7HOP.md

## DV-PortTiming (DV-BU theme)

- DV-BU01 drm-mode bind  (DRM mode) `wired`
- DV-BU02 cvt bind  (CVT timing) `wired`
- DV-BU03 rb bind  (reduced blanking) `wired`
- DV-BU04 link-rate bind  (DP link) `wired`
- DV-BU05 preferred-mode bind  (preferred mode) `wired`
- DV-BU06 HTOTAL `open`
- DV-BU07 VTOTAL `open`
- DV-BU08 pixel-clock `open`
- DV-BU09 mode set `open`
- DV-BU10 mode verify `open`
- DV-BU11 blanking `open`
- DV-BU12 sync polar `open`
- DV-BU13 timing telemetry `open`
- DV-BU14 mode cache `open`
- DV-BU15 mode apply `open`

Status: 5 wired, 10 open. research/PORTTIMING-DRIVER-7HOP.md

## DV-CodecGraph (DV-BV theme)

- DV-BV01 hda bind  (snd_hda_codec) `wired`
- DV-BV02 codec-graph bind  (widget tree) `wired`
- DV-BV03 amp-gain bind  (gain staging) `wired`
- DV-BV04 widgets bind  (pin/adc/dac/mixer) `wired`
- DV-BV05 dapm bind  (dynamic power mgmt) `wired`
- DV-BV06 pin-widget `open`
- DV-BV07 adc-verb `open`
- DV-BV08 dac-verb `open`
- DV-BV09 mixer-gain `open`
- DV-BV10 amp-boost `open`
- DV-BV11 codec scan `open`
- DV-BV12 widget power `open`
- DV-BV13 codec telemetry `open`
- DV-BV14 verb sequence `open`
- DV-BV15 graph export `open`

Status: 5 wired, 10 open. research/CODECGRAPH-DRIVER-7HOP.md

## DV-Flush (DV-BW theme)

- DV-BW01 flush bind  (write barriers) `wired`
- DV-BW02 barrier bind  (FLUSH/FUA) `wired`
- DV-BW03 wbcache bind  (write_cache) `wired`
- DV-BW04 fsync bind  (fsync) `wired`
- DV-BW05 nvme-flush bind  (NVMe FLUSH opcode) `wired`
- DV-BW06 writeback mode `open`
- DV-BW07 writethrough mode `open`
- DV-BW08 fdatasync `open`
- DV-BW09 barrier stats `open`
- DV-BW10 flush ack `open`
- DV-BW11 cache flush `open`
- DV-BW12 dm-flush `open`
- DV-BW13 flush telemetry `open`
- DV-BW14 flush retry `open`
- DV-BW15 flush barrier sync `open`

Status: 5 wired, 10 open. research/FLUSH-DRIVER-7HOP.md


## DV-PortTiming (DV-BM theme)

- DV-BM01 drm-mode bind  (DRM timing) `wired`
- DV-BM02 cvt bind  (CVT reduction) `wired`
- DV-BM03 rb bind  (reduced blanking) `wired`
- DV-BM04 link bind  (DP link rate) `wired`
- DV-BM05 mode set bind  (preferred mode) `wired`
- DV-BM06 HTOTAL `open`
- DV-BM07 VTOTAL `open`
- DV-BM08 pixel-clock `open`
- DV-BM09 mode list `open`
- DV-BM10 mode delete `open`
- DV-BM11 blanking `open`
- DV-BM12 sync `open`
- DV-BM13 timing telemetry `open`
- DV-BM14 mode cache `open`
- DV-BM15 timing sync `open`

Status: 5 wired, 10 open. research/PORTTIMING-DRIVER-7HOP.md

## DV-CodecGraph (DV-BN theme)

- DV-BN01 hda bind  (snd_hda_codec) `wired`
- DV-BN02 codec-graph bind  (widget tree) `wired`
- DV-BN03 amp bind  (amp gain) `wired`
- DV-BN04 widgets bind  (pin/adc/dac/mixer) `wired`
- DV-BN05 dapm bind  (dynamic power mgmt) `wired`
- DV-BN06 pin-widget `open`
- DV-BN07 adc-verb `open`
- DV-BN08 dac-verb `open`
- DV-BN09 mixer-gain `open`
- DV-BN10 amp-boost `open`
- DV-BN11 codec scan `open`
- DV-BN12 widget power `open`
- DV-BN13 codec telemetry `open`
- DV-BN14 verb sequence `open`
- DV-BN15 graph export `open`

Status: 5 wired, 10 open. research/CODECGRAPH-DRIVER-7HOP.md

## DV-Flush (DV-BO theme)

- DV-BO01 flush bind  (write barriers) `wired`
- DV-BO02 barrier bind  (FLUSH/FUA) `wired`
- DV-BO03 wbcache bind  (write_cache) `wired`
- DV-BO04 fsync bind  (fsync) `wired`
- DV-BO05 nvme-flush bind  (NVMe FLUSH opcode) `wired`
- DV-BO06 writeback mode `open`
- DV-BO07 writethrough mode `open`
- DV-BO08 fdatasync `open`
- DV-BO09 barrier stats `open`
- DV-BO10 flush ack `open`
- DV-BO11 cache flush `open`
- DV-BO12 dm-flush `open`
- DV-BO13 flush telemetry `open`
- DV-BO14 flush retry `open`
- DV-BO15 flush sync `open`

Status: 5 wired, 10 open. research/FLUSH-DRIVER-7HOP.md

## DV-BacklightPWM (DV-BR theme)

- DV-BR01 sysfs bind  (sysfs backlight) `wired`
- DV-BR02 pwm bind  (raw PWM) `wired`
- DV-BR03 acpi bind  (acpi-video) `wired`
- DV-BR04 intel bind  (intel_backlight) `wired`
- DV-BR05 amd bind  (amdgpu_bl) `wired`
- DV-BR06 brightness set `open`
- DV-BR07 brightness get `open`
- DV-BR08 bl power `open`
- DV-BR09 max brightness `open`
- DV-BR10 mode set `open`
- DV-BR11 led trigger `open`
- DV-BR12 pwm period `open`
- DV-BR13 backlight telemetry `open`
- DV-BR14 brightness ramp `open`
- DV-BR15 backlight sync `open`

Status: 5 wired, 10 open. research/BACKLIGHTPWM-DRIVER-7HOP.md

## DV-AEC (DV-BS theme)

- DV-BS01 webrtc bind  (WebRTC APM) `wired`
- DV-BS02 pipewire bind  (PipeWire aec) `wired`
- DV-BS03 pulseaudio bind  (module-echo-cancel) `wired`
- DV-BS04 alsa bind  (dmix+dsnoop) `wired`
- DV-BS05 rnnoise bind  (RNNoise) `wired`
- DV-BS06 speex algo `open`
- DV-BS07 ooura algo `open`
- DV-BS08 gain control `open`
- DV-BS09 dc block `open`
- DV-BS10 high-pass `open`
- DV-BS11 AEC telemetry `open`
- DV-BS12 echo path `open`
- DV-BS13 noise meter `open`
- DV-BS14 AEC sync `open`
- DV-BS15 voice activity `open`

Status: 5 wired, 10 open. research/AEC-DRIVER-7HOP.md

## DV-Dedup (DV-BT theme)

- DV-BT01 dm-dedup bind  (device-mapper) `wired`
- DV-BT02 btrfs bind  (btrfs dedup) `wired`
- DV-BT03 xfs bind  (xfs reflink) `wired`
- DV-BT04 zfs bind  (ZFS dedup) `wired`
- DV-BT05 duperemove bind  (userspace dedup) `wired`
- DV-BT06 inode dedup `open`
- DV-BT07 block dedup `open`
- DV-BT08 file dedup `open`
- DV-BT09 dedup scan `open`
- DV-BT10 dedup stats `open`
- DV-BT11 reflink copy `open`
- DV-BT12 send/receive `open`
- DV-BT13 dedup telemetry `open`
- DV-BT14 dedup rate `open`
- DV-BT15 dedup sync `open`

Status: 5 wired, 10 open. research/DEDUP-DRIVER-7HOP.md


## DV-VRAM (DV-BX theme)

- DV-BX01 drm-mm bind  (drm_mm memory mgr) `wired`
- DV-BX02 ttm bind  (TTM buffer objects) `wired`
- DV-BX03 framebuffer bind  (fbmem scanout) `wired`
- DV-BX04 stolen bind  (Intel stolen memory) `wired`
- DV-BX05 dxg bind  (WSL GPU paravirt VRAM) `wired`
- DV-BX06 vram alloc `open`
- DV-BX07 vram free `open`
- DV-BX08 pool stats `open`
- DV-BX09 meminfo `open`
- DV-BX10 vram migrate `open`
- DV-BX11 staging `open`
- DV-BX12 domain `open`
- DV-BX13 vram telemetry `open`
- DV-BX14 vram sync `open`
- DV-BX15 vram reclaim `open`

Status: 5 wired, 10 open. research/VRAM-DRIVER-7HOP.md

## DV-SPDIF (DV-BY theme)

- DV-BY01 spdif bind  (snd_soc_spdif) `wired`
- DV-BY02 hdmi bind  (HDMI audio infoframe) `wired`
- DV-BY03 iec61937 bind  (IEC61937 framing) `wired`
- DV-BY04 passthru bind  (AC3/DTS passthrough) `wired`
- DV-BY05 i2s bind  (I2S codec) `wired`
- DV-BY06 format raw `open`
- DV-BY07 format burst `open`
- DV-BY08 format hbr `open`
- DV-BY09 codec ac3 `open`
- DV-BY10 codec dts `open`
- DV-BY11 spdif stats `open`
- DV-BY12 infoframe `open`
- DV-BY13 spdif telemetry `open`
- DV-BY14 passthru sync `open`
- DV-BY15 spdif mute `open`

Status: 5 wired, 10 open. research/SPDIF-DRIVER-7HOP.md

## DV-CMB (DV-BZ theme)

- DV-BZ01 cmb bind  (NVMe CMB) `wired`
- DV-BZ02 pmicm bind  (NVMe 2.0 PMICM) `wired`
- DV-BZ03 queue bind  (SQ/CQ memory) `wired`
- DV-BZ04 sqs bind  (submission queue) `wired`
- DV-BZ05 cqs bind  (completion queue) `wired`
- DV-BZ06 cap1 reg `open`
- DV-BZ07 cap2 reg `open`
- DV-BZ08 qbr `open`
- DV-BZ09 nvme log `open`
- DV-BZ10 queue alloc `open`
- DV-BZ11 cmb free `open`
- DV-BZ12 pmicm stats `open`
- DV-BZ13 cmb telemetry `open`
- DV-BZ14 sq sync `open`
- DV-BZ15 cq sync `open`

Status: 5 wired, 10 open. research/CMB-DRIVER-7HOP.md


## DV-GPUBand (DV-BM2 theme)

- DV-BM201 drm-sched bind  (DRM scheduler priority) `wired`
- DV-BM202 fair bind  (GPU fair scheduling) `wired`
- DV-BM203 prio bind  (priority levels) `wired`
- DV-BM204 entity bind  (sched entities) `wired`
- DV-BM205 stats bind  (scheduler stats) `wired`
- DV-BM206 band routing `open`
- DV-BM207 class routing `open`
- DV-BM208 band preempt `open`
- DV-BM209 band migrate `open`
- DV-BM210 band stats `open`

Status: 5 wired, 5 open. research/GPUBAND-DRIVER-7HOP.md

## DV-Compress (DV-BN2 theme)

- DV-BN201 btrfs bind  (btrfs compression) `wired`
- DV-BN202 zfs bind  (ZFS compression) `wired`
- DV-BN203 zstd algo `wired`
- DV-BN204 lz4 algo `wired`
- DV-BN205 mode routing `open`
- DV-BN206 compress stats `open`
- DV-BN207 lzo `open`
- DV-BN208 zlib `open`
- DV-BN209 gzip `open`
- DV-BN210 compress tune `open`

Status: 5 wired, 5 open. research/COMPRESS-DRIVER-7HOP.md

## DV-Filter (DV-BO2 theme)

- DV-BO201 biquad bind  (Audio EQ Cookbook) `wired`
- DV-BO202 lpf bind  (lowpass filter) `wired`
- DV-BO203 hpf bind  (highpass filter) `wired`
- DV-BO204 eq bind  (equalizer) `wired`
- DV-BO205 lsp/hsp bind  (shelving) `wired`
- DV-BO206 biquad stats `open`
- DV-BO207 filter graph `open`
- DV-BO208 filter chain `open`
- DV-BO209 filter conv `open`
- DV-BO210 filter preset `open`

Status: 5 wired, 5 open. research/FILTER-DRIVER-7HOP.md


## DV-GpuRst (DV-CF theme)

- DV-CF01 amdgpu-recover bind  (amdgpu device_gpu_recover) `wired`
- DV-CF02 i915-reset bind  (i915 reset_device) `wired`
- DV-CF03 ring-hang bind  (ring test) `wired`
- DV-CF04 heartbeat bind  (GPU heartbeat) `wired`
- DV-CF05 job-timeout bind  (scheduler job timeout) `wired`
- DV-CF06 stage routing `open`
- DV-CF07 ring routing `open`
- DV-CF08 reset stats `open`
- DV-CF09 recover chain `open`
- DV-CF10 reset trace `open`

Status: 5 wired, 5 open. research/GPURST-DRIVER-7HOP.md

## DV-IoSched (DV-CG theme)

- DV-CG01 mq-deadline bind  (mq-deadline scheduler) `wired`
- DV-CG02 kyber bind  (kyber) `wired`
- DV-CG03 bfq bind  (bfq) `wired`
- DV-CG04 none bind  (noop) `wired`
- DV-CG05 cfq bind  (cfq legacy) `wired`
- DV-CG06 algo routing `open`
- DV-CG07 mode routing `open`
- DV-CG08 qd stats `open`
- DV-CG09 wbt tune `open`
- DV-CG10 iosched swap `open`

Status: 5 wired, 5 open. research/IOSCHED-DRIVER-7HOP.md

## DV-WiFiUtil (DV-CH theme)

- DV-CH01 mac80211-util bind  (channel utilization) `wired`
- DV-CH02 iwlwifi bind  (iwlwifi util) `wired`
- DV-CH03 ath11k bind  (ath11k util) `wired`
- DV-CH04 cca bind  (CCA busy) `wired`
- DV-CH05 airtime bind  (station airtime) `wired`
- DV-CH06 band routing `open`
- DV-CH07 state routing `open`
- DV-CH08 chan stats `open`
- DV-CH09 survey `open`
- DV-CH10 chan switch `open`

Status: 5 wired, 5 open. research/WIFIUTIL-DRIVER-7HOP.md


## DV-DDCCI (DV-CI theme)

- DV-CI01 i2c-ddcci bind  (i2c DDC/CI) `wired`
- DV-CI02 cec bind  (CEC control) `wired`
- DV-CI03 edid bind  (EDID EEPROM) `wired`
- DV-CI04 backlight-ctrl bind  (backlight class) `wired`
- DV-CI05 drm-ddc bind  (drm DDC) `wired`
- DV-CI06 cmd routing `open`
- DV-CI07 bus routing `open`
- DV-CI08 osd stats `open`
- DV-CI09 power ctrl `open`
- DV-CI10 ddcci trace `open`

Status: 5 wired, 5 open. research/DDCCI-DRIVER-7HOP.md

## DV-SampleRate (DV-CJ theme)

- DV-CJ01 snd-pcm bind  (ALSA PCM) `wired`
- DV-CJ02 snd-usb bind  (USB audio hi-res) `wired`
- DV-CJ03 float fmt  (float PCM) `wired`
- DV-CJ04 24bit fmt  (S24_LE) `wired`
- DV-CJ05 hi-res rate  (192k/384k) `wired`
- DV-CJ06 fmt routing `open`
- DV-CJ07 rate routing `open`
- DV-CJ08 hw-params `open`
- DV-CJ09 convert `open`
- DV-CJ10 rate stats `open`

Status: 5 wired, 5 open. research/SAMPLERATE-DRIVER-7HOP.md

## DV-Smart (DV-CK theme)

- DV-CK01 ata-smart bind  (ATA S.M.A.R.T.) `wired`
- DV-CK02 nvme-smart bind  (NVMe smart log) `wired`
- DV-CK03 smartmontools bind  (smartctl) `wired`
- DV-CK04 health bind  (health log) `wired`
- DV-CK05 temp bind  (temp sensor) `wired`
- DV-CK06 attr routing `open`
- DV-CK07 status routing `open`
- DV-CK08 threshold `open`
- DV-CK09 selftest `open`
- DV-CK10 smart stats `open`

Status: 5 wired, 5 open. research/SMART-DRIVER-7HOP.md


## DV-Overclock (DV-CL theme)

- DV-CL01 amdgpu-od bind  (OverDrive) `wired`
- DV-CL02 i915-gt bind  (GT freq) `wired`
- DV-CL03 sysfs bind  (drm sysfs clocks) `wired`
- DV-CL04 core clk  (sclk) `wired`
- DV-CL05 mem clk  (mclk) `wired`
- DV-CL06 clk routing `open`
- DV-CL07 state routing `open`
- DV-CL08 od stats `open`
- DV-CL09 voltage ctrl `open`
- DV-CL10 oc telemetry `open`

Status: 5 wired, 5 open. research/OVERCLOCK-DRIVER-7HOP.md

## DV-DspGraph (DV-CM theme)

- DV-CM01 snd-soc-dapm bind  (ALSA dapm) `wired`
- DV-CM02 audio-graph-card bind  (DT audio graph) `wired`
- DV-CM03 widget bind  (widget routing) `wired`
- DV-CM04 path bind  (source-sink path) `wired`
- DV-CM05 route bind  (route control) `wired`
- DV-CM06 widget routing `wired`  (wubu_dapm)
- DV-CM07 path routing `wired`  (wubu_dapm)
- DV-CM08 graph stats `open`
- DV-CM09 graph dump `open`
- DV-CM10 graph trace `open`

Status: 5 wired, 5 open. research/DSPGRAPH-DRIVER-7HOP.md

## DV-Smr (DV-CN theme)

- DV-CN01 host-managed bind  (SMR host-managed) `wired`
- DV-CN02 nvme-zns bind  (NVMe ZNS) `wired`
- DV-CN03 zonefs bind  (zonefs) `wired`
- DV-CN04 zone bind  (zone mgmt) `wired`
- DV-CN05 wp bind  (write pointer) `wired`
- DV-CN06 zone routing `open`
- DV-CN07 op routing `open`
- DV-CN08 zone stats `open`
- DV-CN09 wp sync `open`
- DV-CN10 zone reset `open`

Status: 5 wired, 5 open. research/SMR-DRIVER-7HOP.md


## DV-PGPowergate (DV-CO theme)

- DV-CO01 amdgpu-pg bind  (power gating) `wired`
- DV-CO02 i915-pg bind  (i915 power well) `wired`
- DV-CO03 nvidia-pg bind  (nvidia power gating) `wired`
- DV-CO04 drm-pm bind  (runtime PM) `wired`
- DV-CO05 shader-domain bind  (shader) `wired`
- DV-CO06 domain routing `open`
- DV-CO07 state routing `open`
- DV-CO08 pg stats `open`
- DV-CO09 voltage rail `open`
- DV-CO10 pg telemetry `open`

Status: 5 wired, 5 open. research/POWERGATE-DRIVER-7HOP.md

## DV-DapmWidget (DV-CP theme)

- DV-CP01 snd-soc-dapm bind  (ALSA DAPM) `wired`
- DV-CP02 widget bind  (widget routing) `wired`
- DV-CP03 power bind  (widget power state) `wired`
- DV-CP04 path bind  (widget path) `wired`
- DV-CP05 stream bind  (stream power) `wired`
- DV-CP06 widget type routing `open`
- DV-CP07 power routing `open`
- DV-CP08 graph dump `open`
- DV-CP09 power trace `open`
- DV-CP10 widget stats `open`

Status: 5 wired, 5 open. research/DAPMWIDGET-DRIVER-7HOP.md

## DV-ZnsZone (DV-CQ theme)

- DV-CQ01 nvme-zns bind  (NVMe ZNS) `wired`
- DV-CQ02 zone bind  (zone descriptor) `wired`
- DV-CQ03 state bind  (zone state) `wired`
- DV-CQ04 action bind  (mgr action) `wired`
- DV-CQ05 report bind  (zone report) `wired`
- DV-CQ06 state routing `open`
- DV-CQ07 action routing `open`
- DV-CQ08 zone stats `open`
- DV-CQ09 wp sync `open`
- DV-CQ10 zone reset `open`

Status: 5 wired, 5 open. research/ZNSZONE-DRIVER-7HOP.md


## DV-Computectx (DV-CO2 theme)

- DV-CO201 amdgpu-kfd bind  (KFD compute queues) `wired`
- DV-CO202 opencl bind  (OpenCL context) `wired`
- DV-CO203 cuda bind  (CUDA context) `wired`
- DV-CO204 queue bind  (compute queue) `wired`
- DV-CO205 priority bind  (queue priority) `wired`
- DV-CO206 queue routing `open`
- DV-CO207 priority routing `open`
- DV-CO208 ctx stats `open`
- DV-CO209 ctx trace `open`
- DV-CO210 ctx reset `open`

Status: 5 wired, 5 open. research/COMPUTECTX-DRIVER-7HOP.md

## DV-Chanmap (DV-CO3 theme)

- DV-CO301 snd-pcm bind  (ALSA channel map) `wired`
- DV-CO302 hdmi-chmap bind  (HDMI channel map) `wired`
- DV-CO303 position bind  (channel position) `wired`
- DV-CO304 layout bind  (mono/stereo/5.1/7.1) `wired`
- DV-CO305 surround bind  (surround routing) `wired`
- DV-CO306 position routing `open`
- DV-CO307 layout routing `open`
- DV-CO308 chmap stats `open`
- DV-CO309 chmap trace `open`
- DV-CO310 chmap remap `open`

Status: 5 wired, 5 open. research/CHANMAP-DRIVER-7HOP.md

## DV-DmCrypt (DV-CO4 theme)

- DV-CO401 dm-crypt bind  (device-mapper crypt) `wired`
- DV-CO402 cryptsetup-luks bind  (LUKS format) `wired`
- DV-CO403 dm-mod bind  (device-mapper core) `wired`
- DV-CO404 aes bind  (AES cipher) `wired`
- DV-CO405 xts bind  (XTS mode) `wired`
- DV-CO406 cipher routing `open`
- DV-CO407 mode routing `open`
- DV-CO408 keysize `open`
- DV-CO409 dm-crypt stats `open`
- DV-CO410 dm-crypt trace `open`

Status: 5 wired, 5 open. research/DMCRYPT-DRIVER-7HOP.md


## DV-Voltagectl (DV-CO5 theme)

- DV-CO501 amdgpu-vid bind  (SVI2 voltage) `wired`
- DV-CO502 i915-vid bind  (i915 voltage) `wired`
- DV-CO503 nvidia-vid bind  (nvidia voltage) `wired`
- DV-CO504 undervolt bind  (voltage undervolt) `wired`
- DV-CO505 core-vdd bind  (core voltage) `wired`
- DV-CO506 domain routing `open`
- DV-CO507 mode routing `open`
- DV-CO508 volt stats `open`
- DV-CO509 volt trace `open`
- DV-CO510 volt curve `open`

Status: 5 wired, 5 open. research/VOLTAGECTL-DRIVER-7HOP.md

## DV-SpdifTx (DV-CO6 theme)

- DV-CO601 snd-spdif bind  (SPDIF TX) `wired`
- DV-CO602 hda-spdif bind  (HDA SPDIF) `wired`
- DV-CO603 encoding bind  (PCM/AC3/DTS) `wired`
- DV-CO604 iec bind  (IEC 60958) `wired`
- DV-CO605 optical bind  (optical/coax) `wired`
- DV-CO606 encoding routing `open`
- DV-CO607 media routing `open`
- DV-CO608 spdif stats `open`
- DV-CO609 spdif trace `open`
- DV-CO610 spdif passthrough `open`

Status: 5 wired, 5 open. research/SPDIFTX-DRIVER-7HOP.md

## DV-BlkQos (DV-CO7 theme)

- DV-CO701 blk-throttle bind  (cgroup io.throttle) `wired`
- DV-CO702 blk-qos bind  (block QoS) `wired`
- DV-CO703 io-max bind  (io.max throttle) `wired`
- DV-CO704 io-weight bind  (io.weight) `wired`
- DV-CO705 io-stat bind  (io.stat) `wired`
- DV-CO706 mode routing `open`
- DV-CO707 unit routing `open`
- DV-CO708 qos stats `open`
- DV-CO709 qos latency `open`
- DV-CO710 qos cost-model `open`

Status: 5 wired, 5 open. research/BLKQOS-DRIVER-7HOP.md


## DV-MemMgr (DV-CO8 theme)

- DV-CO801 amdgpu-ttm bind  (TTM VRAM allocator) `wired`
- DV-CO802 i915-gem bind  (GEM allocator) `wired`
- DV-CO803 nvidia-vram bind  (nvidia VRAM) `wired`
- DV-CO804 vram bind  (VRAM heap) `wired`
- DV-CO805 gtt bind  (GTT heap) `wired`
- DV-CO806 heap routing `open`
- DV-CO807 type routing `open`
- DV-CO808 mm stats `open`
- DV-CO809 bo migrate `open`
- DV-CO810 mm trace `open`

Status: 5 wired, 5 open. research/MEMMGR-DRIVER-7HOP.md

## DV-JackDetect (DV-CO9 theme)

- DV-CO901 snd-jack bind  (ALSA jack detect) `wired`
- DV-CO902 switch-class bind  (switch class) `wired`
- DV-CO903 headset bind  (headset detection) `wired`
- DV-CO904 mic bind  (mic presence) `wired`
- DV-CO905 ctia bind  (CTIA pinout) `wired`
- DV-CO906 pinout routing `open`
- DV-CO907 state routing `open`
- DV-CO908 jack stats `open`
- DV-CO909 jack trace `open`
- DV-CO910 auto-detect `open`

Status: 5 wired, 5 open. research/JACKDETECT-DRIVER-7HOP.md

## DV-Ioprio (DV-CA theme)

- DV-CA01 ionice bind  (I/O priority) `wired`
- DV-CA02 class bind  (RT/BE/IDLE) `wired`
- DV-CA03 scheduler bind  (noop/deadline/cfq) `wired`
- DV-CA04 weight bind  (io.weight) `wired`
- DV-CA05 io-stat bind  (io.stat) `wired`
- DV-CA06 class routing `open`
- DV-CA07 scheduler routing `open`
- DV-CA08 ioprio stats `open`
- DV-CA09 ioprio latency `open`
- DV-CA010 blkio cgroup `open`

Status: 5 wired, 5 open. research/IOPRIO-DRIVER-7HOP.md


## DV-PerfMon (DV-CB theme)

- DV-CB01 gpuprofapi bind  (AMD perf counters) `wired`
- DV-CB02 i915-perf bind  (Intel DRM perf) `wired`
- DV-CB03 nvml-perf bind  (NVML perf) `wired`
- DV-CB04 metric bind  (event counters) `wired`
- DV-CB05 cycles bind  (cycle counting) `wired`
- DV-CB06 metric routing `wired`  (wubu_perf)
- DV-CB07 API routing `open`
- DV-CB08 perf stats `open`
- DV-CB09 perf trace `open`
- DV-CB010 perf sampling `open`

Status: 5 wired, 5 open. research/PERFMON-DRIVER-7HOP.md

## DV-PcmPlugin (DV-CC theme)

- DV-CC01 snd-pcm-plugins bind  (ALSA PCM plugin) `wired`
- DV-CC02 rate bind  (rate conversion plugin) `wired`
- DV-CC03 vol bind  (volume plugin) `wired`
- DV-CC04 copy bind  (copy plugin) `wired`
- DV-CC05 dmix bind  (dmix plugin) `wired`
- DV-CC06 type routing `open`
- DV-CC07 chain routing `open`
- DV-CC08 plugin stats `wired`  (wubu_pcmring)
- DV-CC09 plugin trace `open`
- DV-CC010 plugin chain `wired`  (wubu_pcmlink)

Status: 5 wired, 5 open. research/PCMPLUGIN-DRIVER-7HOP.md

## DV-DAX (DV-CD theme)

- DV-CD01 nd-pmem bind  (NVDIMM persistent memory) `wired`
- DV-CD02 fs-dax bind  (filesystem DAX) `wired`
- DV-CD03 dev-dax bind  (device DAX) `wired`
- DV-CD04 ext4 bind  (ext4 dax) `wired`
- DV-CD05 xfs bind  (xfs dax) `wired`
- DV-CD06 type routing `wired`  (wubu_dapm)
- DV-CD07 fs routing `open`
- DV-CD08 dax stats `open`
- DV-CD09 dax trace `open`
- DV-CD010 dax pagemap `open`

Status: 5 wired, 5 open. research/DAX-DRIVER-7HOP.md


## DV-FbCon (DV-CE theme)

- DV-CE01 fbcon bind  (framebuffer console) `wired`
- DV-CE02 drm-fbcon bind  (DRM framebuffer) `wired`
- DV-CE03 rotate bind  (console rotation) `wired`
- DV-CE04 virtual bind  (virtual console) `wired`
- DV-CE05 mode bind  (video mode) `wired`
- DV-CE06 rotation routing `open`
- DV-CE07 mode routing `open`
- DV-CE08 fbcon stats `open`
- DV-CE09 fbcon trace `open`
- DV-CE010 fbcon blank `open`

Status: 5 wired, 5 open. research/FBCON-DRIVER-7HOP.md

## DV-JackState (DV-CF theme)

- DV-CF01 snd-switch bind  (ALSA switch state) `wired`
- DV-CF02 hda-jack bind  (HDA jack state) `wired`
- DV-CF03 state-machine bind  (plug/unplug states) `wired`
- DV-CF04 event bind  (plug_in/plug_out events) `wired`
- DV-CF05 debounce bind  (debounce timer) `wired`
- DV-CF06 state routing `open`
- DV-CF07 event routing `open`
- DV-CF08 jack stats `open`
- DV-CF09 jack trace `open`
- DV-CF010 jack hysteresis `open`

Status: 5 wired, 5 open. research/JACKSTATE-DRIVER-7HOP.md

## DV-StorageSched (DV-CG theme)

- DV-CG01 blk-sched bind  (block scheduler) `wired`
- DV-CG02 nvme-sched bind  (NVMe scheduler) `wired`
- DV-CG03 mq-deadline bind  (multi-queue deadline) `wired`
- DV-CG04 bfq bind  (BFQ scheduler) `wired`
- DV-CG05 cfq bind  (CFQ scheduler) `wired`
- DV-CG06 type routing `open`
- DV-CG07 mode routing `open`
- DV-CG08 sched stats `open`
- DV-CG09 latency hist `open`
- DV-CG010 io cost model `open`

Status: 5 wired, 5 open. research/STORAGESCHED-DRIVER-7HOP.md


## DV-FenceSync (DV-CH theme)

- DV-CH01 amdgpufence bind  (AMD DMA fence) `wired`
- DV-CH02 i915fence bind  (Intel fence timeline) `wired`
- DV-CHCH03 drm-sync bind  (DRM sync fd) `wired`
- DV-CH04 sdma-fence bind  (SDMA fence) `wired`
- DV-CH05 seqno bind  (seqno tracking) `wired`
- DV-CH06 type routing `open`
- DV-CH07 op routing `open`
- DV-CH08 fence stats `open`
- DV-CH09 fence timeout `open`
- DV-CH010 fence trace `open`

Status: 5 wired, 5 open. research/FENCESYNC-DRIVER-7HOP.md

## DV-JackImpedance (DV-CI theme)

- DV-CI01 snd-impedance bind  (ALSA impedance) `wired`
- DV-CI02 headphone bind  (headphone detect) `wired`
- DV-CI03 mic bind  (mic impedance) `wired`
- DV-CI04 impedance bind  (ohms measurement) `wired`
- DV-CI05 threshold bind  (threshold detect) `wired`
- DV-CI06 type routing `open`
- DV-CI07 device routing `open`
- DV-CI08 impedance stats `open`
- DV-CI09 impedance trace `open`
- DV-CI010 TRRS pinout `open`

Status: 5 wired, 5 open. research/JACKIMPEDANCE-DRIVER-7HOP.md

## DV-Wrriteback (DV-CJ theme)

- DV-CJ01 writeback bind  (kernel writeback) `wired`
- DV-CJ02 dirty bind  (dirty pages) `wired`
- DV-CJ03 sync bind  (sync writeback) `wired`
- DV-CJ04 io-13 bind  (interval) `wired`
- DV-CJ05 thread bind  (writeback thread) `wired`
- DV-CJ06 mode routing `open`
- DV-CJ07 thread routing `open`
- DV-CJ08 writeback stats `open`
- DV-CJ09 latency `open`
- DV-CJ010 congestion `open`

Status: 5 wired, 5 open. research/WRITEBACK-DRIVER-7HOP.md


## DV-ThermalThrottle (DV-DA theme)

- DV-DA01 intel-thermal bind  (thermal zone) `wired`
- DV-DA02 k10temp bind  (AMD thermal) `wired`
- DV-DA03 cooling bind  (cooling device) `wired`
- DV-DA04 trip-point bind  (critical/hot/passive) `wired`
- DV-DA05 governor bind  (step_wise/fair_share) `wired`
- DV-DA06 gov routing `open`
- DV-DA07 trip routing `open`
- DV-DA08 thermal stats `open`
- DV-DA09 thermal trace `open`
- DV-DA010 fan control `open`

Status: 5 wired, 5 open. research/THERMALTHROTTLE-DRIVER-7HOP.md

## DV-Compressor (DV-DB theme)

- DV-DB01 snd-compressor bind  (ALSA compressor) `wired`
- DV-DB02 ratio bind  (compression ratio) `wired`
- DV-DB03 threshold bind  (dB threshold) `wired`
- DV-DB04 attack bind  (attack time) `wired`
- DV-DB05 release bind  (release time) `wired`
- DV-DB06 ratio routing `open`
- DV-DB07 knee routing `open`
- DV-DB08 compressor stats `open`
- DV-DB09 compressor trace `open`
- DV-DB010 limiter mode `open`

Status: 5 wired, 5 open. research/COMPRESSOR-DRIVER-7HOP.md

## DV-RAID5 (DV-DC theme)

- DV-DC01 raid5 bind  (RAID5 layout) `wired`
- DV-DC02 mdadm bind  (metadata) `wired`
- DV-DC03 stripe bind  (stripe cache) `wired`
- DV-DC04 parity bind  (P/Q parity) `wired`
- DV-DC05 disk bind  (disk array) `wired`
- DV-DC06 layout routing `open`
- DV-DC07 parity routing `open`
- DV-DC08 raid stats `open`
- DV-DC09 raid resync `open`
- DV-DC010 raid rebuild `open`

Status: 5 wired, 5 open. research/RAID5-DRIVER-7HOP.md


## DV-SMC (DV-DD theme)

- DV-DD01 smu-fw bind  (AMD SMU firmware) `wired`
- DV-DD02 radeon-smc bind  (Radeon SMC) `wired`
- DV-DD03 i915-fw bind  (i915 firmware) `wired`
- DV-DD04 vcn bind  (VCN firmware) `wired`
- DV-DD05 uvd bind  (UVD firmware) `wired`
- DV-DD06 block routing `open`
- DV-DD07 state routing `open`
- DV-DD08 smc stats `open`
- DV-DD09 smc trace `open`
- DV-DD010 smc pstate `open`

Status: 5 wired, 5 open. research/SMC-DRIVER-7HOP.md

## DV-IecControl (DV-DE theme)

- DV-DE01 snd-iec bind  (ALSA IEC 60958) `wired`
- DV-DE02 aes bind  (AES bits) `wired`
- DV-DE03 encoding bind  (consumer/professional) `wired`
- DV-DE04 clock bind  (ext/int/master/slave) `wired`
- DV-DE05 rate bind  (sample rate) `wired`
- DV-DE06 encoding routing `open`
- DV-DE07 clock routing `open`
- DV-DE08 iec stats `open`
- DV-DE09 iec trace `open`
- DV-DE010 iec bypass `open`

Status: 5 wired, 5 open. research/IECCONTROL-DRIVER-7HOP.md

## DV-Flush2 (DV-DF theme)

- DV-DF01 flush-barrier bind  (write barrier) `wired`
- DV-DF02 fsync bind  (fsync) `wired`
- DV-DF03 cache-flush bind  (write cache flush) `wired`
- DV-DF04 write-cache bind  (write cache toggle) `wired`
- DV-DF05 flush-cmd bind  (FLUSH/SYNCHRONIZE) `wired`
- DV-DF06 type routing `open`
- DV-DF07 cmd routing `open`
- DV-DF08 flush stats `open`
- DV-DF09 flush latency `open`
- DV-DF010 barrier ordering `open`

Status: 5 wired, 5 open. research/FLUSH2-DRIVER-7HOP.md


## DV-MMU (DV-DH theme)

- DV-DH01 amdgpu-mmu bind  (AMD GPUVM page table) `wired`
- DV-DH02 i915-mmu bind  (Intel GGTT/PPGTT) `wired`
- DV-DH03 nvidia-mmu bind  (NVIDIA page table) `wired`
- DV-DH04 page-table bind  (page table) `wired`
- DV-DH05 fault bind  (page fault handler) `wired`
- DV-DH06 type routing `open`
- DV-DH07 fault routing `open`
- DV-DH08 mmu stats `open`
- DV-DH09 mmu trace `open`
- DV-DH010 vm context `open`

Status: 5 wired, 5 open. research/MMU-DRIVER-7HOP.md

## DV-DapmPath (DV-DI theme)

- DV-DI01 snd-dapm bind  (ALSA DAPM paths) `wired`
- DV-DI02 playback bind  (playback path) `wired`
- DV-DI03 capture bind  (capture path) `wired`
- DV-DI04 mux bind  (mux widget) `wired`
- DV-DI05 mix bind  (mix widget) `wired`
- DV-DI06 type routing `open`
- DV-DI07 widget routing `open`
- DV-DI08 dapm stats `open`
- DV-DI09 dapm trace `open`
- DV-DI010 dapm power `open`

Status: 5 wired, 5 open. research/DAPPATH-DRIVER-7HOP.md

## DV-Bio (DV-DJ theme)

- DV-DJ01 blk-bio bind  (block I/O bio) `wired`
- DV-DJ02 bio-vec bind  (bio_vec scatterlist) `wired`
- DV-DJ03 bdi bind  (BDI backing dev) `wired`
- DV-DJ04 read bind  (READ op) `wired`
- DV-DJ05 write bind  (WRITE op) `wired`
- DV-DJ06 op routing `open`
- DV-DJ07 layer routing `open`
- DV-DJ08 bio stats `open`
- DV-DJ09 bio trace `open`
- DV-DJ010 bio merge `open`

Status: 5 wired, 5 open. research/BIO-DRIVER-7HOP.md


## DV-Encode (DV-DK theme)

- DV-DK01 amd-vcn bind  (AMD video encode) `wired`
- DV-DK02 i915-qsv bind  (Intel Quick Sync) `wired`
- DV-DK03 nvidia-nvenc bind  (NVIDIA NVENC) `wired`
- DV-DK04 h264 bind  (H.264 codec) `wired`
- DV-DK05 h265 bind  (H.265 codec) `wired`
- DK-06 codec routing `open`
- DV-DK07 API routing `open`
- DV-DK08 encode stats `open`
- DV-DK09 encode trace `open`
- DV-DK010 av1 encode `open`

Status: 5 wired, 5 open. research/ENCODE-DRIVER-7HOP.md

## DV-SpdifStatus (DV-DL theme)

- DV-DL01 snd-spdif-status bind  (SPDIF receiver) `wired`
- DV-DL02 aes bind  (AES status bits) `wired`
- DV-DL03 rate bind  (sample rate) `wired`
- DV-DL04 lock bind  (lock detected) `wired`
- DV-DL05 valid bind  (validity bit) `wired`
- DV-DL06 rate routing `open`
- DV-DL07 lock routing `open`
- DV-DL08 spdif stats `open`
- DV-DL09 spdif trace `open`
- DV-DL010 aes decode `open`

Status: 5 wired, 5 open. research/SPDIFSTATUS-DRIVER-7HOP.md

## DV-DevMapper (DV-DM theme)

- DV-DM01 device-mapper bind  (DM core) `wired`
- DV-DM02 dm-linear bind  (linear target) `wired`
- DV-DM03 dm-stripe bind  (stripe target) `wired`
- DV-DM04 dm-mirror bind  (mirror target) `wired`
- DV-DM05 dm-snapshot bind  (snapshot target) `wired`
- DV-DM06 target routing `open`
- DV-DM07 mode routing `open`
- DV-DM08 dm stats `open`
- DV-DM09 dm table `open`
- DV-DM010 dm thin `open`

Status: 5 wired, 5 open. research/DEVMAPPER-DRIVER-7HOP.md


## DV-HWDecode (DV-DN theme)

- DV-DN01 uvd bind  (UVD video decode) `wired`
- DV-DN02 vcn-decode bind  (VCN decode) `wired`
- DV-DN03 nvdec bind  (NVIDIA NVDEC) `wired`
- DV-DN04 qsv-decode bind  (Intel QSV decode) `wired`
- DV-DN05 codec routing `open`
- DV-DN06 API routing `open`
- DV-DN07 decode stats `open`
- DV-DN08 decode trace `open`
- DV-DN09 av1 decode `open`
- DV-DN010 vp9 decode `open`

Status: 4 wired, 6 open. research/DECODE-DRIVER-7HOP.md

## DV-AudioFW (DV-DO theme)

- DV-DO01 snd-fw bind  (ALSA firmware loader) `wired`
- DV-DO02 hda-fw bind  (HDA firmware) `wired`
- DV-DO03 codec routing `open`
- DV-DO04 loader routing `open`
- DV-DO05 DSP firmware `open`
- DV-DO06 bezel firmware `open`
- DV-DO07 audiofw stats `open`
- DV-DO08 audiofw trace `open`
- DV-DO09 realtek fw `open`
- DV-DO010 codec fw `open`

Status: 2 wired, 8 open. research/AUDIOFW-DRIVER-7HOP.md

## DV-NfsMount (DV-DP theme)

- DV-DP01 nfs-mount bind  (NFS client mount) `wired`
- DV-DP02 nfs-server bind  (NFS server nfsd) `wired`
- DV-DP03 vers routing `open`
- DV-DP04 option routing `open`
- DV-DP05 nfs stats `open`
- DV-DP06 nfs trace `open`
- DV-DP07 nfs root `open`
- DV-DP08 nfs idmap `open`
- DV-DP09 nfs dns `open`
- DV-DP010 nfs security `open`

Status: 2 wired, 8 open. research/NFSMOUNT-DRIVER-7HOP.md


## DV-DRM (DV-DV theme)

- DV-DV01 amdgpu-drm bind  (AMD DRM driver) `wired`
- DV-DV02 i915-drm bind  (Intel DRM driver) `wired`
- DV-DV03 nouveau-drm bind  (Nouveau DRM) `wired`
- DV-DV04 mgag200 bind  (Matrox DRM) `wired`
- DV-DV05 ast bind  (ASPEED DRM) `wired`
- DV-DV06 KMS bind  (Kernel Mode Setting) `wired`
- DV-DV07 GEM bind  (Graphics Execution Manager) `wired`
- DV-DV08 PRIME bind  (buffer sharing) `wired`
- DV-DV09 subsys routing `open`
- DV-DV010 obj routing `open`

Status: 8 wired, 2 open. research/DRM-DRIVER-7HOP.md

## DV-MixerGraph (DV-DW theme)

- DV-DW01 snd-mixer bind  (ALSA mixer) `wired`
- DV-DW02 control routing `open`
- DV-DW03 playback path `open`
- DW-04 capture path `open`
- DV-DW05 monitor path `open`
- DV-DW06 loopback `open`
- DV-DW07 group routing `open`
- DV-DW08 mixer stats `open`
- DV-DW09 mixer trace `open`
- DV-DW010 mixer bypass `open`

Status: 1 wired, 9 open. research/MIXERGRAPH-DRIVER-7HOP.md

## DV-NfsClient (DV-DX theme)

- DV-DX01 nfs-client bind  (NFS client mount) `wired`
- DV-DX02 rpc-idmapd bind  (NFSv4 identity) `wired`
- DV-DX03 rpc-statd bind  (NFS lock) `wired`
- DV-DX04 vers routing `open`
- DV-DX05 proto routing `open`
- DV-DX06 nfs stats `open`
- DV-DX07 nfs trace `open`
- DV-DX08 nfs root `open`
- DV-DX09 nfs dns `open`
- DV-DX010 nfs sec `open`

Status: 3 wired, 7 open. research/NFSCLIENT-DRIVER-7HOP.md


## DV-VBLANK (DV-VB theme)

- DV-VB01 amdgpu-vblank bind  (AMD VBLANK interrupt) `wired`
- DV-VB02 i915-vblank bind  (Intel VBLANK interrupt) `wired`
- DV-VB03 nvidia-vblank bind  (NVIDIA VBLANK) `wired`
- DV-VB04 vc4-vblank bind  (VC4 VBLANK) `wired`
- DV-VB05 ast-vblank bind  (AST VBLANK) `wired`
- DV-VB06 meson-vblank bind  (Meson VBLANK) `wired`
- DV-VB07 sun4i-vblank bind  (Allwinner VBLANK) `wired`
- DV-VB08 exynos-vblank bind  (Exynos VBLANK) `wired`
- DV-VB09 tegra-vblank bind  (Tegra VBLANK) `wired`
- DV-VB10 omap-disp vblank bind  (OMAP VBLANK) `wired`
- DV-VB11 sti-vblank bind  (STI VBLANK) `wired`
- DV-VB12 amdgpufw-vblank bind  (AMD GPU FW VBLANK) `wired`
- DV-VB13 nouveau-vblank bind  (Nouveau VBLANK) `wired`
- DV-VB14 mgag200-vblank bind  (MGA G200 VBLANK) `wired`
- DV-VB15 udl-vblank bind  (UDL VBLANK) `wired`
- DV-VB16 gma500-vblank bind  (GMA500 VBLANK) `wired`
- DV-VB17 virtual-vblank bind  (Virtual VBLANK) `wired`
- DV-VB18 xen-drm-vblank bind  (Xen DRM VBLANK) `wired`
- DV-VB19 bochs-vblank bind  (Bochs VBLANK) `wired`
- DV-VB20 cirrus-vblank bind  (Cirrus VBLANK) `wired`
- DV-VB21 qxl-vblank bind  (QXL VBLANK) `wired`
- DV-VB22 virtio-gpu-vblank bind  (Virtio GPU VBLANK) `wired`
- DV-VB23 vmwgfx-vblank bind  (VMware GPU VBLANK) `wired`
- DV-VB24 imx-drm-vblank bind  (i.MX DRM VBLANK) `wired`
- DV-VB25 pl111-vblank bind  (PL111 VBLANK) `wired`
- DV-VB26 xlnx-vblank bind  (Xilinx DRM VBLANK) `wired`

## DV-SPDIFRX (DV-SR theme)

- DV-SR01 snd-hda-intel-spdif-rx bind  (S/PDIF receiver) `wired`
- DV-SR02 rt286-spdif-rx bind  (Realtek SPDIF RX) `wired`
- DV-SR03 alc-spdif-rx bind  (ALC SPDIF RX) `wired`
- DV-SR04 cirrus-spdif-rx bind  (Cirrus SPDIF RX) `wired`
- DV-SR05 wm8280-spdif-rx bind  (WM8280 SPDIF RX) `wired`
- DV-SR06 stac-spdif-rx bind  (STAC SPDIF RX) `wired`
- DV-SR07 sigmatel-spdif-rx bind  (Sigmatel SPDIF RX) `wired`
- DV-SR08 via-spdif-rx bind  (VIA SPDIF RX) `wired`
- DV-SR09 nVIDIA-spdif-rx bind  (NVIDIA SPDIF RX) `wired`
- DV-SR10 intel-spdif-rx bind  (Intel SPDIF RX) `wired`
- DV-SR11 amd-spdif-rx bind  (AMD SPDIF RX) `wired`
- DV-SR12 asihpi-spdif-rx bind  (ASI HPI SPDIF RX) `wired`
- DV-SR13 asoundspdif-rx bind  (ASoUND SPDIF RX) `wired`
- DV-SR14 hdmi-spdif-rx bind  (HDMI SPDIF RX) `wired`
- DV-SR15 usb-spdif-rx bind  (USB SPDIF RX) `wired`
- DV-SR16 iec61883-spdif-rx bind  (IEC61883 SPDIF RX) `wired`
- DV-SR17 firewire-spdif-rx bind  (FireWire SPDIF RX) `wired`
- DV-SR18 bluetooth-a2dp-spdif-rx bind  (BT A2DP SPDIF RX) `wired`
- DV-SR19 sony-spdif-rx bind  (Sony SPDIF RX) `wired`
- DV-SR20 panasonic-spdif-rx bind  (Panasonic SPDIF RX) `wired`
- DV-SR21 toshiba-spdif-rx bind  (Toshiba SPDIF RX) `wired`
- DV-SR22 realtek-pci-spdif-rx bind  (Realtek PCI SPDIF RX) `wired`
- DV-SR23 idt-spdif-rx bind  (IDT SPDIF RX) `wired`
- DV-SR24 via8237-spdif-rx bind  (VIA 8237 SPDIF RX) `wired`
- DV-SR25 intel-sst-spdif-rx bind  (Intel SST SPDIF RX) `wired`
- DV-SR26 amd-audiopc-spdif-rx bind  (AMD AudioPC SPDIF RX) `wired`

## DV-FUSEFS (DV-FU theme)

- DV-FU01 fuse-bind mount  (FUSE bind) `wired`
- DV-FU02 fuse-sshfs mount  (SSHFS) `wired`
- DV-FU03 fuse-ntfs mount  (NTFS-3G) `wired`
- DV-FU04 fuse-mp3fs mount  (mp3fs) `wired`
- DV-FU05 fuseiso mount  (FUSE ISO) `wired`
- DV-FU06 encfs mount  (encfs) `wired`
- DV-FU07 bindfs mount  (bindfs) `wired`
- DV-FU08 fuse-smb mount  (SMB/CIFS) `wired`
- DV-FU09 fuse-ftp mount  (FTPFS) `wired`
- DV-FU10 fuse-curl mount  (curl) `wired`
- DV-FU11 fuse-http mount  (HTTPFS) `wired`
- DV-FU12 fuse-gmailfs mount  (GmailFS) `wired`
- DV-FU13 fuse-wdfs mount  (WebDAV DFS) `wired`
- DV-FU14 fuse-avfs mount  (AVFS) `wired`
- DV-FU15 fuse-archivemount mount  (ArchiveMount) `wired`
- DV-FU16 fuse-unionfs mount  (UnionFS) `wired`
- DV-FU17 fuse-aufs mount  (AUFS) `wired`
- DV-FU18 fuse-squashfs mount  (SquashFS) `wired`
- DV-FU19 fuse-btrfs mount  (Btrfs) `wired`
- DV-FU20 fuse-zerofs mount  (ZeroFS) `wired`
- DV-FU21 fuse-memfs mount  (MemFS) `wired`
- DV-FU22 fuse-procfs mount  (ProcFS) `wired`
- DV-FU23 fuse-sysfs mount  (SysFS) `wired`
- DV-FU24 fuse-debugfs mount  (DebugFS) `wired`
- DV-FU25 fuse-configfs mount  (ConfigFS) `wired`
- DV-FU26 fuse-tracefs mount  (TraceFS) `wired`
