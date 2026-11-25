#!/usr/bin/env bash
# metal_display_gate.sh -- the metal display leg, proven OUTSIDE the kernel.
#
# One boot, two verdicts:
#   1. FBMAP   -- a 1x1 damage rect presented from a packed client buffer and a
#                 pixel written through a ring-3 mapping of the scanout both land
#                 in the physical frame (read back with pmemsave, NOT by the
#                 kernel: an MMIO window discards writes and lies on readback).
#   2. SCANOUT -- the desktop really is in the frame the display scans out:
#                 >= 90% of pixels painted, >= 64 distinct colours, on two
#                 consecutive reads (a single-buffer scanout can be sampled
#                 mid-repaint -- measured at 26% under load).
#
# Why one boot: each QEMU gate rebuilds the firmware + ESP image and boots it.
# Two gates cost 2.6x the suite (995 s vs 378 s measured); this runs both checks
# against the SAME boot and the same frame.
set -uo pipefail
FW=$(cd "$(dirname "$0")/../src/firmware" && pwd)
ESP=${METAL_ESP:-$FW/build/metal-gate-esp.img}
KELF=$FW/../../src/kernel/kernel.elf
FWF=$FW/wubufw.fd
# A stamp, not just an mtime comparison: an image can be NEWER than the kernel and
# still contain an older one (a rebuild that only touched the firmware, a copy, a
# restored file), and then the gate boots a kernel without the command it is
# testing and reports "no report from the kernel".
STAMP=$ESP.stamp
WANT="$(stat -c '%s-%Y' "$KELF" 2>/dev/null)-$(stat -c '%s-%Y' "$FWF" 2>/dev/null)"
if [ -f "$ESP" ] && [ -f "$STAMP" ] && [ "$(cat "$STAMP")" = "$WANT" ]; then
    echo "=== ESP up to date (kernel + firmware unchanged) ==="
else
    # A nested `make kernel` inside an already-parallel suite is the reason this
    # gate cost 985 s: oversubscribed builds and a serial nested make. The suite
    # builds the kernel first (the target depends on it); only the image is ours.
    echo "=== building firmware + ESP ==="
    SKIP_KERNEL_MAKE=1 bash "$FW/build-esp.sh" "$ESP" > /tmp/metal-build.log 2>&1 \
        || { tail -20 /tmp/metal-build.log; exit 1; }
    printf '%s' "$WANT" > "$STAMP"
fi

# QEMU takes an exclusive lock on its drive image. A gate that is killed (a
# timeout, a Ctrl-C, a wrapper that stops waiting) leaves QEMU running and holding
# $ESP, and every later run then dies at startup with
#   "Is another process using the image [...]?" -- which the gate reported as a
# failed SCANOUT check. Each run boots its own copy.
ESP_RUN=/tmp/metal-esp-$$-$(date +%N).img
cp -f "$ESP" "$ESP_RUN" 2>/dev/null || cp -f "$ESP" "$ESP_RUN"
trap 'rm -f "$ESP_RUN"' EXIT INT TERM

python3 - "$FW" "$ESP_RUN" <<'PY'
import os, re, select, socket, subprocess, sys, time

fw, esp = sys.argv[1], sys.argv[2]
pid = os.getpid()
mon = f'/tmp/metal-mon-{pid}.sock'
dump = f'/tmp/metal-lfb-{pid}.bin'
for p in (mon,):
    if os.path.exists(p):
        os.unlink(p)

p = subprocess.Popen(
    ['qemu-system-x86_64', '-machine', 'q35', '-bios', 'wubufw.fd',
     '-drive', f'file={esp},format=raw,if=none,id=esp',
     '-device', 'ahci,id=ahci', '-device', 'ide-hd,drive=esp,bus=ahci.0',
     '-m', '512', '-serial', 'stdio',
     '-monitor', f'unix:{mon},server,nowait',
     '-display', 'none', '-no-reboot'],
    cwd=fw, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT, bufsize=0)

