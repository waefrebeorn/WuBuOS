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

# WuBuKernel contract source: prefer the pinned submodule (CI initialises
# it); fall back to the canonical local checkout only when the submodule
# is uninitialised. Never hardcode /home/wubu into the shipped build.
WUBU_KERNEL_CONTRACT ?= $(KERNEL)/WuBuKernel
ifeq ($(wildcard $(WUBU_KERNEL_CONTRACT)/src/wubu_cap.c),)
WUBU_KERNEL_CONTRACT := /home/wubu/WuBuKernel
endif
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
# already-cached repo-local mbedTLS dev root enables the ladder; OpenSSL
# enables the native backend on its own (the verification policy is
# bignum-free in src/runtime/wubu_eddsa_policy.c).  An absent dev root yields
# a valid native-only config; absent OpenSSL AND mbedTLS yields a fail-closed
# config instead of blocking `make hosted`.  Regenerated at parse time and
# written only on change, so the generated header also drives incremental
# rebuilds.
WUBU_EDDSA_BUILD_DIR := build
$(shell mkdir -p $(WUBU_EDDSA_BUILD_DIR) && bash tools/detect_native_eddsa.sh --out-dir $(WUBU_EDDSA_BUILD_DIR) --quiet)
include $(WUBU_EDDSA_BUILD_DIR)/native_eddsa.mk
# The product links the SAME fully bignum-free EdDSA path the package-signing
# gate tests by default.  The legacy mbedTLS MPI backend remains available to
# the opt-in gate via WUBU_EDDSA_FIXED=0; the shipped product never selects it.
WUBU_EDDSA_HOSTED_CFLAGS += -DWUBU_EDDSA_FIXED_LADDER

# ── Hosted TLS configuration gate ─────────────────────────────────────────
# Build-time probe, NO network fetch. When an mbedTLS dev root is ALREADY
# present (explicit WUBU_MBEDTLS_ROOT, or the repo-local cached root) the
# DEFAULT hosted build links the HTTPS path (HOSTED_TLS=1). Without a root
# the default stays fail-closed HTTP-only; the EdDSA ladder and HTTP sync
# path keep working. HOSTED_TLS=0/1 still force either state explicitly.
# tools/hosted_tls_gate.sh (make test_hosted_tls) verifies all states.
WUBU_MBEDTLS_ROOT ?= $(shell bash tools/mbedtls_devroot.sh 2>/dev/null || true)
HOSTED_TLS_AUTO := 0
ifneq ($(strip $(WUBU_MBEDTLS_ROOT)),)
  ifneq ($(wildcard $(WUBU_MBEDTLS_ROOT)/usr/include/mbedtls/version.h),)
    ifneq ($(wildcard $(WUBU_MBEDTLS_ROOT)/usr/lib/x86_64-linux-gnu/libmbedcrypto.a),)
      HOSTED_TLS_AUTO := 1
    endif
  endif
endif
HOSTED_TLS ?= $(HOSTED_TLS_AUTO)
OCI_TLS_CFLAGS :=
HOSTED_TLS_LIBS :=
ifeq ($(HOSTED_TLS),1)
  ifeq ($(strip $(WUBU_MBEDTLS_ROOT)),)
    $(error HOSTED_TLS=1 requires an already-present mbedTLS dev root: set WUBU_MBEDTLS_ROOT or run make mbedtls-devroot)
  endif
  OCI_TLS_CFLAGS := -DWUBU_HAVE_MBEDTLS -I$(WUBU_MBEDTLS_ROOT)/usr/include
  HOSTED_TLS_LIBS := -L$(WUBU_MBEDTLS_ROOT)/usr/lib/x86_64-linux-gnu -lmbedtls -lmbedx509 -lmbedcrypto -Wl,-rpath,$(WUBU_MBEDTLS_ROOT)/usr/lib/x86_64-linux-gnu
endif

# OCI objects are compiled with OCI_TLS_CFLAGS, which depends on HOSTED_TLS
# and the dev-root path.  Make that state an explicit prerequisite: without
# it, a TLS-enabled object survives a later root-less build and the link
# fails with undefined mbedtls_* references.  The recipe is forced to run
# but only rewrites the file when the state actually changes, so unrelated
# builds do not recompile everything.
HOSTED_TLS_STATE := build/hosted_tls.state
.PHONY: FORCE_HOSTED_TLS_STATE
FORCE_HOSTED_TLS_STATE:
$(HOSTED_TLS_STATE): FORCE_HOSTED_TLS_STATE
	@mkdir -p build
	@printf '%s|%s\n' '$(HOSTED_TLS)' '$(WUBU_MBEDTLS_ROOT)' | cmp -s - $@ || \
	  printf '%s|%s\n' '$(HOSTED_TLS)' '$(WUBU_MBEDTLS_ROOT)' > $@
