/*
 * wubu_proton2_test.c  --  Tests for Proton container + HID/USB + GPU
 *
 * Cell 399: Proton runs as real Arch container with Wine + DXVK.
 */
#include "wubu_proton2.h"
#include <stdio.h>
#include <string.h>

static int pass = 0, fail = 0;
#define TEST(name) printf("  TEST Cell399: %-55s", name)
#define PASS() do { pass++; printf("✅\n"); } while(0)
#define FAIL(msg) do { fail++; printf("❌ %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- GPU Detection ------------------------------------------------ */

static void test_gpu_detect(void) {
    TEST("GPU detection");
    char name[64], pci[32];
    int ret = wubu_gpu_detect(name, sizeof(name), pci, sizeof(pci));
    /* May or may not find GPU in test environment */
    printf("(%s) ", ret == 0 ? name : "none");
    PASS();
}

/* -- HID Enumeration ---------------------------------------------- */

static void test_hid_enumerate(void) {
    TEST("HID device enumeration");
    char names[16][64];
    int types[16];
    int n = wubu_hid_enumerate(names, types, 16);
    printf("(%d devices) ", n);
    PASS();  /* Just verify it doesn't crash */
}

/* -- USB Enumeration ---------------------------------------------- */

static void test_usb_enumerate(void) {
    TEST("USB device enumeration");
    char paths[8][256], names[8][64];
    int n = wubu_usb_enumerate(paths, names, 8);
    printf("(%d devices) ", n);
    PASS();
}

/* -- MIDI Enumeration --------------------------------------------- */

static void test_midi_enumerate(void) {
    TEST("MIDI device enumeration");
    char names[8][64];
    int n = wubu_midi_enumerate(names, 8);
    printf("(%d devices) ", n);
    PASS();
}

/* -- Proton Manager ----------------------------------------------- */

static void test_proton_mgr_create(void) {
    TEST("Proton manager create");
    WubuProtonConfig cfg = {0};
    cfg.dxvk_enabled = true;
    cfg.gpu_passthrough = true;
    cfg.xinput = true;
    WubuProtonManager *mgr = wubu_proton_mgr_create(&cfg);
    CHECK(mgr != NULL, "manager should exist");
    CHECK(mgr->global.dxvk_enabled == true, "DXVK should be enabled");
    CHECK(mgr->global.gpu_passthrough == true, "GPU passthrough should be on");
    wubu_proton_mgr_destroy(mgr);
    PASS();
}

static void test_proton_mgr_default_config(void) {
    TEST("Proton manager default config");
    WubuProtonManager *mgr = wubu_proton_mgr_create(NULL);
    CHECK(mgr != NULL, "manager should exist");
    CHECK(mgr->global.dxvk_enabled == true, "DXVK on by default");
    CHECK(mgr->global.dxvk_async == true, "DXVK async by default");
    CHECK(mgr->global.esync == true, "esync on by default");
    CHECK(mgr->global.fsync == true, "fsync on by default");
    CHECK(mgr->global.xinput == true, "XInput on by default");
    wubu_proton_mgr_destroy(mgr);
    PASS();
}

static void test_proton_add_app(void) {
    TEST("Proton add app");
    WubuProtonManager *mgr = wubu_proton_mgr_create(NULL);
    WubuProtonApp app = {0};
    strncpy(app.name, "TestApp", sizeof(app.name));
    strncpy(app.exe_path, "/home/user/game.exe", sizeof(app.exe_path));
    int idx = wubu_proton_add_app(mgr, &app);
    CHECK(idx == 0, "first app should be index 0");
    CHECK(mgr->n_apps == 1, "should have 1 app");
    wubu_proton_mgr_destroy(mgr);
    PASS();
}

static void test_proton_app_launch_name(void) {
    TEST("Proton launch by name");
    WubuProtonManager *mgr = wubu_proton_mgr_create(NULL);
    WubuProtonApp app = {0};
    strncpy(app.name, "MyGame", sizeof(app.name));
    strncpy(app.exe_path, "/game.exe", sizeof(app.exe_path));
    wubu_proton_add_app(mgr, &app);
    /* Can't actually launch without container, but API should work */
    int ret = wubu_proton_launch_name(mgr, "MyGame");
    CHECK(ret == -1, "should fail without container running");
    ret = wubu_proton_launch_name(mgr, "NonExistent");
    CHECK(ret == -1, "should fail for unknown app");
    wubu_proton_mgr_destroy(mgr);
    PASS();
}

static void test_proton_is_running(void) {
    TEST("Proton is_running check");
    WubuProtonManager *mgr = wubu_proton_mgr_create(NULL);
    CHECK(!wubu_proton_is_running(mgr), "should not be running initially");
    wubu_proton_mgr_destroy(mgr);
    PASS();
}

static void test_proton_container_access(void) {
    TEST("Proton container/ramdisk access");
    WubuProtonManager *mgr = wubu_proton_mgr_create(NULL);
    CHECK(wubu_proton_container(mgr) == NULL, "container should be NULL before start");
    CHECK(wubu_proton_ramdisk(mgr) == NULL, "ramdisk should be NULL before start");
    wubu_proton_mgr_destroy(mgr);
    PASS();
}

static void test_proton_dump(void) {
    TEST("Proton diagnostics dump");
    WubuProtonManager *mgr = wubu_proton_mgr_create(NULL);
    wubu_proton_dump(mgr);  /* Should not crash */
    wubu_proton_mgr_destroy(mgr);
    PASS();
}

/* -- Main --------------------------------------------------------- */

int main(void) {
    printf("\n-- GPU Detection (Cell 399) --\n\n");
    test_gpu_detect();

    printf("\n-- HID/USB Enumeration (Cell 399) --\n\n");
    test_hid_enumerate();
    test_usb_enumerate();
    test_midi_enumerate();

    printf("\n-- Proton Manager (Cell 399) --\n\n");
    test_proton_mgr_create();
    test_proton_mgr_default_config();
    test_proton_add_app();
    test_proton_app_launch_name();
    test_proton_is_running();
    test_proton_container_access();
    test_proton_dump();

    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n",
           pass, pass + fail, fail);
    printf("==================================================\n");
    return fail > 0 ? 1 : 0;
}
