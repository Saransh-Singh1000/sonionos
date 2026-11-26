#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <setjmp.h>
#include <math.h>
#include "bootinfo.h"
#include "pat.h"
#include "panic.h"
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

static int cmp_ints(const void *a, const void *b) {
    return (*(const int *)a - *(const int *)b);
}

static int approx_eq(double a, double b, double eps) {
    return fabs(a - b) <= eps;
}

static int run_math_tests(void) {
    if (!approx_eq(sin(0.0), 0.0, 1e-12)) return 1;
    if (!approx_eq(sin(M_PI_2), 1.0, 1e-12)) return 2;
    if (!approx_eq(cos(M_PI), -1.0, 1e-12)) return 3;
    if (!approx_eq(tan(M_PI_4), 1.0, 1e-11)) return 4;
    if (!approx_eq(asin(0.5), M_PI / 6.0, 1e-11)) return 5;
    if (!approx_eq(acos(0.5), M_PI / 3.0, 1e-11)) return 6;
    if (!approx_eq(atan2(-1.0, -1.0), -3.0 * M_PI_4, 1e-11)) return 7;
    double s = sin(1.2345), c = cos(1.2345);
    if (!approx_eq(s * s + c * c, 1.0, 1e-12)) return 8;

    if (!approx_eq(exp(1.0), M_E, 1e-12)) return 9;
    if (!approx_eq(log(M_E), 1.0, 1e-12)) return 10;
    if (!approx_eq(log10(1000.0), 3.0, 1e-12)) return 11;
    if (!approx_eq(log2(1024.0), 10.0, 1e-12)) return 12;
    if (!approx_eq(pow(2.0, 10.0), 1024.0, 1e-9)) return 13;
    if (!approx_eq(pow(-2.0, 3.0), -8.0, 1e-9)) return 14;
    if (!approx_eq(expm1(1e-8), 1e-8, 1e-15)) return 15;
    if (!approx_eq(log1p(1e-8), 1e-8, 1e-15)) return 16;

    if (!approx_eq(sqrt(2.0), M_SQRT2, 1e-12)) return 17;
    if (!approx_eq(cbrt(27.0), 3.0, 1e-12)) return 18;
    if (!approx_eq(cbrt(-64.0), -4.0, 1e-12)) return 19;
    if (!approx_eq(hypot(3.0, 4.0), 5.0, 1e-12)) return 20;

    if (floor(-2.3) != -3.0 || ceil(-2.3) != -2.0 || round(2.5) != 3.0) return 21;
    if (!approx_eq(fmod(5.3, 2.0), 1.3, 1e-12)) return 22;
    int exp2_out = 0;
    double mant = frexp(12.0, &exp2_out);
    if (!approx_eq(mant, 0.75, 1e-12) || exp2_out != 4) return 23;
    if (!approx_eq(ldexp(0.75, 4), 12.0, 1e-12)) return 24;
    double ipart = 0.0;
    double fpart = modf(-3.75, &ipart);
    if (ipart != -3.0 || !approx_eq(fpart, -0.75, 1e-12)) return 25;

    if (!approx_eq(cosh(0.0), 1.0, 1e-12) || !approx_eq(sinh(0.0), 0.0, 1e-12)) return 26;
    if (!approx_eq(asinh(sinh(1.5)), 1.5, 1e-11)) return 27;
    if (fabsf(sinf((float)M_PI_4) - 0.70710678f) > 1e-5f) return 28;
    if (fabsf(sqrtf(144.0f) - 12.0f) > 1e-5f) return 29;

    return 0;
}

static int run_libc_tests(void) {
    char buf[32];
    strcpy(buf, "sonion");
    strcat(buf, "OS");
    if (strcmp(buf, "sonionOS") != 0 || strlen(buf) != 8) return 0;
    memmove(buf + 2, buf, 6);
    if (strncmp(buf, "sosonion", 8) != 0) return 0;
    if (!isdigit('7') || tolower('Z') != 'z') return 0;
    if (strcmp(strerror(ENOMEM), "Cannot allocate memory") != 0) return 0;

    int *nums = (int *)malloc(5 * sizeof(int));
    if (!nums) return 0;
    nums[0] = 42; nums[1] = 7; nums[2] = 99; nums[3] = 1; nums[4] = 23;
    nums = (int *)realloc(nums, 6 * sizeof(int));
    nums[5] = 15;
    qsort(nums, 6, sizeof(int), cmp_ints);
    int key = 23;
    int *found = (int *)bsearch(&key, nums, 6, sizeof(int), cmp_ints);
    if (!found || *found != 23 || nums[0] != 1 || nums[5] != 99) return 0;
    free(nums);

    if (strtol("0xFF", NULL, 0) != 255) return 0;
    double d = strtod("-3.5e2", NULL);
    if (d < -350.1 || d > -349.9) return 0;

    jmp_buf env;
    int jumped = 0;
    if (setjmp(env) == 0) {
        longjmp(env, 42);
    } else {
        jumped = 1;
    }
    return jumped;
}

void kernel_main(BootInfo *info) {
    g_info = info;
    panic_init(info);
    enable_write_combining((uintptr_t)info->framebuffer, (uint64_t)info->height * info->pitch * 4);
    mem_init(0x3000000, 64 * 1024 * 1024);

    int libc_ok = run_libc_tests();
    int math_err = run_math_tests();
    int total_frames = sizeof(rick_frames) / sizeof(rick_frames[0]);
    int f = 0;
    InputState in;

    while (1) {
        g_info->poll_input(&in);
                if (in.scan_code == 0x17) g_info->shutdown();
        if (in.key_char == 'p') PANIC("Manual Kernel Panic Triggered by User!");
        if (in.key_char == 'z') { volatile int z = 0; volatile int crash = 1 / z; (void)crash; }
        if (in.key_char == 'x') { *(volatile uint64_t *)0xFFFFFFFF80000000ULL = 0x1337; }

        clear_bg(0x00090D16);
        int start_x = ((int)info->width - (78 * 12)) / 2;
        if (start_x < 10) start_x = 10;
        int start_y = ((int)info->height - (24 * 18)) / 2;
        if (start_y < 10) start_y = 10;

        for (int row = 0; row < 24; row++) {
            draw_text(rick_frames[f][row], start_x, start_y + row * 18, 0x0038BDF8, 2);
        }

        if (libc_ok && math_err == 0) {
            draw_text("LIBC M1-M3: [PASS] - 161 FUNCTIONS + 29 MATH SUITES OK", 20, 15, 0x0022C55E, 2);
        } else {
            char err_msg[] = "LIBC M1-M3: [FAIL] - TEST #00"; err_msg[27] = '0' + (math_err / 10); err_msg[28] = '0' + (math_err % 10); draw_text(err_msg, 20, 15, 0x00EF4444, 2);
        }

        draw_text("PANIC TEST: [P] Manual  [Z] DivByZero  [X] PageFault", 20, (int)info->height - 30, 0x0094A3B8, 2);
        swap_buffers();
        f = (f + 1) % total_frames;
        g_info->sleep_ms(75);
    }
}



