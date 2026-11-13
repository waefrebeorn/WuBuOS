/*
 * guest_file.h -- the one way applications read and write files.
 *
 * WHY THIS IS A SEPARATE HEADER WITH NO INCLUDES
 *   The namespace itself belongs to src/hosted/hosted_styxfs.c, and only that
 *   file touches `g_fs`.  Applications must not reach into it: they would each
 *   grow their own idea of where a file lives, and then Paint and Notepad would
 *   silently disagree about the filesystem.  This header declares exactly two
 *   operations and nothing else, so an app depends on the contract rather than
 *   on the host's internal types.
 *
 * NAMES ARE FLAT
 *   The namespace has no directory tree yet; a name is a whole path-like string
 *   such as "notes/todo.txt" and it is matched literally.  Adding directories
 *   later means interpreting '/' here, in this one place, instead of in every
 *   application.
 *
 * FAILURE POLICY
 *   Every failure returns -1 and leaves the caller's buffer untouched.  An
 *   application must never be handed a partially written buffer and told it
 *   succeeded.
 */
#ifndef WUBU_GUEST_FILE_H
#define WUBU_GUEST_FILE_H

#include <stdint.h>
#include <stdbool.h>

/* Read a file into `buf` (capacity `cap`).
 * On success returns 0 and sets *len to the number of bytes copied, which is
 * always <= cap.  Returns -1 if the file does not exist, if it is a directory,
 * or if cap is zero while the file is non-empty. */
int guest_file_read(const char *name, uint8_t *buf, uint32_t cap, uint32_t *len);

/* Create or replace a file with exactly these bytes.
 * On success returns 0 and marks the namespace dirty so a clean shutdown will
 * persist it.  Returns -1 if the name is empty or too long, or if the data does
 * not fit -- the existing file, if any, is left unchanged. */
int guest_file_write(const char *name, const uint8_t *data, uint32_t len);

/* True when a regular file with this exact name exists. */
bool guest_file_exists(const char *name);

/* Human-readable capacity limit, so applications can check before saving. */
#define GUEST_FILE_MAX (8192u)

#endif /* WUBU_GUEST_FILE_H */
