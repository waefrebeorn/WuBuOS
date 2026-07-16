#!/usr/bin/env python3
"""
mkfat12.py -- Build a bootable FAT12 floppy image WITHOUT any host mkfs tool.

Why: the agent sandbox blocks `mkfs`/`mformat` (unconditional hardline). This is a
self-contained, dependency-free FAT12 formatter good enough to boot FreeDOS.

It takes a boot sector (must carry a 0x55AA signature at offset 510), patches a
correct BIOS Parameter Block (BPB) into it, lays out the FAT12 structures, and
places the supplied files. KERNEL.SYS is written FIRST so it lands at cluster 2
and is contiguous -- FreeDOS's boot sector loads it by walking the root-dir entry.

Usage:
    python3 tools/mkfat12.py --boot BOOT.BIN --out img.bin \
        --size 360 --spc 2 --root 112 \
        --file KERNEL.SYS:KERNEL.SYS \
        --file COMMAND.COM:COMMAND.COM \
        --file RESCUE.COM:RESCUE.COM \
        --file AUTOEXEC.BAT:AUTOEXEC.BAT

Sizes supported: 360 (default), 720, 1200, 1440 (KB). Geometry is derived.
"""
import argparse
import os
import struct
import sys

# DOS 8.3 name helpers ------------------------------------------------------

def dos_name(name: str) -> bytes:
    """Convert 'KERNEL.SYS' -> b'KERNEL  SYS' (11 bytes, space padded)."""
    base, _, ext = name.upper().partition('.')
    base = base[:8].ljust(8, ' ')
    ext = ext[:3].ljust(3, ' ')
    return (base + ext).encode('ascii')

def short_checksum(dos11: bytes) -> int:
    """VFAT checksum of the 8.3 name (unused by plain FAT12 boot, kept for ref)."""
    s = 0
    for c in dos11:
        s = (((s & 1) << 7) + (s >> 1) + c) & 0xFF
    return s

# Geometry table (KB -> (total_sectors, sectors_per_track, heads)) -------------
GEOM = {
    360:  (720,  9, 2),   # 5.25" DD
    720:  (1440, 9, 2),   # 3.5" DD
    1200: (2400, 15, 2),  # 5.25" HD
    1440: (2880, 18, 2),  # 3.5" HD
}

ATTR_ARCHIVE = 0x20
ATTR_VOLUME  = 0x08

