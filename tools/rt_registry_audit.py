#!/usr/bin/env python3
"""rt_registry_audit.py -- find runtime modules that no object list mentions.

`undefined reference` in WuBuOS body tests is usually not a missing function:
the function exists, but its translation unit was never added to RT_OBJS /
KERNEL_OBJS, so only targets with hand-written source lists ever saw it. Those
lists drift; the registry is the thing that should be complete.

Reports every src/runtime/*.c whose stem is not referenced as $(RT)/<stem>.o.
Exit code is 0 always -- this is an audit, not a gate, until the remaining
entries are classified (some are deliberate alternates such as wubu_hw_stub.c
versus the real wubu_hw_detect.c, and should stay unregistered).
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MK = (ROOT / "mk" / "objects.mk").read_text()

registered = set(re.findall(r"\$\(RT\)/([\w/]+)\.o", MK))
sources = [p for p in (ROOT / "src" / "runtime").rglob("*.c")
           if "test" not in p.name and "/tests/" not in str(p)]
missing = sorted(p for p in sources if p.stem not in registered)

print(f"runtime modules: {len(sources)}  registered: {len(registered)}  "
      f"unregistered: {len(missing)}")
for p in missing:
    print(f"  {p.relative_to(ROOT)}")
sys.exit(0)