buf = b''
fd = p.stdout.fileno()
state = 0
deadline = time.time() + 90
while time.time() < deadline:
    r, _, _ = select.select([fd], [], [], 0.5)
    if r:
        ch = p.stdout.read(1)
        if not ch:
            break
        buf += ch
    if state == 0 and b'WuBuFW>' in buf:
        p.stdin.write(b'exit\n'); p.stdin.flush(); buf = b''; state = 1
    elif state == 1 and b'WuBuOS>' in buf:
        p.stdin.write(b'fbmap\n'); p.stdin.flush(); state = 2
    elif state == 2 and b'fbmap: WROTE' in buf and b'\n' in buf.split(b'fbmap: WROTE')[-1]:
        break

log = buf.decode('latin-1', 'replace')
fail = 0

# ---- 1. fbmap: pixels the kernel says it wrote ------------------------------
fb_line = [l for l in log.splitlines() if 'fbmap: WROTE' in l]
fbmap_ok = False
phys = pitch = None
probes = []
if not fb_line:
    print('  FBMAP: MISSING (no report from the kernel)')
    print(f'  boot reached: {"console" if b"WuBuOS>" in buf else "firmware shell" if b"WuBuFW>" in buf else "nothing"}')
    for l in log.splitlines()[-8:]:
        if l.strip():
            print('  | ' + l.strip()[:150])
    fail += 1
else:
    print('  ' + fb_line[-1].strip())
    m = re.search(r'phys=([0-9a-f]+) pitch=(\d+) dmg=(\d+):(\d+):([0-9a-f]+) '
                  r'usr=(\d+):(\d+):([0-9a-f]+) map=(\w+) bytes=([0-9a-f]+)',
                  fb_line[-1])
    if not m:
        print('  FBMAP: FAIL (unparseable report)')
        fail += 1
    else:
        phys = int(m.group(1), 16)
        pitch = int(m.group(2))
        probes = [('damage present', int(m.group(3)), int(m.group(4)), int(m.group(5), 16)),
                  ('ring-3 mapping', int(m.group(6)), int(m.group(7)), int(m.group(8), 16))]
        mapped = m.group(9) == 'ok'
        if not mapped:
            print('  FBMAP: FAIL (scanout could not be mapped for ring 3)')
            fail += 1

