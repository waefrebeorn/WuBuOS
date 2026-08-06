#!/usr/bin/env python3
"""Generate the xdg-shell server protocol C file in the real wubuos repo.
Solves the wubunos phantom path problem (deleted; see AGI_OS.md): we find the real repo
by glob, never by hardcoded path. Then generate via wayland-scanner."""
import glob, os, subprocess

# Find the real repo
hdrs = glob.glob("/home/wubu/*/src/gui/xdg-shell-server-protocol.h")
real = [h for h in hdrs]
assert len(real) == 1, f"expected 1 real xdg-shell-server-protocol.h, got {real}"
hdr = real[0]
gui_dir = os.path.dirname(hdr)
print("real gui dir:", gui_dir)

xml = "/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml"
out_c = os.path.join(gui_dir, "xdg-shell-server-protocol.c")
print("generating:", out_c)

r = subprocess.run(
    ["wayland-scanner", "private-code", xml, out_c],
    capture_output=True, text=True, timeout=30
)
print("scanner exit:", r.returncode)
if r.stdout: print("stdout:", r.stdout)
if r.stderr: print("stderr:", r.stderr)

assert os.path.exists(out_c), "output not generated"
print("generated OK:", os.path.getsize(out_c), "bytes")
