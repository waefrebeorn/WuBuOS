/* edr_fanotify.c  --  WuBuOS EDR File Telemetry Pins
 *
 * Two kernel telemetry sources for file and configuration events:
 *
 * 1. fanotify — file create/write/delete/rename, exec, AND timestomp (FAN_ATTRIB)
 *    Monitors the entire root mount for security-relevant file operations.
 *    PID tracking on every event, script content capture on open-for-exec.
 *
 * 2. inotify — directory-level monitoring for "registry-like" events:
 *    /etc, /tmp, ~/.config, /usr/share, /var/spool/cron for config drift,
 *    scheduled task changes, and persistence mechanisms.
 *
 * Maps to heavener's kernel minifilter + CmRegisterCallbackEx equivalents
 * on Linux: fanotify ~ IRP_MJ_CREATE/WRITE/DELETE/RENAME,
 * inotify ~ registry callback, FAN_ATTRIB ~ SetBasicInfo timestomp detection.
 */

#include "edr_internal.h"
#include <sys/fanotify.h>
#include <sys/inotify.h>

/* ================================================================
 * fanotify — file operations
 * ================================================================ */

int edr_fanotify_start(void) {
    /* fanotify — file events with PID tracking */
    g_fanotify_fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_CONTENT,
                                   O_RDONLY);
    if (g_fanotify_fd < 0) {
        /* fanotify not supported (WSL/containers) — not fatal */
    } else {
        unsigned long mask = FAN_CREATE | FAN_DELETE | FAN_CLOSE_WRITE |
                             FAN_RENAME | FAN_OPEN_EXEC | FAN_ATTRIB;
        fanotify_mark(g_fanotify_fd, FAN_MARK_ADD | FAN_MARK_MOUNT,
                      mask, AT_FDCWD, "/");
        printf("[edr_fanotify] fanotify active on root mount (mask=0x%lx)\n",
               (unsigned long)mask);
    }

    /* inotify — monitor config/cron dirs for registry-like events */
    g_inotify_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (g_inotify_fd >= 0) {
        inotify_add_watch(g_inotify_fd, "/etc",
                          IN_CREATE | IN_DELETE | IN_MODIFY);
        inotify_add_watch(g_inotify_fd, "/tmp",
                          IN_CREATE | IN_DELETE);
        inotify_add_watch(g_inotify_fd, "/usr/share",
                          IN_CREATE | IN_DELETE | IN_MODIFY);
        inotify_add_watch(g_inotify_fd, "/var/spool/cron",
                          IN_CREATE | IN_DELETE | IN_MODIFY);
        inotify_add_watch(g_inotify_fd, "/etc/cron.d",
                          IN_CREATE | IN_DELETE | IN_MODIFY);
        inotify_add_watch(g_inotify_fd, "/etc/systemd/system",
                          IN_CREATE | IN_DELETE | IN_MODIFY);
        printf("[edr_fanotify] inotify active on %d dirs\n", 6);
    } else {
        perror("[edr_fanotify] inotify_init1");
    }

    return 0;
}

/* Attempt to read the filename behind a fanotify event's fd */
static void fanotify_read_name(int fd, uint32_t pid, char *out, size_t out_size) {
    char proc_fd[64];
    snprintf(proc_fd, sizeof(proc_fd), "/proc/%u/fd/%d", pid, fd);
    ssize_t r = readlink(proc_fd, out, out_size - 1);
    if (r > 0) out[r] = '\0';
    else out[0] = '\0';
}

/* Capture script content from a file opened for execution */
static char *capture_script_content(const char *path, size_t *out_len) {
    if (!path || !path[0]) return NULL;
    /* Only capture common script types */
    const char *exts[] = {".sh", ".py", ".pl", ".rb", ".lua", ".js", ".ps1", NULL};
    const char *ext = NULL;
    size_t plen = strlen(path);
    for (int i = 0; exts[i]; i++) {
        size_t elen = strlen(exts[i]);
        if (plen >= elen && strcmp(path + plen - elen, exts[i]) == 0) {
            ext = exts[i];
            break;
        }
    }
    if (!ext) return NULL;

    /* Read first 4KB */
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char *buf = calloc(1, 4096);
    if (!buf) { fclose(f); return NULL; }
    *out_len = fread(buf, 1, 4095, f);
    fclose(f);
    buf[*out_len] = '\0';
    return buf;
}

/* Check if a file path should trigger a REGISTRY-like event */
static bool is_registry_path(const char *path) {
    if (!path) return false;
    return (strncmp(path, "/etc/", 5) == 0) ||
           (strstr(path, "/.config/") != NULL) ||
           (strstr(path, "/systemd/") != NULL) ||
           (strncmp(path, "/usr/share/", 11) == 0);
}

