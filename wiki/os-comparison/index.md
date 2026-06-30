# WuBuOS OS Comparison Wiki — Index
**Location**: `/home/wubu/.hermes/profiles/mind-palace/home/myseed/wiki/os-comparison/`
**Updated**: 2026-06-29

---

## ══════════════════════════════════════════════════════════════════════════════════
## WIKI STRUCTURE
## ══════════════════════════════════════════════════════════════════════════════════

```
wiki/os-comparison/
├── index.md                          ← This file
├── triple_da_wiki.md                 ← Full Triple DA comparison (GNOME vs SteamOS vs TempleOS vs Arch vs WuBuOS)
├── gap_analysis_wiki.md              ← Actionable gap tracking (Feature → WuBuOS File → REAL_GAPs → Priority)
├── gnome_study.md                    ← GNOME 46+ architecture & feature inventory
├── steamos_study.md                  ← SteamOS 3.x / gamescope / Proton / Pressure Vessel inventory
├── templeos_study.md                 ← TempleOS / ZealOS HolyC / RedSea / Ring-0 inventory
├── arch_study.md                     ← Arch Linux / pacman / systemd / AUR / mkinitcpio inventory
└── README.md                         ← Quick start guide
```

---

## ═════════════════════════════════════════════════════════════════════════════════
## QUICK START
## ══════════════════════════════════════════════════════════════════════════════════

### For Gap Closure Work
1. **Start here**: `gap_analysis_wiki.md` — Pick highest priority gap from "NEXT SESSION PICK LIST"
2. **Reference**: `triple_da_wiki.md` — Understand the full context of why this gap exists
3. **Deep dive**: OS-specific study files for implementation details

### For Architecture Decisions
1. **Read**: `triple_da_wiki.md` → "TRIPLE DEVIL'S ADVOCATE: CONFLICTING REQUIREMENTS" section
2. **Reference**: OS-specific study files for technical details

### For Session Handoff
1. **Update**: `gap_analysis_wiki.md` → "SESSION CLOSED" log with void casts eliminated
2. **Update**: `BATTLESHIP.md` (in myseed root) → Active REAL_GAPs only
3. **Update**: `vault/achievements.md` → Immutable achievement record

---

## ══════════════════════════════════════════════════════════════════════════════════
## KEY FILES OUTSIDE THIS WIKI
## ══════════════════════════════════════════════════════════════════════════════════

| File | Purpose | Location |
|------|---------|----------|
| **BATTLESHIP.md** | Active REAL_GAPs only (work to do) | `/home/wubu/.hermes/profiles/mind-palace/home/myseed/BATTLESHIP.md` |
| **vault/achievements.md** | Immutable record of resolved gaps | `/home/wubu/.hermes/profiles/mind-palace/vault/achievements.md` |
| **My-Seed-Project.md** | WuBuOS architecture overview | `/home/wubu/.hermes/profiles/mind-palace/wiki/My-Seed-Project.md` |
| **Phase vaults** | Archived phase summaries | `/home/wubu/.hermes/profiles/mind-palace/home/myseed/vault/phases/` |

---

## ══════════════════════════════════════════════════════════════════════════════════
## CURRENT STATUS SNAPSHOT (2026-06-29)
## ═══════════════════════════════════════════════════════════════════════════════════

| Metric | Value |
|--------|-------|
| **REAL_GAPs** | 1434 (down from 1562) |
| **Tests Passing** | 747+ across 30+ suites |
| **C Files** | 73 |
| **Lines of Code** | ~15K real LOC |
| **Sessions Since Start** | ~20 |
| **Gaps Closed This Session** | 128 |

### Completed This Session (2026-06-28/29)
- ✅ **GUI Subsystems**: WM (16 tests), Terminal (PTY), Explorer (74 tests, Styx 9P), Start Menu (4 tests, .desktop), Clipboard (17 tests, multi-MIME) — **111 tests**
- ✅ **Bear Vulkan**: 4 compute pipelines (Policy Forward, GAE, N-Pole Step, MMA MatMul) + 4 SPIR-V shaders
- ✅ **VSL Syscalls**: 26 → 45 Linux syscalls via host delegation
- ✅ **ZealOS Name Parity**: 96/96 (32 aliases added)
- ✅ **StyxFS**: Full 9P2000 callback set (walk, read, stat, open, clunk, remove, create, write)
- ✅ **Bridge Syscalls**: fd-based handlers, Styx offset tracking, container fork+exec, 26 trampolines
- ✅ **Hosted Wayland**: Registry/kbd/pointer callbacks, modifier tracking, focus/axis handling
- ✅ **Bear cuDNN**: CPU fallbacks for all cuBLAS/cuDNN ops using bear_simd.h

### Next Critical Gap
**`hosted/wubu_metal.c`** — 31 void casts + 6 weak aliases + stubs (DRM/ALSA/Pulse/X11/Vulkan surface)

---

## ══════════════════════════════════════════════════════════════════════════════════
## TRIPLE DA METHODOLOGY
## ══════════════════════════════════════════════════════════════════════════════════

For every feature comparison:

1. **DA 1 — Reference OS**: What does GNOME/SteamOS/TempleOS/Arch actually do? (Specs, source, behavior)
2. **DA 2 — WuBuOS**: What does WuBuOS currently do? (Code, tests, BATTLESHIP.md)
3. **DA 3 — The Gap**: What is the REAL_GAP? (void casts, stubs, system() calls, missing files)

**Rule**: "Rewriting from scratch in C" = Every stub/void cast/system() = REAL_GAP that must be implemented.

---

## ══════════════════════════════════════════════════════════════════════════════════
## WUBuOS ARCHITECTURE REMINDER
## ═══════════════════════════════════════════════════════════════════════════════════

```
WuBuOS = ZealOS kernel + Win98 shell + Styx/9P + Arch containers
         ↓
         Hosted binary (Inferno emu pattern) runs on Linux
         ↓
         Wayland → VBE → ZealOS kernel → GUI shell → 9P → Containers
         ↓
         Arch base + Wine/DXVK/VKD3D + gamescope → Windows compat
         ↓
         "Rewriting from scratch in C" = THE WORK
         Form≠Function = THE ENEMY
         Triple DA = THE FILTER
         1434 REAL_GAPs = THE SCOREBOARD
```

---

*This wiki is the single source of truth for WuBuOS gap analysis. Update it every session.*