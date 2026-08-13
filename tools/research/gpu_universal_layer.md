# WuBu Universal GPU Layer — SPIR-V + Vulkan, cross-vendor (NVIDIA / AMD / Intel)

**Research date:** 2026-08-13. **Status:** RESEARCH — design + driver map + gaps,
NOT yet implemented. Mirrors the WuBu philosophy: SPIR-V/Vulkan is our ONE universal
GPU interface so the same kernel code runs identically on NVIDIA, AMD, Intel, Arm,
Apple Silicon, Steam Deck, laptops, and desktop dGPUs. We containerize every vendor's
compute personality (CUDA, ROCm/HIP, oneAPI) but Vulkan+SPIR-V is the shared substrate
they all converge on, the way WuBuOS sits at Ring 0 over all guest OS rings.

Sources: Khronos SPIR-V/Vulkan docs, Mesa docs (RADV/ANV/NVK), NVIDIA dev blog
(cooperative matrices), VUDA arXiv 2605.01352 (2026-05-02, SJTU), Phoronix,
vulkan.gpuinfo.org, Valve/gamescope, local host probe (this WSL2 box).

---

## 1. The universal interface: SPIR-V is THE portable kernel/shader IR

- **SPIR-V** (Khronos) is the binary intermediate representation for BOTH graphics
  shader stages AND compute kernels. It is the only cross-vendor IR that is natively
  consumed by Vulkan, OpenGL, AND OpenCL drivers. The Khronos white paper calls it
  "the standard IR for parallel compute and graphics."
- **Vulkan consumes SPIR-V directly** via `vkCreateShaderModule` — no driver-side
  GLSL compile. So if WuBuOS's HolyC-derived compiler can emit SPIR-V, every vendor
  driver (AMD RADV, Intel ANV, NVIDIA NVK + proprietary, Arm, Apple Metal→Vulkan
  bridges) can run it with ZERO per-vendor shader recompiles. This is the GPU
  analogue of "we compile once, the container runs everywhere."
- Toolchain that already exists (all present on this host):
  - `glslangValidator` (Khronos) — GLSL/HLSL/ESSL → SPIR-V.
  - `spirv-as` / `spirv-val` / `spirv-opt` / `spirv-dis` — SPIRV-Tools: binary
    assembler, validator, optimizer, disassembler. **`spirv-as` lets us emit SPIR-V
    text ourselves and assemble to the binary — the clean path for a self-hosted
    HolyC→SPIR-V backend.**
  - `shaderc`/`glslc`, `spvc` (SPIRV-Cross, disassemble back to GLSL/HLSL/MSL for
    reflection + cross-API), DXC (HLSL→SPIR-V), `clspv` (OpenCL-C→SPIR-V compute).
- **clspv is the closest existing "C-like → Vulkan compute" compiler** (Google,
  LLVM-based, OpenCL-C subset → SPIR-V compute). It is the proof that a C-family
  frontend emitting SPIR-V for Vulkan compute is a solved, buildable thing — a
  template for a WuBuOS HolyC→SPIR-V backend (see §5).

## 2. The driver map — all three (plus) vendors converge on Mesa Vulkan

The host's `/usr/share/vulkan/icd.d/` already ships EVERY vendor ICD, confirming
this is the universal substrate:

