#!/usr/bin/env python3
"""Compile-check every shell gate in tools/, including the Python inside them.

Why this exists: a gate script is only exercised when its own target runs, which
for the hardware gates means a QEMU boot and a minute of wall time -- and the
whole target is inside a `make -j24 test` that then takes seven minutes to come
back red. A `nonlocal s` in one of them (illegal at module level) cost exactly
that: the gate itself never ran, and the suite reported a SCANOUT failure.

`bash -n` does not look inside a `<<'PY'` heredoc, so both layers are checked:
the shell parses, and every embedded Python block compiles.
"""
import os
import py_compile
import re
import subprocess
import sys
import tempfile

TOOLS = os.path.join(os.path.dirname(os.path.abspath(__file__)))

PY_BLOCK = re.compile(r"<<'PY'\n(.*?)\nPY\n", re.S)


def check_shell(path):
    p = subprocess.run(["bash", "-n", path], capture_output=True, text=True)
    return [] if p.returncode == 0 else ["%s: bash -n: %s" % (path, p.stderr.strip())]


def check_python(path):
    errors = []
    try:
        src = open(path, encoding="utf-8", errors="replace").read()
    except OSError as e:
        return ["%s: %s" % (path, e)]
    for n, block in enumerate(PY_BLOCK.findall(src)):
        with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False) as fh:
            fh.write(block)
            tmp = fh.name
        try:
            py_compile.compile(tmp, cfile=tmp + "c", doraise=True)
        except py_compile.PyCompileError as e:
            msg = str(e).replace(tmp, "%s[PY block %d]" % (path, n + 1))
            errors.append(msg.splitlines()[0] + (" | " + msg.splitlines()[-1] if len(msg.splitlines()) > 1 else ""))
        finally:
            for f in (tmp, tmp + "c"):
                if os.path.exists(f):
                    os.unlink(f)
    return errors


def main():
    scripts = sorted(f for f in os.listdir(TOOLS) if f.endswith(".sh"))
    errors = []
    blocks = 0
    for f in scripts:
        path = os.path.join(TOOLS, f)
        errors += check_shell(path)
        src = open(path, encoding="utf-8", errors="replace").read()
        blocks += len(PY_BLOCK.findall(src))
        errors += check_python(path)
    for e in errors:
        print("  " + e)
    print("gate_scripts: %d script(s), %d embedded python block(s), %d error(s)"
          % (len(scripts), blocks, len(errors)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
