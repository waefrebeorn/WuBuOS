#!/usr/bin/env bash
# boot_audit.sh -- is kernel.elf actually freestanding?
#
# A kernel that links against the host's libc is not independent, no matter how
# it boots. This checks the artifact, not the intent:
#   1. no NEEDED entries (no dynamic libraries)
#   2. no PT_INTERP (no host loader)
#   3. no undefined symbols (nm -u is empty)
#   4. ELF type EXEC and a non-zero entry point
#   5. no PT_DYNAMIC segment
#
# Usage: tools/boot_audit.sh [path/to/kernel.elf]
set -u

ELF="${1:-src/kernel/kernel.elf}"
fail=0

pass() { printf '  PASS  %s\n' "$1"; }
bad()  { printf '  FAIL  %s\n' "$1"; fail=1; }

if [ ! -f "$ELF" ]; then
    echo "boot-audit: missing $ELF (build the kernel first)"
    exit 1
fi

echo "boot-audit: $ELF"
echo "  size: $(stat -c%s "$ELF") bytes"

if readelf -d "$ELF" 2>/dev/null | grep -q 'NEEDED'; then
    bad "dynamic dependencies present"
else
    pass "no NEEDED entries (no shared libraries)"
fi

if readelf -l "$ELF" 2>/dev/null | grep -q 'INTERP'; then
    bad "PT_INTERP present (a host loader would run this)"
else
    pass "no PT_INTERP"
fi

if readelf -l "$ELF" 2>/dev/null | grep -q 'DYNAMIC'; then
    bad "PT_DYNAMIC present"
else
    pass "no PT_DYNAMIC segment"
fi

undef_count=$(nm -u "$ELF" 2>/dev/null | wc -l)
if [ "$undef_count" -eq 0 ]; then
    pass "no undefined symbols"
else
    bad "$undef_count undefined symbol(s)"
    nm -u "$ELF" 2>/dev/null | sed 's/^/        /'
fi

type_line=$(readelf -h "$ELF" 2>/dev/null | awk -F: '/Type:/{print $2}' | xargs)
entry=$(readelf -h "$ELF" 2>/dev/null | awk -F: '/Entry point/{print $2}' | xargs)
if [ "${type_line%% *}" = "EXEC" ]; then
    pass "ELF type EXEC"
else
    bad "ELF type is '${type_line:-unknown}', not EXEC"
fi

case "$entry" in
    ""|0x0|0) bad "entry point is zero or missing" ;;
    *)        pass "entry point $entry" ;;
esac

echo
if [ "$fail" -eq 0 ]; then
    echo "BOOT AUDIT OK: the artifact is freestanding."
else
    echo "BOOT AUDIT FAILED: host dependencies found."
fi
exit "$fail"
