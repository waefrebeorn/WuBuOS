# What ZealOS provides that the holyd (HolyC/ZealC) compiler-runtime needs

**Study scope.** This is a source-grounded research note, not code. It answers, from
ZealOS (the maintained fork of Terry Davis's TempleOS, by the Zeal-Operating-System
project), what OS-level runtime support a HolyC-family compiler-as-a-JIT actually depends
on, and maps each feature to the concrete need for WuBuOS's `holyd` (the HolyC-compiler
in `/home/wubu/wubunos`, `holyd_*.c`). The primary source is a `git clone --depth 1` of
`https://github.com/Zeal-Operating-System/ZealOS` taken at study time; ZC source files
quoted below were read directly from that clone, so the file URLs in Sources are the
canonical blobs of exactly the text cited. Claims I could not confirm from a fetched or
cloned artifact are explicitly marked `[unverified]`.

> Naming note: ZealOS renamed the "HolyC" language to "ZealC" and the source extension
> became `.ZC` (files like `CMain.ZC` are the compiler). In this study "HolyC/ZealC" means
> that language, and "holyd" means WuBuOS's reimplementation.[1]

---

## 1. What ZealOS is: identity, lineage, and design (cited)

ZealOS is a *modernized fork of the 64-bit Temple Operating System*. Its README states:
"The Zeal Operating System is a modernized fork of the 64-bit Temple Operating System.
Guiding principles of development include transparency, full user control, and adherence
to public-domain/open-source implementations," and it lists "HolyC -> ZealC" among its
changes.[1] Its lineage is documented in the same README: ZealOS was forked from ZenithOS
in July of 2021; ZenithOS itself was forked from TempleOS in November of 2019 by VoidNV
(which had itself been forked/renamed through the TempleOS family tree).[1] So ZealOS is
TempleOS's living, maintained descendant, and its compiler is a HolyC compiler by
lineage even though it is branded ZealC.[1]

The "everything is the OS" single-address-space, ring-0, no-MMU-protection design is
readable directly in the source layout and the runtime model.[5][4] The whole system — kernel,
compiler, window manager, apps — is one body of ZealC source that is compiled *in the
OS, at boot, self-hosted*: the boot-tail script `StartOS.ZC` begins "//This is executed by the System task at boot" and immediately
does `#include "/Kernel/KernelA.HH"`, `#include "/Compiler/CompilerA.HH"`,
`#include "/Kernel/KernelB.HH"`, `#include "/Kernel/KernelC.HH"`,
`#include "/Compiler/CompilerB.HH"` — i.e. it *#includes the kernel headers and the
compiler headers as ordinary source* before building the window-manager task and entering
"server mode".[5]

Two runtime tells that confirm the shared/ring-0 model from *inside the compiler's data
structures*: the compiler allocates its compiled-code output from a normalized heap that
distinguishes a **code heap** from a **data heap** (`Fs->code_heap` vs `Fs->data_heap`),
with the module loader noting "Less than 2Gig system memory" as a single-address-space
edge case,[4] and the symbol tables chain *through the file-system's* hash table
(`cc->htc.global_hash_table->next = Fs->hash_table`) — meaning symbol identity is global
across the OS, not per-process (there is no process isolation in the classic
single-range ring-0 design).[2] The hard claim "no MMU hardware protection / identity
map" from the prior TempleOS study is consistent with this but I did not extract a
dedicated ZealOS `Memory.ZC` passage this session, so I mark the *mechanism-level* version
(full identity-mapped single range, paging/protection disabled system-wide) of that
specific claim `[unverified]` here; the *consequence* — that the compiler and runtime
live in one shared address range and all OS code is ZealC-compiled into that range — is
directly evidenced by `StartOS.ZC` and the code/data-heap fields above.[5][4]

Given all of it is the OS, the OS *is* the compiler and vice-versa: there is no
user/kernel boundary and no separate "toolchain" — compilation is a runtime service.[5][2]

---

## 2. The HolyC compiler-as-library / JIT model (cited, with source files)

ZealOS keeps the compiler as a **set of callable library functions plus a terminal
driver**, all compiled into the OS image.[2] From `src/Compiler/CMain.ZC`:

- `LexStatement2Bin(CCompCtrl *cc, I64 *_type, I64 comp_flags=0)` — "Compile one cc
  statement to bin code." It runs `ParseStatement(...)` then `COCCompile(cc, &size, NULL, _type)`
  and returns `res`, a pointer to emitted machine code; when `CCF_AOT_COMPILE` is set it
  streams the code into an AOT buffer with `AOTStoreCodeU64(...)` and returns the address
  instead.[2][6]
