#ifndef SONION_KBD_EVENTS_H
#define SONION_KBD_EVENTS_H

#include <stdint.h>

#define KMOD_LSHIFT 0x01
#define KMOD_RSHIFT 0x02
#define KMOD_SHIFT  0x03
#define KMOD_LCTRL  0x04
#define KMOD_RCTRL  0x08
#define KMOD_CTRL   0x0C
#define KMOD_LALT   0x10
#define KMOD_RALT   0x20
#define KMOD_ALT    0x30
#define KMOD_SUPER  0x40
#define KMOD_CAPS   0x80

typedef struct {
    uint8_t scancode;
    uint8_t pressed;   /* 1 = Key-Down, 0 = Key-Up */
    char    ascii;     /* Translated ASCII (honors Shift, CapsLock, Ctrl+A..Z) */
    uint8_t modifiers; /* Live KMOD_* bitmask at moment of key event */
} KeyEvent;

uint8_t kbd_get_modifiers(void);
void    kbd_set_modifiers(uint8_t mods);
char    kbd_push_event(uint8_t scancode, uint8_t pressed, char fallback_ascii);
int     kbd_pop_event(KeyEvent *out);

#endif
