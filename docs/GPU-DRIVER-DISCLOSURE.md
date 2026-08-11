# GPU Driver Disclosure

> **Status:** The host GPU is detected but **no NVIDIA kernel module is loaded**
> (`lsmod | grep nvidia` → empty, `/dev/nvidia*` → nonexistent). Only the
> userspace runtime libraries are present (`libnvidia-ml.so.580`,
> `libcudart12`, etc.) plus the WSL2 `/dev/dxg` paravirtualized device.

## The situation

| Component | Present? | Notes |
|-----------|----------|-------|
| NVIDIA RTX 4050 (sm_89, Blackwell) | ✅ hardware | On the PCI bus (per lspci) |
| `libs/libcudart12`, `libcublas12` | ✅ installed | CUDA 12.0 runtime |
| `libnvidia-ml.so.580.173.02` | ✅ installed | NVIDIA userspace driver |
| `libvulkan_nvidia.so` (Vulkan ICD) | ❌ missing | No Vulkan path to the RTX 4050 |
| `nvidia.ko` (kernel module) | ❌ not loaded | `/dev/nvidia0` does not exist |
| `/dev/dxg` (WSL GPU paravirtualized) | ✅ present | The fallback path |

## The policy constraint

Per the FIRM RUN POLICY:

> Third-party software runs ON the AGI kernel — wubuos exec backends.
> Booting host Arch + host wine = cheating, NOT allowed.

The AGI kernel itself (`src/kernel/wubu_drv_gpu.c`) only knows AMD (Van Gogh,
Rembrandt) and Intel (Tiger Lake, Alder Lake). There is **no NVIDIA driver**
in the kernel tree. Writing one from scratch (PCI enumeration, BAR mapping,
NVML register programming, VRAM manager, MMU page tables) is a multi-month
reverse-engineering effort — it cannot be done ad hoc.

## The disclosure + workaround

**Option A — Scripted install of the proprietary driver** (`tools/install-gpu-stack.sh`):
1. Installs `nvidia-driver-580` (the distro package — NVIDIA's proprietary binary kernel module)
2. Installs `nvidia-cuda-toolkit` + `vulkan-nvidia` (Vulkan ICD)
3. Loads `nvidia.ko` into the host kernel
4. Verifies `/dev/nvidia*` appears and `nvidia-smi` reports the RTX 4050

**The disclosure:** `nvidia.ko` is third-party proprietary code owned by
NVIDIA. It is **not** source-stolen, **not forked, **not modified**. It is the
stock distro binary. It loads into the host Linux kernel — which IS the WuBuOS
execution surface in the bare-metal case. WuBuOS then exposes the GPU devices
to game containers via bind-mounts in `wubu_ct_start()` (the container config
already mounts `/dev/dri` and `/dev/nvidia`). The `wubu_secmon` syscall camera
observes all game syscalls through this container.

**Option B — Use the WSL2 path** (`/dev/dxg`): The host already provides
`libvulkan_gfxstream.so` and `/dev/dxg`, so Vulkan compute/graphics can run via
the WSL GPU paravirtualized path. This requires no new driver install but
falls back to the Intel iGPU for rasterization (no NVIDIA Vulkan ICD).

## Recommendation

Run `sudo tools/install-gpu-stack.sh` for the full RTX 4050 path (Option A).
This is the only way to get CUDA compute + NVIDIA Vulkan on bare metal.
The script is idempotent and writes a disclosure into `/var/log/wubu-gpu-install.log`.

## Trust boundary

The GPU driver is a **third-party binary** running in kernel space. This
breaches the "kernel-owned decoders" doctrine (which only covers *decompression*
code in `src/kernel/`). The GPU driver cannot be kernel-owned without a
from-scratch NVIDIA driver — which is a separate, long-term project.

The AGI's policy enforcement (seccomp allowlists in `ct_iso_seccomp.c`) and
syscall observation (`wubu_secmon`) still apply to game containers regardless
of whether the GPU is bare-metal or paravirtualized.
