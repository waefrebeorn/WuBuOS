# WuBuOS — Battleship / Risk Register

**Methodology**: Triple DA (Affirm → Attack → Synthesize) per phase  
**Last audit**: 2026-06-07  
**Current state**: 100 files, 28,097 LOC, 297/297 tests pass

---

## Systemic Risks (Cross-Phase)

| # | Risk | Sev | Mitigation | Status |
|---|------|-----|------------|--------|
| S1 | Complexity vs Simplicity — GUI + bridge may create "yet another hobby OS" | 🔴 | <100K LOC charter (locked) | 🟡 |
| S2 | Maintenance burden — one person | 🔴 | Public repo; tiny milestones | 🟡 |
| S3 | Hardware compat — VBE legacy | 🟡 | Software-rendered first; QEMU primary | ⬜ |
| S4 | Motivation burnout | 🔴 | Ship runnable builds early and often | 🟡 |
| S5 | "Why not ReactOS?" | 🟡 | Ownership IS the project | ✅ |

## Phase 0 Risks (COMPLETE)

| # | Risk | Prob | Impact | Status |
|---|------|------|--------|--------|
| P0-1 | Scope creep | High | Critical | 🟡 Mitigated |
| P0-2 | ZealOS HolyC-heavy | High | High | ✅ Hybrid approach |
| P0-3 | holyc-lang bugs | Med | Med | ✅ Verified, workaround |
| P0-4 | NanoShell maintenance | Low | Med | ✅ Forked as reference |

## Phase 1 Risks (IN PROGRESS)

| # | Risk | Prob | Impact | Status |
|---|------|------|--------|--------|
| P1-1 | JIT crashes whole OS | High | Critical | 🟡 Fuzz + AOT dual-mode |
| P1-2 | JIT slower than HolyC | Med | High | 🟡 Interpreter→simple→optimizing |
| P1-3 | Losing divine feel | Med | High | 🟡 Keep HolyC userland |
| P1-4 | mmap JIT neg rax encoding | Med | Med | ✅ Fixed: -O0 for stub |

## Phase 2 Risks (NEXT)

| # | Risk | Prob | Impact | Status |
|---|------|------|--------|--------|
| P2-1 | GUI sluggishness (software VBE) | Med | High | ⬜ Double-buffer early |
| P2-2 | Window mgmt complexity | Med | Med | ⬜ Start limited overlap |
| P2-3 | Aesthetics drift from Win98 | Low | Med | ⬜ 98.css reference |

## Phase 3 Risks

| # | Risk | Prob | Impact | Status |
|---|------|------|--------|--------|
| P3-1 | Context switch data loss | Med | Critical | ⬜ Transactional FS ops |
| P3-2 | Flip feels jarring | Med | Med | ⬜ Windowed Temple first |
| P3-3 | No isolation (ring-0) | Low | Low | ✅ Intentional |

## WorldSim Integration Risks

| # | Risk | Prob | Impact | Status |
|---|------|------|--------|--------|
| WS-1 | WorldSim render not wired to VBE | High | Med | ✅ vbe_ws_bridge wired |
| WS-2 | ECS 1024 entity limit | Low | Low | ✅ Sufficient for seed |
| WS-3 | Terrain gen determinism | Low | Low | ✅ xorshift64 seeded |

---

## Battleship Cell Status

| Cell | Description | Status |
|------|-------------|--------|
| 001 | ZealOS cloned + explored | ✅ |
| 002 | holyc-lang built, -transpile works | ✅ |
| 003 | NanoShellOS cloned as reference | ✅ |
| 004 | ZealOS booted in QEMU | ✅ |
| 005 | JIT mmap stub (20/20 tests) | ✅ |
| 006 | MIR built from source | ✅ |
| 007 | MIR JIT backend (add(3,4)=7) | ✅ |
| 010 | Kernel memory (14/14 tests) | ✅ |
| 011 | Kernel tasking (15/15 tests) | ✅ |
| 012 | Kernel VBE/input/interrupt stubs | ✅ |
| 020 | GUI wm/taskbar/desktop/theme | ✅ |
| 021 | NanoShellOS wm_nano forked into src/gui/wm_nano/ | ✅ |
| 030 | Bridge mode switch + clipboard + IPC | ✅ |
| 040 | Apps: REPL + Notepad | ✅ |
| 050 | WorldSim integrated (18/18 tests) | ✅ |
| 051 | moondream3_vision_weights.bin | ⬜ NOT PRESENT |
| 052 | Grand Makefile (67/67 tests) | ✅ |
| 060 | Bootable ISO | ✅ 20/20 tests |
| 070 | VBE → WorldSim render wiring | ✅ 25/25 tests |
| 071 | FAT32 filesystem | ✅ 20/20 tests |
| 072 | AHCI disk driver | ✅ 16/16 tests |
| 080 | HolyC compiler in C | ✅ 41/41 tests |
| 090 | .wubu container format | ✅ 38/38 tests |
| 091 | VSL (Linux virtualization layer) | ✅ 46/46 tests |
| 092 | Proton (Windows compat layer) | ✅ 24/24 tests |
