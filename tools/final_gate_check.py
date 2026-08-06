#!/usr/bin/env python3
"""Full wubuos gate — final verification before commit."""
import glob, os, subprocess

repo = "/home/wubu/wubuos"
r = subprocess.run(["make", "-C", repo, "test"], capture_output=True, text=True, timeout=300)
print("WUBUOS make test EXIT:", r.returncode)
out = r.stdout + r.stderr
fails = [l for l in out.splitlines() if any(k in l for k in ["No rule", "Error 1", "❌", "FAIL:", "undefined reference"])]
print("fail lines:", len(fails))
for l in fails[:10]:
    print(" ", l)
# tier completions
for l in out.splitlines():
    if "Tier" in l and ("complete" in l or "passed" in l):
        print(" ", l.strip())
print("--- tail ---")
for l in out.splitlines()[-3:]:
    print(l)
