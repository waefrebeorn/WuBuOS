#!/bin/bash
# rsa-gate.sh -- end-to-end RSA-2048 Secure Boot enforcement gate.
#
# Proves the firmware's real asymmetric check (not the hash-of-cert model):
#   1. generate an RSA-2048 keypair (openssl),
#   2. sign the chainloader, appending a "WRSB" + signature trailer,
#   3. build the EFI_SIGNATURE_LIST holding the RSA public key,
#   4. boot: enroll the RSA key + PK, then boot -> the SIGNED loader is
#      accepted and the kernel comes up,
#   5. replace the loader with a tampered copy -> the boot is REJECTED.
#
# Reuses the same loader/ESP build as run-agi.sh.
set -euo pipefail
cd "$(dirname "$0")"
ROOT=../..
CC=${CC:-gcc}

echo "=== 1/5 build firmware + tools ==="
bash build.sh >/dev/null
$CC -std=c11 -Wall -Wextra -O2 tools/mkpe.c  -o build/mkpe
$CC -std=c11 -Wall -Wextra -O2 tools/mkesp.c -o build/mkesp

echo "=== 2/5 build loader ==="
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

echo "=== 3/5 RSA keypair + sign loader + build RSA.ESL ==="
if [ ! -f build/sb_key.pem ]; then openssl genrsa -out build/sb_key.pem 2048 2>/dev/null; fi
python3 - <<'PY'
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding
import struct

priv = serialization.load_pem_private_key(open('build/sb_key.pem','rb').read(), password=None)
pub  = priv.public_key().public_numbers()
n, e = pub.n, pub.e
img  = open('build/BOOTX64.EFI','rb').read()
sig  = priv.sign(img, padding.PKCS1v15(), hashes.SHA256())
open('build/BOOTX64RSA.EFI','wb').write(img + b'WRSB' + sig)

# EFI_SIGNATURE_LIST with SignatureType = EFI_CERT_RSA2048_GUID
RSA2048_GUID = bytes([0x14,0x74,0x71,0xa7,0x16,0xc6,0x77,0x49,0x94,0x20,0x84,0x47,0x12,0xa7,0x35,0xbf])
GLOBAL_GUID  = bytes([0x61,0xdf,0xe4,0x8b,0xca,0x93,0xd2,0x11,0xaa,0x0d,0x00,0xe0,0x98,0x03,0x2b,0x8c])
payload = n.to_bytes(256,'big') + e.to_bytes(4,'big')
sig_size = 16 + len(payload)
list_size = 28 + sig_size
esl = RSA2048_GUID + struct.pack('<III', list_size, 0, sig_size) + GLOBAL_GUID + payload
open('build/RSA.ESL','wb').write(esl)
print(f"  signed loader: {len(img)} -> {len(img)+260} bytes; RSA.ESL {list_size} bytes")
PY

echo "=== 4/5 boot: enroll RSA + PK, signed loader must be accepted ==="
cp "$ROOT/src/kernel/kernel.elf" build/KERNEL.ELF
./build/mkesp build/rsa-esp.img 64 \
    build/BOOTX64RSA.EFI '\EFI\BOOT\BOOTX64.EFI' \
    build/RSA.ESL         '\EFI\BOOT\RSA.ESL' \
    build/KERNEL.ELF      '\EFI\BOOT\KERNEL.ELF'
rm -f build/rsa-boot1.log
python3 - <<'PY'
import subprocess, time, sys
p=subprocess.Popen(['qemu-system-x86_64','-machine','q35','-bios','wubufw.fd',
    '-drive','file=build/rsa-esp.img,format=raw,if=none,id=esp',
    '-device','ahci,id=ahci','-device','ide-hd,drive=esp,bus=ahci.0',
    '-m','512','-serial','stdio','-display','none','-no-reboot'],
    stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,bufsize=0)
time.sleep(2.0)
for cmd in (b'sbenroll db \\EFI\\BOOT\\RSA.ESL\n', b'sbpk\n', b'exit\n'):
    for ch in cmd:
        p.stdin.write(bytes([ch])); p.stdin.flush(); time.sleep(0.02)
    time.sleep(0.8)
time.sleep(12)
p.terminate()
open('build/rsa-boot1.log','wb').write(p.stdout.read())
PY
grep -q "\[sb\] RSA-2048 public key enrolled" build/rsa-boot1.log || { echo "MISSING: RSA key enrolled"; tail -30 build/rsa-boot1.log; exit 1; }
grep -q "WuBuOS: kernel_main entered" build/rsa-boot1.log || { echo "MISSING: kernel booted with signed loader"; tail -30 build/rsa-boot1.log; exit 1; }
echo "  signed loader accepted; kernel booted"

echo "=== 5/5 tampered loader must be rejected ==="
cp build/BOOTX64RSA.EFI build/BOOTX64.EFI.tampered
python3 -c "
d=bytearray(open('build/BOOTX64.EFI.tampered','rb').read()); d[0x100]^=0xff; open('build/BOOTX64.EFI.tampered','wb').write(d)"
./build/mkesp build/rsa-esp2.img 64 \
    build/BOOTX64.EFI.tampered '\EFI\BOOT\BOOTX64.EFI' \
    build/RSA.ESL             '\EFI\BOOT\RSA.ESL' \
    build/KERNEL.ELF          '\EFI\BOOT\KERNEL.ELF'
rm -f build/rsa-boot2.log
python3 - <<'PY'
import subprocess, time
p=subprocess.Popen(['qemu-system-x86_64','-machine','q35','-bios','wubufw.fd',
    '-drive','file=build/rsa-esp2.img,format=raw,if=none,id=esp',
    '-device','ahci,id=ahci','-device','ide-hd,drive=esp,bus=ahci.0',
    '-m','512','-serial','stdio','-display','none','-no-reboot'],
    stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,bufsize=0)
time.sleep(2.0)
for cmd in (b'sbenroll db \\EFI\\BOOT\\RSA.ESL\n', b'sbpk\n', b'exit\n'):
    for ch in cmd:
        p.stdin.write(bytes([ch])); p.stdin.flush(); time.sleep(0.02)
    time.sleep(0.8)
time.sleep(8)
p.terminate()
open('build/rsa-boot2.log','wb').write(p.stdout.read())
PY
grep -q "RSA: signature verify failed -> reject" build/rsa-boot2.log || { echo "MISSING: tampered loader rejection"; tail -30 build/rsa-boot2.log; exit 1; }
if grep -q "WuBuOS: kernel_main entered" build/rsa-boot2.log; then
  echo "FAIL: tampered loader booted"; exit 1
fi
echo "  tampered loader rejected"
echo "RSA BOOT GATE PASS: real RSA-2048 signature enforcement (signed accepted, tampered rejected)"
