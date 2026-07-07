/*
 * wubu_holyd_9p.c  --  WuBuOS HolyC DOS Daemon: 9P
 */

#include "wubu_holyd_internal.h"

/* -- 9P Namespace ------------------------------------------------- */

int wubu_holyd_mount(WubuHoly *d, const char *session, const char *path) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    if (s->mount_point) {
        free(s->mount_point);
    }
    s->mount_point = strdup(path);
    if (!s->mount_point) return -1;
    s->mounted = true;
    holyd_log(d, 2, "Session '%s' mounted at %s", session, path);
    wubu_holyd_publish_event(d, "session_mounted", session, path);
    return 0;
}

int wubu_holyd_unmount(WubuHoly *d, const char *session) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    if (s->mount_point) {
        free(s->mount_point);
        s->mount_point = NULL;
    }
    s->mounted = false;
    holyd_log(d, 2, "Session '%s' unmounted", session);
    wubu_holyd_publish_event(d, "session_unmounted", session, NULL);
    return 0;
}

int wubu_holyd_export(WubuHoly *d, const char *session,
                        const char *path, const char *target) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    if (!path || !target) return -1;

    /* Create parent directory for target if needed */
    char *parent = holyd_path_join(target, "..");
    if (parent) {
        mkdir(parent, 0755);
        free(parent);
    }

    /* Remove existing symlink/file at target if present */
    unlink(target);

    /* Create symlink: target -> path */
    if (symlink(path, target) < 0) {
        holyd_log(d, 0, "Session '%s' export failed: symlink(%s -> %s): %s",
                  session, target, path, strerror(errno));
        return -1;
    }

    holyd_log(d, 2, "Session '%s' export %s -> %s", session, path, target);
    wubu_holyd_publish_event(d, "session_exported", session, target);
    return 0;
}

