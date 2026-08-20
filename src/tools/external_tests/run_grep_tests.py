#!/usr/bin/env python3
"""Rigorous grep-parity harness.

For every line STATUS@PAT@INPUT[@NOTE] in GNU grep's bre.tests/ere.tests,
we use GNU grep ITSELF as the oracle: run `grep -E` (or -G) with PAT on INPUT,
capture (rc, stdout). Then run wubugrep the same way and require byte-identical
(rc + stdout). This is the "byte-identical to grep" contract, derived from the
real binary rather than the file's sometimes-stale STATUS field.

The file's STATUS field is used only as a sanity cross-check (informational).
"""
import subprocess, sys, os, tempfile, argparse

HERE = os.path.dirname(os.path.abspath(__file__))
WUB = os.path.abspath(os.path.join(HERE, "..", "wubugrep"))
GREP = "/usr/bin/grep"

def run(binpath, flag, pat, inp):
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write(inp + "\n")
        path = f.name
    try:
        p = subprocess.run([binpath, flag, pat, path],
                           capture_output=True, timeout=10)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired:
        return 124, b""
    finally:
        os.unlink(path)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("suite")
    ap.add_argument("--mode", choices=["bre","ere"], required=True)
    args = ap.parse_args()
    flag = "-E" if args.mode == "ere" else "-G"

    passn = failn = 0
    fails = []
    n = 0
    with open(args.suite) as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("@")
            status = parts[0]; pat = parts[1]; inp = parts[2]
            n += 1
            g_rc, g_out = run(GREP, flag, pat, inp)
            w_rc, w_out = run(WUB, flag, pat, inp)
            if g_rc == w_rc and g_out == w_out:
                passn += 1
            else:
                failn += 1
                fails.append((pat, inp, g_rc, g_out, w_rc, w_out, status))
    print(f"=== {args.mode.upper()} parity vs GNU grep ({args.suite}) ===")
    print(f"cases={n}  PASS={passn}  FAIL={failn}  ({100.0*passn/max(n,1):.1f}% identical)")
    if fails:
        print(f"--- {len(fails)} divergences (first 50) ---")
        for pat, inp, g_rc, g_out, w_rc, w_out, status in fails[:50]:
            g = g_out if g_rc == 0 else f"<rc={g_rc}>"
            w = w_out if w_rc == 0 else f"<rc={w_rc}>"
            print(f"  pat={pat!r} inp={inp!r} [file:{status}]")
            print(f"      grep : rc={g_rc} out={g_out!r}")
            print(f"      wubu : rc={w_rc} out={w_out!r}")
    sys.exit(1 if failn else 0)

if __name__ == "__main__":
    main()
