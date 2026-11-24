#!/usr/bin/env bash
# n9kv_gate.sh -- a real host 9P client mounts /n/kv off the RUNNING body.
#
#   host (this script)  <-- unix socket / COM2 -->  QEMU/WuBuOS kernel
#     tools/n9kv_client.py sends 9P2000 requests;
#     the kernel answers from wubu_9psrv when the console runs `n9serve N`,
#     serving bytes out of the live KV tensor.
#
# Topology mirrors the A3 proof: the HOST listens and QEMU's `-serial unix:`
# connects to it. The client waits for a go-file before its first request,
# because the kernel only polls COM2 while `n9serve` is running -- sending
# earlier would just overflow the UART FIFO.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$(pwd)
RUN=${TMPDIR:-/tmp}/wubu-n9kv.$$
mkdir -p "$RUN"
trap 'rm -rf "$RUN"' EXIT

echo "=== 1/3 image (kernel + firmware + loader + ESP) ==="
bash src/firmware/build-esp.sh "$RUN/n9kv-esp.img" >"$RUN/build.log" 2>&1 || {
    tail -30 "$RUN/build.log" >&2
    echo "FAIL: image build" >&2
    exit 1
}
test -s "$RUN/n9kv-esp.img" || { echo "FAIL: no ESP image" >&2; exit 1; }

echo "=== 2/3 host 9P client (listener) ==="
SOCK="$RUN/n9kv.sock"
GO="$RUN/go"
rm -f "$SOCK" "$GO"
python3 tools/n9kv_client.py --listen --go-file "$GO" --wait 120 \
    --bytes 16 --expect 0,0,0,0 "$SOCK" /kv/world/tick_0 \
    >"$RUN/client.log" 2>&1 &
CLIENT=$!

echo "=== 3/3 boot + serve ==="
python3 - "$RUN" "$ROOT" <<'PY'
import os, select, subprocess, sys, time
run, root = sys.argv[1], sys.argv[2]
esp   = os.path.join(run, "n9kv-esp.img")
sock  = os.path.join(run, "n9kv.sock")
go    = os.path.join(run, "go")
boot  = os.path.join(run, "boot.log")
fw    = os.path.join(root, "src/firmware/wubufw.fd")

t0 = time.time()
while not os.path.exists(sock) and time.time() - t0 < 30:
    time.sleep(0.1)
if not os.path.exists(sock):
    print("FAIL: client never listened on the socket", file=sys.stderr)
    sys.exit(1)

cmd = ["qemu-system-x86_64",
       "-machine", "q35", "-bios", fw,
       "-drive", "file=%s,format=raw,if=none,id=esp" % esp,
       "-device", "ahci,id=ahci", "-device", "ide-hd,drive=esp,bus=ahci.0",
       "-m", "512",
       "-serial", "stdio",                 # COM1 = boot log / console
       "-serial", "unix:%s" % sock,        # COM2 = 9P (QEMU connects to us)
       "-display", "none", "-no-reboot"]
p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, bufsize=0)
log = open(boot, "wb")
buf = b""
armed = False
deadline = time.time() + 120
try:
    while time.time() < deadline:
        r, _, _ = select.select([p.stdout], [], [], 0.5)
        if r:
            chunk = os.read(p.stdout.fileno(), 4096)
            if not chunk:
                break
            buf += chunk
            log.write(chunk)
            log.flush()
            if not armed and b"WuBuOS>" in buf:
                armed = True
                open(go, "w").close()      # release the client
                time.sleep(0.3)
                p.stdin.write(b"n9serve 64\n")
                p.stdin.flush()
            if armed and b"n9srv: served" in buf:
                break
        if p.poll() is not None:
            break
finally:
    time.sleep(3)                          # let the client finish its round trip
    try:
        p.stdin.write(b"exit\n")
        p.stdin.flush()
    except Exception:
        pass
    time.sleep(1)
    p.terminate()
    log.close()
PY

echo "--- client ---"
cat "$RUN/client.log"
echo "--- kernel ---"
grep -a "n9srv\|kvfs:" "$RUN/boot.log" | tail -5 || true
test -f "$RUN/boot.log" && cp "$RUN/boot.log" "$ROOT/build/n9kv-boot.log" 2>/dev/null || true
rm -rf /tmp/last-n9kv && mv "$RUN/boot.log" /tmp/last-n9kv-boot.log 2>/dev/null || true

wait "$CLIENT" || {
    echo "FAIL: host client did not mount /n/kv"
    grep -a "PANIC\|TRIPLE" /tmp/last-n9kv-boot.log 2>/dev/null || true
    exit 1
}
grep -q "PASS: /n/kv served off the running kernel" "$RUN/client.log" 2>/dev/null || {
    echo "FAIL: client did not report PASS"
    exit 1
}
echo "RESULT: PASS -- host mounted /n/kv off the running body over COM2"
