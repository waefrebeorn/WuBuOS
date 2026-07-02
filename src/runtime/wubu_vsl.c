/*
 * wubu_vsl.c  --  WuBuOS VSL Legacy API Shim
 *
 * This file exists ONLY for backward compatibility with the Makefile's RT_OBJS
 * list. All actual VSL implementation lives in the vsl/ submodules.
 *
 * On first link against the RT_OBJS that include both wubu_vsl.o and vsl/vsl.o,
 * g_vsl is defined once (in vsl/vsl.c) and seen via extern here.
 */

/* Pull in the VSL state declaration — actual definition is in vsl/vsl.c */
#include "vsl/vsl_internal.h"

/* No code here — all VSL implementation is in vsl/vsl.c, vsl/vsl_syscall.c, etc. */