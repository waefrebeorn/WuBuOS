#!/bin/bash
# run-agi.sh -- boot the REAL WuBuOS bare-metal AGI kernel under WuBuFW.
#
# Full measured-boot chain (ring 1 -> ring 2):
#   WuBuFW firmware (own UEFI, no EDK2/OVMF)
#     -> fw_agi_attest_and_boot() verifies + measures the chainloader (PCR4)
#     -> loader reads \EFI\BOOT\KERNEL.ELF, SHA-256s it, stashes the
#        attestation + kernel digest handoff in low memory
#     -> ExitBootServices -> 32-bit teardown -> crt0.S (_start)
#     -> kernel_main consumes the handoff (firmware attestation VALID)
#     -> AGI supervisor boots with the root-of-trust gate live
#
# Asserts every hop of the chain on the QEMU serial log.
set -euo pipefail
cd "$(dirname "$0")"
ROOT=../..
CC=${CC:-gcc}

echo "=== 1/5 kernel.elf ==="
make -C "$ROOT" kernel

echo "=== 2/5 firmware ==="
./build.sh

echo "=== 3/5 tools + chainloader ==="
mkdir -p build
$CC -std=c11 -Wall -Wextra -O2 tools/mkpe.c  -o build/mkpe
$CC -std=c11 -Wall -Wextra -O2 tools/mkesp.c -o build/mkesp

LCFLAGS="-std=c11 -Wall -Wextra -O2 -ffreestanding -fno-pie -fno-pic
         -fno-stack-protector -fno-builtin -mno-red-zone -mno-sse -mno-mmx
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
echo "loader entry offset = 0x$OFF"
./build/mkpe build/loader.bin build/BOOTX64.EFI 3FFF000 "$OFF"

echo "=== 4/5 ESP image ==="
cp "$ROOT/src/kernel/kernel.elf" build/KERNEL.ELF
# Authenticode-signed copy of the loader (used by the Secure Boot self-test).
python3 -c "open('build/test_cert.der','wb').write(bytes([0x30,0x1e])+bytes([0xA5]*30))"
build/mkpe -cert build/test_cert.der build/loader.bin build/SIGNED.EFI 3FFF000 "$OFF"
./build/mkesp build/agi-esp.img 64 \
    build/BOOTX64.EFI  '\EFI\BOOT\BOOTX64.EFI' \
    build/SIGNED.EFI   '\EFI\BOOT\SIGNED.EFI' \
    build/KERNEL.ELF   '\EFI\BOOT\KERNEL.ELF'

echo "=== 5/5 boot ==="
set +e
python3 -c "
import subprocess,time,sys
p=subprocess.Popen(['qemu-system-x86_64',
    '-machine','q35','-bios','wubufw.fd',
    '-drive','file=build/agi-esp.img,format=raw,if=none,id=esp',
    '-device','ahci,id=ahci','-device','ide-hd,drive=esp,bus=ahci.0',
    '-m','512','-serial','stdio','-display','none','-no-reboot'],
    stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,bufsize=0)
time.sleep(1.5)
for c in b'exit\n':
    p.stdin.write(bytes([c])); p.stdin.flush()
    time.sleep(0.05)
time.sleep(6)
p.terminate()
sys.stdout.buffer.write(p.stdout.read())
" 2>&1 | tee build/agi-boot.log
set -e

echo
PASS=1
grep -q "\[agi\] attestation table published"     build/agi-boot.log || { echo "MISSING: firmware attestation published"; PASS=0; }
grep -q "wubufw-loader.*attestation table: FOUND" build/agi-boot.log || { echo "MISSING: loader found attestation"; PASS=0; }
grep -q "wubufw-loader.*kernel sha256"            build/agi-boot.log || { echo "MISSING: loader measured kernel"; PASS=0; }
grep -q "wubufw-loader.*boot services exited"     build/agi-boot.log || { echo "MISSING: ExitBootServices + handoff"; PASS=0; }
grep -q "WuBuOS: kernel_main entered"             build/agi-boot.log || { echo "MISSING: kernel_main reached"; PASS=0; }
grep -q "firmware attestation consumed"           build/agi-boot.log || { echo "MISSING: kernel consumed attestation"; PASS=0; }
grep -q "AGI kernel booted"                       build/agi-boot.log || { echo "MISSING: AGI kernel booted"; PASS=0; }
grep -q "AGI: firmware attestation VALID"         build/agi-boot.log || { echo "MISSING: AGI attestation VALID"; PASS=0; }
grep -q "Bonzi Buddy loop active"                 build/agi-boot.log || { echo "MISSING: Bonzi Buddy loop active"; PASS=0; }
grep -q "bonzi: heartbeat"                        build/agi-boot.log || { echo "MISSING: bonzi heartbeat (loop alive)"; PASS=0; }
if grep -q "PANIC\|TRIPLE\|TRIPLE FAULT" build/agi-boot.log; then echo "ERROR MARKERS PRESENT"; PASS=0; fi

if [ "$PASS" -eq 1 ]; then
    echo "RESULT: PASS -- bare-metal AGI kernel booted via WuBuFW (measured boot chain green)"
else
    echo "RESULT: FAIL"
    tail -50 build/agi-boot.log
    exit 1
fi