| Vendor | Mesa Vulkan driver | Notes |
|---|---|---|
| **AMD** | **RADV** (`libvulkan_radeon`) | Vulkan 1.3 (GCN1-2), 1.4 (GCN3+/RDNA). **THE Steam Deck driver.** amdgpu kernel driver, open-source. Valve funds it. |
| **Intel** | **ANV** (`libvulkan_intel`) | Vulkan 1.4, Gen7+. Intel's OFFICIAL Linux Vulkan driver. `intel_hasvk` = older HW. |
| **NVIDIA** | **NVK** (Mesa, open) + proprietary `nvidia_icd` | NVK = conformant Vulkan 1.4, merged into Mesa 25.0 (early 2025), targets nouveau/Nova kernel drivers. NVIDIA also ships its own proprietary Vulkan driver (still the highest-perf for CUDA-era GPUs + cooperative matrices). |
| **Arm** | PanVK / Valhall | Mali GPUs. |
| **Qualcomm** | Turnip | Adreno. |
| **Apple** | MoltenVK (Metal→Vulkan) | The macOS leg (matches WuBuOS's existing `WUBU_METAL2VULKAN=1` shim + metal2vulkan path). |
| **Fallback** | llvmpipe (lvp) | Software rasterizer, Vulkan 1.4 — always works, slow. **This is what this WSL2 host currently enumerates by default.** |

**The key insight for WuBu:** because Vulkan (via Mesa) has a conformant open-source
driver for AMD, Intel, AND NVIDIA, Vulkan is the ONLY compute API that runs the same
SPIR-V on all three vendors' mainstream hardware from a single binary. CUDA is
NVIDIA-only; ROCm/HIP is AMD-first; oneAPI Level Zero is Intel-first. **SPIR-V +
Vulkan is the true universal interface.** WuBuOS containerizes those per-vendor
personalities on top of it.

## 3. Vulkan compute can be CUDA-competitive — cooperative matrices (AI hot path)

- **`VK_KHR_cooperative_matrix`** + **`VK_NV_cooperative_matrix2`**: matrix types
  spread across a subgroup, tapping vendor tensor hardware (Turing+ Tensor Cores,
  AMD matrix cores) with NO per-vendor shader rewrites. NVIDIA's own benchmark:
  significant speedups over scalar Vulkan math, and NVIDIA states Vulkan ML is
  **competitive with CUDA** (Jeff Bolz, Vulkanised 2025; Phoronix 2025-03-02).
- **ggml/llama.cpp ships a Vulkan backend** (Vulkanised 2026 talk) — a real, shipping
  proof that full LLM inference runs cross-vendor through Vulkan. This is exactly the
  WuBu-35M / wubuwizard / Bear-RL GPU-accelerator substrate.
- This is the answer to the user's instinct: **Vulkan IS the CUDA/AMD/Intel "cross
  module."** One SPIR-V kernel source → Tensor Cores (NVIDIA), RDNA CUs (AMD), Xe
  matrix engines (Intel).

## 4. CUDA↔Vulkan interop — VUDA breaks execution isolation (research, 2026)

**arXiv 2605.01352 (VUDA, SJTU, 2026-05-02):** CUDA compute and Vulkan graphics on
the same NVIDIA GPU are confined to separate TimeSlice Groups (TSGs) → they time-slice
mutually exclusively (execution isolation). VUDA **spatially shares** them by:
1. redirecting CUDA streams into the Vulkan context's channel/TSG (both converge on
   the same low-level channel primitive), and
2. grafting page tables to unify the two address spaces (no data copying).
Result: **up to 85% higher throughput** than temporal sharing for embodied-AI
simulate(physics/CUDA)+render(Vulkan) workloads.

**Why this matters for WuBuOS:** WuBu's Bear RL / worldsim wants to SIMULATE physics
and RENDER photorealistically at the same time on one GPU. VUDA is the research
proof that the hardware + driver primitives exist to run both concurrently. For WuBu
specifically, since we'd do BOTH compute and graphics through Vulkan anyway (one
runtime, one TSG), we **avoid the isolation problem entirely** — that is the clean
architectural win of going all-Vulkan rather than CUDA+Vulkan.

## 5. Hardware identification — "the system must identify properly"

The universal layer must pick the RIGHT GPU on a machine with several (laptop
iGPU+dGPU, Steam Deck APU, eGPU):

