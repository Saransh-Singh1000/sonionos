p = "kernel/desktop.c"
s = open(p).read()

s = s.replace('#include "gfx.h"', '#include "gfx.h"\n#include "wallpaper.h"')

start = s.index("static void draw_open_apps(void) {")
end = s.index("static void draw_scene(void) {", start)

replacement = r'''
static void draw_open_apps(void) {
    if (!d_info) return;

    int cy = (int)d_info->height / 2;
    int radius = 88;
    int cx = -18;

    int dx = mouse_x - cx;
    int dy = mouse_y - cy;
    int near = (dx * dx + dy * dy) < (radius + 38) * (radius + 38);

    uint8_t alpha = near ? 238 : 105;
    uint32_t accent = near ? 0x0038BDF8 : 0x002D83A9;

    /* Semicircle extending beyond the left edge */
    for (int y = cy - radius; y <= cy + radius; y++) {
        int oy = y - cy;
        int remaining = radius * radius - oy * oy;
        if (remaining < 0 || y < 0 || y >= (int)d_info->height)
            continue;

        int span = 0;
        while ((span + 1) * (span + 1) <= remaining)
            span++;

        for (int x = 0; x <= span + cx; x++) {
            if (x < 0) continue;
            gfx_blend_pixel(x, y, 0x0018273D, alpha);
        }

        /* Outer illuminated boundary */
        int edge = cx + span;
        if (edge >= 0 && edge < (int)d_info->width)
            gfx_blend_pixel(edge, y, accent, near ? 255 : 170);

        /* Inner coloured curved boundary */
        int inner = cx + span - 7;
        if (inner >= 0 && inner < (int)d_info->width)
            gfx_blend_pixel(inner, y, 0x0022D3EE, near ? 230 : 115);
    }

    /* Small accent indicators */
    for (int i = 0; i < 3; i++) {
        int px = 9 + i * 9;
        int py = cy + 38;
        gfx_fill_rect(px, py, 4, 4,
                      near ? 0x0038BDF8 : 0x00516B87);
    }

    /* Console app tile */
    int ix = 12;
    int iy = cy - 20;

    gfx_fill_rect(ix, iy, 40, 40,
                  near ? 0x0038BDF8 : 0x002B5675);
    gfx_rect(ix, iy, 40, 40,
             near ? 0x00FFFFFF : 0x007B9BB8);

    gfx_fill_rect(ix + 8, iy + 9, 24, 22, 0x00090D16);
    gfx_fill_rect(ix + 12, iy + 15, 8, 2, 0x00FFFFFF);
    gfx_fill_rect(ix + 18, iy + 19, 2, 7, 0x00FFFFFF);
    gfx_fill_rect(ix + 18, iy + 24, 8, 2, 0x00FFFFFF);
}

'''

s = s[:start] + replacement + s[end:]

# Replace old background with wallpaper rendering.
s = s.replace(
    '    gfx_fill(0x000B1220);\n\n    gfx_fill_rect(w * 3 / 5, 60, w / 3, h / 3,\n                  0x0015223A);\n\n    gfx_fill_rect(w * 2 / 3, 90, w / 4, h / 4,\n                  0x001B2C48);',
    '    gfx_fill(0x000B1220);\n    wallpaper_draw((int)w, (int)h);\n\n    /* Subtle desktop accent lines */\n    for (uint32_t i = 0; i < 5; i++) {\n        int px = (int)(w * 3 / 5 + i * 13);\n        gfx_blend_pixel(px, 52, 0x0038BDF8, 90);\n    }'
)

# Add subtle hover highlight to desktop icons.
s = s.replace(
    '        if (selected_icon == i) {',
    '        int hovered = inside(mouse_x, mouse_y, x - 6, y - 6, 60, 60);\\n\\n        if (selected_icon == i || hovered) {'
)

# Add a small status detail to the top panel.
s = s.replace(
    '    gfx_fill_rect(0, 33, w, 1, 0x0038BDF8);',
    '    gfx_fill_rect(0, 33, w, 1, 0x0038BDF8);\\n    gfx_fill_rect((int)w - 72, 13, 7, 7, 0x0022C55E);\\n    gfx_fill_rect((int)w - 58, 15, 35, 3, 0x00516B87);'
)

# Public redraw entry point.
marker = 'static void maximize_window(void) {'
s = s.replace(marker, 'void desktop_refresh(void) {\\n    if (d_info) draw_scene();\\n}\\n\\n' + marker)

open(p, "w").write(s)
print("Desktop renderer patched.")
