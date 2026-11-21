#!/usr/bin/env bash
# eddsa_gimple_toolchain.sh -- shared LOCAL-only toolchain discovery and GIMPLE
# emission for the compiled-tree constant-time gates.  Never fetches, never
# installs, never guesses a toolchain.  An absent or incapable compiler is
# reported as absent so callers FAIL CLOSED instead of asserting a fake PASS.

# find_gimple_gcc -- print the path/name of a LOCAL gcc that really supports
# `-fdump-tree-optimized -O2`, or print nothing (caller fails closed).
find_gimple_gcc() {
    local c tmp
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/wubu-gimple-toolchain.XXXXXX") || return 0
    printf 'int wubu_gimple_probe(int n){int s=0;for(int i=0;i<n;i++)s+=i;return s;}\n' > "$tmp/probe.c"
    for c in "${WUBU_GIMPLE_GCC:-}" gcc gcc-15 gcc-14 gcc-13 gcc-12 gcc-11; do
        [ -n "$c" ] || continue
        command -v "$c" >/dev/null 2>&1 || continue
        "$c" --version >/dev/null 2>&1 || continue
        # A real capability probe: compile and require a non-empty optimized
        # tree dump containing SSA PHI nodes (i.e. -O2 actually ran).
        if (cd "$tmp" && "$c" -c -O2 -w "$tmp/probe.c" -o "$tmp/probe.o" \
                -fdump-tree-optimized >/dev/null 2>&1) \
           && [ -n "$(find "$tmp" -name '*optimized' -size +0c -print -quit)" ] \
           && grep -q 'PHI <' "$tmp"/*optimized 2>/dev/null; then
            rm -rf "$tmp"
            printf '%s\n' "$c"
            return 0
        fi
    done
    rm -rf "$tmp"
    return 0
}

# emit_gimple_dump <gcc> <source.c> <outdir> <marker-note> <outfile>
# Compile <source.c> at -O2 with GCC's -fdump-tree-optimized and write the
# real GIMPLE dump to <outfile>, prefixed with a provenance marker the checker
# requires.  Returns nonzero on ANY toolchain/prereq failure; never writes a
# PASS and never substitutes a fixture for compiler output.
emit_gimple_dump() {
    local cc=$1 src=$2 outdir=$3 note=$4 outfile=$5
    local root mb incs dump
    root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
    mb=$(bash "$root/tools/mbedtls_devroot.sh" 2>/dev/null) || mb=""
    if [ -z "$mb" ]; then
        # Linked worktrees share the canonical repo's object/config dir but
        # not its untracked .cache.  Resolve that SAME repo's local devroot
        # offline (no fetch, no guessing an external toolchain); if it is
        # absent too, fail closed.
        common=$(git -C "$root" rev-parse --git-common-dir 2>/dev/null) || common=""
        if [ -n "$common" ]; then
            case "$common" in /*) ;; *) common=$(cd "$root" && cd "$common" && pwd);; esac
            cand=$(dirname "$common")/.cache/mbedtls-devroot
            if [ -f "$cand/usr/include/mbedtls/version.h" ] && \
               [ -f "$cand/usr/lib/x86_64-linux-gnu/libmbedcrypto.a" ]; then
                mb="$cand"
            fi
        fi
    fi
    [ -n "$mb" ] || return 2
    incs="-I$root/include -I$root/src/gui -I$root/src/runtime -I$root/src/kernel -I$mb/usr/include"
    rm -f "$outdir"/*optimized 2>/dev/null || true
    # shellcheck disable=SC2086
    "$cc" -c -O2 -w -std=c11 -D_POSIX_C_SOURCE=200809L \
        -DWUBU_HAVE_MBEDTLS -DWUBU_NO_LIBM $incs \
        -include "$root/src/runtime/wubu_gnu_compat.h" \
        "$src" -o "$outdir/ladder.o" -fdump-tree-optimized \
        >"$outdir/compile.log" 2>&1 || return 3
    dump=$(find "$outdir" -name '*optimized' -size +0c -print -quit 2>/dev/null)
    [ -n "$dump" ] || return 4
    { printf ';; WUBU_GIMPLE_CT: %s\n' "$note"
      cat "$dump"; } > "$outfile" || return 5
    return 0
}
