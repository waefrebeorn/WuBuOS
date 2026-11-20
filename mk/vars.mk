# ── Variables & Directory Paths ──────────────────────────────────
# WuBuOS Top-Level Makefile variables

CC      = gcc
# Cached test-object dirs: defined HERE (before any rule reads them) so
# $(CACHE) in target PREREQUISITES expands correctly; a late := definition
# left prerequisites empty and clean-tree tier builds could not find them.
CACHE   := build/testobj
TEST_BIN := build/testbin
CFLAGS  = -Iinclude -Wall -Wextra -Wno-unused-function -std=c11 -O2 -g -D_POSIX_C_SOURCE=200809L -Wno-array-bounds -DWUBU_NO_LIBM -I/usr/include/libdrm -Wno-unused-result -Wno-implicit-function-declaration -Wno-return-type -Wno-unused-variable -Wno-format-truncation -Wno-unused-parameter
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
SPELLS  = src/spells
FRAMEWORK = src/framework
TOOLS   = src/tools
HOSTED  = src/hosted
LUA     = src/lua
AUDIO   = src/audio
SHELL_DIR  = src/shell
BEAR    = src/bear
FW       = src/firmware

# ── Auto-detected EdDSA backend config (hosted/product build) ──────────────
# Build-time probe: probes what is installed, NEVER fetches anything.  An
# already-cached repo-local mbedTLS dev root enables the ladder (and, with
# OpenSSL, the native core); an absent root yields a valid fail-closed config
# instead of blocking `make hosted`.  Regenerated at parse time and written
# only on change, so the generated header also drives incremental rebuilds.
WUBU_EDDSA_BUILD_DIR := build
$(shell mkdir -p $(WUBU_EDDSA_BUILD_DIR) && bash tools/detect_native_eddsa.sh --out-dir $(WUBU_EDDSA_BUILD_DIR) --quiet)
include $(WUBU_EDDSA_BUILD_DIR)/native_eddsa.mk

# ── Hosted TLS configuration gate ─────────────────────────────────────────
# The DEFAULT hosted build deliberately does NOT link mbedTLS: HTTPS fails
# closed (oci_http_connect returns -1 without -DWUBU_HAVE_MBEDTLS).  Wiring
# mbedTLS into every hosted build would make a network-fetched dev root a
# hard dependency of the bare-runner gate, so TLS is a configuration choice:
#   make hosted HOSTED_TLS=1 WUBU_MBEDTLS_ROOT=<extracted dev root>
# tools/hosted_tls_gate.sh (make test_hosted_tls) verifies BOTH states.
HOSTED_TLS ?= 0
OCI_TLS_CFLAGS :=
HOSTED_TLS_LIBS :=
ifeq ($(HOSTED_TLS),1)
  ifeq ($(strip $(WUBU_MBEDTLS_ROOT)),)
    $(error HOSTED_TLS=1 requires WUBU_MBEDTLS_ROOT=<extracted mbedTLS dev root>)
  endif
  OCI_TLS_CFLAGS := -DWUBU_HAVE_MBEDTLS -I$(WUBU_MBEDTLS_ROOT)/usr/include
  HOSTED_TLS_LIBS := -L$(WUBU_MBEDTLS_ROOT)/usr/lib/x86_64-linux-gnu -lmbedtls -lmbedx509 -lmbedcrypto -Wl,-rpath,$(WUBU_MBEDTLS_ROOT)/usr/lib/x86_64-linux-gnu
endif
