#!/usr/bin/env python3
"""Ratchet for STATIC_ANALYZE_SKIP: the skip list may only shrink.

A file can leave the list once it parses under the narrow include set.
Adding a name silently would turn a measured gate into a paper gate, so
this fails on any name not present in the recorded baseline.
"""
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
mk = (root / "mk" / "targets.mk").read_text()
start = mk.index("STATIC_ANALYZE_SKIP")
end = mk.index("STATIC_METAL_SRCS")
chunk = mk[start:end]
current = set(re.findall(r"[A-Za-z0-9_]+\.c", chunk))
baseline = {
    line.strip()
    for line in (root / "tools" / "static_skip_baseline.txt").read_text().splitlines()
    if line.strip()
}
added = sorted(current - baseline)
if added:
    print("FAIL: new static_analysis skips: " + ", ".join(added))
    sys.exit(1)
print(
    "STATIC SKIP RATCHET PASS: %d/%d baseline skips remain (%d cleared)"
    % (len(current & baseline), len(baseline), len(baseline - current))
)
