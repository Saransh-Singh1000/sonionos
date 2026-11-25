#include <stdint.h>
#include "bootinfo.h"

typedef struct __attribute__((packed)) {
    uint16_t attributes;
    uint8_t  window_a, window_b;
    uint16_t granularity, window_size, segment_a, segment_b;
    uint32_t win_func_ptr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t  w_char, y_char, planes, bpp, banks, memory_model, bank_size, image_pages, reserved0;
    uint8_t  red_mask, red_pos, green_mask, green_pos, blue_mask, blue_pos, rsvd_mask, rsvd_pos, direct_color;
    uint32_t framebuffer;
} VesaModeInfo;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static const char scancode_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p',0x5B,0x5D,'\r',
    0,  'a','s','d','f','g','h','j','k','l',';','\'',0x60, 0,
    '\\','z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' '
};

static uint64_t tsc_ticks_per_ms = 2000000;

static void calibrate_tsc(void) {
    uint8_t gate = inb(0x61) & 0xFC;
    outb(0x61, gate);
    outb(0x43, 0xB0);
    outb(0x42, 0x9C);
    outb(0x42, 0x2E);
    outb(0x61, gate | 1);

    uint64_t t0 = rdtsc();
    uint32_t guard = 0;
    while (!(inb(0x61) & 0x20) && ++guard < 10000000);
    uint64_t t1 = rdtsc();

    if (guard < 10000000 && t1 > t0) tsc_ticks_per_ms = (t1 - t0) / 10;
}

void bios_poll_input(InputState *state) {
    state->key_char = 0;
    state->scan_code = 0;
    state->left_click = 0;
    while (inb(0x64) & 1) {
        uint8_t status = inb(0x64);
        uint8_t data   = inb(0x60);
        if (!(status & 0x20) && !(data & 0x80)) {
            if (data == 0x01) state->scan_code = 0x17;
            else if (data < 128) state->key_char = scancode_ascii[data];
        }
    }
    state->mouse_x = 400;
    state->mouse_y = 300;
}

void bios_sleep_ms(uint32_t ms) {
    uint64_t target = rdtsc() + (uint64_t)ms * tsc_ticks_per_ms;
    while (rdtsc() < target) {
        __asm__ volatile ("pause");
    }
}

void bios_reboot(void) {
    while (inb(0x64) & 0x02);
    outb(0x64, 0xFE);
    while (1) __asm__("hlt");
}

void bios_shutdown(void) {
    __asm__ volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
    while (1) __asm__("hlt");
}

__attribute__((section(".text.entry")))
void bios_entry(VesaModeInfo *vesa) {
    calibrate_tsc();

    BootInfo info;
    info.framebuffer = (uint32_t *)(uintptr_t)vesa->framebuffer;
    info.width       = vesa->width  ? vesa->width  : 800;
    info.height      = vesa->height ? vesa->height : 600;
    info.pitch       = vesa->pitch  ? (vesa->pitch / 4) : info.width;
    info.poll_input  = bios_poll_input;
    info.sleep_ms    = bios_sleep_ms;
    info.reboot      = bios_reboot;
    info.shutdown    = bios_shutdown;

    kernel_main(&info);
    while (1) __asm__("hlt");
}
