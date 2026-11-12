/*
 * wubu_agent.c -- the OS-side agent identity (see wubu_agent.h).
 *
 * The canonical card line is the single source of truth for both the seal and
 * persistence:
 *
 *      AID|CLASS|YY|SEQ|SPONSOR|MODEL|ENDPOINT|CAPABILITIES|NOTE
 *
 * Fields must not contain '|' or a newline; enrolment rejects them so the
 * format can never be ambiguous.
 *
 * C11, minimal includes, self-contained.
 */
#include "wubu_agent.h"
#include "wubu_ns_bridge_internal.h"
#include "../kernel/wubu_sha256.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static WubuAgentCard g_card;
static int   g_enroled;
static int   g_seal_ok;

/* -- the AID arithmetic ------------------------------------------------ */

int wubu_agent_check_digit(int yy, int seq)
{
    if (yy < 0 || yy > 99 || seq < 0 || seq > 9999) return -1;
    char digits[8];
    snprintf(digits, sizeof(digits), "%02d%04d", yy, seq);
    static const int weights[6] = { 7, 6, 5, 4, 3, 2 };
    int sum = 0;
    for (int i = 0; i < 6; i++)
        sum += (digits[i] - '0') * weights[i];
    int rem = sum % 11;
    return (rem == 10) ? 'X' : ('0' + rem);
}

int wubu_agent_format_aid(const char *cls, int yy, int seq, char *out, size_t cap)
{
    if (!cls || !out || cap < 12) return -1;
    if (strcmp(cls, "IN") != 0 && strcmp(cls, "WI") != 0 && strcmp(cls, "DO") != 0)
        return -1;
    int c = wubu_agent_check_digit(yy, seq);
    if (c < 0) return -1;
    snprintf(out, cap, "AID-%s-%02d-%04d-%c", cls, yy, seq, (char)c);
    return 0;
}

int wubu_agent_aid_valid(const char *aid)
{
    if (!aid) return 0;
    /* AID-<CLASS>-<YY>-<NNNN>-<C> */
    if (strncmp(aid, "AID-", 4) != 0) return 0;
    const char *p = aid + 4;
    char cls[WUBU_AGENT_CLASS_MAX] = {0};
    size_t cl = 0;
    while (*p && *p != '-' && cl < sizeof(cls) - 1) cls[cl++] = *p++;
    cls[cl] = '\0';
    if (strcmp(cls, "IN") != 0 && strcmp(cls, "WI") != 0 && strcmp(cls, "DO") != 0)
        return 0;
    if (*p != '-') return 0;
    p++;
    if (strlen(p) != 9) return 0;            /* YY-NNNN-C */
    if (p[2] != '-' || p[7] != '-') return 0;
    char yy[3] = { p[0], p[1], 0 };
    char nn[5] = { p[3], p[4], p[5], p[6], 0 };
    for (int i = 0; i < 2; i++) if (yy[i] < '0' || yy[i] > '9') return 0;
    for (int i = 0; i < 4; i++) if (nn[i] < '0' || nn[i] > '9') return 0;
    int expect = wubu_agent_check_digit(atoi(yy), atoi(nn));
    if (expect < 0) return 0;
    return (p[8] == (char)expect) ? 1 : 0;
}

/* -- enrolment + persistence ------------------------------------------- */

static int field_ok(const char *s)
{
    if (!s || !s[0]) return 0;
    return strchr(s, '|') == NULL && strchr(s, '\n') == NULL;
}

int wubu_agent_enrol(WubuAgentCard *card)
{
    if (!card) return -1;
    if (strcmp(card->class_code, "IN") != 0 &&
        strcmp(card->class_code, "WI") != 0 &&
        strcmp(card->class_code, "DO") != 0) return -1;
    if (!field_ok(card->sponsor)) return -1;         /* no anonymous standing */
    if (!field_ok(card->model))        return -1;
    if (!field_ok(card->endpoint))     return -1;
    if (!field_ok(card->capabilities)) return -1;
    if (strchr(card->note, '|') != NULL) return -1;

    char aid[WUBU_AGENT_AID_MAX];
    if (wubu_agent_format_aid(card->class_code, card->yy, card->seq, aid, sizeof(aid)) != 0)
        return -1;

    memset(&g_card, 0, sizeof(g_card));
    memcpy(&g_card, card, sizeof(g_card));
    snprintf(g_card.aid, sizeof(g_card.aid), "%s", aid);
    g_enroled = 1;
    return 0;
}

static void card_canonical(char *out, size_t cap)
{
    snprintf(out, cap, "%s|%s|%d|%d|%s|%s|%s|%s|%s",
             g_card.aid, g_card.class_code, g_card.yy, g_card.seq,
             g_card.sponsor, g_card.model, g_card.endpoint,
             g_card.capabilities, g_card.note);
}

int wubu_agent_save(const char *path)
{
    if (!path || !g_enroled) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    char line[1024];
    card_canonical(line, sizeof(line));
    fprintf(f, "%s\n", line);
    fclose(f);
    return 0;
}

