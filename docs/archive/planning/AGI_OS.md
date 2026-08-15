# WuBuOS AGI Operating System — Architecture (firmware-mediated)

## Status: VERIFIED — REAL kernel boots on WuBuFW (measured boot chain green)

- `make test_uefi` — conformance payload on WuBuFW: **28/28 PASS** (regression).
- `make test_agi_metal` — the REAL bare-metal AGI kernel (`src/kernel/kernel.elf`)
  boots as a WuBuFW measured payload: **RESULT: PASS — measured boot chain green**.
  Full chain: firmware measures+verifies the chainloader (PCR4 + AuthentiCode)
  → loader reads KERNEL.ELF off the ESP, SHA-256s it, stashes the attestation
  handoff in low RAM → ExitBootServices → 32-bit teardown → crt0 → kernel_main
  consumes the attestation → AGI supervisor boots with the root-of-trust gate
  LIVE. `wubu_attest.{c,h}` (kernel) ↔ `fw_agi_attest.h` (shared wire format)
  ↔ `src/firmware/loader/` (chainloader) is the closed loop.

## Boot evidence (real kernel, latest `make test_agi_metal` serial)

```
[agi] attestation table published (PCR0-7, SB=0 setup=1)
[sb] secureboot policy engine selftest: PASS
[boot] vol0 \EFI\BOOT\BOOTX64.EFI (6656 bytes)
[agi] PCR4 extended with image digest; PCR4 now: 02B03ABC...
[agi] attest-and-boot: OS image \EFI\BOOT\BOOTX64.EFI verified + measured
[fw] starting payload image (entry=0x4000000)
[wubufw-loader] WuBuOS chainloader v1
[wubufw-loader] attestation table: FOUND (boot=0 sb=0 pcr4=02B03ABC...)
[wubufw-loader] reading \EFI\BOOT\KERNEL.ELF
[wubufw-loader] ELF64 ok: phnum=1 entry=FFFFFFFF8010012C
[wubufw-loader] loaded 1 PT_LOAD at 0x100000 (BSS zeroed)
[wubufw-loader] kernel sha256=989983AE403D01FD15A66FDC6194ECB5131651BDF45C62A61EB4BD87C6022517
[wubufw-loader] handoff @ 0x91000 (attest=92000)
[wubufw-loader] boot services exited (0) -- handing control to kernel @ 10012C
MPGCEWXYJKSIWuBuOS: 64-bit long mode entered, stack up
QZAhiWuBuOS: kernel_main entered (long mode OK)
jWuBuOS: BSS zeroed
WuBuOS: firmware attestation consumed (boot=0 sb=0 setup=1 pcr4=02B03ABC...)
kqrWuBuOS: heap initialized (64 MB)
B1ipqlmnogWuBuOS: interrupts initialized
2WuBuOS: VBE initialized (1920x1080)
33WuBuOS: GAAD viewport decomposed (1920x1080, 6 regions)
45WuBuOS: input/PS2 initialized
6WuBuOS: tasking initialized
78WuBuOS AGI: GAAD viewport 1920x1080 -> 34 regions
WuBuOS AGI: firmware attestation VALID (root of trust live)
WuBuOS: AGI kernel booted (regions=34)
9WuBuOS AGI: agent realm task spawned (ok)
```

## The platform (3 rings)
1. **Root of trust** (WuBuFW, already proved): PCR0–7 + Secure Boot + TPM measurements. This is the *verifiable* anchor — every self-modification the AGI makes is checked against a measurement that survives boot.
2. **Recursive learning substrate** (wubuwizard C11 engine ↔ recursive_optimize): the optimizer proposes self-mutations, the firmware's attestation chain verifies them, and the delta is committed only if the TCG vectors + AuthentiCode proofs still pass. Closed = open gaps in research/INDEX.md, specifically:
   - AV01–AV08: vector memory substrate (HNSW, PQ quant, episodic ANN index)
   - AW01–AW10: causal/neuro-symbolic (SCM, do-intervention, counterfactual, PDDL/STRIPS, ASP logic, abductive diagnosis)
   - AX02/AX03: seccomp sandbox + formal verification of generated C11
3. **Human surface** (WuBuFX GUI): Bonzi Buddy agent persona + Comfy node-graph editor. The human *bonzi* (queries the AGI, gets causal explanations) and *comfy* (edits the node graph of optimizer↔attestation↔memory). `wubufx_app_launch` routes real actions.

