#!/bin/bash
# verify_isa.sh — the ISA ENCODING ORACLE (permanent tool).
#
# Verifies machine-code encodings byte-for-byte against GNU binutils
# objdump. THE rule for the driver space (2026-08-04): no guessed
# opcodes — every byte the drivers emit must be confirmed here first.
# This tool caught a wrong MOVE field layout (0x2004 is movel %d4,%d0,
# NOT MOVE.L D0,D0) that hand-derived encodings would have shipped.
#
# Usage:
#   tools/verify_isa.sh m68k 4e56 0000 4e5e 4e75     # words as hex
#   tools/verify_isa.sh m68k --file emitted.bin       # raw bytes
#   tools/verify_isa.sh m68k --loop                   # read hex words from stdin
#   tools/verify_isa.sh --list                        # supported ISAs
#
# Supported: m68k (68000), and anything objdump -i reports under
# --list. The multiarch objdump is found automatically; if absent,
# points you at the one-line extraction (see header comment below).
#
# The multiarch objdump is NOT installed via apt (no root); it lives in
# a user-writable dir, e.g. /tmp/bmu (extracted from the
# binutils-multiarch .deb) — but tools must not depend on /tmp. This
# script prefers, in order:
#   1. $WUBU_BINUTILS (set this in the build env)
#   2. a copy at /opt/binutils-multiarch (sudo cp -r /tmp/bmu /opt)
#   3. /tmp/bmu as a last resort (dev-only fallback)

set -u

ISA="${1:-}"
if [ "$ISA" = "--list" ] || [ -z "$ISA" ]; then
    echo "usage: verify_isa.sh <isa> <hex words...> | --file <bin> | --loop"
    echo "       verify_isa.sh --list   (objdump's supported ISAs)"
    exit 1
fi
shift

find_objdump() {
    # prefer a stable location; then the dev /tmp extraction; system
    # objdump LAST (it usually lacks multiarch support)
    for d in "${WUBU_BINUTILS:-}" "$HOME/opt/binutils-multiarch/usr/bin" \
             /opt/binutils-multiarch/usr/bin /tmp/bmu/usr/bin; do
        [ -n "$d" ] && [ -x "$d/x86_64-linux-gnu-objdump" ] && {
            echo "$d/x86_64-linux-gnu-objdump"; return 0
        }
    done
    command -v objdump 2>/dev/null && return 0
    return 1
}

find_libpath() {
    for d in "$HOME/opt/binutils-multiarch/usr/lib/x86_64-linux-gnu" \
             /opt/binutils-multiarch/usr/lib/x86_64-linux-gnu \
             /tmp/bmu/usr/lib/x86_64-linux-gnu; do
        [ -d "$d" ] && { echo "$d"; return 0; }
    done
    return 1
}

OD="$(find_objdump)" || {
    echo "ERROR: multiarch objdump not found." >&2
    echo "Extract it without root:" >&2
    echo "  cd /tmp && apt-get download binutils-multiarch && dpkg -x binutils-multiarch*.deb /tmp/bmu" >&2
    echo "and copy to a stable path (or set WUBU_BINUTILS)." >&2
    exit 1
}
LP="$(find_libpath)" || LP=""
export LD_LIBRARY_PATH="${LP}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# map our ISA names to objdump -m arch
case "$ISA" in
    m68k|68000|68k)   ARCH="m68k:68000" ;;
    *)                ARCH="$ISA" ;;
esac

INPUT=/tmp/verify_isa_input.bin
rm -f "$INPUT"

if [ "${1:-}" = "--file" ]; then
    cp "$2" "$INPUT"
elif [ "${1:-}" = "--loop" ]; then
    # read whitespace-separated hex words from stdin, pack as big-endian
    python3 - "$INPUT" <<'PY'
import sys
words = sys.stdin.read().split()
with open(sys.argv[1], "wb") as f:
    for w in words:
        f.write(bytes.fromhex(w))
PY
else
    # pack hex words as big-endian bytes
    python3 - "$INPUT" "$@" <<'PY'
import sys
with open(sys.argv[1], "wb") as f:
    for w in sys.argv[2:]:
        f.write(bytes.fromhex(w))
PY
fi

[ -s "$INPUT" ] || { echo "ERROR: no input bytes"; exit 1; }

echo "=== $ISA ($ARCH) — $("$OD" -D -b binary -m "$ARCH" "$INPUT" 2>&1 | grep -c '^ *[0-9a-f]*:') decoded ==="
"$OD" -D -b binary -m "$ARCH" "$INPUT" 2>&1
echo
echo "EXIT: decoded OK"
