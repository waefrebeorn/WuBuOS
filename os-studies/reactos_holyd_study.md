# How compiled code runs on NT/ReactOS — the runtime host a holyd-compiled PE needs

## WuBuOS os-study: loader + process/thread host (the execution contract for `holyd`)

Scope note: this is the companion to `reactos_study.md` (which covered the NT-kernel /
win32k / object-manager backbone). This study answers the *other* half of the question
WuBuOS's from-scratch C11 `holyd` compiler needs answered before it can run anything: when
you hand the system a compiled image — either a standalone Windows PE or an in-process
native module — what exactly must exist to *load it and start its code executing*? Primary
source is the ReactOS tree shallow-cloned to `/tmp/reactos`; line-level claims below are
backed by the source I read. Anything asserted from general NT knowledge without a ReactOS
source is marked `[unverified]`.

Legend: `[unverified]` = standard behaviour asserted from working knowledge, not confirmed
in a fetched ReactOS file this session. Every other claim maps to a registered source.

---

## 1. Architecture — the path from an EXE on disk to code running

### 1.1 The kernel pre-seeds ntdll into every address space; ntdll is the user-mode runtime seed

The user side of "running an image" is not self-booting. Before the loader runs at all,
the Process/Manager must already have mapped the system DLL (ntdll) into the new process.
This is the Process Manager's `PspLocateSystemDll`: it opens `\SystemRoot\System32\ntdll.dll`,
validates the image, creates a section for it (`ZwCreateSection(... PAGE_EXECUTE, SEC_IMAGE,
FileHandle)`), references that section, and maps it into the process `MmMapViewOfSection` via
`PspMapSystemDll`.[6] The mapped image base is remembered as `PspSystemDllBase`, the section as
`PspSystemDllSection`, and its entry point as `PspSystemDllEntryPoint`.[6]

The kernel then *finds by exported name* the handful of user-mode thunks it must be able to
enter, using `RtlpFindExportedRoutineByName` against the mapped system DLL
(`PspLookupSystemDllEntryPoint`). `PspLookupKernelUserEntryPoints` resolves the user-mode APC,
exception, callback and raise-exception dispatchers (`KiUserApcDispatcher`,
`KiUserExceptionDispatcher`, `KiUserCallbackDispatcher`, `KiRaiseUserExceptionDispatcher`),
and on x86 chooses the actual syscall mechanism by CPU capability: if `KF_FAST_SYSCALL`,
`KiFastSystemCall`/`KiFastSystemCallRet` are resolved and their addresses written into
`SharedUserData->SystemCall`/`SystemCallReturn`; otherwise the `int 2Eh`-style
`KiIntSystemCall` stub is used.[6] `PspInitializeSystemDll` resolves the single most
important entry — `LdrInitializeThunk` — into `PspSystemDllEntryPoint`.[6]

So the *contract* a holyd/runtime host must match is: before your compiled code ever runs,
a resident seed image must already exist at a known base, export the dispatcher/syscall
thunks, and have a bootstrap entry point that the kernel will call into. On NT that seed is
ntdll; on WuBuOS it must be the equivalent of `PspMapSystemDll`.[6]

### 1.2 `LdrInitializeThunk` → `LdrpInit` → `LdrpInitializeProcess` (first thread) / `LdrpInitializeThread` (later threads)

When the kernel starts a user thread it points the initial instruction pointer at the
seed DLL's startup thunk (see §1.3), so **the first user-mode code a process executes is
the loader, not `main`.[8] In ReactOS the work is `LdrpInit(PCONTEXT Context, ...)`, invoked
with the initial thread's captured context so the loader can finish building the process
before the entry point runs. Read from `dll/ntdll/ldr/ldrinit.c`[1]:

- **Stack deallocation discovery.** If the TEB has no deallocation stack yet, `LdrpInit`
  calls `NtQueryVirtualMemory` on `Teb->NtTib.StackLimit` (MemoryBasicInformation) and
  records `Teb->DeallocationStack = MemoryBasicInfo.AllocationBase` — i.e. it asks the VM
  manager which allocation backs the initial stack so that stack can be released when the
  thread / process exits.[1]
- **Single-flight process init.** An interlocked gate flag `LdrpProcessInitialized` is
  compare-exchanged; while another thread is already initializing (flag == 1) the caller
  sleeps in 30 ms delays (`ZwDelayExecution`) until the flag clears, guaranteeing one
  process-level load.[1]
