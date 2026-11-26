#include "desktop.h"
#include "gfx.h"

static BootInfo *d_info;

static int mouse_x, mouse_y;
static int old_mouse_x, old_mouse_y;
static int previous_click;
static int dragging;

static int selected_icon = -1;

static int win_x, win_y, win_w, win_h;
static int saved_x, saved_y, saved_w, saved_h;

static int win_open = 1;
static int win_minimized = 0;
static int win_maximized = 0;
static int win_focused = 1;

static const uint32_t icon_colors[3] = {
    0x0038BDF8, 0x0022C55E, 0x00A78BFA
};

static int inside(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && y >= ry &&
           x < rx + rw && y < ry + rh;
}

static int dock_y(void) {
    return (d_info->height > 100)
        ? (int)d_info->height - 76 : 0;
}

static void clamp_window(void) {
    int max_x = (int)d_info->width - win_w;
    int max_y = (int)d_info->height - win_h;

    if (win_x < 0) win_x = 0;
    if (win_y < 34) win_y = 34;
    if (win_x > max_x) win_x = max_x;
    if (win_y > max_y) win_y = max_y;
}

static void draw_window(void) {
    if (!win_open || win_minimized) return;

    gfx_fill_rect(win_x + 5, win_y + 5,
                  win_w, win_h, 0x00000000);

    gfx_fill_rect(win_x, win_y, win_w, win_h,
                  0x002B3D55);

    gfx_rect(win_x, win_y, win_w, win_h,
             win_focused ? 0x0038BDF8 : 0x00475B78);

    /* Title bar */
    gfx_fill_rect(win_x + 1, win_y + 1,
                  win_w - 2, 29,
                  win_focused ? 0x0018273D : 0x00101829);

    gfx_fill_rect(win_x + 1, win_y + 29,
                  win_w - 2, 1, 0x0038BDF8);

    /* Minimize */
    gfx_fill_rect(win_x + win_w - 62, win_y + 10,
                  9, 9, 0x00F59E0B);

    /* Maximize / restore */
    gfx_fill_rect(win_x + win_w - 44, win_y + 10,
                  9, 9, 0x0022C55E);

    /* Close */
    gfx_fill_rect(win_x + win_w - 26, win_y + 10,
                  9, 9, 0x00EF4444);

    /* Console content */
    gfx_fill_rect(win_x + 10, win_y + 38,
                  win_w - 20, win_h - 48,
                  0x00090D16);
}

static void draw_scene(void) {
    uint32_t w = d_info->width;
    uint32_t h = d_info->height;

    gfx_fill(0x000B1220);

    gfx_fill_rect(w * 3 / 5, 60, w / 3, h / 3,
                  0x0015223A);

    gfx_fill_rect(w * 2 / 3, 90, w / 4, h / 4,
                  0x001B2C48);

    /* Top panel */
    gfx_fill_rect(0, 0, w, 34, 0x00101829);
    gfx_fill_rect(0, 33, w, 1, 0x0038BDF8);

    /* Desktop icons */
    for (int i = 0; i < 3; i++) {
        int x = 28;
        int y = 64 + i * 100;

        if (selected_icon == i) {
            gfx_fill_rect(x - 6, y - 6, 60, 60,
                          0x00304765);
            gfx_rect(x - 6, y - 6, 60, 60,
                     0x0038BDF8);
        }

        gfx_fill_rect(x, y, 48, 48, icon_colors[i]);
        gfx_rect(x, y, 48, 48, 0x00E2E8F0);
        gfx_fill_rect(x + 12, y + 12, 24, 24,
                      0x00E2E8F0);
    }

    /* Dock */
    int dw = (w > 440) ? 400 : (int)w - 32;
    if (dw < 0) dw = 0;

    int dx = ((int)w - dw) / 2;
    int dy = dock_y();

    gfx_fill_rect(dx, dy, dw, 56, 0x00182235);
    gfx_rect(dx, dy, dw, 56, 0x00475B78);

    const uint32_t colors[5] = {
        0x0038BDF8, 0x0022C55E, 0x00F59E0B,
        0x00A78BFA, 0x00EF4444
    };

    for (int i = 0; i < 5; i++) {
        int x = dx + 14 + i * 72;
        if (x + 40 >= dx + dw) break;

        gfx_fill_rect(x, dy + 8, 40, 40, colors[i]);
        gfx_rect(x, dy + 8, 40, 40, 0x00E2E8F0);
    }

    /* Console dock indicator */
    if (win_open && win_minimized) {
        gfx_fill_rect(dx + 14, dy + 51, 40, 3,
                      0x0038BDF8);
    }

    draw_window();
}

