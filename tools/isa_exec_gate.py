#!/usr/bin/env python3
"""Execute the cross-target ISA backends' emitted code, for real, under qemu-user.

Why this exists
---------------
A backend that emits for a foreign CPU cannot run on x86-64, so a test can only
assert "bytes came out". QEMU described exactly what happens to tests in that
state, about its own tests/tcg directory:

    "most developers never ran them. As the tests were hardly ever built
     inevitably a certain amount of bit-rot set in."
    -- https://www.qemu.org/2018/06/21/tcg-testing/

Their answer was making cross-target tests runnable. The cheap version here:
qemu-user-static needs no root, so a raw code blob plus a few instructions of
stub IS a runnable program for that CPU.

How
---
test_isa_driver, run with WUBU_EXEC_DUMP=arm64,riscv,... and
WUBU_EXEC_DUMP_DIR=<dir>, writes for every comparison-battery case:

    <dir>/<target>/<case>.bin    the bytes that backend emitted
    <dir>/<target>/<case>.want   the value those bytes must compute

This gate wraps each blob in a minimal ELF for that architecture (one PT_LOAD,
entry at the stub), runs it under the matching qemu-user emulator, and compares
the process exit status with the expected value, masked to the low byte (a MIR
return value such as -5 comes back as 251).

A blob that does not run, or runs to the wrong value, is a bug in the backend --
not a missing feature. Missing features belong in test_isa_driver's
g_known_gaps[] with a reason, so they are printed rather than hidden.
"""

import os
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLOB_DIR = os.path.join(ROOT, "build", "isaexec")
BASE = 0x400000
ENTRY_OFF = 64 + 56            # ELF header + one program header
QEMU_DIR = os.path.expanduser("~/opt/qemu-user/usr/bin")

# --- per-architecture pieces ------------------------------------------------

MOVZ = lambda rd, imm: 0xD2800000 | ((imm & 0xFFFF) << 5) | rd
BL = lambda byte_off: 0x94000000 | ((byte_off >> 2) & 0x03FFFFFF)


def elf(machine, body, base=BASE):
    e_ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + b"\x00" * 8
    ehdr = e_ident + struct.pack(
        "<HHIQQQIHHHHHH", 2, machine, 1, base + ENTRY_OFF,
        64, 0, 0, 64, 56, 1, 0, 0, 0)
    phdr = struct.pack("<IIQQQQQQ", 1, 7, 0, base, base,
                       len(body), len(body), 0x1000)
    return ehdr + phdr + body


def stub_arm64():
    # BL code ; MOVZ x8,#93 ; SVC #0 ; RET        -> exit(x0)
    return struct.pack("<IIII", BL(16), MOVZ(8, 93), 0xD4000001, 0xD65F03C0)


def stub_riscv64():
    # jal x1, +16 ; addi a7, x0, 93 ; ecall ; nop  -> exit(a0)
    jal = 0x010000EF                     # jal ra, +16
    addi_a7_93 = 0x05D00893             # addi a7, x0, 93
    ecall = 0x00000073
    nop = 0x00000013
    return struct.pack("<IIII", jal, addi_a7_93, ecall, nop)


def stub_mips():
    # MIPS32 BE? No: this tree's model is little-endian words; use the same
    # little-endian layout as the driver emits.
    # jal +16 ; addiu $v0,$zero,4001 ; syscall ; nop  (o32: exit=4001)
    # MIPS jal targets (PC+4)[31:28] | (imm26 << 2), so reaching stub+16 needs
    # imm = (16 - 4) / 4 = 3 -- unlike arm64/riscv, whose branches are relative
    # to the instruction's own PC. Using 16 here landed 4 bytes past the code.
    jal = 0x0C000000 | (((16 - 4) >> 2) & 0x03FFFFFF)
    addiu_v0 = (0x09 << 26) | (0 << 21) | (2 << 16) | (4001 & 0xFFFF)
    syscall_ = 0x0000000C
    nop = 0
    return struct.pack("<IIII", jal, addiu_v0, syscall_, nop)


# Targets whose emitted bytes are real machine code for that CPU, and so can be
# executed by qemu-user. The mips driver is NOT here: its control flow uses a
# custom 0xFC000000 escape (see MIR_RET in wubu_isa_mips.c) that only the in-tree
# interpreter models, so its output is a hybrid, not MIPS. Recorded rather than
# faked -- a green tick from qemu on a hybrid would mean nothing.
NOT_REAL_CODE = {
    "mips": "emits a custom 0xFC000000 control escape (in-tree interpreter only)",
}

TARGETS = {
    # name: (ELF e_machine, stub builder, qemu binary candidates)
    "arm64": (183, stub_arm64,
              ["qemu-aarch64-static", "qemu-aarch64"]),
    "riscv": (243, stub_riscv64,
              ["qemu-riscv64-static", "qemu-riscv64"]),
}


def find_qemu(cands):
    for c in cands:
        for d in (QEMU_DIR, "/usr/bin", "/usr/local/bin"):
            p = os.path.join(d, c)
            if os.path.exists(p) and os.access(p, os.X_OK):
                return p
    return None


def run_target(name, machine, stub, cands):
    qemu = find_qemu(cands)
    d = os.path.join(BLOB_DIR, name)
    if not qemu:
        print("  %s: SKIP (no qemu-user for this target; see script header)" % name)
        return 0, 0
    if not os.path.isdir(d):
        print("  %s: SKIP (no blobs dumped)" % name)
        return 0, 0
    cases = sorted(f[:-4] for f in os.listdir(d) if f.endswith(".bin"))
    fails = 0
    for case in cases:
        code = open(os.path.join(d, case + ".bin"), "rb").read()
        try:
            want = int(open(os.path.join(d, case + ".want")).read().strip())
        except (OSError, ValueError):
            print("  %s/%s: FAIL (no .want)" % (name, case))
            fails += 1
            continue
        with tempfile.NamedTemporaryFile(suffix=".elf", delete=False) as fh:
            fh.write(elf(machine, stub() + code))
            exe = fh.name
        try:
            os.chmod(exe, 0o755)
            p = subprocess.run([qemu, exe], stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, timeout=20)
            got = p.returncode
            ok = (got & 0xFF) == (want & 0xFF)
            signed = got - 256 if got >= 128 else got
            if not ok:
                fails += 1
                print("  %s/%s: FAIL got %d (exit %d) want %d%s" % (
                    name, case, signed, got, want,
                    "" if got >= 0 else "  [qemu could not run it]"))
        except subprocess.TimeoutExpired:
            fails += 1
            print("  %s/%s: FAIL (qemu timed out)" % (name, case))
        finally:
            os.unlink(exe)
    print("  %s: %d case(s), %d failed  [%s]" % (name, len(cases), fails,
                                                 os.path.basename(qemu)))
    return len(cases), fails


def main():
    if not os.path.isdir(BLOB_DIR):
        print("isa_exec_gate: SKIP (no %s -- run the dumper first)" % BLOB_DIR)
        return 0
    print("== ISA execution gate ==")
    for name, why in NOT_REAL_CODE.items():
        print("  %s: SKIP (not real machine code: %s)" % (name, why))
    total = fails = 0
    for name, (machine, stub, cands) in TARGETS.items():
        n, f = run_target(name, machine, stub, cands)
        total += n
        fails += f
    print("== %d case(s) executed, %d failed ==" % (total, fails))
    if total == 0:
        print("isa_exec_gate: SKIP (nothing dumped)")
        return 0
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
