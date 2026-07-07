/*
 * wubu_holyd_save.c  --  WuBuOS HolyC DOS Daemon: Save
 */

#include "wubu_holyd_internal.h"

/* -- Auto-Save ---------------------------------------------------- */

int wubu_holyd_session_save(WubuHoly *d, const char *session) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    s->state = SESSION_STATE_SAVING;

    /* Use dynamic allocation for save file path */
    char *save_file = holyd_path_join(s->save_path, "session.sav");
    if (!save_file) {
        s->state = SESSION_STATE_ERROR;
        return -1;
    }
    FILE *f = fopen(save_file, "w");
    free(save_file);
    if (!f) { s->state = SESSION_STATE_ERROR; return -1; }

    /* Save session metadata */
    fprintf(f, "name=%s\n", s->name);
    fprintf(f, "state=%d\n", s->state);
    fprintf(f, "windows=%d\n", s->window_count);
    fprintf(f, "created=%ld\n", (long)s->created);
    fprintf(f, "last_active=%ld\n", (long)s->last_active);
    fclose(f);

    s->last_save = time(NULL);
    s->state = SESSION_STATE_ACTIVE;
    holyd_log(d, 2, "Session '%s' saved to %s/session.sav", session, s->save_path);
    return 0;
}

int wubu_holyd_session_restore(WubuHoly *d, const char *session,
                                 const char *save_path) {
    if (!d || !session || !save_path) return -1;

    /* Check if session already exists */
    if (holyd_find_session(d, session)) return -1;
    if (d->session_count >= d->config.max_sessions) return -1;

    /* Open save file */
    FILE *f = fopen(save_path, "r");
    if (!f) { holyd_log(d, 0, "Restore: cannot open %s", save_path); return -1; }

    WubuHolySession *s = &d->sessions[d->session_count];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, session, WUBU_HOLYD_MAX_SESSION_NAME - 1);
    s->state = SESSION_STATE_STARTING;
    s->focused_window = -1;
    s->save_interval_sec = d->config.save_interval_sec;

    /* Parse key=value lines */
    char line[1024];
    int width = d->config.default_width, height = d->config.default_height;
    while (fgets(line, sizeof(line), f)) {
        char key[64], val[512];
        if (sscanf(line, "%63[^=]=%511[^\n]", key, val) == 2) {
            if (strcmp(key, "name") == 0) {
                strncpy(s->name, val, WUBU_HOLYD_MAX_SESSION_NAME - 1);
            } else if (strcmp(key, "state") == 0) {
                s->state = (WubuHolySessionState)atoi(val);
            } else if (strcmp(key, "windows") == 0) {
                /* Window count hint — actual windows recreated on demand */
                int wc = atoi(val);
                if (wc > WUBU_HOLYD_MAX_WINDOWS) wc = WUBU_HOLYD_MAX_WINDOWS;
                /* Use wc to set initial window count hint */
                s->window_count = 0; /* Will be recreated on demand */
            } else if (strcmp(key, "created") == 0) {
                s->created = atol(val);
            } else if (strcmp(key, "last_active") == 0) {
                s->last_active = atol(val);
            } else if (strcmp(key, "width") == 0) {
                width = atoi(val);
            } else if (strcmp(key, "height") == 0) {
                height = atoi(val);
            }
        }
    }
    fclose(f);

    /* Create session directory using dynamic allocation */
    s->save_path = holyd_path_join(d->config.sessions_path, s->name);
    if (!s->save_path) {
        s->state = SESSION_STATE_ERROR;
        return -1;
    }
    mkdir(s->save_path, 0755);

    /* Recreate default terminal window */
    WubuHolyWindow *w = &s->windows[0];
    w->id = d->session_count * WUBU_HOLYD_MAX_WINDOWS + 0;
    strncpy(w->title, "HolyC Terminal", sizeof(w->title) - 1);
    w->type = HOLY_WINDOW_TERM;
    w->x = 10; w->y = 10;
    w->w = width; w->h = height;
    w->visible = true;
    w->focused = true;
    strncpy(w->session_name, s->name, WUBU_HOLYD_MAX_SESSION_NAME - 1);
    w->fb_size = w->w * w->h * sizeof(uint32_t);
    w->framebuffer = (uint32_t *)calloc(1, w->fb_size);
    if (!w->framebuffer) {
        s->state = SESSION_STATE_ERROR;
        return -1;
    }
    s->window_count = 1;
    s->focused_window = w->id;
    s->last_save = time(NULL);
    s->state = SESSION_STATE_ACTIVE;
    d->session_count++;

    holyd_log(d, 2, "Session '%s' restored from %s", session, save_path);
    wubu_holyd_publish_event(d, "session_restored", session, NULL);
    return 0;
}

