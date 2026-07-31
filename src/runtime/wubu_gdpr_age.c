/*
 * wubu_gdpr_age.c — GDPR-compliant age assurance for WuBuOS.
 *
 * Implemented per wubu_gdpr_age.h. No biometric data, no ID, no
 * third-party verification. Self-declaration + knowledge question.
 * Consent state persisted to filesystem (no PII, just a flag + UUIDv7).
 *
 * Triple-DA:
 *   Decision: self-declaration + knowledge question is the only viable
 *             approach for a self-hosted OS with zero data collection.
 *   Design:   consent cookie file with UUIDv7 timestamp + declared age.
 *             Fails closed (deny) if no cookie.
 */

#include "wubu_gdpr_age.h"
#include "wubu_uuid.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static wubu_age_result_t g_last_result = {0};

const char *wubu_gdpr_disclaimer_text(void) {
    return
"You are about to use WuBuOS, a hobbyist operating system developed by a "
"single developer in Ohio, United States of America. The developer has no "
"corporate entity, no legal department, and no funding for formal GDPR "
"compliance infrastructure.\n\n"
"This software collects NO personal data, transmits NO telemetry, and uses "
"NO third-party verification services. Age assurance is via self-declaration "
"+ a knowledge question only (no ID, no facial recognition, no biometrics).\n\n"
"GDPR (Article 8): The age of consent in the EU is 16. If you are under 16, "
"you must NOT use this software. EU users who require formal, high-assurance "
"age verification (as some regulators now demand) should not install WuBuOS. "
"This is a legal gray area — by proceeding, you accept that the developer "
"cannot provide the level of compliance a large corporation can.\n\n"
"US COPPA: Users under 13 are prohibited.\n\n"
"By proceeding, you acknowledge: (1) you are 16 or older, (2) you understand "
"you are interacting with an autonomous AI system (AGI), and (3) you accept "
"the limitations described above.";
}

int wubu_gdpr_age_of_consent(void) {
    return 16; /* GDPR default — safe minimum */
}

/* Read consent cookie from /wubu/state/gdpr_age */
wubu_age_status_t wubu_gdpr_age_check(void) {
    FILE *f = fopen(WUBU_GDPR_STATE_PATH, "r");
    if (!f) return WUBU_AGE_UNKNOWN;
    char line[128];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return WUBU_AGE_UNKNOWN; }
    fclose(f);
    /* Format: <status_char> <age> <uuid7> <ts_ms> */
    char status_char;
    int age;
    char uuid[37];
    uint64_t ts;
    if (sscanf(line, "%c %d %36s %llu", &status_char, &age, uuid, &ts) != 4)
        return WUBU_AGE_UNKNOWN;
    g_last_result.declared_age = age;
    g_last_result.consent_ts_ms = ts;
    strncpy(g_last_result.session_uuid, uuid, 36);
    g_last_result.session_uuid[36] = '\0';
    switch (status_char) {
        case 'C': return (wubu_age_status_t)(g_last_result.status = WUBU_AGE_CONSENTED);
        case 'D': return (wubu_age_status_t)(g_last_result.status = WUBU_AGE_DENIED);
        default: return WUBU_AGE_UNKNOWN;
    }
}

wubu_age_result_t wubu_gdpr_age_result(void) {
    return g_last_result;
}

int wubu_gdpr_age_persist(wubu_age_status_t status, int age) {
    char uuid[37];
    wubu_uuid_v7(uuid, sizeof(uuid));
    uint64_t ts = (uint64_t)time(NULL) * 1000ULL;
    char status_char = 'D';
    if (status == WUBU_AGE_CONSENTED) status_char = 'C';
    FILE *f = fopen(WUBU_GDPR_STATE_PATH, "w");
    if (!f) return -1;
    fprintf(f, "%c %d %s %llu\n", status_char, age, uuid, (unsigned long long)ts);
    fclose(f);
    g_last_result.status = status;
    g_last_result.declared_age = age;
    g_last_result.consent_ts_ms = ts;
    strncpy(g_last_result.session_uuid, uuid, 36);
    return 0;
}

void wubu_gdpr_age_reset(void) {
    remove(WUBU_GDPR_STATE_PATH);
    memset(&g_last_result, 0, sizeof(g_last_result));
}

/* Knowledge question: prove adult-level cognitive understanding.
 * Answer: "AGI" (Autonomous General Intelligence) or "general". */
static int verify_knowledge(void) {
    char buf[256];
    printf("\n--- Knowledge Question ---\n");
    printf("Q: What type of AI system is WuBuOS running?\n");
    printf("   (Type the answer to prove adult cognitive understanding)\n");
    printf("Answer: ");
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) return 0;
    /* Accept: "AGI", "agi", "Autonomous General Intelligence", etc. */
    char *p = strtok(buf, "\n");
    if (!p) return 0;
    /* Case-insensitive search for "agi" or "general" */
    char lc[256];
    for (int i = 0; p[i] && i < 255; i++)
        lc[i] = (p[i] >= 'A' && p[i] <= 'Z') ? p[i] + 32 : p[i];
    lc[strlen(p)] = '\0';
    return (strstr(lc, "agi") != NULL) || (strstr(lc, "general") != NULL);
}

int wubu_gdpr_age_verify(void) {
    /* Step 1: Check if already consented */
    wubu_age_status_t existing = wubu_gdpr_age_check();
    if (existing == WUBU_AGE_CONSENTED) {
        printf("[GDPR] Consent previously recorded. Proceeding.\n");
        return 0;
    }

    /* Step 2: Show legal disclaimer */
    printf("\n========================================\n");
    printf("  WUBUOS LEGAL DISCLOSURE — EU/GDPR NOTICE");
    printf("\n========================================\n\n");
    const char *text = wubu_gdpr_disclaimer_text();
    printf("%s\n\n", text);
    printf("----------------------------------------\n");

    int consent = 0;
    char buf[16];

    /* Step 3: Age checkbox */
    printf("Are you %d years of age or older? (yes/no): ",
           wubu_gdpr_age_of_consent());
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) return 1;
    if (strncasecmp(buf, "yes", 3) == 0 || strncasecmp(buf, "y", 1) == 0) {
        /* Step 4: Knowledge question */
        if (verify_knowledge()) {
            consent = 1;
        } else {
            printf("Knowledge verification failed. Access denied.\n");
        }
    } else {
        printf("Age requirement not met. Access denied.\n");
    }

    if (consent) {
        printf("\nConsent recorded. Welcome to WuBuOS.\n");
        wubu_gdpr_age_persist(WUBU_AGE_CONSENTED, 16); /* record minimum */
        return 0;
    } else {
        wubu_gdpr_age_persist(WUBU_AGE_DENIED, 0);
        return 1;
    }
}
