/*
 * hosted_fs_persist.h -- durable backing store for the guest filesystem.
 *
 * WHY THIS EXISTS
 *   The guest namespace used to live in `styxfs_file_t g_fs[STYXFS_MAX_FILES]`,
 *   a flat array in process memory.  Every byte a user typed or downloaded was
 *   lost the moment the client exited, which made the OS unusable for anything
 *   real: there was nothing to come back to.  This module gives that array a
 *   durable image on disk so a restart restores exactly what was saved.
 *
 * WHAT IT DOES NOT DO
 *   It is not a filesystem.  There are no directories-on-disk, no permissions,
 *   no fragmentation and no concurrency control.  It serialises the existing
 *   in-memory namespace to one image file and reads it back.  Keeping the
 *   in-memory model means every existing reader (walk/read/stat) is untouched.
 *
 * FAILURE POLICY
 *   Everything here fails closed.  A truncated image, a bad magic, a record
 *   claiming more data than the format allows, or a short read all return -1
 *   and leave the live namespace exactly as it was.  A corrupt store must never
 *   be silently half-loaded, because that is how you end up with a desktop that
 *   shows files whose contents do not exist.
 */
#ifndef WUBU_HOSTED_FS_PERSIST_H
#define WUBU_HOSTED_FS_PERSIST_H

#include <stdint.h>

/* Load the image into the live namespace.
 *
 * Returns the number of entries restored, or -1 if the store is absent,
 * unreadable, or malformed.  A missing store is reported as 0 so a first boot
 * is not an error; any other failure is -1 and leaves the namespace untouched.
 *
 * When `replace` is non-zero the live namespace is cleared first, so the image
 * becomes the whole truth.  When zero, entries are appended.
 */
int fs_persist_load(int replace);

/* Write the live namespace to the image, atomically.
 *
 * The image is written to a temporary file in the same directory and renamed,
 * so a crash mid-write can never leave a half-written store behind -- the old
 * image stays valid.  Returns 0 on success, -1 on any failure (the caller must
 * treat a failed save as data loss and say so rather than carry on).
 */
int fs_persist_save(void);

/* Absolute path of the image, or NULL if no writable location could be found.
 * Exposed so tests and diagnostics can point at the right file. */
const char *fs_persist_path(void);

/* Size of the image in bytes, or -1 if it cannot be determined. */
long fs_persist_size(void);

#endif /* WUBU_HOSTED_FS_PERSIST_H */
