#!/usr/bin/env python3
"""
build_rescue_disk.py -- Assemble the unified WuBuOS boot disk with an embedded
16-bit emergency rescue layer.

Layout:
  LBA 0      : boot.bin  (WuBuOS 16-bit bootsector; CPUID/key check -> rescue)
  LBA 1..N   : kernel.elf (the 64-bit WuBuOS kernel, loaded by boot.bin)
  LBA 64     : rescue_shim.bin (512B A20-enable + FreeDOS chainloader)
  LBA 65..   : rescue FAT image (FreeDOS KERNEL.SYS/COMMAND.COM or WuBuDOS)

When boot.S detects "no long mode" or "emergency key held", it loads the shim
from RESCUE_LBA (64) and jumps; the shim enables A20 and chainloads the real
16-bit OS from the FAT image at LBA 65.

Usage:
  python3 tools/build_rescue_disk.py --out disk.img \
      --boot src/kernel/boot.bin \
      --kernel src/kernel/kernel.elf \
      --shim tools/rescue_shim.bin \
      --rescue-fs vendor/freedos/freedos_rescue.img
"""
import argparse
import os
import sys


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--boot', required=True)
    ap.add_argument('--kernel', required=True)
    ap.add_argument('--shim', required=True)
    ap.add_argument('--rescue-fs', required=True, help='FAT image (must start its FAT at LBA0 of that file)')
    ap.add_argument('--rescue-lba', type=int, default=64)
    ap.add_argument('--out', required=True)
    ap.add_argument('--total-kb', type=int, default=1440)
    args = ap.parse_args(argv)

    boot = open(args.boot, 'rb').read()
    assert len(boot) == 512 and boot[510:512] == b'\x55\xaa', "bad boot.bin"
    kernel = open(args.kernel, 'rb').read()
    shim = open(args.shim, 'rb').read()
    assert len(shim) == 512 and shim[510:512] == b'\x55\xaa', "bad shim"
    rescue_fs = open(args.rescue_fs, 'rb').read()

    total = args.total_kb * 2048  # 512-byte sectors
    img = bytearray(total)
    # boot at LBA 0
    img[0:512] = boot
    # kernel at LBA 1 (matches boot.S disk_dap start LBA = 1)
    off = 512
    img[off:off + len(kernel)] = kernel
    # shim at RESCUE_LBA
    soff = args.rescue_lba * 512
    img[soff:soff + 512] = shim
    # rescue FAT image at RESCUE_LBA+1 (its LBA0 == its FAT boot sector)
    foff = (args.rescue_lba + 1) * 512
    img[foff:foff + len(rescue_fs)] = rescue_fs

    with open(args.out, 'wb') as f:
        f.write(img)
    print(f"wrote {args.out}: {len(img)} bytes "
          f"(boot@0, kernel@1, shim@{args.rescue_lba}, rescue_fs@{args.rescue_lba+1})")
    return 0


if __name__ == '__main__':
    sys.exit(main())