void edr_fanotify_poll(void) {
    /* Poll fanotify */
    if (g_fanotify_fd >= 0) {
        char buf[4096];
        struct fanotify_event_metadata *meta;
        ssize_t n = read(g_fanotify_fd, buf, sizeof(buf));
        if (n > 0) {
            meta = (struct fanotify_event_metadata*)buf;
            if (meta->fd >= 0) {
                uint32_t pid = meta->pid;
                uint16_t evt_type = 0;
                bool capture_script = false;

                if (meta->mask & FAN_CREATE)
                    evt_type = EDR_EV_FILE_CREATE;
                else if (meta->mask & FAN_DELETE)
                    evt_type = EDR_EV_FILE_DELETE;
                else if (meta->mask & FAN_CLOSE_WRITE)
                    evt_type = EDR_EV_FILE_WRITE;
                else if (meta->mask & FAN_RENAME)
                    evt_type = EDR_EV_FILE_RENAME;
                else if (meta->mask & FAN_ATTRIB)
                    evt_type = EDR_EV_FILE_SET_BASIC_INFO; /* timestomp detection */
                else if (meta->mask & FAN_OPEN_EXEC) {
                    evt_type = EDR_EV_SCRIPT_EXECUTION;
                    capture_script = true;
                }

                if (evt_type) {
                    /* Read filename from /proc/pid/fd/ */
                    char fname[EDR_MAX_PATH];
                    fanotify_read_name(meta->fd, pid, fname, sizeof(fname));

                    size_t extra_size = 0;
                    char *script_content = NULL;

                    if (capture_script && fname[0]) {
                        script_content = capture_script_content(fname, &extra_size);
                    }

                    /* Allocate event with space for filename + script content */
                    size_t ev_size = sizeof(EdrEvent) + EDR_MAX_PATH + extra_size + 1;
                    EdrEvent *ev = calloc(1, ev_size);
                    if (ev) {
                        ev->header.version = 1;
                        ev->header.type = evt_type;
                        ev->header.timestamp = (uint64_t)time(NULL) * 1000000000ULL;
                        ev->header.pid = pid;

                        if (meta->mask & FAN_ATTRIB)
                            ev->header.u32 = 1; /* attribute change flag */

                        ev->header.var_count = 1; /* filename */
                        if (script_content) ev->header.var_count++;
                        ev->header.size = (uint32_t)ev_size;

                        /* Copy filename */
                        char *dst = EDR_EVENT_DATA(ev);
                        size_t fn_len = strlen(fname) + 1;
                        if (fn_len > EDR_MAX_PATH) fn_len = EDR_MAX_PATH;
                        memcpy(dst, fname, fn_len);

                        /* Copy script content after filename */
                        if (script_content) {
                            memcpy(dst + EDR_MAX_PATH, script_content, extra_size);
                            free(script_content);
                        }

                        /* Check if this path is registry-like */
                        if (fname[0] && is_registry_path(fname)) {
                            EdrEvent *reg_ev = calloc(1, sizeof(EdrEvent) + EDR_MAX_PATH);
                            if (reg_ev) {
                                reg_ev->header.version = 1;
                                reg_ev->header.type = EDR_EV_REG_SET_VALUE;
                                reg_ev->header.timestamp = ev->header.timestamp;
                                reg_ev->header.pid = pid;
                                reg_ev->header.size = sizeof(EdrEvent) + EDR_MAX_PATH;
                                reg_ev->header.var_count = 1;
                                memcpy(EDR_EVENT_DATA(reg_ev), fname, fn_len);
                                edr_queue_push(reg_ev);
                            }
                        }

                        edr_queue_push(ev);
                    }
                }
                close(meta->fd);
            }
        }
    }

    /* Poll inotify */
    if (g_inotify_fd >= 0) {
        char ibuf[sizeof(struct inotify_event) + NAME_MAX + 1];
        ssize_t n = read(g_inotify_fd, ibuf, sizeof(ibuf));
        if (n > 0) {
            struct inotify_event *iev = (struct inotify_event *)ibuf;
            if (iev->len > 0) {
                uint16_t evt_type = 0;
                if (iev->mask & IN_CREATE)
                    evt_type = EDR_EV_FILE_CREATE;
                else if (iev->mask & IN_DELETE)
                    evt_type = EDR_EV_FILE_DELETE;
                else if (iev->mask & IN_MODIFY)
                    evt_type = EDR_EV_FILE_WRITE;

                if (evt_type) {
                    EdrEvent *ev = calloc(1, sizeof(EdrEvent) + EDR_MAX_PATH);
                    if (ev) {
                        ev->header.version = 1;
                        ev->header.type = evt_type;
                        ev->header.timestamp = (uint64_t)time(NULL) * 1000000000ULL;
                        ev->header.pid = 0; /* inotify doesn't give us PID */
                        ev->header.size = sizeof(EdrEvent) + EDR_MAX_PATH;
                        ev->header.var_count = 1;
                        /* We can't get the full path from inotify easily,
                           but iev->name gives us the basename */
                        char *dst = EDR_EVENT_DATA(ev);
                        snprintf(dst, EDR_MAX_PATH, "inotify:%.*s", (int)iev->len, iev->name);
                        edr_queue_push(ev);
                    }
                }
            }
        }
    }
}

void edr_fanotify_stop(void) {
    if (g_fanotify_fd >= 0) {
        close(g_fanotify_fd);
        g_fanotify_fd = -1;
    }
    if (g_inotify_fd >= 0) {
        close(g_inotify_fd);
        g_inotify_fd = -1;
    }
}