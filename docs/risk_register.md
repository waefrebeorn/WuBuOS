# WuBuOS — Battleship / Risk Register (v2 — Post Hard-Dive)

**Methodology**: Behavioral verification. ✅ means feature works at RUNTIME, not just compiles.
**Current state**: 30 C files, 11K real LOC, 462 signature tests, 0 behavioral tests
**Architecture**: WuBuOS = GUI shell + container runtime. ZealOS IS the kernel.

## Real Gaps (Cells 200-207)

| Cell | Description | Depends | Status |
|------|-------------|---------|--------|
| 200 | ZealOS kernel runs in-process inside `wubu` hosted binary | — | ⬜ |
| 201 | HolyC REPL compiles and executes code in-process | 200 | ⬜ |
| 202 | Win98 GUI dispatches real input events to ZealOS apps | 200 | ⬜ |
| 203 | Fork+exec for Linux .wubu containers (host delegation) | — | ⬜ |
| 204 | Per-container 9P namespace mount (Styx socket isolation) | 203 | ⬜ |
| 205 | SteamOS container launches with GPU passthrough | 203 | ⬜ |
| 206 | Bare-metal: ZealOS boots → WuBuOS as Win98 shell | 200,201,202 | ⬜ |
| 207 | Integration test: `wubu` binary runs, GUI appears, REPL executes code | 200,201,202 | ⬜ |

## What Actually Works (runtime-verified)

- Styx/9P2000 message serialization (real bytes on wire)
- Win98 GUI pixel rendering (real X11 pixels)
- Ctrl+Alt+T DOS flip (real X11 key event)
- Double-buffered rendering with dirty rects
- Test infrastructure (462 tests, TDD discipline)

## Systemic Risks

| # | Risk | Sev | Truth |
|---|------|-----|-------|
| S1 | VSL is dispatch table, not Linux compat | 🔴 | Rename to wubu_host_linux.c. Delegate to host libc. |
| S2 | Proton cannot load PEs | 🔴 | Use host Wine via platform delegation. |
| S3 | Bare-metal ≠ rip Linux drivers | 🔴 | ZealOS boot + WuBuOS as HolyC shell. |
| S4 | SteamOS needs real Linux process execution | 🔴 | Cell 203: host fork+exec. |
| S5 | LOC was inflated (claimed 41K, real 11K) | 🟡 | Honest accounting done. |

## Old Battleship (001-110) — ARCHIVED
Tracked API signatures, not runtime behavior. All 36 cells resolved at signature level.
Zero resolved at behavioral level. See vault/achievements.md for archive.
