---
title: "RFC: Add an LLVM CAS library and experiment with fine-grained caching for builds"
source_url: https://discourse.llvm.org/t/rfc-add-an-llvm-cas-library-and-experiment-with-fine-grained-caching-for-builds/59864
author: Duncan P. N. Exon Smith (dexonsmith), LLVM Discourse
ingested: 2026-08-03
avenue: DevTools (DT)
type: rfc
---

# RFC: Add an LLVM CAS library and experiment with fine-grained caching for builds

## Goal

Long-term: Speed up builds by enabling fine-grained caching in the compilers
and linkers.

Short-term (this RFC): Add a builtin content-addressable storage (CAS) to
LLVM, designed to integrate with external CAS instances, to facilitate
experiments with using content-based caching in LLVM tools.

**TL;DR:** Let's give LLVM-based tools access to a CAS for storing structured
data, as foundation for experimenting with caching.

## Motivation

### Opportunity: Builds do redundant work

The computations and data in a build tend to be redundant, both within an
individual build and across subsequent builds. Within a build, multiple
compiler and linker invocations parse and process different subsets of the
same inputs, repeating many computations (such as type-checking, generating
code, and optimizing the same C++ template functions). Across consecutive
builds, usually at least one input will change, but often many of the
computations match between builds.

### Problem #1: Caching is hard

It's hard to cache computations internal to the compiler and linker. One
challenge is to define a cache key that is sound to use across different
contexts: filesystem inputs are specified by path, which change independently
of the input content; and some inputs are found implicitly, discovered only
after the computation begins. Another challenge is to avoid cache misses when
an input changes in a way that's irrelevant for a particular computation.

### Problem #2: Caching is expensive

It's expensive to store compiler and linker outputs, which limits the lifetime
of caches (thereby limiting their effectiveness). The main challenge is that
current output artifacts are monolithic, such that a small semantic change in
the input has a ripple effect across the binary representation of the output
artifact. Even when two artifacts are semantically similar, where most of the
represented data is fundamentally equivalent, it's hard to avoid storing that
data redundantly.

### Observation: Distributed build systems operate in the same space

Distributed build systems, such as Bazel and Buck, have a wider lens but
target the same opportunity. In particular, a single build system invocation
includes many compiler and linker invocations, which are largely redundant
across builds when many users ask for builds of the same software stack in
parallel.

These build systems use content-addressable storage (CAS) for storing inputs
and outputs of compiler and linker invocations. Using a CAS simplifies
Problem #1 (caching is hard) by factoring out cache invalidation. Storage cost
of inputs and outputs is amortized by scale, relying on objects in the CAS
being referenced many times to mitigate Problem #2 (caching is expensive).

### Leading questions

- Can compiler and linker invocations make internal use of a CAS to simplify
  Problem #1, and speed up builds by caching internal computations?
  - Where would minor refactoring enable existing algorithms to be cached?
  - Where would architectural changes allow more effective or finer-grained
    caching?
- Can we improve the schemas for compiler outputs / linker inputs to address
  Problem #2 for the compiler, linker, and build system?
- If the compiler, linker, and build system all use the same CAS, are there
  other opportunities that emerge?

## Proposal: an LLVMCAS library as a foundation for content-based caching

- Content-addressable storage (CAS) uses strong hashing to provide
  content-based UUIDs for arbitrary data. A CAS object can be structured,
  referencing sub-objects by UUID as part of their content, forming a DAG. If
  two objects (or sub-objects) have the same content, then they have the same
  UUID and are implicitly deduplicated. A prominent example of using a CAS for
  storage is Git, where each data structure is a CAS object.
  - Persistent caching across contexts is often easy when inputs and outputs
    are stored in a CAS. The action cache can be a key-value map from an
    action (which includes UUIDs of inputs) to a result (UUID of the output).
    For an action cache, there is no need to check validity of the cached
    result, since there is only a cache hit when it's correct.
- This library should provide abstractions for object storage (the CAS itself)
  and action caching, with a builtin implementation optimized for local use.
  Later, a plugin system can hook up external CAS instances (such as those
  used by distributed build systems).

