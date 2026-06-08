# WuBuOS — Battleship / Risk Register (POST HARD DIVE)

**Methodology**: Triple DA (Affirm → Attack → Synthesize) per phase
**Last audit**: 2026-06-07 (Hard Dive + Triple DA sweep)
**Current state**: 11K real C LOC (not 41K), 462 tests passing against API signatures, NOT against real OS behavior

## ⚠️ ARCHITECTURAL REALITY
- WuBuOS **does not boot**. Has no kernel entry point, no IDT, no GDT.
- The hosted binary is the only executable, and it only renders gray pixels.
- VSL has 46 syscalls but **cannot create a process** (no ELF loader, no fork/exec).
- Proton has Win32 API names but **cannot load a PE**.
- The .wubu container format is a header + blob, **not a real container** (no process isolation, no namespace, no resource limits).
- **THE BATTLESHIP WILL NOW TRACK REAL BEHAVIOR, NOT API SIGNATURES.**

## What Actually Works (verified)
- Styx/9P2000 message serialization/deserialization (real bytes on wire)
- Win98 GUI drawing primitives (real pixels in X11 framebuffer)
- Ctrl+Alt+T mode toggle (real X11 key event → real mode switch)
- Double-buffered rendering with dirty rect tracking
- Test infrastructure (462 tests, TDD discipline)

## New Battleship — Real Cells

| Cell | Description | Status |
|------|-------------|--------|
| 200 | ZealOS kernel runs in-process inside `wubu` hosted binary | ⬜ |
| 201 | HolyC REPL actually compiles and executes code in-process | ⬜ |
| 202 | Win98 GUI dispatches real input events to ZealOS apps | ⬜ |
| 203 | Fork+exec for Linux .wubu containers (host delegation, not VSL emulation) | ⬜ |
| 204 | Per-container 9P namespace mount (Styx socket isolation) | ⬜ |
| 205 | SteamOS container launches with GPU passthrough | ⬜ |
| 206 | Bare-metal: ZealOS boots → WuBuOS as Win98 shell | ⬜ |
| 207 | Integration test: `wubu` binary runs, GUI appears, REPL executes code | ⬜ |

## Old Battleship (Cells 001-110) — ARCHIVED
These tracked API signature existence, not runtime behavior.
See vault/achievements.md for evidence archive.
All 36 cells resolved at API-signature level. Zero resolved at behavioral level.

## Systemic Risks (Updated)

| # | Risk | Sev | Reality |
|---|------|-----|---------|
| S1 | VSL is not a Linux compat layer — it's a dispatch table pretending | 🔴 | Rename to wubu_host_linux.c. Delegate to host libc. |
| S2 | Proton cannot run PEs — use host Wine via platform delegation | 🔴 | Drop PE emulation; shell out to wine if available |
| S3 | Bare-metal requires ZealOS boot, not Linux driver ripping | 🔴 | WuBuOS = HolyC shell that ZealOS loads at boot |
| S4 | SteamOS as container needs real Linux process execution | 🔴 | Cell 203: host fork+exec, not VSL syscall emulation |
| S5 | LOC inflation: claimed 41K, actually 11K | 🟡 | Honest accounting restores credibility |
| S6 | Inferno's 767K LOC is the real reference implementation | 🟡 | Study emu/Linux/os.c as the platform layer template |
