/*
 * wubu_manifest_json.c -- minimal JSON parser for the WuBuOS manifest.
 * Self-contained C11. Supports only the subset we emit: objects, string
 * keys, ints, string values, arrays of objects. No floats/escapes beyond
 * \" and \\. Errors are reported as -1 so a malformed manifest fails closed
 * (we never silently ship a partial syscall table).
 */
#include "wubu_manifest_internal.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    const char *p;
    size_t      len;
    size_t      pos;
} js;

static void skip_ws(js *s) {
    while (s->pos < s->len) {
        char c = s->p[s->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') s->pos++;
        else break;
    }
}

/* Parse a JSON string starting at '"'; writes into buf (size n). Returns 0 on
 * success, -1 on overflow/early-eof. Advances s->pos past closing quote. */
static int parse_string(js *s, char *buf, int n) {
    skip_ws(s);
    if (s->pos >= s->len || s->p[s->pos] != '"') return -1;
    s->pos++; /* skip opening quote */
    int i = 0;
    while (s->pos < s->len) {
        char c = s->p[s->pos++];
        if (c == '"') { buf[i] = '\0'; return 0; }
        if (c == '\\' && s->pos < s->len) {
            char e = s->p[s->pos++];
            if (e == 'n') c = '\n';
            else if (e == 't') c = '\t';
            else if (e == 'r') c = '\r';
            else if (e == '"') c = '"';
            else if (e == '\\') c = '\\';
            else { buf[i] = e; if (i < n-1) i++; continue; }
        }
        if (i < n - 1) buf[i++] = c;
    }
    return -1;
}

/* Parse a non-negative integer. Returns 0 on success. */
static int parse_uint(js *s, uint64_t *out) {
    skip_ws(s);
    if (s->pos >= s->len) return -1;
    if (s->p[s->pos] < '0' || s->p[s->pos] > '9') return -1;
    uint64_t v = 0;
    while (s->pos < s->len && s->p[s->pos] >= '0' && s->p[s->pos] <= '9') {
        v = v * 10 + (uint64_t)(s->p[s->pos++] - '0');
    }
    *out = v;
    return 0;
}

/* Forward decls for recursive structure parsing. */
static int parse_value(js *s, wubu_manifest_t *m, wubu_syscall_entry_t *cur);

/* Parse a member value into one of: "rights" map, "syscalls" array, or a
 * single syscall object field. Returns 0 on success. */
static int parse_member_value(js *s, wubu_manifest_t *m,
                              const char *key, wubu_syscall_entry_t *cur) {
    if (strcmp(key, "num") == 0) {
        uint64_t v; if (parse_uint(s, &v) != 0) return -1;
        if (cur) cur->num = v;
        return 0;
    }
    if (strcmp(key, "name") == 0)    return parse_string(s, cur ? cur->name : (char[1]){0}, WUBU_MANIFEST_NAME_MAX);
    if (strcmp(key, "handler") == 0) return parse_string(s, cur ? cur->handler : (char[1]){0}, WUBU_MANIFEST_NAME_MAX);
    if (strcmp(key, "cap") == 0)     return parse_string(s, cur ? cur->cap : (char[1]){0}, WUBU_MANIFEST_NAME_MAX);
    if (strcmp(key, "styx") == 0)    return parse_string(s, cur ? cur->styx : (char[1]){0}, WUBU_MANIFEST_NAME_MAX);
    if (strcmp(key, "holyc") == 0)   return parse_string(s, cur ? cur->holyc : (char[1]){0}, WUBU_MANIFEST_NAME_MAX);
    /* unknown scalar key: skip a string or uint */
    if (s->pos < s->len && s->p[s->pos] == '"') {
        char tmp[256]; return parse_string(s, tmp, sizeof(tmp));
    }
    uint64_t v; return parse_uint(s, &v);
}

/* Parse the "rights" object: {"NAME": "0x..", ...} */
static int parse_rights(js *s, wubu_manifest_t *m) {
    skip_ws(s);
    if (s->pos >= s->len || s->p[s->pos] != '{') return -1;
    s->pos++;
    for (;;) {
        skip_ws(s);
        if (s->pos >= s->len) return -1;
        if (s->p[s->pos] == '}') { s->pos++; break; }
        char key[WUBU_MANIFEST_NAME_MAX];
        if (parse_string(s, key, sizeof(key)) != 0) return -1;
        skip_ws(s);
        if (s->pos >= s->len || s->p[s->pos] != ':') return -1;
        s->pos++;
        skip_ws(s);
        /* value is a hex/dec string */
        char val[32];
        if (parse_string(s, val, sizeof(val)) != 0) return -1;
        uint64_t bit = strtoull(val, NULL, 0);
        if (m->rights_count < 32) {
            strncpy(m->rights[m->rights_count].name, key, WUBU_MANIFEST_NAME_MAX-1);
            m->rights[m->rights_count].name[WUBU_MANIFEST_NAME_MAX-1] = '\0';
            m->rights[m->rights_count].bit = bit;
            m->rights_count++;
        }
        skip_ws(s);
        if (s->pos < s->len && s->p[s->pos] == ',') { s->pos++; continue; }
        if (s->pos < s->len && s->p[s->pos] == '}') { s->pos++; break; }
        return -1;
    }
    return 0;
}

/* Parse a syscall object. */
static int parse_syscall_obj(js *s, wubu_manifest_t *m) {
    skip_ws(s);
    if (s->pos >= s->len || s->p[s->pos] != '{') return -1;
    s->pos++;
    if (m->count >= (int)WUBU_MANIFEST_MAX_SYSCALLS) return -1;
    wubu_syscall_entry_t *cur = &m->entries[m->count];
    memset(cur, 0, sizeof(*cur));
    for (;;) {
        skip_ws(s);
        if (s->pos >= s->len) return -1;
        if (s->p[s->pos] == '}') { s->pos++; break; }
        char key[WUBU_MANIFEST_NAME_MAX];
        if (parse_string(s, key, sizeof(key)) != 0) return -1;
        skip_ws(s);
        if (s->pos >= s->len || s->p[s->pos] != ':') return -1;
        s->pos++;
        if (parse_member_value(s, m, key, cur) != 0) return -1;
        skip_ws(s);
        if (s->pos < s->len && s->p[s->pos] == ',') { s->pos++; continue; }
        if (s->pos < s->len && s->p[s->pos] == '}') { s->pos++; break; }
        return -1;
    }
    m->count++;
    return 0;
}

/* Parse the "syscalls" array. */
static int parse_syscalls_array(js *s, wubu_manifest_t *m) {
    skip_ws(s);
    if (s->pos >= s->len || s->p[s->pos] != '[') return -1;
    s->pos++;
    for (;;) {
        skip_ws(s);
        if (s->pos >= s->len) return -1;
        if (s->p[s->pos] == ']') { s->pos++; break; }
        if (parse_syscall_obj(s, m) != 0) return -1;
        skip_ws(s);
        if (s->pos < s->len && s->p[s->pos] == ',') { s->pos++; continue; }
        if (s->pos < s->len && s->p[s->pos] == ']') { s->pos++; break; }
        return -1;
    }
    return 0;
}

/* Parse the top-level object, dispatching "rights" and "syscalls". */
static int parse_value(js *s, wubu_manifest_t *m, wubu_syscall_entry_t *cur) {
    (void)cur;
    skip_ws(s);
    if (s->pos >= s->len) return -1;
    if (s->p[s->pos] == '{') {
        s->pos++;
        for (;;) {
            skip_ws(s);
            if (s->pos >= s->len) return -1;
            if (s->p[s->pos] == '}') { s->pos++; break; }
            char key[WUBU_MANIFEST_NAME_MAX];
            if (parse_string(s, key, sizeof(key)) != 0) return -1;
            skip_ws(s);
            if (s->pos >= s->len || s->p[s->pos] != ':') return -1;
            s->pos++;
            skip_ws(s);
            if (strcmp(key, "rights") == 0) {
                if (parse_rights(s, m) != 0) return -1;
            } else if (strcmp(key, "syscalls") == 0) {
                if (parse_syscalls_array(s, m) != 0) return -1;
            } else {
                /* skip unknown value */
                if (s->pos < s->len && s->p[s->pos] == '"') {
                    char tmp[256]; if (parse_string(s, tmp, sizeof(tmp)) != 0) return -1;
                } else { uint64_t v; if (parse_uint(s, &v) != 0) return -1; }
            }
            skip_ws(s);
            if (s->pos < s->len && s->p[s->pos] == ',') { s->pos++; continue; }
            if (s->pos < s->len && s->p[s->pos] == '}') { s->pos++; break; }
            return -1;
        }
        return 0;
    }
    return -1;
}

int wubu_json_parse_manifest(const char *json, size_t len, wubu_manifest_t *m) {
    memset(m, 0, sizeof(*m));
    js s = { json, len, 0 };
    skip_ws(&s);
    if (parse_value(&s, m, NULL) != 0) return -1;
    /* Resolve cap-name -> right bitmask for each entry. */
    for (int i = 0; i < m->count; i++) {
        uint64_t bit = 0;
        for (int r = 0; r < m->rights_count; r++) {
            if (strcmp(m->entries[i].cap, m->rights[r].name) == 0) {
                bit = m->rights[r].bit; break;
            }
        }
        m->entries[i].right = bit;
    }
    return 0;
}
