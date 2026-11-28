#pragma once
#include <stdint.h>

typedef struct {
    int mouse_x, mouse_y;
    uint8_t left_click;
    uint8_t mouse_btns;
    int8_t scroll_z;
    char key_char;
    uint16_t scan_code;
} InputState;

typedef struct {
    uint32_t *framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint64_t acpi_rsdp; /* Physical RSDP address, or 0. */

    void (*poll_input)(InputState *state);
    void (*sleep_ms)(uint32_t ms);
    void (*reboot)(void);
    void (*shutdown)(void);
} BootInfo;

void kernel_main(BootInfo *info);