int wubu_agent_load(const char *path)
{
    if (!path) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[1024];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';

    /* split on '|' into the nine fields, in order */
    char *fields[9] = {0};
    size_t n = 0;
    char *p = line;
    fields[n++] = p;
    while (*p && n < 9) {
        if (*p == '|') { *p = '\0'; fields[n++] = p + 1; }
        p++;
    }
    if (n != 9) return -1;

    WubuAgentCard c;
    memset(&c, 0, sizeof(c));
    snprintf(c.aid, sizeof(c.aid), "%s", fields[0]);
    snprintf(c.class_code, sizeof(c.class_code), "%s", fields[1]);
    c.yy  = atoi(fields[2]);
    c.seq = atoi(fields[3]);
    snprintf(c.sponsor, sizeof(c.sponsor), "%s", fields[4]);
    snprintf(c.model, sizeof(c.model), "%s", fields[5]);
    snprintf(c.endpoint, sizeof(c.endpoint), "%s", fields[6]);
    snprintf(c.capabilities, sizeof(c.capabilities), "%s", fields[7]);
    snprintf(c.note, sizeof(c.note), "%s", fields[8]);

    if (!wubu_agent_aid_valid(c.aid)) return -1;
    /* the AID's class/seq must agree with the row's own fields */
    char expect[WUBU_AGENT_AID_MAX];
    if (wubu_agent_format_aid(c.class_code, c.yy, c.seq, expect, sizeof(expect)) != 0) return -1;
    if (strcmp(expect, c.aid) != 0) return -1;

    memcpy(&g_card, &c, sizeof(g_card));
    g_enroled = 1;
    return 0;
}

const WubuAgentCard *wubu_agent_card(void) { return g_enroled ? &g_card : NULL; }
bool wubu_agent_is_enroled(void)           { return g_enroled != 0; }

/* -- seal + namespace -------------------------------------------------- */

int wubu_agent_seal(char *hex_out, size_t cap)
{
    if (!hex_out || cap < 2 * WUBU_SHA256_SZ + 1 || !g_enroled) return -1;
    char line[1024];
    card_canonical(line, sizeof(line));
    uint8_t digest[WUBU_SHA256_SZ];
    if (wubu_sha256(line, strlen(line), digest) != 0) return -1;
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < WUBU_SHA256_SZ; i++) {
        hex_out[2 * i]     = hex[digest[i] >> 4];
        hex_out[2 * i + 1] = hex[digest[i] & 0x0F];
    }
    hex_out[2 * WUBU_SHA256_SZ] = '\0';
    g_seal_ok = 1;
    return 0;
}

int wubu_ns_publish_agent(void)
{
    if (!g_enroled) return -1;
    if (ns_mkdir("agent") != 0) return -1;

    char seal[80];
    if (wubu_agent_seal(seal, sizeof(seal)) != 0) return -1;

    char sub[128], buf[512];
    snprintf(sub, sizeof(sub), "agent/aid");          snprintf(buf, sizeof(buf), "%s\n", g_card.aid);
    ns_write(sub, buf);
    snprintf(sub, sizeof(sub), "agent/class");       snprintf(buf, sizeof(buf), "%s\n", g_card.class_code);
    ns_write(sub, buf);
    snprintf(sub, sizeof(sub), "agent/sponsor");     snprintf(buf, sizeof(buf), "%s\n", g_card.sponsor);
    ns_write(sub, buf);
    snprintf(sub, sizeof(sub), "agent/model");       snprintf(buf, sizeof(buf), "%s\n", g_card.model);
    ns_write(sub, buf);
    snprintf(sub, sizeof(sub), "agent/endpoint");    snprintf(buf, sizeof(buf), "%s\n", g_card.endpoint);
    ns_write(sub, buf);
    snprintf(sub, sizeof(sub), "agent/capabilities"); snprintf(buf, sizeof(buf), "%s\n", g_card.capabilities);
    ns_write(sub, buf);
    snprintf(sub, sizeof(sub), "agent/note");        snprintf(buf, sizeof(buf), "%s\n", g_card.note);
    ns_write(sub, buf);
    snprintf(sub, sizeof(sub), "agent/seal");        snprintf(buf, sizeof(buf), "%s\n", seal);
    ns_write(sub, buf);

    /* the federation card shape: one JSON object per agent */
    snprintf(sub, sizeof(sub), "agent/card");
    snprintf(buf, sizeof(buf),
             "{\n"
             "  \"schema\": \"wubu-agent-card-v1\",\n"
             "  \"aid\": \"%s\",\n"
             "  \"class\": \"%s\",\n"
             "  \"sponsor\": \"%s\",\n"
             "  \"model\": \"%s\",\n"
             "  \"endpoint\": \"%s\",\n"
             "  \"capabilities\": \"%s\",\n"
             "  \"note\": \"%s\",\n"
             "  \"seal\": \"%s\"\n"
             "}\n",
             g_card.aid, g_card.class_code, g_card.sponsor, g_card.model,
             g_card.endpoint, g_card.capabilities, g_card.note, seal);
    ns_write(sub, buf);
    return 0;
}
