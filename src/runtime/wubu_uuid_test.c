/*
 * wubu_uuid_test.c — Test UUIDv7 generation and parsing.
 */
#include "wubu_uuid.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void) {
    char uuid[37];
    int errors = 0;

    /* Test 1: Generate UUIDv7, verify format */
    char *result = wubu_uuid_v7(uuid, sizeof(uuid));
    if (!result) { printf("FAIL: uuid_v7 returned NULL\n"); return 1; }
    printf("Generated: %s\n", uuid);
    /* UUIDv7 format: 8-4-4-4-12, version nibble = 7 */
    if (strlen(uuid) != 36) { printf("FAIL: wrong length %zu\n", strlen(uuid)); errors++; }
    if (uuid[8] != '-') { printf("FAIL: missing hyphen at 8\n"); errors++; }
    if (uuid[13] != '-') { printf("FAIL: missing hyphen at 13\n"); errors++; }
    if (uuid[14] != '7') { printf("FAIL: version not 7 (got %c)\n", uuid[14]); errors++; }
    /* Variant bits: char at 19 should be 8-b */
    char vc = uuid[19];
    if (vc < '8' || vc > 'b') { printf("FAIL: variant bits wrong (%c)\n", vc); errors++; }
    printf("Test 1 - UUIDv7 format: %s\n", errors ? "FAIL" : "PASS");

    /* Test 2: Sequential UUIDs are lexicographically ordered */
    char uuid1[37], uuid2[37];
    wubu_uuid_v7(uuid1, sizeof(uuid1));
    wubu_uuid_v7(uuid2, sizeof(uuid2));
    if (strcmp(uuid1, uuid2) < 0) {
        printf("Test 2 - Lexicographic order: PASS (sequential)\n");
    } else {
        printf("Test 2 - Lexicographic order: FAIL\n");
        errors++;
    }

    /* Test 3: Timestamp extraction */
    int64_t ts = wubu_uuid_v7_timestamp(uuid);
    if (ts > 0) {
        /* Should be within last few seconds */
        int64_t now = (int64_t)time(NULL) * 1000;
        if (ts <= now + 2000 && ts > now - 10000) {
            printf("Test 3 - Timestamp extraction: PASS (%lld ms)\n", (long long)ts);
        } else {
            printf("Test 3 - Timestamp out of range: ts=%lld now=%lld\n",
                   (long long)ts, (long long)now);
            errors++;
        }
    } else {
        printf("Test 3 - Timestamp extraction: FAIL (ts=%lld)\n", (long long)ts);
        errors++;
    }

    /* Test 4: Parse UUID components */
    int64_t parsed_ts;
    uint16_t parsed_ra;
    uint64_t parsed_rb;
    int parsed_ver;
    if (wubu_uuid_parse(uuid, &parsed_ts, &parsed_ra, &parsed_rb, &parsed_ver) == 0) {
        if (parsed_ver == 7 && parsed_ts == ts) {
            printf("Test 4 - Parse UUIDv7: PASS (ver=%d, ts=%lld)\n",
                   parsed_ver, (long long)parsed_ts);
        } else {
            printf("Test 4 - Parse: FAIL ver=%d ts=%lld\n", parsed_ver, (long long)parsed_ts);
            errors++;
        }
    } else {
        printf("Test 4 - Parse failed\n");
        errors++;
    }

    printf("\n=== UUID tests: %d errors ===\n", errors);
    return errors;
}
