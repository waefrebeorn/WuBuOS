#!/bin/bash
# build.sh -- build WuBuFW (WuBuOS own UEFI firmware) into a 256KB flash image.
set -euo pipefail
cd "$(dirname "$0")"

CC=${CC:-gcc}
OUT=wubufw.fd

CFLAGS="-std=c11 -fcommon -Wall -Wextra -Werror -O2 -g
        -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-builtin
        -mno-red-zone -mno-sse -mno-mmx -mno-80387 -mcmodel=large
        -fno-asynchronous-unwind-tables -fno-unwind-tables
        -Wno-unused-parameter"

LDFLAGS="-nostdlib -no-pie -Wl,--build-id=none -Wl,-z,noexecstack -T fw.ld"

SRCS="fw_lib.c fw_mem.c fw_time.c fw_ata.c fw_media.c fw_pe.c fw_guid.c
      fw_con.c fw_handle.c fw_bs_mem.c fw_bs_proto.c fw_rt.c fw_fsproto.c
      fw_pci.c fw_acpi.c fw_sha256.c fw_tpm.c fw_tpmlog.c fw_drivers.c
      fw_ahci.c fw_nvme.c fw_xhci.c fw_gop.c fw_e1000.c fw_block.c fw_fwcfg.c fw_acpiload.c fw_pcires.c
      fw_secureboot.c fw_shell.c fw_agi.c
      fw_table.c fw_main.c"

mkdir -p build
OBJS=""
for s in $SRCS; do
    o="build/${s%.c}.o"
    $CC $CFLAGS -c "$s" -o "$o"
    OBJS="$OBJS $o"
done
$CC $CFLAGS -c reset.S -o build/reset.o
OBJS="build/reset.o $OBJS"

$CC $CFLAGS $LDFLAGS $OBJS -o build/wubufw.elf -lgcc

# Flash image: 256KB covering 0xFFFC0000..0xFFFFFFFF.
objcopy -O binary --gap-fill 0xFF \
        --set-section-flags .bss=alloc,load,contents \
        build/wubufw.elf build/wubufw.raw

python3 - "$OUT" <<'PY'
import sys, subprocess, struct
elf = "build/wubufw.elf"
size = 256 * 1024
base = 0xFFFC0000
img = bytearray(b'\xFF' * size)

# Extract each PT_LOAD by its physical address.
out = subprocess.check_output(["readelf", "-lW", elf]).decode()
segs = []
for line in out.splitlines():
    p = line.split()
    if len(p) >= 6 and p[0] == "LOAD":
        off  = int(p[1], 16)
        vad  = int(p[2], 16)
        pad  = int(p[3], 16)
        fsz  = int(p[4], 16)
        segs.append((off, pad, fsz))

data = open(elf, "rb").read()
for off, paddr, fsz in segs:
    if fsz == 0:
        continue
    if paddr < base or paddr + fsz > base + size:
        raise SystemExit(f"segment paddr 0x{paddr:x} size 0x{fsz:x} outside flash window")
    dst = paddr - base
    img[dst:dst+fsz] = data[off:off+fsz]
    print(f"  seg paddr=0x{paddr:08x} size=0x{fsz:x} -> flash off 0x{dst:x}")

open(sys.argv[1], "wb").write(img)
print(f"wrote {sys.argv[1]} ({size} bytes)")
PY

echo "OK: $OUT"
