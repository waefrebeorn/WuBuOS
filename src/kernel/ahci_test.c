/*
 * ahci_test.c — Test Suite for WuBuOS AHCI (SATA) Disk Driver
 *
 * Cell 072: Tests HBA init, port enumeration, IDENTIFY,
 * sector read/write, simulated disk, and diagnostics.
 */

#include "ahci.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-45s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ── HBA Lifecycle Tests ───────────────────────────────────── */

static void test_hba_init(void) {
    TEST("HBA init");
    ahci_hba_t hba;
    int rc = ahci_hba_init(&hba);
    CHECK(rc == 0, "init should succeed");
    CHECK(hba.initialized == 1, "should be initialized");
    CHECK(hba.version != 0, "version should be set");
    CHECK(hba.cap != 0, "capabilities should be set");
    ahci_hba_shutdown(&hba);
    CHECK(hba.initialized == 0, "should be off after shutdown");
    PASS();
}

static void test_hba_capabilities(void) {
    TEST("HBA capabilities set correctly");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    CHECK(hba.cap != 0, "CAP should be non-zero");
    CHECK(hba.ghc & AHCI_GHC_AE, "AHCI should be enabled");
    CHECK(hba.pi != 0, "at least one port should be implemented");
    ahci_hba_shutdown(&hba);
    PASS();
}

/* ── Port Enumeration ──────────────────────────────────────── */

static void test_enumerate_ports(void) {
    TEST("enumerate ports");
    ahci_hba_t hba;
    ahci_hba_init(&hba);

    int count = ahci_enumerate_ports(&hba);
    CHECK(count > 0, "should find at least 1 port");
    CHECK(hba.num_ports == count, "num_ports should match");
    CHECK(hba.num_ports == 2, "should have 2 ports (pi=0x03)");

    ahci_hba_shutdown(&hba);
    PASS();
}

static void test_port_types(void) {
    TEST("port device types from signature");
    CHECK(ahci_dev_type_from_sig(AHCI_SIG_SATA) == AHCI_DEV_SATA, "SATA sig");
    CHECK(ahci_dev_type_from_sig(AHCI_SIG_SATAPI) == AHCI_DEV_SATAPI, "SATAPI sig");
    CHECK(ahci_dev_type_from_sig(AHCI_SIG_SEMB) == AHCI_DEV_SEMB, "SEMB sig");
    CHECK(ahci_dev_type_from_sig(0x00000000) == AHCI_DEV_NONE, "no sig = NONE");
    PASS();
}

static void test_port_init(void) {
    TEST("port init allocates command structures");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    ahci_enumerate_ports(&hba);

    int rc = ahci_port_init(&hba, 0);
    CHECK(rc == 0, "port 0 init should succeed");
    CHECK(hba.ports[0].state == AHCI_PORT_ACTIVE, "port 0 should be active");
    CHECK(hba.ports[0].cmd_list != NULL, "cmd_list should be allocated");
    CHECK(hba.ports[0].cmd_table != NULL, "cmd_table should be allocated");

    /* Init port 1 */
    rc = ahci_port_init(&hba, 1);
    CHECK(rc == 0, "port 1 init should succeed");
    CHECK(hba.ports[1].state == AHCI_PORT_ACTIVE, "port 1 should be active");

    /* Invalid port */
    rc = ahci_port_init(&hba, 5);
    CHECK(rc == -1, "port 5 (not in pi) should fail");

    ahci_hba_shutdown(&hba);
    PASS();
}

/* ── Simulated Disk ────────────────────────────────────────── */

static void test_sim_disk_create(void) {
    TEST("create simulated disk");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    ahci_enumerate_ports(&hba);
    ahci_port_init(&hba, 0);

    int rc = ahci_sim_disk_create(&hba, 0, 1); /* 1 MB */
    CHECK(rc == 0, "1MB sim disk should succeed");
    CHECK(hba.ports[0].sim_disk != NULL, "sim_disk should be allocated");
    CHECK(hba.ports[0].sim_disk_size == 1024 * 1024, "size should be 1MB");
    CHECK(hba.ports[0].lba_count > 0, "lba_count should be > 0");
    CHECK(hba.ports[0].lba_count == (1024 * 1024) / 512, "lba_count = 2048");

    ahci_hba_shutdown(&hba);
    PASS();
}

static void test_sim_disk_destroy(void) {
    TEST("destroy simulated disk");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    ahci_enumerate_ports(&hba);
    ahci_port_init(&hba, 0);
    ahci_sim_disk_create(&hba, 0, 1);

    ahci_sim_disk_destroy(&hba, 0);
    CHECK(hba.ports[0].sim_disk == NULL, "sim_disk should be NULL");
    CHECK(hba.ports[0].sim_disk_size == 0, "size should be 0");
    CHECK(hba.ports[0].lba_count == 0, "lba_count should be 0");

    ahci_hba_shutdown(&hba);
    PASS();
}

