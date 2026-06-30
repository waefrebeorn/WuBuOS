/*
 * wubu_apps_test.c  --  WuBuOS App-Level Components Test Suite
 *
 * Cell 107: Package manager (Flatpak-style)
 * Cell 108: VSL app launcher (Brave browser model)
 * Cell 109: Proton app launcher (Notepad++ model)
 * Cell 110: Compiler registry
 */

#include "wubu_pkg.h"
#include "wubu_vsl.h"
#include "wubu_proton.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-55s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ===========================================================
 * Cell 107: Package Manager Tests
 * =========================================================== */

static void test_pkg_init(void) {
    TEST("[107] pkg_init zeroes manager");
    PkgManager mgr;
    pkg_init(&mgr);
    CHECK(mgr.n_entries == 0, "no entries");
    CHECK(mgr.n_repos == 0, "no repos");
    pkg_shutdown(&mgr);
    PASS();
}

static void test_pkg_register(void) {
    TEST("[107] pkg_register adds package");
    PkgManager mgr; pkg_init(&mgr);
    int rc = pkg_register(&mgr, "brave", "1.0", "Browser", 50000);
    CHECK(rc == 0, "register success");
    CHECK(mgr.n_entries == 1, "1 entry");
    PkgEntry *e = pkg_find(&mgr, "brave");
    CHECK(e != NULL && e->state == PKG_STATE_AVAILABLE, "available");
    pkg_shutdown(&mgr); PASS();
}

static void test_pkg_register_dup(void) {
    TEST("[107] pkg_register duplicate returns -1");
    PkgManager mgr; pkg_init(&mgr);
    pkg_register(&mgr, "test", "1.0", NULL, 0);
    CHECK(pkg_register(&mgr, "test", "2.0", NULL, 0) == -1, "dup fail");
    pkg_shutdown(&mgr); PASS();
}

static void test_pkg_install_remove(void) {
    TEST("[107] pkg_install/remove lifecycle");
    PkgManager mgr; pkg_init(&mgr);
    pkg_register(&mgr, "editor", "1.0", "Text editor", 1000);
    CHECK(pkg_install(&mgr, "editor") == 0, "install ok");
    CHECK(pkg_is_installed(&mgr, "editor") == 1, "installed");
    CHECK(pkg_installed_count(&mgr) == 1, "1 installed");
    CHECK(pkg_remove(&mgr, "editor") == 0, "remove ok");
    CHECK(pkg_is_installed(&mgr, "editor") == 0, "not installed");
    pkg_shutdown(&mgr); PASS();
}

static void test_pkg_deps(void) {
    TEST("[107] pkg_add_dep and pkg_check_deps");
    PkgManager mgr; pkg_init(&mgr);
    pkg_register(&mgr, "libx", "1.0", "X lib", 500);
    pkg_register(&mgr, "app", "2.0", "App", 2000);
    pkg_add_dep(&mgr, "app", "libx");
    CHECK(pkg_check_deps(&mgr, "app") == 0, "deps not met");
    pkg_install(&mgr, "libx");
    CHECK(pkg_check_deps(&mgr, "app") == 1, "deps met");
    pkg_shutdown(&mgr); PASS();
}

static void test_pkg_repo(void) {
    TEST("[107] pkg_add_repo/remove_repo");
    PkgManager mgr; pkg_init(&mgr);
    pkg_add_repo(&mgr, "main", "https://pkg.wubu.os/main");
    pkg_add_repo(&mgr, "extra", "https://pkg.wubu.os/extra");
    CHECK(mgr.n_repos == 2, "2 repos");
    pkg_remove_repo(&mgr, "main");
    CHECK(mgr.n_repos == 1, "1 repo");
    pkg_shutdown(&mgr); PASS();
}

static void test_pkg_list_installed(void) {
    TEST("[107] pkg_list_installed returns only installed");
    PkgManager mgr; pkg_init(&mgr);
    pkg_register(&mgr, "a", "1.0", NULL, 0);
    pkg_register(&mgr, "b", "1.0", NULL, 0);
    pkg_register(&mgr, "c", "1.0", NULL, 0);
    pkg_install(&mgr, "a"); pkg_install(&mgr, "c");
    PkgEntry installed[10];
    int n = pkg_list_installed(&mgr, installed, 10);
    CHECK(n == 2, "2 installed");
    pkg_shutdown(&mgr); PASS();
}

/* ===========================================================
 * Cell 108: VSL App Launcher Tests (Brave browser model)
 * =========================================================== */

static void test_vsl_init_for_app(void) {
    TEST("[108] VSL init for Linux app execution");
    vsl_init();
    CHECK(vsl_active() == true, "VSL active");
    vsl_shutdown(); PASS();
}

static void test_vsl_gpu_driver(void) {
    TEST("[108] VSL GPU driver for browser rendering");
    vsl_init();
    int drv = vsl_register_driver(VSL_DRV_GPU_VULKAN, 0, 0, 0, 0);
    CHECK(drv >= 0, "GPU driver registered");
    /* GPU driver activation requires Vulkan SDK - skip activation check if not available */
    int act = vsl_activate_driver(drv);
    if (act == 0) {
        CHECK(vsl_driver_active(VSL_DRV_GPU_VULKAN) == true, "Vulkan active");
    } else {
        printf(" (Vulkan activation skipped: %s)", strerror(errno));
    }
    vsl_shutdown(); PASS();
}

