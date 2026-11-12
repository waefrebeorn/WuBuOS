/*
 * wubu_spellbook_test.c -- the spell book must be honest about its contents.
 *
 * Every assertion here is about a property the ORGANIZATION depends on, not
 * about the parser's internals: a duplicate name must be visible, a missing
 * source must be visible, two spells claiming one capability must be a
 * conflict, casting must resolve dependencies in order and must fail closed
 * when a need is unmet.
 */
#include "../egojar.h"
#include "wubu_spell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static int checks;
#define CHECK(c, m) do { \
    checks++; \
    if (!(c)) { printf("  FAIL: %s\n", m); failures++; } \
} while (0)

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(2); }
    fputs(text, f);
    fclose(f);
}

static void mkdir_p(const char *path)
{
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

#define ROOT "/tmp/wubu_spellbook_test"

static void build_book(void)
{
    mkdir_p(ROOT "/graphics/render");
    mkdir_p(ROOT "/graphics/window");
    mkdir_p(ROOT "/networking/relay");

    /* a real spell: the graphics translation layer */
    write_file(ROOT "/graphics/render/vkd3d.spell",
        "# the D3D12 -> Vulkan translation layer\n"
        "spell vkd3d\n"
        "version 1.0.0\n"
        "book graphics\n"
        "genre render\n"
        "origin third-party\n"
        "provides d3d12-on-vulkan\n"
        "provides d3d12-device\n"
        "needs vulkan-driver\n"
        "source wubu_vkd3d.c\n"
        "source wubu_vkd3d_heap.c\n"
        "test wubu_vkd3d_test.c\n");
    write_file(ROOT "/graphics/render/wubu_vkd3d.c", "/* module */\n");
    write_file(ROOT "/graphics/render/wubu_vkd3d_heap.c", "/* module */\n");
    write_file(ROOT "/graphics/render/wubu_vkd3d_test.c", "/* test */\n");

    /* a second spell that provides what the first one needs */
    write_file(ROOT "/graphics/window/vulkan.spell",
        "spell vulkan\n"
        "version 1.4.351\n"
        "book graphics\n"
        "genre window\n"
        "origin first-party\n"
        "provides vulkan-driver\n"
        "provides surface\n");
    write_file(ROOT "/graphics/window/wubu_vulkan.c", "/* module */\n");

    /* an UNSATISFIED need: nothing provides 'wine' */
    write_file(ROOT "/networking/relay/bridge.spell",
        "spell bridge\n"
        "version 0.1.0\n"
        "book networking\n"
        "genre relay\n"
        "origin third-party\n"
        "provides win32-bridge\n"
        "needs wine\n");
    write_file(ROOT "/networking/relay/wubu_bridge.c", "/* module */\n");
}

static void test_scan_and_layout(void)
{
    wubu_spellbook_t *b = wubu_spellbook_open(ROOT);
    CHECK(b != NULL, "the book opens");
    if (!b) return;

    CHECK(wubu_spellbook_error_count(b) == 0,
          "a well-formed book has no errors");
    CHECK(wubu_spellbook_count(b) == 3, "all three spells are found");

    const wubu_spell_t *s = wubu_spellbook_find(b, "vkd3d");
    CHECK(s != NULL, "a spell is found by name");
    if (s) {
        CHECK(strcmp(s->book, "graphics") == 0, "the book is read");
        CHECK(strcmp(s->genre, "render") == 0, "the genre is read");
        CHECK(strcmp(s->origin, "third-party") == 0, "the origin is read");
        CHECK(s->provides_count == 2, "both capabilities are read");
        CHECK(s->needs_count == 1, "the need is read");
        CHECK(s->sources_count == 2, "both sources are read");
    }

    /* listing by book and genre */
    const wubu_spell_t *out[16];
    CHECK(wubu_spellbook_list(b, "graphics", NULL, out, 16) == 2,
          "the graphics book lists two spells");
    CHECK(wubu_spellbook_list(b, "graphics", "render", out, 16) == 1,
          "the render genre lists one spell");
    CHECK(wubu_spellbook_list(b, "networking", NULL, out, 16) == 1,
          "the networking book lists one spell");

    wubu_spellbook_close(b);
}

static void test_resolve_and_cast(void)
{
    wubu_spellbook_t *b = wubu_spellbook_open(ROOT);
    CHECK(b != NULL, "the book opens");
    if (!b) return;

    /* resolve */
    const wubu_spell_t *p = wubu_spellbook_resolve(b, "vulkan-driver");
    CHECK(p != NULL && strcmp(p->name, "vulkan") == 0,
          "a need resolves to its provider");
    CHECK(wubu_spellbook_resolve(b, "wine") == NULL,
          "an unprovided capability resolves to nothing");

    /* unmet needs are REPORTED, not silently dropped */
    CHECK(wubu_spellbook_unmet_count(b) == 1,
          "the one unmet need is reported");
    CHECK(wubu_spellbook_unmet_at(b, 0) &&
          strcmp(wubu_spellbook_unmet_at(b, 0), "wine") == 0,
          "the unmet need is named");

    /* casting vkd3d must first cast vulkan, because vkd3d needs it */
    CHECK(wubu_spellbook_cast(b, "vkd3d") == 0, "vkd3d casts");
    const wubu_spell_t *v = wubu_spellbook_find(b, "vulkan");
    const wubu_spell_t *k = wubu_spellbook_find(b, "vkd3d");
    CHECK(v && v->cast, "the dependency was cast first");
    CHECK(k && k->cast, "the dependent was cast");

    /* idempotent */
    CHECK(wubu_spellbook_cast(b, "vkd3d") == 0, "casting twice is fine");

    /* fail closed on an unmet need */
    size_t before = wubu_spellbook_error_count(b);
    CHECK(wubu_spellbook_cast(b, "bridge") != 0,
          "casting a spell with an unmet need FAILS");
    CHECK(wubu_spellbook_error_count(b) > before,
          "the failure is reported with a reason");

    /* unknown spell */
    CHECK(wubu_spellbook_cast(b, "nonexistent") != 0,
          "casting an unknown spell fails");

    wubu_spellbook_close(b);
}

static void test_duplicate_and_conflict(void)
{
    mkdir_p(ROOT "/dup");
    write_file(ROOT "/dup/a.spell",
        "spell same\nbook dup\ngenre x\nprovides cap-a\n");
    write_file(ROOT "/dup/b.spell",
        "spell same\nbook dup\ngenre x\nprovides cap-a\n");

    wubu_spellbook_t *b = wubu_spellbook_open(ROOT);
    CHECK(b != NULL, "the book opens");
    if (b) {
        /* the duplicate name and the shared capability are BOTH reported */
        bool dup = false, conf = false;
        for (size_t i = 0; i < wubu_spellbook_error_count(b); i++) {
            const char *e = wubu_spellbook_error_at(b, i);
            if (e && strstr(e, "duplicate spell name")) dup = true;
        }
        CHECK(dup, "a duplicate spell name is reported");
        CHECK(wubu_spellbook_conflict_count(b) >= 1,
              "two spells providing one capability is a CONFLICT");
        if (wubu_spellbook_conflict_count(b) >= 1) {
            const wubu_spell_conflict_t *c = wubu_spellbook_conflict_at(b, 0);
            CHECK(c && strcmp(c->capability, "cap-a") == 0,
                  "the conflict names the contested capability");
            CHECK(c && c->a && c->b, "the conflict names both spells");
            conf = true;
        }
        (void)conf;
        wubu_spellbook_close(b);
    }

    unlink(ROOT "/dup/a.spell");
    unlink(ROOT "/dup/b.spell");
    rmdir(ROOT "/dup");
}

static void test_missing_source_and_unknown_key(void)
{
    mkdir_p(ROOT "/bad");
    write_file(ROOT "/bad/broken.spell",
        "spell broken\nbook bad\ngenre x\n"
        "source does_not_exist.c\n"
        "frobnicate yes\n");

    wubu_spellbook_t *b = wubu_spellbook_open(ROOT);
    CHECK(b != NULL, "the book opens");
    if (b) {
        bool missing = false, unknown = false;
        for (size_t i = 0; i < wubu_spellbook_error_count(b); i++) {
            const char *e = wubu_spellbook_error_at(b, i);
            if (!e) continue;
            if (strstr(e, "is missing")) missing = true;
            if (strstr(e, "unknown key")) unknown = true;
        }
        CHECK(missing, "a declared source that does not exist is reported");
        CHECK(unknown, "an unknown key is reported");
        wubu_spellbook_close(b);
    }

    unlink(ROOT "/bad/broken.spell");
    rmdir(ROOT "/bad");
}

static void test_layout_mismatch(void)
{
    /* a spell filed under graphics/render that claims to be in audio/music */
    mkdir_p(ROOT "/graphics/render");
    write_file(ROOT "/graphics/render/misfiled.spell",
        "spell misfiled\nbook audio\ngenre music\n");

    wubu_spellbook_t *b = wubu_spellbook_open(ROOT);
    CHECK(b != NULL, "the book opens");
    if (b) {
        bool mismatch = false;
        for (size_t i = 0; i < wubu_spellbook_error_count(b); i++) {
            const char *e = wubu_spellbook_error_at(b, i);
            if (e && strstr(e, "filed under")) mismatch = true;
        }
        CHECK(mismatch, "a spell filed under the wrong book is reported");
        wubu_spellbook_close(b);
    }

    unlink(ROOT "/graphics/render/misfiled.spell");
}

static void test_real_book(void)
{
    /* The organization is only real if it is CHECKED.  This scans the actual
     * spell book in the tree, so a spell filed wrong, a missing source or a
     * broken manifest fails the gate rather than going unnoticed. */
    wubu_spellbook_t *b = wubu_spellbook_open("src/spells");
    CHECK(b != NULL, "the real spell book opens");
    if (!b) return;

    if (wubu_spellbook_error_count(b) != 0) {
        for (size_t i = 0; i < wubu_spellbook_error_count(b); i++)
            printf("  book error: %s\n", wubu_spellbook_error_at(b, i));
    }
    CHECK(wubu_spellbook_error_count(b) == 0, "the real book has no errors");
    CHECK(wubu_spellbook_count(b) >= 1, "the real book is not empty");

    const wubu_spell_t *s = wubu_spellbook_find(b, "vkd3d");
    CHECK(s != NULL, "the vkd3d spell is in the book");
    if (s) {
        CHECK(strcmp(s->book, "graphics") == 0, "vkd3d is in the graphics book");
        CHECK(strcmp(s->genre, "render") == 0, "vkd3d is in the render genre");
        CHECK(strcmp(s->origin, "third-party") == 0, "vkd3d is marked third-party");
        CHECK(s->provides_count >= 6, "vkd3d declares its capabilities");
        CHECK(s->sources_count == 15, "vkd3d declares all 15 slices");
        CHECK(s->requires_count == 2, "vkd3d declares its platform requirements");
        CHECK(s->needs_count == 0, "vkd3d needs no other spell");
    }

    CHECK(wubu_spellbook_resolve(b, "d3d12-on-vulkan") != NULL,
          "the capability resolves to vkd3d");
    CHECK(wubu_spellbook_cast(b, "vkd3d") == 0, "vkd3d casts");
    CHECK(wubu_spellbook_unmet_count(b) == 0, "the real book has no unmet needs");

    wubu_spellbook_close(b);
}

int main(void)
{
    printf("=== test_spellbook ===\n");
    build_book();
    test_scan_and_layout();
    test_resolve_and_cast();
    test_duplicate_and_conflict();
    test_missing_source_and_unknown_key();
    test_layout_mismatch();
    test_real_book();
    wubu_spellbook_close(wubu_spellbook_open("/nonexistent-book"));

    /* a nonexistent root must not crash and must report nothing found */
    wubu_spellbook_t *bad = wubu_spellbook_open("/nonexistent-book");
    CHECK(bad != NULL, "a nonexistent root still yields a book");
    if (bad) {
        CHECK(wubu_spellbook_count(bad) == 0, "an empty root has no spells");
        wubu_spellbook_close(bad);
    }

    if (failures == 0) printf("=== spellbook: %d checks, 0 failed ===\n", checks);
    else printf("=== spellbook: %d failed of %d ===\n", failures, checks);
    return failures ? 1 : 0;
}
