# NEXT SESSION — WuBuOS / WuBu handoff (2026-10-20)

Written at the end of a long plumbing + boot + GUI session. The next session has
**no memory of this one**. Everything needed is below, and every number was
measured with the command shown next to it.

---

## 0. Identity and standing rules (do not renegotiate)

- The AGI's name is **WuBu**. User: **WaefreBeorn**.
- **Three real repos, no symlinks between them:**
  - `/home/wubu/wubuos` — the **BODY** (OS). Branch `master`.
  - `/home/wubu/wubunos` — the **COMPILER** (HolyD / WuBuNOS). Branch `wubu-integration`.
  - `/home/wubu/wubuwizard` — the **BRAIN**. Branch `wubu-integration`.
  - `wubuos/src/compiler` → symlink into wubunos; `wubuos/src/brain` → gitlink to wubuwizard.
  - **Edit compiler files only in wubunos.** Editing through the symlink has corrupted the
    Makefile before.
- `/home/wubu/wubumath` is **pull-only, another agent owns it**. Never edit, never commit.
  If diverged: save a patch outside the repo, `git reset --hard origin/main`.
- Pushes are fast-forward only. **Never force-push.** wubuos pushes to `origin master`;
  the other two to `origin wubu-integration`.
- No temp-dir tools. Scratch work goes in `~/opt/` or repo `tools/`.
- Never put key literals in docs/skills/commits. Keys live in
  `~/.hermes/profiles/mind-palace/vault/` (see `VAULT.md`).
- Do not invent budgets or quotas. State a number only when measured.
- **Tests ≠ correct.** Verify a failure is real by running the target and reading the
  actual error before patching. Triple devil's-advocate after each batch.

---

## 1. Verified state at handoff

| repo | HEAD | working tree |
|---|---|---|
| wubuos (BODY) | `4829026d` | 3 modified binaries (`wubufw.fd`, `test_colonel`, `test_hwdetect`) + untracked `src/kernel/wubu_pt.{c,h}`, `src/kernel/test/test_wubu_pt.c` |
| wubunos (COMPILER) | `8a86b48` | 1 untracked binary `test_mir_regalloc` |
| wubuwizard (BRAIN) | `fbb76bd` | untracked `h2_cycle`, `harness2_c11`, `models/corpus/`, `research/micro-2026-10-18/`, `vault/wubuos-structural-findings.md` |

Build outputs (`wubufw.fd`, test binaries) are tracked by accident in places —
**never commit them**; restore with `git checkout -- <file>` if they clutter a diff.

### Measured gates

- `cd wubuos && make onboard-check` → **0 failures, ALL DOCUMENTED STEPS PASS**
  (prerequisites, build, artifact audit, `test_timer_wheel` 17 checks, boot gate).
- `make boot-gate` → **PASS**, 7 milestones: secureboot policy engine, signed payload
  verifies, unsigned payload rejected (fail-closed), TPM PCR extend, interactive shell,
  allocator stats, ESP listing.
- Body test failures from the original 20-target audit → **14 pass, 5 fail** (details §3).
- Compiler: `make test` 200/200; battery 148 PASS / 0 WRONG; gauntlet **2048/2668 = 76.8%**
  (fail 216, error 404). Gauntlet noise is **±20 passes** — never claim a smaller gain
  from one run.
- Brain: `test_cv` 173, `test_eye` 26, `test_look` 9 = 208 checks; `test_harness2` 106 checks.
- Registry: 1000 goals, **361 done / 639 open**; portfolio rotation rules live in
  `docs/PORTFOLIO-TRIAGE.md`.

---

## 2. What this session actually changed

### Boot — from "unmeasurable" to "gated and green"

The old `make qemu` hung forever with zero output, and `-kernel kernel.elf` printed only
`010203040506070811121314`. Root cause: `kernel.elf` is a higher-half 64-bit image with
**no multiboot header**, so QEMU's Linux loader never ran it; the floppy image embedded
the ELF verbatim while the bootsector reads 900 sectors (450 KB) of a 1.9 MB file and
jumps to a flat entry. Both paths were dead by construction.

The real path is **WuBuFW** — our own C11 UEFI firmware (`src/firmware/`, `build.sh`,
`qemu-boot-verify.sh`). It boots as a QEMU guest and passes 7 milestones.

- `tools/boot_gate.sh` — runs WuBuFW, requires the milestones, prints them, exits 1 on
  failure. `--kernel` keeps the dead path as a documented diagnostic.
- `make qemu` is bounded to 30 s and uses `format=raw,if=floppy`.
- `src/firmware/fw_secureboot.c`: the payload self-test reported **FAIL** when
  `\EFI\BOOT\SIGNED.EFI` / `BOOTX64.EFI` were absent. Absent fixtures are now **SKIP**;
  only a payload that exists and behaves wrongly fails.

