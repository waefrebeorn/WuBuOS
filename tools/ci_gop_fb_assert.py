#!/usr/bin/env python3
"""ci_gop_fb_assert.py -- L4 GOP/Limine framebuffer fallback gate.

Proves the GOP fallback display path with REAL pixels, not a banner:

  (b) forced-GOP boot: kernel rebuilt with -DWUBU_FORCE_GOP_FB=1, booted
      through the WuBuFW measured chain (firmware GOP -> chainloader
      Limine-fb handoff -> kernel). Asserts:
        - the loader published the GOP framebuffer (address/pitch),
        - the kernel mapped it ('display LIVE via GOP' + phys/pitch),
        - the render selftest wrote a pattern through the render target
          and read it back from the mapped fb ('GOP fb selftest PASS'),
        - the kernel's fb phys agrees with the loader's GOP handoff phys.
  (c) make kernel-strict: 0 errors.
  (a) default boot unchanged: default kernel rebuilt, WuBuFW measured
      chain still green (run-agi.sh) AND ci_boot_assert.py PASS.

Usage: python3 tools/ci_gop_fb_assert.py   (from the repo root)
Exit: 0 all gates pass, 1 regression, 2 missing tool/build failure.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FW = os.path.join(ROOT, "src", "firmware")
LOG = os.path.join(FW, "build", "agi-boot.log")


def agi_window():
    return int(os.environ.get("WUBU_AGI_BOOT_WINDOW", "40"))


def boot_window():
    return int(os.environ.get("WUBU_BOOT_WINDOW", "45"))


def run(cmd, cwd=ROOT, timeout=900, env_extra=None):
    print("+ " + " ".join(cmd), flush=True)
    env = dict(os.environ)
    # Slow-host headroom for the fixed QEMU boot window (ci_boot_assert
    # uses the same pattern with WUBU_BOOT_WINDOW).
    env.setdefault("WUBU_AGI_BOOT_WINDOW", "40")
    if env_extra:
        env.update(env_extra)
    r = subprocess.run(cmd, cwd=cwd, timeout=timeout, env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = r.stdout.decode(errors="replace")
    return r.returncode, out


def default_kernel_cflags():
    """Read the pinned KERNEL_CFLAGS from mk/pattern-rules.mk so the forced
    build changes ONLY the added define (never the pinned arch/flags)."""
    txt = open(os.path.join(ROOT, "mk", "pattern-rules.mk")).read()
    m = re.search(r"^KERNEL_CFLAGS\s*=\s*(.+)$", txt, re.M)
    if not m:
        print("FAIL: cannot read KERNEL_CFLAGS from mk/pattern-rules.mk")
        sys.exit(2)
    return m.group(1).strip()


def tail(out, n=25):
    lines = out.splitlines()
    return "\n".join(lines[-n:])


def main():
    cflags = default_kernel_cflags()

    # ---- (b) forced-GOP boot through the WuBuFW measured chain --------
    print("=== gate (b): forced-GOP boot (WUBU_FORCE_GOP_FB) ===", flush=True)
    rc, out = run(["make", "-B", "kernel", "-j24",
                   "KERNEL_CFLAGS=" + cflags + " -DWUBU_FORCE_GOP_FB=1"])
    if rc != 0:
        print("FAIL: forced kernel build failed\n" + tail(out))
        return 2
    # Honest MMIO rendering is ~100x slower than RAM under TCG (every pixel
    # write is an uncached device exit), so bonzi's 5 Hz full-frame redraw
    # cannot keep its heartbeat cadence. Swap the liveness oracle to the
    # supervisor's continuous promotions (run-agi.sh WUBU_AGI_LIVENESS);
    # "Bonzi Buddy loop active" still proves a full MMIO frame rendered.
    rc, out = run(["bash", "run-agi.sh"], cwd=FW, timeout=agi_window() + 180,
                  env_extra={"WUBU_AGI_BOOT_WINDOW":
                             os.environ.get("WUBU_AGI_BOOT_WINDOW", "90"),
                             "WUBU_AGI_LIVENESS": "promoted"})
    if rc != 0:
        for line in out.splitlines():
            if "MISSING" in line or "RESULT" in line or "ERROR MARKERS" in line:
                print(line)
        # Devil's advocate: the marker list says WHAT is missing, not WHERE
        # the chain stopped. Dump the boot log's last words — the loader
        # prints explicit FAIL markers (no ESP / AllocatePages / unreadable
        # N bytes / not ELF64) that pinpoint the stall.
        try:
            log = open(LOG, "rb").read().decode(errors="replace")
            print("--- agi-boot.log tail (forced GOP boot) ---")
            print("\n".join(log.splitlines()[-40:]))
        except OSError as e:
            print("no boot log: %s" % e)
        print("FAIL: WuBuFW measured chain broke under forced GOP")
        return 1
    data = open(LOG, "rb").read()
    # Keep the forced-boot trace: gate (a)'s default boot reuses the same
    # log path and would otherwise overwrite the evidence.
    with open(LOG + ".gop", "wb") as f:
        f.write(data)

    checks = [
        (b"WUBU_FORCE_GOP_FB set", "kernel forced-GOP marker"),
        (b"GOP fb handoff:", "loader published GOP framebuffer"),
        (b"display LIVE via GOP", "kernel mapped the GOP framebuffer"),
        (b"GOP fb selftest PASS", "pixel write/read-back selftest"),
    ]
    for needle, what in checks:
        if needle not in data:
            print("FAIL: missing %s (%r)" % (what, needle))
            return 1
    for bad in (b"GOP fb selftest FAIL", b"GOP fb refused",
                b"GOP selftest mismatch", b"PANIC", b"WuBuOS PF:"):
        if bad in data:
            print("FAIL: error marker present: %r" % bad)
            return 1

    # The kernel's mapped fb phys must be the SAME address the loader
    # captured from GOP -- not some other surface.
    mk = re.search(rb"display LIVE via GOP \((\d+)x(\d+)@32, "
                   rb"fb phys=([0-9a-fA-F]+) pitch=(\d+), (\d+) pages mapped\)",
                   data)
    ml = re.search(rb"GOP fb handoff: (\d+)x(\d+) pitch=(\d+) "
                   rb"phys=([0-9a-fA-F]+)", data)
    mp = re.search(rb"limine fb response patched \((\d+) requests", data)
    if not mk or not ml or not mp:
        print("FAIL: GOP markers malformed (cannot cross-verify)")
        return 1
    if int(mp.group(1)) < 1:
        print("FAIL: loader patched 0 limine fb requests")
        return 1
    kw, kh, kphys, kpitch, kpages = mk.groups()
    lw, lh, lpitch, lphys = ml.groups()
    if (kw, kh, kpitch) != (lw, lh, lpitch) or \
            int(kphys, 16) != int(lphys, 16):
        print("FAIL: kernel GOP fb (%sx%s phys=%s pitch=%s) != loader "
              "handoff (%sx%s phys=%s pitch=%s)" %
              (kw, kh, kphys, kpitch, lw, lh, lphys, lpitch))
        return 1
    print("GOP mapping verified: %sx%s phys=0x%s pitch=%s pages=%s "
          "(loader handoff agrees)" %
          (kw.decode(), kh.decode(), kphys.decode(), kpitch.decode(),
           kpages.decode()))
    print("gate (b) PASS: forced-GOP display LIVE + pixel read-back")

    # ---- (c) kernel-strict: 0 errors ----------------------------------
    print("=== gate (c): make kernel-strict ===", flush=True)
    rc, out = run(["make", "kernel-strict"])
    if rc != 0:
        print("FAIL: kernel-strict errored\n" + tail(out))
        return 1
    print("gate (c) PASS: kernel-strict 0 errors")

    # ---- (a) default boot unchanged ------------------------------------
    print("=== gate (a): default boot unchanged ===", flush=True)
    rc, out = run(["make", "-B", "kernel", "-j24"])
    if rc != 0:
        print("FAIL: default kernel rebuild failed\n" + tail(out))
        return 2
    # The default Bochs LFB path is ALSO honest device MMIO (each pixel
    # write is an uncached device exit under TCG). On a throttled runner
    # bonzi's 5 Hz full-frame redraw misses its heartbeat cadence even
    # though the loop is live -- CI run 44580241915 failed on that one
    # marker while "Bonzi Buddy loop active" and PROMOTED spans were
    # present. Use the SAME promotion liveness oracle as gate (b);
    # run-agi.sh still requires "Bonzi Buddy loop active" (one full MMIO
    # frame rendered) and this gate still asserts the Bochs display.
    rc, out = run(["bash", "run-agi.sh"], cwd=FW, timeout=agi_window() + 180,
                  env_extra={"WUBU_AGI_LIVENESS": "promoted"})
    if rc != 0:
        for line in out.splitlines():
            if "MISSING" in line or "RESULT" in line or "ERROR MARKERS" in line:
                print(line)
        print("FAIL: default WuBuFW measured chain regressed\n" + tail(out))
        return 1
    data = open(LOG, "rb").read()
    if b"display LIVE via GOP" in data:
        print("FAIL: default boot took the GOP path (Bochs must win)")
        return 1
    if b"bare-metal display LIVE" not in data:
        print("FAIL: default WuBuFW boot lost the Bochs LFB display")
        return 1
    rc, out = run([sys.executable, "tools/ci_boot_assert.py",
                   "src/kernel/kernel.elf", "--kernel"],
                  timeout=boot_window() + 90)
    print(tail(out, 6))
    if rc != 0:
        print("FAIL: default ci_boot_assert regressed")
        return 1
    print("gate (a) PASS: default boot unchanged (Bochs LFB + console)")

    print("RESULT: PASS -- L4 GOP fb fallback gates (b)+(c)+(a) all green")
    return 0


if __name__ == "__main__":
    sys.exit(main())