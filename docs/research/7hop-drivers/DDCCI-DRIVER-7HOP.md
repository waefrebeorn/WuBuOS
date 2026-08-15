# DDCCI-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux display DDC/CI gaps

DDC/CI controls brightness/contrast/OSD over the I2C DDC bus.

### Command routing (wubu_ddcci.c)

| Command | Opcode |
|---------|--------|
| brightness / vcb | `0x10` |
| contrast | `0x12` |
| osd | `0x60` |
| power | `0x6D` |
| input | `0x60` |
| else | `0x10` |

### Bus routing

| Bus | Routing |
|-----|---------|
| ddc | `ddc-bus` |
| i2c | `i2c-bus` |
| cec | `cec-bus` |
| else | `ddc-bus` |

### Kernel summary

```
ddcci[ddc=0 i2c=0 cec=0 edid=0 ctrl=0 drv=none]
```

Published to `/kv/world/hw_ddcci`. (No i2c-adapter on WSL2.)