- **PCI vendor IDs** (also = Vulkan `vendorID`): `0x10DE` NVIDIA, `0x1002` AMD,
  `0x8086` Intel, `0x13B5` Arm, `0x5143` Qualcomm, `0x106B` Apple. (llvmpipe uses
  pseudo-vendor `0x10005` — this host's default enumeration.)
- **`vkGetPhysicalDeviceProperties`** → `vendorID`, `deviceID`, `deviceName`,
  `deviceType` (`VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU` / `DISCRETE_GPU` /
  `CPU` for llvmpipe), `apiVersion`. This is THE canonical runtime hardware query.
- **Device ranking heuristic** (per gamedev best practice): prefer a discrete GPU,
  prefer higher apiVersion, prefer `VK_KHR_cooperative_matrix` capability for the AI
  lane, then fall back through integrated → llvmpipe. On a laptop, rank dGPU > iGPU;
  on Steam Deck (single AMD APU) there's just RADV.
- **`VkPhysicalDeviceVulkan11Properties.deviceUUID/driverUUID`** — stable UUIDs for
  caching which driver+GPU combo a shader was tuned against.
- **The local WSL2 reality (grounding):** this box's default loader enumerates ONLY
  llvmpipe (software), because the real GPU is behind `/dev/dxg` (gfxstream ICD) —
  unless `VK_ICD_FILENAMES` is shimmed to prefer gfxstream/radeon/nvidia/etc.
  That is precisely the "identify properly" failure the existing
  `wubuos-gpu-shim` skill fixes. **A WuBu universal GPU layer must re-derive its
  ICD preference from the physical device query (not hardcode), so it survives on
  every host: real dGPU, real iGPU, APU, WSL2-gfxstream, or pure software.**

## 6. Container / Ring-0 framing (the WuBu architectural story)

WuBuOS is the universal container running at Ring 0 while guest OSes sit in their own
rings. The GPU layer mirrors this exactly:
- **SPIR-V = the universal "machine code" for GPUs** — one kernel, every vendor, no
  recompile (like one C11 binary, every ISA via a JIT).
- **Vulkan = the universal scheduler/ring-0 for GPUs** — the single driver contract
  that all vendor kernels converge on, the way WuBuOS is the single host all guest
  OSes converge on.
- Per-vendor personalities (CUDA, ROCm/HIP, oneAPI) are **containers** on top — run
  them when present, but the default is the universal SPIR-V/Vulkan path so we are
  never hostage to one vendor's toolchain.

## 7. Optimization opportunities for the WuBu HolyC→Vulkan backend

1. **Self-hosted SPIR-V emitter** — use `spirv-as` (binary assembler) as the back
   end of a HolyC→SPIR-V codegen (compute + vertex/fragment). Then `glslang` is only
   needed at dev time, not runtime — WuBuOS emits SPIR-V itself. (Clspv shows the
   LLVM route; a HolyC-native emitter is the "own system" path.)
2. **Cooperative-matrix GEMM** for the AI lane (Bear RL, wubuwizard, WuBu-35M) —
   `VK_KHR_cooperative_matrix`, 16-bit, vendor tensor hardware, one source all
   vendors. Directly supersedes the Bear Vulkan scalar matmul.
3. **Whole-pipeline Vulkan** (compute AND graphics in one TSG) — avoids the
   VUDA-style CUDA/Vulkan isolation problem and enables overlapped
   simulation+rendering for worldsim (the VUDA use case, without the two-runtime
   tax).
4. **Persistent/batched transfers** — the wuburvc lesson: per-call upload+sync
   dominates (9.84ms/call); keep buffers resident, batch, use host-coherent
   memory. (Already a Bear-Vulkan lesson; applies to the universal layer too.)
5. **`spirv-opt`** at shader-build time for vendor-neutral optimization; cache the
   tuned SPIR-V keyed by `driverUUID`.
6. **gfxstream/Venus/DXG paths** already exist in `wubu_vsl_vulkan.c` — extend the
   same detect-and-route to a rank-based physical-device picker (§5) so the shim
   becomes data-driven.

## 8. Honest gaps / next steps

- **NOT implemented yet** — this is research. The concrete next step is a WuBu
  HolyC→SPIR-V emitter prototype (compute kernel: storage buffers + push constants +
  `main` entry — the exact pattern Bear Vulkan already uses), gated on
  `make test_vulkan` style verification with `spirv-val vulkan1.3`.
- CUDA↔Vulkan VUDA-style spatial sharing requires vendor kernel-driver work — not
  something WuBuOS needs if we go all-Vulkan (the whole point of §4).
- llvmpipe fallback is correct-but-slow; the shim must keep preferring real devices.
- The existing `wubuos-gpu-shim` skill documents the WSL2 ICD ordering; a universal
  layer builds on it, replacing the hardcoded list with runtime device ranking.

## Sources
- Khronos: SPIR-V (khronos.org/spirv), Vulkan docs (docs.vulkan.org/guide/latest/what_is_spirv.html), SPIRV-Tools (github.com/KhronosGroup/SPIRV-Tools), SPIRV-Cross.
- Mesa: RADV (docs.mesa3d.org/drivers/radv.html), ANV (mesa3d.org), NVK (docs.mesa3d.org/drivers/nvk.html), Mesa 2025/26 highlights (Phoronix).
- NVIDIA: "Machine Learning Acceleration in Vulkan with Cooperative Matrices" (developer.nvidia.com), Vulkanised 2025 (Jeff Bolz), ggml/llama.cpp Vulkan backend (Vulkanised 2026).
- VUDA: arXiv 2605.01352 (2026-05-02) — CUDA+Vulkan spatial sharing.
- vulkan.gpuinfo.org, gamedev.stackexchange device selection, raphlinus swapchain frame pacing.
- Local: host ICD list + vkGetPhysicalDeviceProperties probe (this WSL2 box → llvmpipe default, /dev/dxg gfxstream).
