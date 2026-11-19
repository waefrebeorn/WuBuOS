# Hardcoding census — the org-wide frontier

Measured 2026-11-19 by `tools/hardcode_census.py` (first-party files only;
submodules and vendored trees excluded — they are measured in their own
checkouts). Categories: duplicate `#define`s (CONFLICT = different values,
mirror = same value in 2+ files), filesystem path literals, `getenv` knobs,
literal-size allocations, literal sleeps. The front door for owners is
`docs/CONSTANTS.md`; the batch protocol is in
`docs/compendium/04-roadmap/dehardcode-workflow.md`.

## Org frontier (2026-11-19, corrected classifier)

The first census run counted trailing comments as part of a `#define`
value, so identical values with different comments read as CONFLICTS.
Fixed (values compared comment-free); these are the honest numbers:

| repo | files | dup CONFLICTS | dup mirrors | paths | env | alloc | sleep |
|---|---|---|---|---|---|---|---|
| wubuos (body) | 2276 | 30 | 154 | 3240 | 95 | 180 | 23 |
| wubunos (compiler) | 254 | 4 | 81 | 71 | 58 | 54 | 8 |
| wubuwizard (brain) | 1608 | 56 | 38 | 485 | 218 | 193 | 1 |
| wubucompositor | 12 | 0 | 1 | 3 | 3 | 0 | 0 |
| wubulens | 23 | 0 | 0 | 20 | 8 | 2 | 1 |
| W-Code | 10 | 0 | 0 | 15 | 1 | 4 | 0 |
| WuBuKernel | 8 | 0 | 0 | 0 | 0 | 0 | 0 |

Work order: CONFLICTS first (one side is usually a bug), then mirrors
(dedup to the owner), then env-knob registry + revolver clamping, then path
owners, then allocation/sleep naming. Each repo is edited in its OWN
checkout with its OWN gates — never across the submodule boundary.

## Fixed in the body this session (9 true conflicts + 6 mirrors single-owned)

- **Mach-O magics** — the kernel loader's private `MH_MAGIC_64` was
  `0xFEEDFC02` (WRONG: it rejected every valid 64-bit Mach-O) while VSL had
  the right `0xFEEDFACF`. One owner now: `include/wubu_macho_fmt.h`, included
  by `src/kernel/wubu_macho.c` and `src/runtime/vsl/vsl_macho.h`. New gate
  `make test_kernel_macho` drives real headers incl. the wrong magic as a
  must-reject regression. Gates: test_kernel_macho ALL PASS, test_vsl_macho
  24/24.
- **DESK_ICON_COUNT 8 vs 9** — `src/apps/apps.h` (registry owner) said 8
  while `src/gui/shell/desktop.h` re-defined the enum with DOOM=8, COUNT=9:
  dynamic icons collided with DOOM. apps.h now owns DOOM + COUNT 9;
  desktop.h includes it. The `DESK_ICON_EXPLORTER` typo was killed (renamed
  both sides to EXPLORER). Gate: test_apps 34/34, test_wm 55/55.
- **LAPIC registers** — `src/kernel/wubu_smp.c` re-defined LAPIC_ID/ICR
  offsets owned by `src/kernel/interrupt_apic.h`; now includes the owner.
  Gate: test_kernel_bridge all pass.
- **FTW_* flags** — `src/runtime/wubu_snapshot_internal.h` pre-defined a
  private FTW numbering BEFORE the system `<ftw.h>` / `wubu_ftw.h`
  (a POST-system-header fill-in by design), shadowing the real flags.
  Block removed. Gate: test_snapshot 133/133.

## Body batch 2 (2026-11-19): guards, NVMe, PCI — conflicts 55 -> 40

