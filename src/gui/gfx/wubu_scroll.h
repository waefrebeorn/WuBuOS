/*
 * wubu_scroll.h -- scrolling arithmetic, with no rendering and no widget state of its own.
 *
 * Every list in the OS (Control Panel pages, Explorer, Start menu) needs the same three answers: how far can
 * this scroll, where is the thumb, and where does a press on the track land. Each surface was going to answer
 * them privately -- that is how two lists end up scrolling at different rates -- so the arithmetic lives here.
 *
 * The numbers come from Microsoft's ScrollViewer guidance, because it is the reference this OS is matching:
 *   - panning indicator 2 px, traditional thumb 6 px;
 *   - "when the scrollbar is visible it is overlaid as 16px on top of the content".
 * A wheel notch moves three rows (Windows' SPI_GETWHEELSCROLLLINES default).
 */
#ifndef WUBU_SCROLL_H
#define WUBU_SCROLL_H

#define WUBU_SCROLL_INDICATOR_W 2   /* Microsoft: the panning indicator */
#define WUBU_SCROLL_THUMB_W     6   /* Microsoft: the traditional thumb   */
#define WUBU_SCROLL_BAND_W      16  /* Microsoft: the overlay hit band    */
#define WUBU_SCROLL_THUMB_MIN   24  /* a thumb shorter than this cannot be grabbed; floor it and let drag scale */
#define WUBU_SCROLL_WHEEL_ROWS  3   /* SPI_GETWHEELSCROLLLINES default    */

typedef struct {
    int content;   /* total height of the content, px */
    int view;      /* visible height, px              */
    int offset;    /* scroll offset, px (0 = top)     */
} WubuScroll;

/* Largest legal offset. 0 when everything fits, which is also what makes the bar not draw. */
int wubu_scroll_max(const WubuScroll *s);

/* Clamp the offset into range and return it. */
int wubu_scroll_clamp(WubuScroll *s);

/* Thumb length and distance from the top of the track. A full-height track means nothing to scroll. */
void wubu_scroll_thumb(const WubuScroll *s, int track, int *len, int *pos);

/* Offset a press at track position y would jump to, keeping the grab point under the pointer (grab_dy is the
 * press offset inside the thumb). Clamped. */
int wubu_scroll_from_thumb_drag(const WubuScroll *s, int track, int y, int grab_dy);

/* Offset after `notches` wheel steps of `row` px each (positive = down). Clamped. */
int wubu_scroll_wheel(WubuScroll *s, int notches, int row);

/* 1 when a press at y is inside the 16 px band (x measured from the band's outer edge, 0..band). */
int wubu_scroll_in_band(int x);

/* 1 when the point is on the thumb (so a drag starts), 0 when it is on the track (so it jumps). */
int wubu_scroll_on_thumb(const WubuScroll *s, int track, int y);

#endif
