<!-- GENERATED FILE -- do not edit by hand.
     Run `make docs` (tools/gen_docs.py) to regenerate. -->

# Parity (compiled binaries across OSes)
> Generated 2026-08-02 17:06 UTC -- the PARITY project: the hosted layer is the scaffold that must run on Linux/Windows/macOS.

## Hosted core (portable abstraction)
wubu_metal.c, wubu_metal.h

## OS legs (per-platform backends)
- `wubu_metal_audio.c`
- `wubu_metal_drm.c`
- `wubu_metal_evdev.c`
- `wubu_metal_test.c`
- `wubu_metal_vulkan.c`
- `wubu_metal_x11.c`

## Toolchain
- CC: `gcc`

## Portability matrix (evidence of record)
| Platform | Build | Runtime legs | Status |
|----------|-------|--------------|--------|
| Linux (this host) | `make runtime tools` -> **PASS** | wubu_metal_audio, wubu_metal_drm, wubu_metal_evdev, wubu_metal_test | VERIFIED |
| Windows (WSL host / native) | cross build (see 00-philosophy/cross-platform-build.md, gap K3) | core + win32 leg (planned) | CONFIG |
| macOS | cross build (see 00-philosophy/cross-platform-build.md, gap K4) | core + metal leg (see wubuos-macos-leg-proof) | CONFIG |
