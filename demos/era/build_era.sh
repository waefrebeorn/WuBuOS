#!/usr/bin/env bash
#
# build_era.sh -- build the "one app per computing era" demo set for WuBuOS.
#
# Produces real, runnable binaries for the eras that HAVE a CPU emulator /
# exec backend inside WuBuOS, and reports the eras that are currently
# EMULATOR GAPS (no 8080 / 68000 interpreter, no cross-toolchain) honestly.
#
# Each runnable demo writes a marker file (WUBU_ERA_<ERA>.OK) so the launch
# path can be verified end-to-end through its VSL personality.
#
# Run from the repo root:  bash demos/era/build_era.sh
set -uo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
ERA="$REPO/demos/era"
OUT="$ERA"
cd "$ERA"

echo "== WuBuOS era demo build =="
mkdir -p "$OUT"

# --- 1981 MS-DOS : real 8086 .COM (runs via in-process 8086 shim) ---
if command -v nasm >/dev/null 2>&1; then
    nasm -f bin dos_hello.asm -o dos_hello.com && echo "[OK] dos_hello.com (DOS 8086)"
else
    echo "[SKIP] dos_hello.com -- nasm missing"
fi

# --- 1993 Windows NT : real Win64 PE (runs via Wine/Proton) ---
if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
    x86_64-w64-mingw32-gcc -O2 win_era_demo.c -o win_era_demo.exe && echo "[OK] win_era_demo.exe (Win64 PE)"
else
    echo "[SKIP] win_era_demo.exe -- mingw missing"
fi

# --- 2007 Linux native : real ELF (runs via VSL linux_elf exec) ---
if command -v gcc >/dev/null 2>&1; then
    gcc -O2 -static linux_era_demo.c -o linux_era_demo.elf && echo "[OK] linux_era_demo.elf (Linux ELF)"
else
    echo "[SKIP] linux_era_demo.elf -- gcc missing"
fi

# --- 2020 HolyC / TempleOS : SOURCE run via hc_eval (no compile needed) ---
if [ -f holyc_era_demo.hc ]; then
    echo "[OK] holyc_era_demo.hc (HolyC source -- JIT via hc_eval)"
fi

# --- 1974 CP/M 2.2 : EMULATOR GAP (no 8080 emu + no 8080 assembler) ---
echo "[GAP] cpm_stat.asm  -- CP/M personality has BDOS syscalls but NO 8080 CPU emulator; not runnable yet."

# --- 1984 Classic Mac 68K : EMULATOR GAP (no 68K emu + no 68K toolchain) ---
echo "[GAP] mac_about.c   -- Classic Mac personality has A-line syscalls but NO 68000 CPU emulator; not runnable yet."

echo "== done =="
ls -la "$OUT"/*.com "$OUT"/*.exe "$OUT"/*.elf 2>/dev/null
