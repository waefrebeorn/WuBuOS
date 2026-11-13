# WuBuOS NT syscall ABI

**Scope:** the numbered NT-compatible syscall surface exposed by
`src/runtime/vsl/vsl_syscall_nt.c` (`g_nt_dispatch[]`, 512 slots) and every handler
registered into it by the `vsl_nt_*` modules.

**Status: stable for callers. Slots are only ever added.** A slot number, once it
returns something other than `NOT_IMPLEMENTED`, must keep meaning the same syscall
for the lifetime of the tree. This document exists because until now the behaviour
existed but the rule did not; `docs/BMDS_GAP_ANALYSIS.md` flagged exactly this as the
cheapest and most protective gap to close.

---

## 1. The surface

| Property | Value |
|---|---|
| Slot count | `NT_TBL_SIZE` = **512**, numbered 1–512 |
| Default contents | `vsl_sys_nosys` (returns `NT_STATUS_NOT_IMPLEMENTED`) |
| Registration | modules register into the shared table at init (`nt_dispatch_init`) |
| Dispatch entry | `vsl_nt_dispatch(num, args, n_args)` |
| Argument marshalling | up to 6 `uint64_t`; missing arguments become 0 |
| Return convention | `NT_STATUS_SUCCESS` (0), or negative errno translated by `vsl_errno_to_nt_status` |

Registered modules, in order (the order is part of the design, see §3):
`misc_w11` (stubs first) → `atoms`, `job`, `io`, `vmem`, `process`, `thread`,
`section`, `timer`, `sync`, `registry`, `token`, `misc` → `alpc`, `wnf`, `worker`,
`enclave`, `ioring`, `partition`, `ktm`.

## 2. Slot states

Every slot is in exactly one state. Callers can read it without invoking the slot.

| State | Meaning | Dispatch result |
|---|---|---|
| `WUBU_NT_SLOT_EMPTY` | nothing registered | `NOT_IMPLEMENTED` |
| `WUBU_NT_SLOT_STUB` | placeholder registered early so a real handler can replace it | `NOT_IMPLEMENTED` |
| `WUBU_NT_SLOT_REAL` | genuine handler | handler runs |
| `WUBU_NT_SLOT_CONFLICT` | two REAL handlers claimed the slot; last writer won | handler runs (which one is **unspecified**) |

Query with:

```c
wubu_nt_slot_state_t st;
if (vsl_nt_probe(num, &st) == 0 && st == WUBU_NT_SLOT_REAL) { /* safe to call */ }
```

`vsl_nt_probe()` never invokes the handler and never has side effects. **Always
probe before calling a slot whose support you depend on** — calling an unsupported
syscall cannot be distinguished from a stub by return value alone, and calling a
real one may have side effects.

Slot-state totals are available via `vsl_nt_slot_counts(&empty,&stub,&real,&conflict)`
and coverage (anything non-empty) via `vsl_nt_coverage()`. Measured at the time of
writing: **100 empty, 17 stub, 395 real, 0 conflict** -- 412 of 512 non-empty.
`test_nt_abi` asserts the partition sums to 512, that per-slot probes agree with the
totals, and that nothing looks REAL before the table is initialised;
`test_vsl_nt_collision` asserts the collision count stays zero and
`test_vsl_nt_contract` sweeps every slot forked with `alarm(1)`.

Two probe semantics worth stating plainly:

- A probe of an **uninitialised** table reports EMPTY for every slot. The dispatch
  table is zero-filled statically and only populated on the first dispatch, so
  NULL must not be read as a handler.
- A **refused** probe (out of range, or NULL state pointer) still writes EMPTY.
  It does not leave the caller's previous contents behind, so a caller that forgets
  to check the return value sees "unsupported" rather than acting on stale data.

## 3. What may change, and what may never

**May never change**
- A slot's number ↔ syscall identity, once that slot leaves EMPTY.
- The argument count/order of a REAL handler, and the meaning of its arguments.
- That an EMPTY or STUB slot returns `NOT_IMPLEMENTED`.
- Dispatch stays fail-closed: out-of-range and unhandled both yield `NOT_IMPLEMENTED`.

**May change (additive)**
- An EMPTY slot becoming STUB or REAL.
- A STUB becoming REAL (this is the documented upgrade path).
- Internal implementation of a REAL handler, provided the observable contract holds.
- New modules registering new slots.

**Requires an ABI note in the commit message**
- A REAL handler replaced by a different REAL handler (a CONFLICT). The current
  code makes this *visible* (`vsl_nt_collision_count()`, per-slot records, and now
  `WUBU_NT_SLOT_CONFLICT`) rather than silent — that visibility is the mechanism,
  and removing it is an ABI break.

## 4. Retirement procedure

A syscall cannot simply be deleted. To retire slot N:

1. Replace its handler with a STUB that logs once and returns
   `NT_STATUS_NOT_IMPLEMENTED`. The slot number stays occupied; nothing that
   probed it sees EMPTY.
2. Note the retirement in the commit message and here, with the release boundary.
3. Keep the slot reserved forever. EMPTY is only for slots that have never been
   used; re-using a retired number would silently change what a caller sees.

If a slot was never REAL, it may return to EMPTY freely.

## 5. Conflict handling

Registration is order-based: later modules overwrite earlier ones. The
`misc_w11` stub layer registers FIRST precisely so that real `vsl_nt_*` handlers
can take their slots without conflict. When a REAL handler does overwrite another
REAL handler, the slot is marked CONFLICT and:

- the collision is recorded (`slot`, previous owner, new owner),
- `vsl_nt_collision_count()` increments,
- `vsl_nt_probe()` reports `WUBU_NT_SLOT_CONFLICT`.

**A conflict means the syscall's ownership is ambiguous. Treat any non-zero
collision count as a defect to fix, never as expected state.** The conformance test
fails on it.

## 6. Conformance gates

- `make test_vsl_nt_contract` — sweeps all 512 slots with all-zero arguments, each in
  a forked child under `alarm(1)`: no handler may crash on hostile arguments.
- `make test_vsl_nt_collision` — asserts no slot is contested.
- `make test_nt_abi` — probes states, verifies fail-closed dispatch, checks that
  probing never invokes a handler.

Run all three before changing anything in `vsl_syscall_nt.c` or adding a module.

## 7. Adding a syscall

1. Pick the lowest EMPTY slot in your module's range; record which one in a comment.
2. Register through your module's `_register(tbl, size)` function.
3. Handle NULL and garbage arguments as a normal error — the contract sweep depends
   on it.
4. Add a probe-based check to your unit test rather than assuming support.
