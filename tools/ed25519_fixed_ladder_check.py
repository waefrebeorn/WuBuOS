#!/usr/bin/env python3
"""ed25519_fixed_ladder_check.py -- proof-style checker for the bignum-free ladder.

Two modes, both fail closed (exit 1 on any unmet obligation):

  source <wubu_ed25519_fixed.c>   structural C obligations (root-independent):
    S1  ladder trip count is the compile-time constant 256, no secret bitlen
    S2  secret state (bit/swap) never selects a branch/ternary/memory index
    S3  cswap is mask-only (all-ones/all-zeros), no branch on `swap`
    S4  NO mbedtls_mpi / bignum symbol appears anywhere in the module

  asm <objdump -d -M intel file>  checked against REAL gcc -O2 output:
    A1  fixed trip count survives -O2 (mov <reg>,0x100 / sub <reg>,0x1)
    A2  scalar-bit load + xor feeds the unconditional cswap call
    A3  no conditional jump is governed by a scalar-bit-derived register/flag
    A4  no memory operand is indexed by a scalar-bit-derived register
    A5  cswap is mask-only (neg + pand + por), not a branchy select

Structural/machine-code evidence, not a full formal proof; see
docs/ED25519_FIXED_LADDER_CHECK.md.
"""
import re
import sys

LADDER = "wubu_ed25519_fixed_ladder"
CSWAP = "wubu_ed25519_fixed_cswap"

def die(msg):
    print("[fixed_ladder] FAIL: " + msg)
    print("FIXED_LADDER_CHECK FAILED")
    sys.exit(1)

def strip_comments(s):
    out, i, n = [], 0, len(s)
    while i < n:
        if s[i:i+2] == "/*":
            j = s.find("*/", i+2); i = n if j < 0 else j+2; out.append(" ")
        elif s[i:i+2] == "//":
            j = s.find("\n", i+2); i = n if j < 0 else j+1; out.append("\n")
        else:
            out.append(s[i]); i += 1
    return "".join(out)

def extract_function(src, name):
    m = re.search(r"\b" + re.escape(name) + r"\s*\([^;{]*\)\s*\{", src)
    if not m:
        return None
    start = m.end()-1; depth = 0; i = start
    while i < len(src):
        c = src[i]
        if c in ('"', "'"):
            q = c; i += 1
            while i < len(src):
                if src[i] == "\\": i += 2; continue
                if src[i] == q: i += 1; break
                i += 1
            continue
        if c == "{": depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0: return strip_comments(src[start:i+1])
        i += 1
    return None

