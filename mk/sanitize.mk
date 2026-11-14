# Sanitizer gate -- see docs/BMDS_GAP_ANALYSIS.md item 6.
#
# The tree had fuzzing for the JIT and hostile-input sweeps that fork, but NO
# ASan/UBSan build anywhere in mk/*.mk.  A hostile sweep proves "did not crash";
# a sanitizer proves "did not read or write out of bounds, did not overflow, did
# not execute undefined behaviour".  Those are different claims and only one of
# them was being made.
#
# Only the input-handling surfaces live here: parsers and loaders that consume
# bytes from outside the tree (.desktop files, guest filesystem images, sprite
# sheets, NT syscall arguments).  Everything is compiled fresh with the sanitizers
# on; the ordinary (fast) targets above stay unsanitized.

# -Werror=implicit-function-declaration: calling an undeclared function is
# undefined behaviour, and it already bit once -- mime_contract read a bool
# from garbage because parse_desktop_file had no visible prototype, so the
# check passed or failed depending on stack layout.
SAN_CFLAGS := -O1 -g -std=c11 -fno-omit-frame-pointer \
              -fsanitize=address,undefined -fno-sanitize-recover=all \
              -Werror=implicit-function-declaration

# Run a sanitized binary with leak detection off: these tests intentionally fork
# and some children exit via _exit(), which ASan's leak checker reads as leaks.
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=0:exitcode=97:halt_on_error=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1

# Defined BEFORE the test_sanitize rule on purpose.  GNU make expands a rule's
# prerequisite list at parse time, so a target named only through a variable
# defined further down the file expands to NOTHING and the prerequisite is
# silently dropped -- the recipe then runs a path that was never built.
# Every future target added here must keep its variable above the rule.
FS_PERSIST_FUZZ_SAN = $(HOSTED)/fs_persist_fuzz.san
MIME_FUZZ_SAN = $(GUI)/mime_fuzz.san
SPELLMOD_FUZZ_SAN = $(RT)/spellmod_fuzz.san
JSON_FUZZ_SAN = $(RT)/json_fuzz.san
SPELL_RESOLVE_SAN = $(RT)/wubu_spell_resolve_test.san
SPELL_SCAN_SAN = $(RT)/wubu_spell_scan_test.san
FS_CRASH_SAFE_SAN = $(HOSTED)/fs_crash_safe_test.san

.PHONY: test_sanitize
# All four targets live here.  The NT sweep looks like it should be excluded for
# cost (it forks once per slot) but MEASURED at ~0.7s under ASan, so splitting the
# gate would save nothing and would hide the most valuable target behind a second
# command somebody has to remember to run.
test_sanitize: $(GUI)/mime_contract_test.san $(RT)/revolver_test.san \
               $(HOSTED)/test_fs_persist.san $(FS_PERSIST_FUZZ_SAN) $(MIME_FUZZ_SAN) $(SPELLMOD_FUZZ_SAN) $(JSON_FUZZ_SAN) $(SPELL_RESOLVE_SAN) $(SPELL_SCAN_SAN) $(FS_CRASH_SAFE_SAN) $(VSL_NT_SAN_BIN)
	@set -e; fail=0; \
	for b in $(GUI)/mime_contract_test.san $(RT)/revolver_test.san \
	         $(HOSTED)/test_fs_persist.san $(SPELL_RESOLVE_SAN) $(SPELL_SCAN_SAN) $(FS_CRASH_SAFE_SAN) $(VSL_NT_SAN_BIN); do \
	   echo "--- sanitized: $$b"; \
	   if ! $$b; then echo "!!! $$b failed under sanitizers"; fail=1; fi; \
	done; \
	$(FS_PERSIST_FUZZ_SAN) 7 4000; \
	$(MIME_FUZZ_SAN) 7 4000; \
	$(SPELLMOD_FUZZ_SAN) 7 4000; \
	$(JSON_FUZZ_SAN) 7 4000; \
	if [ $$fail -eq 0 ]; then echo "=== all sanitized targets clean ==="; fi; \
	exit $$fail

# .desktop / MIME action parser -- the file format that comes off disk.
$(GUI)/mime_contract_test.san: $(GUI)/wubu_mime.c $(GUI)/wubu_mime_desktop.c $(GUI)/mime_contract_test.c
	$(CC) $(SAN_CFLAGS) -DVBE_HOSTED -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
		-I$(GUI) -I$(RT) -I$(KERNEL) \
		$(GUI)/wubu_mime.c $(GUI)/wubu_mime_desktop.c $(GUI)/mime_contract_test.c \
		-o $@ -lm

# Revolver clamping -- integer parsing of environment variables.
$(RT)/revolver_test.san: $(RT)/wubu_revolver.c $(RT)/test_revolver.c $(HOSTED)/hosted_styxfs.c $(HOSTED)/hosted_fs_persist.c
	$(CC) $(SAN_CFLAGS) -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
		-I$(RT) -I$(HOSTED) -I$(KERNEL) $(RT)/wubu_revolver.c \
		$(HOSTED)/hosted_styxfs.c $(HOSTED)/hosted_fs_persist.c \
		$(RT)/test_revolver.c -o $@ -lm

