#!/bin/bash
# detect_hardware.sh — the AGI's host-detection tool (2026-08-04).
#
# "we work by using detection, methodologies and known research" — this
# tool detects WHAT the AGI is running on and WHICH driver the driver
# space should use. Detection → driver selection → differential
# self-test → run. It reads the OS's OWN truth (uname/lscpu/cpuid) and
# the WSL GPU passthrough, and reports the driver-space mapping.
#
# Usage: tools/detect_hardware.sh        (full report)
#        tools/detect_hardware.sh --isa  (just the ISA + driver name)
#        tools/detect_hardware.sh --gpu  (just the GPU line)

set -u
ISA="$(uname -m 2>/dev/null || echo unknown)"
DRIVER=""

case "$ISA" in
    x86_64|amd64)  DRIVER="x86-64" ;;
    aarch64|arm64) DRIVER="arm64" ;;
    riscv64|rv64)  DRIVER="riscv" ;;
    m68k|m68*)     DRIVER="m68k" ;;
    i386|i486|i586|i686) DRIVER="x86-32" ;;
    ppc64|ppc64le) DRIVER="powerpc" ;;
    s390x)         DRIVER="s390x" ;;
    loongarch64)   DRIVER="loongarch" ;;
    *)             DRIVER="unknown (portable interpreter fallback)" ;;
esac

# CPU feature flags (x86): AVX/AVX2/AVX512/AMX — the type-set ladder
FLAGS=""
if command -v lscpu >/dev/null 2>&1; then
    FLAGS="$(lscpu | grep -i '^Flags:' | sed 's/^Flags:[[:space:]]*//' | tr ' ' '\n' \
             | grep -E '^(avx|avx2|avx512|amx|sse4|sse4_2|f16c|fma|vaes)' | sort | tr '\n' ' ')"
fi

# GPU: WSL passthrough first (libcuda via /dev/dxg), else nvidia-smi
GPU_LINE=""
if [ -e /dev/dxg ]; then
    GPU_LINE="WSL GPU passthrough present (/dev/dxg)"
    if [ -x "$(command -v nvidia-smi.exe 2>/dev/null)" ]; then
        GPU_LINE="$(nvidia-smi.exe --query-gpu=name,memory.total --format=csv,noheader 2>/dev/null | head -1)"
    fi
elif command -v nvidia-smi >/dev/null 2>&1; then
    GPU_LINE="$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1)"
elif command -v lspci >/dev/null 2>&1; then
    GPU_LINE="$(lspci 2>/dev/null | grep -iE 'vga|3d|display' | head -1)"
fi
[ -z "$GPU_LINE" ] && GPU_LINE="none detected (CPU-only)"

# NPU / accelerators (the neural tier)
NPU_LINE=""
if command -v lspci >/dev/null 2>&1; then
    NPU_LINE="$(lspci 2>/dev/null | grep -iE 'npu|neural|accelerat|movidius|tpu' | head -1)"
fi
[ -z "$NPU_LINE" ] && NPU_LINE="none detected"

# WSL? (the detection nuance: we run on Windows hardware via /dev/dxg)
WSL=""
[ -e /proc/sys/fs/binfmt_misc/WSLInterop ] && WSL=" (WSL: Windows host, Linux kernel)"

if [ "${1:-}" = "--isa" ]; then
    echo "$DRIVER"
    exit 0
fi
if [ "${1:-}" = "--gpu" ]; then
    echo "$GPU_LINE"
    exit 0
fi

echo "=== HOST DETECTION (the AGI knows where it is) ==="
echo "ISA:          $ISA → driver: $DRIVER"
echo "CPU flags:    ${FLAGS:-none detected (non-x86 or no lscpu)}"
echo "GPU:          $GPU_LINE$WSL"
echo "NPU:          $NPU_LINE"
echo "OS:           $(uname -srm 2>/dev/null)$WSL"
echo
echo "Driver space: $DRIVER native; m68k/riscv/mcu via interpreters;"
echo "              GPU via Vulkan/WebGPU front; NPU via GGUF/ONNX bridge."
