# PSR-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux display PSR + NIC SR-IOV gaps

Two power/virtualization capabilities.

### PSR (panel self-refresh) routing (wubu_psr.c)

| GPU | Driver |
|-----|--------|
| Intel i915 | `i915-psr` |
| AMD amdgpu | `amdgpu-psr` |
| Xe | `xe-psr` |

### SR-IOV routing

| NIC | Driver |
|-----|--------|
| Intel X540 | `ixgbe` |
| Intel XL710 | `i40e` |
| Intel E810 | `ice` |
| Mellanox | `mlx5` |

### Kernel summary line

```
psr[psr=0(none) sriov=0 vf=0 vfs=0]
```

Published to `/kv/world/hw_psr` by `wubu_psr_summary()`.
