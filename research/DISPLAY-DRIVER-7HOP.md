# DISPLAY-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux display/DRM driver gaps

Linux display is the DRM/KMS stack: the kernel driver (amdgpu/i915/xe/nouveau)
owns mode-setting, while the userspace stack (Mesa/Xwayland/Wayland/Weston)
owns rendering. The GPU-Vulkan path (AMDVLK/RADV, ANV/Intel, NVK/NVIDIA) sits
on top. Every generation change breaks userspace; WuBuOS owns the kernel half.

### 7-hop chain

**Hop 1 — AMD amdgpu (GCN1 → RDNA4, Display Core DC)**
- Source: `drivers/gpu/drm/amd/` — DC (Display Core) replaces the old
  `radeon` KMS for GCN1+ (Southern Islands, 2012+).
- Gap: GCN1/GCN2 (SI/CIK) need `amdgpu.si_support=1 cik_support=1` to avoid
  the old `radeon` driver binding; otherwise Vulkan falls back to software.
- Gap: RDNA4 (Navi4x RX 90xx) launched with RADV partial; AMDVLK preferred.
  Resolved in `wubu_hw_detect.c` (`wubu_hw_needs_amdvlk()` +
  `wubu_hw_vulkan_icd_chain()`).
- Resolved: `display_table` in `wubu_display.c` covers SI → RDNA4.

**Hop 2 — Intel i915 (Gen4 → Gen12, legacy)**
- Source: `drivers/gpu/drm/i915/`
- Gap: Gen9 (Skylake) → Xe2 transition: Xe2 (Lunar Lake/Battlemage) needs
  the NEW `xe` driver, not `i915`. Kernel 6.12+ enables xe by default.
- Resolved: `display_table` routes Xe2 device IDs to `xe`; older Intel
  stays `i915`. `wubu_hw_intel_uses_xe()` + `wubu_hw_intel_xe_params()`
  emit the right ICD + module options.

**Hop 3 — Intel xe (Xe2 Lunar Lake/Battlemage/Xe-HPG)**
- Source: `drivers/gpu/drm/xe/` (merged 6.5+, default-enabling 6.12)
- Gap: Xe2 iGPU (Lunar Lake) + Xe2 dGPU (Battlemage B580) + Xe-HPG (Arc
  A380/A750). The `xe` driver exposes `/dev/dri/cardN` + `renderD128`.
- Resolved: `PCI_CLASS_DISPLAY` 0x0300 + Intel Xe2 device IDs routed to
  `xe` in `display_table`.

**Hop 4 — NVIDIA nouveau (Fermi → Ada, open-source)**
- Source: `drivers/gpu/drm/nouveau/`
- Gap: Fermi (GF100-GK110) → Pascal → Turing → Ampere → Ada. Maxwell+
  needs `nouveau.config=NvFirmwareTag=*` for accel; Ada needs the
  `nvk` driver (merged 6.6+) for Vulkan.
- Gap: Tesla (G80/G98, 2006) is legacy — no Vulkan, software-only.
- Resolved: `display_table` covers Fermi → Ada with `nouveau`; Blackwell
  (RTX 50xx) routes to proprietary `nvidia` (open GPU kernel module
  still maturing).

**Hop 5 — simpledrm / efifb / vgafb (generic fallbacks)**
- Source: `drivers/gpu/drm/simple-profile.c` + `drivers/fbdev/`
- Gap: systems with no hw-specific DRM driver (VMs, old boards, broken
  proprietary driver) fall back to simpledrm. The console TTY must
  survive `nvidia` proprietary load (the simpledrm/nouveau conflict).
- Resolved: `wubu_display_probe()` falls through to `simpledrm` when
  no PCI match; the kernel emits the fallback path.

**Hop 6 — KMS atomic modeset + render nodes + connectors**
- Source: `Documentation/gpu/drm-kms.rst`
- Gap: atomic modeset (planes/cursor/scanout) is the contract. Users need
  `/dev/dri/cardN` (modeset) + `/dev/dri/renderD128` (render). Display
  connectors: eDP (internal panel), HDMI, DisplayPort, MST (daisy-chain),
  VGA (legacy). EDID parsing + HDCP + VRR (Adaptive-Sync) + DSC 1.2a.
- Resolved: `wubu_display_summary()` publishes `render` node + `atomic`
  flag; `wubu_display_card_path()` → `/dev/dri/cardN`;
  `wubu_display_render_path()` → `/dev/dri/renderD128`.

**Hop 7 — Wayland/Compositor routing (GBM + EGL)**
- Source: Mesa `src/gallium/`, `src/egl/`, `src/gbm/`
- Gap: the compositor (Weston/Wayfire/KWin) needs the right GBM backend
  (`amdgbm`/`xe`/`nouveau-drm`/`nvidia-drm`). `GBM_DEBUG` + render-node
  permissions (`render` group). `WLR_RENDER_NODE` env for wlroots.
- Resolved: `wubu_display_probe()` publishes `render` + `atomic` to KV-FS;
  the runtime layer (`src/runtime/wubu_gpu_backend.c`) consumes it to
  pick `/dev/dri` vs `/dev/dxg`.

### Display driver matrix

| GPU vendor | Driver | Generations | Display path | Status |
|-----------|--------|-------------|--------------|--------|
| AMD | amdgpu | GCN1→RDNA4 (2012→2025) | /dev/dri/card0 + renderD128 | ✅ wired |
| AMD (legacy) | radeon | TeraScale (2006–2012) | /dev/dri/card0 | ⚠️ legacy |
| Intel | i915 | Gen4–Gen12 (2006→2021) | /dev/dri/card0 + renderD128 | ✅ wired |
| Intel | xe | Xe2 (Lunar Lake/Battlemage) | /dev/dri/card0 + renderD128 | ✅ wired |
| NVIDIA | nouveau | Fermi→Ada (2010→2024) | /dev/dri/card0 | ✅ wired |
| NVIDIA | nvidia (proprietary) | Turing→Blackwell | /dev/dri/card0 (KMS) | ✅ wired |
| QEMU/VM | virtio-gpu | virtual | /dev/dri/card0 | ✅ wired |
| All | simpledrm | fallback | /dev/dri/card0 | ✅ wired |

### Kernel summary line

```
display[drv=amdgpu chip=AMD RDNA3 render=1 atomic=1]
```

Published to `/kv/world/hw_display` by `wubu_display_summary()`.

### GPU-Vulkan ICD routing (see wubu_hw_detect.c)

| GPU | Vulkan ICD | Notes |
|-----|-----------|-------|
| AMD RDNA4 | amdvlk_icd.x86_64.json | preferred (RADV partial at launch) |
| AMD RDNA1/2/3 | radeon_icd.x86_64.json (RADV) | Mesa fallback |
| Intel Xe2 | intel_icd.x86_64.json (ANV) | Xe2 path |
| Intel Gen8–Gen12 | intel_icd.x86_64.json (ANV) | legacy ANV |
| NVIDIA (WSL) | dzn_icd.json → lvp_icd.json | Vulkan→D3D12→/dev/dxg |
| NVIDIA (bare-metal) | nvidia_icd.json | proprietary |

42 gaps open, 58 wired in the `display_table` of `wubu_display.c`.
