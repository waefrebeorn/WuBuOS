# Handoff — new model getting oriented into WuBuOS

Welcome. You are picking up a large, living tree. This file is the shortest
honest path from "I just arrived" to "I am shipping verified work". Read
`docs/SITREP-2026-11-17.md` first for the shape of the system, and
`docs/MIND_PALACE.md` for the room-by-room walk.

## The standing instruction

Work autonomously and keep working. Take the next concrete step; do not stop
after writing a plan, a stub, or a single command. Keep going until you have
actually exercised the code and can show real output. If a goal is genuinely
complete, say so explicitly with evidence and stop; if you are blocked, say so
plainly and stop.

Vision bar (verbatim from the owner): *vision restored, make it like a pro,
better than claude, better than chatgpt, better than grok.*

## Laws you must not break

1. **No monolithic files.** One `.c` = one concern. Past ~400–500 lines or mixed
   responsibilities, dissolve into per-concern leaves (`_name.c` + public `.h`
   + `_internal.h`).
2. **Opaque structs, minimal includes, C11 only.** No god headers, no function
   bodies in headers.
3. **One owner per concern.** If a function exists, call it. Never write a second
   copy. Search the tree before writing anything that smells like a duplicate.
4. **Names mean something.** A bare first token names its layer, not "wubu".
   Rename surgically and re-verify both definition and callers.
5. **Verify by running targets.** Never by memory. Build green at every step
   before the next. Fix broken code first; turn stubs into behaviour gates.
6. **Docs are testable.** A doc that names a path which should exist and does
   not is a build failure (`make test_docs`). Stale is worse than short.
7. **Never disturb what you did not touch** — especially submodules: they stay at
   their own commits unless the task is about them.

## Build and gate

```sh
cd /home/wubu/wubuos
make hosted                       # -> src/hosted/wubu (GUI host; -h headless)
src/hosted/wubu -h --screenshot /tmp/f.ppm   # real frame, no Wayland needed
ffmpeg -y -i /tmp/f.ppm /tmp/f.png           # PPM rows are top-down already
make -k -j24 test                 # full gate; expect GATE_EXIT=0, 0 segfaults
make test_docs                    # doc/path gate, ~581 checks
make test_wm && make test_explorer && make test_startmenu && make test_greeter
make test_kernel_bridge && make test_agi_metal    # bridge + measured boot
```

Build layout: `Makefile` includes `mk/vars.mk`, `objects.mk`, `targets.mk`,
`pattern-rules.mk`, `tests.mk`. Shared runtime objects belong in `RT_OBJS`
(`mk/objects.mk`) so every consumer links them.

## Pitfalls that cost me real time (do not repeat)

- **Include flags matter for probes.** Several `src/bear/**` drivers include
  `"src/bear/foo.h"` (repo-root-relative). A syntax probe without `-I.` makes
  them look dead. I once classified 18 working drivers as STALE because of this.
  Always probe with `-I. -Iinclude` plus the family dir.
- **Two clones of WuBuKernel exist**: `/home/wubu/WuBuKernel` (working repo) and
  the submodule checkout `src/kernel/WuBuKernel`. Commit and push in the first,
  then `git -C src/kernel/WuBuKernel fetch && git reset --hard origin/main`, bump
  the parent gitlink, verify `make test_kernel_bridge`.
- **`-lm` goes after the sources** on the link line.
- **`bear_train.c` and `*_test.c` define their own `main`** — exclude them when
  linking a driver.
- **`glReadPixels` is bottom-up**; the PPM writer emits reversed rows.
- **XRandR/`wl_output` are HOSTED-ONLY.** Bare metal has no Linux/X11 host;
  display there is UEFI GOP + EDID over the GPU bus. Never put XRandR in the
  kernel path.
- **GPU:** RTX 5050 = sm_120; CUDA 12 toolkit has no native kernels (use
  compute_80 + driver JIT). PyTorch needs cu130/cu13x. NVIDIA ICD segfaults
  headless — use the Intel iGPU for Vulkan.
- **Timing-sensitive tests flake under `-j24`.** If a gate fails once and passes
  standalone, stress it (N concurrent runs) before calling it a flake; if it is a
  real timing bug, fix the clock source, not the assertion.
- **GitHub token**: `set -a; . ~/.hermes/profiles/mind-palace/secrets/github.env;
  set +a`. Never commit it or put it in `.gitmodules`.
- **This laptop is shared** — the owner's wife uses it. Do not leave flashing
  displays or blocking windows; prefer `--screenshot`/headless verification.

## Where to start working

`docs/compendium/04-roadmap/grunt-work-tiers.md` — tiered claims, cheapest first.
`docs/compendium/04-roadmap/gui-slerm-impossible-os.md` — the GUI slerm workflow
(owner directive 2026-11-19): pinned reference, license boundary, ownership map,
tiered legs with evidence gates. The GUI is the current priority workstream.
`docs/compendium/04-roadmap/dehardcode-workflow.md` — the de-hardcoding ledger
and ratchet protocol (owner directive 2026-11-19): one owner per constant,
measured by `make test_standards`. Every slermed surface is de-hardcoded as it
lands.
Tier 0 items are deliberately small and self-contained; finish several per batch
before committing, then run the relevant tier of the gate and read the real
output. Commit per logical change with an honest message naming what was verified.

## Environment facts worth knowing

- Host: Mint 22.3 x86_64, 24c/93GB, gcc 13.3. Repos at `/home/wubu/*`.
- Registry of the ~WuBu~ family: `Waefrebeorn/WuBuFederation/REPOS.md`.
  `PROVENANCE.md`: Claude/Cursor never contributed — any claim otherwise is an
  error to correct.
- Sensors: CPU = `x86_pkg_temp`, GPU = NVML, SSD = `nvme`; no RAM temp in sysfs.
- Background jobs from this session may still be running; check with
  `process_manage(action=list)` before assuming a port/display is free.
