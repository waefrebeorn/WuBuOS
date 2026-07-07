/*
 * wubu_holyd_event.c  --  WuBuOS HolyC DOS Daemon: Event
 */

#include "wubu_holyd_internal.h"

/* -- Event Bus ---------------------------------------------------- */

int wubu_holyd_publish_event(WubuHoly *d, const char *event_type,
                               const char *session, const char *data) {
    if (!d || !event_type) return -1;
    holyd_log(d, 2, "EVENT: %s session=%s", event_type, session ? session : "*");
    char *event_path = holyd_path_join(d->config.sessions_path, "events");
    if (!event_path) return -1;
    FILE *f = fopen(event_path, "a");
    free(event_path);
    if (!f) return -1;
    time_t now = time(NULL);
    fprintf(f, "{\"time\":%ld,\"event\":\"%s\",\"session\":\"%s\",\"data\":\"%s\"}\n",
            (long)now, event_type, session ? session : "", data ? data : "");
    fclose(f);
    return 0;
}

