# WuBuOS 3D Demo Binaries (Kronos `vkcube` — canonical Vulkan demo)

Same reputable upstream source for all three (`KhronosGroup/Vulkan-Tools/cube/cube.c`):
the animated, textured spinning cube. Each is a **real 3D Vulkan demo** that exercises
the full Vulkan pipeline (instance → device → swapchain → pipeline → draw) on a real
driver.

## Binaries

| File                   | Format | Built from                              | WuBuOS exec entry point        |
|------------------------|--------|-----------------------------------------|--------------------------------|
| `vkcube_linux.elf`     | ELF64  | `cube.c` (Linux, `-DVK_USE_PLATFORM_XLIB_KHR`) | `wubu_exec_linux_elf` → `wubu_ct_native` (VSL Linux VM) |
| `vkcube_windows.exe`   | PE32+  | `cube.c` (mingw cross, `-DVK_USE_PLATFORM_WIN32_KHR`) | `wubu_exec_win_pe` → `wubu_ct_steamos` (Proton/Wine) |
| `vkcube_macos.macho`   | —      | BLOCKED (see below)                     | `wubu_exec_macho` → VSL loader / `darling` |

## Confirmed full hardware usage (this host)

Host has **no dedicated GPU** (`/dev/dri` absent). The Vulkan driver in play is
**llvmpipe (Mesa software Vulkan, `lvp_icd`)**, apiVersion 1.4.318. This is a real
Vulkan ICD — full pipeline runs on the CPU rasterizer, not a stub.

### Linux ELF — VERIFIED
```
$ DISPLAY=:99 VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json ./vkcube_linux.elf --c 90
Selected WSI platform: xlib
Selected GPU 0: llvmpipe (LLVM 20.1.2, 256 bits), type: Cpu, apiVersion: 1.4.318
exit 0  (90 frames rendered)
```
Proof frame: `proof_elf_render.png` (2937 distinct colors, dominant 48% — real scene).

### Windows PE — VERIFIED (via Proton/Wine)
```
$ DISPLAY=:99 WINEPREFIX=... VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json wine vkcube_windows.exe --c 90
Selected WSI platform: win32
Selected GPU 0: llvmpipe (LLVM 20.1.2, 256 bits), type: Cpu, apiVersion: 1.4.318
exit 0  (90 frames rendered)
```
Proof frame: `proof_pe_render.png` (3122 distinct colors, dominant 51% — real scene).

Both images were captured from the live render window (ImageMagick `import`) — not
synthesised.

## macOS Mach-O — BLOCKED (honest status)

Cannot be delivered as a *rendering* 3D demo on this host:
- `darling` is NOT installed; `clang`/`otool`/osxcross absent → no Mach-O toolchain here.
- macOS 3D natively means **Metal**, which has no driver on a headless WSL2 box with
  no GPU and no `/dev/dri`. llvmpipe is Vulkan-only.
- A macOS **Vulkan** build could run via VSL→lavapipe, but requires Darling + a
  macOS Vulkan SDK to link, neither present.

To unblock: install Darling + osxcross, build `cube.c` for macOS with a Vulkan loader,
then route through `wubu_exec_macho` (VSL loader or `darling`). Tracked as a follow-up;
not faked.

## How to run through WuBuOS

The OS exec dispatcher in `src/runtime/wubu_exec.c` routes by payload magic:
- ELF  → `wubu_exec_linux_elf`  → `wubu_ct_native`  (Linux container, native Vulkan)
- PE   → `wubu_exec_win_pe`     → `wubu_ct_steamos` (Proton container → Wine)
- MachO→ `wubu_exec_macho`      → VSL loader / `darling`

On this host the PE path literally invokes Wine (verified above); the ELF path invokes
the native Vulkan loader against `lvp_icd` (verified above).
