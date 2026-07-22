#!/usr/bin/env bash
#
# build_macho.sh -- build the WuBuOS macOS (Mach-O) 3D demo.
#
# Compiles demos/3d/macho_demo.c -> demos/3d/macho_demo.macho (a Mach-O
# binary) using Darling's xclang/xcrun cross toolchain. Darling serves
# the Mach-O's OpenGL onto the host Mesa/Vulkan stack (lavapipe here),
# so this is a REAL 3D pipeline through WuBuOS's VSL capability layer.
#
# Prereqs: Darling built (darling + xclang in darling-ref/build), and
# the host has freeglut + mesa. Run as root (Darling build dir is root-owned).
set -euo pipefail

REPO="/home/wubu/.hermes/profiles/mind-palace/home/myseed"
DRAGLING="$REPO/darling-ref/build"
DEMO="$REPO/demos/3d"
XCLANG=$(find "$DRAGLING" -maxdepth 4 -name 'xclang' -type f 2>/dev/null | head -1)
DARLING_BIN=$(find "$DRAGLING" -maxdepth 3 -name 'darling' -type f 2>/dev/null | head -1)

echo "[build_macho] xclang = ${XCLANG:-MISSING}"
echo "[build_macho] darling = ${DARLING_BIN:-MISSING}"

if [[ -z "$XCLANG" || -z "$DARLING_BIN" ]]; then
    echo "[build_macho] Darling toolchain not ready yet. Build Darling first."
    exit 2
fi

# basic-headers provide the Mach-O target SDK shim (OpenGL/gl.h, GLUT, etc.)
HDRS="$REPO/darling-ref/basic-headers"
SYSROOT="$REPO/darling-ref/build/Developer/SDKs/MacOSX.sdk"

# Compile + link a Mach-O. Darling's xclang targets x86_64-apple-darwin
# and rewrites the Mach-O load commands; GLUT/OpenGL resolve via the
# SDK shim + Darling's runtime at exec time.
"$XCLANG" \
    -target x86_64-apple-darwin20 \
    -isysroot "${SYSROOT:-$HDRS}" \
    -I"$HDRS" \
    -fobjc-arc \
    -O2 \
    "$DEMO/macho_demo.c" \
    -o "$DEMO/macho_demo.macho" \
    -framework OpenGL -framework GLUT -framework Cocoa 2>&1 | head -40

if [[ -x "$DEMO/macho_demo.macho" ]]; then
    echo "[build_macho] OK -> $DEMO/macho_demo.macho"
    file "$DEMO/macho_demo.macho" || true
    exit 0
else
    echo "[build_macho] FAILED to produce Mach-O"
    exit 1
fi
