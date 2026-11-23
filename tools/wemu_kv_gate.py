#!/usr/bin/env python3
"""Wave 3 gate: ring-0 KV namespace state survives a real reboot.

Boot 1 calls `kv write 11 22 33 44`: four floats go through wubu_kvfs
path routing and a compact state file is written to the FAT32 boot volume.
Boot 2, a fresh emulator process against the same image, calls `kv read`
and must see the same four values served back through the KV namespace.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG = os.path.join(ROOT, "build", "wemu_kv_gate.img")
HELPER = os.path.join(ROOT, "build", "make_fat_image")
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

    print("== WEMU ring-0 KVFS persistence gate ==")
    env["WEMU_KERNEL_SEND"] = "kv write 11 22 33 44"
    env["WEMU_KERNEL_EXPECT"] = "kv: wrote 11 22 33 44"
    out1 = run([GATE, "src/kernel/kernel.elf"], env, check=False)
    if b"kv: wrote 11 22 33 44" not in out1:
        print("FAIL: boot 1 did not write KV state")
        print(out1.decode("utf-8", "replace")[-600:])
        return 1
    print("  boot 1: kv write 11 22 33 44")

    env.pop("WEMU_KERNEL_SEND", None)
    env.pop("WEMU_KERNEL_EXPECT", None)
    env["WEMU_KERNEL_SEND"] = "kv read"
    env["WEMU_KERNEL_EXPECT"] = "kv state 11 22 33 44"
    out2 = run([GATE, "src/kernel/kernel.elf"], env, check=False)
    if b"kv state 11 22 33 44" not in out2:
        print("FAIL: boot 2 did not read KV state after reboot")
        print(out2.decode("utf-8", "replace")[-600:])
        return 1
    print("  boot 2: kv state 11 22 33 44")
    print("PASS: ring-0 KV namespace state survived write -> reboot -> read")
    return 0


if __name__ == "__main__":
    sys.exit(main())
