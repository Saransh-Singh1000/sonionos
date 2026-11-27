#include "kbd_events.h"

#define KBD_Q_SIZE 64
static KeyEvent kq[KBD_Q_SIZE];
static volatile uint32_t kq_head = 0;
static volatile uint32_t kq_tail = 0;

void kbd_push_event(uint8_t scancode, uint8_t pressed, char ascii) {
    uint32_t next = (kq_head + 1U) & (KBD_Q_SIZE - 1U);
    if (next == kq_tail) return; /* Queue full */
    kq[kq_head].scancode = scancode;
    kq[kq_head].pressed  = pressed ? 1 : 0;
    kq[kq_head].ascii    = ascii;
    kq[kq_head].reserved = 0;
    kq_head = next;
}

int kbd_pop_event(KeyEvent *out) {
    if (!out || kq_tail == kq_head) return 0;
    *out = kq[kq_tail];
    kq_tail = (kq_tail + 1U) & (KBD_Q_SIZE - 1U);
    return 1;
}
