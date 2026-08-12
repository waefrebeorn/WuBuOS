# VIRT-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux virtualization (PV driver) gaps

WuBuOS must "run on everything" — including under every hypervisor. The
PV (paravirtualized) driver set differs per host, and the hypervisor is
detected via the CPUID 0x40000000 leaf vendor string.

### Hypervisor detection + PV driver sets

| Hypervisor | CPUID vendor | PV drivers | WuBuOS route |
|-----------|-------------|-----------|--------------|
| KVM/QEMU | `KVMKVMKVM` | virtio_blk/net/gpu/input | `wubu_virt_driver_set(1)` |
| Hyper-V/Azure | `Microsoft Hv` | hv_vmbus/hv_netvsc/storvsc | `wubu_virt_driver_set(2)` |
| VMware | `VMwareVMware` | vmxnet3/vmw_balloon/vmwgfx | `wubu_virt_driver_set(3)` |
| Xen | `XenVMMXenVMM` | xen-blkfront/xen-netfront | `wubu_virt_driver_set(4)` |
| VirtualBox | `VBoxVBoxVBox` | vboxguest/vboxsf/vboxvideo | `wubu_virt_driver_set(5)` |
| Parallels | `prl hyperv` | prl_fs/prl_net/prl_balloon | `wubu_virt_driver_set(6)` |

### Kernel summary line

```
virt[hyper=Hyper-V pv=hv_vmbus virtio=1]
```

Published to `/kv/world/hw_virt` by `wubu_virt_summary()`.

**Verified live:** on this WSL2 host it correctly detected `hyper=Hyper-V
pv=hv_vmbus` — the hypervisor routing is real and confirms we're running
under Microsoft's Hyper-V (WSL2).
