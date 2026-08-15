#!/usr/bin/env bash
# install-gpu-stack.sh — bring up the host GPU stack for the RTX 4050
#
# POLICY COMPLIANCE (see docs/GPU-DRIVER-DISCLOSURE.md):
#   The NVIDIA kernel module (nvidia.ko) is a THIRD-PARTY PROPRIETARY
#   BINARY. It is NOT source-stolen, NOT forked, NOT modified. This script
#   installs the stock distro package and loads it into the running host
#   kernel. WuBuOS then exposes /dev/nvidia* + /dev/dxg to game containers
#   via bind-mounts in wubu_ct_start().
#
#   This is a HOST DRIVER install (the host kernel IS the WuBuOS execution
#   surface). The alternative — a from-scratch bare-metal NVIDIA driver in
#   src/kernel/ — is a multi-month reverse-engineering effort and is NOT
#   feasible to implement ad hoc.
#
# PREREQ: apt-based host (Ubuntu 24.04), RTX 4050 (sm_89, Blackwell)
# USAGE:  sudo ./tools/install-gpu-stack.sh
set -euo pipefail

echo "=== WuBuOS GPU Stack Installer (RTX 4050 / Blackwell) ==="
echo "DISCLOSURE: nvidia.ko is NVIDIA proprietary — not forked, not modified."

# --- 1. Detect the GPU ------------------------------------------------------
echo "[1/5] Detecting NVIDIA hardware..."
if ! lspci -nn | grep -i 'nvidia\|3d.*controller' | grep -q '10de:'; then
    echo "WARN: no NVIDIA PCI device found by lspci."
    echo "      If this is WSL2, /dev/dxg paravirtualized path may be used instead."
fi
if ! lspci -knn 2>/dev/null | grep -i nvidia | grep -q .; then
    echo "WARN: NVIDIA PCI device present but no kernel driver claimed it yet."
fi

# --- 2. Kernel headers (needed for module compilation if DKMS triggers) ------
echo "[2/5] Ensuring kernel headers..."
KERNEL_VER=$(uname -r)
apt-get install -y "linux-headers-${KERNEL_VER}" 2>/dev/null || \
    apt-get install -y linux-headers-generic || true

# --- 3. Install NVIDIA driver (proprietary binary package) ------------------
echo "[3/5] Installing NVIDIA driver 580 (proprietary)..."
# The distro package ships nvidia.ko built for this kernel.
apt-get update -qq
apt-get install -y nvidia-driver-580 nvidia-kernel-common-580 \
    nvidia-prime 2>/dev/null || \
    apt-get install -y nvidia-driver-580 2>/dev/null || \
    echo "WARN: nvidia-driver-580 install failed — check apt sources"

# --- 4. Install CUDA + Vulkan ICD -------------------------------------------
echo "[4/5] Installing CUDA runtime + NVIDIA Vulkan ICD..."
apt-get install -y nvidia-cuda-toolkit nvidia-utils-580 vulkan-nvidia 2>/dev/null || \
    apt-get install -y nvidia-cuda-toolkit nvidia-utils-580 2>/dev/null || \
    echo "WARN: CUDA/Vulkan ICD install failed — will rely on existing libs"

# --- 5. Load the module + verify --------------------------------------------
echo "[5/5] Loading NVIDIA module + verifying..."
modprobe nvidia 2>/dev/null || echo "WARN: modprobe nvidia failed (may be built-in or WSL path)"

if [ -c /dev/nvidia0 ] && [ -c /dev/nvidiactl ]; then
    echo "✅ /dev/nvidia0 present"
    if [ -f /proc/driver/nvidia/version ]; then
        echo "  $(head -1 /proc/driver/nvidia/version)"
    fi
else
    echo "WARN: /dev/nvidia0 not found. If under WSL2, /dev/dxg should be present instead."
    [ -c /dev/dxg ] && echo "  /dev/dxg present (WSL GPU paravirtualized path)" || echo "  /dev/dxg NOT present — GPU passthrough unavailable"
fi

# --- Verify Vulkan ICD ------------------------------------------------------
if command -v vulkaninfo >/dev/null 2>&1; then
    echo "=== Vulkan devices ==="
    vulkaninfo --summary 2>/dev/null | grep -E 'GPU|deviceName|apiVersion' | head -6 || echo "(vulkaninfo produced no GPU summary)"
fi

# --- Verify CUDA ------------------------------------------------------------
if ldconfig -p 2>/dev/null | grep -q 'libcudart'; then
    echo "✅ CUDA runtime library found"
else
    echo "WARN: libcudart not in ldconfig cache"
fi

echo ""
echo "=== GPU stack install COMPLETE ==="
echo "Next: rebuild WuBuOS runtime and run:"
echo "  make runtime"
echo "  make test_agi_play test_secmon"
echo ""
echo "The AGI kernel's wubu_host_exec.c will bind-mount /dev/nvidia*"
echo "into game containers. The wubu_secmon camera observes syscalls."
