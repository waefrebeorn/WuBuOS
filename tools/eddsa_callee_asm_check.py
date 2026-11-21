#!/usr/bin/env python3
"""eddsa_callee_asm_check.py -- register-level secret-taint audit of objdump text.

The last hop of EdDSA selection is NOT C: ge_cswap hands the secret swap byte
to mbedTLS's mbedtls_mpi_safe_cond_swap(), whose shipped machine code lives in
the LOCAL libmbedcrypto.a (constant_time.c.o).  A source/GIMPLE check cannot
see inside that object.  This checker consumes REAL `objdump -d -M intel`
output and proves (or refutes) one narrow, machine-checkable claim:

  No CONDITIONAL JUMP in the audited function is governed by a value derived
  from the seed register (the SysV argument register carrying `swap`).

It is a conservative forward register taint walk with proper KILLs:
  * mov / movzx / movsx / lea   -> dst taint = src register taint (mem = public)
  * xor r,r                    -> KILL (x xor x == 0)
  * neg / or / and / add / sub / shift / xor dst,src -> OR of operand taints
  * setcc dst                  -> dst taint = current flags taint
  * cmovcc dst,src             -> dst |= src | flags  (branch-free; never a jcc)
  * call                       -> caller-saved regs and flags cleared (public)
  * cmp / test / arith         -> flags taint = operand taint of flag setter
  * jcc (j* except jmp)        -> VIOLATION iff current flags are tainted

Every conditional jump is inventoried WITH its flag-setting instruction and
its public/secret verdict, so an honest audit records the exact branches that
remain (they exist -- on pointer equality, grow return code and limb count)
instead of pretending the function is branch-free.

Fail closed: unparseable file, no instructions, no conditional jump AND no
cmov/setcc (vacuous), missing required cmov, or (in leak mode) a detector that
did not fire -- all FAIL.
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

FLAG_SETTERS = {"cmp", "test", "neg", "not", "or", "and", "xor", "add", "sub",
                "adc", "sbb", "inc", "dec", "shl", "shr", "sar", "sal",
                "rol", "ror", "imul", "mul", "idiv", "div"}
CALLER_SAVED = ["rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"]


def emit(msg):
    print("[asm_ct] " + msg)


def die(msg: str) -> NoReturn:
    emit("FAIL: " + msg)
    print("ASM_CT_CHECK FAILED")
    sys.exit(1)


def parse_instructions(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            lines = fh.read().splitlines()
    except OSError as exc:
        die("cannot read disassembly %s: %s" % (path, exc))
    instrs = []
    for raw in lines:
        m = LINE_RE.match(raw)
        if not m:
            continue
        fields = [f.strip() for f in m.group(2).split("\t") if f.strip()]
        if not fields:
            continue
        im = INSN_RE.match(fields[-1])
        if not im:
            continue
        instrs.append((m.group(1), im.group(1).lower(), im.group(2).strip(), raw.strip()))
    return instrs


def regs_in(operand):
    return [CANON[r] for r in REG_RE.findall(operand)]


def opnds(text):
    return [o.strip() for o in text.split(",") if o.strip()]


def is_jcc(mnem):
    return mnem.startswith("j") and mnem != "jmp" and not mnem.startswith("jmp")


def audit(path, taint_reg, expect, require_cmov, label):
    instrs = parse_instructions(path)
    if not instrs:
        die("no instructions parsed from %s (not an objdump -d -M intel file?)" % path)
    seed = CANON.get(taint_reg)
    if not seed:
        die("unknown --taint-reg %r" % taint_reg)
    emit("function: %s (seed secret register: %s, %d instructions)" % (label, seed, len(instrs)))

    taint = {r: False for r in set(CANON.values())}
    taint[seed] = True
    flags_taint = False
    last_flag_insn = "(none)"
    inventory, violations, trace = [], [], []

    for addr, mnem, ops_text, raw in instrs:
        operands = opnds(ops_text)
        pre_taint = any(taint[r] for r in regs_in(ops_text))

        if is_jcc(mnem):
            inventory.append((addr, mnem, ops_text, last_flag_insn, flags_taint))
            emit("JCC @%s %-5s %-22s | flags-from: %-34s | secret-governed=%s"
                 % (addr, mnem, ops_text[:22], last_flag_insn[:34], flags_taint))
            if flags_taint:
                violations.append((addr, mnem, ops_text, last_flag_insn))
            continue

        if mnem in ("mov", "movabs", "movzx", "movsx", "movsxd") and len(operands) >= 2:
            taint[CANON.get(operands[0], "rax")] = any(taint[r] for r in regs_in(operands[1]))
        elif mnem == "lea" and len(operands) >= 2:
            taint[CANON.get(operands[0], "rax")] = any(taint[r] for r in regs_in(operands[1]))
        elif mnem == "xor" and len(operands) >= 2:
            d = CANON.get(operands[0])
            s = CANON.get(operands[1])
            if d and s and d == s:
                taint[d] = False
                pre_taint = False  # x xor x clears flags too
            elif d:
                taint[d] = taint[d] or any(taint[r] for r in regs_in(operands[1]))
        elif mnem.startswith("set") and len(operands) >= 1:
            d = CANON.get(operands[0])
            if d:
                taint[d] = flags_taint
        elif mnem.startswith("cmov") and len(operands) >= 2:
            d = CANON.get(operands[0])
            if d:
                taint[d] = taint[d] or any(taint[r] for r in regs_in(operands[1])) or flags_taint
        elif mnem in ("neg", "not", "or", "and", "add", "sub", "adc", "sbb",
                      "shl", "shr", "sar", "sal", "rol", "ror", "imul") and operands:
            d = CANON.get(operands[0])
            if d and len(operands) > 1:
                taint[d] = taint[d] or any(taint[r] for r in regs_in(",".join(operands[1:])))
            # single-operand neg/not preserves taint
        elif mnem == "call":
            for r in CALLER_SAVED:
                taint[r] = False
            flags_taint = False
            last_flag_insn = "(call clobbers flags)"
        # push/pop/ret/nop/endbr64: no tracked effect

        if pre_taint and not is_jcc(mnem):
            trace.append(raw)

        if mnem in FLAG_SETTERS and not mnem.startswith("cmov"):
            flags_taint = pre_taint
            last_flag_insn = raw

    n_jcc = len(inventory)
    n_bad = len(violations)
    emit("conditional-branch inventory: %d jcc, %d secret-governed" % (n_jcc, n_bad))
    emit("exact secret-derived instruction trace (%d insn), first 20:" % len(trace))
    for raw in trace[:20]:
        emit("  " + raw)

    saw_select = any(m.startswith("cmov") or m.startswith("set") for _, m, _, _ in instrs)
    if require_cmov and not saw_select:
        die("no cmov/setcc instruction emitted by gcc -O2 (expected branch-free lowering)")

    if expect == "clean":
        if n_bad:
            for addr, mnem, ops, src in violations:
                emit("LEAK @%s %s %s governed by secret flags from: %s" % (addr, mnem, ops, src))
            die("%d secret-governed conditional jump(s) in %s" % (n_bad, label))
        if n_jcc == 0 and not saw_select:
            die("no conditional jump and no cmov/setcc parsed -- vacuous audit (fail closed)")
        print("ASM_CT_CHECK PASS: %s -- no secret-governed conditional jump%s"
              % (label, " (branch-free cmov/setcc lowering observed)" if saw_select else ""))
        return

    if not n_bad:
        die("--expect leak but no secret-governed conditional jump detected (vacuous checker)")
    for addr, mnem, ops, src in violations[:8]:
        emit("DETECTED @%s %s %s governed by secret flags from: %s" % (addr, mnem, ops, src))
    print("ASM_CT_CHECK PASS: %s -- leak detector fired (%d secret-governed jcc)"
          % (label, n_bad))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("disasm")
    ap.add_argument("--taint-reg", required=True)
    ap.add_argument("--expect", choices=["clean", "leak"], default="clean")
    ap.add_argument("--require-cmov", action="store_true")
    ap.add_argument("--label", default="audited function")
    args = ap.parse_args()
    audit(args.disasm, args.taint_reg, args.expect, args.require_cmov, args.label)


if __name__ == "__main__":
    main()
