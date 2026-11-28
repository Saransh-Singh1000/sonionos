#include <string.h>
#include "usyscall.h"
#include "drivers/ui_drivers.h"
#include "desktop.h"
#include "wallpaper.h"
#include "icons.h"
#include "term.h"

int main(void) {
    UiFramebuffer fb;
    if (!ui_fb_open(&fb)) return 1;

    wallpaper_load("/SYSTEM/WALLPAPER.RGB");
    icons_init();
    term_init();

    BootInfo info;
    info.framebuffer = fb.backbuf;
    info.width       = fb.width;
    info.height      = fb.height;
    info.pitch       = fb.width;

    desktop_init(&info, fb.backbuf);

    while (1) {
        int should_exit = 0;
        SonionKeyEvent ev;
        while (ui_kbd_poll(&ev)) {
            term_on_key(&ev, &should_exit);
            if (should_exit) {
                ui_fb_close(&fb);
                sys_clear();
                return 0;
            }
        }

        UiMouseState ms;
        ui_mouse_poll(&ms);

        InputState in;
        memset(&in, 0, sizeof(in));
        in.mouse_x    = ms.x;
        in.mouse_y    = ms.y;
        in.mouse_btns = ms.buttons;
        in.left_click = (ms.buttons & 1) ? 1 : 0;
        in.scroll_z   = ms.scroll_dz;

        desktop_update(&in);
        term_draw(fb.backbuf, (int)fb.width, (int)fb.height);
        desktop_draw_cursor();
        ui_fb_present(&fb);
    }
}