- **First thread only:** if `Peb->Ldr` is still absent (no loader data yet), it is the first
  thread: `Peb->LoaderLock = &LdrpLoaderLock` and `LdrpInLdrInit = TRUE` are set, then the
  whole process load runs `LdrpInitializeProcess(Context, SystemArgument1)` inside a
  top-level SEH frame; on success `LdrpProcessInitialized` is incremented so later threads
  skip it; on failure `LdrpInitFailure` raises the NTSTATUS into the process.[1]
- **Later threads:** if `Peb->Ldr` is already set up, `LdrpInitializeThread(Context)` runs
  the much lighter per-thread TLS / resync path instead of re-loading the world.[1]
- **APC drain** finishes `LdrpInit`: `NtTestAlert()` at the end.[1]

### 1.3 The kernel thread-start path — `PspCreateThread` rides a user APC into the loader thunk

`PspCreateThread` (ntoskrnl/ps/thread.c) allocates an `ETHREAD` via `ObCreateObject`,
creates the thread's TEB with `MmCreateTeb`, then calls `KeInitThread` with
`PspUserThreadStartup` as the user mode callback and the captured thread context.[8]
It then links the ETHREAD into the process's `ThreadListHead` and calls `KeStartThread`
(or `KeSuspendThread` if `CreateSuspended`).[8]

`PspUserThreadStartup` is the concrete hand-off: it raises to APC_LEVEL and calls
`KiInitializeUserApc` with the exception/trap frames and, critically,
`PspSystemDllEntryPoint` + `PspSystemDllBase` as the routine to run in user mode[8] — which,
per §1.1, is `LdrInitializeThunk` in ntdll.[6] So the *initial user instruction* is
`LdrInitializeThunk` reached via a user APC, which funnels into `LdrpInit` → the loader.[6][8]

### 1.4 Process creation — `PspCreateProcess` / the PEB and the section

Before the thread there must be a process. `PspCreateProcess` (ntoskrnl/ps/process.c)
allocates `EPROCESS`, initializes the KPROCESS via `KeInitializeProcess`, and when given a
section handle (a real user-mode image) calls `MmInitializeProcessAddressSpace` to build
the address space and map `MmCreatePeb` to install the PEB, then `PspMapSystemDll(Process,
...)` to seed ntdll into it.[7] The initial PEB is minimal: ReactOS zeroes an `INITIAL_PEB`,
sets `Mutant = (HANDLE)-1` and `ImageUsesLargePages = 0` before `MmCreatePeb`.[7] The process
is then inserted into the Object Manager (`ObInsertObject`) and, under the parent route,
into the active-process list.[7]

So the *kernel side* of the contract is: an EPROCESS/ETHREAD pair with real address space,
a PEB page, a pre-seeded system DLL (ntdll), and an initial thread whose context is set so
the CPU lands on the loader thunk via a user APC. **Nobody "executes" your EXE; the kernel
only builds the host and points the first instruction at the loader, and the loader
interprets your PE's headers and calls its entry point.**

### 1.5 The user-mode spawn syscall used to reach the above

Process creation in the Win32/console world flows `CreateProcessW` →
`kernelbase!CreateProcessInternalW` → CSRSS notification (»§3.4) and ultimately the Native
API `NtCreateUserProcess`, whose signature is: `(ProcessHandle, ThreadHandle,
ProcessDesiredAccess, ThreadDesiredAccess, ProcessObjectAttributes, ThreadObjectAttributes,
ProcessFlags, ThreadFlags, ProcessParameters (RTL_USER_PROCESS_PARAMETERS),
CreateInfo (PS_CREATE_INFO), AttributeList (PS_ATTRIBUTE_LIST))`.[11] Calling it directly is
fraught because a process created with system calls "dies instantaneously" unless the
Windows subsystem is first told about it via `ntdll!CsrClientCallServer`, which sends a
message to the CSRSS process notifying it of the new process (done after create-in-suspended
and before resume); and `RTL_USER_PROCESS_PARAMETERS` pointers must be re-based into the new
process's VA space.[10] Note the ReactOS trunk this session stores process creation under the
same Ps module but exposes the older `NtCreateProcess`/`NtCreateProcessEx` wrappers
(`NtCreateProcessEx` at line 1384 of process.c, absent from the spawned-first-thread API in
`NtCreateUserProcess`)[7][10]; the NT6 `NtCreateUserProcess` single-call form is what performs
image-load + process + initial thread atomically.[10][11]

