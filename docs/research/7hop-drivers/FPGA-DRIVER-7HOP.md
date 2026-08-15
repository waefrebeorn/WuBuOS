# FPGA-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux FPGA driver gaps

FPGAs on adapters/SoCs need the FPGA manager (bitstream load), regions
(DT overlay), and bridges. WuBuOS routes them.

### FPGA manager routing (wubu_fpga.c)

| Vendor | Driver |
|--------|--------|
| Xilinx | `xilinx-pr-decoupler` |
| Altera/Intel | `altera-fpga2sdram` |
| Lattice | `lattice-ecp3` |
| Microsemi | `microsemi-spi` |
| Zynq | `xilinx-pr-decoupler` |

### Subsystems
- fpga-mgr: bitstream load (/sys/class/fpga_manager)
- fpga-region: DT overlay programming (Zynq PL, SoCFPGA)
- fpga-bridge: freeze bridges (altera-freeze-bridge, xlnx-pr-decoupler)

### Kernel summary line

```
fpga[present=0 mgr=0 region=0 bridge=0 drv=none]
```

Published to `/kv/world/hw_fpga` by `wubu_fpga_summary()`.
