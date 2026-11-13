/*
 * wubu_thermal_read.h -- read REAL temperatures from the host sensors.
 *
 * WHY THIS EXISTS
 *   Two things were wrong before this file existed.
 *
 *   1. The kernel thermal driver's state was written only by tests, so the
 *      System card showed stale zeros forever regardless of machine heat.
 *   2. The hosted shim DID read sensors, but badly: for the GPU it searched
 *      for a zone whose type contained "gpu", found none on any normal laptop,
 *      and silently returned the FIRST zone it had seen -- usually `acpitz`,
 *      the ACPI ambient sensor.  So "GPU 72 C" actually meant "somewhere on
 *      the motherboard is 72 C".  A plausible-looking wrong number is worse
 *      than no number at all.
 *
 * WHAT IT READS
 *   CPU:  thermal_zoneN/type == x86_pkg_temp, then cpu-thermal, then
 *         hwmon `coretemp` temp1_input.
 *   GPU:  NVML via dlopen (so there is no link-time dependency) -- NVIDIA
 *         exposes temperature ONLY through NVML, never through sysfs.  Falls
 *         back to hwmon amdgpu / nouveau / radeon / i915.
 *   SSD:  hwmon `nvme` temp1_input, surfaced separately.
 *
 * HONESTY RULE
 *   An absent or unreadable sensor yields -1, NEVER 0.  Zero degrees is a
 *   reading; "we found no sensor" is not.  Callers must render -1 as unknown.
 */
#ifndef WUBU_THERMAL_READ_H
#define WUBU_THERMAL_READ_H

/* Temperature for WUBU_THERMAL_CPU or WUBU_THERMAL_GPU in Celsius, or -1 when
 * no such sensor exists on this machine.  Re-reads at most once per second. */
int wubu_thermal_read_temp(int zone);

/* Last NVMe drive temperature in Celsius, or -1 if none.  Deliberately NOT
 * filed under WUBU_THERMAL_SKIN: skin means body temperature, and quietly
 * reporting a drive temperature as one would be a lie. */
int wubu_thermal_nvme(void);

/* Last memory (DIMM / SPD hub) temperature in Celsius, or -1 if the machine
 * exposes none.  Most laptops do NOT -- returning 0 here would claim the RAM is
 * freezing, which is a measurement nobody took. */
int wubu_thermal_ram(void);

/* Force a re-read now, ignoring the one-second cache. */
void wubu_thermal_refresh(void);

/* True when a CPU (resp. GPU) sensor was found on the last refresh. */
int wubu_thermal_have_cpu(void);
int wubu_thermal_have_gpu(void);

#endif /* WUBU_THERMAL_READ_H */
