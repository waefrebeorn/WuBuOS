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
| wubuos (body) | 2274 | 55 | 153 | 3230 | 95 | 180 | 23 |
| wubunos (compiler) | 254 | 4 | 81 | 71 | 58 | 54 | 8 |
| wubuwizard (brain) | 1608 | 56 | 38 | 485 | 218 | 193 | 1 |
| wubucompositor | 9 | 1 | 0 | 2 | 3 | 2 | 0 |
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