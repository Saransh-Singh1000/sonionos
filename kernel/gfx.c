#include "gfx.h"

static BootInfo *g_info;
static uint32_t *g_buf;

void gfx_init(BootInfo *info, uint32_t *backbuf) {
    g_info = info;
    g_buf = backbuf;
}

void gfx_pixel(int x, int y, uint32_t color) {
    if (!g_info || !g_buf) return;
    if (x < 0 || y < 0 || x >= (int)g_info->width ||
        y >= (int)g_info->height) return;

    g_buf[(uint32_t)y * g_info->pitch + (uint32_t)x] = color;
}

void gfx_fill(uint32_t color) {
    if (!g_info || !g_buf) return;

    for (uint32_t y = 0; y < g_info->height; y++) {
        uint32_t *row = &g_buf[y * g_info->pitch];
        for (uint32_t x = 0; x < g_info->width; x++)
            row[x] = color;
    }
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!g_info || !g_buf || w <= 0 || h <= 0) return;

    int x2 = x + w;
    int y2 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > (int)g_info->width) x2 = (int)g_info->width;
    if (y2 > (int)g_info->height) y2 = (int)g_info->height;

    for (int py = y; py < y2; py++) {
        for (int px = x; px < x2; px++)
            g_buf[(uint32_t)py * g_info->pitch + (uint32_t)px] = color;
    }
}

void gfx_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;

    gfx_fill_rect(x, y, w, 1, color);
    gfx_fill_rect(x, y + h - 1, w, 1, color);
    gfx_fill_rect(x, y, 1, h, color);
    gfx_fill_rect(x + w - 1, y, 1, h, color);
}

void gfx_draw_image(const uint32_t *pixels,
                    uint32_t image_width,
                    uint32_t image_height,
                    int x, int y, int width, int height) {
    if (!g_info || !g_buf || !pixels ||
        !image_width || !image_height ||
        width <= 0 || height <= 0)
        return;

    for (int dy = 0; dy < height; dy++) {
        int py = y + dy;
        if (py < 0 || py >= (int)g_info->height)
            continue;

        uint32_t sy = (uint32_t)(((uint64_t)dy * image_height) / height);

        for (int dx = 0; dx < width; dx++) {
            int px = x + dx;
            if (px < 0 || px >= (int)g_info->width)
                continue;

            uint32_t sx = (uint32_t)(((uint64_t)dx * image_width) / width);

            g_buf[(uint32_t)py * g_info->pitch + (uint32_t)px] =
                pixels[sy * image_width + sx];
        }
    }
}

void gfx_blend_pixel(int x, int y, uint32_t color, uint8_t alpha) {
    if (!g_info || !g_buf || x < 0 || y < 0 ||
        x >= (int)g_info->width || y >= (int)g_info->height)
        return;

    uint32_t *dst = &g_buf[(uint32_t)y * g_info->pitch + (uint32_t)x];
    uint32_t old = *dst;
    uint32_t a = alpha;
    uint32_t inv = 255 - a;

    uint32_t r = (((color >> 16) & 255) * a +
                  ((old >> 16) & 255) * inv) / 255;
    uint32_t g = (((color >> 8) & 255) * a +
                  ((old >> 8) & 255) * inv) / 255;
    uint32_t b = ((color & 255) * a +
                  (old & 255) * inv) / 255;

    *dst = (r << 16) | (g << 8) | b;
}
