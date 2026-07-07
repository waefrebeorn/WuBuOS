/*
 * wubu_holyd_window.c  --  WuBuOS HolyC DOS Daemon: Window
 */

#include "wubu_holyd_internal.h"

/* -- Window Operations -------------------------------------------- */

int wubu_holyd_window_create(WubuHoly *d, const char *session,
                               WubuHolyWindowType type,
                               int x, int y, int w, int h,
                               const char *title, int *out_window_id) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    if (s->window_count >= WUBU_HOLYD_MAX_WINDOWS) return -1;

    WubuHolyWindow *win = &s->windows[s->window_count];
    memset(win, 0, sizeof(*win));
    win->id = (int)(s - d->sessions) * WUBU_HOLYD_MAX_WINDOWS + s->window_count;
    win->type = type;
    win->x = x; win->y = y;
    win->w = w; win->h = h;
    win->visible = true;
    if (title) strncpy(win->title, title, sizeof(win->title) - 1);
    strncpy(win->session_name, session, WUBU_HOLYD_MAX_SESSION_NAME - 1);

    win->fb_size = w * h * sizeof(uint32_t);
    win->framebuffer = (uint32_t *)calloc(1, win->fb_size);
    if (!win->framebuffer) return -1;

    s->window_count++;
    if (out_window_id) *out_window_id = win->id;

    holyd_log(d, 2, "Window %d created in session '%s' (%s %dx%d)",
              win->id, session, wubu_holyd_window_type_str(type), w, h);
    wubu_holyd_publish_event(d, "window_created", session, title);
    return 0;
}

int wubu_holyd_window_destroy(WubuHoly *d, const char *session, int window_id) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].id == window_id) {
            if (s->windows[i].framebuffer) free(s->windows[i].framebuffer);
            memmove(&s->windows[i], &s->windows[i + 1],
                    (s->window_count - i - 1) * sizeof(WubuHolyWindow));
            s->window_count--;
            wubu_holyd_publish_event(d, "window_destroyed", session, NULL);
            return 0;
        }
    }
    return -1;
}

int wubu_holyd_window_show(WubuHoly *d, const char *session, int window_id) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].id == window_id) {
            s->windows[i].visible = true;
            wubu_holyd_publish_event(d, "window_shown", session, NULL);
            return 0;
        }
    }
    return -1;
}

int wubu_holyd_window_hide(WubuHoly *d, const char *session, int window_id) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].id == window_id) {
            s->windows[i].visible = false;
            wubu_holyd_publish_event(d, "window_hidden", session, NULL);
            return 0;
        }
    }
    return -1;
}

int wubu_holyd_window_resize(WubuHoly *d, const char *session,
                               int window_id, int w, int h) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].id == window_id) {
            s->windows[i].w = w;
            s->windows[i].h = h;
            if (s->windows[i].framebuffer) {
                free(s->windows[i].framebuffer);
                s->windows[i].fb_size = w * h * sizeof(uint32_t);
                s->windows[i].framebuffer = (uint32_t *)calloc(1, s->windows[i].fb_size);
            }
            wubu_holyd_publish_event(d, "window_resized", session, NULL);
            return 0;
        }
    }
    return -1;
}

int wubu_holyd_window_move(WubuHoly *d, const char *session,
                             int window_id, int x, int y) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].id == window_id) {
            s->windows[i].x = x;
            s->windows[i].y = y;
            return 0;
        }
    }
    return -1;
}

int wubu_holyd_window_focus(WubuHoly *d, const char *session, int window_id) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    for (int i = 0; i < s->window_count; i++) {
        s->windows[i].focused = (s->windows[i].id == window_id);
    }
    s->focused_window = window_id;
    s->last_active = time(NULL);
    wubu_holyd_publish_event(d, "window_focused", session, NULL);
    return 0;
}

int wubu_holyd_window_list(WubuHoly *d, const char *session,
                             WubuHolyWindow *out, int max) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s || !out) return -1;
    int count = s->window_count < max ? s->window_count : max;
    memcpy(out, s->windows, count * sizeof(WubuHolyWindow));
    return count;
}

