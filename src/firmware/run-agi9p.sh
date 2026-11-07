#!/bin/bash
# run-agi9p.sh -- A3 (docs/GAP_ROADMAP.md, GAP A3): the BOOTED-IMAGE 9P proof.
#
#   kernel (COM2, 0x2F8, framed 9P) --> COM2 unix socket --> host
#     agi-9p-server (styxfs over /tmp/agi9p.rund/apps) --> serves
#      /apps/agi-demo.bin (a self-contained PE returning 55).
#
# The WuBuOS AGI kernel fetches that module over 9P at boot and runs it
# through the loader (wubu_zx_run), then prints on COM1:
#   A3: RESULT: PASS -- 9P fetched /apps/agi-demo.bin (...) and ran -> entry=N
#
# COM1 = the boot log (stdio). COM2 = the 9P channel (unix socket; QEMU is the
# socket CLIENT, agi-9p-server is the listener -- must start first).
set -euo pipefail
cd "$(dirname "$0")"
ROOT=../..
CC=${CC:-gcc}

RUN=/tmp/agi9p  # rundir
rm -rf "$RUN"; mkdir -p "$RUN"

echo "=== 1/6 kernel.elf ==="
make -C "$ROOT" kernel
echo "=== 2/6 firmware ==="
./build.sh
echo "=== 3/6 tools + chainloader ==="
mkdir -p build
$CC -std=c11 -Wall -Wextra -O2 tools/mkpe.c  -o build/mkpe
$CC -std=c11 -Wall -Wextra -O2 tools/mkesp.c -o build/mkesp
$CC -std=c11 -Wall -Wextra -O2 "$ROOT/tools/mkmod.c" -o build/mkmod
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
./build/mkpe build/loader.bin build/BOOTX64.EFI 3FFF000 "$OFF"

echo "=== 4/6 ESP image ==="
cp "$ROOT/src/kernel/kernel.elf" build/KERNEL.ELF
./build/mkesp build/agi-esp.img 64 \
    build/BOOTX64.EFI  '\EFI\BOOT\BOOTX64.EFI' \
    build/KERNEL.ELF   '\EFI\BOOT\KERNEL.ELF'

echo "=== 5/6 host 9P server + demo module ==="
$CC -O0 -g -std=c11 -D_POSIX_C_SOURCE=200809L \
    -include "$ROOT/src/runtime/wubu_gnu_compat.h" -Wno-format-truncation \
    -I "$ROOT/src/runtime" -I "$ROOT/src/kernel" \
    "$ROOT/tools/agi-9p-server.c" \
    "$ROOT/src/runtime/styx_names.c" "$ROOT/src/runtime/styx_enc.c" \
    "$ROOT/src/runtime/styx_serve.c" "$ROOT/src/runtime/styx_parse.c" \
    "$ROOT/src/runtime/styx_fid.c" "$ROOT/src/runtime/styxfs_vfs.c" \
    "$ROOT/src/runtime/styxfs_callbacks.c" "$ROOT/src/runtime/styxfs_posix.c" \
    "$ROOT/src/runtime/styxfs_path.c" "$ROOT/src/runtime/styxfs_host.o" \
    "$ROOT/src/runtime/styxfs_util.c" "$ROOT/src/runtime/wubu_container.c" \
    "$ROOT/src/runtime/wubu_spawn.c" -o build/agi-9p-server
mkdir -p "$RUN/n9root/apps"
./build/mkmod "$RUN/n9root/apps/agi-demo.bin" 55
echo "demo module: $(stat -c%s "$RUN/n9root/apps/agi-demo.bin") bytes"
rm -f "$RUN/agi9p.sock"
# agi-9p-server is the LISTENER; qemu connects to it as the COM2 client.
./build/agi-9p-server "$RUN/n9root" "$RUN/agi9p.sock" \
    > "$RUN/server.log" 2>&1 &
SRV=$!
sleep 0.5

echo "=== 6/6 boot + 9P proof ==="
set +e
python3 -c "
import subprocess,time,sys,os
p=subprocess.Popen(['qemu-system-x86_64',
    '-machine','q35','-bios','wubufw.fd',
    '-drive','file=build/agi-esp.img,format=raw,if=none,id=esp',
    '-device','ahci,id=ahci','-device','ide-hd,drive=esp,bus=ahci.0',
    '-m','512',
    '-serial','stdio',                       # COM1 = boot log
    '-serial','unix:$RUN/agi9p.sock',        # COM2 = 9P (client -> server)
    '-display','none','-no-reboot'],
    stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,bufsize=0)
time.sleep(1.5)
for c in b'exit\\n':
    p.stdin.write(bytes([c])); p.stdin.flush()
    time.sleep(0.05)
time.sleep(16)
p.terminate()
sys.stdout.buffer.write(p.stdout.read())
" 2>&1 | tee build/agi9p-boot.log
set -e

echo
PASS=1
grep -q "A3: RESULT: PASS" build/agi9p-boot.log || { echo "MISSING: A3 RESULT PASS"; PASS=0; }
grep -q "A3: 9P proof starting"    build/agi9p-boot.log || { echo "MISSING: A3 start"; PASS=0; }
grep -q "entry=55"                 build/agi9p-boot.log || { echo "MISSING: module result 55"; PASS=0; }
grep -q "AGI kernel booted"        build/agi9p-boot.log || { echo "MISSING: AGI kernel booted"; PASS=0; }
if grep -q "PANIC\|TRIPLE\|TRIPLE FAULT" build/agi9p-boot.log; then echo "ERROR MARKERS"; PASS=0; fi
kill $SRV 2>/dev/null
echo "-- server log --"; cat "$RUN/server.log"

if [ "$PASS" -eq 1 ]; then
    echo "RESULT: PASS -- booted kernel fetched /n/apps/agi-demo.bin over 9P (COM2) and ran it -> 55"
else
    echo "RESULT: FAIL"
    grep -a -E "A3:|AGI|PANIC" build/agi9p-boot.log | tail -20
    exit 1
fi
