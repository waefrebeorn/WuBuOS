# WuBu NT syscall subset — research for completing the 512-slot table

**Date:** 2026-11-15 · **Goal:** online deep research on the Windows 64-bit kernel so
the currently-stubbed NT dispatch slots get real, correctly-signed handlers, giving a
COMPLETE working subset. Standing project framing: WuBu = TempleOS kernel × Linux
kernel merger ("create a Frankenstein"). This doc is the research half; the C11
implementation follows the reviewed plan.

## 1. Authoritative sources (fetched this session, not memory)

- **hfiref0x/SyscallTables** — `data/Composition/X86_64/NT10/ntos/{27924,28020}.txt`:
  full x86_64 NT10 ordinal→name enumeration (NtAcceptConnectPort=2 … NtWorkerFactory
  WorkerReady=1). Gives EVERY name at every x64 ordinal. [raw tab-delimited, pulled]
- **Wine ntsyscalls.h / ntdll.spec** (wine-mirror master; wine-11.0) — the canonical
  per-arch syscall table (`SYSCALL_ENTRY(0x0089, NtCreateUserProcess, 44)`) and each
  Nt* prototype (`@ stdcall -syscall NtCreateKey(ptr long ptr long ptr long ptr)`).
  This is the ONE source that gives both the x64 syscall NUMBER and the EXACT arg
  stack for every syscall — the binding between "which slot" and "what signature."
- **ReactOS doxygen** — `sdk/include/ndk/exfuncs.h`, `obfuncs.h`, `zwfuncs.h`,
  `ntoskrnl/ex/sysinfo.c` : complete NTSTATUS prototypes e.g.
  `NtSetSystemInformation(SYSTEM_INFORMATION_CLASS, PVOID, ULONG)`.
- **windowssyscalls.com /version-map** — cross-version syscall drift (e.g.
  NtCreateUserProcess 0xBA on Win10 1507 → 0xD1 on Win11 24H2): the subset must
  pick ONE stable target (ReactOS NT 5.2/Vista, which the tree already claims).

## 2. What is TRUE today about the 512-slot table (measured, in-tree)

- Dispatch table = 512 slots; `vsl_nt_probe()` per-slot scan (ran a probe binary):
  **395 REAL, 117 STUB, 0 EMPTY, 0 conflict.** Of the 512, coverage = 512.
- **The stub slots cluster in the HIGH ordinals: `320-323`, `339`, `395-512`**
  (117 total). Every classic, frequently-hit NT syscall — NtCreateFile, NtReadFile,
  NtWriteFile, NtQueryInformation*, NtSetInformation*, NtCreateSection/NtMapViewOfSection,
  NtCreateProcess/ThreadEx, registry NtCreateKey/NtOpenKey/NtSetValueKey, object manager
  NtCreateObject/NtQueryObject/NtDuplicateObject, sync NtWaitForSingleObject/
  NtCreateEvent/NtCreateMutant/NtReleaseMutant/NtCreateSemaphore/**the keyed-event
  family (290-293, the SteamOS ntsync take)**, token NtOpenProcessToken/NtAdjustPrivilegesToken,
  APCs, ALPC, WNF, ioring, partition, KTM — are already REAL. So the bridge already
  implements a large, coherent working subset of the classic NT API.
- **Critical naming caveat (why I will not guess):** the bridge enumerates ordinals
  with its OWN `NT_ORD_*` constants, generated from ReactOS `ntoskrnl/sysfuncs.lst`
  (NT 5.2/Vista parity) — NOT the Windows/hfiref0x x64 syscall numbers. Verified:
  `NT_ORD_NtQueryInformationProcess=162` and `NT_ORD_NtWaitForSingleObject=282`
  (Windows x64 uses 25 and 4). So a stub ordinal like 398 must NOT be assumed to be
  hfiref0x's 398 (which is `NtSetInformationWorkerFactory`); assigning names that way
  would fabricate a wrong mapping. `vsl_syscall_list.h` is alphabetical, not ordinal.

## 3. The correct completion plan (how the subset becomes complete)

Step A — **recover the true ordinal→name bindings for 320-512.** The tree's own
`NT_ORD_*` constants reach NtCreateFileNt=398, so a handful exist; the rest must be
derived by mapping the ReactOS `sysfuncs.lst` ordering (which generated the bridge
enumeration) to the 512-wide table. Do this IN THE TREE (the ordinal table generator /
`reactos-study/` copy), not by assuming Windows numbers. Verify the mapping with the
same per-slot probe against a name table, so no slot is mislabeled.

Step B — **implement the highest-value of the 117, from ReactOS source**, gated exactly
like the existing batches (one concern per .c, opaque structs, fail-closed, a
`test_vsl_nt_*` target, a contract-sweep row). Priority by what real Windows software
calls at runtime:
1. system-info/config tail: NtQuerySystemInformationEx, NtSetSystemInformation,
   NtSetInformationDebugObject, NtSetSecurityObject, NtSetSystemTime, NtSetSystemPowerState.
2. sync/fini tail already half-present (keyed-event family done; add NtWaitForAll/AnyEx
   if the slots exist).
3. profile/debug (NtCreateProfile, NtDebugActiveProcess), KTX resolution tail.
Each becomes a `vsl_nt_<subsys>.c` handler bound to the CORRECT recovered ordinal, with
a new failing→passing gate — never a mass overwrite.

Step C — **merge with the SteamOS harvest** (SteamOS judge already mined: plasma 6.8 +
Breeze, ntsync.ko semantics landed). The NT subset and the holo/ATOM/KDE assets are both
"the other OS's kernel/stack, absorbed"; the Frankenstein (TempleOS core × Linux device/
syscall surface = WuBu) is served on the bootable-image side.

## 4. Bottom line for the builder

Do NOT fill slots blind from hfiref0x. Recover the bridge's real ordinal names first
(Step A, in-tree, verifiable), then implement the top batch from ReactOS prototypes
(Step B). That turns "512 registered but 117 stubbed" into "512 registered AND a
complete, correctly-named, probed subset." The research is done; the naming-recovery
is the next concrete, verifiable step before any new handler is written.
