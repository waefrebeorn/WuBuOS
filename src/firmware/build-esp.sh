#!/usr/bin/env bash
# build-esp.sh OUT_IMG -- the ONE bootable-image build: kernel.elf + the WuBuFW
# UEFI firmware + the measured loader, packed into an ESP image.
#
# Every QEMU gate (the A3 9P proof, the /n/kv server gate) boots THIS image, so
# the build recipe lives here once instead of being copied per gate.
#
#   usage: src/firmware/build-esp.sh /abs/path/to/esp.img
set -euo pipefail
cd "$(dirname "$0")"
ROOT=../..
CC=${CC:-gcc}
OUT=${1:?usage: build-esp.sh /path/to/esp.img}

echo "=== kernel.elf ==="
make -C "$ROOT" kernel

echo "=== firmware (WuBuFW) ==="
./build.sh

echo "=== loader + ESP ==="
mkdir -p build
$CC -std=c11 -Wall -Wextra -O2 tools/mkpe.c  -o build/mkpe
$CC -std=c11 -Wall -Wextra -O2 tools/mkesp.c -o build/mkesp

# -march=x86-64: the freestanding loader pins its ISA baseline. Distro gcc
# defaults to x86-64-v3; -mno-sse blocks vectors but NOT BMI2 -- sha256's rotr
# compiled to rorx, which qemu64 #UDs, silently hanging the measured boot.
LCFLAGS="-std=c11 -Wall -Wextra -O2 -ffreestanding -fno-pie -fno-pic
         -fno-stack-protector -fno-builtin -mno-red-zone -mno-sse -mno-mmx
         -march=x86-64 -mtune=generic
         -mno-80387 -fno-asynchronous-unwind-tables -fno-unwind-tables
         -Wno-unused-parameter -Iloader"
$CC $LCFLAGS -c loader/sha256.c    -o build/lsha.o
$CC $LCFLAGS -c loader/loader.c    -o build/lloader.o
$CC $LCFLAGS -c loader/start.S     -o build/lstart.o
$CC $LCFLAGS -c loader/teardown.S  -o build/lteardown.o
$CC $LCFLAGS -nostdlib -no-pie -Wl,--build-id=none -T loader/loader.ld \
    build/lstart.o build/lloader.o build/lsha.o build/lteardown.o \
    -o build/loader.elf -lgcc
objcopy -O binary build/loader.elf build/loader.bin
ENTRY_OFF=$(readelf -sW build/loader.elf | awk '/ _start$/{print $2; exit}')
START_ADDR=$(readelf -sW build/loader.elf | awk '/ _image_start$/{print $2; exit}')
OFF=$(python3 -c "print('%x' % (0x$ENTRY_OFF - 0x$START_ADDR))")
./build/mkpe build/loader.bin build/BOOTX64.EFI 3FFF000 "$OFF"

cp "$ROOT/src/kernel/kernel.elf" build/KERNEL.ELF
./build/mkesp "$OUT" 64 \
    build/BOOTX64.EFI  '\EFI\BOOT\BOOTX64.EFI' \
    build/KERNEL.ELF   '\EFI\BOOT\KERNEL.ELF'
echo "ESP: $OUT"
