# DSPTRACE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio DSP trace/debug gaps

DSP firmware exposes trace buffers and debug channels for the kernel
to inspect DSP state (load, xruns, overruns, firmware errors).

### Impl routing (wubu_dsptrace.c)

| Route | Path |
|-------|------|
| Debugfs presence | /sys/kernel/debug |
| SOF DSP config    | /sys/module/snd_sof/parameters |

Trace events: ok(0), xrun(1), overrun(2), firmware_error(3).
LogLevel clamped 0-4.
