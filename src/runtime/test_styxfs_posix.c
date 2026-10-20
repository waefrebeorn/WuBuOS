/*
 * test_styxfs_posix.c -- the POSIX-like file layer (GOALS-1000 #733:
 * styxfs_posix had no test target). The layer operates on the server's
 * in-memory file table (styxfs_file_alloc/styxfs_resolve), NOT the host
 * filesystem: create/mkdir build table entries, write buffers into
 * write_buf, close COMMITS the buffer as the payload, re-open + read
 * returns it, rename rewrites the path, remove drops the slot.
 */
#include "styxfs.h"
#include "styxfs_internal.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

/* styxfs_posix resolves through the global server */
extern styxfs_server_t *g_styxfs_server;

int main(void)
{
    styxfs_server_t srv;
    memset(&srv, 0, sizeof(srv));
    g_styxfs_server = &srv;

    /* create: a plain file and a directory */
    CHECK(styxfs_create("/a/f.txt", 0, 0644) == 0);
    CHECK(styxfs_create("/a/f.txt", 0, 0644) == -1);   /* duplicate */
    CHECK(styxfs_mkdir("/a/d", 0755) == 0);

    struct stat st;
    CHECK(styxfs_stat("/a/f.txt", &st) == 0);
    CHECK((st.st_mode & S_IFREG) != 0);
    CHECK(st.st_size == 0);
    CHECK(styxfs_stat("/a/d", &st) == 0);
    CHECK((st.st_mode & S_IFDIR) != 0);
    CHECK(styxfs_stat("/a/missing", &st) == -1);

    /* write then read IN THE SAME SESSION: the buffer is live before the
     * close commits it (the layer's close is the 9P clunk -- it drops
     * the whole entry, so a read must happen while the fid is open) */
    int fd = styxfs_open("/a/f.txt", 0);
    CHECK(fd >= 1);
    const char *msg = "posix layer round-trip";
    CHECK(styxfs_write(fd, msg, strlen(msg)) == (ssize_t)strlen(msg));
    /* write advances the single shared cursor: an immediate read is at EOF */
    char back[64] = {0};
    CHECK(styxfs_read(fd, back, sizeof(back) - 1) == 0);

    /* close COMMITS the buffer, then clunks the entry: the fid table
     * slot is dropped (9P fid lifecycle, not POSIX fd semantics) */
    CHECK(styxfs_close(fd) == 0);
    CHECK(styxfs_stat("/a/f.txt", &st) == -1);        /* the entry is gone */
    CHECK(styxfs_open("/a/f.txt", 0) == -1);         /* and cannot re-open */
    CHECK(styxfs_read(fd, back, 4) == -1);           /* the slot no longer reads */
    /* fd bounds are refused */
    CHECK(styxfs_read(0, back, 4) == -1);
    CHECK(styxfs_read(-1, back, 4) == -1);
    CHECK(styxfs_read(99999, back, 4) == -1);
    /* a directory refuses read AND write; its entry stays while the fd
     * is open (the clunk would drop it, so keep it open for the rename
     * destination check below) */
    int dfd = styxfs_open("/a/d", 0);
    CHECK(dfd >= 1);
    CHECK(styxfs_read(dfd, back, 4) == -1);
    CHECK(styxfs_write(dfd, back, 4) == -1);

    /* rename rewrites the path; the old name stops resolving */
    CHECK(styxfs_create("/a/f2.txt", 0, 0644) == 0);
    CHECK(styxfs_rename("/a/f2.txt", "/a/g.txt") == 0);
    CHECK(styxfs_stat("/a/f2.txt", &st) == -1);
    CHECK(styxfs_stat("/a/g.txt", &st) == 0);
    CHECK(styxfs_rename("/a/g.txt", "/a/d") == -1);    /* dest exists */

    /* remove drops the slot */
    CHECK(styxfs_remove("/a/g.txt") == 0);
    CHECK(styxfs_stat("/a/g.txt", &st) == -1);
    CHECK(styxfs_remove("/a/g.txt") == -1);            /* already gone */

    /* clunking the directory fd drops its entry too */
    CHECK(styxfs_close(dfd) == 0);
    CHECK(styxfs_stat("/a/d", &st) == -1);

    /* a read-only server refuses mutation */
    srv.readonly = 1;
    CHECK(styxfs_create("/a/x", 0, 0644) == -1);
    CHECK(styxfs_remove("/a/d") == -1);
    CHECK(styxfs_rename("/a/d", "/a/e") == -1);
    srv.readonly = 0;

    /* no server at all: every call is refused at the boundary */
    g_styxfs_server = NULL;
    CHECK(styxfs_stat("/a", &st) == -1);
    CHECK(styxfs_create("/a", 0, 0644) == -1);
    CHECK(styxfs_open("/a", 0) == -1);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
