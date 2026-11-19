# WEMU user-mode overlay (L14)

`src/wemu` is a git submodule pinned at **public main** (`24c625b`).
The user-mode leg — `wemu_elf.c` (static ELF loader + SysV stack),
FS/GS TLS segment bases, the SSE2/SSSE3/AVX2 growth set, the
busybox-found decoder fixes, and `wemu_cpu_halt` — is committed in the
wemu repo on branch `leg/user-mode-vsl` (commits `a9ee4bd`, `bf4e0b7`),
which is **local-only** for this leg (nothing pushed).

## Mechanism (honest, reproducible)

`wemu-usermode.patch` in this directory is the exact
`git format-patch 24c625b..bf4e0b7` series from the wemu repo.
`mk/wemu_user.mk` builds WEMU out-of-tree:

1. copy `src/wemu/{src,include}` into `build/wemu-user/`
2. `git apply` the patch there (submodule checkout stays pristine)
3. compile `libwemu-user.a` with C11 `-Werror`

The bridge (`src/runtime/wubu_wemu_bridge.c`) and its gate
(`make test_wemu_busybox`) link that library. No vendored duplicate
sources, no token URLs, no detached-HEAD commits in the submodule.

## Integrator reconciliation

Push wemu `leg/user-mode-vsl`, bump the `src/wemu` submodule to it,
then delete the patch + apply step (build in-tree or keep the copy
rule). The patch file is the single source of truth until then; if the
wemu branch gains commits, regenerate it with the same command.

## Ownership

- WEMU owns instruction semantics + guest architectural state.
- VSL owns the ONE Linux syscall table (`vsl_syscall_dispatch`).
- The bridge owns only the guest<->host ABI boundary: pointer
  marshalling (VSL handlers are host-backed), arch_prctl -> guest
  FS/GS base (never the host's), exit -> halt + capture.