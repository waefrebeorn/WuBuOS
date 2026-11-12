/*
 * wubu_agent_test.c -- prove the OS-side agent identity against the real
 * federation contract (agent-protocol.json): the AID arithmetic, the
 * no-anonymous-standing rule, the card round-trip, a deterministic SHA-256
 * seal, and the /n/agent subtree.
 */
#include "wubu_agent.h"
#include "wubu_ns_bridge_internal.h"
#include "../kernel/wubu_sha256.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#define NSROOT "/tmp/wubu_agent_ns"

static int failures;
static int checks;
#define CHECK(c, m) do { \
    checks++; \
    if (!(c)) { printf("  FAIL: %s\n", m); failures++; } \
} while (0)

static void fill(WubuAgentCard *c, const char *cls, int yy, int seq)
{
    memset(c, 0, sizeof(*c));
    snprintf(c->class_code, sizeof(c->class_code), "%s", cls);
    c->yy = yy;
    c->seq = seq;
    snprintf(c->sponsor, sizeof(c->sponsor), "WID-WB-26-0001-8");
    snprintf(c->model, sizeof(c->model), "wubuos/hosted");
    snprintf(c->endpoint, sizeof(c->endpoint), "a2a://127.0.0.1:8080");
    snprintf(c->capabilities, sizeof(c->capabilities), "govgen.audit,govgen.seal");
    snprintf(c->note, sizeof(c->note), "the hosted body");
}

int main(void)
{
    printf("\n=== agent identity ===\n");

    /* -- the AID arithmetic: mod 11, weights 7,6,5,4,3,2 over YYNNNN -- */
    CHECK(wubu_agent_check_digit(26, 1)    == '8', "check digit 26/0001 -> 8");
    CHECK(wubu_agent_check_digit(26, 2)    == 'X', "check digit 26/0002 -> X");
    CHECK(wubu_agent_check_digit(26, 10)   == '9', "check digit 26/0010 -> 9");
    CHECK(wubu_agent_check_digit(99, 9999) == '1', "check digit 99/9999 -> 1");
    CHECK(wubu_agent_check_digit(26, 10000) == -1, "a 5-digit sequence is rejected");

    char aid[WUBU_AGENT_AID_MAX];
    CHECK(wubu_agent_format_aid("IN", 26, 1, aid, sizeof(aid)) == 0, "format an instrument AID");
    CHECK(strcmp(aid, "AID-IN-26-0001-8") == 0, "the AID reads AID-IN-26-0001-8");
    CHECK(wubu_agent_aid_valid(aid) == 1, "the AID validates");
    CHECK(wubu_agent_aid_valid("AID-IN-26-0001-9") == 0, "a wrong check digit is rejected");
    CHECK(wubu_agent_aid_valid("AID-XX-26-0001-8") == 0, "an unknown class is rejected");
    CHECK(wubu_agent_aid_valid("AID-WI-26-0002-X") == 1, "a witness AID validates");

    /* -- enrolment: no anonymous standing -- */
    WubuAgentCard c;
    fill(&c, "IN", 26, 1);
    CHECK(wubu_agent_enrol(&c) == 0, "enrol accepts a sponsored card");
    CHECK(wubu_agent_is_enroled(), "the enroled flag is set");
    CHECK(strcmp(wubu_agent_card()->aid, "AID-IN-26-0001-8") == 0, "the AID is assigned");

    WubuAgentCard bad = c;
    bad.sponsor[0] = '\0';
    CHECK(wubu_agent_enrol(&bad) == -1, "enrol rejects an anonymous card");
    fill(&bad, "ZZ", 26, 1);
    CHECK(wubu_agent_enrol(&bad) == -1, "enrol rejects an unknown class");
    fill(&bad, "IN", 26, 1);
    snprintf(bad.model, sizeof(bad.model), "a|b");
    CHECK(wubu_agent_enrol(&bad) == -1, "enrol rejects a field containing the separator");

    /* re-enrol the good card (the rejected attempts must not clobber it) */
    CHECK(wubu_agent_enrol(&c) == 0, "the good card is enroled again");

    /* -- the seal: deterministic, 64 lowercase hex -- */
    char seal1[80], seal2[80];
    CHECK(wubu_agent_seal(seal1, sizeof(seal1)) == 0, "the seal renders");
    CHECK(strlen(seal1) == 2 * WUBU_SHA256_SZ, "the seal is 64 hex chars");
    int hex_ok = 1;
    for (const char *p = seal1; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) hex_ok = 0;
    CHECK(hex_ok, "the seal is lowercase hex");
    CHECK(wubu_agent_seal(seal2, sizeof(seal2)) == 0 && strcmp(seal1, seal2) == 0,
          "the seal is deterministic for the same card");

    /* -- persistence round-trip -- */
    char path[300];
    snprintf(path, sizeof(path), "%s/card.txt", NSROOT);
    CHECK(wubu_agent_save(path) == 0, "the card saves");
    WubuAgentCard other;
    fill(&other, "WI", 26, 42);
    CHECK(wubu_agent_enrol(&other) == 0, "a second card is enroled");
    CHECK(wubu_agent_load(path) == 0, "the saved card loads");
    CHECK(strcmp(wubu_agent_card()->aid, "AID-IN-26-0001-8") == 0,
          "the card survives the round-trip");
    CHECK(strcmp(wubu_agent_card()->sponsor, "WID-WB-26-0001-8") == 0,
          "the sponsor survives the round-trip");
    CHECK(wubu_agent_load("/no/such/card.txt") == -1, "a missing card is reported");

    /* -- the /n/agent subtree -- */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", NSROOT);
    if (system(cmd) != 0) { /* best effort */ }
    mkdir(NSROOT, 0755);
    *(const char **)&g_ns_root = NSROOT;

    CHECK(wubu_ns_publish_agent() == 0, "/n/agent publishes");
    char fpath[300], buf[512];
    snprintf(fpath, sizeof(fpath), "%s/agent/aid", NSROOT);
    FILE *f = fopen(fpath, "r");
    CHECK(f != NULL, "/n/agent/aid exists");
    if (f) {
        if (fgets(buf, sizeof(buf), f)) {
            buf[strcspn(buf, "\r\n")] = '\0';
            CHECK(strcmp(buf, "AID-IN-26-0001-8") == 0, "/n/agent/aid carries the AID");
        } else {
            CHECK(0, "/n/agent/aid is not empty");
        }
        fclose(f);
    }
    snprintf(fpath, sizeof(fpath), "%s/agent/sponsor", NSROOT);
    f = fopen(fpath, "r");
    CHECK(f != NULL, "/n/agent/sponsor exists");
    if (f) {
        if (fgets(buf, sizeof(buf), f)) {
            buf[strcspn(buf, "\r\n")] = '\0';
            CHECK(strcmp(buf, "WID-WB-26-0001-8") == 0, "/n/agent/sponsor carries the sponsor");
        }
        fclose(f);
    }
    snprintf(fpath, sizeof(fpath), "%s/agent/card", NSROOT);
    f = fopen(fpath, "r");
    CHECK(f != NULL, "/n/agent/card exists");
    if (f) {
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);
        CHECK(strstr(buf, "\"schema\": \"wubu-agent-card-v1\"") != NULL,
              "/n/agent/card is the federation card shape");
        CHECK(strstr(buf, "AID-IN-26-0001-8") != NULL, "/n/agent/card carries the AID");
    }

    printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
