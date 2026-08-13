# WuBuOS Driver Self-Installation — Numeracy, Enumeration, and Self-Bootstrapping

**Research date:** 2026-08-13. **Status:** RESEARCH — gap identification +
design of the self-install layer. This is the AGI-OS requirement the user
flagged: "it needs to be numerate, identify all of its own drivers, and
install itself." WuBuOS can DISCOVER hardware (see `wubu_probe.c`,
`wubu_drv.c`) but when a device has NO registered driver, it writes
"unbound" to the machine-matrix and stops. It does NOT fetch/build/compile
a driver module for the unknown device and load it. That is the gap.

The contrast with firmware (firmware fetch via `wubu_fw.c`/`wubu_gpufw.c`)
is the design hint: firmware already self-fetches; driver modules must do the
same.

---

## 1. The exact current gap (grounded in this tree)

`src/kernel/wubu_drv.c` (the device model, DRV1–DRV9):
- Static registry: `g_drivers[WUBU_DRV_MAX_DRV=24]`, populated in
  `wubu_drv_init()` with a fixed list (nvme, ahci, wifi, net, gpu, battery,
  usb-hid, usb-msc, …). Each driver = `{name, id_table[], n_ids, probe}`.
- `wubu_drv_pci_scan()` enumerates the real PCI bus (config-space via
  `0xCF8/0xCFC`), copies devices into `g_devs[]`.
- The bind loop (`wubu_drv.c:114`) matches `vendor/device` against each
  driver's `id_table`. A match → `bound=1`, call `drv->probe`. **No match →
  the device stays `bound=0`, name="unbound".**
- `wubu_drv_summary` / `wubu_probe_drv_matrix` emit
  `"drvpower[drivers=%d devices=%d]"` and per-device "unbound" — **there is
  no arm that says "I don't have a driver for vendor 0x8086/dev 0x9841 →
  go fetch/build the `intel_gpu` module and load it".**

Firmware (`wubu_fw.c`, `wubu_gpufw.c`, `wubu_audiofw.c`) already implements
the *fetch* leg (`wubu_fw_lib`, `wubu_fw_loader`, `wubu_fw_update`), so the
self-install layer is structurally parallel but missing for code modules.

`tools/install-gpu-stack.sh` is the current "escape hatch" — but a HOST
helper (apt + modprobe nvidia), explicitly *not* kernel-owned self-install:
> "The alternative — a from-scratch bare-metal NVIDIA driver in src/kernel/ —
> is a multi-month reverse-engineering effort and is NOT feasible to
> implement ad hoc."
That note defines the boundary the self-install layer must cross to make the
AGI-OS claim true.

## 2. The reference mechanisms (Linux / eBPF / self-hosting)

### 2.1 udev + MODULE_DEVICE_TABLE = the standard "auto-load" pattern
A kernel module declares:
```c
static struct usb_device_id hello_id_table[] = { { USB_INTERFACE_INFO(...) }, { } };
MODULE_DEVICE_TABLE(usb, hello_id_table);   // emits .moddevice.dep.modinfo
```
The kernel/bus exports `MODALIAS` uevents (e.g.
`pci:v000010DEd00002504…`); **udev** has the rule:
```
DRIVER!="?*", ENV{MODALIAS}=="?*", RUN{builtin}="kmod load $env{MODALIAS}"
```
→ the module for the modalias is loaded on coldplug. This is the proven
"discover → identify needed driver → load it" loop the WuBu device model
already mirrors conceptually (`wubu_drv_id_t`, id tables, probe) but lacks
the **fetch-if-missing → build-from-source → load** tail.

### 2.2 DKMS = "build from source in the running kernel" pattern
DKMS (Dynamic Kernel Module Support) is the in-OS pattern for *compiling* a
module against the *present* kernel headers at install time. A DKMS tree =
source + `dkms.conf`; `dkms autoinstall` walks `/usr/src/<name>-<version>`,
builds, and installs into `/lib/modules/$(uname -r)/updates/dkms/`. The
equivalent for WuBuOS: a `/kv/drivers/src/<name>/{src,wubu.mk}` tree whose
`wubu_drv_build()` compiles `wubu_rt.o` + a per-driver object using the
running kernel's exported symbols and links it into an in-memory module to
hand to `wubu_kmod_load()`.

