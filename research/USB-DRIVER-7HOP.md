# USB-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux USB driver stack gaps

USB is how EVERYTHING plugs in. The kernel-owned routing in
`src/kernel/wubu_usbf.c` covers the host-controller matrix, the per-device
class drivers, power management, USB-C/Type-C alt modes, and gadget mode.

### The host-controller matrix (chosen by PCI class + programming interface)

| Controller | USB version | PCI prog-if | Kernel module | WuBuOS route |
|-----------|-------------|-------------|--------------|--------------|
| OHCI / UHCI | USB 1.1 | 0x00 / 0x10 | `ohci_hcd`/`uhci_hcd` | `wubu_usbf_hcd()="ohci_hcd"` |
| EHCI | USB 2.0 | 0x20 | `ehci_hcd` | `wubu_usbf_hcd()="ehci_hcd"` |
| xHCI | USB 3.x (supersets all) | 0x30 | `xhci_hcd` | `wubu_usbf_hcd()="xhci_hcd"` |
| USB4/TBT | USB4 / Thunderbolt | 0x40 | `thunderbolt` (tb.ko) | `wubu_usbf_hcd()="thunderbolt"` |

xHCI (USB 3.x) is the modern superset — every controller since ~2015 is
xHCI, and it handles USB 1/2/3 simultaneously. USB4/Thunderbolt rides over
PCIe and uses the `thunderbolt` driver (intel vs amd vs renesas).

### Per-device class drivers (chosen by USB class code)

| Class | bInterfaceClass | Kernel driver | WuBuOS route |
|-------|----------------|---------------|--------------|
| HID (gamepad/kb/mouse) | 0x03 | `usbhid` | `wubu_usbf_class_driver(0x03)` |
| Mass storage | 0x08 | `usb-storage`/`uas` | `wubu_usbf_class_driver(0x08)` |
| Audio (headset/DAC) | 0x01 | `snd-usb-audio` | `wubu_usbf_class_driver(0x01)` |
| Video (webcam/capture) | 0x0E | `uvcvideo` | `wubu_usbf_class_driver(0x0E)` |
| CDC comm (network/serial) | 0x02 | `cdc_ether`/`cdc-acm` | `wubu_usbf_class_driver(0x02)` |
| Hub | 0x09 | `hub` | `wubu_usbf_class_driver(0x09)` |
| Printer | 0x07 | `usblp` | `wubu_usbf_class_driver(0x07)` |
| Composite/IAD | 0xEF | `usb-storage` (via IAD) | `wubu_usbf_class_driver(0xEF)` |

### Power management (the #1 gamepad/audio headache)

`usbcore.autosuspend` puts idle USB devices to sleep. For gamepads, audio
DACs, and storage this adds wake latency and can drop packets. WuBuOS
emits `usbcore.autosuspend=-1` when latency-critical classes are present:
`wubu_usbf_autosuspend_param()`.

### USB-C / Type-C / Thunderbolt alt modes

The Type-C connector (PCI class 0x0C/0x06 serial-bus) enables:
- **DP alt mode** — DisplayPort over USB-C (monitor/dock)
- **Thunderbolt alt mode** — TBT over USB-C
- **USB PD** — power delivery (charging/power)
- **OTG** — device/host dual role

WuBuOS detects the Type-C controller (`wubu_usbf_has_usbc()`) and OTG
capability (`wubu_usbf_has_otg()`).

### Gadget (device mode)

USB device mode via configfs function drivers:
- `g_uvc` (gadget webcam), `g_mass_storage` (gadget disk),
  `g_serial`, `g_ether` (RNDIS), `g_hid`
- `wubu_usbf_gadget_driver()` routes a function name to its module.

### Kernel summary line

```
usb[ctl=xhci+usb4 hcd=xhci_hcd hid=1 msc=1 aud=1 vid=0 net=0 ser=0]
```

Published to `/kv/world/hw_usb` by `wubu_usbf_summary()`.

### Gaps remaining (open in DRIVER-BANK DV-G)

Wired by `wubu_usbf.c`: controller matrix (xHCI/EHCI/OHCI/USB4),
class-driver routing (HID/storage/audio/video/network/serial/hub/printer),
autosuspend decision, USB-C/OTG detect, gadget routing. Remaining open:
USB4/TB4 alt-mode switch integration, USB-C DP-alt actual switchover,
usb-ids database, quirks table, benchmark harness.
