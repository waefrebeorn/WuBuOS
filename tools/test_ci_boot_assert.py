#!/usr/bin/env python3
"""Unit gate for ci_boot_assert's driver-registry check.

A boot that reaches the prompt but never mapped a BAR must FAIL; otherwise
D1/D5's boot evidence is theatre. Uses the REAL function from the CI tool.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ci_boot_assert import assert_driver_registry

CASES = [
    (b"WuBuOS: 1 device MMIO BARs mapped\n"
     b"WuBuOS: driver registry 7 devices / 8 drivers registered, 7 bound, 1 MMIO BARs mapped\n",
     0, "one mapped BAR + registry report"),
    (b"WuBuOS: 12 device MMIO BARs mapped\n"
     b"WuBuOS: driver registry 7 devices / 8 drivers registered, 7 bound, 12 MMIO BARs mapped\n",
     0, "many mapped BARs + registry report"),
    (b"WuBuOS> prompt only, no registry line\n", 1, "missing map line"),
    (b"WuBuOS: 0 device MMIO BARs mapped\n", 1, "zero mapped BARs"),
    (b"WuBuOS: 2 devices MMIO BARs mapped\n", 1, "wrong wording, no match"),
    (b"WuBuOS: 1 device MMIO BARs mapped\n", 1, "registry report line missing"),
    (b"WuBuOS: 1 device MMIO BARs mapped\n"
     b"WuBuOS: driver registry 6 devices / 18 drivers registered, 6 bound, 2 MMIO BARs mapped\n",
     1, "registry count disagrees with mapper"),
]

failures = 0
for data, want, name in CASES:
    got = assert_driver_registry(data)
    if got != want:
        print("FAIL: %s: got %d, want %d" % (name, got, want))
        failures += 1
    else:
        print("ok: %s (rc=%d)" % (name, got))

if failures:
    print("test_ci_boot_assert: %d FAILURES" % failures)
    sys.exit(1)
print("test_ci_boot_assert: ALL PASS")