Prototype contents (Apple fork branch `experimental/cas/main`):

- A few thread-safe data structures.
- Abstractions to model a CAS and an action cache.
  - Three object types: "Blob" (data), "Node" (data + references), "Tree"
    (map: name -> reference + kind).
  - Builtin CAS implementations for in-memory and on-disk.
- A thread-safe utility for caching lazily-discovered directories/files,
  supporting concurrent views that have different working directory state,
  used by:
  - Filesystem that reads from a CAS, and treats a CAS tree as the filesystem
    root.
  - Filesystem that provides an immutable view of the system filesystem
    (`sys::fs`), tracking accesses to enable creation of CAS trees.

### Open design questions raised in the RFC

- Should LLVMCAS exist at all, or should its pieces land in LLVMSupport?
- Does LLVMCAS need to support Windows immediately? (Proposal: no)
- Should the CAS object storage / action cache abstractions be stable? support
  plugins? (Proposal: evolve incrementally; plugins eventually, with a stable
  plugin interface)
- Is the serialization of CAS objects stable? the hash function? the
  persistent on-disk format? (Proposal: eventually, to allow tools with
  different revisions of LLVMCAS to talk to the same builtin CAS)
- Should clients be able to configure which stable serialization/hash to use?
  (Proposal: yes, eventually)

## Experiments

### Clang experiment: cache full compilation across worktrees

Adds `-fdepscan` and `-fdepscan-prefix-map` options to clang.

- `-fdepscan` causes the driver to launch / connect to a daemon that discovers
  and ingests `-cc1` inputs into the CAS. It adds the ID of the resulting CAS
  tree to the `-cc1` command-line, which will read from a `CASFileSystem`
  instead of the disk. As a result, the `-cc1` command-line lists all its
  inputs explicitly and can be used as a cache key for the outputs.
- `-fdepscan-prefix-map` applies a prefix map to the discovered CAS tree and
  the `-cc1` command-line. This allows cache hits across different worktrees
  (or build directories).

On macOS, clean builds with an empty/never-used CAS and action cache slow down
1-2%. Clean builds with a primed CAS and action cache are "fast" (hundreds of
compiles per second), with the build dominated by linking, running tablegen,
and process launch. One side effect of `-fdepscan-prefix-map` is that the
output is fully canonical (reproducible builds), even diagnostics.

### Clang experiment: cache raw tokenization

A `-cc1` option that turns on "raw token caching". Before entering a file, the
lexer first tokenizes it in "raw" mode (without the preprocessor) to create and
cache a binary token stream and raw identifier table. This result is cached
across compilations. On macOS this speeds up clean builds with an empty cache
by 1-2% -- enough to recover the overhead from `-fdepscan`.

### TableGen experiment: cache llvm-tblgen and clang-tblgen

Adds `-depscan` and `-depscan-prefix-map` to the TableGen executable, which
operate like the clang options above without any daemons. TableGen reverses the
`-depscan-prefix-map` when emitting diagnostics and `.d` files: inputs are
canonicalized with the prefix map, actions are cached using canonical results,
and then the cached output is de-canonicalized for tool output.

### Skip writing object file content, writing out CAS UUIDs instead

Reduce I/O during a build by writing out the contents of object files in a CAS
and then only writing the associated CAS ID references as the `.o` files on
disk.

- Clang: `-fcasid-output` makes clang write an embedded CAS ID as `.o` output,
  with special `file_magic` so other tools recognize it.
- llvm-libtool-darwin: writes out just the CAS ID in the static archive output
  instead of the full object file contents.
- ld64.lld: modified to read `.o` files with embedded CAS IDs.

### ld64.lld experiment: cache linking

`--fcas-cache-results` makes linking 2-stage:

1. Do a pass and record all the inputs relevant for the linker invocation into
   a CAS tree. This forms the cache key for the invocation.
2. If the associated linker output for the given cache key was recorded in the
   action cache then write out the cached output. Otherwise run the normal
   linker invocation and record its output in the action cache.

