/*
 * wubu_wifi_test.c -- the association workflow, proven without a radio.
 *
 * A fake supplicant answers through the transport seam and RECORDS every command,
 * so three things are testable that otherwise need hardware:
 *   - the wire shape of SCAN_RESULTS (SSIDs with spaces and slashes),
 *   - the exact command sequence of a join, in order,
 *   - the refusal cases (a quote in a passphrase, a FAIL from the supplicant).
 */

#include "wubu_wifi.h"
#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(c, m) do { if (c) { printf("  [PASS] %s\n", m); g_pass++; } \
                         else { printf("  [FAIL] %s (%s:%d)\n", m, __FILE__, __LINE__); g_fail++; } } while (0)

/* -- fake supplicant ---------------------------------------------- */

static char g_sent[16][256];
static int  g_nsent;
static const char *g_add_reply = "7\n";
static const char *g_ok = "OK\n";
static int  g_fail_on = -1;   /* index of the command that should FAIL */

static int fake_xfer(void *user, const char *cmd, char *reply, size_t n) {
    (void)user;
    if (g_nsent < 16) snprintf(g_sent[g_nsent], sizeof g_sent[0], "%s", cmd);
    int idx = g_nsent++;
    if (idx == g_fail_on) { snprintf(reply, n, "FAIL-BUSY\n"); return 0; }
    if (strcmp(cmd, "ADD_NETWORK") == 0) snprintf(reply, n, "%s", g_add_reply);
    else if (strcmp(cmd, "SCAN_RESULTS") == 0) {
        snprintf(reply, n,
            "bssid / frequency / signal level / flags / ssid\n"
            "aa:bb:cc:dd:ee:01\t/2412/\t-44\t[WPA2-PSK-CCMP][ESS]\tHome Net\n"
            "aa:bb:cc:dd:ee:02\t/5180/\t-71\t[RSN-PSK-CCMP][ESS]\tCafe/Bar 5G\n"
            "aa:bb:cc:dd:ee:03\t/2437/\t-88\t[ESS]\t\n");
    } else if (strcmp(cmd, "STATUS") == 0) {
        snprintf(reply, n, "wpa_state=%s\nssid=Home Net\nip_address=192.168.1.20\n",
                 g_ok /* repurposed: holds the state word for this test */);
    } else snprintf(reply, n, "%s", "OK\n");
    return 0;
}

static void reset_fake(void) { g_nsent = 0; g_fail_on = -1; g_ok = "OK\n"; }

/* -- tests -------------------------------------------------------- */

static void test_scan_parse(void) {
    WubuWifiNet net[8];
    reset_fake();
    wubu_wifi_set_transport(fake_xfer, NULL);
    int n = wubu_wifi_scan(net, 8);
    CHECK(n == 2, "two visible networks (the hidden one with no SSID is skipped)");
    CHECK(strcmp(net[0].ssid, "Home Net") == 0, "an SSID with a space is preserved");
    CHECK(strcmp(net[1].ssid, "Cafe/Bar 5G") == 0, "an SSID containing a slash is preserved");
    CHECK(net[0].secure == 1, "WPA2 flags mark the network secure");
    CHECK(net[0].signal == -44 && net[0].freq == 2412, "signal and frequency parsed");
    CHECK(g_nsent == 2 && strcmp(g_sent[0], "SCAN") == 0, "a scan is SCAN then SCAN_RESULTS");

    /* The header line must never become a network. */
    CHECK(strcmp(net[0].ssid, "bssid / frequency / signal level / flags / ssid") != 0,
          "the header row is not an entry");
}