- `CompJoin(...)` — a full compilation context handler that allocates a `CLexHashTableContext`,
  builds **fresh `local_hash_table` and `global_hash_table` hash tables** for symbol
  lookup, chains the global table onto the prior context (on JIT path onto `Fs->hash_table`),
  invokes `Lex(cc)` (the lexer), loops `ParseStatement`, runs `AOTGlobalsResolve(...)`, and
  wraps the whole pipe in `try { ... } catch { ... }` with a `//TODO: else del misc?`
  clean-up path — i.e. the compiler is a stateful, exception-guarded library.[2][6]
- `Comp(comp)` / `CompBuf(...)` / `CAOT *CompBuf(U8 *buf, ...)` — compile a whole file or an
  in-memory buffer into an AOT object/executable.[2]
- `RunFile(...)`, `ExeFile(...)`, `ExePutS(...)`, `ExePrint(...)` — the runtime JIT entry
  points (section 3 below).

The **identifier-resolution / symbol-table** model: the compiler keeps per-compile-context
hash tables (`local`, `global`, `define`) and links the global table into the parent
(`global_hash_table = HashTableNew(1024)` when no function context, chaining onto
`Fs->hash_table` for JIT). A module that is dynamically loaded is registered as a symbol in
the *same* global hash table as `HTT_MODULE | HTF_PUBLIC` (see section 3), so **symbols and
identifiers resolve at runtime against a live, extensible registry** rather than a static
link-time symbol table.[2][4][6] The loader even does the file-system-relative symbol
registration by name (`HashGenericAdd(fbuf+2, HTT_MODULE | HTF_PUBLIC, zxe_addr)`) — the
name under which the module was loaded becomes a runtime-resolvable identifier.[4]

The **file hierarchy is literally source files**: the repo tree is `src/Apps/`,
`src/Compiler/`, `src/Demo/`, `src/Doc/`, `src/Home/`, `src/Kernel/`, `src/Misc/`
(containing `Bible.TXT`), `src/System/` — the "Bible", the "Compiler", the "Apps", the
"Kernel" the study title references are directories/files in the OS's own fs and are
compiled in by `#include`, not external binaries.[2][5][7] `ExeFile` compiles "a file" by
turning it into an `#include` and running it.[2]

## 3. The runtime primitives a JIT'd HolyC needs (cited)

**Executable memory produced and called in place.** `ExeCmdLine` in `src/Kernel/KTask.ZC`
is the canonical loop: `machine_code = LexStatement2Bin(cc, &type); ... res = Call(machine_code);`
— the compiler emits a machine-code buffer and the terminal driver *calls it directly as a
C function pointer*. The function's own doc-comment reads "//Terminal JIT-compile-and-execute
loop for CCompCtrl."[3] `Call` itself is declared as the primitive `public _extern _CALL
I64 Call(U8 *machine_code); //Call address with no args.`[6] The module loader allocates
into the executable **code heap** (`MAllocAligned(size, module_align, Fs->code_heap, ...)`),
so executable memory is an explicit runtime resource.[4]

**The JIT string/printf/REPL surface.** In `CMain.ZC`:
- `I64 ExePutS(U8 *buf, ...)` — "//JIT Compile and execute text from a puts(\"\")."[2]
- `I64 ExePrint(U8 *format, ...)` — "//JIT Compile and execute text from a printf()."[2]
- `I64 ExeFile(U8 *name, ...)` — "//JIT Compile and execute a file." implemented as
  `#include "<file>";` then `ExePutS`.[2]
- `RunFile(name, ...)` — ExeFile then "//$LK, ExeFile ...$() with args using ... LastFun()/LastFun(argc, argv)".[2]
- `StreamExePrint(...)` — feeds a streamed value into an `#exe{}` AOT block ("StreamExePrint
  only allowed in AOT compiled #exe{} mode").[2]
- `CInit()` — compiler initialization routine shipped in the same file; the compiler
  library self-initializes.[2]

So the runtime needs: compile-from-a-source-string, compile-from-a-file, compile-from-a-formatted-
string, and compile-and-run-with-arguments, each producing machine code that executes
in the current address space immediately.[2][3]

**The string/class/virtual-method system and first-class 64-bit scalar types.**
`LexStatement2Bin` returns the runtime type of the compiled statement in `*_type` and the
result is stored as `Fs->answer` with `Fs->answer_type`, so a value (+ its type code) is a
first-class interpreted-style return even though execution is compiled.[3][2] The kernel
header defines the scalar type vocabulary for the 64-bit codegen: the NaN-tracked union
`I64i` — a `union I64` overlay made of 8-byte arrays of `I8i`/`U8i`, `I16`/`U16`, and
`I32`/`U32` — plus the compiler's runtime type codes (`#define RT_I64  10`,
`#define RT_U64 11`, `#define RT_F64 14` for the 64-bit double), and the
`CCompCtrl` state carries native `I64 cur_i64; F64 cur_f64;` as its working scalars — i.e.
64-bit signed/unsigned ints and 64-bit `double` floats are the compiler's primitive scalar
types (this is a 64-bit, all-`I64` OS).[6]

