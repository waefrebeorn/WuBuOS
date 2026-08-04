# ── Variables & Directory Paths ──────────────────────────────────
# WuBuOS Top-Level Makefile variables

CC      = gcc
CFLAGS  = -Wall -Wextra -Wno-unused-function -std=c11 -O2 -g -D_POSIX_C_SOURCE=200809L -Wno-array-bounds -DWUBU_NO_LIBM -I/usr/include/libdrm -Wno-unused-result -Wno-implicit-function-declaration -Wno-return-type -Wno-unused-variable -Wno-format-truncation -Wno-unused-parameter
EDR_INC = -I$(RT)/edr
LDFLAGS = -lcublas -lcudnn -lcudart

# ── Directories ──────────────────────────────────────────────────
KERNEL  = src/kernel
JIT     = src/jit
COMP    = src/compiler
GUI     = src/gui
BRIDGE  = src/bridge
APPS    = src/apps
WS      = src/worldsim
RT      = src/runtime
FRAMEWORK = src/framework
TOOLS   = src/tools
HOSTED  = src/hosted
AUDIO   = src/audio
SHELL_DIR  = src/shell
BEAR    = src/bear
FW       = src/firmware
