/* wubu_atom_test.c -- gate for the SteamOS ATOM A/B auto-rollback spine.
 * Simulates: a successful staged update, and a staging that boots three times
 * without success -> the marker votes to revert to the sibling slot. */
#include "wubu_atom.h"
#include <stdio.h>
#include <string.h>

static int fails = 0, checks = 0;
#define OK(c, m) do { checks++; if (!(c)) { printf("FAIL %s\n", m); fails++; } } while (0)

int main(void)
{
    wubu_atom_t a;

    /* 1. Init: slot 0 current, both idle, marker intact. */
    OK(wubu_atom_init(&a) == 0, "init");
    OK(a.magic == WUBU_ATOM_MAGIC && a.current == 0, "slot 0 is current");
    OK(a.var[0].state == WUBU_ATOM_IDLE && a.var[1].state == WUBU_ATOM_IDLE, "both idle");
    OK(wubu_atom_marker_valid(&a), "fresh marker is valid");

    /* 2. Stage an update onto the inactive slot (1). */
    OK(wubu_atom_begin_update(&a) == 1, "begin targets the inactive slot 1");
    OK(a.var[1].state == WUBU_ATOM_PENDING && a.current == 0, "slot 1 pending, 0 still current");

    /* Layering guard: a second stage over a pending slot is refused. */
    OK(wubu_atom_begin_update(&a) == -1, "cannot stage over an already-pending slot");

    /* 3. Commit: the next boot votes on slot 1. */
    OK(wubu_atom_commit(&a) == 0, "commit");
    OK(a.current == 1, "current flipped to the pending slot 1");

    /* 4. Boot, and the update is good. */
    OK(wubu_atom_boot(&a) == 0 && a.var[1].tries == 1, "boot counts an attempt on slot 1");
    OK(wubu_atom_boot_ok(&a) == 0, "boot success");
    OK(a.current == 1 && a.var[1].state == WUBU_ATOM_IDLE && a.var[0].state == WUBU_ATOM_IDLE,
       "the voted slot is good, sibling settled");

    /* 5. Stage another update onto the now-inactive slot (0), commit, then let
     *    it boot WITHOUT success over the full trial budget -> auto-revert. */
    OK(wubu_atom_begin_update(&a) == 0, "revert-update targets slot 0");
    OK(wubu_atom_commit(&a) == 0, "commit revert-update");
    OK(a.current == 0, "current now slot 0 (the pending one)");
    int t;
    for (int i = 0; i < WUBU_ATOM_MAX_TRIES; i++)
        OK(wubu_atom_boot(&a) == 0, "failing boot attempt");
    t = wubu_atom_rollback_target(&a);
    OK(t == 1, "after N failed boots the marker votes to boot slot 1");
    /* Within budget there is no revert. */
    OK(wubu_atom_begin_update(&a) == 1, "re-arm: stage slot 1 again");
    OK(wubu_atom_commit(&a) == 0, "commit re-arm");
    OK(wubu_atom_boot(&a) == 0, "one boot, still inside budget");
    OK(wubu_atom_rollback_target(&a) == -1, "not yet past the trial budget");

    /* 6. Marker integrity: a torn write flips the CRC -> refused. */
    wubu_atom_t t2;
    OK(wubu_atom_init(&t2) == 0, "re-init");
    wubu_atom_t copy = t2;                 /* intact */
    OK(wubu_atom_marker_valid(&copy), "copy of a clean marker is valid");
    uint8_t *b = (uint8_t *)&t2.var;
    b[0] ^= 0xFF;                          /* corrupt the first slot record */
    OK(!wubu_atom_marker_valid(&t2), "corrupt marker fails close");

    printf(fails ? "FAIL %d\n" : "PASS %d checks\n", fails ? fails : checks);
    return fails ? 1 : 0;
}