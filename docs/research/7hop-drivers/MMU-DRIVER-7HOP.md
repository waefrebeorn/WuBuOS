# MMU-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU MMU page-table gaps

GPU MMU (Memory Management Unit) handles page tables + fault handling.

### Table type routing (wubu_mmu.c)

| Type | Routing |
|------|---------|
| pte / pt | `page-table` |
| pde / pd | `page-directory` |
| vm / context | `vm-context` |
| ggtt | `ggtt` |
| ppgtt | `ppgtt` |
| else | `page-table` |

### Fault routing

| Fault | Routing |
|-------|---------|
| page | `page-fault` |
| access | `access-fault` |
| prot | `protection-fault` |
| gpu | `gpu-page-fault` |
| else | `page-fault` |

### Kernel summary

```
mmu[mmu=0 pt=0 fault=0 vma=0 ctx=0 drv=none]
```

Published to `/kv/world/hw_mmu`. (No drm/amdgpu/i915 on WSL2.)
