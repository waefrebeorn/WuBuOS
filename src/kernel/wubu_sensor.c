/*
 * wubu_sensor.c -- kernel-owned IIO sensor driver routing.
 *
 * The IIO (Industrial I/O) subsystem is how EVERY sensor connects:
 * accelerometers, gyroscopes, IMUs, magnetometers, barometers, ambient
 * light, proximity, temperature, humidity. Modern phones/laptops/tablets
 * carry 6-10 of these. The kernel must route each to its driver and
 * expose the raw/scale/offset attributes over sysfs (or IIO buffers).
 *
 * Common sensor chips (driver in parentheses):
 *   - Accel: lis3dh (st-accel), bmc150 (bmc150-accel), mpu6050 (inv-mpu6050)
 *   - Gyro:  mpu6050, bmg160 (bmg160), lsm6dsm (st-accel/gyro)
 *   - IMU:   inv_icm45600 (inv-icm45600), bmi160, mpu9250 (inv-mpu9250)
 *   - Mag:   ak8963, bmc150-magn, lis3mdl (st-magn)
 *   - Baro:  bmp280 (bmp280), bme280 (bme280), ms5611 (ms5611)
 *   - ALS/Prox: tsl2561, apds9960 (apds9960), ltr559
 *   - Temp/Humid: bme280, si7020 (si7020), hts221 (st-humidity)
 *
 * WuBuOS owns this: detect which sensor classes are present (via /sys/bus/iio
 * or device-tree), route to the right driver, and expose the IIO topology.
 *
 * Research (Kevin-Bacon 7-hop on the sensor frontier):
 *   - IIO core + triggers + buffers: industrialio-core.ko
 *   - st-accel/gyro/magn/humidity: ST MEMS family (lis3dh, lsm6dsm, lis3mdl)
 *   - inv-mpu6050/mpu9250/icm45600: InvenSense IMUs (SPI + I2C)
 *   - bmp280/bme280: Bosch pressure + temp + humidity
 *   - bmc150, ak8963 (mag), apds9960 (ALS/prox/gesture)
 */
#include "wubu_sensor.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- IIO device classes (matched by sysfs presence) ---- */
#define SENSOR_ACCEL   (1<<0)
#define SENSOR_GYRO    (1<<1)
#define SENSOR_MAG     (1<<2)
#define SENSOR_IMU     (1<<3)
#define SENSOR_BARO    (1<<4)
#define SENSOR_ALS     (1<<5)
#define SENSOR_PROX    (1<<6)
#define SENSOR_TEMP    (1<<7)
#define SENSOR_HUMID   (1<<8)

static int  g_sensor_mask = 0;
static int  g_sensor_count = 0;

/* ---- W1: probe the sensor topology ---- */
void wubu_sensor_probe(void)
{
    g_sensor_mask = 0;
    g_sensor_count = 0;
#ifdef _GNU_SOURCE
    /* Scan the IIO device name nodes for known sensor families. */
    char path[256];
    char name[128];
    for (int i = 0; i < 32; i++) {
        snprintf(path, sizeof(path), "/sys/bus/iio/devices/iio:device%d/name", i);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        name[0] = '\0';
        if (fgets(name, sizeof(name), f)) {
            g_sensor_count++;
            if (strstr(name, "accel") || strstr(name, "lis3dh") ||
                strstr(name, "bmc150-accel") || strstr(name, "bma"))
                g_sensor_mask |= SENSOR_ACCEL;
            if (strstr(name, "gyro") || strstr(name, "bmg160") || strstr(name, "lsm6dsm"))
                g_sensor_mask |= SENSOR_GYRO;
            if (strstr(name, "magn") || strstr(name, "ak8963") || strstr(name, "lis3mdl"))
                g_sensor_mask |= SENSOR_MAG;
            if (strstr(name, "imu") || strstr(name, "mpu60") || strstr(name, "mpu9250") ||
                strstr(name, "icm") || strstr(name, "bmi160"))
                g_sensor_mask |= SENSOR_IMU;
            if (strstr(name, "bmp") || strstr(name, "bme") || strstr(name, "ms5611"))
                g_sensor_mask |= (SENSOR_BARO | SENSOR_TEMP);
            if (strstr(name, "tsl") || strstr(name, "apds") || strstr(name, "ltr"))
                g_sensor_mask |= SENSOR_ALS;
            if (strstr(name, "prox") || strstr(name, "apds"))
                g_sensor_mask |= SENSOR_PROX;
            if (strstr(name, "temp") || strstr(name, "hts221"))
                g_sensor_mask |= SENSOR_TEMP;
            if (strstr(name, "humid") || strstr(name, "hts221") || strstr(name, "si7020"))
                g_sensor_mask |= SENSOR_HUMID;
        }
        fclose(f);
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_sensor_present(void)      { return g_sensor_count > 0; }
int  wubu_sensor_count(void)        { return g_sensor_count; }
int  wubu_sensor_has_accel(void)    { return (g_sensor_mask & SENSOR_ACCEL) != 0; }
int  wubu_sensor_has_gyro(void)     { return (g_sensor_mask & SENSOR_GYRO) != 0; }
int  wubu_sensor_has_mag(void)      { return (g_sensor_mask & SENSOR_MAG) != 0; }
int  wubu_sensor_has_imu(void)      { return (g_sensor_mask & SENSOR_IMU) != 0; }
int  wubu_sensor_has_baro(void)     { return (g_sensor_mask & SENSOR_BARO) != 0; }
int  wubu_sensor_has_als(void)      { return (g_sensor_mask & SENSOR_ALS) != 0; }
int  wubu_sensor_has_proximity(void){ return (g_sensor_mask & SENSOR_PROX) != 0; }
int  wubu_sensor_has_temp(void)     { return (g_sensor_mask & SENSOR_TEMP) != 0; }
int  wubu_sensor_has_humidity(void) { return (g_sensor_mask & SENSOR_HUMID) != 0; }

/* ---- W3: driver routing per sensor family ---- */
const char *wubu_sensor_driver(const char *family)
{
    if (!family) return NULL;
    if (strstr(family, "accel"))    return "st-accel";
    if (strstr(family, "gyro"))     return "st-gyro";
    if (strstr(family, "magn"))     return "st-magn";
    if (strstr(family, "imu"))      return "inv-mpu6050";
    if (strstr(family, "baro"))     return "bmp280";
    if (strstr(family, "als"))      return "apds9960";
    if (strstr(family, "prox"))     return "apds9960";
    if (strstr(family, "humid"))    return "st-humidity";
    if (strstr(family, "temp"))     return "st-thermal";
    return "industrialio-core";
}

/* ---- W4: summary ---- */
int wubu_sensor_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "sensor[count=%d accel=%d gyro=%d mag=%d imu=%d baro=%d als=%d prox=%d temp=%d hum=%d]",
        g_sensor_count,
        wubu_sensor_has_accel(), wubu_sensor_has_gyro(),
        wubu_sensor_has_mag(), wubu_sensor_has_imu(),
        wubu_sensor_has_baro(), wubu_sensor_has_als(),
        wubu_sensor_has_proximity(), wubu_sensor_has_temp(),
        wubu_sensor_has_humidity());
}
