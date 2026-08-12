# GPU Driver Frontier — Kevin Bacon 7-Hop Research

Research date: 2026-08-11. Method: kevin-bacon-research (7-hop convergence).

## Convergence
The WuBuOS kernel routes each GPU vendor to the correct Vulkan ICD. Online
research (ArchWiki AMDGPU/Intel, Gentoo AMDGPU table, Intel dgpu-docs, Mesa
docs, NVIDIA forums) converged on FOUR driver gaps the kernel does NOT yet cover:

| # | Gap | Driver | Evidence | Kernel action |
|---|-----|--------|----------|---------------|
| 1 | **AMD GCN1/2 (Southern Islands/Sea Islands)** | `radeon` KMD by default; `amdgpu` needs `si_support=1 cik_support=1`; radeon has NO Vulkan | ArchWiki AMDGPU "Enable Southern Islands/Sea Islands support"; Gentoo stable since 6.19 | Kernel must emit `amdgpu.si_support=1 amdgpu.cik_support=1` for GCN1/2 device IDs |
| 2 | **AMD RDNA4 (Navi44/48)** | RADV partial; AMDVLK needed for full Vulkan 1.4 | Gentoo AMDGPU table "RDNA 4 ... For Vulkan AMDVLK 2025.Q1.3+ needed" | ICD selection must prefer `amdvlk_icd.x86_64.json` for RDNA4 |
| 3 | **Intel i915 vs xe KMD** | Pre-Meteor Lake uses i915; Xe2 (Lunar Lake, Battlemage) prefers `xe` | ArchWiki Intel graphics; Intel dgpu-docs | Kernel must pick ICD by KMD: `intel_icd.json` for i915 (Tiger/Alder/Raptor), `xe` for Arc/Battlemage |
| 4 | **Hybrid iGPU+dGPU (DRI_PRIME)** | Laptop with iGPU + discrete | Gentoo AMDGPU "DRI_PRIME=1" | Kernel should expose DRI_PRIME when both a bare-metal iGPU and dGPU are present |

## AMD device generations (GCN → RDNA) — the ICD/routing table
| Family | Arch | Kernel | Vulkan ICD |
|--------|------|--------|-----------|
| Southern Islands (GCN1/2) | GCN 1.0/1.1 | amdgpu (si_support=1) | RADV |
| Volcanic Islands (GCN3) | GCN 3.x | amdgpu | RADV |
| Arctic Islands (GCN4) | GCN 4.x | amdgpu (kernel ≥4.15) | RADV |
| Vega (GCN5): Raven/Renoir | GCN 5.x | amdgpu | RADV |
| Navi10/12/14 (RDNA1) | RDNA | amdgpu (≥5.3) | RADV |
| Navi21-24 (RDNA2): Van Gogh/Rem/Ald | RDNA2 | amdgpu | RADV |
| Navi31/32/33 (RDNA3): Phoenix/Strix | RDNA3 | amdgpu (≥6.0) | RADV |
| Navi44/48 (RDNA4) | RDNA4 | amdgpu (≥6.12) | **AMDVLK** |

## Intel device generations — i915 vs xe KMD
| Generation | Arch | KMD | Vulkan ICD |
|-----------|------|-----|-----------|
| Broadwell+ (Gen8+) | iris | i915 | intel_icd.json (ANV) |
| Tiger/Rocket/Alder Lake | Xe | i915 | intel_icd.json |
| Raptor Lake | Xe | i915 | intel_icd.json |
| Meteor Lake | Xe-LPG | i915 | intel_icd.json |
| Lunar Lake / Battlemage (Xe2) | Xe2 | **xe** | intel_icd.json |
| Celestial (Xe3) | Xe3 | xe | intel_icd.json |

## Sources (persistent — nothing lives only in a conversation)
- ArchWiki AMDGPU — https://wiki.archlinux.org/title/AMDGPU (archived cache)
- Gentoo AMDGPU table — https://wiki.gentoo.org/wiki/AMDGPU
- Intel dgpu-docs hardware table — https://dgpu-docs.intel.com/devices/hardware-table.html
- Mesa RADV docs — https://docs.mesa3d.org/drivers/radv.html
