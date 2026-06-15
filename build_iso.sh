#!/bin/bash
# build_iso.sh — Build WuBuOS Bare-Metal ISO Image
# Uses Limine bootloader for UEFI/BIOS booting

set -e

PROJECT_ROOT="/home/wubu/.hermes/profiles/mind-palace/home/myseed"
ISO_DIR="$PROJECT_ROOT/iso_build"
ISO_OUT="$PROJECT_ROOT/wubuos.iso"

echo "=== Building WuBuOS Bare-Metal ISO ==="
echo "Project: $PROJECT_ROOT"
echo "ISO output: $ISO_OUT"

# Clean previous build
rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR/boot"
mkdir -p "$ISO_DIR/EFI/BOOT"

# Check for Limine
if ! command -v limine &> /dev/null; then
    echo "Limine not found. Installing..."
    cd /tmp
    git clone https://github.com/limine-bootloader/limine.git --depth=1
    cd limine
    make
    sudo make install
    cd "$PROJECT_ROOT"
fi

# Build kernel
echo "Building kernel..."
cd "$PROJECT_ROOT"
make clean
make CFLAGS="-Wall -Wextra -std=c11 -O2 -g -DWUBU_NO_LIBM -DMYSEED_METAL -ffreestanding -nostdlib -nostartfiles -fno-pie -mno-red-zone -mcmodel=kernel" \
     LDFLAGS="-T src/kernel/kernel.ld -nostdlib -nostartfiles -z max-page-size=0x1000" \
     src/kernel/crt0.o src/kernel/metal_main.o src/kernel/memory.o src/kernel/tasking.o src/kernel/tasking_switch.o \
     src/kernel/vbe.o src/kernel/input.o src/kernel/interrupt.o src/kernel/isr_stubs.o src/kernel/ps2.o \
     src/kernel/fat32.o src/kernel/ahci.o src/kernel/txfs.o src/kernel/wubu_gaad.o \
     src/hosted/wubu_metal.o \
     src/jit/jit.o src/compiler/holyc_lexer.o src/compiler/holyc_parse.o src/compiler/holyc_codegen.o \
     src/gui/wm.o src/gui/taskbar.o src/gui/desktop.o src/gui/theme.o src/gui/startmenu.o src/gui/gui_dbuf.o src/gui/wubu_wm.o src/gui/wubu_theme.o \
     src/bridge/bridge.o \
     src/apps/repl.o \
     src/shell/wubu_shell.o \
     src/kernel/kernel.elf

if [ ! -f "$PROJECT_ROOT/src/kernel/kernel.elf" ]; then
    echo "ERROR: Kernel build failed"
    exit 1
fi

# Copy kernel and config
cp "$PROJECT_ROOT/src/kernel/kernel.elf" "$ISO_DIR/boot/"
cp "$PROJECT_ROOT/limine.conf" "$ISO_DIR/boot/"

# Copy Limine bootloader files
LIMINE_INSTALL="/usr/local/share/limine"  # Adjust if needed
if [ -d "$LIMINE_INSTALL" ]; then
    cp "$LIMINE_INSTALL/limine-bios.sys" "$ISO_DIR/boot/"
    cp "$LIMINE_INSTALL/limine-bios-cd.bin" "$ISO_DIR/boot/"
    cp "$LIMINE_INSTALL/limine-uefi-cd.bin" "$ISO_DIR/boot/"
    cp "$LIMINE_INSTALL/BOOTX64.EFI" "$ISO_DIR/EFI/BOOT/"
    cp "$LIMINE_INSTALL/BOOTIA32.EFI" "$ISO_DIR/EFI/BOOT/"
else
    echo "WARNING: Limine install dir not found at $LIMINE_INSTALL"
    echo "Trying /usr/share/limine..."
    LIMINE_INSTALL="/usr/share/limine"
    if [ -d "$LIMINE_INSTALL" ]; then
        cp "$LIMINE_INSTALL/limine-bios.sys" "$ISO_DIR/boot/"
        cp "$LIMINE_INSTALL/limine-bios-cd.bin" "$ISO_DIR/boot/"
        cp "$LIMINE_INSTALL/limine-uefi-cd.bin" "$ISO_DIR/boot/"
        cp "$LIMINE_INSTALL/BOOTX64.EFI" "$ISO_DIR/EFI/BOOT/"
        cp "$LIMINE_INSTALL/BOOTIA32.EFI" "$ISO_DIR/EFI/BOOT/"
    fi
fi

# Create initrd (empty for now - we'll add .wubu containers later)
cd "$ISO_DIR"
tar -cf boot/initrd.tar --files-from /dev/null

# Build ISO with xorriso
echo "Building ISO..."
xorriso -as mkisofs \
    -b boot/limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    --efi-boot boot/limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image --protective-msdos-label \
    -o "$ISO_OUT" \
    "$ISO_DIR"

# Install Limine to ISO for BIOS booting
limine bios-install "$ISO_OUT"

echo "=== ISO Build Complete ==="
echo "Output: $ISO_OUT"
echo ""
echo "To test in QEMU:"
echo "  qemu-system-x86_64 -cdrom $ISO_OUT -m 2G -serial stdio"
echo ""
echo "To write to USB:"
echo "  sudo dd if=$ISO_OUT of=/dev/sdX bs=4M status=progress"