def check_source(path):
    try:
        with open(path, encoding="utf-8") as fh: src = fh.read()
    except OSError as exc:
        die("cannot read %s: %s" % (path, exc))
        return 1
    print("[fixed_ladder] source: %s (text-only, no toolchain needed)" % path)
    bare = strip_comments(src)
    banned = re.findall(r"\bmbedtls_\w+|\bmbedtls_mpi\b|\bcrypto_bignum\w*", bare)
    if banned:
        die("bignum symbol in bignum-free module: %s" % ", ".join(sorted(set(banned))[:6]))
    print("[fixed_ladder] S4 PASS: zero mbedtls/bignum symbols in module")
    ladder = extract_function(src, LADDER)
    if ladder is None: die("%s() definition not found" % LADDER)
    cswap = extract_function(src, CSWAP)
    if cswap is None: die("%s() definition not found" % CSWAP)
    mdef = re.search(r"^\s*#\s*define\s+WUBU_ED25519_FIXED_LADDER_BITS\s+([0-9]+)u?\b", src, re.M)
    if not mdef or int(mdef.group(1)) != 256:
        die("WUBU_ED25519_FIXED_LADDER_BITS is not the numeric constant 256")
    if not re.search(r"\bfor\s*\([^)]*WUBU_ED25519_FIXED_LADDER_BITS", ladder):
        die("ladder loop is not bounded by WUBU_ED25519_FIXED_LADDER_BITS")
    if re.search(r"\bbitlen\b|mbedtls", ladder):
        die("secret-dependent trip count in ladder")
    print("[fixed_ladder] S1 PASS: trip count = compile-time constant 256")
    if re.search(r"\b(if|while|switch|for)\s*\([^)]*\b(bit|swap)\b[^)]*\)", ladder):
        die("secret-dependent branch in ladder")
    if re.search(r"\b(bit|swap)\b\s*\?", ladder) or re.search(r"\?\s*[^:;]*\b(bit|swap)\b\s*:", ladder):
        die("secret-dependent ternary in ladder")
    if re.search(r"\[[^]]*\b(bit|swap)\b[^]]*\]", ladder):
        die("secret-indexed memory access in ladder")
    calls = re.findall(r"\b%s\s*\(" % CSWAP, ladder)
    good = re.findall(r"\b%s\s*\([^)]*,\s*swap\s*\)" % CSWAP, ladder)
    if not calls or len(calls) != len(good):
        die("a %s call is not the unconditional ...,swap call" % CSWAP)
    print("[fixed_ladder] S2 PASS: no secret branch/ternary/index; %d/%d cswap calls driven by swap" % (len(good), len(calls)))
    if re.search(r"\b(if|while|switch|for)\s*\([^)]*\bswap\b[^)]*\)", cswap):
        die("secret-dependent branch inside cswap")
    if re.search(r"\bswap\b\s*\?", cswap):
        die("secret-dependent ternary inside cswap")
    if not re.search(r"\(uint64_t\)0\s*-\s*\(uint64_t\)", cswap):
        die("cswap does not build an all-ones/all-zeros mask from swap")
    if not (re.search(r"&\s*~m", cswap) and re.search(r"&\s*m", cswap)):
        die("cswap does not mask-select both sides of each limb")
    if not re.search(r"for\s*\([^)]*<\s*4", cswap):
        die("cswap does not select all four projective coordinates")
    print("[fixed_ladder] S3 PASS: cswap is mask-only over all 4 coordinates")
    print("[fixed_ladder] SOURCE PASS: bignum-free structural contract held")
    return 0

REG_ALIASES = {}
for base, names in {
    "a": ["rax","eax","ax","al","ah"], "b": ["rbx","ebx","bx","bl","bh"],
    "c": ["rcx","ecx","cx","cl","ch"], "d": ["rdx","edx","dx","dl","dh"],
    "si": ["rsi","esi","si","sil"], "di": ["rdi","edi","di","dil"],
    "bp": ["rbp","ebp","bp","bpl"], "sp": ["rsp","esp","sp","spl"],
}.items():
    for nm in names: REG_ALIASES[nm] = base
for i in range(8, 16):
    for suf in ("", "d", "w", "b"): REG_ALIASES["r%d%s" % (i, suf)] = "r%d" % i

def regk(tok):
    return REG_ALIASES.get(tok.strip().lower())

def parse_asm(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as fh: lines = fh.read().splitlines()
    except OSError as exc:
        die("cannot read objdump file %s: %s" % (path, exc))
    funcs = {}; cur = None
    insn_re = re.compile(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2}\s+)+\s*([a-z][a-z0-9.]*)\s*(.*)$")
    for ln in lines:
        m = re.match(r"^[0-9a-f]+ <([^>]+)>:", ln)
        if m:
            cur = m.group(1); funcs[cur] = []; continue
        if cur is None: continue
        m = insn_re.match(ln)
        if m: funcs[cur].append({"addr": m.group(1), "op": m.group(2), "args": m.group(3).strip(), "raw": ln})
    return funcs

def operands(s):
    s = s.split("#", 1)[0].strip()
    return [x.strip() for x in s.split(",") if x.strip()]

def mem_regs(op):
    inner = op[op.find("[")+1:op.rfind("]")] if "[" in op and "]" in op else ""
    found = set()
    for tok in re.split(r"[+\-*/()\s]+", inner):
        k = regk(tok)
        if k: found.add(k)
    return found

FLAG_WRITERS = {"cmp","test","add","sub","and","or","xor","inc","dec","neg","shl","shr","sar","sal","imul","mul","adc","sbb"}
JCC = re.compile(r"^j[a-z]+$")

