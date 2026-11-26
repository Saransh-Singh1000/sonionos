#include "console.h"
#include "desktop.h"
#include "string.h"

#define MAX_ROWS 135
#define MAX_COLS 240

static const uint8_t cfont[95][5] = {
    {0,0,0,0,0},{0,0,95,0,0},{0,7,0,7,0},{20,127,20,127,20},{36,42,127,42,18},
    {35,19,8,100,98},{54,73,85,34,80},{0,5,3,0,0},{0,28,34,65,0},{0,65,34,28,0},
    {20,8,62,8,20},{8,8,62,8,8},{0,80,48,0,0},{8,8,8,8,8},{0,96,96,0,0},
    {32,16,8,4,2},{62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},
    {24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},
    {6,73,73,41,30},{0,54,54,0,0},{0,86,54,0,0},{8,20,34,65,0},{20,20,20,20,20},
    {0,65,34,20,8},{2,1,81,9,6},{50,73,121,65,62},{126,17,17,17,126},{127,73,73,73,54},
    {62,65,65,65,34},{127,65,65,34,28},{127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},
    {127,8,8,8,127},{0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
    {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},{62,65,81,33,94},
    {127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},{63,64,64,64,63},{31,32,64,32,31},
    {63,64,56,64,63},{99,20,8,20,99},{7,8,112,8,7},{97,81,73,69,67},{0,127,65,65,0},
    {2,4,8,16,32},{0,65,65,127,0},{4,2,1,2,4},{64,64,64,64,64},{0,1,2,4,0},
    {32,84,84,84,120},{127,72,68,68,56},{56,68,68,68,32},{56,68,68,72,127},{56,84,84,84,24},
    {8,126,9,1,2},{12,82,82,82,62},{127,8,4,4,120},{0,68,125,64,0},{32,64,68,61,0},
    {127,16,40,68,0},{0,65,127,64,0},{124,4,24,4,120},{124,8,4,4,120},{56,68,68,68,56},
    {124,20,20,20,8},{8,20,20,24,124},{124,8,4,4,8},{72,84,84,84,32},{4,63,68,64,32},
    {60,64,64,32,124},{28,32,64,32,28},{60,64,48,64,60},{68,40,16,40,68},{12,80,80,80,60},
    {68,100,84,76,68},{0,8,54,65,0},{0,0,127,0,0},{0,65,54,8,0},{16,8,8,16,8}
};

static BootInfo *c_info = 0;
static uint32_t *c_buf  = 0;
static void (*c_flush)(void) = 0;
void console_set_flush(void (*fn)(void)) { c_flush = fn; }
void console_flush_now(void) { console_render(); if (c_flush) c_flush(); }
static int c_scale = 2, c_cols = 80, c_rows = 25;
static int cur_x = 0, cur_y = 0, dirty = 1;
static uint32_t fg_col = 0x00E2E8F0, bg_col = 0x00090D16;

static int c_area_x = 16, c_area_y = 16;
static int c_area_w = 0, c_area_h = 0;

static char     grid_ch[MAX_ROWS][MAX_COLS];
static uint32_t grid_fg[MAX_ROWS][MAX_COLS];

static inline void serial_outb(uint16_t p, uint8_t v) {
    __asm__ volatile ("outb %0, %1" : : "a"(v), "Nd"(p));
}
static inline uint8_t serial_inb(uint16_t p) {
    uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(p)); return r;
}

static void serial_putc(char c) {
    if (c == '\n') serial_putc('\r');
    for (int t = 0; t < 100000 && !(serial_inb(0x3FD) & 0x20); t++);
    serial_outb(0x3F8, (uint8_t)c);
}

void console_clear(void) {
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            grid_ch[r][c] = ' ';
            grid_fg[r][c] = fg_col;
        }
    }
    cur_x = 0; cur_y = 0; dirty = 1;
}

void console_init(BootInfo *info, uint32_t *backbuf, int scale) {
    c_info  = info;
    c_buf   = backbuf ? backbuf : info->framebuffer;
    c_scale = (scale > 0) ? scale : 2;

    desktop_console_bounds(&c_area_x, &c_area_y,
                           &c_area_w, &c_area_h);

    c_cols = c_area_w / (6 * c_scale);
    c_rows = c_area_h / (9 * c_scale);

    if (c_cols < 1) c_cols = 1;
    if (c_rows < 1) c_rows = 1;
    if (c_cols > MAX_COLS) c_cols = MAX_COLS;
    if (c_rows > MAX_ROWS) c_rows = MAX_ROWS;
    console_clear();
}

void console_set_color(uint32_t fg, uint32_t bg) {
    fg_col = fg; bg_col = bg; dirty = 1;
}