- **Include-guard collisions (8 pairs)** — two different headers sharing
  one guard macro: whichever is included second is SILENTLY SKIPPED.
  Renamed layer-qualified: kernel/audio/bus/codec/display/bonzi headers
  got WUBU_KERNEL_* / WUBU_RUNTIME_* / WUBU_AUDIO_SUBSYS_H etc.; app
  facade vs impl headers (calc, control) got WUBU_APP_* / WUBU_*_IMPL_H;
  bear's math header got WUBU_BEAR_MATH_H. Worst overload:
  `WUBU_CONTROL_H` was BOTH apps/control.h's guard AND a 32px metric in
  wubu_tokens.h — the metric is now WUBU_CONTROL_HEIGHT. (The brain
  submodule carries its own copies of some guards; that is the brain
  leg's work, not editable from the body.)
- **NVMe registers** — firmware and kernel kept private tables; one
  owner now: `include/wubu_nvme_regs.h` (10 registers, spec offsets).
  Gate: `make firmware` builds wubufw.fd OK; test_kernel_bridge all pass.
- **PCI IDs** — five drivers kept private subsets and two names were
  overloaded: PCI_VENDOR_AMD meant 0x1022 (chipset) in accel but 0x1002
  (ATI graphics) in audio; PCI_VENDOR_BROADCOM meant 0x14E4 in net but
  0x1000 (LSI MegaRAID) in raid. One owner: `include/wubu_pci_ids.h`;
  audio uses PCI_VENDOR_AMD_ATI, raid uses PCI_VENDOR_LSI, codec's
  base-byte match is PCI_CLASS_AUDIO_BASE. Gates: hosted green,
  test_audio all pass, test_apps 34/34.
- **Census classifier** now normalizes numeric spelling (0x30 == 0x0030,
  1u == 1, 3.14f == 3.14) — the NVMe family were mirrors all along.

Remaining body conflicts (40) triaged: math-constant family (WUBU_PI*
defined as literals in wubu_math.c AND aliased in wubu_math.h; M_PI in
bear/filter/std — next batch), STYXFS_MAX_OPEN_FILES 128 vs 256 (real,
read before fixing), feature-test macros (_GNU_SOURCE etc. — per-TU
ladders, benign), test-local caps (bear tests — benign unless a shared
harness emerges), FALSE/TRUE (layer-local UEFI vs parity, accepted).

## Body batch 3 (2026-11-19): math owner + STYXFS — conflicts 40 -> 30

- **Math constants**: wubu_math.c kept a private literal table while
  wubu_math.h owned WUBU_M_* (+ conditional aliases). The .c now includes
  the header and uses WUBU_M_* directly; only implementation-local
  NAN/INF/hex-float constants remain in the .c. bear/wubu_math.h's WUBU_PI
  aliases libm's M_PI (it already includes <math.h>). wubu_filter.c's
  private M_PI fallback replaced by the kernel math owner.
- **STYXFS_MAX_OPEN_FILES**: styxfs.h's 128 sizes the PROTOCOL struct;
  styxfs_server.c re-defined the same name to 256 for its HOST-side table
  (different type, different concern, no shared TU). Renamed the server's
  to STYXFS_HOST_MAX_OPEN_FILES — both named, no overload.
- Gates: test_math PASS, test_styxfs 18/18, make bear built, hosted green,
  test_standards clean, test_docs 581/0.

The remaining 30 body conflicts are ACCEPTED triage classes, not bugs:
feature-test macro ladders (_GNU_SOURCE/_POSIX_C_SOURCE etc., per-TU by
design), guarded M_PI fallbacks in wubu_std.h/bear (POST-system-header
pattern), file-local test caps and paths (MAX_ENVS/MAX_STEPS/H/W/NSROOT/
ROOT/OUT_DIR — never co-included), FALSE/TRUE (UEFI spec vs kernel
parity, layer-local), BEAR_HAVE_CUDA/CUDNN (#ifdef ladders), MAX_FILES
(firmware fsproto vs fwcfg — different subsystems, generic name).
Renaming these for grep-ability is optional hygiene, not correctness.
The body's REAL duplicate-owner conflicts are closed.

## Sibling leg 1: wubunos (compiler) — pushed a3cf7c5 (wubu-integration)

The PIC encoder (`wubu_isa_pic.c`) and interpreter (`wubu_pic_interp.c`)
kept PRIVATE opcode tables that had drifted — three real codegen bugs:
- `MIR_EQ` emitted 0x16 where the executor runs GTS: equality compiled
  to signed-greater-than. EQ is 0x18.
- `MIR_LT`/`MIR_ULT` emitted 0x15 where the executor runs LEU (<=):
  less-than compiled to less-or-equal.
- `MIR_GT` (signed) emitted GTU (unsigned): GT(-5,3) was true.

Fix: `wubu_pic_opcodes.h` is the ONE owner — the executor's numbering is
the contract, the encoder conforms; both private tables deleted.
Encoder: GT→GTS, LT→GTS with operands swapped (a<b ⟺ b>a), ULT→LEQ.
Gate: `test_isa_driver` gained a 16-case comparison battery across all
13 ISAs (EQ/LT had ZERO coverage — that is why the drift shipped).
Verified 356/392: pic passes all 16; the 22 pre-existing failures are
unchanged. The battery newly exposes z80 signed lt/gt (4 cases), mips eq
(1), ptx (9) — next backend work items, recorded not hidden.
wubunos census after: 4 conflicts / 81 mirrors (remaining conflicts:
CC_LE/CC_NE layer-local HolyD-vs-m68k names, OP_RETURN wasm-vs-spirv,
MAX_TESTS test-local caps — triage before touching).

## Sibling leg 2: wubucompositor — pushed 08579e6 (main)

- The default guest command was the absolute literal
  `/home/wubu/wubuos/src/hosted/wubu` in TWO files — the compositor only
  worked on one machine's layout. New leaf `src/wcc_spawn.{c,h}` owns
  guest launch: the env knobs + a default derived from `/proc/self/exe`
  (`<exe_dir>/../wubuos/src/hosted/wubu`); the old path survives only as
  a named build-overridable last-resort fallback. Gate: `make test-spawn`
  (headless) — derived default executable, NO_SPAWN=1 forks nothing,
  START_CMD runs a real child to exit 0, NO_SPAWN=0 still spawns. ALL PASS.
- `src/main.c` is the DEAD wlroots-era entry point (the Makefile builds
  only the slerm) — banner-marked LEGACY, removal is an owner decision.
- The WL_PRIVATE "conflict" was generated wayland-scanner code; the
  census now skips generated files, and `calloc(1, sizeof T)` is no
  longer counted as a literal allocation. Compositor census: 0 conflicts.
- Live nested run NOT performed (no DISPLAY in the agent session);
  `wcc_test.sh` remains the live gate on a desktop session.
- NEXT compositor leg: dissolve wubucompositor.c per the no-monolith law.
  PROGRESS: step 0 (wcc_spawn) DONE; step 1 (wcc_internal.h) DONE;
  step 2 (wcc_gpu.c, first host-bound leaf) DONE — pushed 5ded1bc,
  monolith 1186 -> 1046 lines, build + test-spawn green. Remaining map:
  1. `wcc_internal.h` — move struct wcc_server / wcc_surface /
     wcc_output_ctx + WC_OUTPUT_W/H out of the .c (shared by leaves,
     opaque to the outside). Mechanical first step; build green.
  2. `wcc_gpu.c` — gpu_destroy/setup/present (~229-287): GLX visual,
     texture upload, fullscreen quad. Self-contained hardware concern.
  3. `wcc_present.c` — blit + XShm fallback (~288-334);
     `wcc_capture.c` — wcc_capture_screenshot PPM (~335-367).
  4. `wcc_seat.c` — seat_mod_key/send_modifiers + pointer/keyboard/
     touch impls + seat_bind (~170-192, 553-661). The wl_fixed_t and
     modifiers rules from the skill live here.
  5. `wcc_surface.c` — surface_* + commit + region + destroy listeners
     (~194-227, 368-486); `wcc_globals.c` — ownership, refused no-ops,
     compositor_bind (~488-551).
  6. `wcc_output.c` — per-monitor wl_output globals (~663-707);
     enumeration already lives in wcc_monitors.c.
  7. `wcc_xdg.c` — wm_base/positioner/xdg_surface/toplevel (~709-900).
     Read the GENERATED signatures for every request (skill pitfall).
  8. `wcc_x11.c` + a fresh entry `main.c` — X event dispatch, input
     forwarding, init/loop (~900-1186). The legacy wlroots main.c is
     removed or renamed at this step (owner decision pending).
  Each step: make green + make test-spawn; wcc_test.sh live on a
  desktop session at the end. The spawn leaf (wcc_spawn.c) is already
  extracted — step 0 done.

## Scope: wubuwizard is its own mission (owner directive 2026-11-19)

The brain is the AGI portion of the OS and the KV cache IS its
filesystem — it is NOT part of this OS de-hardcoding workstream. Its
census numbers above are informational; edits there happen under the
brain's own missions (wubuwizard-kvfs-modules, wubu-agi-training-pipeline),
not this ledger.

## The philosophy this serves (owner directive 2026-11-19)

WuBuOS is the **WSL replacement**: develop in Linux, run in macOS.
The guest desktop is a pure C11 Wayland client (host-agnostic); the
compositor is the HOST SEAM — X11 today, Quartz/Metal tomorrow, protocol
logic unchanged. Fixes made toward that truth:
- compositor keymap transport: Linux-only memfd_create -> POSIX shm_open;
- executable-path derivation portable in BOTH the compositor spawn leaf
  and the guest's buddy launcher (hosted.c): /proc/self/exe on Linux,
  _NSGetExecutablePath on macOS;
- host-bound code being isolated into leaves (wcc_gpu.c first) so a
  second backend is a drop-in, not a rewrite.
Open legs, stated plainly: the Quartz/Metal host backend (cannot build
or verify on this Linux box), full Mach-O process execution (the shader
leg IS proven via metal2vulkan; darling is a dead end), and the live
nested gate (needs a desktop session). The compositor README carries
the full portability map.

## Triage classes (do not "fix" blindly)

- **Layer-local spec constants**: `FALSE` in `src/firmware/efi.h` (UEFI
  spec) vs `src/kernel/zealos_parity.h` (parity) — both correct in their own
  layer; a conflict only if co-included. Accepted, documented.
- **Conditional-compilation fallbacks**: `BEAR_HAVE_CUDA` defined 1 then 0
  in `src/bear/bear_cudnn.c` is an `#ifdef` ladder, not two owners. The
  census reports it; triage marks it benign.
- **Test-local caps**: bear test files define their own MAX_ENVS/MAX_STEPS
  per experiment — legitimate unless two tests share a harness; merge only
  then.
- **Math constants** (M_PI family): owner is `src/kernel/wubu_math.h`
  (WUBU_M_* aliases); bear/test re-definitions are the next dedup batch.
- **Path literals**: `/n/*` is the OS's own namespace API (legitimate
  contract, not hardcoding); `/dev`, `/sys`, `/proc` probes are hosted-only
  hardware discovery (candidates for a probe-path owner); `/tmp` scratch
  and `~/.config` paths belong in settings/path owners. Classify before
  moving — the count is a frontier, not a to-do list of 3000 edits.

## Ratchet

```sh
python3 tools/hardcode_census.py --write-baseline tools/hardcode_census_baseline.json
python3 tools/hardcode_census.py --baseline tools/hardcode_census_baseline.json
```

Regenerate only for keys that went DOWN. Sibling repos get their own
baseline file in their own checkout when their first batch lands.