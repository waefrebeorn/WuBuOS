# DMCRYPT-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux dm-crypt/LUKS gaps

dm-crypt encrypts block devices; LUKS is the key setup format.

### Cipher routing (wubu_dmcrypt.c)

| Cipher | Routing |
|--------|---------|
| aes | `aes` |
| serpent | `serpent` |
| twofish | `twofish` |
| camellia | `camellia` |
| else | `aes` |

### Mode routing

| Mode | Routing |
|------|---------|
| xts | `xts` |
| cbc | `cbc` |
| ecb | `ecb` |
| gcm | `gcm` |
| else | `xts` |

### Kernel summary

```
dmcrypt[crypt=0 luks=0 aes=0 xts=0 dm=0 drv=none]
```

Published to `/kv/world/hw_dmcrypt`. (No dm-crypt/LUKS on WSL2.)
