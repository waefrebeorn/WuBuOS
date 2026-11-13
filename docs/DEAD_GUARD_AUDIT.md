# Dead guard audit

**Method:** `tools/dead_guard_scan.py` (run 2026-11-13). It builds the real call
graph over `src/**.c`, excludes each function's own signature from its call count,
and reports functions whose name suggests a check but that nothing reaches.

This was run because one such defect was found by hand and fixed in `77b23af`:
`check_canaries()` stamped red-zone canaries on every allocation, detected clobbered
ones correctly, and had **no caller on any allocator path** — an overflow past a user
allocation was silently absorbed by `free()`. A guard that runs only when a test
remembers to is worse than no guard, because it reads as protection.

## Headline result

| repo | defs | uncalled | probe-named |
|---|---|---|---|
| wubuos (`src`) | 21,233 | 4,071 | **47** |
| wubucompositor | 85 | 72 | 0 |
| wubuwizard | 3,222 | 2,062 | **90** |
| wubulens | 103 | 9 | 0 |

The raw "uncalled" numbers are dominated by kernels, JIT codegen and vendored-ish
helpers; they are not defects. The probe-named column is the actionable one.

## Why most hits are false positives

Four mechanisms make live code look dead. Check for these before deleting anything:

1. **Dispatch tables.** `tbl[5-1] = vsl_nt_access_check_and_audit;` has no textual
   `name(` call site, so the scanner counts zero. It is live.
   Live-in-a-table in this tree: `vsl_nt_access_check_and_audit`,
   `vsl_nt_privilege_check`, `vsl_nt_audit_alarm`.
2. **Parity `#define`s.** `zealos_parity.h` maps TempleOS names onto ours:
   `#define TaskValidate task_validate` — so `task_validate` is called as
   `TaskValidate`.
3. **Cross-repo API.** `wubuwizard`/`wubucompositor` link against these objects;
   a header-declared function may have its caller in another repository.
4. **Callbacks registered as arguments** rather than invoked by name.

## Triage of the wubuos probe-named hits

Verified by reading each file rather than trusting the list.

### Real defect — found and fixed
- **`check_canaries`** (`kernel/memory.c`) — the case that motivated the tool.
  Now enforced on `mem_free()` and `mem_realloc()` (`77b23af`). Verification is
  covered by three new checks in `test_memory`.

### Live through a table or alias (not defects)
`vsl_nt_access_check_and_audit`, `vsl_nt_privilege_check`, `vsl_nt_audit_alarm`
(dispatch table), `task_validate` (parity alias), the `*_probe` driver entry points
(registered with the driver framework).

### Header-declared utilities with no caller today (legitimate, documented here)
These are optional helpers for paths that do not exist yet. Not wiring them is not
a bug — but leaving them undocumented invites someone to "fix" them blindly.

- `wubu_gdpr_age_verify()` — the interactive CLI consent flow. Enforcement is
  elsewhere and already fail-closed: `edr_core.c:444` refuses agent events
  (`type >= EDR_EV_AGENT_ACTION`, except `EDR_EV_BOOT_ATTEST`) unless
  `wubu_gdpr_age_check() == WUBU_AGE_CONSENTED`. A GUI consent screen belongs to
  the OOBE work, not to this function.
- `wubu_deploy_verify_output()` — compares a produced image against a caller-supplied
  SHA-256. Nothing in the tree has an expected hash to supply, so there is nothing
  to wire it to. Becomes load-bearing the moment an image pipeline pins a digest.
- `wubu_pkgmgr_verify_installed()` — same shape: reports broken installs when asked.
- `wubu_blk_verify()`, `wubu_smp_detect()`, `wubu_container_detect_format()`,
  `wubu_invariant_check()` — capability/consistency probes for their subsystems.

### Third-party / not ours
`lua/www/**`, `audio/bigmac/**`, `bear/**`, `spells/graphics/render/vkd3d/**` —
excluded from fixes; several appear in the raw list.

## What this means going forward

Run the scanner after adding any checker, and treat a zero-call hit as a question,
not a verdict. When adding a new guard, either call it from the path that needs it
or add it to a dispatch table — and say in the commit which one you did.

The wubuwizard column (90) is the largest remaining surface and lives in its own
repository; it should get the same treatment there rather than being folded into
this tree's numbers.