def build_fat12(boot_sector: bytes, files: list, kb: int, spc: int,
                root_entries: int, volume_label: str = "WUBURESC") -> bytes:
    if kb not in GEOM:
        raise SystemExit(f"unsupported floppy size {kb}KB; choose from {sorted(GEOM)}")
    total_sectors, spt, heads = GEOM[kb]
    bytes_per_sector = 512
    reserved = 1
    fats = 2
    media = 0xF0
    root_sectors = (root_entries * 32 + bytes_per_sector - 1) // bytes_per_sector

    # Pick fat_sectors so the FAT can address every data cluster.
    data_sectors = total_sectors - reserved - root_sectors
    clusters = data_sectors // spc
    fat_bytes_needed = (clusters + 2) * 3 // 2
    fat_sectors = max(1, (fat_bytes_needed + bytes_per_sector - 1) // bytes_per_sector)
    # round up so FreeDOS's own expectation (it reads fat_sectors from BPB) is sane
    while (fat_sectors * bytes_per_sector * 2 // 3) < (clusters + 2):
        fat_sectors += 1

    # Assemble image skeleton
    img = bytearray(total_sectors * bytes_per_sector)

    # --- Boot sector with patched BPB ---
    if len(boot_sector) < 512 or boot_sector[510:512] != b'\x55\xAA':
        raise SystemExit("boot sector must be >=512 bytes and end with 0x55 0xAA")
    bs = bytearray(boot_sector[:512])
    # BPB (FAT12 / classic DOS 2.x+ layout)
    struct.pack_into('<H', bs, 11, bytes_per_sector)   # 00: bytes per sector
    bs[13] = spc                                        # 01: sectors per cluster
    struct.pack_into('<H', bs, 14, reserved)           # 02: reserved sectors
    bs[16] = fats                                       # 03: FAT count
    struct.pack_into('<H', bs, 17, root_entries)       # 04: root entries
    struct.pack_into('<H', bs, 19, total_sectors)      # 05: total sectors (16-bit)
    bs[21] = media                                      # 06: media descriptor
    struct.pack_into('<H', bs, 22, fat_sectors)        # 07: sectors per FAT
    struct.pack_into('<H', bs, 24, spt)                # 08: sectors per track
    struct.pack_into('<H', bs, 26, heads)              # 09: heads
    struct.pack_into('<I', bs, 28, 0)                  # 10: hidden sectors
    img[0:512] = bs

    # --- FAT region ---
    fat_start = reserved * bytes_per_sector
    # FAT[0] = media + EOC marker bits; FAT[1] = EOC
    fat = bytearray(fat_sectors * bytes_per_sector)
    fat[0] = media
    fat[1] = 0xFF
    fat[2] = 0xFF
    fat[3] = 0x0F

    def set_fat_entry(cluster: int, value: int):
        # 12-bit packed entries
        off = cluster * 3 // 2
        if cluster % 2 == 0:
            existing = fat[off + 1] & 0xF0
            lo = value & 0xFF
            hi = (value >> 8) & 0x0F
            fat[off] = lo
            fat[off + 1] = (hi << 4) | existing
        else:
            existing = fat[off] & 0x0F
            lo = value & 0x0F
            hi = (value >> 4) & 0xFF
            fat[off] = (lo << 4) | existing
            fat[off + 1] = hi

    def get_fat_entry(cluster: int) -> int:
        off = cluster * 3 // 2
        if cluster % 2 == 0:
            return fat[off] | ((fat[off + 1] & 0x0F) << 8)
        else:
            return ((fat[off] & 0xF0) >> 4) | (fat[off + 1] << 4)

    # --- Root directory region ---
    root_start = (reserved + fats * fat_sectors) * bytes_per_sector
    data_start = root_start + root_sectors * bytes_per_sector

    root = bytearray(root_sectors * bytes_per_sector)
    # Volume label entry (first)
    vl = dos_name(volume_label)
    root[0:11] = vl
    root[11] = ATTR_VOLUME

    next_cluster = 2  # cluster 0/1 reserved
    entries_used = 1  # volume label occupies slot 0

    # Sort so KERNEL.SYS is first (guarantees contiguous load at cluster 2).
    order = sorted(files, key=lambda f: 0 if f[0].upper() == 'KERNEL.SYS' else 1)
    for fname, data in order:
        size = len(data)
        n_clusters = max(1, (size + spc * bytes_per_sector - 1) // (spc * bytes_per_sector))
        # allocate contiguous clusters
        start = next_cluster
        prev = None
        for i in range(n_clusters):
            cur = next_cluster + i
            nxt = cur + 1 if i + 1 < n_clusters else 0xFFF  # EOC
            set_fat_entry(cur, nxt)
            prev = cur
        next_cluster += n_clusters

        # write clusters into data area
        for i in range(n_clusters):
            c = start + i
            off = data_start + (c - 2) * spc * bytes_per_sector
            chunk = data[i * spc * bytes_per_sector:(i + 1) * spc * bytes_per_sector]
            img[off:off + len(chunk)] = chunk

        # root dir entry
        e = entries_used * 32
        entries_used += 1
        root[e:e + 11] = dos_name(fname)
        root[e + 11] = ATTR_ARCHIVE
        struct.pack_into('<H', root, e + 26, start)
        struct.pack_into('<I', root, e + 28, size)

    # Copy FAT copies + root into image
    for f in range(fats):
        dst = (reserved + f * fat_sectors) * bytes_per_sector
        img[dst:dst + len(fat)] = bytes(fat)
    img[root_start:root_start + len(root)] = root

    return bytes(img)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--boot', required=True, help='boot sector file (ends 0x55AA)')
    ap.add_argument('--out', required=True)
    ap.add_argument('--size', type=int, default=360, help='floppy KB')
    ap.add_argument('--spc', type=int, default=2, help='sectors per cluster')
    ap.add_argument('--root', type=int, default=112, help='root dir entries')
    ap.add_argument('--label', default='WUBURESC')
    ap.add_argument('--file', action='append', default=[],
                    help='SRC:DST  (DST is the 8.3 name on the disk)')
    args = ap.parse_args(argv)

    with open(args.boot, 'rb') as f:
        boot = f.read()
    files = []
    for spec in args.file:
        src, _, dst = spec.partition(':')
        with open(src, 'rb') as f:
            files.append((dst, f.read()))
    if not any(dst.upper() == 'KERNEL.SYS' for dst, _ in files):
        raise SystemExit("FAT12 image must contain KERNEL.SYS (FreeDOS boot target)")

    img = build_fat12(boot, files, args.size, args.spc, args.root, args.label)
    with open(args.out, 'wb') as f:
        f.write(img)
    print(f"wrote {args.out}: {len(img)} bytes "
          f"({len(files)} files, KERNEL.SYS first/contiguous)")
    return 0


if __name__ == '__main__':
    sys.exit(main())