/* ── IDENTIFY DEVICE ───────────────────────────────────────── */

static void test_identify(void) {
    TEST("IDENTIFY DEVICE");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    ahci_enumerate_ports(&hba);
    ahci_port_init(&hba, 0);
    ahci_sim_disk_create(&hba, 0, 10); /* 10 MB */

    int rc = ahci_identify(&hba, 0);
    CHECK(rc == 0, "identify should succeed");

    char model[64] = {0};
    ahci_identify_model(&hba.ports[0].identify, model, sizeof(model));
    CHECK(strlen(model) > 0, "model string should be non-empty");

    char serial[32] = {0};
    ahci_identify_serial(&hba.ports[0].identify, serial, sizeof(serial));
    /* Serial may or may not be populated depending on byte-swap logic */

    /* Check LBA count in identify data */
    CHECK(hba.ports[0].identify.lba48_sectors[0] != 0 ||
          hba.ports[0].identify.lba48_sectors[1] != 0, "LBA48 should be populated for 10MB disk");

    ahci_hba_shutdown(&hba);
    PASS();
}

static void test_identify_lba28(void) {
    TEST("IDENTIFY LBA28 for small disk");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    ahci_enumerate_ports(&hba);
    ahci_port_init(&hba, 0);
    ahci_sim_disk_create(&hba, 0, 1); /* 1 MB — fits in LBA28 */

    ahci_identify(&hba, 0);
    /* 1MB = 2048 sectors = 0x800 */
    uint32_t lba28 = hba.ports[0].identify.lba28_sectors[0] |
                     ((uint32_t)hba.ports[0].identify.lba28_sectors[1] << 16);
    CHECK(lba28 == 2048, "LBA28 should be 2048 for 1MB disk");

    ahci_hba_shutdown(&hba);
    PASS();
}

/* ── Sector Read/Write ─────────────────────────────────────── */

static void test_read_sectors(void) {
    TEST("read sectors from sim disk");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    ahci_enumerate_ports(&hba);
    ahci_port_init(&hba, 0);
    ahci_sim_disk_create(&hba, 0, 1);

    /* Read first sector (should be all zeros from calloc) */
    uint8_t buf[512];
    int rc = ahci_read(&hba, 0, 0, 1, buf);
    CHECK(rc == 1, "read should return 1 sector");

    /* Verify it's zeros */
    int all_zero = 1;
    for (int i = 0; i < 512; i++) if (buf[i] != 0) all_zero = 0;
    CHECK(all_zero, "first sector should be zeros");

    ahci_hba_shutdown(&hba);
    PASS();
}

static void test_write_read_roundtrip(void) {
    TEST("write then read roundtrip");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    ahci_enumerate_ports(&hba);
    ahci_port_init(&hba, 0);
    ahci_sim_disk_create(&hba, 0, 1);

    /* Write pattern to sector 10 */
    uint8_t pattern[512];
    for (int i = 0; i < 512; i++) pattern[i] = (uint8_t)(i & 0xFF);
    int wc = ahci_write(&hba, 0, 10, 1, pattern);
    CHECK(wc == 1, "write should return 1 sector");

    /* Read it back */
    uint8_t readback[512];
    int rc = ahci_read(&hba, 0, 10, 1, readback);
    CHECK(rc == 1, "read should return 1 sector");

    /* Compare */
    int match = (memcmp(pattern, readback, 512) == 0);
    CHECK(match, "read should match written data");

    ahci_hba_shutdown(&hba);
    PASS();
}

static void test_write_multiple_sectors(void) {
    TEST("write/read multiple sectors");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    ahci_enumerate_ports(&hba);
    ahci_port_init(&hba, 0);
    ahci_sim_disk_create(&hba, 0, 1);

    /* Write 4 sectors */
    uint8_t data[512 * 4];
    for (int i = 0; i < (int)sizeof(data); i++) data[i] = (uint8_t)((i * 7 + 13) & 0xFF);

    int wc = ahci_write(&hba, 0, 100, 4, data);
    CHECK(wc == 4, "write 4 sectors should return 4");

    uint8_t readback[512 * 4];
    int rc = ahci_read(&hba, 0, 100, 4, readback);
    CHECK(rc == 4, "read 4 sectors should return 4");
    CHECK(memcmp(data, readback, sizeof(data)) == 0, "data should match");

    ahci_hba_shutdown(&hba);
    PASS();
}

