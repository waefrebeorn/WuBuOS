# Grunt-work tier list — bulk fixes for a fresh model

Purpose: a laundry list of real, evidenced work items, cheapest first, so a new
model can grind through many fixes per batch without re-deriving context.

Standing phrasing for every item: **keep working, do not stop at the plan.**
Take the next concrete step immediately; finish several items before committing;
run the relevant gate and read the real output after every batch; commit with an
honest message naming what was actually verified.

Do these in order. Do not skip the verification lines. Do not mark something done
because it compiles — only when the target runs and the output says so.

---

## Tier 0 — small, self-contained, safe (do a dozen per batch)

0.1 **Wire or classify `src/runtime/wubu_realm.c`.** It compiles but no gate runs
     it. Probe it the way the bear drivers were probed (`-I. -Iinclude`), then
     either add a target that exercises its contract or record why it is
     unreachable. Evidence goes in `docs/compendium/04-roadmap/orphans.md`.
     *Done when:* a target runs it, or the doc says exactly why not, with proof.

0.2 **Re-run the orphan audit with correct flags.** Rebuild the probe with
     `-I. -Iinclude` + family dirs and refresh every row of
     `docs/compendium/04-roadmap/orphans.md`. My earlier pass produced a false
     STALE verdict because of missing `-I.` — assume any old verdict is suspect.
     *Done when:* every row cites a command and its real output.

0.3 **Finish the bear driver sweep (15 left).** For each: try linking against
     the hosted core set (exclude CUDA/cuDNN/Vulkan, `*_test.c`, `bear_train.c`),
     run it, and either wire it into `make test_bear_drivers` or banner-mark it
     with the exact missing symbol (e.g. `bear_mujoco_init`, `bear_policy_load`).
     *Done when:* every driver is either green in the gate or carries a banner
     naming the missing API.

