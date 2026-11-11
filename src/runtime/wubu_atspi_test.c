/*
 * wubu_atspi_test.c -- the AT-SPI2 accessibility object model.
 *
 * The load-bearing rule: role names are the CANONICAL AT-SPI strings, so a
 * screen reader announces the right thing.  "check box" is two words, "push
 * button" is not "button", and child_count == -1 means NOT CACHED rather than
 * empty.
 */
#include "wubu_atspi.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_wubu_atspi ===\n");

    /* -- the canonical role names -- */
    CHECK(strcmp(wb_atspi_role_name(WB_ATSPI_ROLE_CHECK_BOX), "check box") == 0,
          "'check box' is TWO WORDS -- a reader must say 'check box'");
    CHECK(strcmp(wb_atspi_role_name(WB_ATSPI_ROLE_PUSH_BUTTON), "push button") == 0,
          "'push button' is NOT 'button'");
    CHECK(strcmp(wb_atspi_role_name(WB_ATSPI_ROLE_MENU_ITEM), "menu item") == 0,
          "'menu item' is two words");
    CHECK(strcmp(wb_atspi_role_name(WB_ATSPI_ROLE_COMBO_BOX), "combo box") == 0,
          "'combo box' is two words");
    CHECK(strcmp(wb_atspi_role_name(WB_ATSPI_ROLE_TABLE_CELL), "table cell") == 0,
          "'table cell' is two words");
    CHECK(strcmp(wb_atspi_role_name(WB_ATSPI_ROLE_APPLICATION), "application") == 0,
          "a single-word role");
    CHECK(strcmp(wb_atspi_role_name(WB_ATSPI_ROLE_WINDOW), "window") == 0,
          "another single-word role");
    CHECK(wb_atspi_role_name(WB_ATSPI_ROLE_INVALID) == NULL,
          "the invalid role has no name");
    CHECK(wb_atspi_role_name((wb_atspi_role_t)999) == NULL,
          "an out-of-range role has no name");

    /* -- the reverse mapping -- */
    CHECK(wb_atspi_role_from_name("check box") == WB_ATSPI_ROLE_CHECK_BOX,
          "the reverse mapping finds the role");
    CHECK(wb_atspi_role_from_name("push button") == WB_ATSPI_ROLE_PUSH_BUTTON,
          "and distinguishes push button from button");
    CHECK(wb_atspi_role_from_name("button") == WB_ATSPI_ROLE_BUTTON,
          "plain 'button' is its own role");
    CHECK(wb_atspi_role_from_name("checkbox") == WB_ATSPI_ROLE_INVALID,
          "the NON-canonical 'checkbox' is rejected (it is not an AT-SPI name)");
    CHECK(wb_atspi_role_from_name("Check Box") == WB_ATSPI_ROLE_INVALID,
          "and the mapping is case-sensitive");
    CHECK(wb_atspi_role_from_name(NULL) == WB_ATSPI_ROLE_INVALID,
          "a null name is invalid");

    /* -- every role round-trips -- */
    {
        int bad = 0;
        for (int i = 1; i < WB_ATSPI_ROLE_COUNT; i++) {
            const char *n = wb_atspi_role_name((wb_atspi_role_t)i);
            if (!n) { bad++; continue; }
            if (wb_atspi_role_from_name(n) != (wb_atspi_role_t)i) bad++;
        }
        CHECK(bad == 0, "every role name round-trips through the reverse mapping");
    }

    /* -- object paths -- */
    CHECK(wb_atspi_valid_path("/"), "the root is a valid path");
    CHECK(wb_atspi_valid_path("/org/a11y/atspi/accessible/root"),
          "a real AT-SPI path is valid");
    CHECK(!wb_atspi_valid_path("org/a11y"), "a relative path is INVALID");
    CHECK(!wb_atspi_valid_path("/trailing/"), "a trailing slash is invalid");
    CHECK(!wb_atspi_valid_path("/double//slash"), "an empty component is invalid");
    CHECK(!wb_atspi_valid_path("/bad-component"), "a hyphen is not allowed in a component");
    CHECK(!wb_atspi_valid_path("/bad.component"), "nor a dot");
    CHECK(!wb_atspi_valid_path(NULL), "a null path is invalid");
    CHECK(!wb_atspi_valid_path(""), "an empty path is invalid");

    /* -- bus names -- */
    CHECK(wb_atspi_valid_bus_name(":1.42"), "a unique name is valid");
    CHECK(wb_atspi_valid_bus_name(":0.1"), "another unique name");
    CHECK(!wb_atspi_valid_bus_name(":1"), "a unique name needs a dot and a counter");
    CHECK(!wb_atspi_valid_bus_name(":1."), "a unique name needs a counter after the dot");
    CHECK(!wb_atspi_valid_bus_name(":a.b"), "a unique name is numeric");
    CHECK(!wb_atspi_valid_bus_name(":"), "an empty unique name is invalid");
    CHECK(wb_atspi_valid_bus_name("org.a11y.Bus"), "a well-known name is valid");
    CHECK(wb_atspi_valid_bus_name("org.gnome.Shell"),
          "another well-known name");
    CHECK(!wb_atspi_valid_bus_name("bare"), "a bare word is NOT a well-known name");
    CHECK(!wb_atspi_valid_bus_name("1org.a11y"),
          "a well-known name cannot start with a digit");
    CHECK(!wb_atspi_valid_bus_name("org..a11y"), "an empty element is invalid");
    CHECK(!wb_atspi_valid_bus_name("org.a11y."), "a trailing dot is invalid");
    CHECK(!wb_atspi_valid_bus_name(NULL), "a null name is invalid");
    CHECK(!wb_atspi_valid_bus_name(""), "an empty name is invalid");

    /* -- nodes -- */
    {
        wb_atspi_node_t n;
        wb_atspi_node_init(&n);
        CHECK(n.role == WB_ATSPI_ROLE_INVALID, "a new node has no role");
        CHECK(n.child_count == -1, "and its child count is NOT CACHED");
        CHECK(!n.has_name, "and no name");
        CHECK(!wb_atspi_has_children(&n),
              "a NOT-CACHED count is not the same as EMPTY (a reader must not "
              "believe the container is empty)");
        CHECK(!wb_atspi_is_describable(&n),
              "an invalid role with no name cannot be described");

        n.child_count = 0;
        CHECK(!wb_atspi_has_children(&n), "zero children means empty");
        n.child_count = 3;
        CHECK(wb_atspi_has_children(&n), "three children means it has children");

        n.role = WB_ATSPI_ROLE_CHECK_BOX;
        n.has_name = true;
        snprintf(n.name, sizeof(n.name), "Enable sounds");
        CHECK(wb_atspi_is_describable(&n),
              "a check box with a name is describable");
        CHECK(strcmp(wb_atspi_role_name(n.role), "check box") == 0,
              "and a reader announces 'check box'");

        /* an unnamed CONTAINER is fine */
        wb_atspi_node_init(&n);
        n.role = WB_ATSPI_ROLE_SECTION;
        CHECK(!n.has_name, "a container may have no name");
        CHECK(wb_atspi_is_describable(&n),
              "but its ROLE still lets a reader describe it");

        CHECK(!wb_atspi_has_children(NULL), "a null node has no children");
        CHECK(!wb_atspi_is_describable(NULL), "a null node is not describable");
        wb_atspi_node_init(NULL);
    }

    if (failures == 0) printf("ALL WUBU_ATSPI TESTS PASSED\n");
    else printf("%d WUBU_ATSPI FAILURES\n", failures);
    return failures ? 1 : 0;
}
