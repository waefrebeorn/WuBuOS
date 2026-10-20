/*
 * test_styx_names.c -- the 9P2000 message-name registry (GOALS-1000 #721:
 * styx_names had no test target). The standard table is the SEED; the
 * live registry allows runtime registration (the Revolver Doctrine:
 * future dialect messages get names without a recompile).
 */
#include "styx.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

/* the 28 seeded 9P2000 names, in wire order */
static const struct { uint8_t type; const char *name; } kSeed[] = {
    {100,"Tversion"}, {101,"Rversion"}, {102,"Tauth"},    {103,"Rauth"},
    {104,"Tattach"},  {105,"Rattach"},  {106,"Terror"},   {107,"Rerror"},
    {108,"Tflush"},   {109,"Rflush"},   {110,"Twalk"},    {111,"Rwalk"},
    {112,"Topen"},    {113,"Ropen"},    {114,"Tcreate"},  {115,"Rcreate"},
    {116,"Tread"},    {117,"Rread"},    {118,"Twrite"},   {119,"Rwrite"},
    {120,"Tclunk"},   {121,"Rclunk"},   {122,"Tremove"},  {123,"Rremove"},
    {124,"Tstat"},    {125,"Rstat"},   {126,"Twstat"},   {127,"Rwstat"},
};

int main(void)
{
    /* every seeded type resolves to its exact 9P2000 name */
    for (unsigned i = 0; i < sizeof(kSeed)/sizeof(kSeed[0]); i++) {
        const char *n = styx_msg_name(kSeed[i].type);
        if (!n || strcmp(n, kSeed[i].name) != 0) {
            fails++;
            printf("FAIL: type %u -> '%s' (want '%s') line %d\n",
                   kSeed[i].type, n ? n : "(null)", kSeed[i].name, __LINE__);
        }
    }

    /* an unmapped type is 'Unknown', never NULL, never garbage */
    CHECK(strcmp(styx_msg_name(50), "Unknown") == 0);
    CHECK(strcmp(styx_msg_name(200), "Unknown") == 0);
    CHECK(strcmp(styx_msg_name(0), "Unknown") == 0);

    /* runtime registration names a previously-unknown type */
    CHECK(styx_msg_name_register(50, "Tcustom") == 0);
    CHECK(strcmp(styx_msg_name(50), "Tcustom") == 0);

    /* registration can OVERRIDE a seeded name (the live registry wins) */
    CHECK(styx_msg_name_register(114, "Tcreate9P2000u") == 0);
    CHECK(strcmp(styx_msg_name(114), "Tcreate9P2000u") == 0);
    /* restore the seed so later consumers see the standard name */
    CHECK(styx_msg_name_register(114, "Tcreate") == 0);
    CHECK(strcmp(styx_msg_name(114), "Tcreate") == 0);

    /* bad registrations are refused; every uint8 type is registrable
     * (STYX_MSG_REGISTRY_MAX spans the full 0..255 wire range) */
    CHECK(styx_msg_name_register(50, NULL) == -1);
    CHECK(styx_msg_name_register(255, "Tcustom255") == 0);
    CHECK(strcmp(styx_msg_name(255), "Tcustom255") == 0);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
