# Research Index — WUBUOS Optimization Research

Persistent research artifacts (NOT /tmp — these survive across sessions).
Last updated: 2026-08-13

## Tailslayer / DRAM Latency
- `notes_tailslayer.md` — Full deep-dive: DRAM-refresh tail-latency hedge
  via channel-replicated hedged reads. The technique is a memory-subsystem
  shim, not a JIT. Port strategy for WuBuOS kernel + HC codegen.
  **IMPLEMENTED**: HCGen.hedge_loads (default on) emits a `prefetchnta`
  before every memory load class. Gate: `make test_hedge`.
- `tailslayer_hedge.h` — C header shim: `ts_hedge_init/insert/read/probe`
  replicating Tailslayer's 1 GB hugepage + 256-byte channel-offset replication.
- `trefi_probe.c` — DRAM refresh periodicity probe (clflush+reload,
  harmonic binning at 1T/2T/3T of expected tREFI).

## x86-64 Microarchitecture
- `x86_speedup_cheatsheet.md` — Latency/throughput table (Zen 4, Intel
  ARL-P/Golden Cove), LEA-shift-multiply table, division-avoidance,
  branchless (cmov/sign-bit mask), port-pressure/ILP, cache hints,
  verified HC-JIT encodings. Sources: Agner Fog instruction tables
  (2025-09-20), uops.info.

## Universal GPU Layer (SPIR-V + Vulkan, cross-vendor) — NEW 2026-08-13
- `gpu_universal_layer.md` — RESEARCH: SPIR-V as the universal kernel/shader IR;
  Vulkan (Mesa RADV/ANV/NVK) as the ONE interface running identically on
  NVIDIA/AMD/Intel/Arm/Apple/Steam-Deck. Driver map, PCI vendor IDs
  (0x10DE/0x1002/0x8086), vkGetPhysicalDeviceProperties device ranking,
  VK_KHR_cooperative_matrix (CUDA-competitive AI), VUDA (arXiv 2605.01352)
  CUDA↔Vulkan spatial sharing, container/Ring-0 framing, and the HolyC→SPIR-V
  self-hosted emitter plan. Grounded by a live host probe (default loader
  enumerates only llvmpipe; real GPU behind /dev/dxg gfxstream).

## Kevin-Bacon Trace
`notes_tailslayer.md` ends with the 7-step Kevin-Bacon trace from
Tailslayer → channel interleaving → port interleaving → software
pipelining → branchless → linear-scan RA → NUMA topology.

## Related (to be added)
- [ ] pext/pext variable-bit latency deep-dive (microcode penalty)
- [ ] AVX-512 / AVX2 vector-width utilization for WUBU kernel math
- [ ] HolyC→SPIR-V emitter prototype (the gpu_universal_layer §5/§7 next step)

## Driver Self-Installation / Numeracy — NEW 2026-08-13
- `driver_self_install.md` — the AGI-OS gap the user identified: WuBuOS
  DISCOVERS hardware (wubu_probe.c, wubu_drv.c, PCI/USB/ACPI/VirtIO
  buses, 40+ subsystem headers) but when a device has NO registered driver
  it writes "unbound" and stops — it does NOT fetch/build/compile + load a
  driver module. Research + design of the missing `wubu_drv_install()`
  arm: modalias synthesis (pci/usb/acpi), a manifest table
  (modalias→{local|pkg|git|fw_container}), DKMS-style in-kernel build,
  in-memory module load + live re-probe, CO-RE-style relocations, batch
  coldplug, GPU ICD self-install (real dGPU > llvmpipe), and the firmware
  self-fetch path (wubu_fw.c) that driver-modules must mirror. Sources:
  Linux MODULE_DEVICE_TABLE + udev modalias, DKMS, BPF CO-RE/BTF, Linux
  device-tree PCI nodes. Honest gaps enumerated.
