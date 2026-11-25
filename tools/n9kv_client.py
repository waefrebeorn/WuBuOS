#!/usr/bin/env python3
"""n9kv_client.py -- mount /n/kv off a RUNNING WuBuOS body over COM2.

The kernel side is wubu_9psrv (the 9P2000 server) driven by `n9serve N` on the
ring-0 console; the transport is the same length-framed 9P-over-serial that
wubu_n9 uses as a client:

    frame := [u32 LE length][9P2000 message]

Topology matches the A3 proof: the HOST listens on a unix socket and QEMU
(`-serial unix:<path>`) connects as the client end of the chardev. We then act
as the 9P CLIENT: send a request frame, wait for the reply frame.

Usage:
  n9kv_client.py SOCKET /kv/world/tick_0 [--bytes 16] [--expect 11,22,33,44]
  n9kv_client.py SOCKET /kv --list [--wait 20]

Exit 0 on success (bytes match / listing decoded), 1 on any failure.
"""
import argparse
import os
import socket
import struct
import sys
import time

T_VERSION, R_VERSION = 100, 101
T_ATTACH, R_ATTACH = 104, 105
R_ERROR = 107
T_WALK, R_WALK = 110, 111
T_OPEN, R_OPEN = 112, 113
T_READ, R_READ = 116, 117
T_CLUNK, R_CLUNK = 120, 121
T_STAT, R_STAT = 124, 125

QTDIR = 0x80


class NinePError(Exception):
    pass


class N9:
    def __init__(self, sockpath, wait_s=25.0, msize=8192, listen=False,
                 go_file=None):
        self.msize = msize
        self.tag = 0
        self.sock = None
        if listen:
            self._listen(sockpath, wait_s)
        else:
            self._connect(sockpath, wait_s)
        if self.sock is None:
            raise NinePError("no transport")
        if go_file:
            self._await_go(go_file, wait_s)
        self.version()

    # -- transport ---------------------------------------------------
    def _listen(self, path, wait_s):
        # QEMU's `-serial unix:PATH` connects to us, so we own the listener.
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass
        srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        srv.bind(path)
        srv.listen(1)
        srv.settimeout(wait_s)
        conn, _ = srv.accept()
        conn.settimeout(30.0)
        self.sock = conn
        srv.close()

    @staticmethod
    def _await_go(path, wait_s):
        # The kernel only polls COM2 when `n9serve` runs; sending earlier would
        # overflow the UART FIFO, so the harness drops a go-file first.
        deadline = time.time() + wait_s
        while time.time() < deadline:
            if os.path.exists(path):
                return
            time.sleep(0.1)
        raise NinePError("go-file never appeared: %s" % path)

    def _connect(self, path, wait_s):
        deadline = time.time() + wait_s
        last = None
        while time.time() < deadline:
            try:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                s.settimeout(30.0)
                s.connect(path)
                self.sock = s
                return
            except OSError as e:            # QEMU may not have connected yet
                last = e
                time.sleep(0.2)
        raise NinePError("cannot connect to %s: %s" % (path, last))

    def _recv_exact(self, n):
        sock = self.sock
        if sock is None:
            raise NinePError("no link")
        buf = b""
        while len(buf) < n:
            chunk = sock.recv(n - len(buf))
            if not chunk:
                raise NinePError("link closed after %d/%d bytes" % (len(buf), n))
            buf += chunk
        return buf

    def _send(self, msg):
        sock = self.sock
        if sock is None:
            raise NinePError("no link")
        sock.sendall(struct.pack("<I", len(msg)) + msg)

    def _recv(self):
        (n,) = struct.unpack("<I", self._recv_exact(4))
        if n > 0x40000:
            raise NinePError("reply frame too large: %d" % n)
        return self._recv_exact(n)

    def rpc(self, msg):
        self._send(msg)
        r = self._recv()
        if len(r) < 7:
            raise NinePError("short reply")
        if r[4] == R_ERROR:
            (ln,) = struct.unpack("<H", r[7:9])
            raise NinePError("Rerror: " + r[9:9 + ln].decode("utf-8", "replace"))
        return r

    def _tag(self):
        self.tag += 1
        return self.tag & 0xFFFF

    # -- messages ----------------------------------------------------
    def version(self, ver=b"9P2000"):
        tag = self._tag()
        body = struct.pack("<I", self.msize) + struct.pack("<H", len(ver)) + ver
        msg = struct.pack("<IBH", 7 + len(body), T_VERSION, tag) + body
        r = self.rpc(msg)
        if r[4] != R_VERSION:
            raise NinePError("expected Rversion, got %d" % r[4])
        (msz,) = struct.unpack("<I", r[7:11])
        (ln,) = struct.unpack("<H", r[11:13])
        return msz, r[13:13 + ln].decode()

    def attach(self, fid=1):
        tag = self._tag()
        body = struct.pack("<II", fid, 0xFFFFFFFF) + struct.pack("<H", 0) + struct.pack("<H", 0)
        msg = struct.pack("<IBH", 7 + len(body), T_ATTACH, tag) + body
        r = self.rpc(msg)
        if r[4] != R_ATTACH:
            raise NinePError("expected Rattach, got %d" % r[4])
        return r[7]                       # qid.type

    def walk(self, fid, newfid, names):
        tag = self._tag()
        body = struct.pack("<IIH", fid, newfid, len(names))
        for n in names:
            nb = n.encode()
            body += struct.pack("<H", len(nb)) + nb
        msg = struct.pack("<IBH", 7 + len(body), T_WALK, tag) + body
        r = self.rpc(msg)
        if r[4] != R_WALK:
            raise NinePError("expected Rwalk, got %d" % r[4])
        (nq,) = struct.unpack("<H", r[7:9])
        if nq < len(names):
            raise NinePError("partial walk %d/%d" % (nq, len(names)))
        return nq

    def open(self, fid, mode=0):
        tag = self._tag()
        body = struct.pack("<IB", fid, mode)
        msg = struct.pack("<IBH", 7 + len(body), T_OPEN, tag) + body
        r = self.rpc(msg)
        if r[4] != R_OPEN:
            raise NinePError("expected Ropen, got %d" % r[4])
        return r[7]                       # qid.type

    def read(self, fid, offset, count):
        tag = self._tag()
        body = struct.pack("<IQI", fid, offset, count)
        msg = struct.pack("<IBH", 7 + len(body), T_READ, tag) + body
        r = self.rpc(msg)
        if r[4] != R_READ:
            raise NinePError("expected Rread, got %d" % r[4])
        (n,) = struct.unpack("<I", r[7:11])
        return r[11:11 + n]

    def clunk(self, fid):
        tag = self._tag()
        body = struct.pack("<I", fid)
        msg = struct.pack("<IBH", 7 + len(body), T_CLUNK, tag) + body
        r = self.rpc(msg)
        if r[4] != R_CLUNK:
            raise NinePError("expected Rclunk, got %d" % r[4])

    def stat(self, fid):
        tag = self._tag()
        body = struct.pack("<I", fid)
        msg = struct.pack("<IBH", 7 + len(body), T_STAT, tag) + body
        r = self.rpc(msg)
        if r[4] != R_STAT:
            raise NinePError("expected Rstat, got %d" % r[4])
        return r[7:]


