#!/usr/bin/env bash
# linux_product_gate.sh -- prove WuBuOS still RUNS on Linux, not just links.
# Builds the hosted product, boots it headless for a real frame, then runs the
# real-GUI capture and asserts five distinct windows/theme frames.
set -euo pipefail

cd "$(dirname "$0")/.."
root=$(pwd)
outdir="${TMPDIR:-/tmp}/wubu-linux-product-gate.$$"
mkdir -p "$outdir"
trap 'rm -rf "$outdir"' EXIT

log() { printf '[linux-product] %s\n' "$*"; }
fail() { printf '[linux-product] FAIL: %s\n' "$*" >&2; exit 1; }

log "building hosted product"
make -j"$(nproc)" hosted >"$outdir/build.log" 2>&1 || {
    tail -40 "$outdir/build.log" >&2
    fail "make hosted"
}
test -x src/hosted/wubu || fail "src/hosted/wubu missing"

log "booting headless product frame"
timeout 90 ./src/hosted/wubu --screenshot "$outdir/headless.ppm" \
    >"$outdir/headless.log" 2>&1 || {
    tail -40 "$outdir/headless.log" >&2
    fail "headless boot"
}
test -s "$outdir/headless.ppm" || fail "headless screenshot empty"
grep -q 'WuBuOS running' "$outdir/headless.log" || fail "kernel/GUI not initialized"
grep -q 'Screenshot saved' "$outdir/headless.log" || fail "frame not presented"
log "headless frame: $(wc -c <"$outdir/headless.ppm") bytes"

log "capturing real GUI frames"
shots=/tmp/wubu_gui_shots
rm -rf "$shots"
make gui_shot >"$outdir/gui.log" 2>&1 || {
    tail -40 "$outdir/gui.log" >&2
    fail "make gui_shot"
}
grep -q 'Done' "$outdir/gui.log" || fail "GUI capture did not complete"
files=("$shots"/0*.ppm)
test "${#files[@]}" -eq 5 || fail "expected 5 frames, got ${#files[@]}"
for f in "${files[@]}"; do test -s "$f" || fail "empty frame $f"; done
distinct=$(sha256sum "${files[@]}" | awk '{print $1}' | sort -u | wc -l)
test "$distinct" -eq 5 || fail "expected 5 distinct frames, got $distinct"

log "PASS: 1 headless boot + 5 distinct real-GUI frames"