static void console_scroll(void) {
    for (int r = 1; r < c_rows; r++) {
        memcpy(grid_ch[r - 1], grid_ch[r], (size_t)c_cols);
        memcpy(grid_fg[r - 1], grid_fg[r], (size_t)c_cols * sizeof(uint32_t));
    }
    for (int c = 0; c < c_cols; c++) {
        grid_ch[c_rows - 1][c] = ' ';
        grid_fg[c_rows - 1][c] = fg_col;
    }
    cur_y = c_rows - 1;
}

void console_putchar(char c) {
    serial_putc(c);
    if (c == '\r') { cur_x = 0; dirty = 1; return; }
    if (c == '\n') {
        cur_x = 0;
        if (++cur_y >= c_rows) console_scroll();
        dirty = 1;
        return;
    }
    if (c == '\b') {
        if (cur_x > 0) {
            cur_x--;
            grid_ch[cur_y][cur_x] = ' ';
            dirty = 1;
        }
        return;
    }
    if (c == '\t') {
        int next = (cur_x + 4) & ~3;
        while (cur_x < next && cur_x < c_cols) console_putchar(' ');
        return;
    }
    if ((unsigned char)c < 32 || (unsigned char)c > 126) c = '?';
    grid_ch[cur_y][cur_x] = c;
    grid_fg[cur_y][cur_x] = fg_col;
    if (++cur_x >= c_cols) {
        cur_x = 0;
        if (++cur_y >= c_rows) console_scroll();
    }
    dirty = 1;
}

void console_write(const char *buf, size_t len) {
    if (!buf) return;
    for (size_t i = 0; i < len; i++) console_putchar(buf[i]);
}

void console_puts(const char *s) {
    if (!s) return;
    while (*s) console_putchar(*s++);
    console_putchar('\n');
}

void console_render(void) {
    if (!c_info || !c_buf) return;

    if (!desktop_console_visible()) return;

    /* Follow the window when it moves. */
    desktop_console_bounds(&c_area_x, &c_area_y,
                           &c_area_w, &c_area_h);

    if (!dirty) return;
    dirty = 0;

    /* Console is rendered inside the desktop window. */
    int pad_x = c_area_x;
    int pad_y = c_area_y;
    int cw = 6 * c_scale;
    int ch = 9 * c_scale;

    /* Clear only the console content area. */
    for (int y = 0; y < c_area_h; y++) {
        int py = c_area_y + y;
        if (py < 0 || py >= (int)c_info->height) continue;

        for (int x = 0; x < c_area_w; x++) {
            int px = c_area_x + x;
            if (px < 0 || px >= (int)c_info->width) continue;

            c_buf[py * c_info->pitch + px] = bg_col;
        }
    }

    for (int r = 0; r < c_rows; r++) {
        for (int c = 0; c < c_cols; c++) {
            char ch_val = grid_ch[r][c];
            if (ch_val <= 32 || ch_val > 126) continue;
            uint32_t col = grid_fg[r][c];
            int gx = pad_x + c * cw;
            int gy = pad_y + r * ch;
            for (int i = 0; i < 5; i++) {
                uint8_t line = cfont[ch_val - 32][i];
                for (int j = 0; j < 7; j++) {
                    if (line & (1 << j)) {
                        for (int dy = 0; dy < c_scale; dy++) {
                            int py = gy + j * c_scale + dy;
                            if (py >= (int)c_info->height) continue;
                            for (int dx = 0; dx < c_scale; dx++) {
                                int px = gx + i * c_scale + dx;
                                if (px < (int)c_info->width) c_buf[py * c_info->pitch + px] = col;
                            }
                        }
                    }
                }
            }
        }
    }

    // Draw underscore cursor at active (cur_x, cur_y)
    int cx = pad_x + cur_x * cw;
    int cy = pad_y + cur_y * ch + 7 * c_scale;
    for (int dy = 0; dy < c_scale; dy++) {
        int py = cy + dy;
        if (py >= (int)c_info->height) break;
        for (int dx = 0; dx < 5 * c_scale; dx++) {
            int px = cx + dx;
            if (px < (int)c_info->width) c_buf[py * c_info->pitch + px] = 0x0038BDF8;
        }
    }
}

int console_getchar(int blocking) {
    if (!c_info || !c_info->poll_input) return -1;
    InputState in;
    do {
        c_info->poll_input(&in);

        desktop_update(&in);

        /* Desktop was redrawn, so redraw the complete text grid. */
        dirty = 1;
        console_render();

        desktop_draw_cursor();

        if (c_flush) c_flush();

        if (in.key_char != 0)
            return (int)(unsigned char)in.key_char;

        if (in.scan_code == 0x17) return 27; // ESC
        if (blocking && c_info->sleep_ms) c_info->sleep_ms(10);
    } while (blocking);
    return -1;
}


