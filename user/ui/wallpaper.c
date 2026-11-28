#include <img.h>
#include "wallpaper.h"
#include "gfx.h"

static Image wp_img;
static int   wp_loaded = 0;

int wallpaper_load(const char *path) {
    wallpaper_unload();
    if (path && img_load(path, &wp_img)) { wp_loaded = 1; return 0; }
    if (img_load("/SYSTEM/WALLPAPER.PNG", &wp_img)) { wp_loaded = 1; return 0; }
    if (img_load("/SYSTEM/WALLPAPER.JPG", &wp_img)) { wp_loaded = 1; return 0; }
    return -1;
}

void wallpaper_draw(int screen_width, int screen_height) {
    if (!wp_loaded || !wp_img.pixels || screen_width <= 0 || screen_height <= 0) return;
    gfx_draw_image(wp_img.pixels, wp_img.width, wp_img.height, 0, 0, screen_width, screen_height);
}

void wallpaper_unload(void) {
    if (wp_loaded) {
        img_free(&wp_img);
        wp_loaded = 0;
    }
}
