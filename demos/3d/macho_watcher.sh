#!/usr/bin/env bash
#
# macho_watcher.sh -- once Darling finishes, build + run the Mach-O 3D demo
# through the WuBuOS Mach-O -> VSL path, capturing a real render proof.
#
# Pipeline (the "all execution container types through our process. VSL" proof):
#   macho_demo.c --(xclang/Mach-O)--> macho_demo.macho
#   darling macho_demo.macho  (CT_MACHO launcher, OpenGL->host Mesa/lavapipe)
#   under Xvfb -> screenshot == real 3D pipeline render proof
#
# Run as root (Darling build dir is root-owned). Detached; logs to
# /tmp/macho_watcher.log. Idempotent: exits if Mach-O already built.
set -uo pipefail
RPO="/home/wubu/.hermes/profiles/mind-palace/home/myseed"
DRAGLING="$RPO/darling-ref/build"
DEMO="$RPO/demos/3d"
LOG="/tmp/macho_watcher.log"
exec >"$LOG" 2>&1

echo "[watcher] start $(date)"

# Already done?
if [[ -x "$DEMO/macho_demo.macho" ]]; then
    echo "[watcher] Mach-O already built; running render proof only."
else
    # Wait for Darling toolchain (xclang + darling binary)
    for i in $(seq 1 360); do   # up to ~60 min
        XCLANG=$(find "$DRAGLING" -maxdepth 4 -name xclang -type f 2>/dev/null | head -1)
        DARBIN=$(find "$DRAGLING" -maxdepth 3 -name darling -type f 2>/dev/null | head -1)
        [[ -n "$XCLANG" && -n "$DARBIN" ]] && break
        sleep 10
    done
    if [[ -z "$XCLANG" || -z "$DARBIN" ]]; then
        echo "[watcher] Darling toolchain never appeared; aborting."
        exit 3
    fi
    echo "[watcher] darling=$DARBIN xclang=$XCLANG"

    SYSROOT="$DRAGLING/Developer/SDKs/MacOSX.sdk"
    HDRS="$RPO/darling-ref/basic-headers"
    SR="$SYSROOT"; [[ -d "$SR" ]] || SR="$HDRS"

    echo "[watcher] compiling Mach-O demo..."
    "$XCLANG" \
        -target x86_64-apple-darwin20 \
        -isysroot "$SR" -I"$HDRS" -fobjc-arc -O2 \
        "$DEMO/macho_demo.c" -o "$DEMO/macho_demo.macho" \
        -framework OpenGL -framework GLUT -framework Cocoa 2>&1 | head -40

    if [[ ! -x "$DEMO/macho_demo.macho" ]]; then
        echo "[watcher] Mach-O build FAILED (see above)."
        # Try without -framework Cocoa (sometimes unneeded for GLUT)
        "$XCLANG" -target x86_64-apple-darwin20 \
            -isysroot "$SR" -I"$HDRS" -fobjc-arc -O2 \
            "$DEMO/macho_demo.c" -o "$DEMO/macho_demo.macho" \
            -framework OpenGL -framework GLUT 2>&1 | head -40
    fi
    file "$DEMO/macho_demo.macho" 2>/dev/null || true
fi

if [[ ! -x "$DEMO/macho_demo.macho" ]]; then
    echo "[watcher] no Mach-O binary; cannot render. abort."
    exit 4
fi

# Render proof: run via darling under Xvfb, screenshot.
DARBIN=$(find "$DRAGLING" -maxdepth 3 -name darling -type f 2>/dev/null | head -1)
pkill Xvfb 2>/dev/null; sleep 1
Xvfb :97 -screen 0 800x600x24 >/dev/null 2>&1 &
XVFB_PID=$!
sleep 2
export DISPLAY=:97
echo "[watcher] launching darling macho_demo under $DISPLAY ..."
timeout 25 "$DARBIN" "$DEMO/macho_demo.macho" >/tmp/macho_run.log 2>&1 &
RUN_PID=$!
sleep 12
import_argb=$(command -v import || true)
if [[ -n "$import_argb" ]]; then
    import -window root -display :97 "$DEMO/proof_macho_render.png" 2>/dev/null \
        && echo "[watcher] screenshot -> $DEMO/proof_macho_render.png" \
        || echo "[watcher] screenshot failed"
fi
kill "$RUN_PID" 2>/dev/null; wait "$RUN_PID" 2>/dev/null
kill "$XVFB_PID" 2>/dev/null
echo "[watcher] done $(date)"
echo "[watcher] darling stderr tail:"; tail -6 /tmp/macho_run.log 2>/dev/null
