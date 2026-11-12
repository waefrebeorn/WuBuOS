/*
 * hosted_agent_channel.c -- the agent -> desktop channel.
 *
 * See hosted_agent_channel.h.  One concern: read the agent's namespace and
 * turn what is there into a desktop action.  It never draws and never
 * touches the render path.
 *
 * C11, opaque API.
 */

#include "hosted_agent_channel.h"
#include "../gui/wubu_bonzi.h"
#include "../apps/wubu_walkthrough.h"
#include "../audio/wubu_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int mood_from_name(const char *name) {
    static const struct { const char *n; int s; } MAP[] = {
        {"default", WUBU_BUDDY_STATE_DEFAULT},
        {"work",    WUBU_BUDDY_STATE_WORK},
        {"error",   WUBU_BUDDY_STATE_ERROR},
        {"success", WUBU_BUDDY_STATE_SUCCESS},
        {"idle",    WUBU_BUDDY_STATE_IDLE},
        {"overheat",WUBU_BUDDY_STATE_OVERHEAT},
        {"busy",    WUBU_BUDDY_STATE_BUSY},
        {"relaxed", WUBU_BUDDY_STATE_RELAXED},
        {"surf",    WUBU_BUDDY_STATE_SURF},
        {"race",    WUBU_BUDDY_STATE_RACE},
        {"read",    WUBU_BUDDY_STATE_READ},
    };
    for (size_t i = 0; i < sizeof(MAP) / sizeof(MAP[0]); i++)
        if (strcmp(MAP[i].n, name) == 0) return MAP[i].s;
    return -1;
}

static int vowel_from_name(const char *name) {
    static const char *V[] = {"a", "e", "i", "o", "u"};
    for (int i = 0; i < 5; i++)
        if (strcmp(V[i], name) == 0) return i;
    return -1;
}

static void read_first_line(const char *path, char *out, size_t cap) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, (int)cap, f)) {
        size_t n = strlen(out);
        while (n && (out[n - 1] == '\n' || out[n - 1] == '\r')) out[--n] = '\0';
    } else {
        out[0] = '\0';
    }
    fclose(f);
}

void hosted_agent_channel_poll(void) {
    const char *home = getenv("HOME");
    if (!home) return;

    char base[512];
    snprintf(base, sizeof(base), "%s/.wubu/n/agent", home);

    char path[600], line[128];

    /* The buddy's mood. */
    snprintf(path, sizeof(path), "%s/buddy_state", base);
    read_first_line(path, line, sizeof(line));
    if (line[0]) {
        int s = mood_from_name(line);
        if (s >= 0) wubu_bonzi_set_state(s);
    }

    /* The speak request: the desktop says a vowel out loud, once. An agent
       (or a person, from a shell) writes a vowel name here. */
    snprintf(path, sizeof(path), "%s/speak", base);
    read_first_line(path, line, sizeof(line));
    if (line[0]) {
        int vowel = vowel_from_name(line);
        if (vowel >= 0) wubu_glott_speak(vowel, 250, 110.0);
        remove(path);
    }

    /* The walkthrough request: act once, then clear the flag. */
    snprintf(path, sizeof(path), "%s/open_walkthrough", base);
    read_first_line(path, line, sizeof(line));
    if (line[0]) {
        remove(path);
        wubu_walkthrough_run();
    }
}
