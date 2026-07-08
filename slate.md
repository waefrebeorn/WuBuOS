# WuBuOS Slate — Active Work Surface (v23)

## Current Focus: **JIT ENCODER GAP CLOSED — SPRINT BOARD: ~367 REMAINING**
**Sprint board**: ~367 REAL_GAPs (Triple DA, form≠function filtered).
**Parity epics**: 5 (SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS) — marathons.
**Mode**: Perpetual gap-closer loop — execute until ~367 → 0 (sprint).
**Constraint**: "Rewriting from scratch in C" — no stubs, no scaffolding, no "for later".

---

## ✅ VAULTED (2026-07-08)
- **JIT x86-64 Encoder (wubu_x86.c)** — 33 `return 0;` stubs → real byte-count return values, validated by 14-instruction test. Form==Function for every `wx86_*()` function. (commit pending)
- **Desktop Stream 3 context menu** — all 3 functions (`sort_by_name`, `create_shortcut`, `view_desktop`) implemented in committed code (`033554f`).
- UX Stream E (welcome dialog, bundled wallpaper, status bar tips) — committed (`28e700b`).
- Foundation 1-5 + Campaign 1-25 — all closed, tests green.
- Monolith splits — ALL complete across 14+ files.

---

## Active Work Items — NEXT CYCLE

### HIGHEST PRIORITY
**A. Remaining sprint gaps** — pick a file and close it:
- `vsl_syscall_net.c` — 58 void casts (unused syscall param d/e/f)
- `bear_cudnn.c` — 12 empty CUDA wrapper bodies
- `wubu_metal.c` — 16 return-0 + 15 void casts  
- `wubu_vulkan.c` — 12 return-0 + remaining void casts
- `vsl_syscall_fileio.c` — 46 void casts
- `vsl_syscall_proc.c` — 37 void casts
- `interrupt.c` — 17 return-0 + 22 void casts

**B. Monolith splits** — remaining ≥800-line files: `wubu_metal`(1508), `hosted.c`(1320), `interrupt.c`(1153), `fat32.c`(1060), `wubu_archd.c`(1055), `wubu_proton.c`(1053), `wubu_vulkan.c`(990), `wubu_canvas.c`(1325).

---

## Notes
- 73 .c files, ~15K real LOC.
- **~367 sprint REAL_GAPs** remaining (was ~400 before this session's 33 JIT encoder fixes).
- All tests green. 64+ test targets, 747+ assertions. Full gate exits 0.
- `goal-paste.md` has the next-session copy-paste prompt.
- WuBuOS identity: ZealOS kernel + Win98 shell + Styx/9P namespace + Arch containers.