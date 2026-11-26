/*
 * wubu_parental_test.c -- the family limit is a pure function of (policy, clock), so it can be tested at
 * 03:00 on a Sunday without waiting for either.
 *
 * C11.
 */
#include "wubu_parental.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  \u274c %s\n", msg); } \
} while (0)

/* A localtime-shaped moment. tm_wday: 0 = Sunday, as time.h defines it. */
static struct tm at(int year, int mon, int day, int hour, int min, int wday)
{
    struct tm t;
    memset(&t, 0, sizeof t);
    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_wday = wday;
    return t;
}

static WubuParentalPolicy mk(int enabled, int start, int end, unsigned days)
{
    WubuParentalPolicy p;
    memset(&p, 0, sizeof p);
    p.enabled = enabled;
    p.start_min = start;
    p.end_min = end;
    p.days = days;
    p.loaded = true;
    return p;
}

int main(void)
{
    char root[] = "/tmp/partestXXXXXX";
    if (!mkdtemp(root)) { printf("mkdtemp failed\n"); return 1; }
    char conf[400];
    snprintf(conf, sizeof conf, "%s/parental.conf", root);
    setenv("WUBU_PARENTAL_CONF", conf, 1);

    printf("=== Test: no policy file means no limit, and says so ===\n");
    WubuParentalPolicy p;
    CHECK(wubu_parental_load(&p) == -1, "a missing policy file is not a policy");
    CHECK(!p.loaded, "nothing was loaded");
    CHECK(!p.enabled, "a missing file never enables a limit");
    struct tm t = at(2026, 11, 25, 3, 0, 3);            /* Wednesday 03:00 */
    CHECK(wubu_parental_allowed(&p, &t, NULL, 0), "a disabled policy allows any hour");

    printf("\n=== Test: the file is read, including an overnight window ===\n");
    FILE *f = fopen(conf, "w");
    if (f) {
        fputs("# test policy\nenabled=1\nhours=08:00-20:00\ndays=mon,tue,wed,thu,fri\n", f);
        fclose(f);
    }
    CHECK(wubu_parental_load(&p) == 0, "the policy file loads");
    CHECK(p.loaded && p.enabled, "the policy is enabled");
    CHECK(p.start_min == 480 && p.end_min == 1200, "08:00-20:00 is 480..1200 minutes");
    CHECK(p.days == WUBU_PARENTAL_WEEKDAYS, "the days parse to Mon-Fri");

    char win[32], days[64];
    wubu_parental_window_str(&p, win, sizeof win);
    wubu_parental_days_str(&p, days, sizeof days);
    CHECK(strcmp(win, "08:00-20:00") == 0, "the window string round-trips");
    CHECK(strcmp(days, "Mon-Fri") == 0, "the weekdays have a short name");

    printf("\n=== Test: the decision inside the window ===\n");
    char why[160];
    struct tm wed_noon = at(2026, 11, 25, 12, 0, 3);
    CHECK(wubu_parental_allowed(&p, &wed_noon, why, sizeof why), "Wednesday noon is allowed");
    CHECK(why[0] == '\0', "no reason is given when allowed");

    struct tm wed_0759 = at(2026, 11, 25, 7, 59, 3);
    CHECK(!wubu_parental_allowed(&p, &wed_0759, why, sizeof why), "one minute before the window is refused");
    CHECK(strstr(why, "08:00-20:00") != NULL, "the refusal names the window");
    CHECK(strstr(why, "allowed hours") != NULL, "the refusal says it is an hours problem");

    struct tm wed_2000 = at(2026, 11, 25, 20, 0, 3);
    CHECK(!wubu_parental_allowed(&p, &wed_2000, why, sizeof why),
          "the end of the window is exclusive: 20:00 is outside");

    printf("\n=== Test: the day check, with time.h's Sunday-is-0 mapping ===\n");
    struct tm sat_noon = at(2026, 11, 28, 12, 0, 6);   /* Saturday */
    CHECK(!wubu_parental_allowed(&p, &sat_noon, why, sizeof why), "Saturday is refused");
    CHECK(strstr(why, "not an allowed day") != NULL, "the refusal says it is a day problem");
    CHECK(strstr(why, "Mon-Fri") != NULL, "the refusal names the allowed days in the short form");

    struct tm mon_noon = at(2026, 11, 30, 12, 0, 1);
    CHECK(wubu_parental_allowed(&p, &mon_noon, why, sizeof why), "Monday noon is allowed");
    struct tm sun_noon = at(2026, 11, 29, 12, 0, 0);
    CHECK(!wubu_parental_allowed(&p, &sun_noon, why, sizeof why), "Sunday is refused (tm_wday 0)");

    printf("\n=== Test: an overnight window ===\n");
    WubuParentalPolicy night = mk(1, 20 * 60, 7 * 60, WUBU_PARENTAL_ALL_DAYS);   /* 20:00-07:00 */
    struct tm t22 = at(2026, 11, 25, 22, 0, 3);
    struct tm t03 = at(2026, 11, 25, 3, 0, 3);
    struct tm t12 = at(2026, 11, 25, 12, 0, 3);
    CHECK(wubu_parental_allowed(&night, &t22, why, sizeof why), "22:00 is inside an overnight window");
    CHECK(wubu_parental_allowed(&night, &t03, why, sizeof why), "03:00 is inside the same window");
    CHECK(!wubu_parental_allowed(&night, &t12, why, sizeof why), "noon is outside it");

    printf("\n=== Test: a zero-length window is all day, not never ===\n");
    WubuParentalPolicy allday = mk(1, 0, 0, WUBU_PARENTAL_ALL_DAYS);
    CHECK(wubu_parental_allowed(&allday, &t03, why, sizeof why), "00:00-00:00 allows everything");

    printf("\n=== Test: no clock is not permission ===\n");
    CHECK(!wubu_parental_allowed(&p, NULL, why, sizeof why), "a missing clock refuses");
    CHECK(strstr(why, "clock") != NULL, "and says the clock is the problem");

    printf("\n=== Test: normalize refuses to invent nonsense, save round-trips ===\n");
    WubuParentalPolicy bad = mk(1, -10, 5000, 0);
    CHECK(wubu_parental_normalize(&bad), "an impossible policy reports that it changed");
    CHECK(bad.start_min == 8 * 60 && bad.end_min == 20 * 60, "impossible hours fall back to 08:00-20:00");
    CHECK(bad.days == WUBU_PARENTAL_ALL_DAYS, "no days means every day");

    WubuParentalPolicy save = mk(1, 7 * 60 + 30, 21 * 60, WUBU_PARENTAL_WEEKDAYS);
    CHECK(wubu_parental_save(&save) == 0, "the policy saves");
    WubuParentalPolicy back;
    CHECK(wubu_parental_load(&back) == 0, "the saved policy reloads");
    CHECK(back.enabled && back.start_min == 450 && back.end_min == 1260, "the hours survive the round trip");
    CHECK(back.days == WUBU_PARENTAL_WEEKDAYS, "the days survive the round trip");

    /* An unparseable value must not silently widen the window: it keeps the file's other lines' meaning
     * by leaving the field at its previous default rather than zeroing it. */
    f = fopen(conf, "w");
    if (f) {
        fputs("enabled=1\nhours=not-a-window\ndays=sat,sun\n", f);
        fclose(f);
    }
    WubuParentalPolicy junk;
    CHECK(wubu_parental_load(&junk) == 0, "a file with a junk window still loads");
    CHECK(junk.start_min == 480 && junk.end_min == 1200, "a junk window leaves the default hours");
    CHECK(junk.days == WUBU_PARENTAL_WEEKEND, "the days line still applies");

    char rm[400];
    snprintf(rm, sizeof rm, "rm -rf '%s'", root);
    if (system(rm) != 0) { /* best effort */ }

    printf("\n  parental: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
