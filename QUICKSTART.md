# WuBuOS — quickstart (verified)

Every command below was run on this tree; the ones marked **known failing** fail
today, and the failure is documented rather than hidden. `make onboard-check`
runs this list and reports.

## Prerequisites

- `gcc`, `make`, `qemu-system-x86_64`
- ~2 GB disk for the test binaries

Check them:

```sh
command -v gcc make qemu-system-x86_64
```

## 1. Build the kernel

```sh
make
```

Artifact: `src/kernel/kernel.elf` (~1.9 MB).

## 2. Verify it is freestanding

```sh
make boot-audit
```

Passes when the ELF has no interpreter, no shared libraries, no dynamic segment,
no undefined symbols, and a fixed entry point.

## 3. Run one test

```sh
make test_timer_wheel
```

Expected: `=== test_timer_wheel: 17 checks, 0 failures ===`.

## 4. Run the whole body gate

```sh
make test_all
```

445 targets. This is the gate that matters: `make test` alone only reaches a
subset (it used to reach 131 of 446).

## 5. Boot it

```sh
make boot-gate
```

**Known failing.** The gate requires the serial banner
`WuBuOS: kernel_main entered (long mode OK)`. Today the VM stops before it,
printing only probe bytes (`010203040506070811121314`), and exits non-zero with a
diagnosis. `make qemu` is bounded to 30 s for the same reason — it used to hang
the terminal forever.

This is the top open item: the boot chain never reaches `metal_main`. What is
known:

- the artifact is freestanding and its entry point is `0xffffffff8010012c`;
- `qemu-system-x86_64 -kernel src/kernel/kernel.elf` reaches "Booting from ROM"
  and prints only the probe bytes;
- the floppy path (`make qemu`) produces no kernel output at all.

## 6. Check onboarding itself

```sh
make onboard-check
```

Runs steps 1–4 and the boot gate, and fails if any *documented as passing* step
does not pass. Step 5 is reported but does not fail the check while it is listed
as known failing — when it starts passing, the script says so and tells you to
move it out of the known-failing list.

## Layout

| path | what |
|---|---|
| `src/kernel/` | kernel, entry in `metal_main.c`, boot chain in `boot.S`/`loader/` |
| `src/runtime/` | 9P (styx), VSL/NT personality, containers, exec backends |
| `src/gui/`, `src/apps/` | Win98 shell, window manager, applications |
| `mk/tests.mk`, `mk/tests_all.mk` | 445 test targets and the aggregate |
| `tools/boot_gate.sh` | the boot claim that can fail |
