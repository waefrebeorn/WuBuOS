#!/usr/bin/env python3
"""Zero-bignum checker for the fixed-width EdDSA signing path.

Two fail-closed modes:

  files <path>...   strip C comments and string/char literals, then reject any
                    mbedTLS integer / bignum symbol in the shipped source files.

  pp <file>|-       read gcc -E output; using the `# N "path"` line markers,
                    audit ONLY lines that came from wubu_pkg_ed25519.c (the
                    dispatcher whose fixed-mode branch must compile no integer
                    code), stripping literals and comments.

A "symbol" here is an identifier: mbedtls_mpi*, mbedtls_*mpi*, crypto_bignum*,
bignum*, BIGNUM*.  mbedTLS SHA-512 (mbedtls_sha512_*) is a hash, not a bignum,
and is explicitly allowed.  Prose in comments/strings is not a symbol.
"""
import re
import sys

BANNED = re.compile(
    r"\b(?:mbedtls_mpi\w*|mbedtls_\w*mpi\w*|crypto_bignum\w*|bignum\w*|BIGNUM\w*)\b"
)


def strip_c(text, do_comments=True):
    """Remove C block/line comments (optionally), then string/char literals.
    Newlines are preserved so reported line numbers stay accurate."""
    out, i, n = [], 0, len(text)
    while i < n:
        if do_comments and text[i:i + 2] == "/*":
            j = text.find("*/", i + 2)
            if j < 0:
                j = n - 2
            out.append("\n" * text.count("\n", i, j + 2))
            i = j + 2
        elif do_comments and text[i:i + 2] == "//":
            j = text.find("\n", i + 2)
            if j < 0:
                j = n
            out.append("\n")
            i = j
        elif text[i] in ("\"", "'"):
            q = text[i]
            out.append(q)
            i += 1
            while i < n:
                if text[i] == "\\":
                    out.append(" ")
                    i += 2
                    continue
                if text[i] == q:
                    out.append(q)
                    i += 1
                    break
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def audit_lines(lines, label):
    hits = []
    for lineno, line in enumerate(lines, 1):
        clean = strip_c(line, do_comments=False)  # literals only; comments gone
        for m in BANNED.finditer(clean):
            hits.append((label, lineno, m.group(0), line.strip()[:100]))
    return hits


def main(argv):
    if len(argv) < 2:
        print("usage: ed25519_fixed_bignum_check.py {files <path>...|pp <file>|-}")
        return 2
    mode = argv[1]
    hits = []
    if mode == "files":
        if len(argv) < 3:
            print("FAIL: files mode needs >= 1 path")
            return 2
        for path in argv[2:]:
            try:
                with open(path, encoding="utf-8") as fh:
                    text = fh.read()
            except OSError as exc:
                print("FAIL: cannot read %s: %s" % (path, exc))
                return 2
            hits += audit_lines(strip_c(text).splitlines(), path)
    elif mode == "pp":
        if len(argv) != 3:
            print("FAIL: pp mode needs exactly one file or -")
            return 2
        if argv[2] == "-":
            text = sys.stdin.read()
        else:
            try:
                with open(argv[2], encoding="utf-8") as fh:
                    text = fh.read()
            except OSError as exc:
                print("FAIL: cannot read %s: %s" % (argv[2], exc))
                return 2
        cur = None
        scoped = []
        for ln in text.splitlines():
            m = re.match(r'^\s*#\s*\d+\s+"([^"]+)"', ln)
            if m:
                cur = m.group(1)
                continue
            if cur and cur.endswith("wubu_pkg_ed25519.c"):
                scoped.append(ln)
        print("[bignum_check] pp: audited %d active lines from wubu_pkg_ed25519.c" % len(scoped))
        if not scoped:
            print("FAIL: no active lines found (wrong preprocessor flags?)")
            return 2
        hits += audit_lines(scoped, "wubu_pkg_ed25519.c (preprocessed)")
    else:
        print("FAIL: unknown mode %s" % mode)
        return 2

    if hits:
        for path, lineno, tok, raw in hits[:12]:
            print("FAIL %s:%d banned symbol `%s` | %s" % (path, lineno, tok, raw))
        print("BIGNUM_CHECK FAILED")
        return 1
    print("BIGNUM_CHECK PASSED: zero mbedTLS integer/bignum symbols")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
