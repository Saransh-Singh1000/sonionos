#ifndef SONION_KBD_EVENTS_H
#define SONION_KBD_EVENTS_H

#include <stdint.h>

typedef struct {
    uint8_t scancode;
    uint8_t pressed; /* 1 = Key-Down, 0 = Key-Up */
    char    ascii;
    uint8_t reserved;
} KeyEvent;

void kbd_push_event(uint8_t scancode, uint8_t pressed, char ascii);
int  kbd_pop_event(KeyEvent *out);

#endif
