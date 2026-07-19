#!/usr/bin/env python3
"""wubu_manifest_gen.py -- WuBuOS manifest -> generated C headers.

Adopted from GrahaOS scripts/gcp2wit.py: a single JSON manifest is the
source of truth; kernel (VSL dispatch), Styx9P (op enum), and HolyC (FFI
stubs) are all generated so they cannot disagree.

Usage:
    python3 scripts/wubu_manifest_gen.py \
        --manifest src/runtime/wubu_manifest/wubu_manifest.json \
        --out src/runtime/wubu_manifest/generated

Emits:
    wubu_vsl_dispatch.h  -- VSL_SYS_* numbers + cap-gated dispatch table
    wubu_styx_ops.h      -- Styx9P op enum
    wubu_holyc_ffi.h     -- HolyC FFI forward declarations
"""
import argparse
import json
import os
import sys


def load_manifest(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def emit_vsl_dispatch(m, out_dir):
    p = os.path.join(out_dir, "wubu_vsl_dispatch.h")
    lines = []
    lines.append("/* wubu_vsl_dispatch.h -- GENERATED from wubu_manifest.json. Do not edit. */")
    lines.append("#ifndef WUBU_VSL_DISPATCH_H")
    lines.append("#define WUBU_VSL_DISPATCH_H")
    lines.append("#include <stdint.h>")
    lines.append("/* Syscall numbers (single source: manifest). */")
    for s in m["syscalls"]:
        lines.append("#define VSL_SYS_%s %d" % (s["name"], s["num"]))
    lines.append("")
    lines.append("/* Capability-gated dispatch: caller must hold `req_right` (wubu_cap). */")
    lines.append("typedef struct { const char *handler; uint64_t req_right; } wubu_vsl_disp_t;")
    lines.append("static const wubu_vsl_disp_t wubu_vsl_dispatch[] = {")
    for s in m["syscalls"]:
        right = m["rights"].get(s["cap"], "0")
        lines.append('  [%d] = { .handler="%s", .req_right=%sULL }, /* %s */' %
                     (s["num"], s["handler"], right, s["cap"]))
    lines.append("};")
    lines.append("#endif")
    with open(p, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return p


def emit_styx_ops(m, out_dir):
    p = os.path.join(out_dir, "wubu_styx_ops.h")
    lines = []
    lines.append("/* wubu_styx_ops.h -- GENERATED from wubu_manifest.json. Do not edit. */")
    lines.append("#ifndef WUBU_STYX_OPS_H")
    lines.append("#define WUBU_STYX_OPS_H")
    lines.append("typedef enum {")
    for i, s in enumerate(m["syscalls"]):
        comma = "," if i < len(m["syscalls"]) - 1 else ","
        lines.append("  WUBU_STYX_%s = %d%s /* %s */" % (s["name"], i, comma, s["styx"]))
    lines.append("  WUBU_STYX_COUNT = %d" % len(m["syscalls"]))
    lines.append("} wubu_styx_op_t;")
    lines.append("#endif")
    with open(p, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return p


def emit_holyc_ffi(m, out_dir):
    p = os.path.join(out_dir, "wubu_holyc_ffi.h")
    lines = []
    lines.append("/* wubu_holyc_ffi.h -- GENERATED from wubu_manifest.json. Do not edit. */")
    lines.append("#ifndef WUBU_HOLYC_FFI_H")
    lines.append("#define WUBU_HOLYC_FFI_H")
    for s in m["syscalls"]:
        lines.append("int64 %s(int64 num, int64 rdi, int64 rsi, int64 rdx, int64 r10, int64 r8, int64 r9); /* %s */" %
                     (s["holyc"], s["handler"]))
    lines.append("#endif")
    with open(p, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return p


def main():
    ap = argparse.ArgumentParser(description="WuBuOS manifest -> C headers")
    ap.add_argument("--manifest", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    m = load_manifest(args.manifest)
    os.makedirs(args.out, exist_ok=True)

    for fn in (emit_vsl_dispatch, emit_styx_ops, emit_holyc_ffi):
        p = fn(m, args.out)
        print("wrote %s" % p)
    print("OK: %d syscalls -> 3 headers" % len(m["syscalls"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
