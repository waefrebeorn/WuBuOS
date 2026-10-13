#!/bin/bash
# qemu-boot-verify.sh -- verify WuBuOS boots as a QEMU guest (the dev target).
#
# We are QEMU dev users: WuBuOS is developed/tested as a QEMU guest (bare-metal
# WuBuFW UEFI firmware on q35, C11, no EDK2/OVMF). This script builds the
# firmware, boots it under QEMU with the ESP attached, and greps the serial
# console for the boot milestones that back the KERNEL P0 goals:
#   - PCI/driver binding (kernel init)
#   - secureboot measured-boot verifier (KERNEL #9: fail-closed, signed PASS,
#     unsigned REJECT)
#   - TPM PCR extend math
#   - allocator/mem stats via the shell 'mem' command (KERNEL #11)
#   - ESP filesystem visible via 'ls' (9P attach path foundation)
# Exits 0 only if every milestone is observed. Deterministic, no assumptions.
set -euo pipefail

FW_DIR="$(cd "$(dirname "$0")" && pwd)"
ESP="$FW_DIR/build/wubu-esp.img"
FD="$FW_DIR/wubufw.fd"
QEMU_TIMEOUT="${QEMU_TIMEOUT:-30}"

echo "[qemu-boot-verify] building WuBuFW firmware..."
( cd "$FW_DIR" && bash build.sh >/dev/null 2>&1 )

if [ ! -f "$FD" ]; then echo "FAIL: firmware $FD not built"; exit 1; fi
if [ ! -f "$ESP" ]; then echo "FAIL: ESP $ESP not present"; exit 1; fi

echo "[qemu-boot-verify] booting under qemu-system-x86_64 (q35, ESP attached)..."
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

( printf 'help\n'; sleep 1; printf 'mem\n'; sleep 1; printf 'ls\n'; sleep 1 ) \
  | timeout "$QEMU_TIMEOUT" qemu-system-x86_64 \
      -machine q35 -bios "$FD" \
      -drive "file=$ESP,format=raw,if=none,id=esp" \
      -device ahci,id=ahci -device ide-hd,drive=esp,bus=ahci.0 \
      -m 512 -serial stdio -display none -no-reboot \
  > "$LOG" 2>&1 || true

# milestones -> grep pattern (serial has CR, use -a)
check() { if grep -aqE "$2" "$LOG"; then echo "  [PASS] $1"; else echo "  [FAIL] $1"; FAILED=1; fi; }
FAILED=0
check "WuBuFW UEFI firmware banner (C11, no EDK2/OVMF)" 'WuBuFW.*UEFI firmware'
check "PCI bus probe (kernel device init)" '\[pci\] probe 0:0.0'
check "BAR/MMIO resource assignment" '\[pcires\].*BAR0'
check "AHCI ESP disk detected" '\[ahci\] port 0:.*sectors'
check "secureboot policy engine selftest PASS" '\[sb\] secureboot policy engine selftest: PASS'
check "signed payload verifies under enforcement (KERNEL #9)" '\[sb\] signed payload verifies under enforcement: PASS'
check "unsigned payload rejected under enforcement (KERNEL #9 fail-closed)" '\[sb\] unsigned payload rejected under enforcement: PASS'
check "TPM PCR extend math (SHA-256 TCG) PASS" '\[tpm\] PCR extend math.*PASS'
check "interactive shell reached" 'WuBuFW interactive shell'
check "allocator/mem stats (KERNEL #11)" 'free MB approx'
check "ESP filesystem listed (9P attach foundation)" 'd .* EFI'

if [ "$FAILED" = 1 ]; then
  echo "[qemu-boot-verify] FAILED — serial log tail:"; tail -30 "$LOG"
  exit 1
fi
echo "[qemu-boot-verify] ALL MILESTONES PASSED — WuBuOS boots as a QEMU guest."