if phys is not None and pitch is not None:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(60)          # a saturated host can delay the monitor for tens of seconds
    s.connect(mon)

    def drain(tmo=25.0, until=b'(qemu)'):
        out = b''
        end = time.time() + tmo
        while time.time() < end:
            r, _, _ = select.select([s], [], [], 0.3)
            if not r:
                continue
            c = s.recv(8192)
            if not c:
                break
            out += c
            if until in out:
                break
        return out

    def reconnect_monitor():
        """A monitor socket can wedge (a half-read reply, a dropped prompt) while
        the guest stays perfectly healthy. Reopening it costs a connect() and is
        the difference between 'no dump' and a frame."""
        nonlocal s
        try:
            s.close()
        except OSError:
            pass
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(60)
        s.connect(mon)
        drain()

    def read_at(off, n):
        f = f'/tmp/metal-px-{pid}.bin'
        if os.path.exists(f):
            os.unlink(f)
        try:
            s.sendall(f'pmemsave {off} {n} "{f}"\n'.encode())
        except (BrokenPipeError, OSError):
            return None          # monitor gone: QEMU died (see below)
        resp = drain()
        if os.environ.get('METAL_DEBUG_MON'):
            print('    [mon] %r' % resp[-200:])
        # pmemsave runs in QEMU's main loop, and a guest spinning in TCG can
        # delay it past the monitor prompt: the file appears LATER than the
        # prompt does. Checking the path once, immediately after drain(), read
        # "no dump: qemu rc=0, temp file absent" on every attempt under full
        # suite load while the guest was perfectly healthy. Wait for the file,
        # with a deadline, and require it to be complete.
        for _ in range(120):                      # up to ~60 s
            if os.path.exists(f) and os.path.getsize(f) >= n:
                return open(f, 'rb').read()
            time.sleep(0.5)
        if os.path.exists(f):
            return open(f, 'rb').read()            # short: caller reports the size
        print('    [mon] no file for %d bytes; last reply %r -- reconnecting' % (n, resp[-160:]))
        try:
            reconnect_monitor()
            s.sendall(f'pmemsave {off} {n} "{f}"\n'.encode())
            drain()
            for _ in range(60):
                if os.path.exists(f) and os.path.getsize(f) >= n:
                    return open(f, 'rb').read()
                time.sleep(0.5)
        except OSError:
            pass
        return None

    def qemu_startup_failed():
        """The guest never came up: say why, from QEMU's own output."""
        rc = p.poll()
        if rc is None:
            return False
        txt = buf.decode('latin-1', 'replace')
        print(f'  FAIL: qemu exited at startup (rc={rc})')
        for l in [x for x in txt.splitlines() if x.strip()][-4:]:
            print('  | ' + l.strip()[:160])
        return True

    def guest_died():
        """True when QEMU has exited; prints why, with the serial tail."""
        rc = p.poll()
        if rc is None:
            return False
        print(f'  FAIL: the guest exited (code {rc}) before the check could run')
        tail = buf.decode('latin-1', 'replace').splitlines()[-8:]
        for l in tail:
            if l.strip():
                print('  | ' + l.strip()[:150])
        return True

    # The probe pixels are TRANSIENT: the shell repaints (the buddy animates), so
    # a sample taken a second after the write can legitimately read the live
    # wallpaper instead. Re-run the command and read immediately, a few times --
    # the requirement is "the write lands and is observable", not "persists".
    def run_probes():
        before = len(buf)
        p.stdin.write(b'fbmap\n'); p.stdin.flush()
        got_report = False
        end = time.time() + 3.0
        acc = b''
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.2)
            if r:
                acc += p.stdout.read(1)
                if b'fbmap: WROTE' in acc:
                    got_report = True
                    break
        res = []
        for label, x, y, want in probes:
            b = read_at(phys + y * pitch + x * 4, 4)
            got = None if not b or len(b) < 4 else (b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24))
            res.append((label, x, y, want, got, got == want))
        return got_report, res

    drain()
    fbmap_ok = False
    best = None
    for attempt in range(6):
        reported, res = run_probes()
        best = res
        if reported and all(r[5] for r in res):
            fbmap_ok = True
            break
        time.sleep(0.2)

    for label, x, y, want, got, ok in (best or []):
        print(f'  {label:15s} {x},{y} expect {want:#010x}: '
              f'{"ok" if ok else "MISMATCH (%s)" % (f"{got:#010x}" if got is not None else "no read")}')
    print('  FBMAP:', 'PASS' if fbmap_ok else 'FAIL')
    if not fbmap_ok:
        fail += 1

    # ---- 2. scanout: is the desktop really in the frame? --------------------
    if guest_died():
        fail += 1
        s.close()
        print('test_metal_display:', f'FAIL ({fail} check(s))')
        sys.exit(1)
    w = h = 0
    sizes = re.findall(r'bare-metal display LIVE \((\d+)x(\d+)@(\d+)', log)
    if sizes:
        w, h = int(sizes[-1][0]), int(sizes[-1][1])
    if not w or not h:
        w, h = 1024, 768

    # Poll a 64 KiB window (cheap) until it is painted twice in a row, then take
    # one full frame for the final numbers.
    total = nz = colours = 0
    pct = 0.0
    best_score = 1 << 30
    window = min(65536, w * 4 * 16)

    def window_colours():
        """Distinct colours in the same 64 KiB window.

        Non-black alone accepts a flat fill: the retry once sampled a frame that
        was 100.0% non-black with 4 colours and called it the desktop. A real
        frame has hundreds."""
        w = read_at(phys, 65536)
        if not w:
            return 0
        return len({w[i:i + 3] for i in range(0, len(w) - 3, 4)})

    def window_painted():
        b = read_at(phys, window)
        if not b or len(b) < window:
            return 0.0
        zeros = 0
        for i in range(0, window, 8):
            if b[i:i + 3] == b'\x00\x00\x00':
                zeros += 1
        return 100.0 * (1.0 - zeros / (window // 8))

    started = time.time()
    stable = 0
    # 60 s, not 30: this gate runs inside a -j24 suite, and under that load a
    # QEMU boot + pmemsave can take twice as long -- the failure mode was
    # "SCANOUT: FAIL (frame not readable)" while the same build passed twice in
    # a row standalone. A deadline is a duration, so give it the real one.
    while time.time() - started < 150:
        if p.poll() is not None:
            break
        if window_painted() >= 90.0 and window_colours() >= 16:
            stable += 1
            if stable >= 2:
                break
        else:
            stable = 0
        time.sleep(0.3)

    # The window poll proves the renderer is running; give the frame a moment to
    # finish, then take the best of a few full reads (a mid-repaint sample showed
    # 97.9% against the true ~100%).
    time.sleep(0.8)
    data = None
    # Poll on a SMALL window, then take ONE full frame.
    #
    # This used to take up to five 3 MB pmemsaves to decide whether the frame was
    # finished, and then up to eight more. Under `make -j24` that is ~40 MB of
    # monitor traffic competing with 24 compilers, and a single starved command
    # left no dump at all ("qemu rc=0, temp file absent" on a healthy guest).
    # The top band is enough to tell "still black" from "painted", and it costs
    # 64 KB instead of 3 MB.
    band = min(65536, w * h * 4)
    for _ in range(60):
        probe = read_at(phys, band)
        if probe and len(probe) >= band:
            lit = sum(1 for i in range(0, len(probe), 3 * 64) if probe[i:i + 3] != b'\x00\x00\x00')
            if lit * 100 >= (len(probe) // (3 * 64)) * 40:      # top of the desktop
                break
        time.sleep(0.5)
    # A saturated host (this gate runs inside `make -j24`) can delay the QEMU
    # monitor past its socket timeout, or answer a pmemsave with a short read.
    # Both are the harness losing a race, not a broken scanout, so retry longer
    # and say WHICH one happened -- "frame not readable" alone cost an
    # investigation the first time.
    for _attempt in range(4):
        if data and len(data) >= w * h * 4:
            break
        data = read_at(phys, w * h * 4)
        if data and len(data) >= w * h * 4:
            break
        time.sleep(2)
    if not data or len(data) < w * h * 4:
        rc = p.poll()
        if data is None:
            print('  SCANOUT: FAIL (no dump: qemu rc=%s, temp file %s)'
                  % (rc, 'present' if os.path.exists('/tmp/metal-px-%d.bin' % pid) else 'absent'))
        else:
            print('  SCANOUT: FAIL (short dump: %d of %d bytes; wanted the full frame)'
                  % (len(data), w * h * 4))
        fail += 1
    else:
        seen = set()
        for i in range(0, len(data) - 3, 4):
            px = data[i:i + 3]
            if px != b'\x00\x00\x00':
                nz += 1
                if len(seen) < 8000:
                    seen.add(px)
        total = len(data) // 4
        colours = len(seen)
        pct = 100.0 * nz / max(total, 1)
        print(f'  scanout dump: {total} px, non-black {nz} ({pct:.1f}%), colours {colours}')
        ok = pct >= 90.0 and colours >= 64
        print('  SCANOUT:', 'PASS (the desktop is in the live framebuffer)' if ok
              else 'FAIL (frame empty or flat)')
        if not ok:
            fail += 1

    # A dead guest makes this the last thing that happens; an unguarded
    # sendall() raised BrokenPipe AFTER both checks printed PASS, so the
    # target "failed" and re-ran the whole boot for nothing.
    try:
        s.sendall(b'quit\n')
    except OSError:
        pass
    s.close()
    time.sleep(1)

p.terminate()
print('test_metal_display:', 'PASS' if fail == 0 else f'FAIL ({fail} check(s))')
sys.exit(0 if fail == 0 else 1)
PY
