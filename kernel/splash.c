#include "splash.h"
#include "img.h"
#include "boot_logo_data.h"

static void fill_circle(uint32_t *buf, int w, int h, int cx, int cy, int r, uint32_t col) {
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= h) continue;
        for (int dx = -r; dx <= r; dx++) {
            int px = cx + dx;
            if (px >= 0 && px < w && (dx * dx + dy * dy <= r2))
                buf[py * w + px] = col;
        }
    }
}

static void present_frame(BootInfo *info, const uint32_t *backbuf) {
    for (uint32_t y = 0; y < info->height; y++) {
        uint32_t *dst = info->framebuffer + y * info->pitch;
        const uint32_t *src = backbuf + y * info->width;
        for (uint32_t x = 0; x < info->width; x++) dst[x] = src[x];
    }
}

void boot_splash_show(BootInfo *info, uint32_t *backbuf) {
    if (!info || !info->framebuffer || !backbuf || !info->width || !info->height) return;

    int w = (int)info->width, h = (int)info->height;
    Image logo = {0, 0, 0};
    Image title = {0, 0, 0};
    int has_logo  = img_decode(boot_logo_data, sizeof(boot_logo_data), &logo);
    int has_title = img_decode(boot_title_data, sizeof(boot_title_data), &title);

    int lw = 150, lh = 150;
    if (has_logo && logo.width > 0 && logo.height > 0) {
        if (logo.width >= logo.height) {
            lw = 160; lh = (logo.height * 160) / logo.width;
        } else {
            lh = 160; lw = (logo.width * 160) / logo.height;
        }
    }

    int lx = (w - lw) / 2;
    int ly = (h / 2) - 125;
    if (ly < 24) ly = 24;

    int tw = has_title ? title.width : 260;
    int th = has_title ? title.height : 56;
    int tx = (w - tw) / 2;
    int ty = ly + lh + 10;

    /* Place rotating white loading circle lower down (like Windows 11 / Android) */
    int scx = w / 2;
    int scy = h - 115;
    if (scy < ty + th + 55) scy = ty + th + 55;

    static const int rx[24] = {
          0,   6,  11,  16,  19,  21,  22,  21,  19,  16,  11,   6,
          0,  -6, -11, -16, -19, -21, -22, -21, -19, -16, -11,  -6
    };
    static const int ry[24] = {
        -22, -21, -19, -16, -11,  -6,   0,   6,  11,  16,  19,  21,
         22,  21,  19,  16,  11,   6,   0,  -6, -11, -16, -19, -21
    };
    static const uint32_t white_trail[6] = {
        0x00263042, 0x00475569, 0x0064748B, 0x0094A3B8, 0x00E2E8F0, 0x00FFFFFF
    };

    for (int frame = 0; frame < 48; frame++) {
        for (int i = 0; i < w * h; i++) backbuf[i] = 0x00070B14;

        if (has_logo)  img_blit(backbuf, w, h, &logo, lx, ly, lw, lh);
        if (has_title) img_blit(backbuf, w, h, &title, tx, ty, tw, th);

        for (int k = 0; k < 6; k++) {
            int pos = (frame + k * 2) % 24;
            int rad = (k >= 4) ? 4 : 3;
            fill_circle(backbuf, w, h, scx + rx[pos], scy + ry[pos], rad, white_trail[k]);
        }

        present_frame(info, backbuf);
        if (info->sleep_ms) info->sleep_ms(25);
    }

    if (has_logo)  img_free(&logo);
    if (has_title) img_free(&title);
}
