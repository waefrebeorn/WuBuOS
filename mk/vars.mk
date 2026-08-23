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

# HolyD frontend sources (canonical list; wubunos renamed holyc_* -> holyd_*)
HOLYD_SRC = $(COMP)/holyd_lexer.c $(COMP)/holyd_parse.c $(COMP)/holyd_parse_ast.c \
            $(COMP)/holyd_codegen.c $(COMP)/holyd_codegen_emit.c $(COMP)/holyd_codegen_expr.c \
            $(COMP)/holyd_codegen_stmt.c $(COMP)/holyd_codegen_api.c $(COMP)/wubu_preproc.c \
            $(COMP)/holyd_runtime.c $(COMP)/holyd_mir_eval.c