0.4 **Migrate wuoffice's spacing ladder.** `src/apps/wubuoffice/apps/wubuos/
     wuos_theme.h` + `src/wubulayout/layout.c` (~38 findings) use their own
     off-grid literals. Decide the owner (vendor the metric header into the
     submodule, or give the submodule its own on-grid token header consumed by
     both), then migrate. Change it inside the submodule repo and bump the
     gitlink — never edit a submodule from the parent tree alone.
     *Done when:* the submodule builds on its own and its spacing is on the 4px
     grid, with the gate to prove it.

0.5 **Move METAL-ONLY orphans to the metal gate.** `interrupt_pic_test.c` faults
     hosted (PIC ports); `wubu_flush_selftest.c` needs flush/fsync;
     `test_bootattach.c` needs fat32 boot attach; `wubu_elf_test.c` needs the ELF
     loader plus a demo asset and repo-root cwd; `wubufx_test.c` cascades into
     `src/runtime/edr/*` + `holyd_*`. Give each the module set it actually needs,
     under `-DMYSEED_METAL -DWUBU_BAREMETAL=1` where appropriate.
     *Done when:* each runs in the right gate or is documented as subsystem-sized
     with the cascade spelled out.

0.6 **Stress-proof the supervisor fix.** Crash-loop backoff now uses
     `CLOCK_MONOTONIC` ms and passes 6/6 concurrent runs. Add that concurrency
     check to the gate so the flake class cannot return silently.
     *Done when:* the gate runs N instances concurrently and fails on regression.

0.7 **Hunt other timing-sensitive gates.** Run `make test_svc_super`-style
     concurrency probes over the medium tier; any test whose assertion depends on
     wall-clock sleeps gets its clock source fixed, not its assertion widened.
     *Done when:* two consecutive full `-j24` gates pass with no `[FAIL]`.

0.8 **Re-scan for off-grid layout literals.** Fresh scan over `src/gui/**` for
     spacing `#define`s and bare layout numbers outside tests/stubs. Migrate the
     real tokens; leave glyph/cell sizes and physics offsets with a one-line
     comment saying why. Do not trust the old audit JSON — its line numbers have
     drifted.
     *Done when:* the scan prints zero unexplained off-grid spacing values.

0.9 **Commit a golden-frame GUI regression.** Render `src/hosted/wubu -h
     --screenshot` headless and assert geometry (taskbar height, caption height,
     button rects, margins are multiples of 4) from the metric owner, not pixel
     diffs. *Done when:* a deliberately wrong metric fails this gate.

0.10 **Verify the zombie reaper still works.** `tools/dev/reap_stale_tests.sh`
      must find zero stale hung test binaries after a gate run, and must be run
      automatically (hook or reminder) so hung binaries holding X connections do
      not accumulate. *Done when:* a post-gate run reports 0 stale processes.

0.11 **Docs-path hygiene after each batch.** Any file you touch that appears in a
      doc must still exist and mean the same thing; `make test_docs` is the
      referee. New files you add become rows where relevant.
      *Done when:* `make test_docs` stays at 0 failed across the whole tier.

---

## Tier 1 — real modules, roughly an hour to half a day each

1.1 **WuBuDriver probe driver.** Extend WuBuKernel's driver model with a real
     probe/registration test that asserts bind failure paths, not just success.
     Wire it into `make test` there and re-bump the gitlink.
     *Done when:* the kernel repo's own gate covers failure modes and
     `test_kernel_bridge` passes on both sides.

1.2 **PMM hardening.** Add fragmentation and region-edge oracles to
     `src/wubukernel_pmm_test.c`: allocation that spans a region boundary,
     free order permutation, exhaustion at exact frame counts. Keep the
     `-ffreestanding -nostdinc` proof green.
     *Done when:* every oracle passes in both the hosted and freestanding builds.

1.3 **OOBE walkthrough end-to-end.** Drive the greeter headless, screenshot each
     step, and verify margins/controls on the metric grid. Fix what the images
     show, not what the code says. *Done when:* a full first-run sequence renders
     correctly and the fixes consume the metric owner.

1.4 **Metric drift gate expansion.** Add assertions that tokens, wm chrome,
     explorer chrome and shell surfaces agree with `wubu_gui_metrics.h`, so a
     future off-grid literal fails the build. *Done when:* a planted off-grid
     literal is rejected.

1.5 **Deduplication sweep.** Grep for repeated helper bodies (two copies of the
     same loop/table/marshaller) and collapse them onto one owner with thin name
     shims. The supervisor already had two copies of the backoff schedule —
     assume there are more. *Done when:* each collapse is committed separately
     with its gate green.

1.6 **VSL personality tiers.** `test_vsl_cpm` / `test_vsl_macclassic` are green;
     add syscall-coverage measurements (counts of implemented vs stubbed) so the
     NT/Linux/macOS subset has a number that must go up. *Done when:* the number
     is printed by the gate and rises with each batch.

---

## Tier 2 — hours to days, needs design decisions

2.1 **Linux window inside WuBuCompositor.** Today only `src/hosted/wubu-weston.sh`
     (weston x11 backend + xwayland) provides a Linux app beside the WuBuOS
     surface. Build the X-server side into WuBuCompositor itself so a Linux X11
     app gets its own window — the "installable WSL" leg.
     *Done when:* an X11 app runs in its own window next to the WuBuOS desktop,
     verified by screenshot.

2.2 **wuoffice visual parity.** Migrate the submodule's theme/layout to the same
     4px grid and Win11-faithful chrome, on its own commit, then bump the gitlink
     and re-render both apps. *Done when:* office screenshots are on-grid.

2.3 **Buddy animation rig.** Use the reference sheets under
     `tools/assets/wubu_buddy_deck/refs/` as anatomy targets, bake new states into
     `src/gui/wubu_buddy_states.h` **by regenerating**, and wire emotion
     transitions to sensor inputs. No image textures — SDF/shader only.
     *Done when:* the orb visibly changes with system state in a captured frame.

2.4 **Slerm POSIX coverage.** Grow the libc-free layer (`wubu_posix`) with the
     syscalls WuBuTIME and the VSL path actually need; keep the
     `-ffreestanding -nostdinc` gate green and add vectors per syscall.
     *Done when:* each added syscall has a behaviour test.

2.5 **Metal gate for deep-dependency orphans.** Give `wubufx_test` its EDR + holyd
     module set, or split the dependency so a small part is testable hosted.
     *Done when:* it runs somewhere real.

2.6 **Registry and provenance sweep.** Verify `Waefrebeorn/WuBuFederation/REPOS.md`
     claims against reality (repo count, names, branches) and correct
     `PROVENANCE.md` if any claim is wrong. *Done when:* every listed repo exists
     and the count matches.

2.7 **Full-gate reliability.** Two consecutive `-j24` full gates green, plus a
     concurrency stress on the medium tier. Fix root causes, not symptoms.
     *Done when:* the log shows no `[FAIL]` and no segfaults across both runs.

---

## Tier 3 — multi-day legs, start only after Tiers 0–1 are clean

3.1 **Longhorn-derived GUI/OOBE baseline.** Beyond chrome: window templates,
     control library, theme system, all consuming the metric owner.
3.2 **NT syscall subset completion**, measured against the ReactOS transliteration
     tables already in the tree; raise the implemented/stubbed number.
3.3 **SteamOS 3.10 UX parity judge.** A repeatable harness that compares rendered
     surfaces against the reference for spacing, type ramp and hit targets.
3.4 **Swarm legs in wubuwizard.** Model fetch/serve over the peer ledger, gated.
3.5 **Self-hosted compiler/JIT closed loop** — HolyC → ISA backend → execution,
     with the 16 backends' consistency checked by the existing gauntlet.

---

## Rules for working this list

- Batch Tier 0 items; commit per logical change; never mix a Tier 2 refactor
  with a Tier 0 fix.
- Never report an item complete from reading code — run the target and quote it.
- If you find a better ordering, take it, but say so in the commit.
- When a claim in this file turns out false, correct the file; it is a living
  document, not a monument.
