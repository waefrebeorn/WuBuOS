#!/usr/bin/env python3
"""pci_bridge_gate.py -- prove the metal PCI scan walks a real bridge.

Boots the same kernel twice under QEMU: once on the default root bus and
once with a pci-bridge on bus 0. The driver registry must gain exactly the
bridge device (secondary bus devices would also count if attached).

Pattern: tools/ci_boot_assert.py. Usage:
    python3 tools/pci_bridge_gate.py [kernel.elf] [--window SECS]

Both boots run in separate temp directories because ci_boot_assert.py
writes its serial capture as `ci_serial.bin` in the current directory.
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ASSERT = os.path.join(HERE, "ci_boot_assert.py")
BRIDGE_ARGS = ("-device pci-bridge,chassis_nr=1,id=pci.1,bus=pci.0,addr=0x1c "
               "-drive if=none,id=d0,file={image},format=raw "
               "-device virtio-blk-pci,bus=pci.1,drive=d0,disable-modern=on")
LINE = re.compile(rb"WuBuOS: driver registry (\d+) devices / \d+ drivers "
                  rb"registered, \d+ bound, \d+ MMIO BARs mapped")


def boot_count(kernel, extra, window):
    with tempfile.TemporaryDirectory(prefix="wubu-pci-bridge-") as d:
        env = os.environ.copy()
        env["WUBU_QEMU_EXTRA"] = extra
        env["WUBU_BOOT_WINDOW"] = str(window)
        r = subprocess.run([sys.executable, ASSERT, kernel, "--kernel"],
                           cwd=d, env=env, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, timeout=window + 30)
        serial = os.path.join(d, "ci_serial.bin")
        if not os.path.exists(serial):
            print("FAIL: boot produced no serial capture; rc=%d\n%s" %
                  (r.returncode, r.stdout.decode("latin1")[-500:]))
            return None
        data = open(serial, "rb").read()
        hits = LINE.findall(data)
        if not hits:
            print("FAIL: boot produced no driver registry line; rc=%d\n%s" %
                  (r.returncode, r.stdout.decode("latin1")[-500:]))
            return None
        if r.returncode != 0:
            print("FAIL: boot assert rc=%d\n%s" %
                  (r.returncode, r.stdout.decode("latin1")[-500:]))
            return None
        return int(hits[-1])


def main():
    args = sys.argv[1:]
    window = 60
    if "--window" in args:
        i = args.index("--window")
        window = int(args[i + 1])
        del args[i:i + 2]
    kernel = os.path.abspath(args[0] if args else
                             os.path.join(HERE, "..", "src", "kernel", "kernel.elf"))
    base = boot_count(kernel, "", window)
    if base is None:
        return 1
    with tempfile.TemporaryDirectory(prefix="wubu-bridge-disk-") as staging:
        image = os.path.join(staging, "disk.raw")
        open(image, "wb").truncate(1 << 20)   # QEMU needs a real file
        bridged = boot_count(kernel, BRIDGE_ARGS.format(image=image), window)
    if bridged is None:
        return 1
    print("[pci-bridge] baseline %d devices, with bridge+disk %d devices" %
          (base, bridged))
    if bridged != base + 2:
        print("FAIL: bridge scan did not add the bridge and its downstream "
              "disk (expected %d, got %d)" % (base + 2, bridged))
        return 1
    print("[pci-bridge] PASS: real QEMU pci-bridge discovered by the metal scan")
    return 0


if __name__ == "__main__":
    sys.exit(main())