def parse_dirents(blob):
    """9P2000 stat stream -> [(name, is_dir, length)] (walks the size fields)."""
    out, at = [], 0
    while at + 2 <= len(blob):
        (sz,) = struct.unpack("<H", blob[at:at + 2])
        if sz == 0 or at + 2 + sz > len(blob):
            break
        e = blob[at + 2:at + 2 + sz]
        qtype = e[6]
        (length,) = struct.unpack("<Q", e[31:39])
        (nl,) = struct.unpack("<H", e[39:41])
        name = e[41:41 + nl].decode("utf-8", "replace")
        out.append((name, bool(qtype & QTDIR), length))
        at += 2 + sz
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("socket")
    ap.add_argument("path")
    ap.add_argument("--bytes", type=int, default=16)
    ap.add_argument("--expect", default=None,
                    help="comma-separated floats the read must match")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--wait", type=float, default=25.0)
    ap.add_argument("--listen", action="store_true",
                    help="own the unix socket (QEMU connects to us)")
    ap.add_argument("--go-file", default=None,
                    help="wait for this file before sending the first request")
    a = ap.parse_args()

    try:
        n9 = N9(a.socket, a.wait, listen=a.listen, go_file=a.go_file)
        qtype = n9.attach(1)
        if not qtype & QTDIR:
            print("FAIL: attach did not return a directory")
            return 1
        comps = [c for c in a.path.split("/") if c]
        n9.walk(1, 2, comps)
        n9.open(2, 0)
        if a.list:
            entries = parse_dirents(n9.read(2, 0, 4096))
            n9.clunk(2)
            print("%s:" % a.path)
            for name, isdir, length in entries:
                print("  %-24s %s %d" % (name, "dir " if isdir else "file", length))
            return 0 if entries else 1
        blob = n9.read(2, 0, a.bytes)
        n9.clunk(2)
        print("read %d bytes from %s: %s" % (len(blob), a.path, blob.hex()))
        if len(blob) % 4 == 0 and blob:
            vals = struct.unpack("<%df" % (len(blob) // 4), blob)
            print("floats: " + " ".join("%g" % v for v in vals))
            if a.expect:
                want = [float(x) for x in a.expect.split(",")]
                if list(vals) != want:
                    print("FAIL: expected %s" % want)
                    return 1
        if len(blob) != a.bytes:
            print("FAIL: short read")
            return 1
        print("PASS: /n/kv served off the running kernel")
        return 0
    except NinePError as e:
        print("FAIL: %s" % e)
        return 1


if __name__ == "__main__":
    sys.exit(main())
