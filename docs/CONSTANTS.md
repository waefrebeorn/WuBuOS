# CONSTANTS — where every number lives

The front door for the ~WuBu~ de-hardcoding effort (owner directive
2026-11-19: organize and remove all hardcoding into easy-to-find elements).
If you are about to write a constant, find its owner here first. If you are
looking for a constant, its owner is listed here. The rules behind this map
are in `docs/STANDARDS_AND_WORKFLOW.md` (R6 one owner, R7 geometry, R11
colours); the measurement is `make test_standards` plus
`tools/hardcode_census.py`; the work ledger is
`docs/compendium/04-roadmap/dehardcode-workflow.md` and
`docs/compendium/04-roadmap/hardcoding-census.md`.

## The decision tree

1. **User-tunable at runtime** (caps, timeouts, allocation bounds, thread
   counts) → the revolver: `src/runtime/wubu_revolver.h`. Init once behind a
   `rev_ready` guard, env override, clamp to a memory-safe ceiling, REFUSE
   garbage loudly. Arrays keep a compile-time extent; bounds checks use the
   runtime accessor. See the wubu-revolver-pattern skill.
2. **A persisted user choice** (theme, wallpaper, fonts, a11y) → the settings
   object `src/gui/wubu_settings.c`. Apply through the setter, THEN persist;
   never a second store.
3. **Visual geometry** (spacing, radii, sizes, chrome) →
   `include/wubu_gui_metrics.h` (canonical grid) + `src/gui/wubu_tokens.h`
   (aliases + named surface constants). Gate: `make test_gui_metrics`.
4. **Visual colour** → the theme table `src/gui/wubu_theme.c` (all themes),
   read via `tc()`. Theme-independent control/brand art: named in tokens or
   marked `brand-art` / `icon-identity` / `glyph-art` on the line.
5. **An on-disk or on-wire format fact** (magics, struct layouts, protocol
   numbers) → a shared header in `include/`, e.g. `include/wubu_macho_fmt.h`
   (Mach-O magics, shared by the kernel loader and VSL). Format facts are not
   ours to tune; one header, every layer includes it.
6. **Hardware register offsets / IRQ vectors** → the subsystem's own header:
   LAPIC lives in `src/kernel/interrupt_apic.h`, never re-defined in a
   consumer (the SMP module learned this the hard way).
7. **Module-local, never shared** → `static const` in the `.c` that uses it.
   Not in a header. If a second module needs it, promote it to the right
   owner above — do not mirror it.

## Layer map (the organism)

| layer | owners | gates |
|---|---|---|
| firmware (WuBuFW) | `src/firmware/efi.h` (UEFI spec types), per-module consts | `make test_agi_metal` |
| kernel | `src/kernel/interrupt_apic.h` (APIC), `include/wubu_macho_fmt.h` + PE/ELF headers (formats), `src/kernel/zealos_parity.h` (parity names), `src/kernel/wubu_math.h` (math consts) | kernel tier, `test_kernel_macho`, `test_kernel_bridge` |
| runtime | `src/runtime/wubu_revolver.h` (tunables), `src/runtime/wubu_ftw.h` (POST-system-header fill-ins), VSL format headers | `test_revolver`, `test_snapshot`, VSL tier |
| GUI | `include/wubu_gui_metrics.h`, `src/gui/wubu_tokens.h`, `src/gui/wubu_theme.c`, `src/gui/wubu_settings.c` | `test_gui_metrics`, `test_wm`, `test_standards` |
| apps | `src/apps/apps.h` (icon ids + registry — DESK_ICON_* has ONE owner) | `test_apps` |
| guests | brain/compiler/app submodules own their constants; census runs per-repo with `--root`, edits happen in their own checkout | their own gates |

## Env knobs

Every `getenv` site is inventoried by the census (`env` category). A knob
that bounds an allocation MUST be revolver-clamped; a knob that selects a
path belongs in settings or a named path owner. New knobs get a line in the
census ledger's env registry with purpose and default.

## What is measured

- `tools/std_audit.py` — R2/R4/R7/R11 over the GUI's governed surfaces,
  ratcheted (`make test_standards`).
- `tools/hardcode_census.py` — org-wide: duplicate `#define`s (mirrors vs
  CONFLICTS), path literals, env knobs, literal allocations, literal sleeps.
  Ratchet per repo: `--baseline tools/hardcode_census_baseline.json`.
  Run it on any ~WuBu~ checkout with `--root`; submodule mounts and vendored
  trees are excluded automatically (they get their own run).

## The law

A hardcoded constant in a consumer is a second owner of a decision that
already has one — it silently disagrees with every runtime mechanism built
to change that decision. Search this map, then the tree, before defining
anything. When you find a mirror, merge it into the owner and gate the
behaviour that depended on it. When you find a CONFLICT, one side is usually
a bug: read both, decide the truth, fix the loser, and add the gate that
would have caught it.