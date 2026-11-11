# WuBuOS — Build, Run, Test

**Verified 2026-11-11.** Every command below was run against this tree on this
date. `make test` passes 512 targets with 0 errors and 0 segfaults. `test_docs`
fails the build if a path in this file stops existing.

This is the practical guide for a **developer or a user** of WuBuOS. Short
answer: `make all` to build, `make test` to prove it, `make qemu` to boot it.

---

## 1. Get the code

```bash
git clone https://github.com/waefrebeorn/WuBuOS.git
cd WuBuOS
git submodule update --init --recursive   # brain, container, office, pad, www, wet, vws, compiler
```

The repo mounts several other ~WuBu~ repos as submodules (wubuwizard, WWW, WET,
VWS, WuBuPad, WuBuOffice, WuBuContainer). The ecosystem map is
[docs/ECOSYSTEM.md](ECOSYSTEM.md).

## 2. Dependencies

| tool | for | required |
|---|---|---|
| `gcc` (C11) and `make` | everything | yes |
| `python3` | doc generation, some tools | yes |
| `qemu-system-x86_64` | booting the image (`make qemu`) | to run, not to build |
| `nvcc` (CUDA) | the Bear/Vulkan compute paths | optional |
| Vulkan SDK / loader | the live VKD3D and GPU tests | optional (those tests are separate targets, not in the gate) |

Missing optional tools are fine: the gate does not need a GPU or Vulkan.

## 3. Build

```bash
make all        # the whole tree, per-subsystem, in parallel
```

`all` = `kernel jit compiler runtime tools gui bridge apps worldsim metal
audio shell bear hosted_objs`. Per-subsystem targets exist too:
`make kernel`, `make jit`, `make compiler`, `make runtime`, `make gui`,
`make apps`, `make shell`, `make worldsim`, `make metal`, `make audio`,
`make bridge`, `make tools`. An edit to one `.c` recompiles only that object.

Runnable outputs:

| make target | produces | what it is |
|---|---|---|
| `make shell` | `src/shell/wubu_shell` | the runnable shell binary |
| `make kernel` | `src/kernel/kernel.elf` | the bare-metal kernel |
| `make iso` | `src/kernel/wubuos.iso` | a bootable GRUB-multiboot image |

## 4. Run

```bash
# a shell binary runs on Linux today:
make shell && ./src/shell/wubu_shell

# boot the real kernel in QEMU (serial stdio, self-terminating):
make iso
make qemu
```

`make qemu` runs `qemu-system-x86_64 -cdrom src/kernel/wubuos.iso` with a
debug-exit device, so it terminates on its own — safe in CI.

## 5. Test

```bash
make test        # the full gate: 512 targets, all tiers, sequential
```

See [TESTING.md](../TESTING.md) for running one test (`make test_<name>`), a
tier, and how the gate works.

## 6. The extras a developer reaches for daily

```bash
make spells       # list the spell book (reusable modules, e.g. vkd3d, swarm)
make spellcheck   # validate the spell book (fail-closed)
make docs         # regenerate the compendium reference
make test_agi_metal   # the measured-boot/AGI gate (the root of trust)
make test_uefi        # firmware conformance boot
make test_secureboot  # Secure Boot enrollment + enforcement
make test_rsa         # real RSA-2048 firmware signature enforcement
```

## 7. Honest state (2026-11-11)

- **5,067** C/H files, **750,001** lines of C in `src/`, **512** test targets.
- `npm`-style tool registry and the container/network/Proton subsystems are
  real Linux-hosted code — the names are the same, the engines are ours.
- Features that are planned or partial are marked as such in
  [`docs/MIND_PALACE.md`](MIND_PALACE.md) and `vault/planning/`, never
  presented here as done.
