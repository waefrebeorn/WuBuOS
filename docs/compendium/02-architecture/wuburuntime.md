# wuburuntime — THE RUNTIME THAT FILLS THE GAP (research/063)

> 2026-08-04. The user's directive (verbatim-intent):
> "research all of the object oriented [languages], and we even need to
> make our own version of 'wuburuntime' that is basically the runtime
> that fills the gap of all the object oriented runtime based operating
> systems and allows them to have their own compilation space so we
> don't have a huge disorganization issue."
>
> "then we need to snapshot the date at which we made it and the
> intended version of the compiler and version of the language that we
> were going after so that people aren't left in the dust."
>
> Kevin-Bacon 7-hop on the OO languages (the `-i_make_shit_code` bucket
> of the compiler taxonomy), then the wuburuntime design + the version
> snapshot.

## The 7-hop convergence table (the OO families)

| Hop | Family | The core finding |
|-----|--------|------------------|
| 1 | **C++ / Itanium ABI** | STRENGTH: zero-cost OO — vtable dispatch, RTTI, multiple inheritance with offset-to-top, all ABI-defined (the runtime is the ABI, no VM). WEAKNESS: the ABI is a *frozen contract* — any change (new RTTI, new layout) breaks the whole ecosystem; ODR/vtable bloat across compilation units is the disorganization problem. NEED: a place where the ABI's assumptions (vtable layout, RTTI pointers) are *owned and versioned*. |
| 2 | **Simula → Smalltalk → FLEX machine** | STRENGTH: everything is an object; classes ARE objects; the image (VM + all objects) is the program; the FLEX machine was the FIRST attempt at an OO personal computer. WEAKNESS: image persistence is monolithic — no clean file/process boundary, no thread model, no static types; "nobody uses it" because it doesn't play well with a host OS. NEED: an OS that *understands the image* — gives the image its own space instead of fighting it. |
| 3 | **Java JVM** | STRENGTH: the VM as a *platform* — GC (G1/ZGC/Shenandoah), JIT, class loading, all inside a self-contained runtime. WEAKNESS: the JVM IS the OS for its programs (threads, memory, files all virtualized) — it sits ON the host OS as a second OS, duplicating every syscall. NEED: a host that gives the VM *native* integration instead of the VM re-implementing everything. |
| 4 | **C# / .NET CLR** | STRENGTH: designed with full awareness of JVM's strengths/weaknesses — generational GC, value types, unified type system. WEAKNESS: same second-OS problem — the CLR re-implements threads/memory/IO on top of the host. NEED: the runtime's compilation space = a first-class OS object, not an overlord. |
| 5 | **JavaScript / V8 (prototype OO)** | STRENGTH: prototype OO is maximally dynamic — objects can morph, no class ceremony; V8 optimizes with hidden classes/shapes. WEAKNESS: "prototypes force dynamic assumptions on every object which limits how far the engine can optimize" (the V8 prototype-optimization trade-off); poor maintainability at scale. NEED: a runtime that *owns the shape transitions* so dynamic objects have a stable home. |
| 6 | **Rust / Zig / Go / Swift (the modern split)** | STRENGTH: Rust = zero-cost abstractions (trait objects, borrow checking without GC); Go = GC'd simplicity (Oberon lineage); Swift = ARC'd safety. WEAKNESS: each has its own runtime contract (Rust's panic/unwind, Go's goroutines/GC, Swift's ARC) that must coexist on a host. NEED: a host where *multiple runtimes coexist* without fighting over memory/threads/signals. |
| 7 | **WASI / Wasm runtimes (the runtime-OS gap)** | STRENGTH: WASI unifies syscalls across OSes; Wasm runtimes (Wasmtime/Wasmer/WAMR) are embeddable, sandboxed, portable. WEAKNESS: **the exact gap we're filling** — "languages like Python, Ruby, and C# only got basic WASM/WASI support in the last year; GC working inside WASM, exceptions mapping, is still work in progress"; every runtime is fragmented against the host. NEED: an OS where the *runtime IS the interface* — Wasmachine (an OS kernel that IS a Wasm runtime) is the proof this works. |

## The convergence principle

**Every OO runtime is a mini-OS trying to own its compilation space on
top of a host that doesn't know it exists.** C++ owns the ABI; Smalltalk
owns the image; JVM/CLR own a second OS; V8 owns shape transitions;
Rust/Go/Swift each own a runtime contract; Wasm runtimes own a sandbox.
The disorganization is structural: the host OS treats each runtime as a
foreign process, so each re-implements memory/threads/IO, and the
ABIs/images/heaps never share a coherent space.

**wuburuntime is the OS-native answer: every runtime gets its own
compilation space — a first-class OS object — so the runtimes stop
fighting each other and the host.**

## THE WUBURUNTIME DESIGN

### What it is

wuburuntime = the WuBuOS runtime layer that:
1. **Gives every OO runtime its own compilation space** — a named,
   versioned, isolated region (a hive block + a namespace + a
   syscall personality) where a runtime's ABI/image/heap lives.
2. **Fills the gap between the runtimes and the OS** — the OS
   *knows* what a JVM, CLR, Smalltalk image, V8 isolate, or Wasm
   instance is, and provides the shared substrate (memory, threads,
   GC hooks, exception mapping, file namespace) instead of each
   runtime re-implementing it.
3. **Is the compilation-space broker** — when the holyc compiler
   (or any of the `-i_make_shit_code` languages) compiles into
   WuBuOS, the output lands in a compilation space: a versioned
   region that records the compiler version, the language version,
   the ABI snapshot, and the date — so nothing is left in the dust.

### The compilation space (the core abstraction)

```
wuburuntime space {
    id:            u64 (the hive block id)
    name:          "java-jvm-21" / "dotnet-clr-9" / "smalltalk-image-1"
    language:      which OO language/runtime this space serves
    compiler_ver:  the holyc / foreign-compiler version that built it
    language_ver:  the language/runtime spec version targeted
    abi_snapshot:  vtable layout / RTTI / GC hooks / exception map
    created:       the date (snapshot: 2026-08-04)
    heap:          the runtime's memory region (ring-bounded)
    namespace:     the styx 9P namespace for this runtime (/n/java/)
    personality:   the VSL syscall personality (the toast-OS dispatch)
    state:         cold | warm | live | frozen (the amoeba membrane)
}
```

Each space is **isolated by default, bridgeable on request** (the
container doctrine: cgroups/seccomp per space) — a JVM and a CLR can
run in the same OS without fighting over memory, because each owns a
named, bounded region and the OS mediates.

### What the OS provides (the gap filler)

| Need (from the 7-hop) | wuburuntime provides |
|---|---|
| C++ ABI ownership | versioned vtable/RTTI layout per space; the ABI is a *space property*, not a global frozen contract |
| Smalltalk image | the image IS the space; the OS understands object-graph persistence (the hive), no host fighting |
| JVM/CLR second-OS | the VM's syscalls map to the space's personality — native, not re-implemented |
| V8 shape transitions | shape/hidden-class tables as a space resource |
| Rust/Go/Swift runtimes | coexisting runtime contracts — panic/unwind, goroutines, ARC each get a space |
| Wasm/WASI | a space per Wasm instance; WASI syscalls → the OS's native syscalls (the Wasmachine path) |
| GC in a foreign runtime | GC hooks (root scanning, safepoints) as OS-provided services |

### The snapshot (the "not left in the dust" guarantee)

Every compilation space records **the exact versions** of everything
that built it, so a space from 2026 still loads in 2036:

```
wuburuntime SNAPSHOT 2026-08-04
  compiler:    holyc (WuBuOS) — version 0.1.0, the C11 sacred tongue
               + -c_developer (C18/C2*), -i_make_shit_code, -brainfuck
  language:    HolyC — the WuBuOS dialect, C/C++ middle ground, ring-0
  ABI:         wubu-abi-v1 (x86-64, the Itanium-style vtable we own)
  spaces:      per-runtime regions, versioned, bridged via the hive
  guarantee:   any space built against this snapshot loads as-is;
               the compiler's own versions are recorded in every
               artifact it produces (the holyc CLI emits them)
```

The compiler already speaks this: the holyc CLI's `-c_developer`,
`-i_make_shit_code`, and `-brainfuck` flags are the language-policy
front-end; wuburuntime is the runtime that gives each of those
languages a **space to live in**.

## The implementation wave (wubuos)

### Wave 1 (this session): the space registry — `wubu_runtime_space`
- `include/wubu_runtime.h` + `src/wubu_runtime.c`: the space struct
  (id/name/language/compiler_ver/language_ver/abi_snapshot/created/
  heap/namespace/personality/state), the registry (create/find/
  destroy/list), backed by the hive for the slots and the tensor
  store for the snapshot metadata.
- `tools/test_runtime.c`: the DA oracles —
  1. create a space, read it back (round-trip, the snapshot is intact)
  2. two runtimes (jvm-21, clr-9) coexist — separate ids, no overlap
  3. the snapshot carries compiler_ver + language_ver + created date
     (the "not left in the dust" guarantee)
  4. ring-bounded: the registry caps at N spaces, recycles the oldest
  5. namespace: each space maps to its own 9P path (/n/java/, /n/dotnet/)
  6. the state machine: cold → warm → live → frozen
  7. ASan clean
- Wire into `make test_all`.

### Wave 2: the compilation-space broker (the compiler integration)
- the holyc CLI gains `-space <name>`: compiles INTO a named space,
  recording compiler_ver + language_ver + date in the space metadata.
- `-i_make_shit_code` + `-space java-jvm-21`: the foreign language's
  output lands in the JVM's space — disorganization solved by
  construction.

### Wave 3: the runtime personalities (the gap filler)
- each space's `personality` field wires to the VSL syscall dispatch
  (the toast-OS personalities already in wubuos): a JVM space gets
  the POSIX-ish personality, a Smalltalk space gets the image-aware
  personality, a Wasm space gets WASI → native syscall mapping.

### The honest scope
- Wave 1 is real and testable now (the registry + snapshot).
- Waves 2-3 are the roadmap; the OS-native runtime substrate (GC
  hooks, exception mapping) is the long game — but the COMPILATION
  SPACE abstraction is what ends the disorganization, and it's
  buildable today.

## Registration

- This design: `docs/compendium/02-architecture/wuburuntime.md`
  (+ the snapshot table above).
- The compiler it builds on: `compiler-doctrine.md` (the flags + the
  language taxonomy).
- Sources: the 7 hops (Itanium C++ ABI, Smalltalk/FLEX, JVM, CLR, V8
  prototypes, Rust/Zig/Go/Swift, WASI/Wasm-runtimes) — archived in
  `docs/compendium/05-sources/`.
