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
# Tier 2 (the OS actually entered): the firmware shell alone does NOT prove the
# kernel boots. The gate must also run the full chain -- WuBuFW -> secure-boot
# verify -> PE chainloader -> KERNEL.ELF -> kernel_main -> AGI supervisor with
# the measured-boot attestation consumed, root-of-truth VALID, and the bonzi
# heartbeat alive. src/firmware/run-agi.sh builds the ESP + chainloader itself
# and asserts every hop on the QEMU serial log; its PASS is required here.
#
# usage: sh tools/boot_gate.sh            (firmware + chain path, authoritative)
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
    echo "boot-gate: tier 1 PASS — $(grep -c '\[PASS\]' /tmp/boot_gate.log) firmware milestones, shell reached"
else
    echo "boot-gate: FAIL (firmware tier)"
    grep -E '\[FAIL\]|error' /tmp/boot_gate.log | head -5 | sed 's/^/  /'
    echo "  full log: /tmp/boot_gate.log"
    exit 1
fi

# Tier 2: the chain must reach kernel_main + AGI supervisor + heartbeat.
echo "boot-gate: tier 2 — full chain WuBuFW -> loader -> kernel_main (builds ESP)"
if ( cd src/firmware && bash run-agi.sh ) >/tmp/boot_gate_chain.log 2>&1; then
    grep -E 'kernel_main entered|attestation VALID|bonzi: heartbeat' \
        /tmp/boot_gate_chain.log | sed 's/^/  [chain] /' | head -4
    echo "boot-gate: PASS — firmware milestones + kernel_main via the chain"
    exit 0
fi
echo "boot-gate: FAIL (chain tier — kernel never entered)"
tail -8 /tmp/boot_gate_chain.log | sed 's/^/  /'
echo "  full log: /tmp/boot_gate_chain.log"
exit 1
