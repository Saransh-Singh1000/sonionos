#ifndef UI_BOOTINFO_SHIM_H
#define UI_BOOTINFO_SHIM_H

#include <stdint.h>

typedef struct {
    int32_t mouse_x;
    int32_t mouse_y;
    uint8_t left_click;
    uint8_t mouse_btns;
    int8_t  scroll_z;
    char    key_char;
    uint16_t scan_code;
} InputState;

typedef struct {
    uint32_t *framebuffer;
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;
} BootInfo;

#endif
