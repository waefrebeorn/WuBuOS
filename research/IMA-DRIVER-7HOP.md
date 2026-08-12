# IMA-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux IMA/EVM measured boot gaps

IMA + EVM provide file integrity + measurement on Linux.

### IMA/EVM routing (wubu_ima.c)

| Component | Role |
|-----------|------|
| IMA | /sys/kernel/security/ima |
| EVM | /sys/kernel/security/evm (HMAC) |
| IMA policy | /sys/kernel/security/ima/policy |
| measured boot | TPM PCR extends |

### Modes: measure / appraise / audit

### Policies: tcb / ape / ltcb / critical-data

### Kernel summary line

```
ima[ima=0 evm=0 measure=0 appraise=0 pcr=1 drv=none]
```

Published to `/kv/world/hw_ima` by `wubu_ima_summary()`.

**Verified live:** this host reports `pcr=1`.
