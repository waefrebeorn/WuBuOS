#!/usr/bin/env bash
# eddsa_ir_toolchain.sh -- shared LOCAL-only toolchain discovery for the
# compiler-IR constant-time gates.  Never fetches, never installs, never
# guesses: an absent toolchain is reported as absent so callers fail closed.

find_ir_clang() {
    local c tmp probe
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/wubu-ir-toolchain.XXXXXX") || return 0
    printf 'int wubu_ir_probe(void){return 0;}\n' > "$tmp/probe.c"
    for c in "${WUBU_IR_CLANG:-}" clang clang-19 clang-18 clang-17 clang-16 clang-15 clang-14; do
        [ -n "$c" ] || continue
        command -v "$c" >/dev/null 2>&1 || continue
        "$c" --version >/dev/null 2>&1 || continue
        if "$c" -emit-llvm -S -O2 -w "$tmp/probe.c" -o "$tmp/probe.ll" >/dev/null 2>&1 \
           && [ -s "$tmp/probe.ll" ]; then
            rm -rf "$tmp"
            printf '%s\n' "$c"
            return 0
        fi
    done
    rm -rf "$tmp"
    return 0
}

# opt is optional: used only to -verify the emitted IR.  Its absence does not
# relax the gate (clang -O2 is the compile of record) and is reported.
find_ir_opt() {
    local o
    for o in "${WUBU_IR_OPT:-}" opt opt-19 opt-18 opt-17 opt-16 opt-15 opt-14; do
        [ -n "$o" ] || continue
        command -v "$o" >/dev/null 2>&1 || continue
        "$o" --version >/dev/null 2>&1 || continue
        printf '%s\n' "$o"
        return 0
    done
    return 0
}

# emit_ladder_ir <clang> <source.c> <out.ll> <marker-note>
# Compile the shipped ladder source to LLVM IR at -O2 with the same include
# set the existing C gates use, and prepend a provenance marker the checker
# requires.  Returns nonzero (caller fails closed) on any toolchain/prereq
# failure; never writes a PASS without a real compile.
emit_ladder_ir() {
    local cc=$1 src=$2 out=$3 note=$4
    local root mb incs
    root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
    mb=$(bash "$root/tools/mbedtls_devroot.sh") || return 2
    incs="-I$root/include -I$root/src/gui -I$root/src/runtime -I$root/src/kernel -I$mb/usr/include"
    printf '; WUBU_IR_CT: %s\n' "$note" > "$out" || return 2
    # shellcheck disable=SC2086
    "$cc" -S -emit-llvm -O2 -w -std=c11 -D_POSIX_C_SOURCE=200809L \
        -DWUBU_HAVE_MBEDTLS -DWUBU_NO_LIBM $incs \
        -include "$root/src/runtime/wubu_gnu_compat.h" \
        "$src" -o "$out.raw" || return 3
    cat "$out.raw" >> "$out" || return 3
    return 0
}
