#!/usr/bin/env python3
"""Generate src/runtime/wubu_mp3_huffman_tables.h from the vendored PDMP3 table data.

Why a generator instead of a hand-written header: the data is 2804 tree nodes, and a hand-transcribed
copy is exactly the kind of thing that is wrong in one digit and fails 400 lines later as "illegal Huff
code". Wrapping it in a script means the header can be REGENERATED and CHECKED, and `--check` is how the
suite proves nobody edited the generated file into agreement with a broken decoder.

The data is vendored, the code is ours (see the walker in wubu_mp3_huff.c): PDMP3 is public domain under
the Unlicense, so the table bytes carry no licence obligation, and the decoder that reads them is written
here rather than copied.

Usage:
    python3 tools/make_mp3_huffman_tables.py           # write the header
    python3 tools/make_mp3_huffman_tables.py --check    # fail if the header is stale or edited
"""
import hashlib
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "third_party/mp3/pdmp3_huffman.txt"
OUT = ROOT / "src/runtime/wubu_mp3_huffman_tables.h"
TABLES = 34


def load():
    text = SRC.read_text()
    rows, blob, origin = [], [], "unknown"
    sfb_long, sfb_short, slen = {}, {}, []
    for line in text.splitlines():
        if line.startswith("# Original"):
            origin = line.split("sha256:", 1)[1].strip()
        elif line.startswith("MAIN ") and not line.startswith("MAIN 34"):
            idx, off, tlen, lb = line.split()[1:]
            rows.append((idx, off, tlen, lb))
        elif line.startswith("SFBLONG "):
            r, vals = line.split()[1], line.split()[2]
            sfb_long[int(r)] = vals
        elif line.startswith("SFBSHORT "):
            r, vals = line.split()[1], line.split()[2]
            sfb_short[int(r)] = vals
        elif line.startswith("SLEN "):
            slen.append(tuple(line.split()[1:3]))
        elif line and not line.startswith("#") and not line.startswith(("MAIN", "BLOB", "SFB", "SLEN")):
            blob.extend(line.split())
    if len(rows) != TABLES:
        raise SystemExit("expected %d table rows, found %d" % (TABLES, len(rows)))
    if len(sfb_long) != 3 or len(sfb_short) != 3 or len(slen) != 16:
        raise SystemExit("band/slen sections incomplete: %d long, %d short, %d slen rows"
                         % (len(sfb_long), len(sfb_short), len(slen)))
    return rows, blob, origin, (sfb_long, sfb_short, slen)


def render():
    rows, blob, origin, bands = load()
    sfb_long, sfb_short, slen = bands
    digest = hashlib.sha256(" ".join(blob).encode()).hexdigest()
    out = [
        "/* GENERATED FILE -- do not edit. Regenerate with tools/make_mp3_huffman_tables.py",
        " *",
        " * MP3 Layer III Huffman codebook tree, vendored DATA from PDMP3",
        " * (Krister Lagerstrom / technosaurus), public domain under the Unlicense.",
        " * Original pdmp3.c sha256: " + origin,
        " * Tree-node data sha256:   " + digest,
        " *",
        " * Node format (PDMP3's documented representation, walked by wubu_mp3_huff.c):",
        " *   leaf      : high byte zero, x = (entry >> 4) & 0x0f, y = entry & 0x0f",
        " *   interior  : left link = high byte, right link = low byte, values >= 250 chain",
        " * Tables 0, 4 and 14 are unused by the standard and carry no nodes.",
        " */",
        "#ifndef WUBU_MP3_HUFFMAN_TABLES_H",
        "#define WUBU_MP3_HUFFMAN_TABLES_H",
        "",
        "#include <stdint.h>",
        "",
        "typedef struct {",
        "    uint16_t off;      /* index into wubu_mp3_huff_blob */",
        "    uint16_t treelen;  /* node count for this codebook; 0 means unused */",
        "    uint8_t  linbits;  /* escape width applied when x or y reaches 15 */",
        "} WubuMp3HuffTable;",
        "",
        "#define WUBU_MP3_HUFF_BLOB_LEN %d" % len(blob),
        "",
        "static const uint16_t wubu_mp3_huff_blob[WUBU_MP3_HUFF_BLOB_LEN] = {",
    ]
    for s in range(0, len(blob), 12):
        out.append("    " + ", ".join("0x" + v for v in blob[s:s + 12]) + ",")
    out += ["};", "", "static const WubuMp3HuffTable wubu_mp3_huff_tables[%d] = {" % TABLES]
    for idx, off, tlen, lb in rows:
        o = "0" if off == "-" else off
        out.append("    { %s, %s, %s },  /* table %s */" % (o, tlen, lb, idx))
    out += ["};", "",
            "/* MPEG-1 scalefactor band boundaries in spectral-line units (ISO 11172-3 table B.8).",
            " * Row order: 44100, 48000, 32000 Hz. MPEG-2/2.5 use ISO 13818-3 tables and are NOT here;",
            " * the granule decoder refuses those rather than applying the wrong bands. */",
            "static const uint16_t wubu_mp3_sfb_long[3][23] = {"]
    for r in range(3):
        out.append("    { " + sfb_long[r] + " },")
    out += ["};", "", "static const uint16_t wubu_mp3_sfb_short[3][14] = {"]
    for r in range(3):
        out.append("    { " + sfb_short[r] + " },")
    out += ["};", "",
            "/* scalefac_compress -> (slen1, slen2) for MPEG-1 layer III. */",
            "static const uint8_t wubu_mp3_slen[16][2] = {"]
    for a, b in slen:
        out.append("    { %s, %s }," % (a, b))
    out += ["};", "", "#endif /* WUBU_MP3_HUFFMAN_TABLES_H */", ""]
    return "\n".join(out)


def main(argv):
    text = render()
    if "--check" in argv:
        if not OUT.exists():
            print("mp3_huffman_tables: MISSING %s" % OUT.relative_to(ROOT))
            return 1
        if OUT.read_text() != text:
            print("mp3_huffman_tables: STALE -- %s does not match the vendored data" % OUT.relative_to(ROOT))
            print("  regenerate it with: python3 tools/make_mp3_huffman_tables.py")
            return 1
        print("mp3_huffman_tables: generated header matches the vendored data (%d bytes)" % len(text))
        return 0
    OUT.write_text(text)
    print("wrote %s (%d bytes)" % (OUT.relative_to(ROOT), len(text)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
