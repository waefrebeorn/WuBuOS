#!/usr/bin/env python3
"""eddsa_bignum_value_check.py -- bounded abstract interpretation of the
SHIPPED mbedTLS bignum machine code for secret-VALUE control dependence.

The callee-CT gate (tools/eddsa_callee_ct_check.py + eddsa_callee_asm_check.py)
proves that the ladder's *selection byte* does not control a branch.  It
models every callee result and every memory load as PUBLIC, so a secret-derived
VALUE flowing into mbedtls_mpi_add_mpi/sub_mpi/mul_mpi/mod_mpi/cmp_mpi is
still outside its claim.  This checker is the next increment: it carries that
value taint into the object code of the bignum callees themselves.

Input is REAL `objdump -d -M intel` text (from the LOCAL libmbedcrypto.a, no
source, no network, no fixture substituted for the shipped object).  Taint
lattice per register/flags is a subset of:

  P  public                 (unseeded SysV argument, immediate, killed reg)
  S  secret-derived VALUE   (load through a secret mpi pointer, secret arg)
  SP pointer to secret mpi  (seeded mpi * argument, propagated by lea/mov)
  U  UNPROVEN               (call result, pop, unknown mnemonic)

Transfer is a monotone worklist fixpoint over the instruction addresses, so a
join at every branch target is honoured (not a naive linear scan).  Seeds are
explicit per function: `--seed-ptr` args are mpi pointers whose pointed-to
limbs/length/sign are secret-derived; `--seed-val` args are scalar secret
values.  Loads through S/SP yield S; call results are NOT blessed as public.

For every conditional jump the exact flag-setting instruction is inventoried
and classified:

  SECRET_GOVERNED  flags carry S           -> value-independence REFUTED
  UNVERIFIED       flags carry U (call/mem)-> cannot be proven from headers+.a
  PUBLIC           flags carry only P      -> fine

Secret memory indices (`[...+secret_reg*scale]`) are recorded the same way.
Nothing is silently assumed constant-time: an UNVERIFIED site is reported as
an exact unverified instruction, never as a PASS.

Exit codes: 0 = the requested --expect verdict was met; 2 = audit completed
but the requested verdict was NOT met (the verified-negative outcome); 1 =
tool/prereq failure (fail closed, no verdict asserted).
"""

import argparse
import re
import sys
from typing import NoReturn

CANON = {}
for _r in ("rax", "eax", "ax", "al", "ah"):
    CANON[_r] = "rax"
for _r in ("rbx", "ebx", "bx", "bl", "bh"):
    CANON[_r] = "rbx"
for _r in ("rcx", "ecx", "cx", "cl", "ch"):
    CANON[_r] = "rcx"
for _r in ("rdx", "edx", "dx", "dl", "dh"):
    CANON[_r] = "rdx"
for _r in ("rsi", "esi", "si", "sil"):
    CANON[_r] = "rsi"
for _r in ("rdi", "edi", "di", "dil"):
    CANON[_r] = "rdi"
for _r in ("rbp", "ebp", "bp", "bpl"):
    CANON[_r] = "rbp"
for _r in ("rsp", "esp", "sp", "spl"):
    CANON[_r] = "rsp"
for _i in range(8, 16):
    for _s in ("", "d", "w", "b"):
        CANON["r%d%s" % (_i, _s)] = "r%d" % _i
REG_RE = re.compile(r"\b(" + "|".join(sorted(CANON, key=len, reverse=True)) + r")\b")
LINE_RE = re.compile(r"^\s*([0-9a-fA-F]+):\s+(.*)$")
INSN_RE = re.compile(r"^([A-Za-z][\w.]*)\s*(.*)$")
FUNC_RE = re.compile(r"^[0-9a-fA-F]+\s+<([^>]+)>:\s*$")
TARGET_RE = re.compile(r"^\s*([0-9a-fA-F]+)\s")

ALL_REGS = tuple(sorted(set(CANON.values())))
CALLER_SAVED = ("rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11")

P, S, SP, U = "P", "S", "SP", "U"
PUB = frozenset((P,))
UNK = frozenset((U,))

