/*
 * wubucontainer_test.c -- Regression test for wubu_container_register_handler.
 *
 * Verifies the handler registry is real (not the old INVAL no-op): a valid
 * handler registers and increments the count, a duplicate is rejected, and a
 * NULL/empty handler is rejected with INVAL.
 */
#include "container/wubucontainer.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void) {
    printf("Testing container handler registry...\n");

    WubuContainerEngine *engine = NULL;
    int rc = wubu_container_init(&engine, "/tmp/wubu_container_test_dir");
    assert(rc == 0 && engine != NULL);

    /* NULL/invalid inputs are still rejected (defensive, not a no-op). */
    WubuContainerHandler h = {0};
    assert(wubu_container_register_handler(NULL, &h) == WUBU_CTR_ERR_INVAL);
    assert(wubu_container_register_handler(engine, NULL) == WUBU_CTR_ERR_INVAL);
    assert(wubu_container_register_handler(engine, &h) == WUBU_CTR_ERR_INVAL); /* empty name */

    /* Real work: register a valid handler. */
    strncpy(h.name, "FFmpeg", sizeof(h.name) - 1);
    strncpy(h.handler, "ffmpeg", sizeof(h.handler) - 1);
    rc = wubu_container_register_handler(engine, &h);
    assert(rc == 0);
    assert(engine->custom_handler_count == 1);
    printf("Registered handler '%s' (count=%d)\n", h.name, engine->custom_handler_count);

    /* Duplicate name is rejected. */
    assert(wubu_container_register_handler(engine, &h) == WUBU_CTR_ERR_INVAL);

    /* A second, distinct handler registers. */
    WubuContainerHandler h2 = {0};
    strncpy(h2.name, "ImageMagick", sizeof(h2.name) - 1);
    strncpy(h2.handler, "convert", sizeof(h2.handler) - 1);
    rc = wubu_container_register_handler(engine, &h2);
    assert(rc == 0);
    assert(engine->custom_handler_count == 2);
    printf("Registered handler '%s' (count=%d)\n", h2.name, engine->custom_handler_count);

    /* The registry actually stored the descriptors. */
    assert(strncmp(engine->custom_handlers[0].name, "FFmpeg", 7) == 0);
    assert(strncmp(engine->custom_handlers[1].name, "ImageMagick", 11) == 0);

    wubu_container_shutdown(engine);
    printf("✅ Container handler registry test passed\n");
    return 0;
}
