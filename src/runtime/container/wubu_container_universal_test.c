/*
 * wubu_container_universal_test.c -- the universal container must be honestly
 * universal: every kind -- including one a caller registers -- opens, mounts,
 * lists, reads and writes through the same five ops, and an unknown kind is
 * refused (an error, never a fallback).  That is the slerm of the ethos.
 */
#include "../../egojar.h"
#include "wubu_container_universal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, checks;
#define CHECK(c, m) do { checks++; if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

/* A brand-new kind, registered by the test -- the universality proof. */
typedef struct { char note[64]; } wcu_custom;

static int custom_open(const char *source, wubu_container_universal_t **out)
{
    wcu_custom *k = (wcu_custom *)calloc(1, sizeof(*k));
    if (!k) return -1;
    snprintf(k->note, sizeof(k->note), "custom:%s", source ? source : "none");
    return wu_container_handle_new("custom", k, out);
}
static void custom_close(wubu_container_universal_t *c) { free(wu_container_backend(c)); free(c); }
static int custom_stat(wubu_container_universal_t *c, const char *p,
                       uint64_t *size, uint32_t *isdir)
{
    wcu_custom *k = (wcu_custom *)wu_container_backend(c);
    if (strcmp(p, "/note") != 0) return -1;
    if (size) *size = strlen(k->note);
    if (isdir) *isdir = 0;
    return 0;
}
static int custom_read(wubu_container_universal_t *c, const char *p,
                       uint8_t *buf, uint32_t sz)
{
    wcu_custom *k = (wcu_custom *)wu_container_backend(c);
    if (strcmp(p, "/note") != 0) return -1;
    uint32_t n = sz < strlen(k->note) ? sz : (uint32_t)strlen(k->note);
    memcpy(buf, k->note, n);
    return (int)n;
}
static const wubu_container_adapter_t g_custom = {
    "custom", custom_open, custom_close, NULL, custom_stat, NULL, custom_read, NULL
};

int main(void)
{
    printf("=== test_wubu_container_universal ===\n");

    /* -- builtin kinds enumerate -- */
    char kinds[WCU_MAX_KINDS][WCU_KIND_LEN];
    int nk = wu_container_kinds(kinds, WCU_MAX_KINDS);
    CHECK(nk >= 2, "at least the ram and dir kinds are registered");
    int has_ram = 0, has_dir = 0;
    for (int i = 0; i < nk; i++) {
        if (strcmp(kinds[i], "ram") == 0) has_ram = 1;
        if (strcmp(kinds[i], "dir") == 0) has_dir = 1;
    }
    CHECK(has_ram && has_dir, "ram and dir are both present");

    /* -- RAM container, write/read through the uniform ops -- */
    wubu_container_universal_t *ram = NULL;
    CHECK(wu_container_open("ram", "epoch", &ram) == 0, "a ram container opens");
    CHECK(ram && strcmp(wu_container_kind(ram), "ram") == 0,
          "its kind reads back as ram");
    CHECK(wu_container_mount(ram, "/mnt/ram") == 0, "it mounts through the interface");

    const char *hello = "hello universal container";
    CHECK(wu_container_put(ram, "/data.txt", (const uint8_t *)hello,
                           (uint32_t)strlen(hello)) == 0, "a file is written");
    char nbuf[WCU_MAX_CHILDREN][WCU_PATH_LEN];
    int nl = wu_container_list(ram, "/", nbuf, WCU_MAX_CHILDREN);
    CHECK(nl == 1 && strcmp(nbuf[0], "/data.txt") == 0,
          "list returns the written file");

    uint64_t size; uint32_t isdir;
    CHECK(wu_container_stat(ram, "/data.txt", &size, &isdir) == 0,
          "the file stats");
    CHECK(size == strlen(hello) && isdir == 0, "and the size is exact");

    char buf[256];
    int rr = wu_container_read(ram, "/data.txt", (uint8_t *)buf,
                               (uint32_t)sizeof(buf));
    CHECK(rr == (int)size && memcmp(buf, hello, size) == 0,
          "read returns exactly what was written");

    /* overwrite through the same op */
    CHECK(wu_container_put(ram, "/data.txt", (const uint8_t *)"two",
                           (uint32_t)3) == 0, "a file is overwritten");
    CHECK(wu_container_read(ram, "/data.txt", (uint8_t *)buf, 8) == 3 &&
          memcmp(buf, "two", 3) == 0, "the new content reads back");
    wu_container_close(ram);

    /* -- DIR container over a real host directory -- */
    mkdir("/tmp/wcu_dir_a", 0755);
    wubu_container_universal_t *dir = NULL;
    CHECK(wu_container_open("dir", "/tmp/wcu_dir_a", &dir) == 0,
          "a host-directory container opens");
    CHECK(wu_container_put(dir, "note.txt", (const uint8_t *)"from dir",
                           (uint32_t)8) == 0, "a file is written into the host dir");
    int nd = wu_container_list(dir, "", nbuf, WCU_MAX_CHILDREN);
    CHECK(nd == 1 && strcmp(nbuf[0], "note.txt") == 0,
          "the host dir is listed through the interface");
    int nb = wu_container_read(dir, "note.txt", (uint8_t *)buf, 32);
    CHECK(nb == 8 && memcmp(buf, "from dir", 8) == 0,
          "the host file reads through the interface");
    wu_container_close(dir);

    /* -- the extensibility promise: a brand-new kind registers and works -- */
    CHECK(wu_container_register(&g_custom) == 0, "a NEW kind can be registered");
    CHECK(wu_container_register(&g_custom) != 0,
          "registering it twice is refused (no silent override)");
    wubu_container_universal_t *cust = NULL;
    CHECK(wu_container_open("custom", "star", &cust) == 0,
          "the new kind opens through the SAME interface");
    char cb[64];
    CHECK(cust && wu_container_stat(cust, "/note", &size, &isdir) == 0 &&
          wu_container_read(cust, "/note", (uint8_t *)cb, sizeof(cb)) == 11 &&
          memcmp(cb, "custom:star", 11) == 0,
          "and is read through the SAME ops as ram and dir");
    wu_container_close(cust);

    /* -- an unknown kind is an error, never a fallback -- */
    wubu_container_universal_t *bad = NULL;
    CHECK(wu_container_open("nope", "x", &bad) != 0,
          "an unknown kind is REFUSED, not half-served");
    CHECK(bad == NULL, "and yields no handle");
    CHECK(wu_container_open(NULL, "x", &bad) != 0, "a NULL kind is refused");

    if (failures == 0) printf("=== wcu: %d checks, 0 failed ===\n", checks);
    else printf("=== wcu: %d failed of %d ===\n", failures, checks);
    return failures ? 1 : 0;
}