def analyze_cswap(cins):
    taint = {"d"}; findings = []; flag_taint = False; last = None
    for ins in cins:
        base = ins["op"]; args = operands(ins["args"])
        if JCC.match(base):
            if flag_taint: findings.append((ins["addr"], "JCC", ins["raw"], last or "?"))
            continue
        if base == "call":
            for r in ("a","c","d","si","di"): taint.discard(r)
            flag_taint = False; last = None; continue
        if base in ("push","pop","ret","nop","movaps","movups","movdqa","movdqu","pxor","punpcklqdq","movq"):
            continue
        if base in ("cmp","test"):
            flag_taint = any((regk(a) and regk(a) in taint) or any(mr in taint for mr in mem_regs(a)) for a in args)
            last = ins["raw"].strip(); continue
        if args:
            dst = regk(args[0]); src = False
            for a in args[1:]:
                k = regk(a)
                if k and k in taint: src = True
                if any(mr in taint for mr in mem_regs(a)): src = True
            if dst and base in ("mov","movzx","movsx","lea","xor","and","or","add","sub","adc","sbb",
                                "sar","shr","shl","sal","neg","not","inc","dec","imul","mul"):
                if base == "xor" and len(args) == 2 and args[0].strip().lower() == args[1].strip().lower():
                    taint.discard(dst)
                elif src: taint.add(dst)
                elif base in ("mov","movzx","movsx","lea","and","or","sar","shr","shl","sal","neg","not"): taint.discard(dst)
            if base in FLAG_WRITERS: flag_taint = src; last = ins["raw"].strip()
    return findings

