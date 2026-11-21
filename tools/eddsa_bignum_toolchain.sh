#!/usr/bin/env bash
# eddsa_bignum_toolchain.sh -- LOCAL-only discovery of the mbedTLS static
# archive and disassembly of its SHIPPED bignum machine code.  Never fetches,
# never installs, never substitutes a fixture for the shipped object.
# Every prereq failure returns nonzero so callers FAIL CLOSED.

# shellcheck source=tools/eddsa_callee_toolchain.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/eddsa_callee_toolchain.sh"

# disasm_bignum_object <outfile> -- extract bignum.c.o from the LOCAL
# libmbedcrypto.a and write Intel-syntax objdump of the whole object with a
# provenance marker carrying the archive SHA-256.  Nonzero on ANY failure.
disasm_bignum_object() {
    local outfile=$1 mb archive tmp sha ver
    command -v objdump >/dev/null 2>&1 || return 2
    command -v ar >/dev/null 2>&1 || return 2
    command -v sha256sum >/dev/null 2>&1 || return 2
    mb=$(resolve_mbedtls_root)
    [ -n "$mb" ] || return 3
    archive="$mb/usr/lib/x86_64-linux-gnu/libmbedcrypto.a"
    [ -f "$archive" ] || return 4
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/wubu-bignum-dis.XXXXXX") || return 5
    ( cd "$tmp" && ar x "$archive" bignum.c.o ) || { rm -rf "$tmp"; return 6; }
    [ -f "$tmp/bignum.c.o" ] || { rm -rf "$tmp"; return 7; }
    if ! objdump -d -M intel "$tmp/bignum.c.o" > "$tmp/body.asm" 2>/dev/null; then
        rm -rf "$tmp"; return 8
    fi
    sha=$(sha256sum "$archive" | cut -d' ' -f1)
    ver=$(grep -m1 'MBEDTLS_VERSION_STRING ' \
          "$mb/usr/include/mbedtls/version.h" 2>/dev/null | tr -d '\r')
    [ -n "$sha" ] || { rm -rf "$tmp"; return 9; }
    {
        printf ';; WUBU_BIGNUM_VALUE: local libmbedcrypto.a sha256=%s %s\n' "$sha" "$ver"
        cat "$tmp/body.asm"
    } > "$outfile" || { rm -rf "$tmp"; return 10; }
    rm -rf "$tmp"
    # Every function the gate audits must really be present in the dump.
    for fn in mbedtls_mpi_cmp_mpi mbedtls_mpi_cmp_abs add_sub_mpi \
              mbedtls_mpi_mul_mpi mpi_mul_hlp mbedtls_mpi_div_mpi \
              mbedtls_mpi_mod_mpi; do
        grep -q "<$fn>:" "$outfile" || return 11
    done
    return 0
}
