/* test_nt_ob.c -- NT object namespace over /n: the four path semantics + paging. */
#include "vsl_nt_ob.h"
#include <stdio.h>
#include <string.h>

static int checks, fails;
#define CHECK(c, m) do { checks++; if (!(c)) { printf("  [FAIL] %s\n", m); fails++; } } while (0)

int main(void)
{
    char out[256];
    int rc;

    /* --- canonicalisation --- */
    rc = vsl_nt_ob_canon("\\??\\C:\\Brain\\Kv\\slot0", out, sizeof out);
    printf("  win32 : %s -> %s\n", rc == 0 ? "ok" : "ERR", out);
    CHECK(rc == NT_STATUS_SUCCESS && strcmp(out, "/n/brain/kv/slot0") == 0,
          "\\??\\C:\\Brain\\Kv\\slot0 canonicalises under /n");

    char b[256];
    CHECK(vsl_nt_ob_canon("/N/BRAIN/KV/SLOT0", b, sizeof b) == NT_STATUS_SUCCESS &&
          strcmp(b, out) == 0, "mixed case resolves to the same path");
    CHECK(vsl_nt_ob_canon("\\\\??\\C:\\Brain\\Kv\\slot0", b, sizeof b) == NT_STATUS_SUCCESS &&
          strcmp(b, out) == 0, "UNC-style ?? prefix also canonicalises");

    CHECK(vsl_nt_ob_canon("\\Brain\\Kv", b, sizeof b) == NT_STATUS_SUCCESS &&
          strcmp(b, "/n/brain/kv") == 0, "a bare object path is rooted under /n");
    CHECK((uint32_t)vsl_nt_ob_canon("/n/../etc/passwd", b, sizeof b) == NT_STATUS_OBJECT_PATH_INVALID,
          ".. escaping the root is rejected");
    CHECK(vsl_nt_ob_canon("/n/brain/./kv//slot0", b, sizeof b) == NT_STATUS_SUCCESS &&
          strcmp(b, "/n/brain/kv/slot0") == 0, "dot segments and doubled slashes collapse");

    /* --- access mask translation --- */
    int mode; uint32_t un;
    CHECK(vsl_nt_access_to_9p(0x80000000u, &mode, &un) == NT_STATUS_SUCCESS && mode == 0,
          "GENERIC_READ -> OREAD");
    CHECK(vsl_nt_access_to_9p(0xC0000000u, &mode, &un) == NT_STATUS_SUCCESS && mode == 2,
          "GENERIC_READ|GENERIC_WRITE -> ORDWR");
    CHECK(vsl_nt_access_to_9p(0x40000000u, &mode, &un) == NT_STATUS_SUCCESS && mode == 1,
          "GENERIC_WRITE -> OWRITE");
    CHECK(vsl_nt_access_to_9p(0x00010000u, &mode, &un) == NT_STATUS_SUCCESS && mode == 4,
          "DELETE -> ORCLOSE");
    CHECK(vsl_nt_access_to_9p(0x00100000u, &mode, &un) == NT_STATUS_SUCCESS && mode == 0,
          "SYNCHRONIZE -> read mode");
    rc = vsl_nt_access_to_9p(0x00000001u, &mode, &un);
    CHECK((uint32_t)rc == NT_STATUS_ACCESS_DENIED && un == 0x00000001u,
          "a mask with no 9P equivalent is denied and the bit is reported");

    /* --- node tree --- */
    ob_tree_t t;
    ob_tree_init(&t);
    CHECK(ob_tree_add(&t, "/n/brain/kv/slot0", 0, OB_RIGHT_READ) == 0, "node added");
    CHECK(ob_tree_add(&t, "/n/brain/kv/slot1", 0, OB_RIGHT_READ | OB_RIGHT_WRITE) == 0,
          "second node added");
    CHECK(ob_tree_add(&t, "/n/models/llama.bin", 0, OB_RIGHT_READ) == 0, "third node added");

    uint32_t h = 0; uint64_t rights = 0;
    rc = vsl_nt_ob_open(&t, "\\??\\C:\\Brain\\Kv\\slot0", 0x80000000u, 100, &h, &rights);
    CHECK(rc == NT_STATUS_SUCCESS && h > 0 && rights == OB_RIGHT_READ,
          "open-by-name resolves through /n and returns a handle");
    CHECK(vsl_nt_ob_open(&t, "/n/brain/kv/slot0", 0x40000000u, 100, &h, &rights)
          == NT_STATUS_ACCESS_DENIED, "write on a read-only node is denied");
    CHECK(vsl_nt_ob_open(&t, "/n/brain/kv/nope", 0x80000000u, 100, &h, &rights)
          == NT_STATUS_OBJECT_NAME_NOT_FOUND, "missing node is NOT_FOUND");

    /* --- directory enumeration with paging --- */
    int cursor = 0, more = 1, pages = 0;
    char name[128];
    char seen[8][128]; int nseen = 0;
    while (more) {
        rc = vsl_nt_ob_query_dir(&t, "/n/brain/kv", &cursor, name, sizeof name, &more);
        if (rc != NT_STATUS_SUCCESS) break;
        int dup = 0;
        for (int i = 0; i < nseen; i++) if (strcmp(seen[i], name) == 0) dup = 1;
        CHECK(!dup, "paged enumeration never repeats an entry");
        snprintf(seen[nseen], sizeof seen[0], "%s", name); nseen++;
        pages++;
    }
    printf("  paged %d entries from /n/brain/kv\n", pages);
    CHECK(pages == 2, "two entries under /n/brain/kv");
    more = 1;
    rc = vsl_nt_ob_query_dir(&t, "/n/brain/kv", &cursor, name, sizeof name, &more);
    CHECK((uint32_t)rc == NT_STATUS_NO_MORE_ENTRIES, "enumeration ends with NO_MORE_ENTRIES");

    int again = 0, more2 = 1, p2 = 0;
    while (more2) {
        if (vsl_nt_ob_query_dir(&t, "/n/brain/kv", &again, name, sizeof name, &more2)
            != NT_STATUS_SUCCESS) break;
        p2++;
    }
    CHECK(p2 == pages, "restart scan enumerates the identical set");

    /* --- reparse symlink --- */
    CHECK(ob_tree_symlink(&t, "/n/alias", "/n/brain/kv/slot0") == 0, "symlink created");
    char resolved[256];
    CHECK(vsl_nt_ob_resolve(&t, "/n/alias", resolved, sizeof resolved) == NT_STATUS_SUCCESS &&
          strcmp(resolved, "/n/brain/kv/slot0") == 0, "reparse resolves to its target");
    uint32_t ah = 0;
    CHECK(vsl_nt_ob_open(&t, "/n/alias", 0x80000000u, 100, &ah, &rights) == NT_STATUS_SUCCESS,
          "open through the symlink succeeds");
    CHECK(vsl_nt_ob_mkdir(&t, "/n/brain/kv/hot") == NT_STATUS_SUCCESS, "mkdir creates a node");
    CHECK(vsl_nt_ob_mkdir(&t, "/n/brain/kv/hot") == NT_STATUS_OBJECT_NAME_COLLISION,
          "duplicate mkdir collides");

    /* --- cap-backed authority (goal 399) --- */
    wubu_cap_init();
    wubu_cap_token_t ctok;
    CHECK(vsl_nt_ob_bind_cap(&t, "/n/brain/kv/slot0", WUBU_RIGHT_READ, 100, &ctok)
          == NT_STATUS_SUCCESS, "node bound to a STYX_NODE capability");
    CHECK(!wubu_cap_token_is_null(ctok), "binding returns a live token");

    uint32_t ch = 0; uint64_t cr = 0;
    CHECK(vsl_nt_ob_open(&t, "/n/brain/kv/slot0", 0x80000000u, 100, &ch, &cr) == NT_STATUS_SUCCESS,
          "audience member opens through the capability");
    CHECK(cr == WUBU_RIGHT_READ, "returned rights equal the cap rights");
    CHECK(vsl_nt_ob_open(&t, "/n/brain/kv/slot0", 0x80000000u, 999, &ch, &cr)
          == NT_STATUS_ACCESS_DENIED, "a process outside the audience is denied");
    CHECK(vsl_nt_ob_open(&t, "/n/brain/kv/slot0", 0x40000000u, 100, &ch, &cr)
          == NT_STATUS_ACCESS_DENIED, "cap without write rights refuses write");

    uint32_t oh = 0; uint64_t orights = 0;
    CHECK(ObOpenByKeyName(&t, "\\Brain\\Kv\\slot0", 0x80000000u, 100, &oh, &orights)
          == NT_STATUS_SUCCESS, "ObOpenByKeyName opens the cap-backed node");
    CHECK(orights == WUBU_RIGHT_READ, "ObOpenByKeyName reports the cap rights");
    CHECK(ObOpenByKeyName(&t, "\\Brain\\Kv\\slot0", 0x80000000u, 999, &oh, &orights)
          == NT_STATUS_ACCESS_DENIED, "ObOpenByKeyName denies a non-audience caller");

    wubu_cap_revoke(ctok);
    CHECK(vsl_nt_ob_open(&t, "/n/brain/kv/slot0", 0x80000000u, 100, &ch, &cr)
          == NT_STATUS_ACCESS_DENIED, "a revoked capability denies even its audience");

    /* --- 425: directory object creation + enumeration --- */
    uint32_t dh = 0; wubu_cap_token_t dcap;
    CHECK(NtCreateDirectoryObject(&t, "\\Brain\\Kv\\dir425", 100, &dh, &dcap) == NT_STATUS_SUCCESS,
          "NtCreateDirectoryObject creates the directory");
    CHECK(dh > 0 && !wubu_cap_token_is_null(dcap), "directory is cap-backed");
    char back[256];
    CHECK(vsl_nt_ob_canon("\\Brain\\Kv\\dir425", back, sizeof back) == NT_STATUS_SUCCESS &&
          strcmp(back, "/n/brain/kv/dir425") == 0, "directory is visible as /n/brain/kv/dir425");
    CHECK(NtCreateDirectoryObject(&t, "/n/brain/kv/dir425", 100, &dh, &dcap)
          == NT_STATUS_OBJECT_NAME_COLLISION, "duplicate directory collides");

    char a[128], b2[128], c2[128];
    int cc = 0, mm = 1;
    CHECK(NtQueryDirectoryObject(&t, "/n/brain/kv", &cc, 1, a, sizeof a, &mm) == NT_STATUS_SUCCESS,
          "NtQueryDirectoryObject returns the first entry");
    int cc2 = 0, mm2 = 1;
    CHECK(NtQueryDirectoryObject(&t, "/n/brain/kv", &cc2, 0, b2, sizeof b2, &mm2) == NT_STATUS_SUCCESS,
          "second entry follows");
    /* the same set as the 9P-style readdir */
    int rc3 = 0, qc = 0, mm3 = 1, found_hot = 0, count = 0;
    while (1) {
        rc3 = vsl_nt_ob_query_dir(&t, "/n/brain/kv", &qc, c2, sizeof c2, &mm3);
        if (rc3 != NT_STATUS_SUCCESS) break;
        count++;
        if (strcmp(c2, "dir425") == 0) found_hot = 1;
    }
    printf("  readdir sees %d entries under /n/brain/kv\n", count);
    CHECK(count == 4 && found_hot, "NtCreateDirectoryObject entry appears in readdir too");

    /* --- 427: paged query with restart-scan identity --- */
    for (int i = 0; i < 3; i++) {
        char pth[64];
        snprintf(pth, sizeof pth, "/n/models/m%d.bin", i);
        ob_tree_add(&t, pth, 0, OB_RIGHT_READ);
    }
    int qcur = 0, qmore = 1, qpages = 0;
    char qnm[64], first_set[8][64]; int nfirst = 0;
    while (1) {
        int r = NtQueryDirectoryFile(&t, "/n/models", &qcur, 0, qnm, sizeof qnm, &qmore);
        if (r != NT_STATUS_SUCCESS) { rc = r; break; }
        snprintf(first_set[nfirst], sizeof first_set[0], "%s", qnm); nfirst++;
        qpages++;
    }
    printf("  NtQueryDirectoryFile paged %d entries\n", qpages);
    CHECK((uint32_t)rc == NT_STATUS_NO_MORE_ENTRIES, "enumeration ends with NO_MORE_ENTRIES");
    CHECK(qpages == 4, "all four /n/models entries returned exactly once");

    qcur = 99; qmore = 1;
    int n2 = 0, identical = 1;
    while (1) {
        int r = NtQueryDirectoryFile(&t, "/n/models", &qcur, n2 == 0 ? 1 : 0,
                                     qnm, sizeof qnm, &qmore);
        if (r != NT_STATUS_SUCCESS) break;
        if (n2 < nfirst && strcmp(qnm, first_set[n2]) != 0) identical = 0;
        n2++;
    }
    CHECK(n2 == qpages && identical, "restart scan enumerates the identical set in the same order");

    /* --- 428: Tstat reports the symlink and both views agree --- */
    CHECK(ob_tree_symlink(&t, "/n/link", "/n/brain/kv/slot0") == 0, "second symlink created");
    styx_dir_t sd;
    CHECK(vsl_nt_ob_stat(&t, "/n/link", &sd) == NT_STATUS_SUCCESS, "stat of a symlink succeeds");
    CHECK((sd.mode & STX_DMSYMLINK) != 0, "Tstat reports DMSYMLINK for the node");
    CHECK((sd.mode & STX_DMDIR) == 0, "a symlink is not reported as a directory");
    char ltarget[256], rtarget[256];
    CHECK(vsl_nt_ob_readlink(&t, "/n/link", ltarget, sizeof ltarget) == NT_STATUS_SUCCESS,
          "readlink returns the target");
    CHECK(vsl_nt_ob_resolve(&t, "/n/link", rtarget, sizeof rtarget) == NT_STATUS_SUCCESS &&
          strcmp(ltarget, rtarget) == 0, "the 9P view and the NT resolve view agree");
    styx_dir_t fd;
    CHECK(vsl_nt_ob_stat(&t, "/n/brain/kv/slot0", &fd) == NT_STATUS_SUCCESS &&
          (fd.mode & STX_DMSYMLINK) == 0, "a plain file is not a symlink");

    printf("\n=== test_nt_ob: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
