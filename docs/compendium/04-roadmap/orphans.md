# Orphan modules: measured classification

Every .c under src/ that no Makefile or header references, classified by ACTUAL
compile/link/run probes (`gcc -fsyntax-only`, then full link + execution), not by
reading. Kept under test_docs so the table cannot drift.

| file | verdict (evidence-based) | audit note |
|---|---|---|
| `src/bear/tests/bear_train_chained.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/cartpole8_verify.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/record_solved.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/test_cartpole_env.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/test_chained_reward.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/test_env_reward.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/test_gaad_update.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/test_holo_opt.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/test_knockover_reset.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/test_knockover_standalone.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/test_knockover_train.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/test_mingru.c` | WIRED -- make test_bear_drivers (links + runs, rc=0) | was audit probe artifact |
| `src/bear/tests/test_physics.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/test_policy_forward.c` | WIRED -- make test_bear_drivers (links + runs, rc=0) | was audit probe artifact |
| `src/bear/tests/verify_cartpole8.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/tests/verify_gaad_bug.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/train/curriculum_train.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/train/train_cartpole_gaad.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/train/train_cartpole_gaad_v2.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/train/train_cartpole_gaad_video.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/train/train_curriculum.c` | COMPILES with repo-root include (-I.); real driver, no gate target | audit probe lacked -I., which caused the false STALE call |
| `src/bear/train/train_gaad_cartpole.c` | RETIRED -- includes headers that no longer exist; banner added | audit probe lacked -I., which caused the false STALE call |
| `src/kernel/test/legacy/interrupt_pic_test.c` | METAL-ONLY -- builds hosted but faults; needs the bare-metal gate | legacy kernel test; no build rule |
| `src/kernel/test/legacy/test_ahciseq.c` | WIRED -- make test_kernel_ahciseq | legacy kernel test; no build rule |
| `src/kernel/test/legacy/test_bootattach.c` | DEEP-DEPS -- needs kernel module set (fat32/elf/flush) | legacy kernel test; no build rule |
| `src/kernel/test/legacy/wubu_elf_test.c` | DEEP-DEPS -- needs kernel module set (fat32/elf/flush) | legacy kernel test; no build rule |
| `src/kernel/test/wubu_flush2_selftest.c` | DEEP-DEPS -- builds standalone, needs its module family linked | selftest with no build rule |
| `src/kernel/test/wubu_flush_selftest.c` | DEEP-DEPS -- needs kernel module set (fat32/elf/flush) | selftest with no build rule |
| `src/firmware/tools/test_fw_rsa.c` | DEEP-DEPS -- builds standalone, needs its module family linked | no build rule |
| `src/framework/wubufx_apps_test.c` | DEEP-DEPS -- builds standalone, needs its module family linked | docs-only reference (TOPOLOGY.md) |
| `src/framework/wubufx_test.c` | DEEP-DEPS -- builds standalone, needs its module family linked | docs-only reference (TOPOLOGY.md) |
| `src/gui/compositor/wubu_compositor_standalone.c` | DEEP-DEPS -- builds standalone, needs its module family linked | docs-only reference; no target builds it |
| `src/gui/daemon_panel_test.c` | DEEP-DEPS -- builds standalone, needs its module family linked | docs-only reference |
| `src/gui/standalone_hosted_shim.c` | DEEP-DEPS -- builds standalone, needs its module family linked | docs-only reference |
| `src/gui/xdg-shell-client-protocol.c` | DEEP-DEPS -- builds standalone, needs its module family linked | generated protocol; header is included, the .c never is |
| `src/jit/jit_audit.c` | DEEP-DEPS -- builds standalone, needs its module family linked | no build rule |
| `src/jit/jit_gap_analysis.c` | DEEP-DEPS -- builds standalone, needs its module family linked | no build rule |
| `src/jit/test_album_benchmark.c` | DEEP-DEPS -- builds standalone, needs its module family linked | no build rule |
| `src/jit/test_cross_exec.c` | DEEP-DEPS -- builds standalone, needs its module family linked | no build rule |
| `src/runtime/wibu_mips_interp.c` | DEEP-DEPS -- builds standalone, needs its module family linked | typo-named sibling of wubu_mips_interp.c; no build rule |
| `src/runtime/wubu_archd_test.c` | DEEP-DEPS -- builds standalone, needs its module family linked | no build rule |
| `src/runtime/wubu_realm.c` | DEEP-DEPS -- builds standalone, needs its module family linked | implemented but never linked; declared in wubu_realm.h only |
| `src/runtime/wubu_system.c` | WIRED -- make test_uuid / test_system | implemented but never linked; declared in wubu_system.h only |
| `src/runtime/wubu_system_test.c` | WIRED -- make test_uuid / test_system | no build rule |
| `src/runtime/wubu_uuid_test.c` | WIRED -- make test_uuid / test_system | no build rule |
| `src/tools/demo_capture.c` | DEEP-DEPS -- builds standalone, needs its module family linked | no build rule |
| `src/tools/desktop_render_test.c` | DEEP-DEPS -- builds standalone, needs its module family linked | no build rule |
| `src/apps/calc/calc_test_support.c` | DEEP-DEPS -- builds standalone, needs its module family linked | docs-only reference |
| `src/apps/canvas_standalone.c` | DEEP-DEPS -- builds standalone, needs its module family linked | docs-only reference |

## What this means

- WIRED: had a target, now in `test_critical_runtime`.
- METAL-ONLY: cannot run hosted; belongs in the AGI-metal gate, not `make test`.
- DEEP-DEPS: implemented and compilable, but pulling in its module family is real
  work -- queue it per subsystem instead of forcing a hosted link.
- BEAR DRIVERS: an earlier pass called all 18 `src/bear/{tests,train}/*` files
  STALE because the probe lacked `-I.`; with the repo root on the include path
  17 of them compile cleanly (they are real training/eval drivers that simply
  have no gate target). `train/train_gaed_cartpole.c` is genuinely dead -- it
  includes headers that no longer exist; it carries a RETIRED banner. The
  lesson: classify with the same include flags the build actually uses.
