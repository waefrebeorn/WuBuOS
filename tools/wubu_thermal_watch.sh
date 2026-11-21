#!/bin/bash
# wubu_thermal_watch.sh -- report only when something is genuinely HOT.
#
# Silent (empty stdout) when everything is within limits, so the cron delivery is quiet
# by default rather than noisy every tick.
#
# TEMPERATURE SOURCES, best available first:
#   CPU  = coretemp package
#   RAM  = real DDR5 SPD hub via wubu_thermal_ram() (Intel PCH SMBus), root helper
#   GPU  = NVML via nvidia-smi
#   SSD  = nvme
#   FANS = msi_wmi_platform RPMs (READ-ONLY on this machine)
# acpitz is a PLATFORM ADVISORY only.  It is a synthesized/worst-case zone and
# must never drive warn/crit escalation; doing so caused false 90C alarms while
# CPU/GPU/RAM were all normal.  Only real component sensors are alarm inputs.
#
# WHY NO FAN CONTROL IS HARDCODED: there is no PWM/pwm*_enable attribute in any hwmon
# driver here, and the four fan nodes are read-only RPM.  Writing the EC directly over
# port I/O is the only remaining route and can brick a laptop if the offsets are wrong,
# so this script never does it.  Instead it calls $WUBU_FAN_RAMP_CMD with the hottest
# sensor name and its temperature, once per escalation, and lets the owner wire a real
# controller (msi-ec, NBFC, etc.) to that hook.

STATE_DIR="$HOME/.hermes/state"
mkdir -p "$STATE_DIR"
LEVEL_FILE="$STATE_DIR/wubu_thermal_level"

WARN="${WUBU_THERMAL_WARN:-80}"
CRIT="${WUBU_THERMAL_CRIT:-92}"

read_hwmon_temp() {  # $1 = driver name -> prints millidegrees of first temp*_input
    for h in /sys/class/hwmon/hwmon*; do
        [ "$(cat "$h/name" 2>/dev/null)" = "$1" ] || continue
        for t in "$h"/temp*_input; do
            [ -r "$t" ] && cat "$t" 2>/dev/null && return 0
        done
    done
    return 1
}

C_to_mC() { echo $(( $1 * 1000 )); }

samples=()

# --- RAM: real SPD hub, explicit override, or nothing (never invented) -----
if [ -n "$WUBU_SPD_READER" ]; then
    ram=$("$WUBU_SPD_READER" 2>/dev/null)
else
    ram=$(sudo -n /home/wubu/wubuos/tools/spd_read_one 2>/dev/null)
fi
case "$ram" in
    ''|*[!0-9-]*) ;;
    -*) : ;;                     # -1: no sensor, fail closed
    *) samples+=("ram:$(( ram * 1000 ))") ;;
esac
if [ -z "$ram" ] || [ "$ram" -lt 0 ]; then
    if [ -n "$WUBU_RAM_TEMP_SYSFS" ] && [ -r "$WUBU_RAM_TEMP_SYSFS" ]; then
        v=$(cat "$WUBU_RAM_TEMP_SYSFS" 2>/dev/null)
        case "$v" in ''|*[!0-9-]*) : ;; *) samples+=("ram:$v") ;; esac
    fi
fi

# --- CPU ------------------------------------------------------------------
v=$(read_hwmon_temp coretemp); [ -n "$v" ] && samples+=("cpu:$v")

# --- SSD ------------------------------------------------------------------
for h in /sys/class/hwmon/hwmon*; do
    [ "$(cat "$h/name" 2>/dev/null)" = "nvme" ] || continue
    for t in "$h"/temp1_input; do
        [ -r "$t" ] && samples+=("ssd:$(cat "$t" 2>/dev/null)")
    done
done

# --- GPU: NVML preferred, hwmon fallback -----------------------------------
gpu=$(nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits 2>/dev/null | head -1)
case "$gpu" in
    ''|*[!0-9]*) gpu=$(read_hwmon_temp iwlwifi_1 >/dev/null 2>&1; echo "") ;;
    *) samples+=("gpu:$(C_to_mC "$gpu")") ;;
esac

# --- Fans: RPM only, never a control path ----------------------------------
fans=""
for f in /sys/class/hwmon/hwmon*/fan*_input; do
    [ -r "$f" ] || continue
    rpm=$(cat "$f" 2>/dev/null)
    case "$rpm" in ''|*[!0-9]*) continue ;; esac
    [ "$rpm" -gt 0 ] && fans="$fans $rpm"
done

# --- Decide ----------------------------------------------------------------
hottest=""; hot_val=0
for s in "${samples[@]}"; do
    n=${s%%:*}; v=${s##*:}
    case "$v" in ''|*[!0-9]*) continue ;; esac
    if [ "$v" -gt "$hot_val" ]; then hot_val=$v; hottest=$n; fi
done

if [ -z "$hottest" ]; then
    exit 0                                    # no sensor readable: stay silent
fi

level=0
[ "$hot_val" -ge "$(C_to_mC "$WARN")" ]  && level=1
[ "$hot_val" -ge "$(C_to_mC "$CRIT")" ] && level=2

prev=$(cat "$LEVEL_FILE" 2>/dev/null || echo 0)

# Notify ONLY on an ESCALATION, so a sustained 90C is not re-reported every tick and a
# healthy machine stays completely silent. Level 0 clears the remembered level so a
# later rise alerts again from scratch.
if [ "$level" -eq 0 ]; then
    [ "$prev" != 0 ] && echo 0 > "$LEVEL_FILE"
    exit 0
fi
if [ "$level" -le "$prev" ]; then
    exit 0
fi

deg=$(( hot_val / 1000 ))
{
    echo "THERMAL WATCH: ${hottest} at ${deg}C (warn=${WARN}C crit=${CRIT}C)."
    echo
    echo "sensors:"
    for s in "${samples[@]}"; do
        n=${s%%:*}; v=${s##*:}
        printf '  %-6s %dC\n' "$n" $(( v / 1000 ))
    done
    if [ -z "$ram" ] || [ "$ram" -lt 0 ]; then
        echo "  ram    n/a (SPD hub unreadable; no override)"
    fi
    acpi=$(read_hwmon_temp acpitz)
    [ -n "$acpi" ] && echo "  acpi   $(( acpi / 1000 ))C (platform advisory; not an alarm input)"
    [ -n "$fans" ] && echo "fans RPM:$fans"

    if [ "$level" -eq 2 ]; then
        echo
        echo "CRITICAL: ramp fans."
        if [ -n "$WUBU_FAN_RAMP_CMD" ]; then
            echo "  invoking: $WUBU_FAN_RAMP_CMD $hottest $deg"
            # shellcheck disable=SC2086
            eval "\"$WUBU_FAN_RAMP_CMD\" $hottest $deg" 2>&1 |
                sed 's/^/    /'
        else
            echo "  no fan control wired. Set WUBU_FAN_RAMP_CMD to a controller"
            echo "  (this machine exposes only read-only fan RPM; no PWM node exists)."
        fi
    fi
} 

echo "$level" > "$LEVEL_FILE"
