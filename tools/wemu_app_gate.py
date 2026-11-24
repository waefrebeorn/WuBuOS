#!/usr/bin/env python3
"""Wave 0 gate: a script app on the boot disk runs with no host transfer.

Boots the real kernel twice through WEMU against the SAME image. Boot 1
uses the metal console to create APP.TXT containing `mem`. Boot 2 types
`run APP.TXT` with no 9P/COM2 host server; the guest must execute the
command from its own FAT32 disk and report both the command output and
`1 commands executed`.
"""
import os
import atexit
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG = os.path.join(ROOT, "build", "wemu_app_gate.img")
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

    print("== WEMU local app disk gate ==")
    env["WEMU_KERNEL_SEND"] = "write APP.TXT mem"
    env["WEMU_KERNEL_EXPECT"] = "fs: wrote APP.TXT"
    out1 = run([GATE, "src/kernel/kernel.elf"], env, check=False)
    if b"fs: wrote APP.TXT" not in out1:
        print("FAIL: boot 1 did not create APP.TXT")
        print(out1.decode("utf-8", "replace")[-600:])
        return 1

    env["WEMU_KERNEL_SEND"] = "run APP.TXT"
    env["WEMU_KERNEL_EXPECT"] = "commands executed"
    out2 = run([GATE, "src/kernel/kernel.elf"], env, check=False)
    if b"run: APP.TXT: 1 commands executed" not in out2:
        print("FAIL: boot 2 did not execute APP.TXT from the disk")
        print(out2.decode("utf-8", "replace")[-600:])
        return 1
    if b"heap used=" not in out2:
        print("FAIL: run reported success but the command produced no output")
        print(out2.decode("utf-8", "replace")[-600:])
        return 1
    print("PASS: APP.TXT executed from the boot disk with no host 9P server")
    return 0


if __name__ == "__main__":
    sys.exit(main())