static void test_read_out_of_bounds(void) {
    TEST("read out of bounds fails");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    ahci_enumerate_ports(&hba);
    ahci_port_init(&hba, 0);
    ahci_sim_disk_create(&hba, 0, 1); /* 2048 sectors */

    uint8_t buf[512];
    int rc = ahci_read(&hba, 0, 2000, 100, buf); /* 2000+100=2100 > 2048 */
    CHECK(rc == -1, "out-of-bounds read should fail");

    rc = ahci_read(&hba, 0, 2047, 1, buf); /* Last valid sector */
    CHECK(rc == 1, "LBA 2047 (last sector) should succeed");

    rc = ahci_read(&hba, 0, 2048, 1, buf); /* LBA 2048 = past end */
    CHECK(rc == -1, "LBA at end should fail");

    ahci_hba_shutdown(&hba);
    PASS();
}

static void test_write_read_stats(void) {
    TEST("read/write stats tracked");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    ahci_enumerate_ports(&hba);
    ahci_port_init(&hba, 0);
    ahci_sim_disk_create(&hba, 0, 1);

    CHECK(ahci_hba_reads(&hba) == 0, "initial reads 0");
    CHECK(ahci_hba_writes(&hba) == 0, "initial writes 0");

    uint8_t buf[512] = {0};
    ahci_write(&hba, 0, 0, 1, buf);
    CHECK(ahci_hba_writes(&hba) == 1, "1 write");
    CHECK(ahci_hba_reads(&hba) == 0, "still 0 reads");

    ahci_read(&hba, 0, 0, 1, buf);
    CHECK(ahci_hba_reads(&hba) == 1, "1 read");

    ahci_read(&hba, 0, 1, 1, buf);
    CHECK(ahci_hba_reads(&hba) == 2, "2 reads");

    CHECK(ahci_hba_reads(NULL) == 0, "NULL reads 0");
    CHECK(ahci_hba_writes(NULL) == 0, "NULL writes 0");

    ahci_hba_shutdown(&hba);
    PASS();
}

/* ── Two-Port Test ─────────────────────────────────────────── */

static void test_two_ports(void) {
    TEST("two ports, independent disks");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    ahci_enumerate_ports(&hba);
    ahci_port_init(&hba, 0);
    ahci_port_init(&hba, 1);
    ahci_sim_disk_create(&hba, 0, 1);
    ahci_sim_disk_create(&hba, 1, 2);

    /* Write to port 0 */
    uint8_t data0[512];
    memset(data0, 0xAA, 512);
    ahci_write(&hba, 0, 0, 1, data0);

    /* Write different data to port 1 */
    uint8_t data1[512];
    memset(data1, 0xBB, 512);
    ahci_write(&hba, 1, 0, 1, data1);

    /* Read back port 0 */
    uint8_t read0[512];
    ahci_read(&hba, 0, 0, 1, read0);
    CHECK(memcmp(data0, read0, 512) == 0, "port 0 data should match");

    /* Read back port 1 */
    uint8_t read1[512];
    ahci_read(&hba, 1, 0, 1, read1);
    CHECK(memcmp(data1, read1, 512) == 0, "port 1 data should match");

    /* Verify port 0 not contaminated by port 1 write */
    CHECK(read0[0] == 0xAA, "port 0 should still be 0xAA");

    ahci_hba_shutdown(&hba);
    PASS();
}

/* ── Diagnostics ───────────────────────────────────────────── */

static void test_dump(void) {
    TEST("HBA dump does not crash");
    ahci_hba_t hba;
    ahci_hba_init(&hba);
    ahci_enumerate_ports(&hba);
    ahci_port_init(&hba, 0);
    ahci_sim_disk_create(&hba, 0, 10);
    ahci_hba_dump(&hba);
    ahci_hba_dump(NULL); /* NULL should not crash */
    ahci_hba_shutdown(&hba);
    PASS();
}

/* ── Main ──────────────────────────────────────────────────── */

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  WuBuOS AHCI (SATA) Disk Driver Test Suite        ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* Lifecycle */
    test_hba_init();
    test_hba_capabilities();

    /* Port Management */
    test_enumerate_ports();
    test_port_types();
    test_port_init();

    /* Sim Disk */
    test_sim_disk_create();
    test_sim_disk_destroy();

    /* IDENTIFY */
    test_identify();
    test_identify_lba28();

    /* I/O */
    test_read_sectors();
    test_write_read_roundtrip();
    test_write_multiple_sectors();
    test_read_out_of_bounds();
    test_write_read_stats();

    /* Multi-port */
    test_two_ports();

    /* Diagnostics */
    test_dump();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("══════════════════════════════════════════════════\n");

    return g_fail > 0 ? 1 : 0;
}
