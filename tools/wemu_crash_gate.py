#!/usr/bin/env python3
"""Wave 0 gate: a crash record survives a real reboot on metal in WEMU.

Boot 1 forces a crash from the console and writes CRASH.DMP to the FAT32
boot volume. Boot 2 is a fresh emulator process against the SAME image;
it must report `crash record found` during startup. A RAM sim-disk record
or a boot-time pickup before the volume is mounted cannot pass this.
"""
import os
import atexit
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG = os.path.join(ROOT, "build", "wemu_crash_gate.img")
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

    print("== WEMU crash persistence gate ==")
    env["WEMU_KERNEL_SEND"] = "crash"
    env["WEMU_KERNEL_EXPECT"] = "wrote CRASH.DMP"
    out1 = run([GATE, "src/kernel/kernel.elf"], env, check=False)
    if b"wrote CRASH.DMP seq=1" not in out1:
        print("FAIL: boot 1 did not write CRASH.DMP")
        print(out1.decode("utf-8", "replace")[-600:])
        return 1
    print("  boot 1: wrote CRASH.DMP seq=1")

    env.pop("WEMU_KERNEL_SEND", None)
    env.pop("WEMU_KERNEL_EXPECT", None)
    env["WEMU_KERNEL_MARKER"] = "crash record found"
    env["WEMU_KERNEL_SHOW_SERIAL"] = "1"
    out2 = run([GATE, "src/kernel/kernel.elf"], env, check=False)
    if b"crash record found (seq=1" not in out2:
        print("FAIL: boot 2 did not find the crash record after reboot")
        print(out2.decode("utf-8", "replace")[-600:])
        return 1
    print("  boot 2: found seq=1 in a fresh process")
    print("PASS: crash record survived write -> reboot -> pickup")
    return 0


if __name__ == "__main__":
    sys.exit(main())
