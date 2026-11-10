#!/bin/bash
# sb-gate.sh -- WuBuFW Secure Boot enrollment + enforcement gate.
#
# Two halves, both must pass:
#   1. host tool (mksb, the sbctl equivalent): create-keys -> sign -> verify,
#      and a tampered cert must be rejected.
#   2. firmware: the ESL enrollment selftest (PK/KEK/db as real UEFI variables,
#      setup->user transition, enforcement) runs at boot and must PASS, and the
#      signed/unsigned PE enforcement must PASS.
#
# Reuses run-agi.sh for the full measured boot (it builds the firmware, the
# loader, the ESP, and boots the real kernel).
set -euo pipefail
cd "$(dirname "$0")"
CC=${CC:-gcc}

echo "=== 1/3 host tool: mksb (sbctl equivalent) ==="
$CC -std=c11 -Wall -Wextra -O2 tools/mksb.c -o build/mksb
./build/mksb selftest

echo "=== 2/3 full measured boot (firmware + ESP + kernel) ==="
bash run-agi.sh >/dev/null

echo "=== 3/3 Secure Boot assertions ==="
grep -q "\[sb\] esl enrollment selftest: PASS"      build/agi-boot.log
grep -q "\[sb\] signed payload verifies under enforcement: PASS"   build/agi-boot.log
grep -q "\[sb\] unsigned payload rejected under enforcement: PASS" build/agi-boot.log
echo "SECURE BOOT GATE PASS: PK/KEK/db enrollment + setup->user + enforcement + mksb sign/verify"
