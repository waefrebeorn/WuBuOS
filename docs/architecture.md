# WuBuOS — Architecture & Roadmap (v5 — Post Stub+Form Hunt)

> **LOCKED** — Changes require explicit unlock + re-lock ceremony.
> This document reflects ground truth from the full stub+form gap hunt.

## Vision (Revised)

WuBuOS is NOT the kernel. It is a **GUI shell + container runtime** that wraps the ZealOS kernel, following the Inferno OS `emu` pattern:

- **ZealOS IS the kernel** — ring-0, single-user, HolyC JIT, boots on real hardware (154K LOC)
- **WuBuOS IS the shell** — Win98 desktop, Styx/9P namespace, .wubu container execution
- **The hosted binary IS the product** — `wubu` runs on Linux/Windows/macOS as a regular executable
- **Arch rip** — Arch Linux base for containers, ripping through Linux drivers for SteamOS/Proton

## Roadmap

### Phase A: Fill the Hollow Citadel (highest ROI)

| Cell | What | Why |
|------|------|-----|
| 310 | HolyC codegen: ternary, while, for, logical-OR | Unblocks Cell 201 (REPL), which unblocks 202/206/207 |
| 311 | HolyC codegen: function calls, struct layout, string literals | Without this, REPL shows "a+b" only |
| 303 | Tasking: real timer tick + context switch (ucontext or setjmp) | No ZealOS app scheduling without this |
| 300 | Input: real keyboard/mouse event queue with push/pop | No event flows into any window |
| 301 | Interrupt: IDT setup + ISR registration (hosted: signal dispatch) | Required for IRQ-driven I/O |

### Phase B: Delete Dead Code

| Cell | What | Why |
|------|------|-----|
| 324 | Delete wubu_vsl.c — 712 lines of (void) casts, superseded by wubu_host_exec.c | Dead code, false API surface |
| 343 | Delete wubu_vsl_init/run from wubu_exec.c | Route through wubu_host_exec |
| 330 | Replace wubu_proton.c with host Wine delegation | 406 lines of names, 0 PE loads |

### Phase C: Container Polish

| Cell | What | Why |
|------|------|-----|
| 350 | Per-container 9P Styx dispatch inside container | Socket exists but no walk/read |
| 351 | Arch rootfs + Steam Runtime binaries | Infrastructure, not code |
| 353 | cgroup/setrlimit enforcement per container | Config stored but never applied |

### Phase D: App Wiring

| Cell | What | Why |
|------|------|-----|
| 361 | REPL text rendering (bitmap font) | Black rect only currently |
| 362 | Notepad implementation | Pure stub |
| 372 | wm_invalidate: actual dirty rect marking | Empty { } currently |

### Phase E: Integration

| Cell | What | Why |
|------|------|-----|
| 201 | HolyC REPL compiles + executes in-process | Depends on 310, 311 |
| 202 | GUI dispatches events to ZealOS apps | Depends on 300, 301, 303 |
| 204 | Per-container 9P namespace wired | Depends on 350 |
| 205 | SteamOS container launches | Depends on 351 |
| 206 | Bare-metal boot | Depends on 201, 202 |
| 207 | Integration test: wubu runs, GUI appears, REPL works | Depends on 201, 202 |

## The Real Stack

```
┌──────────────────────────────────────────────────────────────────┐
│  Layer 5: .wubu Containers                                       │
│    SteamOS.wubu ─── Steam Runtime + Proton + Games (Arch base)  │
│    Brave.wubu ──── Chromium + host GPU passthrough              │
│    Temple.wubu ─── HolyC REPL + ZealOS userland                 │
├──────────────────────────────────────────────────────────────────┤
│  Layer 4: Container Runtime (wubu_host_exec)                     │
│    fork + chroot + execv (NOT VSL syscall emulation)            │
│    9P namespace: per-container Styx socket mount                 │
│    GPU: /dev/dri + /dev/nvidia* → container                     │
│    Arch base → DRM/KMS/NVIDIA/AMD driver passthrough            │
├──────────────────────────────────────────────────────────────────┤
│  Layer 3: Win98 GUI Shell (Cell 200 ✅)                          │
│    WM, taskbar, start menu, desktop, DOS flip ↔ Temple REPL     │
│    26 WM tests, 13 start menu tests, 17 dbuf tests — REAL      │
├──────────────────────────────────────────────────────────────────┤
│  Layer 2: Platform Layer (Inferno os.c pattern)                  │
│    Linux: X11/Wayland + host libc + DRM/KMS GPU passthrough    │
│    Windows: Win32 + WSL or native                                │
│    Bare: ZealOS kernel boots → WuBuOS as HolyC shell            │
├──────────────────────────────────────────────────────────────────┤
│  Layer 1: ZealOS Kernel (NOT WuBuOS — already boots on metal)   │
│    Memory, task, VBE, FAT32, AHCI, HolyC JIT, RedSea FS        │
└──────────────────────────────────────────────────────────────────┘
```
