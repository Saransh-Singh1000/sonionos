#include "kbd_events.h"

#define KBD_Q_SIZE 64
static KeyEvent kq[KBD_Q_SIZE];
static volatile uint32_t kq_head = 0;
static volatile uint32_t kq_tail = 0;
static uint8_t active_mods = 0;

static const char map_norm[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';',0x27,0x60,
    0,  '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0,  ' ', 0
};

static const char map_shift[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0,  ' ', 0
};

uint8_t kbd_get_modifiers(void) { return active_mods; }
void    kbd_set_modifiers(uint8_t mods) { active_mods = mods; }

static void update_modifier_key(uint8_t sc, uint8_t pressed) {
    uint8_t mask = 0;
    if (sc == 0x2A) mask = KMOD_LSHIFT;
    else if (sc == 0x36) mask = KMOD_RSHIFT;
    else if (sc == 0x1D) mask = KMOD_LCTRL;
    else if (sc == 0x9D) mask = KMOD_RCTRL;
    else if (sc == 0x38) mask = KMOD_LALT;
    else if (sc == 0xB8) mask = KMOD_RALT;
    else if (sc == 0xDB || sc == 0xDC) mask = KMOD_SUPER;
    else if (sc == 0x3A && pressed) { active_mods ^= KMOD_CAPS; return; }
    if (mask) {
        if (pressed) active_mods |= mask;
        else active_mods &= (uint8_t)~mask;
    }
}

static char translate_key(uint8_t sc, char fallback) {
    char ch = 0;
    if (sc < 128) {
        int shift = (active_mods & KMOD_SHIFT) != 0;
        ch = shift ? map_shift[sc] : map_norm[sc];
        if (!shift && (active_mods & KMOD_CAPS) && ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        else if (shift && (active_mods & KMOD_CAPS) && ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
    }
    if (!ch) ch = fallback;
    if ((active_mods & KMOD_CTRL) && ch) {
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 1);
        else if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 1);
    }
    return ch;
}

char kbd_push_event(uint8_t scancode, uint8_t pressed, char fallback_ascii) {
    update_modifier_key(scancode, pressed);
    char ch = translate_key(scancode, fallback_ascii);
    uint32_t next = (kq_head + 1U) & (KBD_Q_SIZE - 1U);
    if (next != kq_tail) {
        kq[kq_head].scancode  = scancode;
        kq[kq_head].pressed   = pressed ? 1 : 0;
        kq[kq_head].ascii     = ch;
        kq[kq_head].modifiers = active_mods;
        kq_head = next;
    }
    return ch;
}

int kbd_pop_event(KeyEvent *out) {
    if (!out || kq_tail == kq_head) return 0;
    *out = kq[kq_tail];
    kq_tail = (kq_tail + 1U) & (KBD_Q_SIZE - 1U);
    return 1;
}


