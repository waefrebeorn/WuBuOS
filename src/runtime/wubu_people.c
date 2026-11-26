/*
 * wubu_people.c -- see the header. A bounded TSV store; no library, no allocation, and every field written
 * with a bounded copy so a long line in the file cannot become a long line in memory.
 */
#include "wubu_people.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *wubu_people_default_path(void)
{
    const char *env = getenv("WUBU_PEOPLE_DB");
    if (env && *env) return env;
    static char path[256];
    const char *home = getenv("HOME");
    snprintf(path, sizeof path, "%s/.local/share/wubu/people.tsv", (home && *home) ? home : "/root");
    return path;
}

static void copy_field(char *dst, size_t cap, const char *src)
{
    if (!src) { dst[0] = 0; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

static int ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static int icontains(const char *hay, const char *needle)
{
    if (!*needle) return 1;
    for (const char *h = hay; *h; h++) {
        const char *a = h, *b = needle;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
        if (!*b) return 1;
    }
    return 0;
}

int wubu_people_load(WubuPeople *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    FILE *f = fopen(wubu_people_default_path(), "r");
    if (!f) return 0;                       /* no file: empty store, not an error */
    char line[512];
    while (fgets(line, sizeof line, f) && out->count < WUBU_PEOPLE_MAX) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (!n || line[0] == '#') continue;
        char *t1 = strchr(line, '\t');
        char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        if (t1) *t1 = 0;
        if (t2) *t2 = 0;
        char *name = line;
        char *email = t1 ? t1 + 1 : "";
        char *phone = t2 ? t2 + 1 : "";
        if (!*name) continue;               /* a person with no name is not a person */
        WubuPerson *p = &out->p[out->count++];
        copy_field(p->name, sizeof p->name, name);
        copy_field(p->email, sizeof p->email, email);
        copy_field(p->phone, sizeof p->phone, phone);
    }
    fclose(f);
    return 0;
}

int wubu_people_save(const WubuPeople *in)
{
    if (!in) return -1;
    const char *path = wubu_people_default_path();

    /* Write to a sibling temp file and rename: a reader never sees a partial store, and a crash mid-write
     * cannot destroy the contacts that were already there. */
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    fprintf(f, "# wubu people: name<TAB>email<TAB>phone\n");
    for (int i = 0; i < in->count; i++)
        fprintf(f, "%s\t%s\t%s\n", in->p[i].name, in->p[i].email, in->p[i].phone);
    if (fclose(f) != 0) return -1;
    if (rename(tmp, path) != 0) { remove(tmp); return -1; }
    return 0;
}

int wubu_people_add(WubuPeople *in, const char *name, const char *email, const char *phone)
{
    if (!in || !name) return -1;
    if (!*name) return 2;
    for (int i = 0; i < in->count; i++)
        if (ieq(in->p[i].name, name)) return 2;          /* duplicate, case-insensitively */
    if (in->count >= WUBU_PEOPLE_MAX) return 1;
    WubuPerson *p = &in->p[in->count++];
    copy_field(p->name, sizeof p->name, name);
    copy_field(p->email, sizeof p->email, email ? email : "");
    copy_field(p->phone, sizeof p->phone, phone ? phone : "");
    return 0;
}

int wubu_people_find(const WubuPeople *in, const char *query, int *idx, int max)
{
    if (!in || !query || !idx || max <= 0) return -1;
    int n = 0;
    for (int i = 0; i < in->count && n < max; i++) {
        if (icontains(in->p[i].name, query) || icontains(in->p[i].email, query) ||
            icontains(in->p[i].phone, query))
            idx[n++] = i;
    }
    return n;
}

int wubu_people_line(const WubuPeople *in, char *out, size_t cap)
{
    if (!in || !out || cap == 0) return -1;
    if (in->count == 0) snprintf(out, cap, "no contacts");
    else if (in->count == 1) snprintf(out, cap, "1 contact");
    else snprintf(out, cap, "%d contacts", in->count);
    return 0;
}
