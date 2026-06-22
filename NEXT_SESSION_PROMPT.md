# Next Session Prompt — WuBuOS 2284-GAP Campaign

## Context
- **Project**: WuBuOS — ZealOS kernel + Win98 shell + Styx/9P + Arch containers
- **State**: Phase 13 — 2284 REAL_GAPs identified by automated form-vs-function audit
- **Location**: /home/wubu/.hermes/profiles/mind-palace/home/myseed/
- **Build**: `make all` | Test: `make test_XXX` (58 targets, 747+ assertions passing)

## Immediate Action
```bash
cd /home/wubu/.hermes/profiles/mind-palace/home/myseed
cat BATTLESHIP.md | head -100  # Review priority matrix
# Pick ONE gap from Critical tier
# Write real C implementation
# make test_<relevant>  # Verify
# make all              # Verify build
# Repeat
```

## Priority Pick (Critical Tier)
**Runtime Stack (996 gaps) → Start with wubu_oci.c (84 gaps)**

| File | Gaps | First Gap to Close |
|------|------|-------------------|
| wubu_oci.c | 84 | oci_config_from_json — parse OCI config from JSON |
| wubu_network.c | 122 | bridge_create — netlink bridge via `ip link add type bridge` |
| wubu_snapshot.c | 82 | snapshot_mount — actual `mount -t overlay` |
| wubu_holyd.c | 75 | holyd_session_save — fix path truncation |
| wubu_vsl.c | 72 | vsl_elf_load — PT_LOAD segment loading |

## Files to Inspect First
```bash
# OCI Runtime — all 17 functions are stubs returning -1
cat src/runtime/wubu_oci.c | head -200

# Network — 122 gaps, need netlink/ioctl for real bridge/vxlan/wg
cat src/runtime/wubu_network.c | grep -n "TODO-NET\|return -1" | head -20

# Snapshot — 82 gaps, need real overlay mount
cat src/runtime/wubu_snapshot.c | grep -n "TODO\|return -1" | head -20
```

## Test Targets to Verify
```bash
make test_oci       # 10/10 passing (but all functions are stubs!)
make test_network   # 139/139 passing (but 122 gaps remain)
make test_snapshot  # 132/132 passing (but 82 gaps remain)
make test_holyd     # 31/30 passing (but 75 gaps remain)
make test_bridge    # 25/25 passing (VSL syscalls)
```

## Mantra
> "Rewriting from scratch in C is the point. Form ≠ Function = REAL_GAP."
> 
> Every `(void)param;`, empty `{}`, `return -1` no-logic, TODO stub = gap to close.
> No "for later", no "scaffolding", no "stub for extension".

## Triple DA Reminder
1. **"2284 gaps = 2284 functions?"** — Graded. Many repetitive patterns, but ALL are "rewrite in C" work.
2. **"Previous 300 was accurate?"** — FALSE. Missed 700+ void casts, 500+ return -1, 300+ empty bodies, 200+ wm_nano TODOs.
3. **"123K LOC is real?"** — NO. ~23K stub/scaffold. Real work ≈ 100K. TempleOS 154K → need 54K more real C.
4. **"Runtime 996 = whole userspace?"** — YES. Container/daemon/network/OCI/snapshot = entire OS layer above kernel.

## Environment
- Python: python3=3.11, uv installed, venv available
- WSL2: 14GB RAM, /mnt/c for Windows files
- No internet for packages — use local deps or vendor

## Hermes Profile
- Active: mind-palace
- Skills: systems-programming, wubuos-masterpiece-architecture, wubuos-zealos-parity, wubuos-container-isolation, wubuos-jit-self-hosted, wubuos-wayland-migration, wubuos-fable-porting
