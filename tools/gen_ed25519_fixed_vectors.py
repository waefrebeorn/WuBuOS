#!/usr/bin/env python3
"""gen_ed25519_fixed_vectors.py -- reproducible RFC 8032 scalar-mult vectors.

Pure-Python big-int reference implementation of Ed25519 scalar multiplication
in extended twisted-Edwards coordinates (RFC 8032 constants).  Used by
tools/ed25519_fixed_ladder_gate.sh as an INDEPENDENT oracle for the shipped
bignum-free C ladder.  Deterministic: fixed scalars, no randomness, stdlib only.
"""
import sys

Q = 2**255 - 19
D = (-121665 * pow(121666, Q - 2, Q)) % Q
BY = 4 * pow(5, Q - 2, Q) % Q
BX = pow((BY * BY - 1) * pow(D * BY * BY + 1, Q - 2, Q), (Q + 3) // 8, Q)
if (BX * BX - (BY * BY - 1) * pow(D * BY * BY + 1, Q - 2, Q)) % Q != 0:
    BX = BX * pow(2, (Q - 1) // 4, Q) % Q
if BX % 2:
    BX = Q - BX
B = (BX, BY, 1, BX * BY % Q)
L = 2**252 + 27742317777372353535851937790883648493


def add(p, q):
    x1, y1, z1, t1 = p
    x2, y2, z2, t2 = q
    a = (y1 - x1) * (y2 - x2) % Q
    b = (y1 + x1) * (y2 + x2) % Q
    c = 2 * D * t1 * t2 % Q
    d = 2 * z1 * z2 % Q
    e, f, g, h = (b - a) % Q, (d - c) % Q, (d + c) % Q, (b + a) % Q
    return e * f % Q, g * h % Q, f * g % Q, e * h % Q


def mul(n, p):
    r = (0, 1, 1, 0)
    s = p
    for i in range(255, -1, -1):
        bit = (n >> i) & 1
        if bit:
            r, s = s, r
        s = add(s, r)
        r = add(r, r)
        if bit:
            r, s = s, r
    return r


def enc(p):
    x, y, z, _ = p
    zi = pow(z, Q - 2, Q)
    x, y = x * zi % Q, y * zi % Q
    return (y | ((x & 1) << 255)).to_bytes(32, "little").hex()


def main():
    print("# RFC 8032 Bx=%064x By=%064x d=%064x" % (BX, BY, D))
    scalars = [
        1, 2, 3, 7, 0, L - 1, L, Q, (1 << 254) + 12345,
        0x0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF,
        int.from_bytes(bytes(range(32)), "little"),
    ]
    for n in scalars:
        print("SC %064x %s" % (n, enc(mul(n, B))))
    a = mul(0xABCDEF0123456789, B)
    ap = enc(a)
    for m in (1, 7, 123456789):
        print("PT %064x %s %s" % (m, ap, enc(mul(m, a))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