### 2.3 BPF CO-RE + BTF = "compile-once, describe-self" pattern
BPF CO-RE (Compile Once – Run Everywhere) brings BTF (kernel type info)
from kernel → `vmlinux.h`, then **rewrites struct offsets at load time** via
`__builtin_preserve_access_index` relocations. The loader side:
`bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h`.
This is the single most relevant precedent for an AGI OS: a program carries
type/relocation info that lets it **identify the host and self-adapt at
load time**, rather than being pre-compiled against one kernel. WuBuOS's
self-install path can mirror this: a driver manifest carries (vendor/device
range, BAR layout expectations, kernel-symbol requirements) and the loader
adapts.

### 2.4 Device Tree + overlays = "data-driven peripherals" pattern
The Linux "Generate device tree node for PCI devices" work (LWN 939317,
Lizhi Hou 2023) makes PCI enumeration emit a flattened device tree blob so
**non-discoverable** peripherals (FPGA endpoints, custom ASICs, laptops with
quirkless firmware) can be described data-first and bound via overlay. This
is the numeracy answer for devices the firmware doesn't expose cleanly:
WuBuOS enumerates PCI + builds a DT-like `/kv/world/dtree`, and a driver
manifest declares "I bind to dtree node matching vendor 0x8086/device
0x9841 + matching compatible string".

## 3. The numeracy table (what we already know, what we bootstrap)

| Bus | What WuBu does | Self-install leg |
|---|---|---|
| PCI | `wubu_pci_scan` (0xCF8/0xCFC config-space, bus 0 flat scan) — done | `wubu_drv_pci_modalias(dev)` synthesizes `pci:vVVVVdDDDD…` from vendor/dev/class/subclass/progif → feeds the fetch table |
| USB | `wubu_usbf.c` (descriptor walk), `wubu_drv_usb_hid/msc/bt` IDs | `wubu_drv_modalias_usb(usb_dev)` → `usb:vVVVVpDDDD…` for HID/BT/HUB class quirks |
| ACPI | `wubu_drv_bus ACPI` enum exists | `_HID`/`_CID` modalias + `wubu_ucode.c` microcode staging |
| Virtual | `wubu_virt.c`, VirtIO (blk/net/gpu/input) IDs | virtio modalias on `/kv/world/virtio` |

Vendor IDs the numeracy table must key on (from research 2026-05):
`0x10DE` NVIDIA, `0x1002` AMD, `0x8086` Intel, `0x13B5` Arm, `0x5143`
Qualcomm, `0x106B` Apple, `0x1234/0x1AF4` QEMU/virtio, `0x10EE` Xilinx.
(LLvmpipe's pseudo `0x10005` is a *driver* choice, not a PCI vendor — see
§4.)

## 4. Hardware-identity for the GPU leg (directly the user's "Steam Deck /
laptop / computers" concern)

`vkGetPhysicalDeviceProperties` → `vendorID`, `deviceID`, `deviceName`,
`deviceType`, `apiVersion`, `VkPhysicalDeviceVulkan11Properties.deviceUUID`
/ `driverUUID`. This host enumerates **only llvmpipe** (vendor `0x10005`,
device type `CPU`) by default because the real GPU is behind `/dev/dxg`
(WSL2 gfxstream) — confirmed by a live probe this session. The universal
layer must rank real device (gfxstream/radeon/nvidia/intel) ABOVE llvmpipe,
keyed by `deviceUUID`. On a real laptop/Steam Deck the same loader finds the
dGPU (AMD `0x1002`, RADV) or the APU. This is the "identify properly"
requirement: the matrix must not silently degrade to software.

## 5. Design: wubu_drv_install() — the missing arm

The self-install layer plugs the existing gap:

```c
/* DRV_install: the arm the OS was missing. */
int wubu_drv_install(wubu_drv_dev_t *dev);   // → fetch + build + load
```
Algorithm (numerate → decide → acquire → build → load):
1. **MODALIAS** — `wubu_drv_modalias(dev)` emits the bus alias string.
2. **MISSING?** — `wubu_drv_lookup(alias)`: already have it in the registry?
   then the device is just unbound-for-another-reason → report. If not →
   proceed.
3. **DECIDE** — consult `/kv/world/driver_manifest` (a Styx/9P-served,
   updatable table mapping `{modalias-regex, vendor/device-range}` →
   `{source: local|pkg|git|fw_container, build: wubu.mk, min_kver}`).
   The manifest is the AGI's mutable knowledge — it learns
   "0x10DE/0x2504 → fetch nv_gpu from source tree S at min kver K".
