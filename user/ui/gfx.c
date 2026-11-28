#include "gfx.h"

static uint32_t *g_buf = 0;
static int g_w = 0, g_h = 0;

void gfx_init(BootInfo *info, uint32_t *backbuf) {
    if (!info || !backbuf) return;
    g_buf = backbuf;
    g_w = (int)info->width;
    g_h = (int)info->height;
}

void gfx_fill(uint32_t col) {
    if (!g_buf) return;
    int tot = g_w * g_h;
    for (int i = 0; i < tot; i++) g_buf[i] = col;
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t col) {
    if (!g_buf || w <= 0 || h <= 0) return;
    for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        if (py < 0 || py >= g_h) continue;
        for (int dx = 0; dx < w; dx++) {
            int px = x + dx;
            if (px >= 0 && px < g_w) g_buf[py * g_w + px] = col;
        }
    }
}

void gfx_rect(int x, int y, int w, int h, uint32_t col) {
    gfx_fill_rect(x, y, w, 1, col);
    gfx_fill_rect(x, y + h - 1, w, 1, col);
    gfx_fill_rect(x, y, 1, h, col);
    gfx_fill_rect(x + w - 1, y, 1, h, col);
}

void gfx_blend_pixel(int x, int y, uint32_t col, uint8_t alpha) {
    if (!g_buf || x < 0 || y < 0 || x >= g_w || y >= g_h || alpha == 0) return;
    if (alpha == 255) { g_buf[y * g_w + x] = col; return; }
    uint32_t dp = g_buf[y * g_w + x], ia = 255U - alpha;
    uint32_t r = (((col >> 16) & 0xFF) * alpha + ((dp >> 16) & 0xFF) * ia) >> 8;
    uint32_t g = (((col >> 8)  & 0xFF) * alpha + ((dp >> 8)  & 0xFF) * ia) >> 8;
    uint32_t b = (( col        & 0xFF) * alpha + ( dp        & 0xFF) * ia) >> 8;
    g_buf[y * g_w + x] = (r << 16) | (g << 8) | b;
}

void gfx_draw_image(const uint32_t *src, uint32_t sw, uint32_t sh, int dx, int dy, int dw, int dh) {
    if (!g_buf || !src || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    for (int y = 0; y < dh; y++) {
        int py = dy + y;
        if (py < 0 || py >= g_h) continue;
        int sy = (y * sh) / dh;
        for (int x = 0; x < dw; x++) {
            int px = dx + x;
            if (px < 0 || px >= g_w) continue;
            int sx = (x * sw) / dw;
            uint32_t sp = src[sy * sw + sx];
            uint8_t a = (uint8_t)(sp >> 24);
            if (a == 0) continue;
            if (a == 255) g_buf[py * g_w + px] = sp & 0x00FFFFFFU;
            else gfx_blend_pixel(px, py, sp & 0x00FFFFFFU, a);
        }
    }
}

