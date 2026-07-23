/*
 * holyc_era_demo.hc -- HolyC / TempleOS era demo (2020)
 *
 * A HolyC source string run INSIDE WuBuOS through the HolyC personality:
 *   wubu_exec_holyc() -> hc_eval() (the in-process HolyC JIT compiler).
 *
 * It writes a marker file via HolyC's file API and prints a banner,
 * proving the TempleOS personality is reachable as a first-class process.
 *
 * Note: this is SOURCE, not a binary -- HolyC is JIT-compiled at launch,
 * exactly like the HolyC Terminal in the desktop. The launcher passes the
 * file bytes straight to hc_eval().
 */
Print("WuBuOS HolyC/TempleOS era: hello from the JIT compiler\n");
FpWriteFile("WUBU_ERA_HOLYC.OK", "HolyC (TempleOS) personality exercised by WuBuOS\n");