---

## 2. The PE image loader contract — exactly what a holyd-compiled PE must satisfy

This is the load-interpretation half that `LdrpInitializeProcess` performs after the
pre-seed (the user-loader files `dll/ntdll/ldr/*.c` drive IAT/relocation/TLS/entry, while the
kernel-side image-section mapping that backs them lives in `ntoskrnl/mm/ARM3/sysldr.c` — e.g.
mapping an image section into the system process via `MmMapViewOfSection` with `ViewUnmap`,
then copying the image into freshly reserved PTEs for a driver-style load).[9][1][2][3][4]
Each numbered item is one term of the contract.

### 2.1 Headers are trusted pointers: `RtlImageNtHeader(ImageBase)` and the data directory

The loader's first act for the image is to recover the NT headers from the mapped base
(`RtlImageNtHeader(Peb->ImageBaseAddress)`), and it reads every subsequent PE structure out
of the `OptionalHeader.DataDirectory` by RVA.[1] The PE format defines these directories
explicitly: import, export, base relocations, resource; `RVA` = "the address of an item after
it is loaded into memory, with the base address of the image file subtracted", and a VA is the
same thing without the subtraction — "the loader might not load the image at its preferred
location", which is precisely why relocation exists.[12] **holyd contract #1: emit a well-formed
`IMAGE_NT_HEADERS` with a populated optional-header data directory, mapped so that RVAs resolve
against the in-memory base.**

### 2.2 Section mapping per headers

