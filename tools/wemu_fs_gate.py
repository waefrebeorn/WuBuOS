#!/usr/bin/env python3
"""Wave 0 gate: a human file survives write -> reboot -> read on metal in WEMU.

Boots the real kernel twice through WEMU against the SAME 1 MiB FAT32 image.
Boot 1 types `write HELLO.TXT world`; boot 2 (a fresh emulator process)
types `cat HELLO.TXT` and must read `world` off the emulated disk. This is a
behavior gate: a RAM sim disk, a dropped T_OUT, or a missing console file
command cannot produce `HELLO.TXT: 5 bytes` on the second boot.
"""
import os
import atexit
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG = os.path.join(ROOT, "build", "wemu_fs_gate.img")
# Per-process helper path: four WEMU gates build this same helper and run it,
# and under `make -j24 test` they used to share one output file -- one gate
# exec'ing it while another had it open for writing, which is ETXTBSY at best
# and a half-written binary at worst.
HELPER = os.path.join(ROOT, "build", "make_fat_image.%d" % os.getpid())
atexit.register(lambda: os.path.exists(HELPER) and os.unlink(HELPER))
KERNEL = os.path.join(ROOT, "src", "kernel", "kernel.elf")
GATE = os.path.join(ROOT, "tools", "wemu_kernel_gate")
FAT_SOURCES = ["fat32_format.c", "fat32.c", "fat32_fat.c", "fat32_dir.c",
               "fat32_name.c", "fat32_cluster.c", "fat32_file.c", "wubu_lfn.c"]


def run(cmd, env=None, check=True):
    p = subprocess.run(cmd, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT)
    if check and p.returncode != 0:
        raise RuntimeError("command failed: %s\n%s" % (
            " ".join(cmd), p.stdout.decode("utf-8", "replace")[-600:]))
    return p.stdout


def main():
    if not os.path.exists(KERNEL) or not os.path.exists(GATE):
        print("FAIL: build make kernel + make test_wemu_kernel first")
        return 1
    os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)
    srcs = ["tools/make_fat_image.c"] + [
        "src/kernel/" + s for s in FAT_SOURCES]
    run(["gcc", "-std=c11", "-O2", "-Isrc/kernel", "-Iinclude"] + srcs +
        ["-o", HELPER, "-lm"])
    run([HELPER, IMG])

    env = dict(os.environ)
    env["WEMU_VIRTIO_DISK"] = IMG
    env["WEMU_KERNEL_STEPS"] = "200000000"
    env["WEMU_KERNEL_MARKER"] = "WuBuOS>"

    print("== WEMU metal file persistence gate ==")
    env["WEMU_KERNEL_SEND"] = "write HELLO.TXT world"
    env["WEMU_KERNEL_EXPECT"] = "fs: wrote HELLO.TXT"
    out1 = run([GATE, "src/kernel/kernel.elf"], env, check=False)
    if b"fs: wrote HELLO.TXT" not in out1:
        print("FAIL: boot 1 did not write HELLO.TXT")
        print(out1.decode("utf-8", "replace")[-600:])
        return 1
    print("  boot 1: wrote HELLO.TXT")

    env.pop("WEMU_KERNEL_SEND", None)
    env.pop("WEMU_KERNEL_EXPECT", None)
    env["WEMU_KERNEL_SEND"] = "cat HELLO.TXT"
    env["WEMU_KERNEL_EXPECT"] = "HELLO.TXT: 5 bytes"
    out2 = run([GATE, "src/kernel/kernel.elf"], env, check=False)
    if b"world\nfs: HELLO.TXT: 5 bytes" not in out2:
        print("FAIL: boot 2 did not read `world` back from the same image")
        print(out2.decode("utf-8", "replace")[-600:])
        return 1
    print("  boot 2: read `world` from the same image")
    print("PASS: metal console file survived write -> reboot -> read")
    return 0


if __name__ == "__main__":
    sys.exit(main())
