#!/usr/bin/env python3
"""virtio_net_gate.py -- L5 END-TO-END gate: real bare-metal networking.

Boots the kernel under QEMU with a legacy virtio-net PCI function on a
user (slirp) netdev. A HOST UDP echo server listens on loopback port
39001 (WUBU_NET_GATE_PORT in wubu_netstack.h -- fixed because the metal
kernel has no command-line channel; slirp forwards guest UDP aimed at
10.0.2.2:<port> to host loopback). The boot-time selftest ARP-resolves
the slirp gateway, pings it (informational), then sends a 64-byte
deterministic marker over UDP and verifies the echo byte-for-byte,
printing 'NET-OK: udp round-trip <n> bytes' to serial.

The gate asserts BOTH sides: the serial marker AND that the host echo
server actually received the exact marker payload (a guest printing
NET-OK without the host seeing the datagram cannot pass).

Pattern: tools/virtio_persist_gate.py. Usage:
    python3 tools/virtio_net_gate.py [kernel.elf] [--window SECS]
Exit: 0 round-trip proven, 1 gate failure, 2 qemu missing.
"""
import os
import socket
import subprocess
import sys
import threading
import time

MARKER_OK = b"NET-OK: udp round-trip"
MARKER_FAIL = b"NET-FAIL"
GATE_PORT = 39001          # == WUBU_NET_GATE_PORT (wubu_netstack.h)
MARKER_LEN = 64
MARKER = bytes((i * 13 + 7) & 0xFF for i in range(MARKER_LEN))


def echo_server(stop, state):
    """Host UDP echo server: reflect every datagram back to its sender."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.bind(("127.0.0.1", GATE_PORT))
    except OSError as e:
        state["bind_error"] = str(e)
        return
    s.settimeout(0.5)
    while not stop.is_set():
        try:
            data, addr = s.recvfrom(2048)
        except socket.timeout:
            continue
        state["received"] += 1
        if data == MARKER:
            state["marker_seen"] = True
        s.sendto(data, addr)
    s.close()


def boot(elf, trace, expect, window):
    if os.path.exists(trace):
        os.remove(trace)
    cmd = ["qemu-system-x86_64", "-kernel", elf,
           "-netdev", "user,id=u0",
           "-device", "virtio-net-pci,netdev=u0,disable-modern=on",
           "-serial", "file:" + trace,
           "-display", "none", "-m", "128", "-no-reboot"]
    try:
        proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
    except FileNotFoundError:
        print("FAIL: qemu-system-x86_64 not on PATH")
        sys.exit(2)
    data = b""
    deadline = time.time() + window
    try:
        while time.time() < deadline:
            if proc.poll() is not None:
                break
            time.sleep(0.5)
            if os.path.exists(trace):
                data = open(trace, "rb").read()
                if expect in data or MARKER_FAIL in data:
                    break
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
    if os.path.exists(trace):
        data = open(trace, "rb").read()
    return data


def relevant(data):
    keep = []
    for line in data.splitlines():
        if (b"virtio-net" in line or b"NET-" in line or
                b"WuBuOS>" in line):
            keep.append(line.decode(errors="replace"))
    return keep


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    elf = args[0] if args else "src/kernel/kernel.elf"
    window = int(os.environ.get("WUBU_BOOT_WINDOW", "90"))
    for i, a in enumerate(sys.argv):
        if a == "--window" and i + 1 < len(sys.argv):
            window = int(sys.argv[i + 1])
    if not os.path.exists(elf):
        print("FAIL: kernel elf not found: %s (run make kernel)" % elf)
        return 1

    os.makedirs("build", exist_ok=True)
    print("== L5 virtio-net end-to-end gate ==")

    stop = threading.Event()
    state = {"received": 0, "marker_seen": False}
    srv = threading.Thread(target=echo_server, args=(stop, state),
                           daemon=True)
    srv.start()
    time.sleep(0.3)
    if "bind_error" in state:
        print("FAIL: cannot bind host echo server on 127.0.0.1:%d (%s)"
              % (GATE_PORT, state["bind_error"]))
        return 1
    print("host UDP echo server: 127.0.0.1:%d (slirp forwards "
          "guest -> 10.0.2.2:%d here)" % (GATE_PORT, GATE_PORT))

    print("-- boot: virtio-net selftest (ARP + ICMP + UDP round-trip) --")
    try:
        data = boot(elf, "build/virtio_net_boot.bin", MARKER_OK, window)
    finally:
        stop.set()
        srv.join(timeout=3)
    for line in relevant(data):
        print("  | " + line)

    if MARKER_FAIL in data:
        print("FAIL: guest reported NET-FAIL")
        return 1
    if MARKER_OK not in data:
        print("FAIL: guest never completed the UDP round-trip "
              "(%d serial bytes)" % len(data))
        return 1
    if not state["marker_seen"]:
        print("FAIL: host echo server never received the exact marker "
              "(datagrams received: %d) -- serial claim unproven"
              % state["received"])
        return 1
    if b"WuBuOS>" not in data:
        print("FAIL: boot lost the interactive console (boot regression)")
        return 1

    ping = "answered" if b"NET-PING: 10.0.2.2 answered" in data \
        else "silent (informational; UDP is the gate)"
    print("host echo server: %d datagram(s), marker verified; ICMP: %s"
          % (state["received"], ping))
    print("PASS: NET-OK -- guest UDP marker echoed by a real host server "
          "through QEMU user-netdev (virtio-net + ARP/IPv4/UDP stack)")
    return 0


if __name__ == "__main__":
    sys.exit(main())