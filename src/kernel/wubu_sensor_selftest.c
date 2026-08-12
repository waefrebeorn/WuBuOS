/*
 * wubu_sensor_selftest.c -- verifies kernel-owned IIO sensor routing.
 */
#include "wubu_sensor.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_sensor_selftest ===\n\n");

    wubu_hw_detect();
    wubu_sensor_probe();

    printf("  present=%d count=%d accel=%d gyro=%d mag=%d imu=%d baro=%d als=%d\n",
           wubu_sensor_present(), wubu_sensor_count(),
           wubu_sensor_has_accel(), wubu_sensor_has_gyro(),
           wubu_sensor_has_mag(), wubu_sensor_has_imu(),
           wubu_sensor_has_baro(), wubu_sensor_has_als());

    /* Count is consistent with present. */
    CHECK(wubu_sensor_count() >= 0, "sensor count is sane");
    CHECK(wubu_sensor_present() == (wubu_sensor_count() > 0),
          "present == (count > 0)");

    /* Driver routing is always consistent. */
    CHECK(strcmp(wubu_sensor_driver("accel"), "st-accel") == 0,
          "accel -> st-accel");
    CHECK(strcmp(wubu_sensor_driver("gyro"), "st-gyro") == 0,
          "gyro -> st-gyro");
    CHECK(strcmp(wubu_sensor_driver("imu"), "inv-mpu6050") == 0,
          "imu -> inv-mpu6050");
    CHECK(strcmp(wubu_sensor_driver("baro"), "bmp280") == 0,
          "baro -> bmp280");
    CHECK(strcmp(wubu_sensor_driver("als"), "apds9960") == 0,
          "als -> apds9960");
    CHECK(strcmp(wubu_sensor_driver("humid"), "st-humidity") == 0,
          "humidity -> st-humidity");
    CHECK(wubu_sensor_driver("unknown") != NULL,
          "unknown family -> fallback driver");

    char s[256];
    wubu_sensor_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "sensor summary generated");

    printf("\n=== SENSOR TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
