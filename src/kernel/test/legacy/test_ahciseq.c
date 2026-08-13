/* host repro of the bootvol's ahci sequence */
#include <stdio.h>
#include "ahci.h"
#include "ahci.c"

int main(void)
{
    ahci_hba_t *bhba = (ahci_hba_t *)calloc(1, sizeof(ahci_hba_t));
    int d1 = ahci_hba_init(bhba);
    int d2 = ahci_enumerate_ports(bhba);
    int d3 = ahci_port_init(bhba, 0);
    printf("hba=%d en=%d p0-state=%d port_init=%d\n", d1, d2,
           (int)bhba->ports[0].state, d3);
    return 0;
}
