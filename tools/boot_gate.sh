#!/bin/sh
# boot_gate.sh -- boot on the path that actually boots.
#
# -kernel is NOT a boot test here: kernel.elf is a higher-half 64-bit image with
# no multiboot header, so QEMU's -kernel shim prints probe bytes and stops. It is
# kept as a diagnostic only.
#
# The supported path is WuBuFW, our own C11 UEFI firmware: it probes PCI, loads
# the ESP, reaches its shell and runs its milestones. That is what this gates.
#
# usage: sh tools/boot_gate.sh            (firmware path, authoritative)
#        sh tools/boot_gate.sh --kernel   (diagnose the -kernel path)
set -u

if [ "${1:-}" = "--kernel" ]; then
    KERNEL=src/kernel/kernel.elf
    OUT=$(mktemp); trap 'rm -f "$OUT"' EXIT
    timeout 25 qemu-system-x86_64 -kernel "$KERNEL" -nographic -m 128 \
            -no-reboot >"$OUT" 2>&1
    if grep -q 'kernel_main entered' "$OUT"; then
        echo "boot-gate(-kernel): PASS"
        exit 0
    fi
    echo "boot-gate(-kernel): expected to fail — no multiboot header, only probe bytes:"
    tail -c 120 "$OUT" | sed 's/^/  /'
    exit 1
fi

FW=src/firmware/wubufw.fd
if [ ! -f "$FW" ]; then
    echo "boot-gate: no firmware at $FW — building it"
    ( cd src/firmware && bash build.sh >/tmp/boot_gate_build.log 2>&1 ) || {
        echo "boot-gate: firmware build FAILED (see /tmp/boot_gate_build.log)"
        exit 2
    }
fi

echo "boot-gate: booting WuBuFW as a QEMU guest"
if ( cd src/firmware && timeout 180 bash qemu-boot-verify.sh ) >/tmp/boot_gate.log 2>&1; then
    grep -E '\[PASS\]' /tmp/boot_gate.log | sed 's/^/  /'
    echo "boot-gate: PASS — $(grep -c '\[PASS\]' /tmp/boot_gate.log) milestones, firmware reached its shell"
    exit 0
fi

echo "boot-gate: FAIL"
grep -E '\[FAIL\]|error' /tmp/boot_gate.log | head -5 | sed 's/^/  /'
echo "  full log: /tmp/boot_gate.log"
exit 1
