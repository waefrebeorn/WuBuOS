#!/usr/bin/env bash
# scanout_gate.sh -- has anyone SEEN the metal desktop? This gate does.
#
# Every other metal gate reads the kernel's BACK buffer or the serial log, so a
# kernel that paints a back buffer and never reaches the scanout passes them all.
# That is exactly what happened: the bochs path attached a NIC's BAR0 as the
# framebuffer and the frame buffer the display actually scans out stayed empty.
#
# This dumps the LIVE LFB out of QEMU's physical address space (pmemsave), at the
# address the kernel itself reports on the boot log ("LFB phys=..."), and asserts
# it holds a real desktop: most pixels painted, many distinct colours. It needs no
# kernel cooperation, so it cannot be fooled by the kernel's own read path.
#
# Not a screendump: under `-display none` QEMU's VGA console surface reports
# 640x480 black even when the LFB is fully painted (measured), so a screendump is
# not a valid oracle for this configuration.
set -uo pipefail
FW=$(cd "$(dirname "$0")/../src/firmware" && pwd)
# PID-keyed: two gates running at once (or a stray manual run) must not fight
# over the same ESP image and monitor socket.
ESP=${SCANOUT_ESP:-/tmp/scanout-esp-$$.img}
echo "=== building firmware + ESP ==="
bash "$FW/build-esp.sh" "$ESP" > /tmp/scanout-build.log 2>&1 || { tail -20 /tmp/scanout-build.log; exit 1; }

python3 - "$FW" "$ESP" <<'PY'
import os, re, select, socket, subprocess, sys, time

fw, esp = sys.argv[1], sys.argv[2]
mon = f'/tmp/scanout-mon-{os.getpid()}.sock'
dump = f'/tmp/scanout-lfb-{os.getpid()}.bin'
for p in (mon, dump):
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
deadline = time.time() + 45
while time.time() < deadline:
    r, _, _ = select.select([fd], [], [], 0.5)
    if r:
        ch = p.stdout.read(1)
        if not ch:
            break
        buf += ch
    if b'WuBuFW>' in buf:
        p.stdin.write(b'exit\n'); p.stdin.flush(); buf = b''
    if b'WuBuOS>' in buf:
        break

log = buf.decode('latin-1', 'replace')
m = re.findall(r'LFB phys=([0-9a-fA-F]+)', log)
if not m:
    print('  MISSING: no "LFB phys=" line in the boot log (display never attached)')
    for line in log.splitlines()[-6:]:
        print('  | ' + line.strip()[:140])
    p.terminate(); sys.exit(1)
phys = int(m[-1], 16)
sizes = re.findall(r'bare-metal display LIVE \((\d+)x(\d+)@(\d+)', log)
w, h = (int(sizes[-1][0]), int(sizes[-1][1])) if sizes else (1024, 768)
print(f'  kernel reports: LFB phys={phys:#x} {w}x{h}')

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.settimeout(10)
s.connect(mon)

def drain(until=b'(qemu)', tmo=12.0):
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

drain()
size = w * h * 4

def grab():
    if os.path.exists(dump):
        os.unlink(dump)
    s.sendall(f'pmemsave {phys} {size} "{dump}"\n'.encode())
    drain()
    if not os.path.exists(dump):
        return None
    data = open(dump, 'rb').read()
    if len(data) < size:
        return None
    nz = 0
    seen = set()
    for i in range(0, len(data) - 3, 4):
        px = data[i:i+3]
        if px != b'\x00\x00\x00':
            nz += 1
            if len(seen) < 8000:
                seen.add(px)
    return len(data) // 4, nz, len(seen)

# A single-buffer scanout can be sampled MID-FRAME, and under -j24 the guest is
# slow enough that a fixed sleep caught a 26% painted desktop (measured). Poll
# until the frame is nearly fully painted TWICE in a row, so the gate measures a
# finished frame instead of racing the renderer.
total = nz = colours = 0
pct = 0.0
for attempt in range(40):
    st = grab()
    if st:
        total, nz, colours = st
        pct = 100.0 * nz / max(total, 1)
        if pct >= 90.0:
            time.sleep(1.0)
            st2 = grab()
            if st2 and 100.0 * st2[1] / max(st2[0], 1) >= 90.0:
                total, nz, colours = st2
                pct = 100.0 * nz / max(total, 1)
                break
    time.sleep(1.0)

s.sendall(b'quit\n'); s.close(); time.sleep(1)
p.terminate()

if not total:
    print('  FAIL: pmemsave produced nothing (address not in QEMU\'s space?)')
    sys.exit(1)
print(f'  scanout dump: {total} px, non-black {nz} ({pct:.1f}%), colours {colours}')
ok = pct >= 90.0 and colours >= 64
print('test_scanout:', 'PASS (the metal desktop is in the live framebuffer)' if ok
      else 'FAIL (framebuffer empty or flat -- the frame never reached the scanout)')
sys.exit(0 if ok else 1)
PY