The class/virtual-method and macro systems (`M(argc, ...)` variadic-metaprogramming, the
"Just" and "Unsafe" compile modes) are core HolyC features I did **not** extract verbatim
this session — the relevant `src/System/DolDoc/DocCodeTools.ZC` use of `CCF_JUST_LOAD` and
`ExeFile` was seen, and `CCF_JUST_LOAD` / `CCF_AOT_COMPILE` flags are defined in
`KernelA.HH`, but the `M`-macro definition and a definitive Just/Unsafe semantic description
were not pulled from source, so those fine points are **marked `[unverified]`** rather than
asserted; the *flag existence* (Just and AOT are real compile modes) is cited.[2][6]

**Dynamic compilation of new functions while running — the DLL-style module loader.**
`src/Kernel/KLoad.ZC` provides `CZXE *Load(U8 *filename, ...)` — "//Load a .ZXE file module
into memory." A `.ZXE` (signature `'ZXE'` = "// Zeal Executable") is an AOT-compiled module:
`Load` reads it from the fs, allocates a code-heap block aligned to
`module_align = 1 << zxe->module_align_bits`, copies it in as **load-address-independent
relocatable code**, then fixes it up with `LoadPass1`/`LoadPass2` over its absolute-address
patch table — a classic load-time relocation/linking pass (TLB-like) — and finally
registers the module *by name* in the global hash table as `HTT_MODULE | HTF_PUBLIC`, making
its symbols resolvable at runtime; `ld_flags & LDF_JUST_LOAD` supports a load-only (no-exec)
mode.[4] This is the "DLL AppLoad / module loading" mechanism: new, independently-compiled
functions/modules get loaded, relocated, linked and called while the system is running.[4] The
kernel itself bootstraps the same way (`LoadKernel()` registers `KERNEL_MODULE_NAME` as an
`HTT_MODULE | HTF_PUBLIC` symbol and runs `LoadPass1` on its boot patch tables).[4]

## 4. How ZealOS boots and self-hosts into the compiler REPL (cited)

Boot: ZealOS UEFI-boots via the Limine bootloader plus the ZealBooter prekernel, then
`LoadKernel()` pulls the kernel module into memory and relinks it against its patch tables
[1][4] (the "AOT to disk + load" path). The tail of boot is self-hosting: the System task
executes `StartOS.ZC` (see section 1), which `#include`s the kernel and compiler headers,
purges the symbol hash table (`HashTablePurge(sys_task->hash_table)`), reads
`#include "/System/MakeSystem"`, spawns the window-manager task
(`sys_winmgr_task = Spawn(&WinMgrTask, NULL, "Window Manager")`), initialises the registry
(`RegInit` / `RLf_REGISTRY`), and then the System task "enters server mode" — the doc
comment is "//After this file, the System task enters $LK,\"server mode\",A=\"HI:Job\"$."[5]

REPL: interactive execution is a task loop. `UserTaskCont()` — "//Terminal key-input-execute
loop" — creates a `CCompCtrl` with `CCF_CMD_LINE | CCF_PROMPT | CCF_QUESTION_HELP`, then
`Lex(cc); ExeCmdLine(cc);` repeatedly until the prompt flag clears:
```
cc = CompCtrlNew(, CCF_CMD_LINE | CCF_PROMPT | CCF_QUESTION_HELP);
...
Lex(cc);
ExeCmdLine(cc);
cont = Bt(&cc->flags, CCf_PROMPT);
...
```
and `ServerTaskCont()` — "//Act as server task in a loop handling commands" — runs
`CLI` (command-line interpreter) plus `JobsHandler(...)` and `Yield` in a `while(TRUE)` loop.
So the boot image ends as a **live command-line JIT REPL where every typed statement is
compiled to machine code and executed by `Call`**.[3]

## 5. "What holyd needs": ZealOS HolyC-runtime feature → WuBuOS holyd need (table)

Each row: the ZealOS feature (cited above) → the concrete runtime service `holyd` depends
on in WuBuOS. The right column is the *need* (there are six distinct holyd needs; count = 6).

