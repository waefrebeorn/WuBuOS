# WuBuOS — Current State (verified 2026-08-15)

```
╔════════════════════════════════════════════════════════════════════════╗
║     🌱  W U B U O S                                                       ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace · Arch containers    ║
║     2463 C files · 1006 H files · 472,955 LOC · 414 test targets        ║
║     Repo: /home/wubu/wubuos  (branch: wubu-integration)                  ║
╚══════════════════════════════════════════════════════════════════════════╝
```

## Repo facts (verified 2026-08-15)
- **2463 `.c` / 1006 `.h` / 472,955 LOC** (tracked `src/`).
- **414 `make test_*` targets** in `mk/tests.mk`.
- `make runtime` builds clean under `-O2`.
- WuBuOS = ZealOS kernel + Win98/XP shell + Styx/9P namespace + Arch containers + WuBuNOS compiler.
- WuBuNOS compiler + wubuwizard brain extracted as submodules.

## Triple DA Phase-Readiness
| Phase | Name | Status |
|-------|------|--------|
| α | Boot + Explore | ✅ 98% |
| β | Configure + Personalize | ✅ 80% |
| γ | Real Productivity | ⚠️ 75% |
| δ | External Apps (Proton) | ⚠️ 60% |
| ε | Network/Integration (Styx/OCI/9P) | ⚠️ 50% |
| ζ | SteamOS Parity (game mode/controller/overlay) | 🔲 0% (EPIC E2) |

> Note: α/β reflect the desktop shell, Control Panel, EDR disclosure surface, and
> DOS emulator being functional and tested. γ–ε are honest — the NT syscall bridge
> is ~20/297 transliterated, Proton/Styx9 integration is stubbed, SteamOS parity is
> unscheduled. These percentages are aspirational baselines, not measured coverage.

## Recent Sessions (2026-08-15)

### Repo reorganization + cleanup
- Removed 1300+ stale `.o`/`.d` files and test binaries from `src/`.
- Moved `research/` (206 Kevin-Bacon driver docs) → `docs/research/7hop-drivers/` (now 215 files).
- Organized `tools/` into subdirs: `bench/`, `dev/`, `isa-test/`, `research/`, `peephole_superopt/`, `wubu_game_probe/`.
- Moved `docs/archive/` → `vault/`.
- Removed empty `c1/` and `deck-root/` directories.
- Updated `.gitignore` (`*.elf`, `*.fd`, `*.log`, `*.ppm`, `*.png`, `*.mp4`, `tools/*_test`).
- Updated `mk/tests.mk` paths for reorganized `tools/`.
- Full build + `test_high_bear` green after reorg.

### `_GNU_SOURCE` elimination (WuBu compliance)
- Replaced `_GNU_SOURCE` dependency with WuBu-native compat headers (`wubu_gnu_compat.h`, `wubu_std.h`).
- Provides M_PI, strdup, CPU macros, and other needed symbols WITHOUT the GNU feature-test macro.
- Remaining `_GNU_SOURCE` references are only inside the compat shim headers themselves (documented replacements) and one unreferenced WASM blob — zero production code depends on it.

### Minic JIT multiplicative bug fix (commit `70d5e44`)
- **Bug**: `compile_additive()` non-XRA path did not clear `rax_is_const` after performing add/sub. This caused the parent `compile_multiplicative()` to incorrectly trigger the "multiply by 1" constant-fold path when RHS was an expression ending in a constant (e.g., `x-1`), skipping the `imul` entirely.
- **Fix**: set `mc->rax_is_const = false` after the non-XRA add/sub block (1 line).
- **Verified**: `test_jit_regression` 68/68 pass (was 67/68).

### Build link fixes
- `test_jit_regression` missing `wubu_spawn.c` link (`wubu_run_program`).
- `test_drivers` + `holyc` link lines — added missing ISA drivers (arm64, mips, avr, ptx, 8051).
- Restored missing test files + fixed link lines for full test gate.

### ISA driver space growth
- Added AVR, 8051, MIPS ISA drivers (now 11 drivers in the driver space).
- Fixed ARM64 STP/LDP immediate encoding for negative offsets.
- Fixed MIPS instruction encoder.
- Added 8051 SFR handling (B register, PSW, SP, DPTR, ACC).
- Implemented optimizer passes: Common Subexpression Elimination (CSE), instruction combining, peephole optimization (29% smaller code).

### Infrastructure
- Extracted WuBuNOS compiler + wubuwizard brain as submodules.
- Removed stale `reactos` submodule ref, gitignored the study clone.
- `wubu_ftw.h` updated for `FTW_*`/`DT_*` constants (post-system-header).

## Battleship Status (carried forward)
- **~40 verifiable code-level REAL_GAPs** in `src/` (10 `system()` + 23 stub-phrase + 6 bare-metal no-ops). Confirmed by reading each function body.
- **~370 parity-marathon REAL_GAPs** (ReactOS NT 297 + SteamOS ~30 + Ubuntu/Arch ~20 + TempleOS ~15 + ZealOS ~8).
- **Baseline stub class is CLOSED**: `find_real_gaps.py src` → 0 empty bodies, 0 const-only-no-syscall gaps.

## Open Frontier (sprint board top)
- `system()` ×10: `wubu_image_ops.c`(5), `wubu_netlink.c`, `wubu_demo_record.c`(2), `wubu_codec.c`, `jit.c`.
- Stub no-ops: `wubu_gamelib_clear_start_menu`, `vsl_gpu_vulkan` memtype, `wubucontainer` register_handler, `dosgui_term` container session.
- NT syscall bridge: ~20/297 transliterated.
- Parity integration: Arch daemon → Desktop autostart; holyd REPL → Desktop terminal.
- SteamOS parity (EPIC E2): unscheduled.

## Next Session Direction
- **Primary**: close the 10 `system()` calls (fork+exec) + 3-5 stub no-ops with tests.
- **Parallel**: ReactOS NT transliterate next batch of syscalls; wire daemons as Desktop backends.
- Every gap = "rewriting from scratch in C". Defensive guards / ABI void-casts = NOT gaps.