static void test_join_sequence(void) {
    reset_fake();
    wubu_wifi_set_transport(fake_xfer, NULL);
    int id = wubu_wifi_join("Home Net", "password123");
    CHECK(id == 7, "ADD_NETWORK's id is returned");
    CHECK(g_nsent == 5, "join sends five commands");
    CHECK(strcmp(g_sent[0], "ADD_NETWORK") == 0, "1: ADD_NETWORK");
    CHECK(strcmp(g_sent[1], "SET_NETWORK 7 ssid \"Home Net\"") == 0, "2: quoted SSID");
    CHECK(strcmp(g_sent[2], "SET_NETWORK 7 psk \"password123\"") == 0, "3: quoted passphrase");
    CHECK(strcmp(g_sent[3], "ENABLE_NETWORK 7") == 0, "4: ENABLE_NETWORK");
    CHECK(strcmp(g_sent[4], "SAVE_CONFIG") == 0, "5: SAVE_CONFIG");

    /* Open network: key_mgmt must be set or a reused id keeps old security. */
    reset_fake();
    CHECK(wubu_wifi_join("Open Wifi", NULL) == 7, "an open network joins");
    CHECK(strcmp(g_sent[2], "SET_NETWORK 7 key_mgmt NONE") == 0,
          "an open network sets key_mgmt NONE explicitly");
}

static void test_refusals(void) {
    reset_fake();
    wubu_wifi_set_transport(fake_xfer, NULL);
    CHECK(wubu_wifi_join("Home\"Net", "password123") < 0, "a quote in the SSID is refused");
    CHECK(g_nsent == 0, "and nothing is sent to the supplicant");
    CHECK(wubu_wifi_join("Home Net", "pa\"ssword") < 0, "a quote in the passphrase is refused");
    CHECK(g_nsent == 0, "again nothing is sent");
    CHECK(wubu_wifi_join("Home Net", "short") < 0, "a passphrase under 8 chars is refused");
    CHECK(wubu_wifi_join("", "password123") < 0, "an empty SSID is refused");

    reset_fake();
    g_fail_on = 0;
    CHECK(wubu_wifi_join("Home Net", "password123") == -2, "ADD_NETWORK FAIL is reported as -2");
    reset_fake();
    g_fail_on = 1;
    CHECK(wubu_wifi_join("Home Net", "password123") == -3, "SET_NETWORK FAIL is reported as -3");
    reset_fake();
    g_fail_on = 3;
    CHECK(wubu_wifi_join("Home Net", "password123") == -4, "ENABLE_NETWORK FAIL is reported as -4");
}

static void test_status(void) {
    reset_fake();
    wubu_wifi_set_transport(fake_xfer, NULL);
    char detail[256];

    g_ok = "COMPLETED";
    CHECK(wubu_wifi_status(detail, sizeof detail) == WUBU_WIFI_CONNECTED,
          "wpa_state=COMPLETED is the ONLY 'connected'");
    CHECK(strstr(detail, "wpa_state=COMPLETED") != NULL, "status detail is returned");

    g_ok = "4WAY_HANDSHAKE";
    CHECK(wubu_wifi_status(NULL, 0) == WUBU_WIFI_AUTHENTICATING,
          "4WAY_HANDSHAKE is authenticating, not connected");
    g_ok = "SCANNING";
    CHECK(wubu_wifi_status(NULL, 0) == WUBU_WIFI_SCANNING, "SCANNING reported");
    g_ok = "DISCONNECTED";
    CHECK(wubu_wifi_status(NULL, 0) == WUBU_WIFI_DISCONNECTED, "DISCONNECTED reported");
    CHECK(strcmp(wubu_wifi_state_name(WUBU_WIFI_CONNECTED), "Connected") == 0,
          "the state has a user-facing name");
}

static void test_no_supplicant(void) {
    /* No socket behind the real transport: the caller must be able to tell
     * "there is no supplicant here" apart from "the join failed". */
    CHECK(wubu_wifi_open("definitely-not-an-interface") != 0,
          "opening a non-existent supplicant fails cleanly");
    wubu_wifi_set_transport(NULL, NULL);
}

int main(void) {
    printf("=== wubu_wifi_test ===\n");
    test_scan_parse();
    test_join_sequence();
    test_refusals();
    test_status();
    test_no_supplicant();
    printf("\n=== wifi: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