4. **ACQUIRE** — `wubu_drv_fetch(manifest)`: if source is a git ref, clone;
   if a pkg module, pull the pre-built object; if firmware-backed
   (`/kv/drivers/firmware/<name>.bin`), reuse the `wubu_fw_loader` path.
5. **BUILD** — `wubu_rt_build(dev_src[], wubu.mk, kver)` compiles against
   the running kernel's symbol table (`__wubu_sym` exports, analogous to
   `vmlinux.h`/BTF in §2.3) into an in-memory relocatable module. This is the
   DKMS leg, but kernel-owned (no host apt/modprobe).
6. **LOAD** — `wubu_kmod_load(module)`: relocate, resolve `__wubu_sym`
   imports, register into the live `g_drivers[]` registry, then re-probe
   the device. The device flips `bound=0 → 1` and the matrix updates
   `/kv/world/hw_matrix` live over 9P.

Fallbacks (the "run everything" doctrine):
- No manifest entry + no network + no source → try the **firmware container**
  path (some vendors ship a signed bytecode blob usable by a generic
  `wubu_fw`-style interpreter driver).
- Still nothing → emit `"unbound:nosrc alias=..."` to the matrix so the AGI
  / user can author a new manifest entry offline (the AGI can even auto-draft
  one from a sibling manifest, à la the recursive self-improve loop).

## 6. Optimization / self-hardening opportunities
- **CO-RE-style relocations**: a driver manifest can carry
  `__attribute__((preserve_access_index))`-like annotations (or a
  `wubu_probe_matrix`-style indirection) so one compiled driver module adapts
  to field offsets that shift between kernel revisions — borrowing the
  BTF/relocate-at-load idea for native modules.
- **Batch coldplug**: on `wubu_probe_all()`, collect all unbound devices,
  dedupe by modalias family, fetch/build per family once (not per device) —
  like `dkms autoinstall` walking `/usr/src`, but over the 9P tree.
- **GPU ICD self-install**: the universal layer from
  `gpu_universal_layer.md` plugs in here — when the loader finds a GPU PCI
  device with no ICD, `wubu_drv_install` fetches/builds the matching Mesa
  driver (radeonsi/anv/nvk) and registers its ICD JSON under the loader's
  ICD dir, so the SAME loader that finds llvmpipe also installs the real
  device.
- **`/tmp` is not for this**: per the host rules, fetched driver sources and
  build artifacts go in `tools/drivers-src/` (persistent) — never `/tmp`.

## 7. Honest gaps (this is research, not implementation)
- `wubu_drv_install` / modalias synth / `driver_manifest` do NOT exist yet.
- The kernel must export a stable symbol table for in-memory module linking
  (a `wubu_kmod.c` ELF loader + `wubu_symtab` — not yet written).
- Building NVIDIA's proprietary `nvidia.ko` cannot be "fetched from source"
  by design (it is a third-party blob); the legitimate leg is the
  *open-source* NVK + Mesa stack (§2 of `gpu_universal_layer.md`), with the
  proprietary NVIDIA kernel module remaining a host-side opt-in per
  `tools/install-gpu-stack.sh` / `docs/GPU-DRIVER-DISCLOSURE.md`.
- Firmware self-fetch (`wubu_fw.c`) exists but the driver-module build leg
  does not — this doc's §5 is the prescribed bridge.

## Sources
- Linux kernel: `MODULE_DEVICE_TABLE`, modalias uevents
  (stackoverflow.com/23307579, linuxfromscratch.org udev chapter,
  raspberrypi.com forum).
- DKMS: docs.oneprocloud.com build guide; opensuse DKMS/system systemd.
- BPF CO-RE/BTF: docs.ebpf.io CO-RE, nakryiko.com CO-RE, `bpftool btf dump`
  → `vmlinux.h`.
- Device Tree / PCI: LWN 939317 (Lizhi Hou 2023), patchew PCI DT node gen,
  ti.com PCIe hotplug rescan, youtube device-tree overlays talk.
- Vulkan GPU identity: `VkPhysicalDeviceProperties` (vendorID/deviceID/
  deviceUUID/driverUUID) — grounded by a live host probe (llvmpipe default,
  /dev/dxg gfxstream on WSL2). Cross-ref `gpu_universal_layer.md` §4/§5.
