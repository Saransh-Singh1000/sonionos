#include <img.h>
#include "icons.h"
#include "gfx.h"

#define ICON_COUNT 3

static Image icons[ICON_COUNT];
static int   icon_loaded[ICON_COUNT];

static const char *paths[ICON_COUNT] = {
    "/SYSTEM/ICONS/COMPUTER.PNG",
    "/SYSTEM/ICONS/FILES.PNG",
    "/SYSTEM/ICONS/SETTINGS.PNG"
};

void icons_unload(void) {
    for (int i = 0; i < ICON_COUNT; i++) {
        if (icon_loaded[i]) {
            img_free(&icons[i]);
            icon_loaded[i] = 0;
        }
    }
}

void icons_init(void) {
    icons_unload();
    for (int i = 0; i < ICON_COUNT; i++) {
        if (img_load(paths[i], &icons[i])) {
            icon_loaded[i] = 1;
        }
    }
}

void icons_draw(int index, int x, int y, int w, int h) {
    if (index < 0 || index >= ICON_COUNT) return;
    if (icon_loaded[index] && icons[index].pixels) {
        gfx_draw_image(icons[index].pixels, icons[index].width, icons[index].height, x, y, w, h);
    } else {
        static const uint32_t fallback[3] = { 0x0038BDF8, 0x0022C55E, 0x00A78BFA };
        gfx_fill_rect(x, y, w, h, fallback[index]);
        gfx_rect(x, y, w, h, 0x00E2E8F0);
    }
}
