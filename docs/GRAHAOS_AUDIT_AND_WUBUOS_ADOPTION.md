# GrahaOS Audit → WuBuOS Adoption Plan

> Generated 2026-07-18 from `github.com/waefrebeorn/grahaos_audit` (clone depth-1).
> Angel-coder review: what to steal, what to reject, what to build next.

## 0. Executive verdict

GrahaOS is a **clean-room, capability-only x86_64 OS** written from the first
commit. Its *core* (capability enforcement, snapshot/rollback, transactional
speculation, audit) is genuinely well-architected and is something WuBuOS
**lacks entirely**. WuBuOS is broader (ZealOS kernel + Win98 shell + Styx/9P +
Arch containers + Proton/Wine) but has **no capability model and no rollback
primitive** — it leans on POSIX uid/permission + NT transliteration.

**Action: adopt GrahaOS's three load-bearing ideas into WuBuOS, and DO NOT copy
its stubbed edges (TLS, FS open/stat/fsync/close, restore).**

## 1. What GrahaOS does RIGHT (steal these)

### 1.1 Capability-only authority (no uid/gid/root)
- `kernel/cap/token.h`: 16 cap kinds (`CAP_KIND_*`), 64-bit rights bitmask
  (`CAP_RIGHT_*`), generation-counted tokens.
- `kernel/cap/object.c` + `handle_table.c`: capabilities are the SOLE admission
  path. No ambient authority.
- `kernel/cap/system.h` (`CAP_KIND_SYSTEM`): diminishing-derive bootcap cascade
  — init gets a narrowed sub-token; revoke at root cascades. O(handle-table).
- `revoke.c`: generation-counted use-after-revoke detection.

**WuBuOS gap:** WuBuOS still threads POSIX-style permission through VSL/NT layer.
Adopt a `cap_object` + `cap_handle_table` model as the substrate; have VSL
syscalls resolve a capability instead of checking uid.

### 1.2 Single machine-readable manifest (`etc/gcp.json`)
- Declares every syscall, channel message type, audit event, cap kind/right.
- Kernel parses it; userspace reads it; `scripts/gcp2wit.py` emits WASM
  Component-Model bindings. Kernel and userspace **cannot disagree**.
- `SYS_MANIFEST_EXPORT` exports the blob to processes.

**WuBuOS gap:** syscall tables live in multiple places (`vsl_syscall_table.c`,
`vsl_syscall_*.c`, NT transliteration tables). Adopt ONE manifest that generates
the VSL dispatch table + the Styx9P op enum + the HolyC FFI stubs.

### 1.3 Speculation + atomic rollback (the killer feature)
- `kernel/snap/` — COW snapshot: captures page tables (PTEs flipped RO, COW
  fault on write), VMOs, GrahaFS v2 version-chain heads, channel endpoints.
  O(1) in the unchanged-memory case.
- `kernel/txn/` — wraps a snapshot in a transactional region; external channel
  sends are buffered into a per-txn VMO ring and replayed on COMMIT or
  discarded on ABORT. State machine: ACTIVE→COMMITTING→{COMMITTED|FAILED},
  ACTIVE→ABORTING→ABORTED, atomic CAS on `transaction_t.state`.
- This lets an AI agent *speculate freely* and undo on error — exactly the
  "we make the AGI OS" requirement.

**WuBuOS gap:** zero rollback. An agent that writes the wrong file/FS state
cannot undo. Adopt `snap` + `txn` as a WuBuOS subsystem (`src/runtime/wubu_spec/`)
wrapping the VSL process + Styx9P FS pins.

### 1.4 Audit as a first-class primitive
- `kernel/audit.c` (1434 LOC): structured audit records, `AUDIT_DEPRECATED_SYSCALL`
  emitted when a retired syscall is called so `audit_query` surfaces unmigrated
  callers.

**WuBuOS gap:** no structured audit. Adopt an audit ring + `audit_query`.

## 2. What GrahaOS does WRONG / is STUBBED (reject these)

| Area | Status in GrahaOS | Do in WuBuOS |
|------|-------------------|--------------|
| TLS stack | `libtls-mg/mongoose_tls_core.c` = 634 B returning `-ENOSYS` | build real TLS or reuse host OpenSSL via hosted layer |
| FS syscalls (OPEN/STAT/FSYNC/CLOSE) | `-ENOSYS` "until Phase 19" | wire real Styx9P FS ops from day one |
| SNAPSHOT RESTORE | `SYS_SNAP_RESTORE` = `-ENOSYS` "until W16" | ship restore BEFORE ship create (rollback is the point) |
| AI mode (`grahai`) | non-functional (blocked on TLS stub) | AGI loop runs in hosted userspace, not in-kernel; no in-kernel TLS needed |
| Drivers | Ring-3 userspace (good) but e1000/AHCI maturity unknown | keep drivers in containers (WuBuOS already does this via Arch/bwrap) |

