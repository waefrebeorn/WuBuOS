#!/bin/bash
# WuBuOS USB Installer Creator
#
# Creates a bootable USB drive that:
#   1. Boots via SYSLINUX
#   2. Loads Linux kernel (Arch)
#   3. Starts WuBuOS as PID 1 (init replacement)
#   4. Win98 GUI shell appears from the hosted binary
#
# Usage: sudo ./create-usb.sh /dev/sdX
#
# Pattern: Inferno OS emu → host kernel → WuBuOS shell
# The USB IS the product. One binary, any host OS.

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "Usage: sudo $0 /dev/sdX"
    echo "  Creates a bootable WuBuOS USB drive"
    exit 1
fi

DEVICE="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DIST_DIR="${SCRIPT_DIR}/dist"

# Verify device exists
if [ ! -b "$DEVICE" ]; then
    echo "Error: $DEVICE is not a block device"
    exit 1
fi

echo "⚠️  This will ERASE all data on $DEVICE"
echo "Press Ctrl+C to abort, or wait 5 seconds..."
sleep 5

# Check dependencies
for cmd in mkfs.vfat syslinux mcopy mformat; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "Error: $cmd not found. Install: pacman -S syslinux mtools"
        exit 1
    fi
done

echo "━━━ Creating WuBuOS USB ━━━"

# 1. Partition: single FAT32 partition, bootable
echo "[1/6] Partitioning $DEVICE..."
# Clear partition table
dd if=/dev/zero of="$DEVICE" bs=512 count=1 2>/dev/null
# Create single FAT32 partition with sfdisk
echo "type=c" | sfdisk "$DEVICE" 2>/dev/null

# 2. Format FAT32
echo "[2/6] Formatting FAT32..."
mkfs.vfat -F 32 -n WUBUOS "${DEVICE}1"

# 3. Install SYSLINUX
echo "[3/6] Installing SYSLINUX bootloader..."
syslinux --install "${DEVICE}1"

# 4. Write MBR
echo "[4/6] Writing MBR..."
dd if=/usr/lib/syslinux/bios/mbr.bin of="$DEVICE" bs=440 count=1 2>/dev/null

# 5. Mount and copy files (using mtools for FAT access)
echo "[5/6] Copying WuBuOS files..."

# Copy the wubu binary
mcopy -i "${DEVICE}1" "${DIST_DIR}/bin/wubu" ::wubu

# Copy namespace
mcopy -i "${DEVICE}1" "${DIST_DIR}/namespace/root.ns" ::namespace/root.ns

# Copy docs
mcopy -i "${DEVICE}1" "${DIST_DIR}/doc/README.md" ::doc/README.md

# Create SYSLINUX config
cat > /tmp/syslinux.cfg << 'SYSCFG'
DEFAULT wubu
PROMPT 0
TIMEOUT 30

LABEL wubu
    LINUX /boot/vmlinuz
    INITRD /boot/initramfs.img
    APPEND init=/wubu quiet

LABEL wubu-debug
    LINUX /boot/vmlinuz
    INITRD /boot/initramfs.img
    APPEND init=/wubu console=tty0 loglevel=7

LABEL wubu-terminal
    LINUX /boot/vmlinuz
    INITRD /boot/initramfs.img
    APPEND init=/wubu -t console=tty0
SYSCFG
mcopy -i "${DEVICE}1" /tmp/syslinux.cfg ::syslinux.cfg
rm /tmp/syslinux.cfg

# 6. Finalize
echo "[6/6] Finalizing..."
echo ""
echo "━━━ WuBuOS USB Created ━━━"
echo ""
echo "  Device: $DEVICE"
echo "  Boot:   SYSLINUX → Arch kernel → wubu (PID 1)"
echo "  Flags:  init=/wubu (Inferno emu pattern)"
echo ""
echo "Insert USB, reboot, select USB in BIOS boot menu."
echo "Win98 desktop appears when WuBuOS starts."
echo ""
echo "Troubleshooting:"
echo "  - If no boot: check BIOS boot order, disable Secure Boot"
echo "  - If black screen: try 'wubu-debug' label (press Tab at boot)"
echo "  - For headless: edit syslinux.cfg, add -t flag"
