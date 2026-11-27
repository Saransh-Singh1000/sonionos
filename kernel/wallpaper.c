#include <stdint.h>
#include <stddef.h>
#include "libc/stdlib.h"
#include <string.h>

#include "wallpaper.h"
#include "gfx.h"
#include "fs/fat32.h"

#define WALLPAPER_MAX_WIDTH  4096
#define WALLPAPER_MAX_HEIGHT 2160
#define WALLPAPER_MAX_BYTES  (16 * 1024 * 1024)

static uint32_t *wallpaper_pixels;
static uint32_t wallpaper_width;
static uint32_t wallpaper_height;

int wallpaper_load(const char *path) {
    wallpaper_unload();

    Fat32File *file = fat32_open(path, "rb");
    if (!file) return -1;

    if (file->size < 8 || file->size > WALLPAPER_MAX_BYTES) {
        fat32_close(file);
        return -2;
    }

    uint32_t header[2];

    if (fat32_read(header, 1, 8, file) != 8) {
        fat32_close(file);
        return -3;
    }

    uint32_t width = header[0];
    uint32_t height = header[1];

    if (!width || !height ||
        width > WALLPAPER_MAX_WIDTH ||
        height > WALLPAPER_MAX_HEIGHT ||
        (uint64_t)width * height > (WALLPAPER_MAX_BYTES - 8) / 4 ||
        (uint64_t)width * height * 4 + 8 != file->size) {
        fat32_close(file);
        return -4;
    }

    size_t pixel_count = (size_t)width * height;
    uint32_t *pixels = malloc(pixel_count * sizeof(uint32_t));

    if (!pixels) {
        fat32_close(file);
        return -5;
    }

    if (fat32_read(pixels, 1, pixel_count * sizeof(uint32_t), file)
        != pixel_count * sizeof(uint32_t)) {
        free(pixels);
        fat32_close(file);
        return -6;
    }

    fat32_close(file);

    wallpaper_pixels = pixels;
    wallpaper_width = width;
    wallpaper_height = height;

    return 0;
}

void wallpaper_draw(int screen_width, int screen_height) {
    if (!wallpaper_pixels || screen_width <= 0 || screen_height <= 0)
        return;

    gfx_draw_image(
        wallpaper_pixels,
        wallpaper_width,
        wallpaper_height,
        0, 0,
        screen_width,
        screen_height
    );
}

void wallpaper_unload(void) {
    if (wallpaper_pixels) free(wallpaper_pixels);

    wallpaper_pixels = 0;
    wallpaper_width = 0;
    wallpaper_height = 0;
}
