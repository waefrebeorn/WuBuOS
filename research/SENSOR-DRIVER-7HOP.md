# SENSOR-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux IIO sensor driver gaps

Every modern device carries 6-10 sensors. WuBuOS routes them through the
IIO (Industrial I/O) subsystem, covering accel/gyro/IMU/mag/baro/ALS.

### Sensor family -> driver routing (wubu_sensor.c)

| Sensor | Chips | Driver |
|--------|-------|--------|
| Accelerometer | lis3dh, bmc150, bma | `st-accel` |
| Gyroscope | bmg160, lsm6dsm | `st-gyro` |
| Magnetometer | ak8963, lis3mdl | `st-magn` |
| IMU | mpu6050, mpu9250, icm45600, bmi160 | `inv-mpu6050` |
| Barometer | bmp280, ms5611 | `bmp280` |
| Ambient light | tsl2561, apds9960, ltr559 | `apds9960` |
| Proximity | apds9960 | `apds9960` |
| Temp/Humidity | bme280, hts221, si7020 | `st-humidity` |

### Kernel summary line

```
sensor[count=0 accel=0 gyro=0 mag=0 imu=0 baro=0 als=0 prox=0 temp=0 hum=0]
```

Published to `/kv/world/hw_sensor` by `wubu_sensor_summary()`.
