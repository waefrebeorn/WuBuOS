#!/bin/bash
# run.sh -- build firmware + payload + ESP image, then boot it under QEMU.
set -euo pipefail
cd "$(dirname "$0")"

CC=${CC:-gcc}
TIMEOUT=${TIMEOUT:-40}

echo "=== 1/4 firmware ==="
./build.sh

echo "=== 2/4 tools ==="
mkdir -p build
$CC -std=c11 -Wall -Wextra -O2 tools/mkpe.c  -o build/mkpe
$CC -std=c11 -Wall -Wextra -O2 tools/mkesp.c -o build/mkesp

echo "=== 3/4 payload ==="
PCFLAGS="-std=c11 -Wall -Wextra -O2 -ffreestanding -fno-pie -fno-pic
         -fno-stack-protector -fno-builtin -mno-red-zone -mno-sse -mno-mmx
         -mno-80387 -fno-asynchronous-unwind-tables -fno-unwind-tables
         -Wno-unused-parameter"
$CC $PCFLAGS -c payload/hello.c -o build/hello.o
$CC $PCFLAGS -c payload/start.S -o build/pstart.o
$CC $PCFLAGS -nostdlib -no-pie -Wl,--build-id=none -T payload/payload.ld \
    build/pstart.o build/hello.o -o build/payload.elf -lgcc
objcopy -O binary build/payload.elf build/payload.bin

# entry offset of _start within the flat image
ENTRY_OFF=$(readelf -sW build/payload.elf | awk '/ _start$/{print $2; exit}')
START_ADDR=$(readelf -sW build/payload.elf | awk '/ _image_start$/{print $2; exit}')
OFF=$(python3 -c "print('%x' % (0x$ENTRY_OFF - 0x$START_ADDR))")
echo "payload entry offset = 0x$OFF"

# The flat payload is linked at 0x4000000, but mkpe puts .text at RVA 0x1000,
# so ImageBase must be biased down by one section alignment for the loaded
# addresses to match the link addresses (keeps the image relocation-free).
./build/mkpe build/payload.bin build/BOOTX64.EFI 3FFF000 "$OFF"

# Authenticode-signed copy of the payload (used by the Secure Boot self-test).
python3 -c "open('build/test_cert.der','wb').write(bytes([0x30,0x1e])+bytes([0xA5]*30))"
build/mkpe -cert build/test_cert.der build/payload.bin build/SIGNED.EFI 3FFF000 "$OFF"

echo "=== 4/4 ESP image ==="
./build/mkesp build/wubu-esp.img 64 \
    build/BOOTX64.EFI  '\EFI\BOOT\BOOTX64.EFI' \
    build/SIGNED.EFI   '\EFI\BOOT\SIGNED.EFI'

# Scratch disks for the AHCI and NVMe drivers to identify.
[ -f build/scratch-ahci.img ] || dd if=/dev/zero of=build/scratch-ahci.img bs=1M count=16 status=none
[ -f build/scratch-nvme.img ] || dd if=/dev/zero of=build/scratch-nvme.img bs=1M count=16 status=none

echo "=== boot ==="
set +e
# Full hardware surface: AHCI, NVMe, xHCI, VGA framebuffer, and a TPM when
# swtpm is available. The ESP stays on IDE so the boot path is unchanged.
TPM_ARGS=()
if [ -n "${SWTPM_DIR:-}" ] && [ -S "$SWTPM_DIR/swtpm-sock" ]; then
    TPM_ARGS=(-chardev "socket,id=chrtpm,path=$SWTPM_DIR/swtpm-sock"
              -tpmdev emulator,id=tpm0,chardev=chrtpm
              -device tpm-crb,tpmdev=tpm0)
    echo "(with swtpm TPM 2.0 backend)"
fi

# Feed 'exit' to the shell after a short delay so the shell runs once
# and then auto-boots the payload (same as before, but via the shell).
python3 -c "
import subprocess,time,sys
p=subprocess.Popen(['qemu-system-x86_64',
    '-machine','q35','-bios','wubufw.fd',
    '-drive','file=build/wubu-esp.img,format=raw,if=none,id=esp',
    '-device','ahci,id=ahci','-device','ide-hd,drive=esp,bus=ahci.0',
    '-drive','file=build/scratch-ahci.img,format=raw,if=none,id=sata0',
    '-device','ide-hd,drive=sata0,bus=ahci.1',
    '-drive','file=build/scratch-nvme.img,format=raw,if=none,id=nvm0',
    '-device','nvme,serial=WUBU0001,drive=nvm0',
    '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
    '-m','512','-serial','stdio','-display','none','-no-reboot'],
    stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,bufsize=0)
time.sleep(1.5)
for c in b'exit\n':
    p.stdin.write(bytes([c])); p.stdin.flush()
    time.sleep(0.05)
time.sleep(1)
p.terminate()
sys.stdout.buffer.write(p.stdout.read())
" 2>&1 | tee build/boot.log
set -e

echo
if grep -q WUBUFW_SELFTEST_OK build/boot.log; then
    echo "RESULT: PASS -- payload ran on WuBuFW and all checks passed"
elif grep -q WUBUFW_SELFTEST_FAIL build/boot.log; then
    echo "RESULT: PARTIAL -- payload ran but some checks failed"
    exit 1
else
    echo "RESULT: FAIL -- payload did not reach its summary"
    exit 1
fi
