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

static const char scancode_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\r',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`', 0,
    '\\','z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' '
};

static VesaModeInfo *g_vesa = 0;
static uint32_t *virt_fb = (uint32_t *)0x2000000;

void bios_flush_vesa(void) {
    if (!g_vesa || !g_vesa->framebuffer) return;
    uint8_t *hw = (uint8_t *)(uintptr_t)g_vesa->framebuffer;
    uint32_t w = g_vesa->width, h = g_vesa->height, pitch = g_vesa->pitch;
    if (g_vesa->bpp == 32) {
        for (uint32_t y = 0; y < h; y++) {
            uint32_t *dst = (uint32_t *)(hw + y * pitch);
            uint32_t *src = virt_fb + y * w;
            for (uint32_t x = 0; x < w; x++) dst[x] = src[x];
        }
    } else {
        for (uint32_t y = 0; y < h; y++) {
            uint8_t *row = hw + y * pitch;
            uint32_t *src = virt_fb + y * w;
            for (uint32_t x = 0; x < w; x++) {
                uint32_t c = src[x];
                row[x * 3 + 0] = (uint8_t)(c & 0xFF);
                row[x * 3 + 1] = (uint8_t)((c >> 8) & 0xFF);
                row[x * 3 + 2] = (uint8_t)((c >> 16) & 0xFF);
            }
        }
    }
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
    state->mouse_x = 512;
    state->mouse_y = 384;
}

void bios_sleep_ms(uint32_t ms) {
    bios_flush_vesa();
    // Exact 1.193182 MHz x86 PIT Channel 2 hardware timer (1193 ticks = 1.000 ms)
    for (uint32_t i = 0; i < ms; i++) {
        outb(0x61, (inb(0x61) & 0xFD) | 1);
        outb(0x43, 0xB0);
        outb(0x42, 0xA9);
        outb(0x42, 0x04);
        while (!(inb(0x61) & 0x20));
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
    g_vesa = vesa;
    uint32_t w = vesa->width ? vesa->width : 1024;
    uint32_t h = vesa->height ? vesa->height : 768;
    for (uint32_t i = 0; i < w * h; i++) virt_fb[i] = 0x00090D16;

    BootInfo info;
    info.framebuffer = virt_fb;
    info.width       = w;
    info.height      = h;
    info.pitch       = w;
    info.poll_input  = bios_poll_input;
    info.sleep_ms    = bios_sleep_ms;
    info.reboot      = bios_reboot;
    info.shutdown    = bios_shutdown;

    kernel_main(&info);
    while (1) __asm__("hlt");
}

