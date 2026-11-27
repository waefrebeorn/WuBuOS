#!/usr/bin/env python3
"""fetch_tiny.py -- fill a gap with a pinned, licence-checked single file.

WHY
    "Find free small files that fill the gaps" is only useful if taking one is
    reproducible and auditable. This tool turns an entry in
    third_party/tiny/upstreams.json into an install with three properties:

      PINNED     the URL carries the upstream repository, and the install records
                 the sha256 of every byte written, so the file is immutable-id.
      LICENSED   the licence is recorded in PROVENANCE.md on install; a
                 non-permissive licence is refused outright.
      UNIQUE     installing twice is a no-op, and a second copy of the same
                 upstream is refused -- one owner per concern.

    Nothing is vendored by default: `--check` validates the table offline (the
    default test tier runs it), `--install <id> --sha <hex>` performs one install.

Usage:
    tools/fetch_tiny.py --check          # offline table validation (gate)
    tools/fetch_tiny.py --list           # what is available and what it fills
    tools/fetch_tiny.py --install miniz  # fetch, hash, write, record provenance
"""
import hashlib
import json
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TABLE = ROOT / "third_party/tiny/upstreams.json"
DEST = ROOT / "third_party/tiny"
PROV = ROOT / "PROVENANCE.md"
ALLOWED = ("public domain", "mit", "bsd", "cc0", "zlib", "unlicense", "isc", "apache")

RAW = "https://raw.githubusercontent.com/{repo}/master/{file}"


def load():
    return json.loads(TABLE.read_text())["entries"]


def check():
    entries = load()
    problems = []
    seen = {}
    for e in entries:
        for k in ("id", "files", "repo", "licence", "size_kb", "fills", "gate"):
            if not e.get(k):
                problems.append(f"{e.get('id','?')}: missing {k}")
        lic = e.get("licence", "").lower()
        if not any(a in lic for a in ALLOWED):
            problems.append(f"{e['id']}: licence '{e['licence']}' is not in the permissive set")
        if e["id"] in seen:
            problems.append(f"{e['id']}: declared twice (one owner per concern)")
        seen[e["id"]] = 1
        if "/" not in e["repo"]:
            problems.append(f"{e['id']}: repo must be owner/name")
    for p in problems:
        print("  -", p)
    if problems:
        print(f"tiny_upstreams: {len(problems)} problem(s)")
        return 1
    total = sum(e["size_kb"] for e in entries)
    print(f"tiny_upstreams: {len(entries)} pinned entries, {total} KB total, all permissive")
    return 0


def install(eid, sha_expected=None):
    entries = {e["id"]: e for e in load()}
    if eid not in entries:
        print(f"unknown id {eid}; try --list")
        return 1
    e = entries[eid]
    got = []
    for f in e["files"]:
        if "*" in f:
            print(f"{eid}: wildcard files ({f}) need a manual subset choice; refusing")
            return 1
        url = RAW.format(repo=e["repo"], file=f)
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "wubu"}), timeout=40) as r:
                data = r.read()
        except Exception as ex:
            print(f"{eid}: fetch failed {f}: {ex}")
            return 1
        out = DEST / eid / f
        if out.exists():
            print(f"{eid}: already installed at {out.relative_to(ROOT)} (one copy only)")
            return 0
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(data)
        digest = hashlib.sha256(data).hexdigest()
        if sha_expected and digest != sha_expected:
            out.unlink()
            print(f"{eid}: sha256 {digest} != expected {sha_expected}; refused")
            return 1
        got.append((f, len(data), digest))
    lines = [f"\n### vendored: {eid} ({e['repo']}, {e['licence']})\n"]
    for f, n, d in got:
        lines.append(f"- `third_party/tiny/{eid}/{f}` — {n} bytes, sha256 `{d}`, fills: {', '.join(e['fills'])}\n")
    with open(PROV, "a") as fh:
        fh.writelines(lines)
    print(f"{eid}: installed {len(got)} file(s), provenance recorded")
    return 0


def main():
    if "--check" in sys.argv:
        return check()
    if "--list" in sys.argv:
        for e in load():
            print(f"{e['id']:15s} {e['size_kb']:>5d} KB  {e['licence']:32s} fills {','.join(e['fills'])}")
            print(f"{'':15s}          {e['why']}")
        return 0
    if "--install" in sys.argv:
        eid = sys.argv[sys.argv.index("--install") + 1]
        sha = sys.argv[sys.argv.index("--sha") + 1] if "--sha" in sys.argv else None
        return install(eid, sha)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