FLAG_SETTERS = {
    "cmp", "test", "neg", "not", "or", "and", "xor", "add", "sub",
    "adc", "sbb", "inc", "dec", "shl", "shr", "sar", "sal", "rol", "ror",
    "imul", "mul", "idiv", "div",
}
BINARY = {
    "add", "sub", "and", "or", "xor", "adc", "sbb", "shl", "shr", "sar",
    "sal", "rol", "ror", "imul",
}
MOVES = {"mov", "movabs", "movzx", "movsx", "movsxd"}
NO_EFFECT = {"endbr64", "nop", "leave", "cld", "std", "push", "ret", "retq",
             "hlt", "ud2", "prefetchnta", "lfence", "mfence", "sfence"}


def emit(msg):
    print("[bnval] " + msg)


def die(msg: str) -> NoReturn:
    emit("FAIL: " + msg)
    print("BNVAL_CHECK FAILED")
    sys.exit(1)


def canon(reg):
    return CANON.get(reg)


def opnds(text):
    return [o.strip() for o in text.split(",") if o.strip()]


def regset(operand):
    return [CANON[r] for r in REG_RE.findall(operand)]


def is_jcc(mnem):
    return mnem.startswith("j") and mnem not in ("jmp", "jmpq") and \
        not mnem.startswith("jmp")


def join(*tagsets):
    out = set()
    for ts in tagsets:
        out |= set(ts)
    return frozenset(out) if out else PUB


def ptr_tags(ts):
    """Arithmetic on an mpi pointer yields a secret VALUE, not a pointer."""
    out = set(ts)
    if SP in out:
        out.discard(SP)
        out.add(S)
    return frozenset(out) if out else PUB


