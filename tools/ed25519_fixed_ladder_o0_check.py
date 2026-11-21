#!/usr/bin/env python3
"""eddsa_fixed_ladder_o0_check.py -- STACK-AWARE structural constant-time
checker for the shipped bignum-free Ed25519 ladder compiled at -O0.

Why a separate checker
----------------------
tools/ed25519_fixed_ladder_check.py tracks taint in *registers* only.  That is
correct for gcc -O2/-O3 where the scalar bit and the swap byte stay in registers,
but at -O0 they are spilled to stack slots ([rbp-X]).  At -O0 the shipped code
looks like:

    mov  QWORD PTR [rbp-0x240], rsi        ; scalar pointer argument -> slot
    ...
    mov  rax, QWORD PTR [rbp-0x240]        ; reload scalar pointer
    add  rax, rdx                          ; + byte offset
    movzx eax, BYTE PTR [rax]              ; <-- SECRET SOURCE (byte load)
    ...
    mov  BYTE PTR [rbp-0x225], al          ; bit slot
    movzx eax, BYTE PTR [rbp-0x225]        ; reload bit
    xor  BYTE PTR [rbp-0x226], al          ; swap ^= bit  (swap slot)
    movzx edx, BYTE PTR [rbp-0x226]        ; swap -> cswap 3rd arg

A register-only checker sees neither the source nor the flow, so at -O0 it
cannot distinguish the shipped ladder from a leaked one -- a RED verdict would
be vacuous.  This checker adds a stack-slot taint lattice on top of the register
lattice, so the SAME CT property is checked on REAL -O0 machine code:

  O1  constant 256 trip count survives -O0 (mov [slot],0x100 / sub [slot],1)
  O2  the scalar byte load through the scalar-pointer slot is a taint source
  O3  taint propagates through registers AND stack slots to the cswap swap arg
  O4  no conditional jump is governed by a tainted register, stack slot or flag
  O5  no memory access is indexed by a tainted register
  O6  cswap is a mask select (neg + not + and/or) with no secret-governed branch

Modes (both fail closed, exit 1 on any unmet obligation):

  source <wubu_ed25519_fixed.c>   delegates to the toolchain-independent
                                  source contract in ed25519_fixed_ladder_check.py
  asm <objdump -d -M intel file>  stack-aware analysis of REAL -O0 machine code

This is STRUCTURAL / MACHINE-CODE evidence, NOT a formal constant-time proof.
See docs/ED25519_FIXED_LADDER_O0.md.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ed25519_fixed_ladder_check as base  # noqa: E402  (reuse source checks/parser)

LADDER = base.LADDER
CSWAP = base.CSWAP
JCC = base.JCC

# [rbp+/-disp] with no index register -> canonical stack-slot key.
SLOT_RE = re.compile(r"^\[(?:rbp|ebp)\s*([+-])\s*(0x[0-9a-f]+|[0-9]+)\]$", re.I)

ARITH = {"add", "sub", "and", "or", "xor", "adc", "sbb", "sar", "shr", "shl",
         "sal", "neg", "not", "inc", "dec", "imul", "mul"}
MOVZX = {"movzx", "movsx", "movsxd"}
PTR_ARITH = {"add", "sub", "lea"}


def die(msg):
    print("[fixed_ladder_o0] FAIL: " + msg)
    print("FIXED_LADDER_O0_CHECK FAILED")
    sys.exit(1)


def slot_of(op):
    s = op.strip()
    if "[" not in s or "]" not in s:
        return None
    br = s[s.find("["):s.rfind("]") + 1]
    if not SLOT_RE.match(br):
        return None
    return re.sub(r"\s+", "", br).lower()


def is_jcc(op):
    # Conditional jumps only: `jmp` is unconditional and must not be counted.
    return bool(JCC.match(op)) and op not in ("jmp", "jmpq")


def operand_taint(a, reg, slot):
    """Secret-value taint of one instruction operand."""
    t = False
    k = base.regk(a.strip())
    if k and reg.get(k, False):
        t = True
    s = slot_of(a)
    if s and slot.get(s, False):
        t = True
    for mr in base.mem_regs(a):        # value-tainted base/index register
        if reg.get(mr, False):
            t = True
    return t


def walk(insns, sptr_slots, seed_d, check_cswap_arg):
    """Forward taint walk over one function at -O0.

    Lattices: reg (value), sptr (holds scalar pointer), slot (stack value),
    flag.  Returns (findings, secret_loads, cswap_calls, tainted_logic, sptr_slots).
    """
    reg = {"d": True} if seed_d else {}
    sptr = {}
    slot = {}
    flag = False
    last = None
    findings = []
    secret_loads = 0
    cswap_calls = 0
    tainted_logic = []

    for ins in insns:
        op = ins["op"]
        args = base.operands(ins["args"])
        addr = ins["addr"]
        raw = ins["raw"]

        # O5: secret-governed memory index/base (a *pointer* register is not
        # itself secret -- only a value-tainted index is a violation).
        for a in args:
            if "[" in a:
                for mr in base.mem_regs(a):
                    if reg.get(mr, False):
                        findings.append((addr, "INDEX", raw, mr))

        # O4: secret-governed conditional jump.
        if is_jcc(op):
            if flag:
                findings.append((addr, "JCC", raw, last or "?"))
            continue

        if op == "call":
            if CSWAP in raw:
                cswap_calls += 1
                if check_cswap_arg and not reg.get("d", False):
                    findings.append((addr, "CSWAP_ARG", raw,
                                     "swap edx not secret-derived"))
            # -O0 keeps all locals in stack slots; only caller-saved regs die.
            for r in ("a", "c", "d", "si", "di"):
                reg[r] = False
                sptr[r] = False
            flag = False
            last = None
            continue

        if op in ("cmp", "test"):
            flag = any(operand_taint(a, reg, slot) for a in args)
            last = raw.strip()
            continue

        if op == "mov":
            if len(args) < 2:
                continue
            src, dst = args[-1], args[0]
            ss, sr = slot_of(src), base.regk(src)
            if ss is not None and ss in sptr_slots:
                val, sp = False, True
            elif ss is not None:
                val, sp = slot.get(ss, False), False
            elif sr is not None:
                val, sp = reg.get(sr, False), sptr.get(sr, False)
            else:
                val, sp = False, False
            ds, dr = slot_of(dst), base.regk(dst)
            if ds is not None:
                # The slot identified in the pre-scan is the one that receives
                # the scalar-pointer argument (rsi): storing into it stores a
                # secret *pointer*, not a secret value.
                if ds in sptr_slots:
                    sp = True
                if sp:
                    sptr_slots.add(ds)
                    slot[ds] = False
                else:
                    slot[ds] = val
                    sptr_slots.discard(ds)
            elif dr is not None:
                reg[dr] = val
                sptr[dr] = sp
            continue

        if op in MOVZX:
            if len(args) < 2:
                continue
            src, dst = args[-1], args[0]
            ss, sr = slot_of(src), base.regk(src)
            val, sp = False, False
            if ss is not None:
                # Reload from a named stack slot (the -O0 spill path).
                val = slot.get(ss, False)
            elif "[" in src:
                mregs = base.mem_regs(src)
                src_sptr = any(sptr.get(m, False) for m in mregs)
                src_val = any(reg.get(m, False) for m in mregs)
                val = src_sptr or src_val
                if src_sptr:
                    secret_loads += 1
            elif sr is not None:
                val, sp = reg.get(sr, False), sptr.get(sr, False)
            ds, dr = slot_of(dst), base.regk(dst)
            if ds is not None:
                slot[ds] = val
                sptr_slots.discard(ds)
            elif dr is not None:
                reg[dr] = val
                sptr[dr] = sp
            continue

        if op == "lea":
            if args:
                dr = base.regk(args[0])
                if dr:
                    reg[dr] = False
                    sptr[dr] = False
            continue

        if op in ARITH:
            if not args:
                continue
            dst = args[0]
            ds, dr = slot_of(dst), base.regk(dst)
            srcval, srcsptr = False, False
            for a in args[1:]:
                srcval = srcval or operand_taint(a, reg, slot)
                k = base.regk(a)
                if k and sptr.get(k, False):
                    srcsptr = True
            # read-modify-write keeps incoming taint
            if dr is not None and reg.get(dr, False):
                srcval = True
            if dr is not None and sptr.get(dr, False):
                srcsptr = True
            if ds is not None and slot.get(ds, False):
                srcval = True
            if (op == "xor" and len(args) == 2 and
                    base.regk(args[1]) is not None and base.regk(args[1]) == dr):
                srcval, srcsptr = False, False
            if ds is not None:
                slot[ds] = slot.get(ds, False) or srcval
            elif dr is not None:
                reg[dr] = srcval
                sptr[dr] = srcsptr if op in PTR_ARITH else False
            if op in ("and", "or", "pand", "por") and srcval:
                tainted_logic.append((addr, op))
            if op in base.FLAG_WRITERS:
                flag = srcval
                last = raw.strip()
            continue
        # endbr64/push/pop/leave/ret/nop/jmp/etc.: no taint effect.

    return findings, secret_loads, cswap_calls, tainted_logic, sptr_slots


def check_o0_asm(path, label="gcc -O0"):
    funcs = base.parse_asm(path)
    if LADDER not in funcs:
        die("no <%s> in objdump output" % LADDER)
    if CSWAP not in funcs:
        die("no <%s> in objdump output" % CSWAP)
    lins, cins = funcs[LADDER], funcs[CSWAP]
    if len(lins) < 80 or len(cins) < 30:
        die("suspiciously small -O0 bodies (ladder=%d cswap=%d)" % (len(lins), len(cins)))

    # O1: constant trip count survives -O0 as a stack slot.
    const256 = [i for i in lins
                if i["op"] == "mov" and re.search(r",0x100\b", i["args"])]
    if not const256:
        die("no `mov <slot|reg>,0x100` (constant 256 trip count) in -O0 ladder")
    cto = base.operands(const256[0]["args"])[0]
    ctr_slot = slot_of(cto)
    dec = [i for i in lins if i["op"] == "sub" and
           re.search(r"\b0x1\b", i["args"]) and
           ((ctr_slot and slot_of(base.operands(i["args"])[0]) == ctr_slot) or
            (not ctr_slot and base.regk(base.operands(i["args"])[0]) == base.regk(cto)))]
    if not dec:
        die("counter %s not decremented by 1 in -O0 ladder" % cto)
    print("[fixed_ladder_o0] O1 PASS: constant 256 trip count (%s) survives %s"
          % (cto, label))

    # Discover the scalar-pointer stack slot(s): the -O0 store of arg rsi.
    sptr_slots = set()
    for ins in lins:
        args = base.operands(ins["args"])
        if ins["op"] == "mov" and len(args) == 2 and "QWORD PTR" in ins["args"].upper():
            s = slot_of(args[0])
            if s and base.regk(args[1]) == "si":
                sptr_slots.add(s)
    if not sptr_slots:
        die("cannot find scalar-pointer stack slot (mov [rbp-X],rsi) in -O0 ladder")

    findings, secret_loads, cswap_calls, _logic, _sp = walk(
        lins, set(sptr_slots), seed_d=False, check_cswap_arg=True)

    if secret_loads == 0:
        die("no scalar-byte taint source (movzx <reg>,BYTE PTR [scalar_ptr]) in -O0 ladder")
    if cswap_calls < 2:
        die("expected >= 2 cswap calls in -O0 ladder, found %d" % cswap_calls)
    if findings:
        for a, kind, raw, who in findings[:8]:
            print("[fixed_ladder_o0] %s @%s %s | governing=%s"
                  % (kind, a, raw.strip(), who))
        die("%d secret-governed branch/index site(s) in -O0 ladder" % len(findings))
    print("[fixed_ladder_o0] O2/O3/O4/O5 PASS: %d scalar-byte taint source(s), "
          "%d unconditional cswap calls, 0 secret-governed jcc/index "
          "(stack slots tracked)" % (secret_loads, cswap_calls))

    # O6: cswap mask select at -O0 (loop not unrolled: a single select pair in
    # the body suffices; tainted logic ops prove swap actually drives it).
    cops = {i["op"] for i in cins}
    simd = [i for i in cins if i["op"] in ("pand", "por")]
    gpr_and = [i for i in cins if i["op"] == "and"]
    gpr_or = [i for i in cins if i["op"] == "or"]
    if "neg" not in cops or not (
            (len(gpr_and) >= 2 and len(gpr_or) >= 2) or
            (len(simd) >= 2)):
        die("cswap lacks -O0 mask-select lowering (neg=%s and=%d or=%d simd=%d)"
            % ("neg" in cops, len(gpr_and), len(gpr_or), len(simd)))
    cfind, _sl, _cc, ctl, _sp2 = walk(cins, set(), seed_d=True, check_cswap_arg=False)
    if len(ctl) < 2:
        die("swap taint does not reach the cswap mask select (tainted_logic=%d)"
            % len(ctl))
    if cfind:
        for a, kind, raw, who in cfind[:8]:
            print("[fixed_ladder_o0] cswap %s @%s %s | governing=%s"
                  % (kind, a, raw.strip(), who))
        die("secret-dependent branch/index in compiled -O0 cswap")
    print("[fixed_ladder_o0] O6 PASS: cswap %s mask select, swap drives %d "
          "select op(s), no secret-governed branch"
          % ("GPR neg+not+and/or" if len(gpr_and) >= 2 else "SIMD", len(ctl)))
    print("[fixed_ladder_o0] ASM PASS: real %s machine code bound "
          "(stack-aware)" % label)
    return 0


def main():
    if len(sys.argv) not in (3, 4) or sys.argv[1] not in ("source", "asm"):
        print("usage: ed25519_fixed_ladder_o0_check.py {source|asm} <path> [config-label]")
        return 2
    if sys.argv[1] == "source":
        return base.check_source(sys.argv[2])
    return check_o0_asm(sys.argv[2],
                        sys.argv[3] if len(sys.argv) == 4 else "gcc -O0")


if __name__ == "__main__":
    sys.exit(main())
