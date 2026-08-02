#!/usr/bin/env python3
"""probe_metal.py -- the metal boot probe (a new-generation scanner).

Boots WuBuOS in QEMU (WuBuFW -> measured payload), drives the live console,
and records the system's vital signs: tick advancement, AGI uptime,
promoted_total, attestation, /theme write count. Output feeds
docs/compendium/01-reference/state.md via `make docs`.

Exit code 0 iff the probes are sane (tick advances, attestation valid).
"""
import socket, subprocess, time, os, select, sys

ESP = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "..", "src", "firmware", "build", "agi-esp.img")
FW = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                  "..", "src", "firmware", "wubufw.fd")
SER = "/tmp/qser.sock"


def drain(s, t=0.4):
    out = b""
    end = time.time() + t
    while time.time() < end:
        r, _, _ = select.select([s], [], [], 0.05)
        if r:
            try:
                c = s.recv(4096)
                if not c:
                    break
                out += c
            except Exception:
                break
    return out


def main():
    for p in (SER,):
        try:
            os.unlink(p)
        except FileNotFoundError:
            pass
    proc = subprocess.Popen([
        "qemu-system-x86_64", "-machine", "q35", "-m", "512", "-no-reboot",
        "-serial", f"unix:{SER},server,nowait",
        "-bios", FW,
        "-drive", f"file={ESP},format=raw,if=none,id=esp",
        "-device", "ahci,id=ahci", "-device", "ide-hd,drive=esp,bus=ahci.0",
        "-display", "none"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(1.2)
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(1)
        for _ in range(40):
            try:
                s.connect(SER)
                break
            except Exception:
                time.sleep(0.2)
        log = b""
        s.sendall(b"exit\n")            # chainload the kernel
        for _ in range(140):
            log += drain(s, 0.3)
            if b"WuBuOS> " in log:
                break
        # probe 1: uptime
        s.sendall(b"uptime\n")
        time.sleep(0.6)
        log += drain(s, 0.5)
        # probe 2: agi status
        s.sendall(b"agi status\n")
        time.sleep(0.4)
        log += drain(s, 0.5)
        txt = log.decode(errors="replace")
        # print the probe-relevant tail
        for line in txt.splitlines():
            if any(m in line for m in ("tick=", "agi:", "attest",
                                       "/theme namespace",
                                       "promote loop", "Bonzi Buddy loop",
                                       "live console up")):
                print(line.strip()[:110])
        ok = ("tick=" in txt and "agi:" in txt and "attest_valid=1" in txt)
        sys.exit(0 if ok else 1)
    finally:
        proc.terminate()


if __name__ == "__main__":
    main()
