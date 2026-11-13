/*
 * input_keymap_test.c -- what a scancode becomes when it is typed.
 *
 * Regression cover for a bug that mangled every text field in the OS:
 * input_scancode_to_key returned the raw scancode for keys with no text form,
 * and LEFTSHIFT is 0x2A -- which IS '*'. Typing "DOS Box" produced "*D*O*S
 * *Box"; ALT would have contributed '8'. Modifier state must arrive through the
 * modifiers bitmask, never as a key press.
 */

#include "input.h"

#include <stdio.h>

static int g_checks = 0, g_fails = 0;

#define TEST(cond, msg) do {                                            \
        g_checks++;                                                     \
        if (!(cond)) { g_fails++;                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); }    \
    } while (0)

int main(void)
{
    /* ---- modifiers must produce NOTHING --------------------------- */
    TEST(input_scancode_to_key(0x1D, MOD_CTRL)  == 0, "Left Ctrl  -> no key code");
    TEST(input_scancode_to_key(0x2A, MOD_SHIFT) == 0, "Left Shift -> no key code (was '*')");
    TEST(input_scancode_to_key(0x38, MOD_ALT)   == 0, "Left Alt   -> no key code (was '8')");

    /* The right-side variants are translated onto the same values by the
     * hosted input path, so guarding these too keeps that true. */
    TEST(input_scancode_to_key(0xE01D, MOD_CTRL) == 0, "Right Ctrl variant -> none");
    TEST(input_scancode_to_key(0xE02A, MOD_SHIFT) == 0, "Right Shift variant -> none");
    TEST(input_scancode_to_key(0xE038, MOD_ALT)  == 0, "Right Alt variant -> none");

    /* ---- letters keep their case ---------------------------------- */
    TEST(input_scancode_to_key(0x1E, 0) == 'a', "A without shift -> 'a'");
    TEST(input_scancode_to_key(0x1E, MOD_SHIFT) == 'A', "A with shift -> 'A'");
    TEST(input_scancode_to_key(0x20, MOD_SHIFT) == 'D', "D with shift -> 'D'");
    TEST(input_scancode_to_key(0x30, 0) == 'b', "B without shift -> 'b'");
    TEST(input_scancode_to_key(0x2D, MOD_SHIFT) == 'X', "X with shift -> 'X'");

    /* A real sentence's worth of shifted capitals must not grow stray
     * characters: this is exactly the "DOS Box" case. */
    {
        const uint32_t seq[] = { 0x20, 0x18, 0x1F, 0x39, 0x30, 0x18, 0x2D };
        char out[16]; int n = 0;
        for (unsigned i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
            uint32_t k = input_scancode_to_key(seq[i], MOD_SHIFT);
            if (k) out[n++] = (char)k;
        }
        out[n] = '\0';
        TEST(n == 7, "seven keystrokes yield seven characters");
        TEST(out[0] != '*' && out[1] != '*' && out[2] != '*',
             "no asterisks from shift presses");
    }

    /* ---- control keys still map to ASCII controls ---------------- */
    TEST(input_scancode_to_key(0x01, 0) == 0x1B, "Escape -> ESC");
    TEST(input_scancode_to_key(0x0E, 0) == 0x08, "Backspace -> BS");
    TEST(input_scancode_to_key(0x1C, 0) == 0x0D, "Enter -> CR");
    TEST(input_scancode_to_key(0x39, 0) == ' ', "Space -> space");

    /* ---- digits and punctuation ---------------------------------- */
    TEST(input_scancode_to_key(0x02, 0) == '1', "'1'");
    TEST(input_scancode_to_key(0x02, MOD_SHIFT) == '!', "shifted '1' -> '!'");
    TEST(input_scancode_to_key(0x34, 0) == '.', "period");
    TEST(input_scancode_to_key(0x35, MOD_SHIFT) == '?', "slash with shift -> '?'");

    printf("=== input_keymap: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
