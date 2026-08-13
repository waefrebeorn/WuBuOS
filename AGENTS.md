# AGENTS.md — wubuos (THE BODY)

> Agent context file. Read this before working in this repo. Kept current;
> update it when the structure changes. Humans: this is your onboarding too.

## What this repo is

WuBuOS is an AGI kernel: a TempleOS-style ring-0 kernel (ZealOS lineage)
with a Win98-style GUI shell, a Styx/9P-namespace filesystem, and a hosted
test leg. The AGI (Colonel) is the OS.

## Directory map

```
src/kernel/            the kernel proper (freestanding C11)
  *.c                  subsystems + device drivers (one file per module)
  *.h                  public surfaces (opaque-ish, accessor-only)
  test/                hosted SELFTESTS (run as user-space binaries)
build/                 ALL build output (gitignored):
  obj/  dep/  kernel/  kernel build intermediates + kernel.elf
  testobj/             cached test objects (fast incremental rebuilds)
  testbin/             test_hw_* binaries
  bear/  tools/        Bear RL + GUI-shot binaries (not source)
tools/                 build/test generators + Python helpers
mk/                    Makefile fragments (objects.mk, tests.mk, ...)
docs/                  design docs + compendium + driver bank
research/              wave research notes (Kevin-Bacon 7-hop)
```

## Conventions (the C11 contract)

- Kernel sources are **freestanding**:
  `-ffreestanding -nostdlib -nostartfiles` — no libc. Only `access()`/`R_OK`
  link (wrapped in `#ifdef _GNU_SOURCE`).
- Test builds define `_GNU_SOURCE` (full libc available); kernel builds do
  **not**. Any libc I/O call (`access`/`opendir`/`readdir`/`fopen`/`fclose`)
  must be inside `#ifdef _GNU_SOURCE`.
- Comments must **never contain `*/`** — kernel paths like `/sys/class/hwmon/hwmon*`
  must be rewritten to drop the `*​/` (else the block comment closes early).
- Opaque structs + accessor functions (Colonel-level design). No exported
  mutable globals across modules.
- Memory from the kernel heap via `mem_alloc`/`mem_free` (declared in `libc.h`).
  Tests call `libm_heap_init()` (bump allocator in `libc.c`) before any
  `calloc`/`malloc`.

## Adding a module (the 6-place rule)

1. `src/kernel/wubu_<mod>.c` + `.h` (+ `<mod>_selftest.c` in `test/`)
2. Include + probe call in `wubu_hw_detect.c`
3. Add `.o` to `mk/objects.mk` (kernel build)
4. Add `.c` to `/tmp/core_modules.txt` ONLY if it's host-testable (no metal-only
   deps) — then regenerate `mk/tests.mk` via `tools/regenerate_tests_mk.py`
5. Format-string + args in `wubu_probe.c` / `wubu_probe_matrix.c`
6. Matrix fragment check in `test/wubu_probe_selftest.c`

**Console commands** (`cmd_*`) go in `wubu_console_cmds.c` (NOT the test cache —
they call metal interrupt/serial functions). The dispatcher is `wubu_console_exec`
in `wubu_console.c`.

## Testing

See `TESTING.md`. Fast incremental: `make test_hw_<mod>` builds/runs one test;
`bash tools/run_hw_tests.sh` runs all 215. Full green gate = 215/215 PASS,
0 FAIL, 0 BUILD_ERR, 0 CRASH.
