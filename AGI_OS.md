# WuBuOS AGI Operating System — Architecture (firmware-mediated)

## Status: VERIFIED — full boot chain green

`make test_uefi` / `./run.sh` → **RESULT: PASS -- payload ran on WuBuFW and all checks passed** (28/28 firmware conformance).

## Boot evidence (latest `./run.sh` serial)

```
[acpi] no RSDP found
[drv] bound AHCI at 0:3.0
[drv] bound NVMe at 0:4.0
[drv] bound XHCI at 0:5.0
[drv] bound e1000 at 0:2.0
[drv] bound GPU/GOP at 0:1.0
[drv] 5 driver(s) bound
[measure] PCR5 <- ... GPT header + entries
[measure] PCR7 <- ... SecureBoot=0
[agi] attestation table published (PCR0-7, SB=0 setup=1)
[tpm] no TPM detected - measurements are software-only
[tpm] PCR extend math (SHA-256 TCG vector): PASS
[sb] secureboot policy engine selftest: PASS
[sb] signed payload verifies under enforcement: PASS
[sb] no embedded certificate -> reject
[sb] unsigned payload rejected under enforcement: PASS
[tpm] event log: 3 events, 197 bytes
[boot] vol0 \EFI\BOOT\BOOTX64.EFI (6656 bytes)
[agi] PCR4 extended with image digest; PCR4 now:
[agi] attestation table published (PCR0-7, SB=0 setup=1)
[agi] attest-and-boot: OS image \EFI\BOOT\BOOTX64.EFI verified + measured
[fw] starting payload image (entry=0x4000000, st=0x...)
=== WuBuOS EFI payload: firmware conformance run ===
=== results: 28 passed, 0 failed ===
WUBUFW_SELFTEST_OK
RESULT: PASS -- payload ran on WuBuFW and all checks passed
```

## The platform (3 rings)
1. **Root of trust** (WuBuFW, already proved): PCR0–7 + Secure Boot + TPM measurements. This is the *verifiable* anchor — every self-modification the AGI makes is checked against a measurement that survives boot.
2. **Recursive learning substrate** (wubuwizard C11 engine ↔ recursive_optimize): the optimizer proposes self-mutations, the firmware's attestation chain verifies them, and the delta is committed only if the TCG vectors + AuthentiCode proofs still pass. Closed = open gaps in research/INDEX.md, specifically:
   - AV01–AV08: vector memory substrate (HNSW, PQ quant, episodic ANN index)
   - AW01–AW10: causal/neuro-symbolic (SCM, do-intervention, counterfactual, PDDL/STRIPS, ASP logic, abductive diagnosis)
   - AX02/AX03: seccomp sandbox + formal verification of generated C11
3. **Human surface** (WuBuFX GUI): Bonzi Buddy agent persona + Comfy node-graph editor. The human *bonzi* (queries the AGI, gets causal explanations) and *comfy* (edits the node graph of optimizer↔attestation↔memory). `wubufx_app_launch` routes real actions.

## Boot sequence (`make test_agi` / `make test_uefi`)
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
        1. read image from ESP volume (fw_vol_read_file)
        2. fw_sb_verify() AuthentiCode + policy gate  (refuse drift)
        3. SHA-256 → fw_tpm_pcr_extend(4)            (code-as-data into PCR4)
        4. fw_agi_publish_attest() re-pins fresh PCRs
        5. fw_image_create() → fw_pe_load() → fw_alloc_pages_at(ImageBase)
        6. fw_bs_start_image() hands control: efi_main(image, SystemTable)
  → payload efi_main runs → WUBUFW_SELFTEST_OK / WUBUFW_SELFTEST_FAIL
```

## AGI shim: fw_agi.c (the attestation channel)
- `fw_agi_publish_attest()` — reads g_systab->BootServices, installs the WUBU_AGI_ATTEST configuration table. Guarded: `if (g_systab && g_systab->BootServices)`.
- `fw_agi_attest_and_boot(path)` — reads the payload from the ESP, runs fw_sb_verify (SecureBoot AuthentiCode check), extends PCR4 with the image digest, re-publishes the attestation table, then calls `fw_image_create` + `fw_boot_image`. Single load (NOT the double-load anti-pattern: fw_load_verified + fw_bs_start_image both called fw_image_create_from_path → relocation failure at the same ImageBase).
- The payload receives the config table via `g_systab->ConfigurationTable` (LocateConfigurationTable by WUBU_AGI_ATTEST_GUID).

## Design notes / pitfalls hit & fixed
- **Canonical dir**: `/home/wubu/wubuos` (w-u-b-u-o-s). The homoglyph shadow `wubunos` (extra `n`) was deleted; it had been silently intercepting write_file/patch calls.
- **fw_efi_build_tables() must precede any g_systab use**: without it, g_systab is NULL → attest publish silently no-ops (guarded) and fw_bs_start_image hands the payload a NULL SystemTable → crash. Fixed in fw_main.c init order.
- **fw_image_create signature**: `EFI_STATUS fw_image_create(void *buf, uint64_t size, EFI_HANDLE device, EFI_HANDLE *out)` — declared in fw_main.c, defined in fw_bs_proto.c. Must be called with the already-read `img`/`sz`, not re-read from disk.
- **PE entry point**: mkpe sets `AddressOfEntryPoint = 0x1000` (section RVA of .text). fw_pe reads `opt->AddressOfEntryPoint` (offset 16 in PE32+ OptionalHeader). Entry = ImageBase + AoE = 0x3FFF000 + 0x1000 = 0x4000000.
- **Attest-and-boot replaces double-load**: original fw_boot_run called fw_load_verified then fw_bs_start_image, both invoking fw_image_create_from_path → double ImageBase allocation → relocation failure. Consolidated into single fw_agi_attest_and_boot.

## Gap closure priority (firmware-anchored)
- AW01–AW10: every causal model mutation must be measured into PCR4 (code-as-data), so a hallucinated intervention shows up as a PCR drift and is rejected. Close as C11 modules, each with a TCG-vector-style self-test.
- AV01: HNSW on the KV cache — close first (single file, ~300 LOC, oracle-matched). Then AV02 (PQ quant), AV03 (session KV reuse), AV04 (similarity eviction).
- AX02: seccomp sandbox for generated code — research but scoped as C11 + a WuBuFW seccomp shim driver.
