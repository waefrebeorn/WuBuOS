/*
 * wubu_spell_resolve_test.c -- unit gate for the .spell dependency resolver.
 *
 * Builds real .spell images in memory (the resolver works on handles from
 * wubu_spellmod_open(), so it is tested with the same bytes a loader would hand it)
 * and exercises every outcome: a straight chain, a diamond, an unsatisfiable need,
 * a cycle, a self-provided need, duplicate registration, capacity limits and the
 * "no partial plan" rule.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wubu_spellmod.h"
#include "wubu_spell_resolve.h"
#include "spellmod_test_img.h"   /* one copy of the layout maths */

static int checks_run = 0, checks_failed = 0;

#define TEST(cond, msg) do { \
    checks_run++; \
    if (!(cond)) { checks_failed++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

/* Open an image built inline.  Each module takes its own slot from the shared
 * pool because wubu_spellmod_open() BORROWS the buffer -- one shared buffer would
 * leave every handle pointing at whichever image was built last. */
static spell_test_pool g_pool;

static wubu_spellmod_t *open_built(const char *name,
                                   const char *provides[], int np,
                                   const char *needs[], int nn)
{
    uint8_t *buf = spell_test_slot(&g_pool);
    if (!buf) return NULL;
    size_t len = spell_test_build(buf, WUSPELL_TEST_BUF_SIZE, name,
                                  provides, np, needs, nn);
    return wubu_spellmod_open(buf, len);
}

/* Handles are opaque; the public info accessor carries the name. */
static const char *mod_name_of(const wubu_spellmod_t *m)
{
    static char name[WUBU_SPELLMOD_NAME_MAX];
    wubu_spellmod_info_t info;
    if (wubu_spellmod_info(m, &info) != 0) return "";
    memcpy(name, info.name, sizeof(name));
    name[sizeof(name) - 1] = '\0';
    return name;
}

static int index_of(wubu_spellmod_t **order, int count, const char *name)
{
    for (int i = 0; i < count; i++)
        if (strcmp(mod_name_of(order[i]), name) == 0) return i;
    return -1;
}

int main(void)
{
    wubu_spell_resolve *r = wubu_spell_resolve_new();
    TEST(r != NULL, "registry allocates");

    /* Capabilities are what provide/need carry -- a module NAME is not a
     * capability, so core must PROVIDE "core" for anything to need it. */
    const char *core_prov[] = { "core" };
    wubu_spellmod_t *core   = open_built("core",   core_prov, 1, NULL, 0);
    /* net needs core. */
    const char *net_prov[]  = { "network" };
    const char *net_needs[] = { "core" };
    wubu_spellmod_t *net    = open_built("net",    net_prov, 1, net_needs, 1);
    /* gui needs both core and network -- the diamond. */
    const char *gui_prov[]  = { "ui" };
    const char *gui_needs[] = { "core", "network" };
    wubu_spellmod_t *gui    = open_built("gui",    gui_prov, 1, gui_needs, 2);
    /* orphan needs something nobody provides. */
    const char *orph_needs[] = { "telepathy" };
    wubu_spellmod_t *orphan = open_built("orphan", NULL, 0, orph_needs, 1);
    /* cycle: a needs b, b needs a. */
    const char *a_prov[]  = { "cap-a" };
    const char *a_needs[] = { "cap-b" };
    const char *b_prov[]  = { "cap-b" };
    const char *b_needs[] = { "cap-a" };
    wubu_spellmod_t *cyc_a = open_built("cyc-a", a_prov, 1, a_needs, 1);
    wubu_spellmod_t *cyc_b = open_built("cyc-b", b_prov, 1, b_needs, 1);
    /* self-sufficient: provides what it needs. */
    const char *self_prov[]  = { "standalone" };
    const char *self_needs[] = { "standalone" };
    wubu_spellmod_t *solo   = open_built("solo", self_prov, 1, self_needs, 1);

    TEST(core && net && gui && orphan && cyc_a && cyc_b && solo,
         "all test images open");

    TEST(wubu_spell_registry_add(r, core) == WUBU_SPELL_OK, "add core");
    TEST(wubu_spell_registry_add(r, net)  == WUBU_SPELL_OK, "add net");
    TEST(wubu_spell_registry_add(r, gui)  == WUBU_SPELL_OK, "add gui");
    TEST(wubu_spell_registry_add(r, orphan) == WUBU_SPELL_OK, "add orphan");
    TEST(wubu_spell_registry_add(r, cyc_a) == WUBU_SPELL_OK, "add cyc-a");
    TEST(wubu_spell_registry_add(r, cyc_b) == WUBU_SPELL_OK, "add cyc-b");
    TEST(wubu_spell_registry_add(r, solo) == WUBU_SPELL_OK, "add solo");

    /* duplicate registration is idempotent, first one wins */
    TEST(wubu_spell_registry_add(r, net) == WUBU_SPELL_OK, "re-add accepted");
    TEST(wubu_spell_find_provider(r, "network") == net,
         "first registration still resolves");

    /* provider lookup */
    TEST(wubu_spell_find_provider(r, "core") == core, "provider by capability");
    TEST(wubu_spell_find_provider(r, "ui") == gui, "second provider lookup");
    TEST(wubu_spell_find_provider(r, "nonexistent") == NULL,
         "unknown capability has no provider");
    TEST(wubu_spell_find_provider(r, "telepathy") == NULL,
         "an unmet need is really unmet");

    /* straight chain + diamond: dependencies must come before consumers */
    wubu_spellmod_t *plan[WUBU_SPELL_MAX_MODULES];
    int count = -1;
    TEST(wubu_spell_plan(r, "gui", plan, &count, WUBU_SPELL_MAX_MODULES) == WUBU_SPELL_OK,
         "diamond plan succeeds");
    TEST(count == 3, "diamond plan has three modules");
    if (count == 3) {
        int ic = index_of(plan, count, "core");
        int in = index_of(plan, count, "net");
        int ig = index_of(plan, count, "gui");
        TEST(ic >= 0 && in >= 0 && ig >= 0, "all three present");
        TEST(ic < in, "core loads before net");
        TEST(in < ig, "net loads before gui");
        TEST(ic < ig, "core loads before gui");
    }

    /* a single module with no needs plans itself */
    count = 0;
    TEST(wubu_spell_plan(r, "core", plan, &count, WUBU_SPELL_MAX_MODULES) == WUBU_SPELL_OK,
         "leaf plan succeeds");
    TEST(count == 1 && plan[0] == core, "leaf plan is just the leaf");

    /* unsatisfiable need */
    count = 12345;                        /* poison: must be cleared on failure */
    int rc = wubu_spell_plan(r, "orphan", plan, &count, WUBU_SPELL_MAX_MODULES);
    TEST(rc == WUBU_SPELL_UNSATISFIED, "unsatisfiable need reported");
    TEST(count == 0, "failed plan leaves NO partial order behind");

    /* cycle */
    count = 12345;
    TEST(wubu_spell_plan(r, "cyc-a", plan, &count, WUBU_SPELL_MAX_MODULES)
         == WUBU_SPELL_CYCLE, "cycle detected");
    TEST(count == 0, "cyclic plan leaves no partial order");

    /* self-provided need is not a cycle */
    count = 0;
    TEST(wubu_spell_plan(r, "solo", plan, &count, WUBU_SPELL_MAX_MODULES) == WUBU_SPELL_OK,
         "self-provided need is satisfiable");
    TEST(count == 1 && plan[0] == solo, "self-provided module plans alone");

    /* capacity */
    count = 0;
    TEST(wubu_spell_plan(r, "gui", plan, &count, 2) == WUBU_SPELL_NOMEM,
         "capacity overflow reported");
    TEST(count == 0, "capacity failure clears the plan");

    /* invalid arguments */
    count = 0;
    TEST(wubu_spell_plan(r, "not-registered", plan, &count, 10) == WUBU_SPELL_INVALID,
         "unknown root refused");
    TEST(wubu_spell_plan(NULL, "gui", plan, &count, 10) == WUBU_SPELL_INVALID,
         "NULL registry refused");
    TEST(wubu_spell_plan(r, "gui", NULL, &count, 10) == WUBU_SPELL_INVALID,
         "NULL order array refused");

    /* registry does not own the modules */
    wubu_spell_resolve_free(r);
    TEST(wubu_spellmod_provides(net, "network"), "module survives registry free");

    wubu_spellmod_close(core);  wubu_spellmod_close(net);
    wubu_spellmod_close(gui);   wubu_spellmod_close(orphan);
    wubu_spellmod_close(cyc_a); wubu_spellmod_close(cyc_b);
    wubu_spellmod_close(solo);

    printf("%d checks, %d failed\n", checks_run, checks_failed);
    return checks_failed ? 1 : 0;
}
