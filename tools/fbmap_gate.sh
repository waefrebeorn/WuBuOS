#!/usr/bin/env bash
# fbmap_gate.sh -- the two things a ring-3 compositor needs from the kernel,
# verified by an EXTERNAL oracle.
#
# The kernel writes two known pixels: one through a damage-rect present from a
# packed client buffer, one through a ring-3 mapping of the scanout. The kernel
# then reports what it wrote; this gate boots the real firmware+kernel, runs the
# command, and reads the physical LFB bytes straight out of QEMU (pmemsave).
#
# Readback inside the kernel is deliberately NOT the proof: an MMIO window
# discards writes and reads back zero, which is exactly how the NIC-BAR bug hid
# for so long.
set -uo pipefail
FW=$(cd "$(dirname "$0")/../src/firmware" && pwd)
# PID-keyed so a second gate (or a manual run) cannot share the image.
ESP=${FBMAP_ESP:-/tmp/fbmap-esp-$$.img}
echo "=== building firmware + ESP ==="
bash "$FW/build-esp.sh" "$ESP" > /tmp/fbmap-build.log 2>&1 || { tail -20 /tmp/fbmap-build.log; exit 1; }

python3 - "$FW" "$ESP" <<'PY'
import os, re, select, socket, subprocess, sys, time

fw, esp = sys.argv[1], sys.argv[2]
mon = '/tmp/fbmap-mon.sock'
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
deadline = time.time() + 50
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
line = [l for l in log.splitlines() if 'fbmap: WROTE' in l]
if not line:
    print('  MISSING: no fbmap report')
    for l in log.splitlines()[-6:]:
        print('  | ' + l.strip()[:140])
    p.terminate(); sys.exit(1)
print('  ' + line[-1].strip())

m = re.search(r'phys=([0-9a-f]+) pitch=(\d+) dmg=(\d+):(\d+):([0-9a-f]+) '
              r'usr=(\d+):(\d+):([0-9a-f]+) map=(\w+) bytes=([0-9a-f]+)', line[-1])
if not m:
    print('  FAIL: could not parse the fbmap report')
    p.terminate(); sys.exit(1)
phys = int(m.group(1), 16)
pitch = int(m.group(2))
dmg = (int(m.group(3)), int(m.group(4)), int(m.group(5), 16))
usr = (int(m.group(6)), int(m.group(7)), int(m.group(8), 16))
mapped = m.group(9) == 'ok'

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.settimeout(10)
s.connect(mon)

def drain(tmo=8.0, until=b'(qemu)'):
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

def read_px(x, y):
    off = phys + y * pitch + x * 4
    f = '/tmp/fbmap-px.bin'
    if os.path.exists(f):
        os.unlink(f)
    s.sendall(f'pmemsave {off} 4 "{f}"\n'.encode())
    drain()
    if not os.path.exists(f):
        return None
    b = open(f, 'rb').read()
    if len(b) < 4:
        return None
    return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)

drain()
ok_dmg = read_px(dmg[0], dmg[1]) == dmg[2]
ok_usr = (read_px(usr[0], usr[1]) == usr[2]) if mapped else False
s.sendall(b'quit\n'); s.close(); time.sleep(1); p.terminate()

print(f'  damage present  {dmg[0]},{dmg[1]} expect {dmg[2]:#010x}: {"ok" if ok_dmg else "MISMATCH"}')
print(f'  ring-3 mapping  {usr[0]},{usr[1]} expect {usr[2]:#010x}: '
      f'{"ok" if ok_usr else ("not mapped" if not mapped else "MISMATCH")}')
ok = ok_dmg and ok_usr
print('test_fbmap:', 'PASS (client-buffer present + ring-3 scanout write both landed)'
      if ok else 'FAIL')
sys.exit(0 if ok else 1)
PY
