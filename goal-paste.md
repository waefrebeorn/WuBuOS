# Goal Paste — WuBuOS 2284-GAP Campaign

## Primary Goal
**Close 2284 REAL_GAPs identified by automated form-vs-function audit.**

Every gap = "rewriting from scratch in C" — empty bodies, void casts only, return -1 no logic, TODO stubs.

## Priority Order (Critical → High → Medium)

### 🔴 CRITICAL — Runtime Stack (996 gaps)
1. **wubu_oci.c** (84) — OCI manifest/blob/config/registry HTTP
2. **wubu_network.c** (122) — netlink bridge/vxlan/wireguard/tailscale
3. **wubu_snapshot.c** (82) — overlay mount/umount, dir_size, restore
4. **wubu_holyd.c** (75) — mouse routing, session restore, event loop
5. **wubu_vsl.c** (72) — ELF PT_LOAD, syscall translation, fd delegation
6. **wubu_image.c** (67) — export, layer cache, base images
7. **wubu_archd.c** (45) — root create, pkg ops, health checks
8. **wubu_bottles.c** (38) — import/export/run .wubu bottles

### 🔴 CRITICAL — Kernel (254 gaps)
9. **interrupt.c** (111) — IOAPIC, LAPIC, TSS, ISR assembly
10. **fat32.c** (57) — filesystem ops
11. **tasking.c** (22) — spawn, kill, suspend, sleep, yield, priority
12. **memory.c** (15) — heap walk, validate, used/available
13. **ahci.c** (23) — port init, FIS, cmd, read, write
14. **txfs.c** (18) — mount, journal, txn begin/commit

### 🟠 HIGH — GUI Shell (326 gaps)
15. **dosgui_wm.c** (44) — input dispatch, holyc term, window ops
16. **wubu_proton.c** (52) — DXVK config, prefix, env
17. **wubu_gamelib.c** (36) — scan, startmenu integration
18. **dosgui_explorer.c** (22) — tree, breadcrumbs, preview, ops
19. **dosgui_term.c** (24) — PTY, tabs, ANSI, scrollback, copy/paste

### 🟠 HIGH — Bear RL (212 gaps)
20. **bear_nn.c** (46) — checkpoint save/load, layers, optimizers
21. **bear_vulkan.c** (25) — forward/GAE/env dispatch
22. **bear_cudnn.c** (40) — cuDNN handle, conv, activation, pooling
23. **bear_cuda.c** (24) — CUDA malloc/free, kernels
24. **bear_vulkan_soft.c** (29) — CPU fallback implementations

### 🟠 HIGH — Hosted Platform (163 gaps)
25. **wubu_vulkan.c** (51) — instance, device, swapchain, pipelines
26. **wubu_metal.c** (34) — DRM/KMS, ALSA, Pulse, evdev
27. **hosted.c** (54) — Wayland frame, fs reset, SHM

## Work Mode
- Pick ONE gap at a time
- Write real C that does real work
- Test passes (make test_XXX)
- Build passes (make all)
- Repeat until 2284 → 0

## Mantra
"Rewriting from scratch in C is the point. Form ≠ Function = REAL_GAP."