void desktop_console_bounds(int *x, int *y, int *w, int *h) {
    if (!d_info) return;

    if (x) *x = win_x + 18;
    if (y) *y = win_y + 44;
    if (w) *w = win_w - 36;
    if (h) *h = win_h - 56;
}

int desktop_console_visible(void) {
    return win_open && !win_minimized;
}

void desktop_init(BootInfo *info, uint32_t *backbuf) {
    if (!info || !backbuf || !info->width || !info->height)
        return;

    d_info = info;
    gfx_init(info, backbuf);

    win_w = (int)info->width - 120;
    win_h = (int)info->height - 150;

    if (win_w > 680) win_w = 680;
    if (win_h > 460) win_h = 460;

    if (win_w > (int)info->width - 16)
        win_w = (int)info->width - 16;

    if (win_h > (int)info->height - 100)
        win_h = (int)info->height - 100;

    if (win_w < 180) win_w = 180;
    if (win_h < 120) win_h = 120;

    win_x = ((int)info->width - win_w) / 2;
    win_y = 48;

    saved_x = win_x;
    saved_y = win_y;
    saved_w = win_w;
    saved_h = win_h;

    mouse_x = (int)info->width / 2;
    mouse_y = (int)info->height / 2;

    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;

    draw_scene();
}

static void maximize_window(void) {
    if (!win_maximized) {
        saved_x = win_x;
        saved_y = win_y;
        saved_w = win_w;
        saved_h = win_h;

        win_x = 8;
        win_y = 40;
        win_w = (int)d_info->width - 16;
        win_h = (int)d_info->height - 124;

        win_maximized = 1;
    } else {
        win_x = saved_x;
        win_y = saved_y;
        win_w = saved_w;
        win_h = saved_h;

        win_maximized = 0;
    }
}

void desktop_update(const InputState *input) {
    if (!d_info || !input) return;

    mouse_x = input->mouse_x;
    mouse_y = input->mouse_y;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;

    if (mouse_x >= (int)d_info->width)
        mouse_x = (int)d_info->width - 1;

    if (mouse_y >= (int)d_info->height)
        mouse_y = (int)d_info->height - 1;

    int pressed = input->left_click != 0;

    if (pressed && !previous_click) {
        int dock_x = ((int)d_info->width -
                     ((d_info->width > 440) ? 400 :
                      (int)d_info->width - 32)) / 2;

        /* Restore console from its dock icon */
        if (inside(mouse_x, mouse_y,
                   dock_x + 14, dock_y() + 8, 40, 40)) {
            win_open = 1;
            win_minimized = 0;
            win_focused = 1;
        }

        else if (desktop_console_visible() &&
                 inside(mouse_x, mouse_y,
                        win_x, win_y, win_w, 30)) {

            win_focused = 1;

            if (inside(mouse_x, mouse_y,
                       win_x + win_w - 62, win_y + 7, 15, 15)) {
                win_minimized = 1;
                dragging = 0;
            }

            else if (inside(mouse_x, mouse_y,
                            win_x + win_w - 44, win_y + 7, 15, 15)) {
                maximize_window();
                dragging = 0;
            }

            else if (inside(mouse_x, mouse_y,
                            win_x + win_w - 26, win_y + 7, 15, 15)) {
                win_open = 0;
                win_minimized = 0;
                dragging = 0;
            }

            else if (!win_maximized) {
                dragging = 1;
            }
        }

        else if (desktop_console_visible() &&
                 inside(mouse_x, mouse_y,
                        win_x, win_y, win_w, win_h)) {
            win_focused = 1;
            dragging = 0;
        }

        else {
            dragging = 0;
            win_focused = 0;
            selected_icon = -1;

            for (int i = 0; i < 3; i++) {
                int iy = 64 + i * 100;

                if (inside(mouse_x, mouse_y,
                           22, iy - 6, 60, 60)) {
                    selected_icon = i;
                    break;
                }
            }
        }
    }

    if (pressed && dragging && desktop_console_visible()) {
        win_x += mouse_x - old_mouse_x;
        win_y += mouse_y - old_mouse_y;
        clamp_window();
    }

    if (!pressed)
        dragging = 0;

    previous_click = pressed;
    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;

    draw_scene();
}

void desktop_draw_cursor(void) {
    if (!d_info) return;

    for (int y = 0; y < 16; y++) {
        int width = y / 2 + 1;
        gfx_fill_rect(mouse_x - 1, mouse_y + y,
                      width + 2, 1, 0x00000000);
    }

    for (int y = 0; y < 14; y++) {
        int width = y / 2 + 1;
        gfx_fill_rect(mouse_x, mouse_y + y,
                      width, 1, 0x00FFFFFF);
    }
}