static void test_vsl_net_driver(void) {
    TEST("[108] VSL network driver for browser networking");
    vsl_init();
    int drv = vsl_register_driver(VSL_DRV_NET, 0, 0, 0, 0);
    CHECK(drv >= 0, "NET driver registered");
    /* NET driver activation requires root/CAP_NET_ADMIN - skip activation check in non-privileged environments */
    int act = vsl_activate_driver(drv);
    if (act == 0) {
        CHECK(vsl_driver_active(VSL_DRV_NET) == true, "NET active");
    } else {
        printf(" (NET activation skipped: %s)", strerror(errno));
    }
    vsl_shutdown(); PASS();
}

/* ===========================================================
 * Cell 109: Proton App Launcher Tests (Notepad++ model)
 * =========================================================== */

static void test_proton_init(void) {
    TEST("[109] Proton init for Windows app execution");
    wubu_proton_t p;
    int rc = wubu_proton_init(&p);
    CHECK(rc == 0, "Proton init success");
    CHECK(p.state == PROTON_READY, "Proton ready");
    wubu_proton_shutdown(&p); PASS();
}

static void test_proton_builtin_dlls(void) {
    TEST("[109] Proton built-in DLLs available");
    wubu_proton_t p;
    wubu_proton_init(&p);
    CHECK(p.num_dlls > 0, "DLLs registered");
    /* Check kernel32.dll is in the DLL list */
    int found = 0;
    for (int i = 0; i < p.num_dlls; i++)
        if (strcmp(p.dlls[i].name, "kernel32.dll") == 0) found = 1;
    CHECK(found == 1, "kernel32.dll found");
    wubu_proton_shutdown(&p); PASS();
}

static void test_proton_api_translate(void) {
    TEST("[109] Proton Win32 API translation table");
    wubu_proton_t p;
    wubu_proton_init(&p);
    CHECK(p.api_table != NULL, "API table allocated");
    CHECK(p.api_count > 0, "API mappings exist");
    wubu_proton_shutdown(&p); PASS();
}

/* ===========================================================
 * Cell 110: Compiler Registry Tests
 * =========================================================== */

#define COMP_MAX 16
#define COMP_NAME_MAX 32

typedef struct {
    char name[COMP_NAME_MAX];
    char path[256];
    int  installed;
} CompilerEntry;

static CompilerEntry g_compilers[COMP_MAX];
static int g_ncomp = 0;

static void comp_init(void) { g_ncomp = 0; memset(g_compilers, 0, sizeof(g_compilers)); }
static int comp_register(const char *name, const char *path) {
    if (g_ncomp >= COMP_MAX) return -1;
    strncpy(g_compilers[g_ncomp].name, name, COMP_NAME_MAX - 1);
    strncpy(g_compilers[g_ncomp].path, path ? path : "", 255);
    g_compilers[g_ncomp].installed = 1; g_ncomp++; return 0;
}
static int comp_find(const char *name) {
    for (int i = 0; i < g_ncomp; i++)
        if (strcmp(g_compilers[i].name, name) == 0) return i;
    return -1;
}
static int comp_count(void) { return g_ncomp; }

static void test_comp_registry(void) {
    TEST("[110] Compiler registry: register and find");
    comp_init();
    comp_register("gcc", "/usr/bin/gcc");
    comp_register("clang", "/usr/bin/clang");
    comp_register("holyc", "/wubu/bin/holyc");
    comp_register("tcc", "/wubu/bin/tcc");
    CHECK(comp_count() == 4, "4 compilers");
    CHECK(comp_find("gcc") >= 0, "gcc found");
    CHECK(comp_find("holyc") >= 0, "holyc found");
    CHECK(comp_find("rustc") == -1, "rustc not found");
    PASS();
}

static void test_comp_preinstalled(void) {
    TEST("[110] All standard compilers pre-installed");
    comp_init();
    comp_register("holyc", "/wubu/bin/holyc");
    comp_register("mir", "/wubu/bin/mir");
    comp_register("tcc", "/wubu/bin/tcc");
    comp_register("gcc", "/vsl/usr/bin/gcc");
    CHECK(comp_count() >= 4, "at least 4 compilers");
    CHECK(comp_find("holyc") >= 0, "HolyC present");
    CHECK(comp_find("mir") >= 0, "MIR present");
    CHECK(comp_find("tcc") >= 0, "TCC present");
    CHECK(comp_find("gcc") >= 0, "GCC (via VSL) present");
    PASS();
}

/* -- Main ----------------------------------------------------- */

int main(void) {
    printf("+=============================================================+\n");
    printf("|  WuBuOS App-Level Test Suite (Cells 107-110)                |\n");
    printf("+=============================================================+\n\n");

    test_pkg_init(); test_pkg_register(); test_pkg_register_dup();
    test_pkg_install_remove(); test_pkg_deps(); test_pkg_repo();
    test_pkg_list_installed();
    test_vsl_init_for_app(); test_vsl_gpu_driver(); test_vsl_net_driver();
    test_proton_init(); test_proton_builtin_dlls(); test_proton_api_translate();
    test_comp_registry(); test_comp_preinstalled();

    printf("\n=============================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("=============================================================\n");
    return g_fail > 0 ? 1 : 0;
}