## Boot sequence (`make test_agi` / `make test_uefi` / `make test_agi_metal`)
```
WuBuFW firmware
  → firmware self-tests (ConInit, Mem, Time, PCI, ACPI, drivers)
  → fw_efi_build_tables() assembles EFI_SYSTEM_TABLE (sets g_systab)
  → fw_efi_register_media() exposes BlockIO + SimpleFileSystem (ESP mounted)
  → fw_measure_secureboot() pins PCR0–7, SecureBoot state
  → fw_agi_publish_attest() installs EFI_CONFIGURATION_TABLE(WUBU_AGI_ATTEST_GUID)
        carrying {magic, version, pcr[8][32], sb_enabled, sb_setup_mode, boot_count}
  → fw_boot_run() → shell (interactive) / auto-boot
  → user 'exit' (or bootpath) → fw_agi_attest_and_boot(path):
        1. read image from ESP volume (fw_vol_read_file)          [the chainloader]
        2. fw_sb_verify() AuthentiCode + policy gate  (refuse drift)
        3. SHA-256 → fw_tpm_pcr_extend(4)            (code-as-data into PCR4)
        4. fw_agi_publish_attest() re-pins fresh PCRs
        5. fw_image_create() → fw_pe_load() → fw_alloc_pages_at(ImageBase)
        6. fw_bs_start_image() hands control: efi_main(image, SystemTable)
  → [wubufw-loader] efi_main (src/firmware/loader/loader.c):
        1. locate WUBU_AGI_ATTEST config table (PCR0-7 + SB state)
        2. read \EFI\BOOT\KERNEL.ELF via EFI file protocol (up to 4MB)
        3. validate ELF64, load every PT_LOAD at its physical LMA (0x100000),
           zero BSS
        4. SHA-256 the kernel ELF → handoff block @ 0x91000 (ptr @ 0x90040):
           {magic, version, kernel_size, kernel_sha256[32], attest_addr@0x92000}
        5. ExitBootServices(image, mapkey) → 64→32 protected-mode teardown
           (teardown.S: clear CR0.PG, CR4.PAE) → jump to physical _start
  → crt0.S builds its own tables (PT_high maps 0xffffffff80100000 → 0x100000)
  → kernel_main: wubu_attest_load_scratch() consumes the handoff
  → AGI supervisor: root-of-trust gate LIVE — cycle() refuses promotion
    without a valid firmware attestation (no attestation → no self-modification)
```

## AGI shim: fw_agi.c (the attestation channel)
- `fw_agi_publish_attest()` — reads g_systab->BootServices, installs the WUBU_AGI_ATTEST configuration table. Guarded: `if (g_systab && g_systab->BootServices)`.
- `fw_agi_attest_and_boot(path)` — reads the payload from the ESP, runs fw_sb_verify (SecureBoot AuthentiCode check), extends PCR4 with the image digest, re-publishes the attestation table, then calls `fw_image_create` + `fw_boot_image`. Single load (NOT the double-load anti-pattern: fw_load_verified + fw_bs_start_image both called fw_image_create_from_path → relocation failure at the same ImageBase).
- The payload receives the config table via `g_systab->ConfigurationTable` (LocateConfigurationTable by WUBU_AGI_ATTEST_GUID).

## Design notes / pitfalls hit & fixed
- **Canonical dir**: `/home/wubu/wubuos` (w-u-b-u-o-s). The homoglyph `wubunos` (extra `n`) is now a SYMLINK to `/home/wubu/wubuos` (created 2026-08-06) — a misspelled path resolves to the real repo instead of silently spawning a shadow directory (a shadow dir intercepted write_file/patch calls before). ALWAYS write `wubuos`; never `rm -rf /home/wubu/wubunos/` (trailing slash follows the symlink and would delete the real repo).
- **fw_efi_build_tables() must precede any g_systab use**: without it, g_systab is NULL → attest publish silently no-ops (guarded) and fw_bs_start_image hands the payload a NULL SystemTable → crash. Fixed in fw_main.c init order.
- **fw_image_create signature**: `EFI_STATUS fw_image_create(void *buf, uint64_t size, EFI_HANDLE device, EFI_HANDLE *out)` — declared in fw_main.c, defined in fw_bs_proto.c. Must be called with the already-read `img`/`sz`, not re-read from disk.
- **PE entry point**: mkpe sets `AddressOfEntryPoint = 0x1000` (section RVA of .text). fw_pe reads `opt->AddressOfEntryPoint` (offset 16 in PE32+ OptionalHeader). Entry = ImageBase + AoE = 0x3FFF000 + 0x1000 = 0x4000000.
- **Attest-and-boot replaces double-load**: original fw_boot_run called fw_load_verified then fw_bs_start_image, both invoking fw_image_create_from_path → double ImageBase allocation → relocation failure. Consolidated into single fw_agi_attest_and_boot.

## Gap closure priority (firmware-anchored)
- AW01–AW10: every causal model mutation must be measured into PCR4 (code-as-data), so a hallucinated intervention shows up as a PCR drift and is rejected. Close as C11 modules, each with a TCG-vector-style self-test.
- AV01: HNSW on the KV cache — close first (single file, ~300 LOC, oracle-matched). Then AV02 (PQ quant), AV03 (session KV reuse), AV04 (similarity eviction).
- AX02: seccomp sandbox for generated code — research but scoped as C11 + a WuBuFW seccomp shim driver.