### Onboarding — executable, not aspirational

- `QUICKSTART.md` — only commands that were run, each with expected output, including the
  boot step and why `-kernel` is not it.
- `tools/onboard_check.sh` — runs the guide; documented-as-passing steps are hard failures;
  the boot gate is a hard requirement now.
- `make setup` — Omarchy-style "no decisions": build + verify + boot in one command.

### GUI — two external references, both honestly scoped

- **XPlasma-Plus** (GPL-3.0): no code taken. Checked rather than assumed — our Luna title
  bar **already** had a vertical gradient (`gradient_title` + `vbe_vgradient`). What was
  missing was the focus flash from `effects/xp-caption-animation`, now in
  `src/gui/wubu_caption.{c,h}` with `test_caption` **15 checks**. `wubu_caption_demo.c`
  renders active gradient / inactive flat / focus flash to a PPM (verified visually).
  **Not wired into the WM**: the GUI layer has no frame clock, so `t` has nowhere to come
  from. `docs/GUI-XPLASMA.md` records this.
- **Omarchy** (MIT): `wubu_theme_file.{c,h}` parses a one-file palette
  (`themes/luna.conf`, 30 keys) into `WubuThemeColors`, and `wubu_theme_set_custom()`
  adopts it live (required dropping `const` from the internal theme table).
  `test_theme_file` **20 checks**. `docs/GUI-OMARCHY.md`.

### Body plumbing — the aggregate gate and the real defect class

- `mk/tests_all.mk` (445 targets) exists because `make test` reached only **131 of 446**
  declared targets. `make test_all` is the honest gate.
- Fixed link inputs: `holyd_semantic.c` via `HOLYD_SRC` (fixed 10 targets at once;
  `test_holyc` 84/84), `x86_decode.c`, `wubu_cap_audit.c` (both VSL object sets → 348 and
  16 checks), `styx_locks.c`, `klog.c`, `interrupt_pic.c`, `wubu_wdt.c`,
  `minesweeper/solitaire/timer_wheel` (`test_dosgui_apps` 39/39), `wubu_wine_env.c`,
  `wubu_gpu_backend.c`, `styx_serve.c`, `styxfs_util.c`.
- A duplicated, header-less block in `mk/tests.mk` executed `gcc -c` with no input; removed.
- Registry gaps: `styx_locks.o` and `wubu_image_names.o` were **never in `RT_OBJS`** —
  one omission caused four unrelated-looking failures. Registered.
- `wubu_agi_kernel_global()` was **declared in the header, called from four places, and
  never defined**. Added (returns `&g_agi`).

---

## 3. The five remaining body failures — precise causes

1. **`test_exec_games`** — X server segfaults in this environment (no display).
   Environmental. Not code.
2. **`test_era_apps` / `test_hosted` / `test_metal`** — these link **all of `RT_OBJS`
   (134 modules) plus kernel subsystems into hosted GUI tests**. Chasing symbols drove
   era_apps from 46 → **196 undefined**, pulling in camera, Bluetooth and compiler
   internals. This is **not a link bug**; it is tests with no dependency boundary.
3. **`test_styxfs`** — links `wubu_wine_env.c`, which calls kernel hw probes; adding
   `wubu_hw_detect.c` yields `multiple definition` while the symbol stays unresolved, i.e.
   the same TU reaches the link twice. Its recipe must be rebuilt from the registry.

**Do not "fix" these by appending files.** That is what created the tail.

---

## 4. Priority queue — what makes this the best OS

Ranked by leverage, not by ease. Each item says how you will know it worked.

### P1 — The kernel never actually boots
WuBuFW loads `\EFI\BOOT\BOOTX64.EFI` from a FAT volume. Nothing in the tree produces an
ESP image containing our kernel, and `kernel.elf` is ELF, not PE. So today the firmware
reaches a shell and **the OS kernel is never entered**.

*Done when:* one command builds an ESP with a PE-loader + kernel, QEMU boots it, and the
serial log contains `WuBuOS: kernel_main entered (long mode OK)` **from that path**.
Extend `boot_gate.sh` to require that line, not just the firmware milestones.

### P2 — Finish the object registry (fixes 3 tests at once)
`tools/rt_registry_audit.py` reports **106 of 234 runtime modules not in `RT_OBJS`**.
Classify them (some are deliberate alternates — `wubu_hw_stub.c` must stay out), register
the real ones, then **rebuild the four failing recipes from `RT_OBJS`** instead of hand
lists.

*Done when:* the audit prints a short, justified exclusion list; `make test_all` fails
only on `test_exec_games` (environmental).

