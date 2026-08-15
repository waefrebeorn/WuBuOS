# POWER-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux CPU/power/thermal driver gaps

Power is the efficiency spine of "runs on everything". WuBuOS owns the
cpufreq driver + governor selection per CPU vendor, C-state tuning for
latency, battery routing, and thermal policy.

### cpufreq driver per CPU vendor

| Vendor | Driver | Governor | WuBuOS route |
|--------|--------|----------|--------------|
| Intel | `intel_pstate` (active/HWP) | schedutil/performance | `wubu_power_cpufreq_driver()` |
| AMD | `amd_pstate` (Epp/guided) | schedutil | `wubu_power_cpufreq_driver()` |
| ARM | `cpufreq-dt` | schedutil | `wubu_power_cpufreq_driver()` |
| legacy x86 | `acpi-cpufreq` | ondemand/performance | `wubu_power_cpufreq_driver()` |

### C-state tuning (latency)

Deep C-states (C6/C7/C10) add wake latency — bad for gaming/audio. WuBuOS
caps depth via `intel_idle.max_cstate=4` (Intel) or
`processor.max_cstate=4` (AMD). `wubu_power_cstate_cap()`.

### Battery + thermal

- Battery: `power_supply` class, `/sys/class/power_supply/BAT0`. Charge
  threshold control (Framework-style) extends lifespan.
- Thermal: `thermal_zoneN` + `cooling_deviceN` (fan/PWM/processor).
  `wubu_power_has_thermal()` / `wubu_power_has_fan()`.

### Kernel summary line

```
power[cpu=2 cores=12 cpufreq=amd_pstate gov=schedutil bat=1 therm=0 fan=1 cstate=processor.max_cstate=4]
```

Published to `/kv/world/hw_power` by `wubu_power_summary()`.

Verified live: on the WSL2 host this detected an AMD CPU (cpu=2), 12 cores,
amd_pstate, battery, and fan — the routing is real.
