#include <stdint.h>
#include "bootinfo.h"
#include "rick_data.h"

static const uint8_t font5x7[95][5] = {
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

static uint32_t backbuf[1920 * 1080];
static BootInfo *g_info = 0;

void swap_buffers(void) {
    uint64_t *dst = (uint64_t *)g_info->framebuffer;
    uint64_t *src = (uint64_t *)backbuf;
    uint32_t qwords = (g_info->height * g_info->pitch) / 2;
    for (uint32_t i = 0; i < qwords; i++) dst[i] = src[i];
}

void clear_bg(uint32_t col) {
    uint64_t c64 = ((uint64_t)col << 32) | col;
    uint64_t *dst = (uint64_t *)backbuf;
    uint32_t qwords = (g_info->height * g_info->pitch) / 2;
    for (uint32_t i = 0; i < qwords; i++) dst[i] = c64;
}

void draw_char(char c, int x, int y, uint32_t col, int scale) {
    if (c < 32 || c > 126) return;
    for (int i = 0; i < 5; i++) {
        uint8_t line = font5x7[c - 32][i];
        for (int j = 0; j < 7; j++) {
            if (line & (1 << j)) {
                for (int dy = 0; dy < scale; dy++) {
                    int py = y + j * scale + dy;
                    if (py < 0 || py >= (int)g_info->height) continue;
                    for (int dx = 0; dx < scale; dx++) {
                        int px = x + i * scale + dx;
                        if (px >= 0 && px < (int)g_info->width) backbuf[py * g_info->pitch + px] = col;
                    }
                }
            }
        }
    }
}

void draw_text(const char *s, int x, int y, uint32_t col, int scale) {
    while (*s) { draw_char(*s++, x, y, col, scale); x += 6 * scale; }
}

void kernel_main(BootInfo *info) {
    g_info = info;
    int total_frames = sizeof(rick_frames) / sizeof(rick_frames[0]);
    int f = 0;
    InputState in;

    while (1) {
        g_info->poll_input(&in);
        if (in.scan_code == 0x17) g_info->shutdown();

        clear_bg(0x00090D16);
        int start_x = ((int)info->width - (78 * 12)) / 2;
        if (start_x < 10) start_x = 10;
        int start_y = ((int)info->height - (24 * 18)) / 2;
        if (start_y < 10) start_y = 10;

        for (int row = 0; row < 24; row++) {
            draw_text(rick_frames[f][row], start_x, start_y + row * 18, 0x0038BDF8, 2);
        }

        swap_buffers();
        f = (f + 1) % total_frames;
        g_info->sleep_ms(75);
    }
}
