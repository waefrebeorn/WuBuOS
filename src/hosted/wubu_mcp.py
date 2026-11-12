#!/usr/bin/env python3
"""
wubu_mcp.py -- a development MCP server for the WuBuOS workspace.

It lets an agent SEE and ACT through the hosted desktop: capture the
workspace window, move/click/type into it, run commands, and read the /n
namespace. Every tool is a thin wrapper over the real thing -- the C11 input
injector (XTEST), xwd for the capture, and the OS's own files.

Protocol: MCP over stdio, newline-delimited JSON-RPC 2.0.

    initialize            -> protocolVersion, capabilities, serverInfo
    tools/list            -> the tool catalog
    tools/call            -> run one tool

Tools:
    wubu_screen           -> the workspace window as a PNG (path + size)
    wubu_move  x y        -> move the pointer (screen coordinates)
    wubu_click x y [btn]  -> click at (x, y)
    wubu_type  text       -> type a string
    wubu_key   keysym     -> press a key (Return, Escape, F4, ...)
    wubu_exec  command    -> run a host command, return stdout/stderr/rc
    wubu_ns    path       -> read a file from the OS namespace (~/.wubu/n)

Self-contained: stdlib only.
"""

import json
import os
import subprocess
import sys
import struct
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
INJECT = HERE / "wubu_inject"
HOSTED_BIN = HERE / "wubu"
NS_ROOT = Path(os.environ.get("WUBU_NS_ROOT", str(Path.home() / ".wubu" / "n")))

PROTOCOL_VERSION = "2024-11-05"

TOOLS = [
    {
        "name": "wubu_screen",
        "description": "Capture the WuBuOS workspace window to a PNG. "
                       "Returns the path and its dimensions.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "wubu_move",
        "description": "Move the pointer to screen coordinates (x, y).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "x": {"type": "integer"}, "y": {"type": "integer"},
            },
            "required": ["x", "y"],
        },
    },
    {
        "name": "wubu_click",
        "description": "Click at screen coordinates (x, y). button defaults "
                       "to 1 (left); 2 is middle, 3 is right.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "x": {"type": "integer"},
                "y": {"type": "integer"},
                "button": {"type": "integer"},
            },
            "required": ["x", "y"],
        },
    },
    {
        "name": "wubu_type",
        "description": "Type a string into the focused window.",
        "inputSchema": {
            "type": "object",
            "properties": {"text": {"type": "string"}},
            "required": ["text"],
        },
    },
    {
        "name": "wubu_key",
        "description": "Press a key by keysym name (Return, Escape, Tab, F4, ...). "
                       "Use 'key' for a single key; modifiers are separate keysyms.",
        "inputSchema": {
            "type": "object",
            "properties": {"keysym": {"type": "string"}},
            "required": ["keysym"],
        },
    },
    {
        "name": "wubu_exec",
        "description": "Run a command on the host and return stdout, stderr and "
                       "the exit code. Use it to build, read logs, or drive the OS.",
        "inputSchema": {
            "type": "object",
            "properties": {"command": {"type": "string"}},
            "required": ["command"],
        },
    },
    {
        "name": "wubu_ns",
        "description": "Read a file from the OS namespace (~/.wubu/n by default: "
                       "agent, worldspace, kv, background, ...). Returns text.",
        "inputSchema": {
            "type": "object",
            "properties": {"path": {"type": "string"}},
            "required": ["path"],
        },
    },
]


# -- helpers ---------------------------------------------------------------

def _inject(*args):
    """Run the C11 injector; returns (rc, output)."""
    try:
        p = subprocess.run([str(INJECT), *args], capture_output=True,
                           text=True, timeout=20)
        return p.returncode, (p.stdout or "") + (p.stderr or "")
    except FileNotFoundError:
        return 1, f"injector not built (expected at {INJECT})"
    except subprocess.TimeoutExpired:
        return 124, "timed out"


def _find_window():
    """The weston window id from the X11 tree, or None."""
    try:
        p = subprocess.run(["xwininfo", "-root", "-tree"], capture_output=True,
                           text=True, timeout=10,
                           env={**os.environ, "DISPLAY": os.environ.get("DISPLAY", ":0")})
        for line in p.stdout.splitlines():
            if "weston" in line.lower() and line.strip().startswith("0x"):
                return line.split()[0]
    except Exception:
        pass
    return None


