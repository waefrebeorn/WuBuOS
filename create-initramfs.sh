#!/bin/bash
# WuBuOS Initramfs Builder
#
# Creates a minimal initramfs with just enough userspace
# for WuBuOS hosted binary to run as PID 1.
# Pattern: Arch Linux minimal root + wubu binary → cpio.gz
#
# The initramfs provides:
#   - /wubu (the hosted binary, runs as init)
#   - /lib, /lib64 (dynamic linker + libc)
#   - /dev (console, tty, null)
#   - /tmp (writable)
#   - /proc, /sys (mounted by kernel)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DIST_DIR="${SCRIPT_DIR}/dist"
INITRD_DIR=$(mktemp -d)
trap "rm -rf $INITRD_DIR" EXIT

echo "━━━ Building WuBuOS Initramfs ━━━"

# Create directory structure
mkdir -p "$INITRD_DIR"/{dev,proc,sys,tmp,run,lib,lib64,bin,sbin,etc,boot}

# Copy the wubu binary
cp "${DIST_DIR}/bin/wubu" "$INITRD_DIR/wubu"
chmod 755 "$INITRD_DIR/wubu"

# Copy shared libraries that wubu needs
echo "[1/3] Resolving shared library dependencies..."
ldd "${DIST_DIR}/bin/wubu" | while read -r lib arrow path _; do
    if [ -f "$path" ] && [ "$arrow" = "=>" ]; then
        cp -L "$path" "$INITRD_DIR/lib/"
        echo "  + $(basename "$path")"
    fi
done

# Create essential symlinks
ln -sf ../wubu "$INITRD_DIR/sbin/init"
ln -sf ../lib/ld-linux-x86-64.so.2 "$INITRD_DIR/lib64/ld-linux-x86-64.so.2"

# Create minimal /etc
echo "wubuos" > "$INITRD_DIR/etc/hostname"
cat > "$INITRD_DIR/etc/passwd" << 'EOF'
root:x:0:0:root:/root:/bin/sh
wubu:x:1000:1000:WuBuOS:/home/wubu:/bin/sh
EOF

# Create init wrapper (mounts /proc, /sys, /dev, then exec wubu)
cat > "$INITRD_DIR/init" << 'INITEOF'
#!/bin/sh
# WuBuOS init wrapper
# Mounts virtual filesystems, then execs wubu as PID 1

mount -t proc proc /proc 2>/dev/null
mount -t sysfs sysfs /sys 2>/dev/null
mount -t devtmpfs devtmpfs /dev 2>/dev/null
mkdir -p /dev/pts /dev/shm
mount -t devpts devpts /dev/pts 2>/dev/null
mount -t tmpfs tmpfs /dev/shm 2>/dev/null
mount -t tmpfs tmpfs /tmp 2>/dev/null
mount -t tmpfs tmpfs /run 2>/dev/null

# Set hostname
hostname wubuos 2>/dev/null

# Exec WuBuOS (replaces init — Inferno emu pattern)
exec /wubu "$@"
INITEOF
chmod 755 "$INITRD_DIR/init"

# Pack into cpio.gz
echo "[2/3] Compressing initramfs..."
(cd "$INITRD_DIR" && find . | cpio -o -H newc 2>/dev/null | gzip -9) \
    > "${DIST_DIR}/boot/initramfs.img"

SIZE=$(du -sh "${DIST_DIR}/boot/initramfs.img" | cut -f1)
echo "[3/3] Done!"
echo ""
echo "━━━ Initramfs Built ━━━"
echo "  Output: dist/boot/initramfs.img ($SIZE)"
echo "  Contains: wubu binary + libc + init wrapper"
echo ""
echo "Usage with kernel:"
echo "  qemu-system-x86_64 -kernel vmlinuz -initrd dist/boot/initramfs.img -append 'init=/init'"
