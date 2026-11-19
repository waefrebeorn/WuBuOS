#!/usr/bin/env python3
"""ci_boot_assert.py -- boot WuBuOS in QEMU on ANY host, assert a healthy trace.

The multi-host proof: one GRUB/multiboot artifact, booted under QEMU on
Linux, Windows, and macOS CI runners (and on user machines). Healthy boot
per skill wubuos-interboot-deploy: ~3700-4600 serial bytes reaching the
interactive WuBuOS> console; 38/106 bytes are the known regression
ceilings (crt0 entry / multiboot-base / SSE bugs, all fixed).

Usage: python3 ci_boot_assert.py <wubuos.iso | kernel.elf> [--kernel]
Exit: 0 healthy, 1 boot regression, 2 qemu missing.
"""
import os
import subprocess
import sys

def main():
    if len(sys.argv) < 2:
        print("usage: ci_boot_assert.py <iso-or-elf> [--kernel]")
        return 2
    image = sys.argv[1]
    flag = "-kernel" if "--kernel" in sys.argv else "-cdrom"
    serial = "ci_serial.bin"
    if os.path.exists(serial):
        os.remove(serial)
    cmd = ["qemu-system-x86_64", flag, image,
           "-serial", "file:" + serial,
           "-display", "none", "-m", "128", "-no-reboot"]
    # CI runners under TCG are ~8x slower than a dev box: the window is
    # configurable (WUBU_BOOT_WINDOW seconds; CI sets 300, local default 45).
    window = int(os.environ.get("WUBU_BOOT_WINDOW", "45"))
    try:
        subprocess.run(cmd, timeout=window,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except subprocess.TimeoutExpired:
        pass  # expected: the boot window ends, we read the trace
    except FileNotFoundError:
        print("FAIL: qemu-system-x86_64 not on PATH")
        return 2
    data = open(serial, "rb").read() if os.path.exists(serial) else b""
    n = len(data)
    print("serial bytes: %d" % n)
    # Threshold recalibrated 2026-11-19: after the WuBuKernel PMM
    # consumption (f487691) a healthy boot measures ~2000 bytes (was
    # 3700-4600 pre-refactor). 1500 stays far above the 38/106-byte
    # regression ceilings; the console marker below is the real truth.
    if n < 1500:
        print("FAIL: boot trace too short (%d < 1500) — a boot-bug "
              "regression ceiling (38/106 bytes)" % n)
        return 1
    if b"WuBuOS>" not in data:
        print("FAIL: interactive console marker WuBuOS> missing")
        return 1
    print("PASS: WuBuOS booted to the interactive console on this host")
    return 0

if __name__ == "__main__":
    sys.exit(main())