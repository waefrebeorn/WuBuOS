/* wubu_screenshot_draw.c -- Screenshot annotation drawing primitives (self-contained).
 *
 * draw_line (Bresenham) / draw_rect / plot_ellipse_points / draw_ellipse /
 * draw_arrow. Operate on a uint32_t framebuffer. Declared in
 * wubu_screenshot_internal.h. Minimal includes.
 */

#include "wubu_screenshot_internal.h"

void draw_line(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;

    for (int t = 0; t < thickness; t++) {
        int cx = x1, cy = y1;
        while (true) {
            if (cx >= 0 && cx < w && cy >= 0 && cy < h) buf[cy * w + cx] = color;
            if (cx == x2 && cy == y2) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; cx += sx; }
            if (e2 <= dx) { err += dx; cy += sy; }
        }
        if (thickness > 1) {
            if (abs(x2 - x1) > abs(y2 - y1)) y1++; else x1++;
        }
    }
}

void draw_rect(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness, uint32_t fill) {
    int left = x1 < x2 ? x1 : x2;
    int right = x1 > x2 ? x1 : x2;
    int top = y1 < y2 ? y1 : y2;
    int bottom = y1 > y2 ? y1 : y2;

    if (fill) {
        for (int y = top + thickness; y < bottom - thickness; y++) {
            for (int x = left + thickness; x < right - thickness; x++) {
                if (x >= 0 && x < w && y >= 0 && y < h) buf[y * w + x] = fill;
            }
        }
    }

    for (int t = 0; t < thickness; t++) {
        int l = left + t, r = right - t, tp = top + t, b = bottom - t;
        for (int x = l; x <= r; x++) {
            if (x >= 0 && x < w) {
                if (tp >= 0 && tp < h) buf[tp * w + x] = color;
                if (b >= 0 && b < h) buf[b * w + x] = color;
            }
        }
        for (int y = tp; y <= b; y++) {
            if (y >= 0 && y < h) {
                if (l >= 0 && l < w) buf[y * w + l] = color;
                if (r >= 0 && r < w) buf[y * w + r] = color;
            }
        }
    }
}

void plot_ellipse_points(uint32_t *buf, int w, int h, int cx, int cy, int x, int y, uint32_t color) {
    if (cx + x >= 0 && cx + x < w && cy + y >= 0 && cy + y < h) buf[(cy + y) * w + cx + x] = color;
    if (cx - x >= 0 && cx - x < w && cy + y >= 0 && cy + y < h) buf[(cy + y) * w + cx - x] = color;
    if (cx + x >= 0 && cx + x < w && cy - y >= 0 && cy - y < h) buf[(cy - y) * w + cx + x] = color;
    if (cx - x >= 0 && cx - x < w && cy - y >= 0 && cy - y < h) buf[(cy - y) * w + cx - x] = color;
}

void draw_ellipse(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness, uint32_t fill) {
    int cx = (x1 + x2) / 2;
    int cy = (y1 + y2) / 2;
    int rx = abs(x2 - x1) / 2;
    int ry = abs(y2 - y1) / 2;
    if (rx == 0 || ry == 0) return;

    for (int t = 0; t < thickness; t++) {
        int x = 0, y = ry + t;
        int rx2 = (rx + t) * (rx + t);
        int ry2 = (ry + t) * (ry + t);
        int two_rx2 = 2 * rx2;
        int two_ry2 = 2 * ry2;
        int px = 0, py = two_rx2 * y;

        int p = ry2 - rx2 * ry + rx2 / 4;
        while (px < py) {
            plot_ellipse_points(buf, w, h, cx, cy, x, y, color);
            x++; px += two_ry2;
            if (p < 0) p += ry2 + px;
            else { y--; py -= two_rx2; p += ry2 + px - py; }
        }
        p = (int)(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
        while (y >= 0) {
            plot_ellipse_points(buf, w, h, cx, cy, x, y, color);
            y--; py -= two_rx2;
            if (p > 0) p += rx2 - py;
            else { x++; px += two_ry2; p += rx2 - py + px; }
        }
    }

    if (fill) {
        for (int dy = -ry; dy <= ry; dy++) {
            double ratio = (double)dy * dy / (ry * ry);
            if (ratio > 1.0) continue;
            int dx = (int)(rx * sqrt(1.0 - ratio));
            for (int x = -dx; x <= dx; x++) {
                int px = cx + x, py = cy + dy;
                if (px >= 0 && px < w && py >= 0 && py < h) buf[py * w + px] = fill;
            }
        }
    }
}

void draw_arrow(uint32_t *buf, int w, int h, int x1, int y1, int x2, int y2, uint32_t color, int thickness) {
    draw_line(buf, w, h, x1, y1, x2, y2, color, thickness);
    double angle = atan2((double)y1 - y2, (double)x1 - x2);
    int head_len = 15 + thickness * 3;
    int ax1 = x2 + (int)(head_len * cos(angle + 0.5));
    int ay1 = y2 + (int)(head_len * sin(angle + 0.5));
    int ax2 = x2 + (int)(head_len * cos(angle - 0.5));
    int ay2 = y2 + (int)(head_len * sin(angle - 0.5));
    draw_line(buf, w, h, x2, y2, ax1, ay1, color, thickness);
    draw_line(buf, w, h, x2, y2, ax2, ay2, color, thickness);
}