def check_provenance(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            markers = [ln.strip() for ln in fh
                       if ln.startswith(";; WUBU_BIGNUM_VALUE:")]
    except OSError as exc:
        die("cannot read %s for provenance: %s" % (path, exc))
    if not markers:
        die("no ';; WUBU_BIGNUM_VALUE:' provenance marker; cannot bind audit to a real archive")
    emit("provenance: " + markers[0])


def parse_functions(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            lines = fh.read().splitlines()
    except OSError as exc:
        die("cannot read disassembly %s: %s" % (path, exc))
    funcs, cur, name = {}, None, None
    for raw in lines:
        m = FUNC_RE.match(raw.strip())
        if m:
            name = m.group(1)
            cur = []
            funcs[name] = cur
            continue
        lm = LINE_RE.match(raw)
        if not lm or cur is None:
            continue
        fields = [f.strip() for f in lm.group(2).split("\t") if f.strip()]
        if not fields:
            continue
        im = INSN_RE.match(fields[-1])
        if not im:
            continue
        cur.append((int(lm.group(1), 16), im.group(1).lower(),
                    im.group(2).strip(), raw.strip()))
    return funcs


def operand_tags(operand, regs):
    """Lattice value of an operand.  Memory through a secret pointer/value is
    itself secret: that is how mpi limb loads become secret-derived values."""
    if "[" in operand:
        inner = operand[operand.index("[") + 1:operand.rindex("]")]
        rs = regset(inner)
        tags = set()
        for r in rs:
            tags |= set(regs.get(r, PUB))
        if S in tags or SP in tags:
            return frozenset((S,))
        if U in tags:
            return UNK
        return PUB
    rs = regset(operand)
    if rs:
        tags = set()
        for r in rs:
            tags |= set(regs.get(r, PUB))
        return frozenset(tags) if tags else PUB
    return PUB  # immediate / symbol


def tag_str(ts):
    return "{" + ",".join(t for t in (P, S, SP, U) if t in ts) + "}"


def target_addr(ops_text):
    m = TARGET_RE.match(ops_text)
    return int(m.group(1), 16) if m else None


def analyze_function(name, insns, seed_ptrs, seed_vals):
    if not insns:
        die("function %s has no parsed instructions" % name)
    addr_idx = {}
    for i, (addr, _, _, _) in enumerate(insns):
        addr_idx[addr] = i

    initial = {r: PUB for r in ALL_REGS}
    for r in seed_ptrs:
        c = canon(r)
        if not c:
            die("unknown --seed-ptr register %r" % r)
        initial[c] = frozenset((SP,))
    for r in seed_vals:
        c = canon(r)
        if not c:
            die("unknown --seed-val register %r" % r)
        initial[c] = frozenset((S,))

    # in_state[i] is (regs tuple, flags, flag_src).  Regs are keyed by ALL_REGS
    # so the state is hashable and the fixpoint is finite.
    in_regs = [None] * len(insns)
    in_flags = [None] * len(insns)
    flag_src = ["(entry)"] * len(insns)
    init_key = tuple(initial[r] for r in ALL_REGS)
    in_regs[0] = init_key
    in_flags[0] = PUB
    work = [0]
    steps = 0
    n_jcc = 0
    records = {}  # dedup by (addr, kind); the fixpoint may revisit an insn

    while work:
        steps += 1
        if steps > 200000:
            die("abstract-interpretation fixpoint did not converge in %s" % name)
        idx = work.pop()
        regs = dict(zip(ALL_REGS, in_regs[idx]))
        flags = in_flags[idx]
        addr, mnem, ops, raw = insns[idx]
        operands = opnds(ops)
        dst = canon(operands[0]) if operands else None
        src_tags = join(*[operand_tags(o, regs) for o in operands[1:]])
        op_tags = join(*[operand_tags(o, regs) for o in operands])

        if is_jcc(mnem):
            if S in flags:
                verdict = "SECRET_GOVERNED"
            elif U in flags:
                verdict = "UNVERIFIED"
            else:
                verdict = "PUBLIC"
            records[(addr, "JCC")] = (addr, "JCC", mnem, ops, raw, flag_src[idx],
                                      tag_str(flags), verdict)
            successors = []
            ta = target_addr(ops)
            if ta is not None and ta in addr_idx:
                successors.append(addr_idx[ta])
            successors.append(idx + 1)
        elif mnem == "jmp" or mnem == "jmpq":
            ta = target_addr(ops)
            successors = [addr_idx[ta]] if ta in addr_idx else list(range(len(insns)))
        elif mnem in ("ret", "retq"):
            successors = []
        else:
            successors = [idx + 1]

        # Secret/unproven memory INDEX is a value-dependence sink too.
        if "[" in ops and mnem not in ("nop", "lea", "endbr64", "cs", "ds",
                                       "es", "ss", "fs", "gs", "lock", "rep",
                                       "repz", "repnz", "bnd", "data16", "addr32"):
            inner = ops[ops.index("["):ops.rindex("]") + 1]
            # x86 index register is the second register inside the brackets.
            br = regset(inner)
            if len(br) >= 2:
                itags = regs.get(br[1], PUB)
                if S in itags or SP in itags:
                    records[(addr, "INDEX")] = (addr, "INDEX", mnem, ops, raw,
                                                "secret index " + br[1], tag_str(itags),
                                                "SECRET_GOVERNED")
                elif U in itags:
                    records[(addr, "INDEX")] = (addr, "INDEX", mnem, ops, raw,
                                                "unproven index " + br[1], tag_str(itags),
                                                "UNVERIFIED")

        # ---- transfer ----
        new_regs = dict(regs)
        new_flags = flags
        new_src = flag_src[idx]

        if is_jcc(mnem) or mnem in ("jmp", "jmpq"):
            pass  # a control transfer writes neither registers nor flags
        elif mnem in MOVES and len(operands) >= 2 and dst:
            new_regs[dst] = operand_tags(operands[1], regs)
        elif mnem == "lea" and len(operands) >= 2 and dst:
            tags = set()
            for o in operands[1:]:
                tags |= set(operand_tags(o, regs))
            if SP in tags:
                new_regs[dst] = frozenset((SP,))
            elif S in tags:
                new_regs[dst] = frozenset((S,))
            elif U in tags:
                new_regs[dst] = UNK
            else:
                new_regs[dst] = PUB
        elif mnem == "xor" and len(operands) >= 2 and dst and canon(operands[1]) == dst:
            new_regs[dst] = PUB
            new_flags = PUB
            new_src = raw
        elif mnem == "sub" and len(operands) >= 2 and dst and canon(operands[1]) == dst:
            new_regs[dst] = PUB
            new_flags = PUB
            new_src = raw
        elif mnem in BINARY and dst:
            new_regs[dst] = ptr_tags(join(regs.get(dst, PUB), src_tags))
        elif mnem in ("neg", "not", "inc", "dec") and dst:
            new_regs[dst] = ptr_tags(join(regs.get(dst, PUB), operand_tags(operands[0], regs)))
        elif mnem in ("mul", "idiv", "div"):
            new_regs["rax"] = ptr_tags(op_tags)
            new_regs["rdx"] = ptr_tags(op_tags)
        elif mnem.startswith("set") and dst:
            new_regs[dst] = flags
        elif mnem.startswith("cmov") and dst:
            new_regs[dst] = join(regs.get(dst, PUB), src_tags, flags)
        elif mnem == "call":
            for r in CALLER_SAVED:
                new_regs[r] = UNK
            new_flags = UNK
            new_src = raw
        elif mnem in ("pop",) and dst:
            new_regs[dst] = UNK
        elif mnem == "push":
            pass
        elif mnem in NO_EFFECT:
            pass
        elif mnem in ("cmp", "test"):
            new_flags = ptr_tags(op_tags)
            new_src = raw
        elif mnem in FLAG_SETTERS:
            new_flags = ptr_tags(op_tags)
            new_src = raw
        else:
            # Unknown mnemonic: fail closed on its destination and flags.
            if dst:
                new_regs[dst] = UNK
            new_flags = UNK
            new_src = raw

        if mnem in FLAG_SETTERS and not mnem.startswith("cmov"):
            new_flags = ptr_tags(op_tags)
            new_src = raw

        key = tuple(new_regs[r] for r in ALL_REGS)
        for succ in successors:
            if succ < 0 or succ >= len(insns):
                continue
            oldk = in_regs[succ]
            merged = key if oldk is None else tuple(
                join(frozenset(a), frozenset(b)) for a, b in zip(oldk, key))
            mf = (new_flags if in_flags[succ] is None
                  else join(in_flags[succ], new_flags))
            if oldk is None or merged != oldk or mf != in_flags[succ]:
                in_regs[succ] = merged
                in_flags[succ] = mf
                flag_src[succ] = new_src
                work.append(succ)

    recs = [records[k] for k in sorted(records)]
    bad = [r for r in recs if r[7] != "PUBLIC"]
    secrets = [r for r in recs if r[7] == "SECRET_GOVERNED"]
    unver = [r for r in recs if r[7] == "UNVERIFIED"]
    emit("function %s: seed_ptr=[%s] seed_val=[%s] insns=%d sites=%d jcc=%d "
         "secret_governed=%d unverified=%d"
         % (name, ",".join(seed_ptrs) or "-", ",".join(seed_vals) or "-",
            len(insns), len(recs), sum(1 for r in recs if r[1] == "JCC"),
            len(secrets), len(unver)))
    return recs, bad


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("disasm")
    ap.add_argument("--func", action="append", default=[],
                    help="function to audit (repeatable); default = all")
    ap.add_argument("--seed-ptr", default="",
                    help="comma-separated secret mpi * argument registers")
    ap.add_argument("--seed-val", default="",
                    help="comma-separated secret scalar argument registers")
    ap.add_argument("--expect", choices=["clean", "negative", "leak"],
                    default="negative")
    ap.add_argument("--min-insns", type=int, default=10)
    ap.add_argument("--min-jcc", type=int, default=1)
    ap.add_argument("--max-print", type=int, default=40)
    ap.add_argument("--allow-branchfree", action="store_true",
                    help="permit a function with zero jcc/index sites when a "
                         "branch-free select idiom (cmov/setcc/sar+and+xor) is present")
    ap.add_argument("--evidence-out", default="",
                    help="write the full exact unverified-instruction list here")
    args = ap.parse_args()

    check_provenance(args.disasm)
    funcs = parse_functions(args.disasm)
    if not funcs:
        die("no functions parsed from %s (not an objdump -d -M intel file?)" % args.disasm)
    names = args.func or sorted(funcs)
    for n in names:
        if n not in funcs:
            die("required function %s not present in disassembly" % n)

    ptrs = [s.strip() for s in args.seed_ptr.split(",") if s.strip()]
    vals = [s.strip() for s in args.seed_val.split(",") if s.strip()]
    all_records, all_bad, labeled = [], [], []
    for n in names:
        insns = funcs[n]
        if len(insns) < args.min_insns:
            die("function %s has only %d instructions (vacuous; need >=%d)"
                % (n, len(insns), args.min_insns))
        recs, bad = analyze_function(n, insns, ptrs, vals)
        if sum(1 for r in recs if r[1] == "JCC") < args.min_jcc and not bad:
            die("function %s has too few conditional jumps to be a meaningful audit" % n)
        if not recs:
            mnems = {i[1] for i in funcs[n]}
            select_ok = bool(mnems & {"cmovbe", "cmovne", "cmove", "cmova", "cmovae",
                                      "sete", "setne", "seta", "setae", "setb",
                                      "setbe", "sbb", "sar"}) and bool(mnems & {"and", "xor"})
            if not (args.allow_branchfree and select_ok):
                die("function %s produced an empty control/address inventory (vacuous; "
                    "no jcc/index and no branch-free select idiom)" % n)
            emit("function %s: BRANCH-FREE (0 jcc/index; mask/cmov select idiom present)" % n)
        all_records.extend(recs)
        all_bad.extend(bad)
        for r in recs:
            labeled.append((n, r))
            emit("  %s @%04x %-5s %-28s flags-from: %-30s tags=%-10s VERDICT=%s"
                 % (r[1], r[0], r[2], r[3][:28], r[5][:30], r[6], r[7]))

    emit("TOTAL: %d inventoried sites, %d non-public "
         "(%d SECRET_GOVERNED, %d UNVERIFIED)"
         % (len(all_records), len(all_bad),
            sum(1 for r in all_bad if r[7] == "SECRET_GOVERNED"),
            sum(1 for r in all_bad if r[7] == "UNVERIFIED")))

    lines = ["[bnval] EXACT NON-PUBLIC INSTRUCTION LIST (%d):" % len(all_bad)]
    for fn, r in labeled:
        if r[7] != "PUBLIC":
            lines.append("[bnval]   %s @%04x %-6s %s | governing: %s | tags=%s | %s"
                         % (fn, r[0], r[1], r[4], r[5], r[6], r[7]))
    exact = "\n".join(lines) + "\n"
    for ln in lines[:args.max_print + 1]:
        print(ln)
    if len(lines) - 1 > args.max_print:
        emit("... %d more exact records (full list: %s)"
             % (len(lines) - 1 - args.max_print,
                args.evidence_out or "use --evidence-out"))
    if args.evidence_out:
        try:
            with open(args.evidence_out, "w", encoding="utf-8") as fh:
                fh.write(exact)
        except OSError as exc:
            die("cannot write evidence file %s: %s" % (args.evidence_out, exc))

    n_secret = sum(1 for r in all_bad if r[7] == "SECRET_GOVERNED")
    n_unver = sum(1 for r in all_bad if r[7] == "UNVERIFIED")
    ok = {
        "clean": n_secret == 0 and n_unver == 0,
        "negative": len(all_bad) > 0,
        "leak": n_secret > 0,
    }[args.expect]
    if not ok:
        emit("verified-negative outcome: expect=%s secret=%d unverified=%d "
             "(requested verdict NOT met)" % (args.expect, n_secret, n_unver))
        sys.exit(2)
    emit("verdict met: expect=%s secret=%d unverified=%d" %
         (args.expect, n_secret, n_unver))
    print("BNVAL_CHECK PASS: expect=%s" % args.expect)


if __name__ == "__main__":
    main()
