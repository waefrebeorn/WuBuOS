#!/usr/bin/env python3
"""virtio_persist_gate.py -- L1 END-TO-END gate: real storage persistence.

Boots the SAME raw disk image twice under QEMU with a legacy virtio-blk
device. Boot 1: the kernel formats the bare disk, mounts FAT32 on the
virtio backend and writes a deterministic marker file (PERSIST-WROTE).
Boot 2: a FRESH QEMU process (all guest RAM gone) reads the marker back
through the same driver and verifies every byte (PERSIST-OK). A RAM sim
disk can never produce PERSIST-OK here -- the bytes must have survived
in the image file on the host.

Pattern: tools/ci_boot_assert.py. Usage:
    python3 tools/virtio_persist_gate.py [kernel.elf] [--window SECS]
Exit: 0 persistence proven, 1 gate failure, 2 qemu missing.
"""
import os
import subprocess
import sys
import time

MARKER_WROTE = b"PERSIST-WROTE"
MARKER_OK = b"PERSIST-OK"
MARKER_FAIL = b"PERSIST-FAIL"


def boot(elf, image, trace, expect, window):
    """Boot once; stop as soon as `expect` appears (or the window ends)."""
    if os.path.exists(trace):
        os.remove(trace)
    cmd = ["qemu-system-x86_64", "-kernel", elf,
           "-drive", "id=vblk0,file=%s,format=raw,if=none" % image,
           "-device", "virtio-blk-pci,drive=vblk0,disable-modern=on",
           "-serial", "file:" + trace,
           "-display", "none", "-m", "128", "-no-reboot"]
    try:
        proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
    except FileNotFoundError:
        print("FAIL: qemu-system-x86_64 not on PATH")
        sys.exit(2)
    data = b""
    deadline = time.time() + window
    try:
        while time.time() < deadline:
            if proc.poll() is not None:
                break                      # guest exited (debug-exit/fault)
            time.sleep(0.5)
            if os.path.exists(trace):
                data = open(trace, "rb").read()
                if expect in data or MARKER_FAIL in data:
                    break
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
    if os.path.exists(trace):
        data = open(trace, "rb").read()
    return data


def relevant(data):
    keep = []
    for line in data.splitlines():
        if (b"virtio" in line or b"PERSIST" in line or
                b"boot FAT32" in line or b"WuBuOS>" in line):
            keep.append(line.decode(errors="replace"))
    return keep


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    elf = args[0] if args else "src/kernel/kernel.elf"
    window = int(os.environ.get("WUBU_BOOT_WINDOW", "90"))
    for i, a in enumerate(sys.argv):
        if a == "--window" and i + 1 < len(sys.argv):
            window = int(sys.argv[i + 1])
    if not os.path.exists(elf):
        print("FAIL: kernel elf not found: %s (run make kernel)" % elf)
        return 1

    os.makedirs("build", exist_ok=True)
    image = os.path.abspath("build/virtio_persist_gate.img")
    # a FRESH bare 16 MB raw disk every gate run
    with open(image, "wb") as f:
        f.truncate(16 * 1024 * 1024)

    print("== L1 virtio-blk persistence gate ==")
    print("image: %s (16 MB raw, fresh)" % image)

    print("-- boot 1: write the marker through FAT32 on virtio-blk --")
    d1 = boot(elf, image, "build/virtio_persist_boot1.bin",
              MARKER_WROTE, window)
    for line in relevant(d1):
        print("  | " + line)
    if MARKER_FAIL in d1:
        print("FAIL: boot 1 reported PERSIST-FAIL")
        return 1
    if MARKER_WROTE not in d1:
        print("FAIL: boot 1 never wrote the marker (%d serial bytes)"
              % len(d1))
        return 1
    if b"WuBuOS>" not in d1:
        print("FAIL: boot 1 lost the interactive console (boot regression)")
        return 1

    print("-- boot 2: FRESH QEMU process, same image -- read it back --")
    d2 = boot(elf, image, "build/virtio_persist_boot2.bin",
              MARKER_OK, window)
    for line in relevant(d2):
        print("  | " + line)
    if MARKER_FAIL in d2:
        print("FAIL: boot 2 reported PERSIST-FAIL (marker corrupted)")
        return 1
    if MARKER_OK not in d2:
        print("FAIL: boot 2 could not read the marker back (%d serial bytes)"
              % len(d2))
        return 1
    if b"WuBuOS>" not in d2:
        print("FAIL: boot 2 lost the interactive console (boot regression)")
        return 1

    print("PASS: PERSIST-OK -- marker survived write -> reboot -> read-back "
          "on real virtio-blk storage")
    return 0


if __name__ == "__main__":
    sys.exit(main())