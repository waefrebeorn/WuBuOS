/*
 * win32_core.h -- the small Win32 personality that sits on top of the NT layer.
 *
 * WHY THIS EXISTS (NT plan, Stage 3 / Gap A)
 *   An NT syscall layer alone does not run Notepad.exe. A Windows binary calls
 *   kernel32!CreateFileW, which converts its arguments and calls ntdll!NtCreateFile,
 *   which dispatches into this tree's VSL layer. The middle piece -- the API
 *   surface, error mapping and character encoding -- did not exist at all: no
 *   GetLastError, no SetLastError, no WideCharToMultiByte, no NTSTATUS-to-Win32
 *   translation anywhere in the runtime.
 *
 *   This module is deliberately the SMALLEST useful slice, not a start at the
 *   whole Win32 surface:
 *     - per-thread last error, because every Windows function reports failure
 *       through it and callers cannot proceed without it;
 *     - NTSTATUS -> Win32 error, because our NT layer speaks NTSTATUS and
 *       Windows callers expect a Win32 code;
 *     - UTF-16 <-> UTF-8 conversion, because Windows strings are UTF-16 and
 *       WuBuOS strings are UTF-8, and every file or window operation crosses
 *       that boundary.
 *
 * WHAT IT DOES NOT DO
 *   It does not create threads, windows or files. Those come in later stages and
 *   will convert, then call the NT layer -- never reimplement it.
 *
 * FAILURE POLICY
 *   Conversion failures return -1 with SetLastError(ERROR_NO_UNICODE_TRANSLATION
 *   or ERROR_INSUFFICIENT_BUFFER) and leave the destination buffer untouched.
 *   A partially converted string is worse than none: the caller would see a
 *   truncated filename and believe it was the real one.
 */
#ifndef WUBU_WIN32_CORE_H
#define WUBU_WIN32_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- Win32 error codes we use ------------------------------- */
#define W32_ERROR_SUCCESS                0u
#define W32_ERROR_INVALID_FUNCTION       1u
#define W32_ERROR_FILE_NOT_FOUND         2u
#define W32_ERROR_ACCESS_DENIED          5u
#define W32_ERROR_INVALID_HANDLE         6u
#define W32_ERROR_INVALID_PARAMETER     0x57u   /* 87  */
#define W32_ERROR_INSUFFICIENT_BUFFER   0x7Au   /* 122 */
#define W32_ERROR_HANDLE_EOF            38u
#define W32_ERROR_FILE_EXISTS           80u
#define W32_ERROR_GEN_FAILURE           31u
#define W32_ERROR_INTERNAL_ERROR        0x3EFu  /* 1007 */
#define W32_ERROR_NO_UNICODE_TRANSLATION 0x459u /* 1113 */

/* ---- last error ------------------------------------------------ */

/* Thread-local, matching Win32: each thread carries its own last error and one
 * thread's failure is invisible to another's. C11 _Thread_local. */
void    SetLastError(uint32_t err);
uint32_t GetLastError(void);

/* ---- NTSTATUS -> Win32 --------------------------------------- */

/* Map an NTSTATUS from the VSL layer onto the Win32 error a Windows caller
 * expects. Unknown statuses map to ERROR_INTERNAL_ERROR rather than to something
 * plausible but wrong -- a caller that switches on the code must be able to
 * tell "known failure" from "we have no idea". */
uint32_t Win32FromNTStatus(int64_t status);

/* ---- character conversion ------------------------------------ */

/* Convert UTF-16 to UTF-8.
 * `wide_chars` counts 16-bit units; -1 means the input is NUL-terminated.
 * Returns the number of BYTES written, excluding the terminator, or -1 with
 * GetLastError() set when the input holds an unpaired surrogate or the buffer
 * cannot hold the result. The destination is left untouched on failure. */
int WideCharToUtf8(const uint16_t *wide, int wide_chars,
                   char *utf8, int utf8_cap);

/* Convert UTF-8 to UTF-16.
 * `utf8_len` counts bytes; -1 means the input is NUL-terminated.
 * Returns the number of 16-bit UNITS written, excluding the terminator, or -1
 * with GetLastError() set on invalid UTF-8 or a too-small buffer. The
 * destination is left untouched on failure. */
int Utf8ToWideChar(const char *utf8, int utf8_len,
                   uint16_t *wide, int wide_cap);

#endif /* WUBU_WIN32_CORE_H */
