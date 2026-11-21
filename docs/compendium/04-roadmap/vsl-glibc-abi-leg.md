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
