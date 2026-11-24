#!/usr/bin/env python3
"""Execute the ARM64 backend's emitted code, for real, under qemu-aarch64-static.

Why this exists
---------------
test_isa_driver could only check that the ARM64 encoder produced non-empty
bytes: AArch64 cannot execute on x86-64, so ARM64 was "encode-only" there. That
is the same trap QEMU described for its own tests/tcg directory -- tests that
cannot run bit-rot, and nobody notices:

    "most developers never ran them. As the tests were hardly ever built
     inevitably a certain amount of bit-rot set in."
    -- https://www.qemu.org/2018/06/21/tcg-testing/

QEMU's answer was to make the cross-target tests runnable (cross compilers +
containers). The cheap version of that answer here: qemu-user-static needs no
root, so a raw code blob plus a six-instruction stub is a runnable AArch64
program.

How
---
test_isa_driver, run with WUBU_ARM64_EXEC_DIR set, dumps
    <case>.bin   the bytes the ARM64 backend emitted
    <case>.want  the value those bytes must compute
for every comparison-battery case. This gate wraps each blob in a minimal
ELF64 (one PT_LOAD, entry at the stub) and runs it:

    BL   code        ; x30 = stub+4, so the backend's RET returns here
    MOVZ x8, #93     ; __NR_exit
    SVC  #0          ; exit(x0)   <- x0 is where the ABI puts the return value
    RET

The process exit status is (x0 & 0xFF), so the comparison is on the low byte:
MIR return values in these cases are small, and -5 must come back as 251.

A blob that does not run, or runs to the wrong value, is a real bug in the
backend -- not a missing feature.
"""

import os
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLOB_DIR = os.path.join(ROOT, "build", "arm64probe")
BASE = 0x400000
ENTRY_OFF = 64 + 56            # ELF header + one program header

QEMU_CANDIDATES = [
    os.path.expanduser("~/opt/qemu-user/usr/bin/qemu-aarch64-static"),
    "/usr/bin/qemu-aarch64-static",
    "/usr/bin/qemu-aarch64",
]

MOVZ = lambda rd, imm: 0xD2800000 | ((imm & 0xFFFF) << 5) | rd
SVC0 = 0xD4000001
RET = 0xD65F03C0
BL = lambda byte_off: 0x94000000 | ((byte_off >> 2) & 0x03FFFFFF)


def aarch64_elf(code, base=BASE):
    """Minimal ET_EXEC AArch64 ELF: stub, then the emitted code, one PT_LOAD."""
    stub = struct.pack("<IIII", BL(16), MOVZ(8, 93), SVC0, RET)
    body = stub + code
    e_ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + b"\x00" * 8
    ehdr = e_ident + struct.pack(
        "<HHIQQQIHHHHHH",
        2,          # ET_EXEC
        183,        # EM_AARCH64
        1,          # EV_CURRENT
        base + ENTRY_OFF,
        64,         # e_phoff
        0,          # e_shoff
        0,          # e_flags
        64, 56, 1, 0, 0, 0)
    phdr = struct.pack("<IIQQQQQQ",
                       1,      # PT_LOAD
                       7,      # rwx
                       0, base, base,
                       len(body), len(body), 0x1000)
    return ehdr + phdr + body


def find_qemu():
    for p in QEMU_CANDIDATES:
        if os.path.exists(p) and os.access(p, os.X_OK):
            return p
    return None


def main():
    qemu = find_qemu()
    if not qemu:
        print("arm64_exec_gate: SKIP (no qemu-aarch64-static; extract one with")
        print("  apt-get download qemu-user-static && dpkg-deb -x ./*.deb ~/opt/qemu-user)")
        return 0
    if not os.path.isdir(BLOB_DIR):
        print("arm64_exec_gate: SKIP (no %s -- run the dumper first)" % BLOB_DIR)
        return 0

    cases = sorted(f[:-4] for f in os.listdir(BLOB_DIR) if f.endswith(".bin"))
    if not cases:
        print("arm64_exec_gate: FAIL (no cases dumped)")
        return 1

    print("== ARM64 execution gate (%s) ==" % qemu)
    fails = 0
    for name in cases:
        blob = os.path.join(BLOB_DIR, name + ".bin")
        wantf = os.path.join(BLOB_DIR, name + ".want")
        code = open(blob, "rb").read()
        try:
            want = int(open(wantf).read().strip())
        except (OSError, ValueError):
            print("  FAIL %-22s (no .want file)" % name)
            fails += 1
            continue
        with tempfile.NamedTemporaryFile(suffix=".a64", delete=False) as fh:
            fh.write(aarch64_elf(code))
            exe = fh.name
        try:
            os.chmod(exe, 0o755)
            p = subprocess.run([qemu, exe], stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, timeout=20)
            got = p.returncode
            # exit status is the low byte of x0; map back to signed for display
            signed = got - 256 if got >= 128 else got
            ok = (got & 0xFF) == (want & 0xFF)
            if not ok:
                fails += 1
                print("  FAIL %-22s got %d (exit %d) want %d%s" % (
                    name, signed, got, want,
                    "" if p.returncode >= 0 else "  [qemu could not run it]"))
            else:
                print("  PASS %-22s x0&0xFF=%d (want %d)" % (name, got, want))
        except subprocess.TimeoutExpired:
            fails += 1
            print("  FAIL %-22s (qemu timed out)" % name)
        finally:
            os.unlink(exe)

    print("== %d case(s), %d failed ==" % (len(cases), fails))
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
