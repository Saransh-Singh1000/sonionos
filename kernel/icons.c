#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "icons.h"
#include "gfx.h"
#include "fs/fat32.h"

#define ICON_COUNT 3
#define MAX_BYTES (16u * 1024u * 1024u)

typedef struct {
    uint32_t *pixels;
    uint32_t width;
    uint32_t height;
} Icon;

static Icon icons[ICON_COUNT];

static const char *paths[ICON_COUNT] = {
    "/SYSTEM/ICONS/COMPUTER.BIN",
    "/SYSTEM/ICONS/FILES.BIN",
    "/SYSTEM/ICONS/SETTINGS.BIN"
};

void icons_unload(void) {
    for (int i = 0; i < ICON_COUNT; i++) {
        free(icons[i].pixels);
        icons[i].pixels = NULL;
        icons[i].width = icons[i].height = 0;
    }
}

void icons_init(void) {
    icons_unload();

    for (int i = 0; i < ICON_COUNT; i++) {
        Fat32File *f = fat32_open(paths[i], "rb");
        if (!f) continue;

        if (f->size < 8 || f->size > MAX_BYTES) {
            fat32_close(f);
            continue;
        }

        uint32_t header[2];
        if (fat32_read(header, 1, 8, f) != 8) {
            fat32_close(f);
            continue;
        }

        uint32_t w = header[0], h = header[1];
        uint64_t bytes = (uint64_t)w * h * 4;

        if (!w || !h || w > 4096 || h > 2160 ||
            bytes + 8 != f->size || bytes > MAX_BYTES - 8) {
            fat32_close(f);
            continue;
        }

        uint32_t *pixels = malloc((size_t)bytes);
        if (!pixels) {
            fat32_close(f);
            continue;
        }

        if (fat32_read(pixels, 1, (size_t)bytes, f) != bytes) {
            free(pixels);
            fat32_close(f);
            continue;
        }

        fat32_close(f);
        icons[i].pixels = pixels;
        icons[i].width = w;
        icons[i].height = h;
    }
}

void icons_draw(int index, int x, int y, int w, int h) {
    if (index < 0 || index >= ICON_COUNT) return;

    Icon *icon = &icons[index];
    if (!icon->pixels) return;

    gfx_draw_image(icon->pixels, icon->width, icon->height,
                   x, y, w, h);
}
