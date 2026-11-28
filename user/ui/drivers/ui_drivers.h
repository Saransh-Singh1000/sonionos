#ifndef UI_DRIVERS_H
#define UI_DRIVERS_H

#include <stdint.h>
#include "usyscall.h"

typedef struct {
    uint32_t *backbuf;
    uint32_t  width;
    uint32_t  height;
} UiFramebuffer;

typedef struct {
    int32_t x;
    int32_t y;
    uint8_t buttons;   /* bit0=Left, bit1=Right, bit2=Middle */
    int8_t  scroll_dz; /* wheel delta since last poll */
} UiMouseState;

int  ui_fb_open(UiFramebuffer *fb);
void ui_fb_present(UiFramebuffer *fb);
void ui_fb_close(UiFramebuffer *fb);

void ui_mouse_poll(UiMouseState *ms);
int  ui_kbd_poll(SonionKeyEvent *ev);

#endif
