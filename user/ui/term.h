#ifndef UI_TERM_H
#define UI_TERM_H

#include <stdint.h>
#include "usyscall.h"

void term_init(void);
void term_on_key(const SonionKeyEvent *ev, int *out_exit);
void term_draw(uint32_t *backbuf, int sw, int sh);

#endif
