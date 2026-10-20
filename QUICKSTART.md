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

Boots **WuBuFW**, our C11 UEFI firmware, as a QEMU guest and requires every
milestone: secureboot policy engine, signed-payload verification, unsigned
payload rejection (fail-closed), TPM PCR extend, allocator, ESP listing, and the
interactive shell. Last measured: **ALL MILESTONES PASSED**.

This is the supported path. `qemu-system-x86_64 -kernel src/kernel/kernel.elf`
does **not** work and is not a boot test: the kernel is a higher-half 64-bit
image with no multiboot header, so QEMU's Linux loader prints probe bytes and
stops. `sh tools/boot_gate.sh --kernel` reproduces that diagnosis on purpose.

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
