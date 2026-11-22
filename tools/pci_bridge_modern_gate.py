#!/usr/bin/env python3
"""pci_bridge_modern_gate.py -- a modern virtio device must work behind a bridge.

The registry stores PCI bus *type* in dev->bus (0 = WUBU_DRV_BUS_PCI) but
capability walkers use that same field as the PCI bus *number*. On the root
bus the values coincide, so the bug only appears when a device is behind a
PCI-to-PCI bridge: the cap list is read from bus 0 and the device never goes
live.

This gate boots the SAME kernel twice with an identical modern-only
virtio-blk, changing only its PCI parent bus:

  * control: device on pci.0  -> must print a live MODERN disk
  * bridge:  device on pci.1  -> must also print a live MODERN disk

Usage: python3 tools/pci_bridge_modern_gate.py [kernel.elf] [--window SECS]
Pattern: tools/pci_bridge_gate.py / tools/virtio_modern_gate.py.
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ASSERT = os.path.join(HERE, "ci_boot_assert.py")
LIVE = re.compile(rb"virtio-blk: MODERN \d+ MB \(\d+ sectors, q=\d+\) "
                  rb"-- persistent disk live")
FAIL = re.compile(rb"virtio-blk modern:[^\n]*")
DISK = ("-drive if=none,id=d0,file={image},format=raw "
        "-device virtio-blk-pci,bus={bus},drive=d0,"
        "disable-legacy=on,disable-modern=off")
BRIDGE = ("-device pci-bridge,chassis_nr=1,id=pci.1,bus=pci.0,addr=0x1c ")


def boot(kernel, extra, window):
    with tempfile.TemporaryDirectory(prefix="wubu-modern-bus-") as d:
        image = os.path.join(d, "disk.raw")
        open(image, "wb").truncate(1 << 20)
        env = os.environ.copy()
        env["WUBU_QEMU_EXTRA"] = extra.format(image=image, bus=extra_bus(extra))
        env["WUBU_BOOT_WINDOW"] = str(window)
        r = subprocess.run([sys.executable, ASSERT, kernel, "--kernel"],
                           cwd=d, env=env, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, timeout=window + 30)
        serial = os.path.join(d, "ci_serial.bin")
        if not os.path.exists(serial):
            print("FAIL: boot produced no serial capture; rc=%d\n%s" %
                  (r.returncode, r.stdout.decode("latin1")[-500:]))
            return None
        return open(serial, "rb").read()


def extra_bus(extra):
    return "pci.1" if "pci.1" in extra.split("virtio-blk-pci,")[0] else "pci.0"


def check(kernel, label, extra, window):
    data = boot(kernel, extra, window)
    if data is None:
        return False
    hits = LIVE.findall(data)
    print("[%s] live=%s" % (label, hits[-1].decode() if hits else "NO"))
    if hits:
        return True
    for line in FAIL.findall(data)[-4:]:
        print("  %s" % line.decode("latin1"))
    print("FAIL: %s boot did not bind a live modern virtio-blk" % label)
    return False


def main():
    args = sys.argv[1:]
    window = 60
    if "--window" in args:
        i = args.index("--window")
        window = int(args[i + 1])
        del args[i:i + 2]
    kernel = os.path.abspath(args[0] if args else
                             os.path.join(HERE, "..", "src", "kernel", "kernel.elf"))
    root = check(kernel, "root-bus", DISK, window)
    bridged = check(kernel, "pci-bridge", BRIDGE + DISK, window)
    if not root or not bridged:
        print("FAIL: modern virtio must be live on BOTH PCI bus 0 and a bridge")
        return 1
    print("[pci-bridge-modern] PASS: same modern disk is live on root bus "
          "and behind a real QEMU pci-bridge")
    return 0


if __name__ == "__main__":
    sys.exit(main())
