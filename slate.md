# WuBuOS Slate — Active Work Surface

## Current Focus: 2284-GAP CLOSURE CAMPAIGN

**Phase**: 13 — Post-automated-audit gap closure
**Mode**: Perpetual gap-closer loop — execute until 2284 → 0
**Constraint**: "Rewriting from scratch in C" — no stubs, no scaffolding, no "for later"

## Active Work Item
- [ ] Pick next gap from priority matrix (BATTLESHIP.md)
- [ ] Write real C implementation
- [ ] Run relevant test target
- [ ] Verify build passes (make all)
- [ ] Commit → repeat

## Priority Queue (Top 10)
1. wubu_oci.c — OCI manifest/blob/config/registry (84 gaps)
2. wubu_network.c — netlink bridge/vxlan/wg/tailscale (122 gaps)
3. wubu_snapshot.c — overlay mount, dir_size, restore (82 gaps)
4. wubu_holyd.c — mouse routing, session restore, event loop (75 gaps)
5. wubu_vsl.c — ELF PT_LOAD, syscall translation (72 gaps)
6. interrupt.c — IOAPIC, LAPIC, TSS, ISR assembly (111 gaps)
7. fat32.c — filesystem ops (57 gaps)
8. wubu_image.c — export, layer cache, base images (67 gaps)
9. wubu_proton.c — DXVK config, prefix, env (52 gaps)
10. wubu_archd.c — root create, pkg ops, health (45 gaps)

## Recently Closed (Vaulted)
✅ JIT self-hosted (310-313): wubu_x86, wubu_disasm, jit_minic — 82 tests
✅ Heap walk + red zones (340-341): bloom scan, canaries, mem_debug_dump — 29 tests
✅ Vulkan soft fallback (380-381): CPU GEMM/softmax/GAE — 3 TODOs resolved
✅ wm_send_mouse (390): holyd→dosgui_wm dispatch
✅ startmenu /apps scan (391): filesystem enumeration + dedup

## Blockers
- None — every gap is "rewrite in C" territory, no external deps blocking
- All test infrastructure working (58 targets, 747+ assertions)

## Notes
- 245 .c files, 111 .h files, ~123K LOC
- Real work LOC ≈ 100K (23K stub/scaffold)
- TempleOS parity target: 154K working LOC → need ~54K more real C
- 197+ test targets passing