# Guest filesystem image loader -- WUBFS01 header + records from disk.
$(HOSTED)/test_fs_persist.san: $(HOSTED)/test_fs_persist.c $(HOSTED)/hosted_styxfs.c $(HOSTED)/hosted_fs_persist.c $(RT)/wubu_revolver.c
	$(CC) $(SAN_CFLAGS) -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
		-I$(HOSTED) -I$(RT) -I$(KERNEL) -I$(GUI) \
		$(HOSTED)/hosted_styxfs.c $(HOSTED)/hosted_fs_persist.c $(RT)/wubu_revolver.c \
		$(HOSTED)/test_fs_persist.c -o $@ -lm

# Mutation fuzzer for the same loader, run under ASan/UBSan.  A memory fault aborts
# the run instead of surfacing as a failure somewhere else later; the deterministic
# seed makes any finding reproducible.
$(FS_PERSIST_FUZZ_SAN): $(HOSTED)/fs_persist_fuzz.c $(HOSTED)/hosted_styxfs.c \
                        $(HOSTED)/hosted_fs_persist.c $(RT)/wubu_revolver.c \
                        $(RT)/wubu_guest_root.c
	$(CC) $(SAN_CFLAGS) -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
		-I$(HOSTED) -I$(RT) -I$(KERNEL) -I$(GUI) \
		$(HOSTED)/hosted_styxfs.c $(HOSTED)/hosted_fs_persist.c $(RT)/wubu_revolver.c \
		$(RT)/wubu_guest_root.c $(HOSTED)/fs_persist_fuzz.c -o $@ -lm

# Mutation fuzzer for the .desktop parser, under ASan/UBSan.
$(MIME_FUZZ_SAN): $(GUI)/mime_fuzz.c $(GUI)/wubu_mime.c $(GUI)/wubu_mime_desktop.c
	$(CC) $(SAN_CFLAGS) -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
		-I$(GUI) -I$(RT) -I$(KERNEL) \
		$(GUI)/wubu_mime.c $(GUI)/wubu_mime_desktop.c \
		$(GUI)/mime_fuzz.c -o $@ -lm

# Mutation fuzzer for the .spell module container, under ASan/UBSan.
$(SPELLMOD_FUZZ_SAN): $(RT)/spellmod_fuzz.c $(RT)/wubu_spellmod.c
	$(CC) $(SAN_CFLAGS) -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
		-I$(RT) -I$(KERNEL) \
		$(RT)/wubu_spellmod.c $(RT)/spellmod_fuzz.c -o $@ -lm

# Mutation fuzzer for the JSON reader, under ASan/UBSan.
$(JSON_FUZZ_SAN): $(RT)/json_fuzz.c $(RT)/wubu_json.c
	$(CC) $(SAN_CFLAGS) -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
		-I$(RT) -I$(KERNEL) \
		$(RT)/wubu_json.c $(RT)/json_fuzz.c -o $@ -lm

# Dependency resolver -- pure graph work over borrowed handles.
$(SPELL_RESOLVE_SAN): $(RT)/wubu_spell_resolve.c $(RT)/wubu_spellmod.c \
                      $(RT)/wubu_spell_resolve_test.c
	$(CC) $(SAN_CFLAGS) -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
		-I$(RT) -I$(KERNEL) \
		$(RT)/wubu_spellmod.c $(RT)/wubu_spell_resolve.c \
		$(RT)/wubu_spell_resolve_test.c -o $@ -lm

# Directory discovery for .spell modules, under ASan/UBSan.
$(SPELL_SCAN_SAN): $(RT)/wubu_spell_scan.c $(RT)/wubu_spell_resolve.c \
                   $(RT)/wubu_spellmod.c $(RT)/wubu_spell_scan_test.c
	$(CC) $(SAN_CFLAGS) -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
		-I$(RT) -I$(KERNEL) \
		$(RT)/wubu_spellmod.c $(RT)/wubu_spell_resolve.c $(RT)/wubu_spell_scan.c \
		$(RT)/wubu_spell_scan_test.c -o $@ -lm

# Guest-store crash safety, under ASan/UBSan.
$(FS_CRASH_SAFE_SAN): $(HOSTED)/fs_crash_safe_test.c $(HOSTED)/hosted_styxfs.c \
                      $(HOSTED)/hosted_fs_persist.c $(RT)/wubu_revolver.c \
                      $(RT)/wubu_guest_root.c
	$(CC) $(SAN_CFLAGS) -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
		-I$(HOSTED) -I$(RT) -I$(KERNEL) -I$(GUI) \
		$(HOSTED)/hosted_styxfs.c $(HOSTED)/hosted_fs_persist.c $(RT)/wubu_revolver.c \
		$(RT)/wubu_guest_root.c $(HOSTED)/fs_crash_safe_test.c -o $@ -lm

# NT syscall surface -- every slot probed with all-zero arguments, forked.
VSL_NT_SAN_BIN = $(RT)/vsl_nt_contract_test.san
$(VSL_NT_SAN_BIN): $(VSL_NT_OBJS) $(KERNEL)/wubu_modsym.o
	$(CC) $(SAN_CFLAGS) -no-pie -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
		-DHAVE_VULKAN -DHAVE_CUDA -I$(RT) -I$(RT)/vsl -I$(KERNEL) \
		$(filter-out $(RT)/vsl/vsl_syscall_nt_test.o,$(VSL_NT_OBJS)) \
		$(KERNEL)/wubu_modsym.o src/runtime/vsl/vsl_nt_contract_test.c \
		-o $@ -ldl -lvulkan -lcuda
