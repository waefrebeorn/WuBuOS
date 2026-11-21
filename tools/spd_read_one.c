/* spd_read_one.c -- print the real DDR5 SPD hub temperature in Celsius.
 * Reads through the production wubu_thermal_ram() owner; -1 means no sensor.
 * Used by the thermal-watch cron script via a tightly scoped sudoers rule.
 */
#include <stdio.h>
#include "wubu_thermal_read.h"

int main(void)
{
    wubu_thermal_refresh();
    printf("%d\n", wubu_thermal_ram());
    return 0;
}