def _capture():
    """Capture the workspace window to a PNG. Returns (path, w, h) or raises."""
    wid = _find_window()
    if not wid:
        raise RuntimeError("no weston window found (is `wubuos-window` running?)")
    out = Path("/tmp/wubu_screen.xwd")
    png = Path("/tmp/wubu_screen.png")
    subprocess.run(["xwd", "-id", wid, "-out", str(out)],
                   capture_output=True, timeout=20,
                   env={**os.environ, "DISPLAY": os.environ.get("DISPLAY", ":0")})
    data = out.read_bytes()
    if len(data) < 100:
        raise RuntimeError("xwd produced no data")
    h = struct.unpack(">25I", data[:100])
    hs, ncolors = h[0], h[19]
    w, hh = h[4], h[5]
    bpp, bpl = h[11], h[12]
    off = hs + ncolors * 12
    pix = data[off:off + bpl * hh]
    if len(pix) < bpl * hh:
        raise RuntimeError("truncated xwd data")
    # decode into a PNG via a minimal PPM, then PIL
    ppm = Path("/tmp/wubu_screen.ppm")
    with open(ppm, "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (w, hh))
        for y in range(hh):
            row = pix[y * bpl:(y + 1) * bpl]
            line = bytearray()
            for x in range(w):
                i = x * (bpp // 8)
                line += bytes((row[i + 2], row[i + 1], row[i]))
            f.write(bytes(line))
    from PIL import Image
    Image.open(ppm).save(png)
    return str(png), w, hh


def _read_ns(path):
    p = (NS_ROOT / path.lstrip("/")).resolve()
    # never escape the namespace
    if NS_ROOT.resolve() not in p.parents and p != NS_ROOT.resolve():
        raise ValueError(f"path escapes the namespace: {path}")
    if not p.exists():
        raise FileNotFoundError(f"{path} does not exist in the namespace")
    if p.is_dir():
        return "\n".join(sorted(x.name for x in p.iterdir()))
    return p.read_text(errors="replace")


# -- tool dispatch ---------------------------------------------------------

def call_tool(name, args):
    if name == "wubu_screen":
        path, w, hh = _capture()
        return {"content": [{"type": "text",
                             "text": f"captured {w}x{hh} -> {path}"}]}

    if name == "wubu_move":
        rc, out = _inject("move", str(args["x"]), str(args["y"]))
        return {"content": [{"type": "text", "text": out or "moved"}]}

    if name == "wubu_click":
        button = str(args.get("button", 1))
        rc, out = _inject("move", str(args["x"]), str(args["y"]))
        rc, out = _inject("click", button)
        return {"content": [{"type": "text", "text": out or f"clicked {button}"}]}

    if name == "wubu_type":
        rc, out = _inject("type", args["text"])
        return {"content": [{"type": "text", "text": out or "typed"}]}

    if name == "wubu_key":
        rc, out = _inject("key", args["keysym"])
        return {"content": [{"type": "text", "text": out or "key sent"}]}

    if name == "wubu_exec":
        p = subprocess.run(args["command"], shell=True, capture_output=True,
                           text=True, cwd=str(HERE.parent), timeout=120)
        text = f"rc={p.returncode}\n{p.stdout}{p.stderr}"
        return {"content": [{"type": "text", "text": text}]}

    if name == "wubu_ns":
        return {"content": [{"type": "text", "text": _read_ns(args["path"])}]}

    return {"content": [{"type": "text", "text": f"unknown tool: {name}"}],
            "isError": True}


# -- JSON-RPC loop ---------------------------------------------------------

def send(obj):
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError:
            continue
        method = req.get("method")
        rid = req.get("id")

        if method == "initialize":
            send({"jsonrpc": "2.0", "id": rid, "result": {
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "wubu-mcp",
                               "version": "1.0.0",
                               "description": "See and act through the WuBuOS workspace"},
            }})
        elif method == "notifications/initialized":
            pass                      # a notification: no response
        elif method == "tools/list":
            send({"jsonrpc": "2.0", "id": rid, "result": {"tools": TOOLS}})
        elif method == "tools/call":
            params = req.get("params") or {}
            name = params.get("name")
            args = params.get("arguments") or {}
            try:
                result = call_tool(name, args)
            except Exception as exc:                     # tool failure
                result = {"content": [{"type": "text", "text": str(exc)}],
                          "isError": True}
            send({"jsonrpc": "2.0", "id": rid, "result": result})
        elif method == "ping":
            send({"jsonrpc": "2.0", "id": rid, "result": {}})
        elif rid is not None:
            send({"jsonrpc": "2.0", "id": rid,
                  "error": {"code": -32601, "message": f"unknown method {method}"}})
    return 0


if __name__ == "__main__":
    sys.exit(main())
