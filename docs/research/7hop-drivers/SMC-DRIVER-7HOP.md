# SMC-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU SMC firmware gaps

SMC (System Management Controller) firmware manages GPU power states (SMU, VCN, UVD).

### Firmware block routing (wubu_smc.c)

| Block | Routing |
|-------|---------|
| smu / smu1 | `SMU` |
| vcn | `VCN` |
| uvd | `UVD` |
| gfx | `GFX` |
| cpu | `CPU` |
| else | `SMU` |

### State routing

| State | Routing |
|-------|---------|
| load / boot | `loading` |
| done / ready | `ready` |
| fail / error | `failed` |
| verif | `verifying` |
| else | `loading` |

### Kernel summary

```
smc[smc=0 smu=0 vcn=0 uvd=0 fw=0 drv=none]
```

Published to `/kv/world/hw_smc`. (No amdgpu/radeon/i915 on WSL2.)
