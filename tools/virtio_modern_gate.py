#!/usr/bin/env python3
"""virtio_modern_gate.py -- boot a modern-only virtio disk under QEMU.

The WuBuOS boot diagnostic reads the PCI capability list and common
config of a 1AF4:1042 device with legacy I/O disabled. This gate proves
the real MMIO register path on QEMU: four virtio caps, feature word 1
(VIRTIO_F_VERSION_1) present, and the expected notification offset.
It does NOT claim a bound disk or block I/O.

Usage: python3 tools/virtio_modern_gate.py [kernel.elf] [--window SECS]
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ASSERT = os.path.join(HERE, "ci_boot_assert.py")
LINE = re.compile(
    rb"WuBuOS: modern virtio-blk 1af4:1042 caps=(\d+) common=(\d+) "
    rb"notify=(\d+) feat_lo=([0-9a-f]+) feat_hi=([0-9a-f]+) status=([0-9a-f]+)")


def main():
    args = sys.argv[1:]
    window = 60
    if "--window" in args:
        i = args.index("--window")
        window = int(args[i + 1])
        del args[i:i + 2]
    kernel = os.path.abspath(args[0] if args else
                             os.path.join(HERE, "..", "src", "kernel", "kernel.elf"))

    with tempfile.TemporaryDirectory(prefix="wubu-virtio-modern-") as d:
        image = os.path.join(d, "disk.raw")
        open(image, "wb").truncate(1 << 20)
        env = os.environ.copy()
        env["WUBU_QEMU_EXTRA"] = (
            "-drive if=none,id=d0,file=%s,format=raw "
            "-device virtio-blk-pci,drive=d0,disable-legacy=on,disable-modern=off"
            % image)
        env["WUBU_BOOT_WINDOW"] = str(window)
        r = subprocess.run([sys.executable, ASSERT, kernel, "--kernel"],
                           cwd=d, env=env, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, timeout=window + 30)
        serial = os.path.join(d, "ci_serial.bin")
        if not os.path.exists(serial):
            print("FAIL: no serial capture; rc=%d\n%s" %
                  (r.returncode, r.stdout.decode("latin1")[-500:]))
            return 1
        hits = LINE.findall(open(serial, "rb").read())
        if r.returncode != 0:
            print("FAIL: boot assert rc=%d\n%s" %
                  (r.returncode, r.stdout.decode("latin1")[-500:]))
            return 1
        if not hits:
            print("FAIL: no modern virtio-blk capability line on QEMU")
            return 1
        caps, common, notify, lo, hi, status = hits[-1]
        print("[virtio-modern] caps=%s common=%s notify=%s feat_lo=%s "
              "feat_hi=%s status=%s" %
              (caps.decode(), common.decode(), notify.decode(), lo.decode(),
               hi.decode(), status.decode()))
        if int(caps) != 4 or int(common) != 0 or int(notify) == 0 or \
           int(hi, 16) & 1 == 0:
            print("FAIL: modern capability layout or VERSION_1 feature missing")
            return 1
    print("[virtio-modern] PASS: real QEMU modern-only virtio-blk "
          "capability/common-config path (read-only, no disk bound)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
