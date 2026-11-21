# L14: WEMU user-mode build (reconciled 2026-11-19).
#
# src/wemu is a submodule pinned at leg/exceptions (29202c1): the
# user-mode leg (ELF loader, SysV stack, FS/GS TLS bases, SSE2/AVX2
# growth, the vpandn es=16 fix), LOCK atomics and guest exception
# delivery are UPSTREAM in the wemu repo. This fragment builds WEMU out-of-tree
# (copy into build/wemu-user) so the submodule checkout stays pristine
# and the wubuos build never writes into it.

WEMU_SUB   := src/wemu
WEMU_BUILD := build/wemu-user
WEMU_LIB   := $(WEMU_BUILD)/libwemu-user.a
WEMU_INC   := -I$(WEMU_BUILD)/include -I$(WEMU_BUILD)/src
WEMU_CFLAGS := -std=c11 -O2 -g -Wall -Wextra -Werror
WEMU_SRC_NAMES := wemu_cpu wemu_mem wemu_decode wemu_exec wemu_sse \
                  wemu_string wemu_machine wemu_elf wemu_x87 wemu_exception \
                  wemu_usermode
WEMU_OBJS := $(patsubst %,$(WEMU_BUILD)/src/%.o,$(WEMU_SRC_NAMES))

# The native user-mode signal handlers run on the guest stack with a
# guest-set %fs, so the default -fstack-protector canary is not the host's.
# Scope the exception to this one translation unit (mirrors wemu/Makefile).
$(WEMU_BUILD)/src/wemu_usermode.o: WEMU_CFLAGS += -fno-stack-protector

# Copy rule: refresh when the submodule pointer or any source changes.
# The VSL mprotect/fetch enforcement hooks are a NARROW overlay applied only
# to the out-of-tree copy (never to the submodule): they add
# wemu_set_mem_perm_hook()/wemu_set_fetch_perm_hook() so the WEMU guest-memory
# path consults VSL's single-owner prot table. The native user-mode fork/clone
# and sandbox-boundary changes now live IN WEMU itself (combined 0fa6b52,
# based on defa672/e484379); the patch no longer duplicates them.
$(WEMU_BUILD)/.copied: $(WEMU_SUB)/.git $(wildcard $(WEMU_SUB)/src/*.c) $(wildcard $(WEMU_SUB)/src/*.h) $(wildcard $(WEMU_SUB)/include/*.h) mk/wemu-mprotect-prot.patch
	rm -rf $(WEMU_BUILD)
	mkdir -p $(WEMU_BUILD)
	cp -r $(WEMU_SUB)/src $(WEMU_SUB)/include $(WEMU_SUB)/tests \
	      $(WEMU_SUB)/Makefile $(WEMU_SUB)/NATIVE_USERMODE.md $(WEMU_BUILD)/
	mkdir -p $(WEMU_BUILD)/tools
	patch -p1 -d $(WEMU_BUILD) < mk/wemu-mprotect-prot.patch
	touch $@

# Objects depend on the stamp (the .c files only exist after the copy
# step, so they cannot be pattern prerequisites). No -MMD here: the
# stamp already forces a full rebuild when sources change.
$(WEMU_BUILD)/src/%.o: $(WEMU_BUILD)/.copied
	$(CC) $(WEMU_CFLAGS) $(WEMU_INC) -c $(WEMU_BUILD)/src/$*.c -o $@

$(WEMU_LIB): $(WEMU_OBJS)
	$(AR) rcs $@ $^
