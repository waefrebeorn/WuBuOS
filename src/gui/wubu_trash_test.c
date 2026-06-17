#include "wubu_trash.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stdlib.h>

int main(void) {
    printf("Testing Trash system...\n");
    wubu_trash_init();

    TrashState *state = wubu_trash_state();
    printf("Trash dir: %s\n", state->trash_dir);

    /* Create test file */
    const char *test_file = "/tmp/wubu_trash_test.txt";
    FILE *f = fopen(test_file, "w");
    assert(f);
    fprintf(f, "Test content for trash\n");
    fclose(f);

    /* Move to trash */
    int result = wubu_trash_move(test_file);
    assert(result == 0);
    
    /* Verify file is gone from original location */
    struct stat st;
    assert(stat(test_file, &st) != 0); /* Should not exist */

    /* List trash */
    TrashEntry *entries;
    int count;
    wubu_trash_list(&entries, &count);
    assert(count == 1);
    printf("Trash entry: %s (size: %lu)\n", entries[0].original_path, entries[0].size);

    /* Test find by path */
    int idx = wubu_trash_find_by_path(test_file);
    assert(idx == 0);

    /* Test get unique name */
    char unique[256];
    const char *uname = wubu_trash_get_unique_name(test_file, unique, sizeof(unique));
    assert(uname);
    printf("Unique name: %s\n", unique);

    /* Restore */
    char deleted_name[256];
    strncpy(deleted_name, basename(entries[0].deleted_path), sizeof(deleted_name) - 1);
    deleted_name[sizeof(deleted_name) - 1] = '\0';
    
    char restored[4096];
    result = wubu_trash_restore(deleted_name, restored, sizeof(restored));
    assert(result == 0);
    
    /* Verify restored */
    assert(stat(test_file, &st) == 0);
    assert(strcmp(restored, test_file) == 0);
    printf("Restored to: %s\n", restored);

    /* Move again and test empty */
    wubu_trash_move(test_file);
    assert(wubu_trash_state()->entry_count == 1);
    
    wubu_trash_empty();
    assert(wubu_trash_state()->entry_count == 0);
    assert(wubu_trash_state()->current_size_bytes == 0);
    printf("Trash emptied\n");

    /* Clean up */
    unlink(test_file);
    wubu_trash_shutdown();
    printf("✅ All trash tests passed\n");
    return 0;
}