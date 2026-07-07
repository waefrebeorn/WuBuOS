/*
 * wubu_holyd_session.c  --  WuBuOS HolyC DOS Daemon: Session
 */

#include "wubu_holyd_internal.h"

/* -- Session Operations ------------------------------------------- */

int wubu_holyd_session_create(WubuHoly *d, const char *name,
                                int width, int height) {
    if (!d || !name) return -1;
    if (d->session_count >= d->config.max_sessions) return -1;
    if (holyd_find_session(d, name)) return -1; /* Already exists */

    WubuHolySession *s = &d->sessions[d->session_count];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, WUBU_HOLYD_MAX_SESSION_NAME - 1);
    s->state = SESSION_STATE_STARTING;
    s->created = time(NULL);
    s->last_active = time(NULL);
    s->save_interval_sec = d->config.save_interval_sec;
    s->focused_window = -1;

    /* Create session directory using dynamic allocation */
    s->save_path = holyd_path_join(d->config.sessions_path, name);
    if (!s->save_path) {
        s->state = SESSION_STATE_ERROR;
        return -1;
    }
    mkdir(s->save_path, 0755);

    /* Create default terminal window */
    WubuHolyWindow *w = &s->windows[0];
    w->id = d->session_count * WUBU_HOLYD_MAX_WINDOWS + 0;
    strncpy(w->title, "HolyC Terminal", sizeof(w->title) - 1);
    w->type = HOLY_WINDOW_TERM;
    w->x = 10; w->y = 10;
    w->w = width > 0 ? width : d->config.default_width;
    w->h = height > 0 ? height : d->config.default_height;
    w->visible = true;
    w->focused = true;
    strncpy(w->session_name, name, WUBU_HOLYD_MAX_SESSION_NAME - 1);
    s->window_count = 1;
    s->focused_window = w->id;

    /* Allocate framebuffer */
    w->fb_size = w->w * w->h * sizeof(uint32_t);
    w->framebuffer = (uint32_t *)calloc(1, w->fb_size);
    if (!w->framebuffer) {
        s->state = SESSION_STATE_ERROR;
        return -1;
    }

    /* Initialize compiler placeholder */
    s->compiler = NULL; /* Will be initialized on first eval */
    s->compiler_initialized = false;

    s->state = SESSION_STATE_ACTIVE;
    d->session_count++;

    holyd_log(d, 2, "Session '%s' created (%dx%d)", name, w->w, w->h);
    wubu_holyd_publish_event(d, "session_created", name, NULL);
    return 0;
}

int wubu_holyd_session_destroy(WubuHoly *d, const char *name) {
    WubuHolySession *s = holyd_find_session(d, name);
    if (!s) return -1;

    s->state = SESSION_STATE_DESTROYING;
    holyd_log(d, 2, "Destroying session '%s'", name);

    /* Free framebuffers */
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].framebuffer) {
            free(s->windows[i].framebuffer);
            s->windows[i].framebuffer = NULL;
        }
    }

    /* Free dynamically allocated paths */
    if (s->save_path) {
        free(s->save_path);
        s->save_path = NULL;
    }
    if (s->mount_point) {
        free(s->mount_point);
        s->mount_point = NULL;
    }

    /* Remove from array */
    int idx = (int)(s - d->sessions);
    memmove(&d->sessions[idx], &d->sessions[idx + 1],
            (d->session_count - idx - 1) * sizeof(WubuHolySession));
    d->session_count--;

    wubu_holyd_publish_event(d, "session_destroyed", name, NULL);
    return 0;
}

int wubu_holyd_session_list(WubuHoly *d, WubuHolySession *out, int max) {
    if (!d || !out) return -1;
    int count = d->session_count < max ? d->session_count : max;
    memcpy(out, d->sessions, count * sizeof(WubuHolySession));
    return count;
}

int wubu_holyd_session_info(WubuHoly *d, const char *name, WubuHolySession *out) {
    WubuHolySession *s = holyd_find_session(d, name);
    if (!s || !out) return -1;
    *out = *s;
    return 0;
}

int wubu_holyd_session_focus(WubuHoly *d, const char *name) {
    WubuHolySession *s = holyd_find_session(d, name);
    if (!s) return -1;
    s->last_active = time(NULL);
    holyd_log(d, 2, "Session '%s' focused", name);
    wubu_holyd_publish_event(d, "session_focused", name, NULL);
    return 0;
}