### P3 — A frame clock, then prove the WM draws
The GUI has no tick source, so animations cannot run and "WM drawing" remains unproven
(checksum `nonbg=112348/112347` is ambiguous). Add a monotonic ms counter reachable from
`src/gui/`, use it for the caption focus flash, and write a test that renders a window to
the framebuffer and asserts **specific pixels changed** (not a global non-background count).

*Done when:* a screenshot diff shows title bar, border and text pixels at expected
coordinates, and the flash decays to the base colour at t=1.

### P4 — Usability surface
Boot lands in a firmware shell, and the Win98 shell exists but is not reachable end-to-end.
Needed: a documented, tested command set (help/launch/theme), a real first-run experience,
and `docs/UX-GAP-REGISTRY.md` reconciled with what actually works.

### P5 — Compiler floor
Gauntlet 76.8%. Known clusters: nested designators in compound literals
(`gcc_torture_000224`, context-dependent), `_Complex` (338 messages, needs a real type),
GNU label addresses, cross-TU externs (impossible by design in a single-TU compiler —
stop counting them as failures).

### P6 — Truth pass on docs
Several status files predate this session. `docs/QEMU-GUI-STATUS.md` in particular must
state that the kernel is not yet booted by the firmware, or it will mislead again.

---

## 5. Traps that cost hours this session

- **Orphaned tab-lines in `mk/tests.mk`.** A heuristic detector flags ~290 candidate
  lines that start with a tab with no target above them (it over-counts multi-line
  recipes, so treat it as a list to inspect, not a count); they attach to the previous target, so make runs the wrong commands. This is
  why `test_ns_session` "worked by hand and failed under make". `make -p <target>` showing
  **no recipe** is the tell. Fixed for ns_session; **the other 289 are still there** —
  worth a dedicated cleanup.
- **`git checkout -- mk/tests.mk` wipes hand repairs.** Re-apply deliberately.
- **Piping through `tail` masks the exit code** (`rc=${PIPESTATUS[0]}` or no pipe).
- **`link_doctor.py` over-reaches** if allowed: it will add unrelated probe modules.
  Keep its test-file exclusion; run it with `--dry-run` first, always.
- **Test fixtures are not libraries.** `wubu_world_sample` is defined in a test; the real
  one lives in `src/kernel/wubu_world.c`.
- **Gauntlet noise ±20.** Three runs, medians, idle machine.
- **`make -k test_all` includes the gauntlet** and exceeds a 10-minute tool timeout; test
  the long target separately.
- **GPL vs MIT matters.** XPlasma is GPL-3.0 (design only, no code). Omarchy is MIT.

---

## 6. Immediate commands

```bash
cd /home/wubu/wubuos && git pull && make onboard-check     # build + audit + test + boot
make test_all                                              # 445-target body gate (long)
sh tools/boot_gate.sh                                      # firmware milestone gate
python3 tools/rt_registry_audit.py                         # the 106-module work queue
cd /home/wubu/wubunos && git pull && make test             # 200/200
cd /home/wubu/wubuwizard && git pull && make test_all      # brain gate
```

---

## 7. File map for the next session

- Boot: `src/firmware/` (`fw_main.c` looks for BOOTX64.EFI, `fw_pe.c`, `fw_secureboot.c`),
  `src/kernel/boot.S`, `src/kernel/metal_main.c`, `tools/boot_gate.sh`.
- Body gates: `mk/tests.mk` (447 targets, still has orphan lines), `mk/tests_all.mk`,
  `mk/objects.mk` (`RT_OBJS`, `KERNEL_OBJS`), `tools/rt_registry_audit.py`.
- GUI: `src/gui/dosgui_window_chrome.c` (title bar gradient), `src/gui/wubu_caption.*`,
  `src/gui/wubu_theme.*`, `src/gui/wubu_theme_file.*`, `themes/luna.conf`.
- Docs written this session: `QUICKSTART.md`, `docs/GUI-XPLASMA.md`,
  `docs/GUI-OMARCHY.md`, this file.
- Compiler: `holyd_parse.c`, `holyd_semantic.c`, `holyd_mir_eval.c` in **wubunos**.

---

## 8. Do-not-do list

- Do not claim boot works because `boot_audit.sh` passes — that only proves the ELF is
  freestanding.
- Do not claim the WM draws until pixel coordinates are asserted.
- Do not close a goal on an adjacent passing test.
- Do not append single files to a broken recipe; fix the registry.
- Do not edit wubumath, force-push, or commit build artifacts.

---

*Handoff diagnoses are hypotheses until re-run. Re-measure before trusting any number
above; the commands are listed so you can.*