| ZealOS HolyC-runtime feature (source) | What holyd needs |
|---|---|
| Executable heap / `Call(machine_code)` — `code_heap` alloc + direct `Call` of emitted code[4][6][3] | **Executable memory**: map emitted machine-code to writable-and-executable memory and call it via a function pointer in-place. |
| Persistent runtime symbol registry — hash-table chains rooted at `Fs->hash_table`, module names registered `HTT_MODULE|HTF_PUBLIC`; `HashGenericAdd`/`HashTableNew`[2][4][6] | **Runtime-extensible symbol/type registry**: a live, mutable global table that resolves identifiers *and* first-class type codes at runtime and can gain new entries (modules) after boot. |
| `ExeCmdLine` / `UserTaskCont` REPL — compile one statement then `Call()` it[3][2] | **REPL entry**: a terminal loop that compiles a source string/buffer and immediately executes it, surviving a failed line (try/catch around compile+`Call`). |
| `Load()` of `.ZXE` modules — alloc, align, copy, relocate via patch tables, link, call[4] | **DLL-style app/module loader**: load a separately-compiled module at an arbitrary address, apply load-time relocation/linking, and call its entry; register it in the symbol registry. |
| `ExePutS`/`ExePrint`/`ExeFile`/`RunFile`/`Comp`/`CompBuf` exported as in-OS library calls[2] | **Compiler-as-library API**: expose lex→parse→emit→call as ordinary callable functions (not an external tool), so the OS/REPL can invoke compilation programmatically. |
| `LexStatement2Bin` returning a runtime type + `Fs->answer`/`answer_type`; 64-bit `I64`/`F64` scalars in `CCompCtrl`[2][3][6] | **First-class 64-bit scalar types + runtime value/type pairing**: I64/U64/F64 primitives matched to the codegen target, and a value-with-type result passed back from executed statements. |

The two flagged entries in section 3 (the `M(argc,…)` macro/metaprogramming system and the
"Just"/"Unsafe" compile modes) are **additional** holyd needs that ZealOS satisfies, but
Since I could not extract their defining source text this session I exclude them from the
"needs" count above rather than assert specifics. (The count of 6 is deliberately limited to
needs whose ZealOS evidence is confirmed verbatim in this study.)

## 6. Verified vs. unverified (honest gaps)

- **Verified from clone/read (verbatim):** README lineage + "HolyC -> ZealC"; `ExePutS`/
  `ExePrint`/`ExeFile`/`RunFile`/`StreamExePrint`/`CompBuf`/`CompJoin`/`LexStatement2Bin`
  in `CMain.ZC`; `ExeCmdLine`/`UserTaskCont`/`ServerTaskCont` + `Call(machine_code)` in
  `KTask.ZC`; `Load()`/`LoadPass1`/`LoadPass2`/`LoadKernel` + `.ZXE`+hash registration in
  `KLoad.ZC`; `StartOS.ZC` self-host order + "server mode"; `CCF_AOT_COMPILE`/`CCF_JUST_LOAD`/
  `Call` decl/`CCompCtrl` `I64 cur_i64; F64 cur_f64`/CZXE struct in `KernelA.HH`; the
  `src/{Apps,Compiler,Demo,Doc,Home,Kernel,Misc{Bible.TXT},System}` tree.[1][2][3][4][5][6][7]
- **`[unverified]` this session:** full-identity-map/no-MMU-protection *mechanism* statement
  (the single-address-space / code-and-data-heap *consequence* is verified, but I did not
  pull a `Memory.ZC` passage describing the identity/paging detail); the `M(argc,…)` macro
  system's definition; exact "Just"/"Unsafe" semantics (only the `CCF_JUST_LOAD`/`CCF_AOT_COMPILE`
  flags were source-confirmed). The `I64i`/`F64i` union bodies are now **verified** (section 3).

## Sources

[1] https://github.com/Zeal-Operating-System/ZealOS — ZealOS README
[2] https://github.com/Zeal-Operating-System/ZealOS/blob/master/src/Compiler/CMain.ZC — [2] Compiler core CMain.ZC: ExePutS/ExeFile/Comp/CompBuf/CompJoin, hash tables
[3] https://github.com/Zeal-Operating-System/ZealOS/blob/master/src/Kernel/KTask.ZC — [3] KTask.ZC: ExeCmdLine JIT loop, Call(machine_code), UserTaskCont REPL
[4] https://github.com/Zeal-Operating-System/ZealOS/blob/master/src/Kernel/KLoad.ZC — [4] KLoad.ZC: Load() .ZXE module loader, abs-reloc, hash register
[5] https://github.com/Zeal-Operating-System/ZealOS/blob/master/src/StartOS.ZC — [5] StartOS.ZC: boot self-host includes, Spawn WinMgrTask, server mode
[6] https://github.com/Zeal-Operating-System/ZealOS/blob/master/src/Kernel/KernelA.HH — [6] KernelA.HH: CCF_* flags, CZXE, Call decl, I64i/F64i, code/data heap
[7] https://github.com/Zeal-Operating-System/ZealOS/blob/master/src/Misc/Bible.TXT — [7] Misc/Bible.TXT + repo file tree (src/Apps,Compiler,Kernel,System)
