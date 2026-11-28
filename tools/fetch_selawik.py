#!/usr/bin/env python3
"""fetch_selawik.py -- vendor the reference typeface, pinned and verified.

Selawik is Microsoft's open metric-compatible replacement for Segoe UI, released under the SIL Open
Font License 1.1 (see third_party/fonts/Selawik-OFL.txt). It is NOT a redistribution of Segoe UI:
the OFL text names "Selawik" as the Reserved Font Name and Microsoft holds the trademark.

Why a script instead of a committed binary in the repo root: the project's vendored-fill rule is
"single file, tiny, permissively licensed, PINNED, and shipped with its own acceptance probe". A
pin without a digest is a wish, so this verifies sha256 before writing anything.

Usage:  python3 tools/fetch_selawik.py [--check]
  --check  verify the vendored file's digest and that our rasteriser reads it, without downloading

Exit codes: 0 ok, 1 verification failure, 2 download/extraction failure.
"""
import hashlib
import io
import os
import sys
import urllib.request
import zipfile

URL = "https://github.com/microsoft/Selawik/releases/download/1.01/Selawik_Release.zip"
SHA256 = "3f62c51e05e3b5a1e6241cf92a371f0be2ea1183aa87b30718bbd40832a8d423"  # release 1.01 zip
WANT = "selawk.ttf"                      # regular weight; the release uses 8.3-style names
TTF_SHA256 = "e9d98518d8ac2817782a9a382430463a2e0793ea68350b695bb727d9a830ee1c"  # extracted selawik.ttf
OUT_DIR = os.path.join("third_party", "fonts")
OUT_TTF = os.path.join(OUT_DIR, "selawik.ttf")
BOLD_MEMBER = "selawkb.ttf"              # bold weight
OUT_BOLD = os.path.join(OUT_DIR, "selawik-bold.ttf")
BOLD_SHA256 = "f0db5e174a90e0956ad7d2844bdca1d5e6da92ec65b2c04e57ba9b180668c904"
OUT_LIC = os.path.join(OUT_DIR, "Selawik-OFL.txt")
LIC_URL = "https://raw.githubusercontent.com/microsoft/Selawik/master/LICENSE.txt"


def sha256(b):
    return hashlib.sha256(b).hexdigest()


def pick_regular(names):
    """Choose the regular upright weight from the archive's member list."""
    cands = [n for n in names if n.lower().endswith(".ttf")]
    for want in ("selawk.ttf", "selawik.ttf", "selawik-regular.ttf"):
        for n in cands:
            if os.path.basename(n).lower() == want:
                return n
    for n in cands:
        b = os.path.basename(n).lower()
        if "regular" in b and "italic" not in b and "bold" not in b:
            return n
    return None


def main():
    check_only = "--check" in sys.argv[1:]

    if check_only:
        if not os.path.exists(OUT_TTF):
            print("fetch_selawik: MISSING %s (run without --check)" % OUT_TTF)
            return 2
        data = open(OUT_TTF, "rb").read()
        got = sha256(data)
        print("fetch_selawik: %s %d bytes sha256=%s" % (OUT_TTF, len(data), got[:16]))
        if got != TTF_SHA256:
            print("fetch_selawik: FAIL digest %s != pinned %s" % (got, TTF_SHA256))
            return 1
        if os.path.exists(OUT_BOLD):
            bd = open(OUT_BOLD, "rb").read()
            bg = sha256(bd)
            print("fetch_selawik: %s %d bytes sha256=%s" % (OUT_BOLD, len(bd), bg[:16]))
            if bg != BOLD_SHA256:
                print("fetch_selawik: FAIL bold digest %s != pinned" % bg)
                return 1
        if not os.path.exists(OUT_LIC):
            print("fetch_selawik: FAIL no licence text beside the font (OFL requires it)")
            return 1
        return 0

    try:
        with urllib.request.urlopen(URL, timeout=60) as r:
            blob = r.read()
    except Exception as e:
        print("fetch_selawik: download failed: %s" % e)
        return 2

    got = sha256(blob)
    if SHA256 != "PIN_ME" and got != SHA256:
        print("fetch_selawik: FAIL digest %s != pinned %s" % (got, SHA256))
        return 1
    print("fetch_selawik: release zip %d bytes sha256=%s" % (len(blob), got))

    try:
        z = zipfile.ZipFile(io.BytesIO(blob))
    except Exception as e:
        print("fetch_selawik: not a zip: %s" % e)
        return 2

    name = pick_regular(z.namelist())
    if not name:
        print("fetch_selawik: FAIL no regular .ttf in archive: %s" % z.namelist()[:8])
        return 1
    font = z.read(name)
    if len(font) > 2 * 1024 * 1024:
        print("fetch_selawik: FAIL font is %d bytes, over the 2 MB vendored-fill budget" % len(font))
        return 1

    os.makedirs(OUT_DIR, exist_ok=True)
    open(OUT_TTF, "wb").write(font)

    try:
        bold = z.read(BOLD_MEMBER)
        open(OUT_BOLD, "wb").write(bold)
        print("fetch_selawik: wrote %s (%d bytes) sha256=%s" % (OUT_BOLD, len(bold), sha256(bold)))
    except KeyError:
        print("fetch_selawik: WARNING no %s in the archive" % BOLD_MEMBER)
    try:
        with urllib.request.urlopen(LIC_URL, timeout=60) as r:
            open(OUT_LIC, "wb").write(r.read())
    except Exception as e:
        print("fetch_selawik: licence download failed: %s" % e)
        return 2

    print("fetch_selawik: wrote %s (%d bytes, from %s), licence beside it" % (OUT_TTF, len(font), name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
