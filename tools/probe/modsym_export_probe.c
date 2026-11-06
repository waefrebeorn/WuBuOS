/* modsym_export_probe.c — verify P2 Piece B: the NT personality publishes its
 * real handlers into wubu_modsym under their Nt* name, and lookups resolve. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "wubu_modsym.h"
#include "vsl_nt_internal.h"

/* real handlers we can compare addresses against (const-ness irrelevant) */
extern int64_t vsl_nt_close(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
extern int64_t vsl_nt_query_system_time(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
extern int64_t vsl_nt_create_event(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

int main(void) {
    wubu_modsym_clear();
    int n = vsl_nt_export_modsym();
    printf("vsl_nt_export_modsym() -> %d symbols published\n", n);

    int pass = (n > 100);

    uintptr_t a = 0;
    pass &= (wubu_modsym_lookup("NTOSKRNL.exe", "NtClose", &a) == 0);
    printf("  NtClose  -> %p  (vsl_nt_close=%p) match=%d\n",
           (void*)a, (void*)vsl_nt_close, (int)(a == (uintptr_t)(void*)vsl_nt_close));
    pass &= (a == (uintptr_t)(void*)vsl_nt_close);

    a = 0;
    pass &= (wubu_modsym_lookup("ntoskrnl.exe", "NtQuerySystemTime", &a) == 0);
    printf("  NtQuerySystemTime -> %p  (vsl_nt_query_system_time=%p) match=%d\n",
           (void*)a, (void*)vsl_nt_query_system_time,
           (int)(a == (uintptr_t)(void*)vsl_nt_query_system_time));
    pass &= (a == (uintptr_t)(void*)vsl_nt_query_system_time);

    a = 0;
    pass &= (wubu_modsym_lookup("NTOSKRNL.exe", "NtCreateEvent", &a) == 0);
    printf("  NtCreateEvent -> %p  (vsl_nt_create_event=%p) match=%d\n",
           (void*)a, (void*)vsl_nt_create_event, (int)(a == (uintptr_t)(void*)vsl_nt_create_event));
    pass &= (a == (uintptr_t)(void*)vsl_nt_create_event);

    /* case-insensitivity of DLL, case-sensitivity of name, and a dead stub skip */
    a = 1; uintptr_t miss = wubu_modsym_lookup("NTOSKRNL.exe", "ntclose", &a);
    printf("  lookup NtClose (lowercase name) -> miss=%d (expect nonzero, no lower-name export)\n", (int)miss);
    pass &= (miss != 0);
    a = 0; miss = wubu_modsym_lookup("WRONG.dll", "NtClose", &a);
    printf("  lookup WRONG.dll!NtClose -> miss=%d (expect nonzero)\n", (int)miss);
    pass &= (miss != 0);

    /* the export register counts correctly: NtClose exists, so >100 and each */
    printf("\nRESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
