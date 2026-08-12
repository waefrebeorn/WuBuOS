/*
 * wubu_sensor.h -- kernel-owned IIO sensor driver routing.
 */
#ifndef WUBU_SENSOR_H
#define WUBU_SENSOR_H

#include <stddef.h>

/* W1: probe the sensor topology (IIO subsystem). */
void wubu_sensor_probe(void);

/* W2: accessors */
int wubu_sensor_present(void);
int wubu_sensor_count(void);
int wubu_sensor_has_accel(void);
int wubu_sensor_has_gyro(void);
int wubu_sensor_has_mag(void);
int wubu_sensor_has_imu(void);
int wubu_sensor_has_baro(void);
int wubu_sensor_has_als(void);
int wubu_sensor_has_proximity(void);
int wubu_sensor_has_temp(void);
int wubu_sensor_has_humidity(void);

/* W3: driver routing per sensor family. */
const char *wubu_sensor_driver(const char *family);

/* W4: summary fragment. */
int wubu_sensor_summary(char *out, size_t cap);

#endif /* WUBU_SENSOR_H */