While doing the linker work in #2, lld only accesses data from the CAS tree
derived from #1; it doesn't read from the filesystem again. This ensures no
input file gets used that was not accounted for in the caching key. Combined
with CAS-ID object outputs, this showed ~23% reduction in build time for a
clean build of the clang executable with a primed CAS and action cache.

### CAS-optimized object file experiments: split object files into a DAG

Exploit the redundant semantic information in collections of object files to
reduce aggregate storage costs in a CAS. Also an opportunity to design compiler
outputs as a static linking format (what is convenient for emitters and
linkers) rather than as an executable format (what runtime loaders need). A
tool, `llvm-cas-object-format`, ingests object files into the CAS in various
ways and computes stats on aggregate storage cost. Two experimental schemas
show 2-4x smaller growth rate than native Mach-O when storing a series of build
directories for consecutive commits.

### Ideas for other experiments

- Make Clang "lazy" about parsing function bodies, but still type-check
  correctly if/when triggered later -- an initial step toward a demand-driven
  model (similar to Rust and Swift).
- Use relative source locations instead of absolute; isolate computations from
  absolute source locations (e.g. by virtualizing them) so computations can be
  cached even when code moves.
- Rework Clang module (`.pcm`) files for a CAS: strip validation information,
  split content (outline the AST block) to get redundancy between two
  compilations of a module with different command-line options but the same
  semantics; reference input files by content instead of full path.
- Bitcode's structure is hostile for fine-grained caching: instructions in a
  function reference global values based on their enumeration order, which
  changes independently of function bodies. Fix: enumerate for each function
  the globals it references (edge-list between globals) so instructions
  reference indexes into that function-local array -- isolating the function's
  instruction stream from module-level global ordering. Related: per-function
  constant pools; the same refactoring would let function passes run
  concurrently (no races on global use-lists).
- Debug info IR is hostile to CAS-optimized bitcode because function-specific
  metadata isn't known a priori to be function-local (e.g. a DISubprogram is
  pinned to a single function declaration) and is serialized in the global
  pool. Move ownership of such metadata to the function.

## Sketch of a long-term vision

- **Caching.** Speed up the compiler by isolating functional computations from
  filesystem and execution environment and modelling input discovery
  explicitly, caching functional computations based on explicit inputs in a
  CAS. Increase cache hits between related compiler invocations by caching
  fine-grained actions/requests that prune and canonicalize their inputs.
  Reduce cost of long-lived caches by using CAS-optimized bitcode and object
  file formats designed to share sub-objects. Speed up the linker by
  refactoring along similar principles.
- **Scheduling.** Improve scheduling of high-level sub-tasks by sending
  "discovered work" graphs to a toolchain daemon that's integrated with the CAS
  (suitable for distributed ThinLTO, implicitly-discovered explicitly-built
  Clang modules, or the new Swift driver). Reduce process overhead by designing
  a daemonization protocol for tools to adopt, allowing the toolchain daemon to
  send tasks via IPC rather than spawning a process. Empower the build system
  to schedule and distribute work via a toolchain API over IPC that can handle
  receiving graphs of "discovered work".
- **Interactive workflows (vs. artifact workflows).** Speed up tooling that
  doesn't need artifacts by transitioning workloads from waterfall to
  demand-driven, requesting only the observed results. Speed up interactive
  workflows (live editor tooling in clangd, edit-and-continue in JITLink,
  ultra-fast local incremental builds) by adding fine-grained dependency
  tracking for cacheable actions, enabling sound reuse of still-valid
  computations from a previous run (cf. Rust's incremental compilation and
  Swift's request evaluator).

## Related threads

- LLVMCAS Upstreaming (discourse.llvm.org/t/llvmcas-upstreaming/72696)
- LLVM bitstream integration with CAS (t/76757)
- Meta-RFC: Long-term vision for improving build times (t/89828)
- A pitch for future RFC proposing Integrated Distributed ThinLTO (t/69553)
