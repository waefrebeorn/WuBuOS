# macOS stub inventory (2026-11-19)

Every macOS-facing piece of code in the ~WuBu~ tree, its status, and
where it lives. Copies of the small live branches are in
apple-branches.md; the Quartz backend skeleton is wcc_quartz.*.stub.
Originals stay wired in their own repos — this folder collects, it does
not move code.

## Live Apple branches (written, compile-pending on a Mac)

| file | branch | gate today | macOS gate |
|---|---|---|---|
| wubucompositor/src/wcc_spawn.c | `_NSGetExecutablePath` + realpath for guest-path derivation | test-spawn ALL PASS (Linux leg) | workflows/macos-verify.yml phase 1 |
| wubuos/src/hosted/hosted.c | same pattern in the buddy launcher | hosted build green (Linux leg) | phase 3 (wubuos-side workflow) |

## VSL macOS execution layer (wubuos/src/runtime/vsl/)

- vsl_macho.c / vsl_macho.h — Mach-O + FAT loader. Gated:
  test_vsl_macho 24/24, test_kernel_macho ALL PASS (the magic-number
  regression). Format constants: include/wubu_macho_fmt.h (one owner).
- vsl_syscall_mac_bsd.c — BSD syscall personality tables.
- vsl_mach_ipc.h — Mach trap / IPC surface.
- Tests: vsl_macho_test.c, vsl_mac_syscall_test, vsl_macclassic_test.
- Status: parse + tables verified ON LINUX. Full Mach-O PROCESS
  execution is OPEN: the historical path shelled to darling, which is
  a dead end (skill wubuos-macos-leg-proof). Running the VSL mac tests
  ON macOS runners is the next honesty step (research/01 phase 3).

## Metal shader leg (proven, but locate the crate first)

- The proven macOS graphics leg is metal2vulkan: Metal AIR/LLVM .ll ->
  SPIR-V, executed on a real Vulkan compute pipeline (buffer[0]==42
  proof, spirv-val green). Procedure: skill wubuos-macos-leg-proof.
- WHEREABOUTS: not found on this box this session (searched ~/metal2vulkan,
  ~/opt/*, wubuos depth 4). The skill references a WSL2 box; the crate
  likely lives there. Re-locate or re-fetch before re-running the proof.
  Stated plainly: the proof artifacts are skill-recorded, not re-verified
  in this session.
- NAMING WARNING: wubuos/src/hosted/wubu_metal*.c are BARE-METAL boot
  files ("Metal" = no host OS), NOT Apple Metal. Do not conflate.

## Quartz host backend (ABSENT — the skeleton is here)

- wcc_quartz.h.stub / wcc_quartz.c.stub: the shape of the macOS host
  leaf, mirroring wcc_gpu.c's concern boundary. NOT compiled, NOT
  verified, bodies are honest failure stubs. Rename to .c/.h and
  implement on the macOS loop; the CI workflow compile-checks it the
  moment it lands.
- The seam it plugs into: wcc_internal.h (host fields: Display*, Window,
  XImage, XShmSegmentInfo, gpu struct) + the host leaves wcc_gpu.c,
  wcc_present (blit/XShm), wcc_x11 (dispatch/input), wcc_monitors.c
  (XRandR -> CGGetActiveDisplayList on macOS).
- Input mapping: X keycodes -> CGEventTap keycodes; the seat logic
  (wl_fixed_t, modifiers) is host-agnostic and stays.

## Submodule __APPLE__ sites (their own repos' concern)

- src/apps/wubupad/src/pkgmgr/pkgmgr.c
- src/apps/wubuoffice/src/wubuocr/ocl_gru.c
- src/brain (ggml-cpu.cpp etc.) — brain is its own mission.
Not editable from the body; listed so the census of macOS stubs is complete.

## What is NOT a stub

- The Wayland protocol machinery builds on macOS via epoll-shim
  (research/05) — no stub needed there, just the MacPorts recipe.
- POSIX shm keymap transport: already portable, shipped.