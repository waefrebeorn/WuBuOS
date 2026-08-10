# Kernel 100-Gap Bank — triple-DA filtered (2026-08-09)

The user's directive: "the point of all this work is to learn and make
our kernel better — templeos kernel is incomplete, all of our operating
systems incompatible — its time to triple devils advocate find 100 gaps
— continue means to close gaps."

Every gap below is REAL (surveyed in src/kernel + the exec layer), not
a literature shopping list. DA filter: correctness (the kernel really
lacks it), privacy (no third-party to fill it — "there is no such
thing as third party if we can properly make the code"), robustness
(the gap causes a real failure today). Status: `open` / `wired`.

## Theme KC — kernel libc (the freestanding C surface)

- KC01 strstr — the kernel lacks it; modules hand-roll loops `open`
- KC02 strchr / strrchr — missing; path parsing needs them `open`
- KC03 memmove — missing; overlapping moves are UB `open`
- KC04 snprintf — missing (the PE loader hit this); vsnprintf exists
  but the bounded wrapper doesn't `open`
- KC05 strdup — missing; the kernel callers use raw malloc+strcpy `open`
- KC06 strncat — missing `open`
- KC07 strcasestr — missing (the file-type probes need case-insensitive
  matching) `open`
- KC08 tolower/toupper/isspace — missing; the parsers duplicate them `open`
- KC09 strtok_r — missing; the config parsers duplicate the logic `open`
- KC10 strerror — missing; the exec layer can't name its errors `open`

## Theme KS — kernel string/format SPLIT (no monoliths)

- KS01 libc.c (606 lines) split: the string half -> libc_string.c `open`
- KS02 libc.c split: the format half (vsprintf/sprintf) -> libc_format.c `open`
- KS03 libc.c split: the port-IO half -> portio.c `open`
- KS04 wubu_console.c (801 lines) split: the glyph tables -> console_font.c `open`
- KS05 wubu_console.c split: the text buffer -> console_buffer.c `open`
- KS06 wubu_console.c split: the IO -> console_io.c `open`
- KS07 vbe.c (659) split: the mode list -> vbe_modes.c `open`
- KS08 vbe.c split: the rect fillers -> vbe_fill.c `open`
- KS09 metal_main.c (656) split: the alignment asserts -> boot_assert.c `open`
- KS10 memory.c (633) split: the freelist -> mem_freelist.c `open`

## Theme KP — the PE/ELF/Mach-O in-kernel loaders (OUR kernel, not the host)

- KP01 wubu_pe.c exists but is NOT wired into the kernel objects `open`
- KP02 the kernel lacks an ELF section parser (the OpenArena x86_64) `open`
- KP03 the kernel lacks a Mach-O thin parser (the Halo/OpenArena mac) `open`
- KP04 the FAT universal-binary slice picker (verified endianness) — not
  in the kernel `open`
- KP05 the RVA->raw mapper exists; the RELOCATION walker is missing `open`
- KP06 the import table names the DLLs; the PERSONALITY answer table
  (D3D8/DSOUND/WINMM/WS2_32 as OUR tables) is missing `open`
- KP07 the kernel lacks the loader test gate for the real binaries `open`
- KP08 the PE entry-point jump (mapped image -> entry) is missing `open`
- KP09 the section ALIGNMENT policy (the image base + the page align) `open`
- KP10 the loader error taxonomy (why a binary failed to load) `open`

## Theme KD — the DOS/8086 in-kernel emulator (OURS — complete it)

- KD01 the 8086 INT table lacks the video INT 10h text modes `open`
- KD02 the 8086 lacks INT 16h keyboard polling `open`
- KD03 the 8086 lacks INT 1Ah clock read `open`
- KD04 the 8086 ALU lacks the 16-bit MUL/DIV carry flags `open`
- KD05 the DOS window lacks the .EXE (MZ) load path (COM works) `open`
- KD06 the 8086 lacks the string ops (MOVSB/CMPSB) `open`
- KD07 the DOS personality lacks the INT 21h file-open record `open`
- KD08 the DOS emu lacks the real-time clock source `open`
- KD09 the DOS window lacks the keyboard passthrough test `open`
- KD10 the 8086 lacks the AAA/AAS/DAA/DAS BCD ops `open`

## Theme KT — the kernel tests (broken/gated)

- KT01 the a11y test had a missing hosted.h include (fixed; regression
  gate needed) `open`
- KT02 the test_dosgui_apps target referenced stale .o files (fixed;
  the gate must build from .c only) `open`
- KT03 the kernel lacks a libc unit test (every new libc fn) `open`
- KT04 the kernel lacks a string-roundtrip fuzz test `open`
- KT05 the PE loader test needs the real-binary gate (exists as a
  tool; the kernel gate is missing) `open`
- KT06 the interrupt stack test lacks the double-fault case `open`
- KT07 the tasking test lacks the priority-inversion case `open`
- KT08 the memory test lacks the freelist-exhaustion case `open`
- KT09 the console test lacks the scroll-wraparound case `open`
- KT10 the boot has no POST self-test log `open`

## Theme KV — the VSL personalities (28 claimed, gaps real)

- KV01 the Win32 personality = host Wine (the crutch); the in-kernel
  answer tables are the target (KP06) `open`
- KV02 the macOS personality = host Darling; the Mach-O in-kernel
  loader is the target `open`
- KV03 the Linux personality = host container exec; the in-kernel ELF
  loader is the target `open`
- KV04 the DOS personality is OURS (the 8086 emu) — the model to
  replicate `open`
- KV05 the HolyC personality is OURS (the JIT) — the model `open`
- KV06 the personality coverage test counts bits, not end-to-end runs `open`
- KV07 the era grid shows the gaps greyed — the gaps need the REAL
  emulators (CP/M 8080, Classic Mac 68K) `open`
- KV08 the CP/M BDOS table exists (168 pairs) but no 8080 CPU `open`
- KV09 the 68K traps exist but no 68000 CPU `open`
- KV10 the personality dispatch lacks the error-message channel `open`

## Theme KM — the kernel memory (the TempleOS ring-0 surface)

- KM01 the kernel malloc has no canary (heap corruption silent) `open`
- KM02 the freelist has no coalescing (fragmentation over time) `open`
- KM03 the heap lacks the OOM callback (the loader hits it) `open`
- KM04 the mmap model (the PE/ELF image mapping) is absent `open`
- KM05 the page allocator lacks the zero-page guard `open`
- KM06 the kernel lacks the memory-region debug dump `open`
- KM07 the tasking stack lacks the guard page `open`
- KM08 the heap lacks the double-free detection `open`
- KM09 the memory init lacks the E820 region logging `open`
- KM10 the kernel lacks the allocation accounting (the AGI sees it) `open`

## Theme KX — the kernel exec/boot (the TempleOS boot surface)

- KX01 the boot lacks the POST log (what initialized) `open`
- KX02 the kernel lacks the version banner with the build hash `open`
- KX03 the boot lacks the memory-map print `open`
- KX04 the kernel lacks the symbol-table export (the AGI debugs) `open`
- KX05 the interrupt gate setup lacks the per-IRQ name table `open`
- KX06 the kernel lacks the uptime clock (the AGI world needs it) `open`
- KX07 the kernel lacks the timer interrupt (tick source) `open`
- KX08 the kernel lacks the panic dump to the serial `open`
- KX09 the boot lacks the CPU feature probe (SSE/AVX) `open`
- KX10 the kernel lacks the cold-reboot syscall `open`

## Theme KA — the AGI-in-kernel surface (the OS is the training space)

- KA01 the world-state (wubu_world) is hosted-side; the kernel needs
  its own in-kernel snapshot (uptime, heap, tasks) `open`
- KA02 the kernel lacks the event log the AGI reads `open`
- KA03 the game-session ledger is user-space; the kernel needs the
  syscall-level play record `open`
- KA04 the AGI-play input (input_key_push) needs the kernel queue
  verified in-kernel `open`
- KA05 the kernel lacks the training-stream export (the ledger to /kv) `open`
- KA06 the kernel lacks the per-task CPU accounting `open`
- KA07 the kernel lacks the load-average the AGI perceives `open`
- KA08 the kernel lacks the syscall-count histogram `open`
- KA09 the kernel lacks the interrupt-rate gauge `open`
- KA10 the kernel lacks the process tree export `open`

## Theme KB — the kernel build (the gates that must hold)

- KB01 the kernel test gate must build from .c only (no stale .o) `open`
- KB02 the kernel needs a libc test target (every fn) `open`
- KB03 the kernel needs the -Werror build for the kernel files `open`
- KB04 the kernel needs the ASan build for the user-space tests `open`
- KB05 the runtime pattern rule carries -I$(KERNEL) (wired) — the
  kernel-side includes need the reverse (the loaders use libc) `open`
- KB06 the kernel needs the clang build (the portability gate) `open`
- KB07 the kernel needs the 32-bit build warning gate `open`
- KB08 the kernel test list must be the single source of truth `open`
- KB09 the kernel needs the cross-compile check (the aarch64 lane) `open`
- KB10 the kernel needs the docker-less CI script `open`

**Total: 100 gaps (10 themes x 10).** The closes land as real C11
modules with the house discipline: opaque structs, minimal includes,
self-contained files, split-not-monolith.