Sections are the "basic unit of code or data"; an image "can contain a number of sections, such
as .tls or .reloc, which have special purposes", and "all the raw data in a section must be loaded
contiguously".[12] The loader relies on `SizeOfRawData` / `Misc.VirtualSize` / `VirtualAddress` to
locate the IAT if the directory is incomplete: `LdrpSnapIAT` falls back to scanning the section
table for the section containing the import directory's RVA and uses that section's `VirtualSize`
(or `SizeOfRawData` if the former is 0 — the comment calls out "Watcom and other retarded
compilers").[2] **holyd contract #2: sections with correct `VirtualAddress`, `VirtualSize`,
`SizeOfRawData`, mapping and characteristics, so the loader's blind header-driven mapping is valid.**

### 2.3 Import resolution: walk the import descriptors, snap the IAT

`LdrpInitializeProcess` calls `LdrpWalkImportDescriptor`, which walks the import descriptors and
for each loads the required DLL and snaps the IAT.[1] `LdrpSnapIAT(ExportLdrEntry, ImportLdrEntry,
IatEntry, ...)` is the concrete fixup: it locates the export directory of the exporting module,
computes the IAT from the importing module (`IMAGE_DIRECTORY_ENTRY_IAT`, or the section that holds
the import directory if no IAT directory is present), then patches each thunk in the IAT with the
resolved import address.[2] **holyd contract #3: emit an import directory the loader can walk, with
an IAT that is writable; the compiler's emitted thunks must be patchable in place.**

### 2.4 Base relocation — the `.reloc` fixup

When an image cannot be mapped at its preferred base, its whole value structure is wrong and must
be moved. The loader detects this after a DLL load: `LdrpMapDll` checks the returned status; on
`STATUS_IMAGE_NOT_AT_BASE` it marks the entry `LDRP_IMAGE_NOT_AT_BASE`, checks whether the base is
free (scanning `InLoadOrderModuleList` for overlaps), unprotects the view, and calls
`LdrRelocateImageWithBias`, then restores protection.[4] The relocation math itself lives in RTL
(`sdk/lib/rtl/image.c`): `LdrRelocateImageWithBias` reads `IMAGE_DIRECTORY_ENTRY_BASERELOC`, walks
`IMAGE_BASE_RELOCATION` blocks, and for each 16-bit `TypeOffset` (offset in low 12 bits, type in high
4) `LdrProcessRelocationBlockLongLong` applies `Delta = BaseAddress - ImageBase` — handling
`IMAGE_REL_BASED_ABSOLUTE`, `HIGH`, `LOW`, `HIGHLOW` (32-bit), and `DIR64` (64-bit) fixups.[15] The
whole EXE path does the same: if `Peb->ImageBaseAddress != ImageBase`, the loader unprotects, runs
`LdrRelocateImageWithBias`, and re-protects.[1] **holyd contract #4: either reserve the preferred
base (then no reloc needed) or emit a correct `.reloc` directory; and emit `IMAGE_FILE_RELOCS_STRIPPED`
only if you truly have no relocations, because the loader returns `Conflict` when that bit is set but
a move is needed.[15]**

### 2.5 Delay imports

Delay-loaded imports are handled by a compiler/runtime-stub mechanism (`__delayLoadHelper`
equivalent) using `IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT`; `ntimage.h` defines the
`IMAGE_DELAYLOAD_DESCRIPTOR` structures the loader/runtime recognizes.[4][12] For a
self-contained holyd PE this directory may be absent entirely. **holyd contract #5: no delay-import
directory needed if no lazy dependencies; if used, the descriptor/helper must match the loader's
structure layout.**

### 2.6 TLS — the loader decides what is thread-local and when callbacks run

`LdrpInitializeProcess` calls `LdrpInitializeTls` after the import walk and before the init
routines.[1] `LdrpInitializeTls` walks the in-load-order module list, reads each module's
`IMAGE_DIRECTORY_ENTRY_TLS` via `RtlImageDirectoryEntryToData`, allocates per-module TLS data, writes
the running `TlsIndex` into the TLS directory's `AddressOfIndex`, and finally `LdrpAllocateTls` to
materialise the per-thread TLS area.[1] TLS callbacks run in load order: `LdrpRunInitializeRoutines`,
for each module that carries `TlsIndex && Context`, calls `LdrpCallTlsInitializers(LdrEntry,
DLL_PROCESS_ATTACH)` — which walks `TlsDirectory->AddressOfCallBacks` (a NULL-terminated
`PIMAGE_TLS_CALLBACK` array) and invokes each callback with `(DllBase, DLL_PROCESS_ATTACH, NULL)` —
*before* the module's `EntryPoint` is called.[4][1] **holyd contract #6: a valid TLS directory
(`AddressOfIndex`, `AddressOfCallBacks`), and thread-local data declared via the compiler's `__declspec(thread)`-
equivalent, so the loader's per-thread copy works; TLS callbacks, if any, must be NULL-terminated.**

### 2.7 Entry point — `AddressOfEntryPoint`, then callbacks, then init routines

`LdrpFetchAddressOfEntryPoint` reads `NtHeaders->OptionalHeader.AddressOfEntryPoint` and adds
`ImageBase` (returns NULL if the field is zero).[4] The loader records it on each data-table entry at
mapping time (`LdrEntry->EntryPoint = LdrpFetchAddressOfEntryPoint(...)` for the image and for
ntdll).[1] Then `LdrpRunInitializeRoutines` drives execution in initialization order: it walks
`Peb->Ldr->InInitializationOrderModuleList`, and for each not-yet-processed entry calls first TLS
callbacks (if TLS present) then `LdrpCallInitRoutine(EntryPoint, LdrEntry->DllBase,
DLL_PROCESS_ATTACH, Context)` — i.e. every DLL initializer runs to completion before the EXE's own
entry point is finally invoked.[1] Post-import, kernel32's `BaseProcessInitPostImport` is resolved
and called first for the EXE path when a context is present.[1] **holyd contract #7: `AddressOfEntryPoint`
must be the exact VC/CRT-style entry the loader expects to enter with `(DllBase, Reason,
Context)` — which for a from-scratch compiler means a small hand-written entry stub that sets up the
runtime (argv/env/atexit heap) and then calls `main`; do not make the loader enter `main` directly.**

**Load-order summary (from `LdrpInitializeProcess`[1]):** recover NT headers → (kernel32
preload + resolve post-import/base queries) → `LdrpWalkImportDescriptor` (loads all static
imports) → relocate EXE if moved (`LdrpSetProtection` / `LdrRelocateImageWithBias`) → set
`LoadCount = -1` across modules → `LdrpInitializeTls` (build TLS area + indexes) →
`LdrpRunInitializeRoutines` (per module: TLS callbacks then entry point, init-order) →
`Peb->PostProcessInitRoutine` → return; loader failure path raises via `LdrpInitFailure`.[1]

---

## 3. The runtime-host services the loader and process depend on

### 3.1 ntdll — the single user-mode syscall boundary and dispatcher host

Everything the loader does with the system is a syscall via ntdll: `NtQueryVirtualMemory` for
stack-deallocation discovery[1], VM map/unmap for image and TEB/PEB placement, and the APC /
exception / syscall thunks the kernel resolved by name into `SharedUserData`.[6] A WuBuOS host
maps its VSL NT syscall layer into exactly this role: the resident seed image's exported
`Nt*`/`Zw*`/`KiUser*` functions are the boundary. [unverified]

### 3.2 The heap — `RtlCreateHeap` / `RtlAllocateHeap` / `ProcessHeap`

`LdrpInitializeProcess` builds the process heap itself: `RtlInitializeHeapManager()` then
`Peb->ProcessHeap = RtlCreateHeap(HeapFlags, NULL, NtHeader->OptionalHeader.SizeOfHeapReserve,
SizeOfHeapCommit, NULL, &HeapParameters)`, with flags influenced by the image's load-config
(`ProcessHeapFlags`) and alignment by subsystem version; a second private `LdrpHeap` is created
for loader data.[1] `RtlAllocateHeap` (heap.c) is the workhorse the loader and everything else use
(alignment rounding, force flags, debug-heap dispatch).[5] **A host must implement a working heap and
publish it as `ProcessHeap` before importing `ntdll`/loader data, since the loader allocates its own
data entries and TLS data from it.**[1][5]

### 3.3 The loader lock, loader data (`LdrData`), and the module lists

`Peb->LoaderLock` (initialised `RtlInitializeCriticalSection(&LdrpLoaderLock)`), `Peb->FastPebLock`,
and `Peb->Ldr -> PEB_LDR_DATA` + the three doubly-linked module lists
(`InLoadOrderModuleList`, `InMemoryOrderModuleList`, `InInitializationOrderModuleList`) are the
synchronised bookkeeping the loader maintains; `LdrLoadDll` takes `LdrLockLoaderLock(...)` for the
duration of a load and releases it via `LdrUnlockLoaderLock(...)` (using the returned `Cookie`).[1][3]
`LdrQueryProcessModuleInformation` walks `InLoadOrderModuleList` under the loader lock to snapshot
the module table (used as a "loader snapshot" by debuggers/tools) — a role a host must reproduce so
`LoadLibrary`-style and diagnostic tooling work.[3]

### 3.4 Process bookkeeping: CSRSS / Win32 subsystem registration

A process spawned purely via the native API is not yet a functioning Windows process: it must be
registered with the Windows subsystem, else it "dies instantaneously" / faults on Win32 calls.[10]
The registration is `ntdll!CsrClientCallServer` sending the process-existence message to CSRSS,
over LPC/ALPC (`NtSecureConnectPort` + `CsrPortHandle`/`CsrPortMemoryRemoteDelta`, then
`NtAlpcSendWaitReceivePort`); the call happens after create-in-suspended, before resume.[10] **On
WuBuOS this is the VSL `ALPC` personality + `/n/win32/subsys/*` registration step for a spawned
process.**[unverified]

### 3.5 TLS allocator and per-thread state

The per-thread TLS area and `TEB->TlsSlots` / static TLS are established at process init by
`LdrpInitializeTls` → `LdrpAllocateTls`, and every later thread runs `LdrpInitializeThread` to
pick up / resync its TLS state.[1] A host must provide the TLS allocator (the loader writes the
module index into `AddressOfIndex`) and per-thread TLS setup on thread spawn.[1][unverified]

---

## 4. How a from-scratch C11 compiler's output could be hosted — two viable routes

### 4.1 Route A — load a compiled PE through an `Ldr`-equivalent (the "PE on disk" route)

Give `holyd` a native PE output and implement a WuBuOS loader that mirrors the contract in §2: [unverified]

1. kernel-side spawn: build an EPROCESS/ETHREAD analogue + PEB page + pre-seed a resident
   runtime DLL (the ntdll role) — §1.1 / §1.3.[6][7][8]
2. user-side load: `LdrpInitializeProcess` equivalent — `RtlImageNtHeader`, section map
   per headers, `LdrpWalkImportDescriptor` → IAT snap, relocate via
   `LdrRelocateImageWithBias`, `LdrpInitializeTls`, then `LdrpRunInitializeRoutines` to the
   entry point.[1][2][15] [unverified]
3. process heap (`RtlCreateHeap` → `ProcessHeap`), loader lock, module lists — §3.2 /
   §3.3.[1][5][3]

This is the "fully Windows-shaped" route: it makes `holyd`'s PE a first-class process that
Win32 console/CSRSS (or WuBu's `/n/win32/subsys`) can register. [unverified]

### 4.2 Route B — present the compiler's output as an in-process native module (the JIT / "no PE on disk" route)

For JIT (or bytecode→native) there is no file to parse, but the Loader still provides the same
*services*; the host reuses the *image-map + fixup + import + entry* contract with the output
held in memory:

- **image-map**: build it at a chosen base instead of `NtMapViewOfSection`; apply the same
  relocation logic (`LdrRelocateImageWithBias` with `Delta = ActualBase - ImageBase`) so
  emitted absolute addresses become correct, or emit position-independent code to make relocs
  a no-op.[15]
- **import binding + entry**: treat the in-memory module's import table as the `LdrpSnapIAT`
  target and run its entry point through the same `(DllBase, Reason, Context)` init-routine
  discipline.[2][1]
- **TLS + heap + loader lock**: give the module a TLS directory (→ per-thread copies by
  `LdrpInitializeTls`) and run on the existing `ProcessHeap` + loader lock.[1][5][3]

The two routes are the *same contract*; Route B just substitutes a memory-resident image for
the disk-backed section mapping. A holyd that emits position-independent, self-contained code
(no reloc directory, empty import table, minimal `AddressOfEntryPoint` stub) minimises the loader
surface it needs from the host. [unverified]

---

## 5. Map table — each ReactOS loader/runtime piece → what WuBuOS `holyd` needs

The mappings below are the study's own design proposals for WuBuOS; each left-column piece is
sourced, the right-column holyd mapping is design intent. [unverified]

| ReactOS / NT piece (source) | What it does for the process | holyd / WuBuOS equivalent |
|---|---|---|
| `PspLocateSystemDll` / `PspMapSystemDll`, `PsNtDllPathName`[6] | Opens, sections, maps ntdll into every new process at a known base | Map a resident runtime "seed" DLL into each hosted process before code runs |
| `PspLookupKernelUserEntryPoints` / `RtlpFindExportedRoutineByName`[6] | Resolves user-mode thunks (APC/exception/syscall) into `SharedUserData->SystemCall` | Export-resolver for the host's VSL dispatcher / syscall thunks by name |
| `PspInitializeSystemDll` → `LdrInitializeThunk`[6] | Resolves the loader bootstrap entry the kernel will first run | Entry thunk (loader bootstrap) in the seed DLL |
| `PspCreateProcess` + `MmInitializeProcessAddressSpace` + `MmCreatePeb`[7] | Builds EPROCESS, installs PEB page, computes address space | WuBu kernel-side spawn: EPROCESS analogue + PEB page in `/n/nt/ps` |
| `PspCreateThread` + `KeInitThread(PspUserThreadStartup)` + `KiInitializeUserApc`[8] | Builds ETHREAD, creates TEB, starts user code at `LdrInitializeThunk` via user APC | Thread spawn path that lands the initial thread on the loader thunk |
| `LdrpInit` → `LdrpInitializeProcess` (first) / `LdrpInitializeThread` (later)[1] | Gates one process-load; builds loader data + TLS; later threads resync | Loader main + per-thread TLS resync in holyd runtime |
| `LdrpInitializeProcess`: `RtlImageNtHeader` + data-directory reads[1] | Interprets the PE optional-header directories | PE-header parser (RVAs against in-memory base) |
| `LdrpWalkImportDescriptor` → `LdrpSnapIAT`[1][2] | Loads dependencies; resolves export directory; patches IAT | Import resolver; empty-table OK for self-contained |
| `LdrpMapDll` / `LdrRelocateImageWithBias` / `LdrProcessRelocationBlockLongLong`[4][15] | Relocates when base is taken (HIGHLOW/DIR64 fixups) | Relocator; or reserve base / emit PIC to skip |
| `LdrpInitializeTls` + `LdrpCallTlsInitializers`[1][4] | Builds per-thread TLS area; runs TLS callbacks before entry | TLS template copy per thread + TLS-callback runner |
| `LdrpFetchAddressOfEntryPoint` / `LdrpRunInitializeRoutines`[1][4] | Computes `ImageBase+AddressOfEntryPoint`; runs init-order routines | Entry-stub + init-routine dispatcher into `main` |
| `RtlCreateHeap` / `RtlAllocateHeap` + `Peb->ProcessHeap` [1][5] | User heap backing the loader and all allocations | VSL/RTL heap published as ProcessHeap before any load |
| `Peb->LoaderLock` / `Peb->Ldr` + 3 module lists / `LdrLockLoaderLock`[1][3] | Synchronised loader bookkeeping + module snapshot | Cap-backed loader lock + module registry (map to `/n`) |
| CSRSS registration via `ntdll!CsrClientCallServer` (LPC/ALPC)[10] | Registers the new process with the Win32 subsystem or it dies on Win32 calls | `/n/win32/subsys/*` + VSL ALPC registration on spawn |

---

## 6. What is NOT verified (deliberate gaps)

- The ReactOS *trunk checked out this session* exposes `NtCreateProcess`/`NtCreateProcessEx`
  (process.c lines 1384/1445)[7]; the NT6 `NtCreateUserProcess` flow is cited from the Core
  Security article and Faran-17 repo, not from that ReactOS file. Treat the exact
  `NtCreateUserProcess` ReactOS implementation as `[unverified]` for this tree.
- Delay-import handling details are asserted from the directory name + `ntimage.h` presence of
  `IMAGE_DELAYLOAD_DESCRIPTOR`; I did not read a full delay-load helper implementation.
  `[unverified]` on the helper internals.
- The `LdrpForkProcess` (inherited-address-space re-bootstrap) path is explicitly
  `STATUS_NOT_IMPLEMENTED` / `UNIMPLEMENTED` in the source read[1], so the map/result for fork
  is not covered.
- ReactOS wiki pages `Loader` and `Environment` return empty (404) and were therefore **not**
  cited; claims that could not be tied to the source tree or the fetched web pages are marked
  `[unverified]` rather than silently padded.

## Sources

[1] https://github.com/reactos/reactos/blob/master/dll/ntdll/ldr/ldrinit.c — ReactOS ntdll ldrinit.c (LdrpInit, LdrpInitializeProcess, LdrpInitializeThread, LdrpInit, LdrpInitializeTls)
[2] https://github.com/reactos/reactos/blob/master/dll/ntdll/ldr/ldrpe.c — ReactOS ntdll ldrpe.c (LdrpMapAndViewSection, PE section mapping, LdrpMapCleanupData)
[3] https://github.com/reactos/reactos/blob/master/dll/ntdll/ldr/ldrapi.c — ReactOS ntdll ldrapi.c (LdrLoadDll, LdrpLoadImportModule, LdrpMapDll)
[4] https://github.com/reactos/reactos/blob/master/dll/ntdll/ldr/ldrutils.c — ReactOS ntdll ldrutils.c (LdrpRelocateImage, LdrpFixupIAT, LdrpLoadDelayedImport, TLS/entrypoint, module list ops)
[5] https://github.com/reactos/reactos/blob/master/sdk/lib/rtl/heap.c — ReactOS RTL heap.c (RtlCreateHeap, RtlAllocateHeap, ProcessHeap)
[6] https://github.com/reactos/reactos/blob/master/ntoskrnl/ps/psmgr.c — ReactOS ntoskrnl psmgr.c / ps.c (NtCreateUserProcess, PspCreateUserProcess)
[7] https://github.com/reactos/reactos/blob/master/ntoskrnl/ps/process.c — ReactOS ntoskrnl process.c (PspCreateProcess, process/PEB setup)
[8] https://github.com/reactos/reactos/blob/master/ntoskrnl/ps/thread.c — ReactOS ntoskrnl thread.c (PspCreateThread, PspStartThread)
[9] https://github.com/reactos/reactos/blob/master/ntoskrnl/mm/ARM3/sysldr.c — ReactOS ntoskrnl/mm ARM3 sysldr.c (MmCreateImageSection, MmMapViewOfSection image loader)
[10] https://www.coresecurity.com/core-labs/articles/creating-processes-using-system-calls — Core Security: Creating Processes Using System Calls (NtCreateUserProcess, CSRSS, RtlCreateProcessParameters)
[11] https://github.com/Faran-17/Windows-Internals/blob/main/Processes%20and%20Jobs/Processes/Creation%20Of%20Process.md — Faran-17 Windows-Internals: NtCreateUserProcess signature & create-process flow
[12] https://learn.microsoft.com/en-us/windows/win32/debug/pe-format — Microsoft Learn: PE Format — sections, base relocation, IAT, TLS directory
[15] https://github.com/reactos/reactos/blob/master/sdk/lib/rtl/image.c — ReactOS RTL image.c (LdrRelocateImageWithBias, LdrProcessRelocationBlockLongLong)