def check_asm(path, label="gcc -O2"):
    funcs = parse_asm(path)
    if LADDER not in funcs: die("no <%s> in objdump output" % LADDER)
    if CSWAP not in funcs: die("no <%s> in objdump output" % CSWAP)
    lins = funcs[LADDER]; cins = funcs[CSWAP]
    if len(lins) < 40 or len(cins) < 10:
        die("suspiciously small function bodies (ladder=%d cswap=%d)" % (len(lins), len(cins)))
    const256 = [i for i in lins if i["op"] == "mov" and re.search(r",0x100\b", i["args"])]
    if not const256: die("no `mov <reg>,0x100` (constant 256 trip count) in compiled ladder")
    ctr = operands(const256[0]["args"])[0].strip().lower()
    dec = [i for i in lins if i["op"] == "sub" and re.search(r"\b%s\s*,\s*0x1\b" % re.escape(ctr), i["args"], re.I)]
    if not dec: die("loop counter %s is not decremented by 1 in compiled ladder" % ctr)
    print("[fixed_ladder] A1 PASS: `mov %s,0x100` and `sub %s,1` survive %s" % (ctr, ctr, label))
    sptr = set()
    for i in lins[:25]:
        if i["op"] == "mov" and re.search(r",\s*rsi\b", i["args"]):
            k = regk(operands(i["args"])[0])
            if k: sptr.add(k)
    if not sptr: die("cannot identify the scalar pointer argument (mov <reg>,rsi) in ladder")
    taint = set(); findings = []; flag_taint = False; last = None
    secret_loads = 0; cswap_calls = 0
    for ins in lins:
        base = ins["op"]; args = operands(ins["args"])
        for a in args:
            if "[" in a:
                for mr in mem_regs(a):
                    if mr in taint: findings.append((ins["addr"], "INDEX", ins["raw"], mr))
        if JCC.match(base):
            if flag_taint: findings.append((ins["addr"], "JCC", ins["raw"], last or "?"))
            continue
        if base == "call":
            if CSWAP in ins["raw"]:
                cswap_calls += 1
                if "d" not in taint:
                    findings.append((ins["addr"], "CSWAP_ARG", ins["raw"], "swap edx not secret-derived"))
            for r in ("a","c","d","si","di"): taint.discard(r)
            flag_taint = False; last = None; continue
        if base in ("push","pop","ret","nop","movaps","movups","movdqa","movdqu","pxor","punpcklqdq"):
            continue
        if base in ("cmp","test"):
            flag_taint = any((regk(a) and regk(a) in taint) or any(mr in taint for mr in mem_regs(a)) for a in args)
            last = ins["raw"].strip(); continue
        if args:
            dst = regk(args[0]); src = False
            for a in args[1:]:
                k = regk(a)
                if k and k in taint: src = True
                if any(mr in taint for mr in mem_regs(a)): src = True
            # read-modify-write destinations keep their incoming taint
            if dst and dst in taint and base in ("and","or","add","sub","adc","sbb",
                    "sar","shr","shl","sal","neg","not","inc","dec","imul","mul"):
                src = True
            if base == "movzx" and "BYTE PTR" in ins["args"] and any(sp in mem_regs(args[-1]) for sp in sptr):
                src = True; secret_loads += 1
            if dst and base in ("mov","movzx","movsx","lea","xor","and","or","add","sub","adc","sbb",
                                "sar","shr","shl","sal","neg","not","inc","dec","imul","mul"):
                if base == "xor" and len(args) == 2 and args[0].strip().lower() == args[1].strip().lower(): taint.discard(dst)
                elif src: taint.add(dst)
                elif base in ("mov","movzx","movsx","lea","and","or","sar","shr","shl","sal","neg","not"): taint.discard(dst)
            if base in FLAG_WRITERS: flag_taint = src; last = ins["raw"].strip()
    if secret_loads == 0: die("no scalar-bit load (movzx BYTE PTR [scalar_ptr+...]) in compiled ladder")
    if cswap_calls < 2: die("expected >= 2 cswap calls in compiled ladder, found %d" % cswap_calls)
    if findings:
        for a, kind, raw, who in findings[:8]:
            print("[fixed_ladder] %s @%s %s | governing=%s" % (kind, a, raw.strip(), who))
        die("%d secret-governed branch/index site(s) in compiled ladder" % len(findings))
    print("[fixed_ladder] A2/A3/A4 PASS: %d scalar-bit loads, %d unconditional cswap calls, 0 secret-governed jcc/index" % (secret_loads, cswap_calls))
    ops = {i["op"] for i in cins}
    # A mask select lowers either to SIMD pand/por (-O2 here) or to GPR
    # and/or (-O1/-O3, and other toolchains).  Both implement the same
    # all-ones/all-zeros mask produced by `neg`; accepting both is a
    # lowering-neutral statement of the SAME CT property, not a weakening:
    # a genuine mask select still needs `neg` plus >=2 selecting ops per
    # coordinate pair, and analyze_cswap() below still fails on any
    # secret-governed branch regardless of lowering.
    simd_ops = [i for i in cins if i["op"] in ("pand", "por")]
    gpr_ops = [i for i in cins if i["op"] in ("and", "or")]
    # SIMD processes two 64-bit limbs per op, GPR one: 4 SIMD selects cover
    # the 4 coordinate pairs that 8 GPR selects cover.
    if "neg" not in ops or not ((len(simd_ops) >= 4) or (len(gpr_ops) >= 8)):
        die("cswap lacks neg + mask-select lowering (simd_selects=%d gpr_selects=%d; ops: %s)"
            % (len(simd_ops), len(gpr_ops), ", ".join(sorted(ops))))
    cfind = analyze_cswap(cins)
    if cfind:
        for a, kind, raw, who in cfind[:8]:
            print("[fixed_ladder] cswap %s @%s %s | governing=%s" % (kind, a, raw.strip(), who))
        die("secret-dependent branch/index in compiled cswap")
    print("[fixed_ladder] A5 PASS: %s mask select, no secret-governed branch" % ("SIMD neg+pand+por" if len(simd_ops) >= 4 else "GPR neg+and+or"))
    print("[fixed_ladder] ASM PASS: real %s machine code bound" % label)
    return 0

if __name__ == "__main__":
    if len(sys.argv) not in (3, 4) or sys.argv[1] not in ("source", "asm"):
        print("usage: eddsa_fixed_ladder_check.py {source|asm} <path> [config-label]")
        sys.exit(2)
    if sys.argv[1] == "source":
        sys.exit(check_source(sys.argv[2]))
    sys.exit(check_asm(sys.argv[2], sys.argv[3] if len(sys.argv) == 4 else "gcc -O2"))
