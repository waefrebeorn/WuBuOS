#!/bin/bash
# WuBuOS QEMU Test — Boot the hosted binary in QEMU
#
# Pattern: Linux kernel → initramfs (wubu + libs) → Win98 GUI
# This is the Inferno emu pattern: host kernel → WuBuOS shell
#
# Usage:
#   ./qemu-test.sh              — Boot with GUI (X11 forwarded)
#   ./qemu-test.sh --headless   — Boot headless (Styx server)
#   ./qemu-test.sh --kernel /path/to/vmlinuz  — Use specific kernel

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DIST_DIR="${SCRIPT_DIR}/dist"
INITRD="${DIST_DIR}/boot/initramfs.img"

# Defaults
KERNEL=""
HEADLESS=""
SERIAL="-serial stdio"
DISPLAY="-display none"  # QEMU virtio-gpu, X11 comes from wubu itself

# Parse args
while [ $# -gt 0 ]; do
    case "$1" in
        --headless) HEADLESS="-t"; shift ;;
        --kernel) KERNEL="$2"; shift 2 ;;
        *) echo "Unknown: $1"; exit 1 ;;
    esac
done

# Find or download kernel
if [ -z "$KERNEL" ]; then
    # Check common locations
    for k in /boot/vmlinuz-linux \
             /usr/lib/modules/*/vmlinuz \
             "${DIST_DIR}/boot/vmlinuz"; do
        if [ -f "$k" ]; then
            KERNEL="$k"
            break
        fi
    done

    # Download minimal kernel if none found
    if [ -z "$KERNEL" ]; then
        echo "No kernel found. Downloading minimal test kernel..."
        echo "(This requires network access on first run)"
        KVER="6.6.87"
        KURL="https://archive.archlinux.org/packages/l/linux/linux-${KVER}.2-1-x86_64.pkg.tar.zst"
        echo "  Note: Download Arch linux kernel package yourself and extract vmlinuz"
        echo "  Alternative: pacman -S linux (if running Arch)"
        echo ""
        echo "  For quick test, run:"
        echo "    qemu-system-x86_64 -kernel /path/to/vmlinuz -initrd $INITRD -append 'init=/init console=ttyS0'"
        exit 1
    fi
fi

if [ ! -f "$INITRD" ]; then
    echo "Error: initramfs not found at $INITRD"
    echo "Run ./create-initramfs.sh first"
    exit 1
fi

echo "━━━ WuBuOS QEMU Boot ━━━"
echo "  Kernel:    $KERNEL"
echo "  Initramfs: $INITRD ($(du -sh "$INITRD" | cut -f1))"
echo "  Init:      /init → /wubu (Inferno emu pattern)"
echo ""

# Boot QEMU
# -m 256: 256MB RAM (enough for Win98 shell + containers)
# -smp 2: 2 CPUs (for container runtime)
# -vga virtio-gpu: GPU for X11 rendering
# -net user: SLIRP networking for container net access
qemu-system-x86_64 \
    -m 256 \
    -smp 2 \
    -kernel "$KERNEL" \
    -initrd "$INITRD" \
    -append "init=/init console=ttyS0 ${HEADLESS}" \
    ${DISPLAY} \
    ${SERIAL} \
    -net nic -net user \
    -no-reboot
