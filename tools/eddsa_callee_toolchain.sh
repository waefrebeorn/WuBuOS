#!/usr/bin/env bash
# eddsa_callee_toolchain.sh -- LOCAL-only discovery of the mbedTLS static
# archive and disassembly of its shipped mbedtls_mpi_safe_cond_swap machine
# code.  Never fetches, never installs, never substitutes a fixture for the
# shipped object code.  An absent toolchain/root returns nonzero so callers
# FAIL CLOSED instead of asserting a fake PASS.

# resolve_mbedtls_root -- print the local mbedTLS devroot path, or nothing.
resolve_mbedtls_root() {
    local root mb common cand
    if [ -n "${WUBU_MBEDTLS_ROOT:-}" ] && \
       [ -f "$WUBU_MBEDTLS_ROOT/usr/include/mbedtls/version.h" ]; then
        printf '%s\n' "$WUBU_MBEDTLS_ROOT"
        return 0
    fi
    root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
    mb=$(bash "$root/tools/mbedtls_devroot.sh" 2>/dev/null) || mb=""
    if [ -n "$mb" ] && [ -f "$mb/usr/include/mbedtls/version.h" ]; then
        printf '%s\n' "$mb"
        return 0
    fi
    # Linked worktrees share the canonical repo's object/config dir but not
    # its untracked .cache.  Resolve that SAME repo's local devroot offline.
    common=$(git -C "$root" rev-parse --git-common-dir 2>/dev/null) || common=""
    if [ -n "$common" ]; then
        case "$common" in /*) ;; *) common=$(cd "$root" && cd "$common" && pwd);; esac
        cand=$(dirname "$common")/.cache/mbedtls-devroot
        if [ -f "$cand/usr/include/mbedtls/version.h" ]; then
            printf '%s\n' "$cand"
            return 0
        fi
    fi
    return 0
}

# disasm_safe_cond_swap <outfile> -- extract constant_time.c.o from the LOCAL
# libmbedcrypto.a and write real Intel-syntax objdump of the shipped
# mbedtls_mpi_safe_cond_swap to <outfile>.  Nonzero on ANY prereq failure.
disasm_safe_cond_swap() {
    local outfile=$1 mb archive tmp
    command -v objdump >/dev/null 2>&1 || return 2
    command -v ar >/dev/null 2>&1 || return 2
    mb=$(resolve_mbedtls_root)
    [ -n "$mb" ] || return 3
    archive="$mb/usr/lib/x86_64-linux-gnu/libmbedcrypto.a"
    [ -f "$archive" ] || return 4
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/wubu-cswap-dis.XXXXXX") || return 5
    ( cd "$tmp" && ar x "$archive" constant_time.c.o ) || { rm -rf "$tmp"; return 6; }
    [ -f "$tmp/constant_time.c.o" ] || { rm -rf "$tmp"; return 7; }
    if ! objdump -d -M intel --disassemble=mbedtls_mpi_safe_cond_swap \
            "$tmp/constant_time.c.o" > "$outfile" 2>/dev/null; then
        rm -rf "$tmp"; return 8
    fi
    rm -rf "$tmp"
    # The dump must contain the real function body, not just a header.
    grep -q '<mbedtls_mpi_safe_cond_swap>' "$outfile" || return 9
    grep -q 'movzx' "$outfile" || return 10   # mask/XOR select really present
    return 0
}
