/* The bridge between the shell's text face and the framebuffer's scaled text path.
 *
 * Kept in its own file so shell_text itself stays testable without vbe: a unit test of heading
 * geometry should not have to initialise a framebuffer.
 */
#ifndef WUBU_SHELL_TEXT_VBE_H
#define WUBU_SHELL_TEXT_VBE_H

/* Register the face renderer with vbe so magnified text re-rasterises outlines instead of resampling
 * the baked cell. Returns 1 when registered (a face is loaded), 0 otherwise -- in which case the
 * bilinear path stays in place and nothing is worse than before. */
int shell_text_vbe_attach(void);

/* Undo attach (used by shutdown and by tests). */
void shell_text_vbe_detach(void);

#endif
