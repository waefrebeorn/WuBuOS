# The NT ordinal table holds two numbering generations (2026-11-24)

## What the tree has

The VSL dispatch table is filled by ~15 translation units. Two generations of it
are registered side by side:

| generation | who | numbered against |
|---|---|---|
| `vsl_nt_*` handlers | `vsl_nt_atoms.c`, `vsl_nt_io.c`, `vsl_nt_job.c`, … | the Win7-era ReactOS `sysfuncs.lst` ordering |
| `sys_Nt*` handlers | `vsl_nt_misc_w11.c` | **Windows 11 24H2** (37 of 38 verified) |

Both claim 83 ordinals, and `tbl[N-1] = handler` means the winner is decided by
**init order** — i.e. by link order, which is arbitrary. Two test failures
(`NtRevertContainerImpersonation`, `NtSetCachedSigningLevel`) were exactly this:
a "ghost" alias from the other generation had overwritten the real handler.

## The authority

`tools/nt_syscall_win11_24h2.txt` — **489 ordinals → names**, taken from the
"Windows 11 24H2" column of <https://j00ru.vexillium.org/syscalls/nt/64/>.

Why 24H2 and not something older: the tree's own `sys_Nt*` bulk table matches that
column 37 times out of 38 (395 = `NtRevertContainerImpersonation`,
408 = `NtSetCachedSigningLevel`, 409 = `NtSetCachedSigningLevel2`, 103 =
`NtAcquireCrossVmMutant`, …). ReactOS's `sysfuncs.lst` was tried first and is
**wrong** for this tree — it is Win7-era, and an attempt to resolve the collisions
with it removed 60 *correct* handlers (`ccb88bc2`, reverted in `19a3ace3`).

There is also a systematic off-by-one to know about: 24H2 has
`NtWorkerFactoryWorkerReady` at ordinal 1, which the older list does not have, so
everything after it shifts. That is why the legacy `vsl_nt_access_check` sits at 2
while 24H2 says `NtAccessCheck` is ordinal 0 (and ordinal 0 cannot even be
expressed by `tbl[N-1]`).

## The measured cost of migrating

```
tools/nt_migrate_ordinals.py
  MOVE     66   the 24H2 slot is free -> the handler can be relocated
  DROP      0
  BLOCK   313   the 24H2 slot holds a DIFFERENT handler -> human decision
  UNKNOWN  28   the handler's name does not appear in the 24H2 table
```

and, decisively, **the test suite calls 136 ordinals by number**. Ten of them
would lose their handler, across **17 call sites** in
`src/runtime/vsl/vsl_syscall_nt_test.c`:

| old | handler | 24H2 | call-site lines |
|---|---|---|---|
| 2 | `vsl_nt_access_check` | 0 | 340, 1440, 1451, 1458, 1469, 1532 (and `test_pim2.c:37`) |
| 235 | `vsl_nt_set_information_job_object` | 425 | 1281 |
| 240 | `vsl_nt_set_information_token` | 429 | 1511 |
| 242 | `vsl_nt_set_io_completion` | 435 | 1320 |
| 256 | `vsl_nt_set_uuid_seed` | 451 | 179, 204 |
| 263 | `vsl_nt_suspend_process` | 462 | 1068 |
| 266 | `vsl_nt_terminate_job_object` | 466 | 257, 1306 |
| 273 | `vsl_nt_unload_key` | 474 | 992 |
| 276 | `vsl_nt_unlock_file` | 477 | 1085 |
| 277 | `vsl_nt_unlock_virtual_memory` | 478 | 966 |

## The rule this established

**The table and the tests must move in the same change.** Applying the 66 moves
alone took `test_vsl_nt` from 278/278 to twelve failures, because the call sites
still asked for the old numbers and therefore exercised whichever handler now held
that slot. Collisions fell 83 → 68 and the suite stopped describing the tree —
"red for the wrong reason", which gets "fixed" by editing the assertion.

`--apply` now refuses in exactly that situation:

```
REFUSING to apply: 10 ordinal(s) the tests call by number would lose their handler
([2, 235, 240, 242, 256, 263, 266, 273, 276, 277]).
```

## What a migration needs

1. Decide the target: 24H2 (recommended — it is what the `sys_Nt*` half already
   matches, and it is the newest).
2. Resolve the 313 BLOCKs by hand (each is a name that maps to an ordinal another
   handler holds; some are genuinely different syscalls, some are the same syscall
   registered twice).
3. Handle ordinal 0 (`NtAccessCheck`): `tbl[N-1]` cannot express it — either shift
   the table to 0-based or leave that one syscall unregistered and say so.
4. Move the 17 call sites in the same commit, then re-run `test_vsl_nt`
   (expect 278/278) and `test_nt_ordinals` (the collision count must DROP).
5. Run the full suite; the migration must not change any other verdict.

Until then the tree stays as it is: 83 baselined collisions, `test_vsl_nt` 278/278,
`test_nt_ordinals` PASS, and the audit naming the correct owner for **62 of 83**
collisions so the work is a checklist instead of a mystery.
