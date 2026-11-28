/* See shell_text_vbe.h. */
#include "shell_text_vbe.h"

#include <stddef.h>

#include "../kernel/vbe.h"
#include "shell_text.h"
#include "wubu_text_face.h"

/* vbe passes the box TOP; the face wants a baseline. y is otherwise identical, so the adapter is a
 * single addition by the face's own ascent at that ppem -- no magic offset. */
static int adapter(int x, int y, const char *s, uint32_t colour, int ppem)
{
    WubuTextFace *f = shell_text_face();
    if (!f || !s || ppem <= 0) return -1;
    int stride = 0, height = 0;
    uint32_t *fb = vbe_draw_surface(&stride, &height);
    if (!fb || stride <= 0 || height <= 0) return -1;
    const int baseline = y + wubu_text_face_ascent_px(f, ppem);
    return wubu_text_face_draw(f, fb, stride, height, x, baseline, s, ppem, colour & 0xFFFFFFu);
}

int shell_text_vbe_attach(void)
{
    if (!shell_text_ready()) return 0;
    vbe_set_face_text(adapter);
    return 1;
}

void shell_text_vbe_detach(void) { vbe_set_face_text(NULL); }