**Lesson:** GrahaOS's *core* is real; its *edges* are stubs. When we adopt, we
build the core AND immediately wire the edges to WuBuOS's existing real
subsystems (Styx9P FS, Arch containers, hosted TLS) so we never ship `-ENOSYS`.

## 3. Concrete WuBuOS adoption (no monoliths, opaque structs, C11)

Proposed new subtree `src/runtime/wubu_spec/` (matches "split + reuse" mandate):

```
src/runtime/wubu_spec/
  wubu_cap.h            // public: opaque cap_object, cap_handle_table API
  wubu_cap_internal.h   // opaque struct, flags, rights bitmask
  wubu_cap_token.c      // token mint/derive/revoke (generation-counted)
  wubu_cap_table.c      // per-process handle table lookup O(n)
  wubu_cap_system.c     // bootcap cascade root (diminishing derive)
  wubu_snap.h           // public snapshot API (SYS_SNAP_CREATE/RESTORE)
  wubu_snap_internal.h  // snapshot_t opaque, COW page tracker
  wubu_snap_create.c    // COW PTE flip + VMO pin + FS version-chain pin
  wubu_snap_restore.c   // per-page PTE reinstall + FD/pledge restore
  wubu_snap_delete.c    // refcount drop
  wubu_txn.h            // public transactional speculation API
  wubu_txn_internal.h   // transaction_t state machine
  wubu_txn_begin.c      // txn_begin -> snap_create + active frame
  wubu_txn_commit.c     // replay buffered channel sends
  wubu_txn_abort.c      // discard buffer, restore snapshot
  wubu_audit.h
  wubu_audit_ring.c     // structured audit records + audit_query
```

Each `.c` includes only its own `_internal.h` (no god headers). The VSL syscall
layer resolves a `wubu_cap_*` instead of checking uid/permission. The Styx9P FS
operations back the snapshot FS-version pins for real (no `-ENOSYS`).

## 4. Manifest-driven dispatch (kill the scattered tables)

Replace `vsl_syscall_table.c` + NT transliteration tables with a single
`etc/wubu_gcp.json` analog (`src/runtime/wubu_spec/wubu_manifest.json`) that
generates, at build time via a Python script (`scripts/wubu_manifest_gen.py`):
  1. `wubu_vsl_dispatch.c`  (VSL syscall switch)
  2. `wubu_styx_ops.h`       (Styx9P op enum)
  3. `wubu_holyc_ffi.h`      (HolyC FFI stubs)

This is the GrahaOS `gcp2wit.py` pattern, applied to WuBuOS's three surfaces.

## 5. Immediate next steps (execute, not plan)

1. [x] Deleted `src/_legacy_bak/` (28 dead files incl. `apps__doom.c`) — the
      literal "doom". Build graph clean; `make hosted` green; 22/22 engine
      tests pass.
2. [ ] Scaffold `src/runtime/wubu_spec/` with opaque `wubu_cap_*` (token+table
      + system) — compile + unit test BEFORE touching VSL.
3. [ ] Add `wubu_snap_*` (create + restore together — restore is not optional).
4. [ ] Add `wubu_txn_*` wrapping snap, with buffered Styx9P sends.
5. [ ] Add `wubu_audit_ring.c`; emit `AUDIT_DEPRECATED` on retired VSL ops.
6. [ ] Write `scripts/wubu_manifest_gen.py`; convert `vsl_syscall_table.c` to
      generated-from-manifest.
7. [ ] Wire VSL syscall entry to `wubu_cap_resolve()`; keep NT transliteration
      as a *layer above* the cap check, not the authority.

## 6. Audit scorecard

| Property | GrahaOS | WuBuOS (today) | After adoption |
|----------|---------|----------------|----------------|
| Capability-only auth | YES (16 kinds) | NO (uid/perm mix) | YES |
| Single manifest | YES (gcp.json) | NO (scattered) | YES (generated) |
| Rollback primitive | YES (snap+txn) | NO | YES |
| Structured audit | YES | NO | YES |
| Real FS | PARTIAL (stubs) | YES (Styx9P) | YES |
| Container/driver isolation | userspace drivers | Arch/bwrap containers | YES (stronger) |
| AGI execution loop | in-kernel (broken TLS) | hosted userspace | hosted userspace (correct) |

**Bottom line:** GrahaOS proves the capability+rollback+manifest architecture.
WuBuOS has the *breadth* (containers, FS, GUI, Wine/Proton) GrahaOS lacks.
Marrying them = the AGI OS. We delete the doom (done) and build the substrate.
