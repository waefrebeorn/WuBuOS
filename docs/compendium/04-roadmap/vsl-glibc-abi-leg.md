# VSL glibc/ld.so ABI leg — verified negative and bounded increment

## What landed

`vsl_exec_engine` now has an OPT-IN bounded PT_INTERP path
(`vsl_dyn_set_interp(1)`; per-image `VslImageCfg.rootfs` or process-wide
`vsl_dyn_set_rootfs()`):

* strict PT_INTERP parse (absolute, NUL inside `p_filesz`, length <= 256,
  no `..`/`.`/`//` components);
* re-rooting of the absolute path inside an allow-listed **guest rootfs**,
  `realpath()`-confined (no host-root reads);
* the interpreter is mapped and relocated with the **same** bounded ET_DYN
  loader as DT_NEEDED deps, and its `DT_INIT`/`DT_FINI` join the existing
  init/fini owner — interpreter first, deps leaf-first, main last; fini in
  reverse;
* kernel-shaped auxv on the guest stack: `AT_PHDR`, `AT_PHENT`, `AT_PHNUM`,
  `AT_BASE` (interpreter bias), `AT_ENTRY` (main entry), `AT_PAGESZ`,
  `AT_RANDOM`, `AT_NULL`. Control starts at the interpreter entry.

All of it is OFF by default; every pre-existing guest keeps its old behavior.
`test_vsl_exec` gate (w) exercises the real path end to end with a freestanding
`ET_DYN` interpreter (`/libvslinterp.so`) and a synthetic `ET_DYN` main that
carries PT_INTERP + DT_INIT/DT_FINI; the interpreter reads its own auxv and
emits exact tag/value pairs, which the gate compares to the main image's real
link geometry.

## Verified negative: full glibc ld.so run is infeasible

Evidence — exact commands and output from this checkout (base 8c0c04b):

```
gcc --version                      -> gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
$ gcc -O2 -Wl,--build-id=none src/runtime/vsl/exec_guests/guest_glibc_dyn.c \
      -o src/runtime/vsl/exec_guests/guest_glibc_dyn.elf
file guest_glibc_dyn.elf           -> ELF 64-bit LSB pie executable, x86-64,
                                      dynamically linked,
                                      interpreter /lib64/ld-linux-x86-64.so.2
readelf -p .interp                 -> /lib64/ld-linux-x86-64.so.2
readelf -d | grep NEEDED           -> Shared library: [libc.so.6]
readelf -d | grep INIT/FINI        -> DT_INIT 0x1000, DT_FINI 0x113c,
                                      DT_INIT_ARRAY 0x3df0 (8),
                                      DT_FINI_ARRAY 0x3df8 (8)
readelf -r .rela.dyn               -> R_X86_64_GLOB_DAT __libc_start_main@GLIBC_2.34,
                                      __cxa_finalize@GLIBC_2.2.5, _ITM_*, __gmon_start__
readelf -r .rela.plt               -> R_X86_64_JUMP_SLOT puts@GLIBC_2.2.5
host ld.so copied into the guest rootfs:
  sha256 c20a2dc8917c755f02b94049356320fe1f62ac7d9f8994731f807d9df39302da
```

Gate (v), exact output:

```
[gate v] old loader: rc=-11 status=-11 interp_present=1
[gate v] new loader: rc=-13 status=-13 interp_present=1 \
  path="/lib64/ld-linux-x86-64.so.2" resolved=1 \
  reason="interpreter needs unsupported relocations/symbols (glibc ld.so ABI unmodeled)"
```

* pre-change loader (PT_INTERP ignored) rejects the real binary with
  `VSL_EXEC_ERR_RELOC` (-11): `__libc_start_main` is absent from the fixed
  host symbol table. This is the **negative control**.
* new loader recognises and bounds the exact interpreter path, resolves it
  **only** inside the guest rootfs, then refuses a full glibc ld.so run with
  `VSL_EXEC_ERR_INTERP` (-13) and the exact reason above.

Why a real glibc ld.so cannot run in this engine today:

1. ld.so has its own relocation/symbol ABI (versioned undefined symbols,
   `R_X86_64_COPY`/`IRELATIVE` resolvers, `_dl_*` exports) outside the small
   fixed host table; the bounded loader fails closed rather than guess.
2. glibc needs `%fs`/TCB via `arch_prctl`, a real TLS module layout and
   `__libc_csu_init`; the engine's TLS model is host-block-only and there is
   no TCB. The seccomp window also does not cover ld.so's early bootstrap
   once it maps libc at an arbitrary host-chosen bias.
3. `LD_LIBRARY_PATH`-like search, version-name preference across deps and
   the full syscall set glibc expects are unmodeled.

No fabricated success is claimed: the loader returns -13 with the recorded
reason, and gate (v) asserts that exact negative.

