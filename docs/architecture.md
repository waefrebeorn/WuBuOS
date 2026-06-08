WuBuOS — Architecture Definition (v2 — Post Hard-Dive)

> **LOCKED** — Changes require explicit unlock + re-lock ceremony.
> This document reflects ground truth from the Hard Dive + Triple DA sweep.

## Vision (Revised)

WuBuOS is NOT a standalone OS. It is a **GUI shell + container runtime** that wraps the ZealOS kernel, following the Inferno OS `emu` pattern:

- **ZealOS IS the kernel** — ring-0, single-user, HolyC JIT, boots on real hardware (154K LOC)
- **WuBuOS IS the shell** — Win98 desktop, Styx/9P namespace, .wubu container execution
- **The hosted binary IS the product** — `wubu` runs on Linux/Windows/macOS as a regular executable

## The Real Stack

```
┌──────────────────────────────────────────────────────────────────┐
│  Layer 5: .wubu Containers                                       │
│    SteamOS.wubu ─── Steam Runtime + Proton + Games              │
│    Brave.wubu ──── Chromium + host GPU passthrough              │
│    Temple.wubu ─── HolyC REPL + ZealOS userland                 │
├──────────────────────────────────────────────────────────────────┤
│  Layer 4: Container Runtime                                      │
│    .wubu exec: host fork+exec (NOT VSL syscall emulation)       │
│    9P namespace: per-container Styx socket mount                 │
│    Resource limits: CPU, memory, GPU, network per container      │
├──────────────────────────────────────────────────────────────────┤
│  Layer 3: Win98 GUI Shell (what we actually built)               │
│    WM, taskbar, start menu, desktop, DOS flip ↔ Temple REPL     │
│    26 WM tests, 13 start menu tests, 17 dbuf tests — REAL      │
├──────────────────────────────────────────────────────────────────┤
│  Layer 2: Platform Layer (Inferno os.c pattern)                  │
│    Linux: X11/Wayland + host libc + DRM/KMS GPU passthrough    │
│    Windows: Win32 + wsl or native                                │
│    Bare-metal: ZealOS kernel + ZealOS drivers                   │
├──────────────────────────────────────────────────────────────────┤
│  Layer 1: ZealOS Kernel (the base — NOT ported, USED as-is)     │
│    Memory, tasking, VBE, FAT32, AHCI, networking                │
│    HolyC JIT (already compiles and runs in ZealOS)              │
│    RedSea filesystem, DolDoc system                              │
└──────────────────────────────────────────────────────────────────┘
```

## What Actually Exists (Honest Accounting)

| Component | C Files | Real LOC | Actually Does |
|-----------|---------|----------|---------------|
| Kernel stubs | 8 | ~2,400 | Structs + init/shutdown. No real HW init, no IDT, no GDT, no scheduler |
| JIT mmap stub | 1 | ~230 | Only a+b, a*b, a-b, -a, const. Not a real compiler |
| HolyC compiler | 3 | ~2,200 | Lexer/parser/codegen skeleton. No real x86-64 output |
| Runtime (.wubu, VSL, Proton, Styx, StyxFS, pkg) | 7 | ~3,800 | API surface. No real process creation, no ELF load, no PE exec |
| GUI (wm, taskbar, desktop, theme, dbuf, startmenu) | 6 | ~1,200 | Real pixel rendering in X11. No real event dispatch to apps |
| Bridge (mode switch, clipboard, IPC) | 2 | ~600 | Toggle works. No Temple REPL integration |
| Hosted (X11 launcher) | 1 | ~500 | Opens X11 window. Gray pixels. No OS services inside |
| Apps (REPL, notepad stubs) | 2 | ~150 | Shells |
| **TOTAL** | **30** | **~11,100** | **462 tests against API signatures** |

## Reference Implementations (Not Ours)

| Reference | LOC | What It Does |
|-----------|-----|-------------|
| Inferno OS | 767K | Real OS. Dis bytecode VM. 9P2000 namespace. `emu` hosted binary on Linux/Win/Mac. Real kernel. |
| ZealOS | 154K | Real OS. Boots on metal. HolyC JIT. RedSea FS. Networking. DolDoc. |

## Key Architectural Decisions

1. **ZealOS kernel runs in-process** (like Inferno emu). Not ported — compiled for hosted mode alongside WuBuOS.

2. **VSL is renamed to host delegation**. On Linux, call host libc. On Windows, call Win32. Do NOT emulate Linux syscalls from scratch.

3. **Proton uses host Wine** when available. Do NOT write a PE loader from scratch.

4. **Containers = host fork/exec**. A .wubu is an ELF binary that runs as a Linux process. 9P namespace provides isolation.

5. **Bare-metal path: ZealOS boot → WuBuOS as HolyC shell**. WuBuOS compiles to HolyC source that ZealOS JIT loads at boot.

## Constraints (LOCKED)

1. **Pure C for shell** — no C++ in WuBuOS shell, GUI, or bridge
2. **ZealOS kernel is consumed, not forked** — we build on top, not modify
3. **Under 100K LOC** for WuBuOS shell + container runtime (ZealOS excluded)
4. **Public domain** for WuBuOS additions; ZealOS license for kernel
5. **Honest accounting** — real LOC, real test semantics, no inflation

*Locked: 2026-06-07*
*Hard-Dive revision: v2*
