# SECUREKEY-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux security key / TOTP / TPM gaps

The security tier: FIDO2 keys, smart-card readers, TPM. WuBuOS routes them.

### Security device routing (wubu_securekey.c)

| Device | Driver |
|--------|--------|
| FIDO2/U2F key (YubiKey, Titan) | `hid-fido2` |
| Smart card reader (CCID) | `ccid` |
| TPM FIFO (LPC) | `tpm_tis` |
| TPM CRB (ACPI) | `tpm_crb` |

### Components
- FIDO2: hid-u2f / hid-fido2 kernel drivers for hardware tokens
- CCID: pcscd + ccid driver for smart-card readers
- TPM: tpm_tis, tpm_crb; /dev/tpm0, /dev/tpmrm0 (TPM 2.0)
- TOTP: hardware OTP tokens (YubiKey OTP, FIDO)

### Kernel summary line

```
sec[fido=0 ccid=0 tpm=0 drv=none type=-]
```

Published to `/kv/world/hw_sec` by `wubu_securekey_summary()`.
