# TPM-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux TPM 2.0 full-stack gaps

The TPM is the hardware root of trust: attestation, PCR, sealing, measured
boot. WuBuOS routes the full stack.

### TPM driver routing (wubu_tpm.c)

| Interface | Driver |
|-----------|--------|
| CRB (ACPI) | `tpm_crb` |
| FIFO/LPC (tis) | `tpm_tis` |
| SPI | `tpm_tis_spi` |
| I2C | `tpm_i2c_atmel` |

### Stack
- Kernel: tpm_tis, tpm_crb, tpm2-space; /dev/tpm0, /dev/tpmrm0
- Userspace: tpm2-tss (libtss2, tpm2-abrmd), tpm2-tools
- Attestation: PCR quote, AK signing; measured boot: PCR[0-7] chain
- Sealing: key sealed to PCR policy

### Kernel summary line

```
tpm[tpm=1 tpm2=0 tss=0 crb=0 measured=0 drv=tpm]
```

Published to `/kv/world/hw_tpm` by `wubu_tpm_summary()`.

**Verified live:** this host reports `tpm=1` — a real TPM detected.
