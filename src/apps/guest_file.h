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

/* Write any pending changes to the backing store right now.
 * A clean shutdown already flushes, so applications normally do not need this;
 * it matters when the result must be readable by another process immediately,
 * or when a command just produced something the user will act on. */
int guest_file_flush(void);

/* Remove a file. Directories are refused -- recursive deletion needs an
 * explicit walk and must not happen by accident from a single call.
 * Returns 0 on success, -1 when the name is absent or is a directory. */
int guest_file_delete(const char *name);

/* List every regular file currently in the namespace.
 * Copies at most `cap` NUL-terminated names into `out` (each row `max_len`
 * bytes) and returns how many were written.  Returns -1 when out/cap are
 * unusable.  Built-in entries are included: they ARE the filesystem a user
 * saved into, and hiding them would make search lie about what exists.
 *
 * This is what lets Start search answer "where did I save that document"
 * instead of only "which app do you want". */
int guest_file_list(char out[][64], int max_len, int cap);

/* True when a regular file with this exact name exists. */
bool guest_file_exists(const char *name);

/* Enumerate the DIRECT children of one guest directory.
 *
 * File Manager needs folders as well as files -- the earlier guest_file_list()
 * deliberately skipped directories for Start-search purposes, so Explorer could
 * never browse into anything.  Returned names are bare basenames (no path), and
 * `is_dir` carries the type so the caller can render and descend.
 *
 * The callback returns false to stop early.  Returns the number delivered, or -1
 * on a bad argument.  Path may be "/" or "dir" or "dir/"; all mean the same
 * thing. */
typedef bool (*guest_child_cb)(const char *name, bool is_dir,
                               uint32_t size, void *user);
int guest_file_children(const char *path, guest_child_cb cb, void *user);

/* Resolve one name against the guest store: size and directory flag.
 * Returns true when the name exists there. */
bool guest_file_is_dir(const char *name);

bool guest_file_stat(const char *name, uint32_t *size, bool *is_dir);

/* Capacity ceiling for one guest file, in bytes.
 *
 * This stays a MACRO because callers use it to size arrays at compile time
 * (notepad's read buffer, the runline capture buffer).  The RUNTIME bound is
 * guest_file_max(), which WUBU_FS_FILE_MAX can lower -- on bare metal a smaller
 * per-file ceiling means a smaller static filesystem table, and it cannot be
 * raised past this value because styxfs_file_t embeds the payload inline. */
#define GUEST_FILE_MAX_LIMIT (8192u)
#define GUEST_FILE_MAX      GUEST_FILE_MAX_LIMIT

/* Effective per-file capacity (<= GUEST_FILE_MAX_LIMIT), revolver-controlled. */
uint32_t guest_file_max(void);

#endif /* WUBU_GUEST_FILE_H */
