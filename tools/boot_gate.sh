#!/bin/sh
# boot_gate.sh -- a boot that can FAIL, not a claim that it boots.
#
# The artifact audit (tools/boot_audit.sh) only proves the ELF is freestanding.
# This runs the kernel and requires the first long-mode banner to appear on the
# serial line within a bounded time. If the kernel hangs or prints only probe
# bytes, this exits 1 and says which.
#
# usage: sh tools/boot_gate.sh [kernel.elf] [timeout_s]
set -u

KERNEL=${1:-src/kernel/kernel.elf}
TIMEOUT=${2:-25}
OUT=$(mktemp)
trap 'rm -f "$OUT"' EXIT

if [ ! -f "$KERNEL" ]; then
    echo "boot-gate: no kernel at $KERNEL (build it first)"
    exit 2
fi

# -nographic keeps it headless; isa-debug-exit lets a healthy kernel end the VM.
timeout "$TIMEOUT" qemu-system-x86_64 -kernel "$KERNEL" -nographic -m 128 \
        -no-reboot -device isa-debug-exit,iobase=0xf4 > "$OUT" 2>&1
rc=$?

bytes=$(wc -c < "$OUT" | tr -d ' ')
echo "boot-gate: serial $bytes bytes, qemu rc=$rc"

if grep -q 'kernel_main entered' "$OUT"; then
    echo "boot-gate: PASS — reached kernel_main in long mode"
    grep -m3 'WuBuOS:' "$OUT" | sed 's/^/  /'
    exit 0
fi

echo "boot-gate: FAIL — no 'kernel_main entered' banner"
if [ "$bytes" -eq 0 ]; then
    echo "  diagnosis: no serial output at all (serial not initialised, or the"
    echo "             kernel never ran: check the boot chain / load address)"
elif grep -qE '^[0-9a-fA-F]+$' "$OUT"; then
    echo "  diagnosis: only probe bytes were printed before the VM stopped:"
    head -c 120 "$OUT" | sed 's/^/    /'
    echo "  the kernel is stalling before metal_main's banner (loader or early init)"
else
    echo "  last serial line:"
    tail -c 300 "$OUT" | sed 's/^/    /'
fi
exit 1
