#!/usr/bin/env python3
"""wubu_dmg.py — our own DMG (UDIF) reader.

The Halo Mac demo ships as a compressed DMG: a sparse image of
per-chunk zlib streams + the koly trailer at the end. This reader
parses the UDIF trailer + the plist, decompresses every blkx chunk,
and writes the RAW HFS+ image — then a caller (or 7zz once the raw
image exists) extracts the files. The zlib decompression is ours; the
blkx layout comes from the public UDIF spec.

Usage: wubu_dmg.py <file.dmg> <out.raw>
"""
import plistlib
import struct
import sys
import zlib

KOLY_MAGIC = b"koly"


def read_trailer(data):
    """The UDIF trailer: the last 512 bytes, magic 'koly'."""
    if len(data) < 512:
        return None
    trailer = data[-512:]
    if trailer[0:4] != KOLY_MAGIC:
        return None
    # koly layout (all big-endian):
    # 0  magic, 4 version, 8 headerSize, 12 flags, 16 runningDataForkOffset,
    # 24 dataForkOffset, 32 dataForkLength, 40 rsrcForkOffset, 48 rsrcForkLength
    # 56 segmentNumber, 60 segmentCount, 64 segmentID,
    # 68 dataChecksum(20) -> 88, 88 videoChecksum(20) -> 108,
    # 108 blkxOffset, 116 blkxCount, 120 pver, 124 plistOffset? no:
    # the plist is at blkxOffset+0x164? Standard: blkxOffset points at the
    # XML plist (the <plist> describing the partition map).
    blkx_offset = struct.unpack(">Q", trailer[108:116])[0]
    blkx_count = struct.unpack(">Q", trailer[116:124])[0]
    return blkx_offset, blkx_count


def parse_plist(data, offset):
    """The XML plist at the blkx offset: the partition map with the
    ddsk/chunk entries."""
    # find the plist start (skip the 'mish'/pmeta if any)
    start = data.find(b"<?xml", offset, offset + 0x10000)
    if start < 0:
        start = data.find(b"<plist", offset, offset + 0x10000)
        if start < 0:
            return None
    end = data.find(b"</plist>", start)
    if end < 0:
        return None
    return plistlib.loads(data[start : end + 8])


def decompress_dmg(path, out_path):
    with open(path, "rb") as f:
        data = f.read()
    t = read_trailer(data)
    if not t:
        print("no koly trailer — not a UDIF dmg")
        return 1
    blkx_offset, blkx_count = t
    pl = parse_plist(data, blkx_offset)
    if not pl:
        print("no plist at blkx offset 0x%x" % blkx_offset)
        return 1

    images = []
    for part in pl.get("resource-fork", {}).get("blkx", []):
        images.append(part)

    chunks = []
    for part in images:
        for entry in part.get("blkx", []):
            chunks.append(entry)

    print("blkx chunks: %d" % len(chunks))
    with open(out_path, "wb") as out:
        total = 0
        for i, c in enumerate(chunks):
            # each chunk: dataStart, dataLength, sectorStart, sectorNumber,
            # compressedLength, type ('zlib'), ...
            data_start = c.get("dataStart", 0)
            data_len = c.get("dataLength", 0)
            comp_len = c.get("compressedLength", 0)
            if comp_len <= 0:
                # raw (uncompressed) chunk: copy verbatim
                raw = data[data_start : data_start + data_len]
            else:
                raw = zlib.decompress(data[data_start : data_start + comp_len])
            out.write(raw)
            total += len(raw)
            if i % 20 == 0 or i == len(chunks) - 1:
                print("  chunk %3d: %8d bytes -> %8d (total %d)"
                      % (i, comp_len, len(raw), total))
    print("raw image: %d bytes -> %s" % (total, out_path))
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("usage: wubu_dmg.py <file.dmg> <out.raw>")
        sys.exit(1)
    sys.exit(decompress_dmg(sys.argv[1], sys.argv[2]))