## Next increment (branch leg/glibc-full from 50be823): arch_prctl/TCB, real
## module-id TLS, interpreter DT_VERSYM self-preference, ordered rootfs search

This leg implements the four bounded pieces that were named as unmodeled
above, WITHOUT claiming a full glibc ld.so run.  Every one is OFF by default
or changes only the opt-in PT_INTERP path, so all pre-existing guests are
unchanged (204 baseline gates still pass; full gate now 236/236).

1. **arch_prctl(ARCH_SET_FS) + TCB** (`vsl_dyn_set_fs_tls(1)`, opt-in).  A
   guest `arch_prctl` trapping into the VSL seccomp window is handled by the
   engine: ARCH_SET_FS validates the requested base is inside the VSL user
   window, seeds a minimal x86-64 TCB there (self pointer at `%fs:0`, dtv at
   `%fs:8`) and switches the running thread's FS base for guest execution.
   Host libc runs on the saved host FS (the handler is `no_stack_protector`
   and swaps FS around dispatch); ARCH_GET_FS returns the recorded base; an
   unknown option fails closed with -EINVAL.  Gate (x) runs a real freestanding
   guest that installs its own TCB and reads `%fs:0/%fs:8` back; fs_tls OFF is
   the negative control (unhandled syscall, no TCB).

2. **Real module-id TLS layout.**  Every loaded image is registered in a
   bounded module table with a stable id: main = 1 (so the existing TLS gate
   (o) is unchanged), interpreter = 2, dependencies = 3+ in load order.
   R_X86_64_DTPMOD64 now writes that image's real id instead of the constant
   1, and each image uses its own TLS block (the interpreter has a dedicated
   slot).  The seeded TCB's dtv array maps module index to real TLS-block
   base; gate (x) reads dtv[0]/dtv[1] and asserts they match the live layout.

3. **DT_VERSYM preference for the interpreter's own symbols.**  The
   interpreter's exported dynamic symbols are registered BEFORE it is
   relocated, and while it self-relocates its own versioned definitions
   outrank deps and its DT_VERNEED vn_file no longer filters them out (real
   ld.so self-relocation).  Gate (y) hand-builds an ET_DYN interpreter with a
   versioned self-definition and a versioned self-import naming a different
   file: it relocates only through its own definition, and emits an exact
   magic proving the binding.  A need-version mutation fails closed.  This
   gate is a true differential: disabling the self-preference drops the total
   from 236/236 to 233/236.

4. **Ordered rootfs search list** (`vsl_dyn_add_rootfs()` / per-image
   `VslImageCfg.rootfs_dirs`).  Directories are searched in call order for
   the PT_INTERP path, first confinement-passing hit wins (an
   LD_LIBRARY_PATH-like search).  Gate (z) places the interpreter only in a
   second dir, proves a two-dir walk, proves reversed order stops at one hit,
   proves a no-interp list fails closed, and exercises the process-wide list.

### Verified negative retained (unchanged)

Gate (v) still runs the REAL dynamically linked gcc fixture
(`guest_glibc_dyn.elf`, plain gcc, PT_INTERP
`/lib64/ld-linux-x86-64.so.2`, DT_NEEDED libc.so.6, versioned GLOB_DAT/JUMP_SLOT
imports).  Pre-change loader: `rc=-11 status=-11` (ERR_RELOC, negative
control).  New loader: `rc=-13 status=-13` with
`reason="interpreter needs unsupported relocations/symbols (glibc ld.so ABI
unmodeled)"`.  A small libc-using binary still cannot complete a full glibc
ld.so run: the remaining reasons are now narrower but real (see below).

### Remaining gaps (NOT closed by this leg)

* A genuine glibc ld.so still cannot run: its own IRELATIVE/COPY resolvers and
  `_dl_*` symbol ABI remain outside the bounded loader, and seccomp coverage
  of ld.so's early libc-mapping bootstrap at a host-chosen bias is unmodeled.
  Gate (v) asserts this exact negative; nothing here fabricates success.
* The TCB is a minimal self/dtv header, not a full glibc `tcbhead_t`
  (sysinfo, gscope_flag, pointer guards, multiple_threads are unmodeled).
* The dtv is a bounded host array, not glibc's malloc-backed dtv with dynamic
  TLS surpluses; only the three already-supported TLS relocation types work.
* No `TLSGD/TLSLD/GOTTPOFF/TLSDESC` models, no `ARCH_SET_GS`, no real
  `LD_LIBRARY_PATH` env parsing (the ordered list is API-configured), and no
  multi-threaded FS switching (the engine is single-threaded by contract).
* The ordered rootfs list bounds the interpreter search only; DT_NEEDED still
  uses the separate per-image directory list, not a unified search path.
